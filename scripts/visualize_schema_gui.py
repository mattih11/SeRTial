#!/usr/bin/env python3
"""
SeRTial Message Schema Visualizer - GUI Version

Tkinter-based GUI to visualize message schemas:
- Memory layout before/after serialization
- Copy blocks with different colors
- Padding analysis
- Interactive message selection
- Variable-size field controls with runtime size calculation

Usage:
    python visualize_schema_gui.py [schema.json]

Interactive Features:
- For messages with variable-size fields (marked with ~ in the list):
  * Sliders appear to control element counts for each variable field
  * Runtime size calculation updates in real-time
  * Third memory bar shows actual serialized layout with current slider values
  * Length prefixes (4 bytes) are visualized before each dynamic field

Legend:
- [1] = Single memcpy (fastest, fixed-size struct)
- o   = Multiple memcpy regions (field splitting)
- P   = Has padding between fields
- ~   = Has variable-size fields (vector, string, etc.)
"""

import json
import sys
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from dataclasses import dataclass
from typing import List, Optional
from pathlib import Path


@dataclass
class FieldInfo:
    name: str
    type: str
    index: int
    offset: int
    size: int
    packed_offset: int
    padding_before: int
    # Variable-length container info
    is_variable_length: bool = False
    element_size: int = 0
    max_elements: int = 0
    header_size: int = 0


@dataclass
class BlockInfo:
    """Information about a serialization block."""
    type: str  # "Fixed", "Padding", "Dynamic", "RuntimeOffset"
    src_offset: int
    dst_offset: int
    size: int
    field_index: int
    field_start: int
    field_count: int
    is_variable: bool


@dataclass
class MemcpyRegion:
    src_offset: int
    dst_offset: int
    size: int
    field_start: int
    field_count: int


@dataclass
class MessageSchema:
    name: str
    category: str
    sizeof_bytes: int
    packed_size: int
    padding_bytes: int
    field_count: int
    has_padding: bool
    can_single_memcpy: bool
    memcpy_region_count: int
    fields: List[FieldInfo]
    memcpy_regions: List[MemcpyRegion]
    # Hybrid memory map data
    has_variable_fields: bool = False
    base_packed_size: int = 0
    fixed_block_count: int = 0
    dynamic_block_count: int = 0
    runtime_offset_block_count: int = 0
    blocks: List[BlockInfo] = None


# Color palette for fields
FIELD_COLORS = [
    # dark color palette with distinguishable colors
    "#c21300",  # Red
    "#0067ab",  # Blue
    "#039540",  # Green
    "#66008f",  # Purple
    "#c29b00",  # Yellow
    "#ac5000",  # Orange
    "#009a7b",  # Turquoise
    "#004386",  # Dark Blue
]

PADDING_COLOR = '#7f8c8d'  # Gray
LENGTH_PREFIX_COLOR = "#ddff00"  # Yellow - distinct from padding
BG_COLOR = '#2c3e50'       # Dark background
TEXT_COLOR = '#ecf0f1'     # Light text
PANEL_BG = '#34495e'       # Panel background


def load_schemas(filepath: str) -> dict:
    """Load schemas from JSON file."""
    with open(filepath, 'r') as f:
        return json.load(f)


def parse_message(data: dict) -> MessageSchema:
    """Parse a message schema from JSON data."""
    # Get field names and types as separate arrays
    field_names = data.get('field_names', [])
    field_types = data.get('field_types', [])
    field_infos = data.get('field_info', [])
    
    # Combine into FieldInfo objects
    fields = []
    for i, info in enumerate(field_infos):
        name = field_names[i] if i < len(field_names) else f"field_{i}"
        ftype = field_types[i] if i < len(field_types) else "unknown"
        fields.append(FieldInfo(
            name=name,
            type=ftype,
            index=info.get('index', i),
            offset=info.get('offset', 0),
            size=info.get('size', 0),
            packed_offset=info.get('packed_offset', 0),
            padding_before=info.get('padding_before', 0),
            is_variable_length=info.get('is_variable_length', False),
            element_size=info.get('element_size', 0),
            max_elements=info.get('max_elements', 0),
            header_size=info.get('header_size', 0)
        ))
    
    # Parse memcpy regions
    memcpy_regions = [
        MemcpyRegion(
            src_offset=r.get('src_offset', 0),
            dst_offset=r.get('dst_offset', 0),
            size=r.get('size', 0),
            field_start=r.get('field_start', 0),
            field_count=r.get('field_count', 0)
        )
        for r in data.get('memcpy_regions', [])
    ]
    
    # Parse blocks
    blocks = [
        BlockInfo(
            type=b.get('type', 'Fixed'),
            src_offset=b.get('src_offset', 0),
            dst_offset=b.get('dst_offset', 0),
            size=b.get('size', 0),
            field_index=b.get('field_index', -1),
            field_start=b.get('field_start', -1),
            field_count=b.get('field_count', 0),
            is_variable=b.get('is_variable', False)
        )
        for b in data.get('blocks', [])
    ]
    
    return MessageSchema(
        name=data.get('name', ''),
        category=data.get('category', ''),
        sizeof_bytes=data.get('sizeof_bytes', 0),
        packed_size=data.get('packed_size', 0),
        padding_bytes=data.get('padding_bytes', 0),
        field_count=data.get('field_count', 0),
        has_padding=data.get('has_padding', False),
        can_single_memcpy=data.get('can_single_memcpy', False),
        memcpy_region_count=data.get('memcpy_region_count', 0),
        fields=fields,
        memcpy_regions=memcpy_regions,
        has_variable_fields=data.get('has_variable_fields', False),
        base_packed_size=data.get('base_packed_size', data.get('packed_size', 0)),
        fixed_block_count=data.get('fixed_block_count', 0),
        dynamic_block_count=data.get('dynamic_block_count', 0),
        runtime_offset_block_count=data.get('runtime_offset_block_count', 0),
        blocks=blocks if blocks else []
    )


class MemoryCanvas(tk.Canvas):
    """Canvas for drawing memory layout visualizations."""
    
    def __init__(self, parent, **kwargs):
        super().__init__(parent, bg=PANEL_BG, highlightthickness=0, **kwargs)
        self.tooltip = None
        self.tooltip_id = None
        self.all_regions = []  # Store all regions from all bars
        
        # Bind mouse events for tooltips
        self.bind('<Motion>', self._on_motion)
        self.bind('<Leave>', self._hide_tooltip)
        
    def clear_regions(self):
        """Clear all stored regions."""
        self.all_regions = []
        
    def draw_memory_bar(self, x, y, width, height, total_size, regions, label="", 
                        show_empty_bg=True, reference_size=None):
        """
        Draw a memory bar with colored regions.
        regions: List of (start, length, color, label, field_info, is_variable, var_info)
                 where var_info is (element_size, max_elements) or None
        show_empty_bg: If True, show gray background for unused space. If False, just outline.
        reference_size: If provided, scale positions to this size for alignment (bar always full width).
        """
        if total_size == 0:
            return
        
        # Use reference_size for scaling positions, bar always uses full width
        scale_size = reference_size if reference_size else total_size
        
        # Draw label
        self.create_text(x, y - 5, text=label, fill=TEXT_COLOR, anchor='sw', 
                        font=('Consolas', 8, 'bold'))
        
        # Calculate where data ends (for showing size label)
        data_end_x = x + int(width * total_size / scale_size)
        
        # Draw full-width outer frame
        self.create_rectangle(x, y, x + width, y + height, 
                             fill='', outline='#555555', width=1)
        
        # Draw background for the actual data portion
        if show_empty_bg:
            # Solid gray background for padding/unused
            self.create_rectangle(x + 1, y + 1, data_end_x - 1, y + height - 1, 
                                 fill=PADDING_COLOR, outline='')
        else:
            # Subtle background for serialized
            self.create_rectangle(x + 1, y + 1, data_end_x - 1, y + height - 1, 
                                 fill='#3d566e', outline='')
        
        # Draw each region (scaled to reference_size so both bars align)
        for region_data in regions:
            # Support both old and new format
            if len(region_data) >= 7:
                start, length, color, name, info, is_variable, var_info = region_data
            else:
                start, length, color, name, info = region_data
                is_variable = False
                var_info = None
            
            if length == 0:
                continue
            
            # Scale positions to reference_size for alignment
            rx = x + int(start * width / scale_size)
            rx_end = x + int((start + length) * width / scale_size)
            rw = max(rx_end - rx, 2)
            
            # Draw the base rectangle
            self.create_rectangle(rx, y + 1, rx + rw, y + height - 1,
                                 fill=color, outline='#1a252f')
            
            # Draw diagonal stripes for variable-length fields
            if is_variable and rw > 10:
                self._draw_stripes(rx, y + 1, rx + rw, y + height - 1, color)
            
            # Store region info for tooltips
            self.all_regions.append((rx, y, rx + rw, y + height, name, info))
            
            # Draw field name if there's enough space
            if rw > 40:
                display_name = name[:8] if len(name) > 8 else name
                # Add ~ prefix for variable-length fields
                if is_variable:
                    display_name = "~" + display_name[:7]
                self.create_text(rx + rw/2, y + height/2, text=display_name,
                               fill='white', font=('Consolas', 8))
        
        # Draw size label at end of data
        self.create_text(data_end_x + 5, y + height/2, 
                        text=f"{total_size}B", fill=TEXT_COLOR, anchor='w',
                        font=('Consolas', 8))
    
    def _draw_stripes(self, x1, y1, x2, y2, base_color):
        """Draw diagonal stripes to indicate variable-length region."""
        # Create a slightly darker color for stripes
        stripe_spacing = 8
        stripe_width = 3
        
        # Darken the base color
        r = int(base_color[1:3], 16)
        g = int(base_color[3:5], 16)
        b = int(base_color[5:7], 16)
        dark_color = f'#{max(0,r-40):02x}{max(0,g-40):02x}{max(0,b-40):02x}'
        
        # Draw diagonal stripes from bottom-left to top-right
        for offset in range(-int(y2-y1), int(x2-x1) + int(y2-y1), stripe_spacing):
            # Line from (x1+offset, y2) to (x1+offset+(y2-y1), y1)
            lx1 = x1 + offset
            ly1 = y2
            lx2 = x1 + offset + (y2 - y1)
            ly2 = y1
            
            # Clip to rectangle bounds
            if lx2 < x1 or lx1 > x2:
                continue
            
            # Adjust line endpoints to stay within bounds
            if lx1 < x1:
                ratio = (x1 - lx1) / (lx2 - lx1) if lx2 != lx1 else 0
                ly1 = ly1 + ratio * (ly2 - ly1)
                lx1 = x1
            if lx2 > x2:
                ratio = (x2 - lx1) / (lx2 - lx1) if lx2 != lx1 else 0
                ly2 = ly1 + ratio * (ly2 - ly1)
                lx2 = x2
            
            self.create_line(lx1, ly1, lx2, ly2, fill=dark_color, width=stripe_width)
    
    def _on_motion(self, event):
        """Show tooltip when hovering over a region."""
        for rx1, ry1, rx2, ry2, name, info in self.all_regions:
            if rx1 <= event.x <= rx2 and ry1 <= event.y <= ry2:
                self._show_tooltip(event.x, event.y, name, info)
                return
        self._hide_tooltip()
    
    def _show_tooltip(self, x, y, name, info):
        """Display tooltip with field information."""
        if self.tooltip:
            self._hide_tooltip()
        
        self.tooltip = tk.Toplevel(self)
        self.tooltip.wm_overrideredirect(True)
        self.tooltip.wm_geometry(f"+{self.winfo_rootx() + x + 10}+{self.winfo_rooty() + y + 10}")
        
        frame = tk.Frame(self.tooltip, bg='#2c3e50', relief='solid', borderwidth=1)
        frame.pack()
        
        text = f"{name}\n{info}"
        label = tk.Label(frame, text=text, bg='#2c3e50', fg='white',
                        font=('Consolas', 8), justify='left', padx=5, pady=3)
        label.pack()
    
    def _hide_tooltip(self, event=None):
        """Hide the tooltip."""
        if self.tooltip:
            self.tooltip.destroy()
            self.tooltip = None


class SchemaVisualizerApp:
    """Main application class."""
    
    def __init__(self, root, schema_file=None):
        self.root = root
        self.root.title("SeRTial Message Schema Visualizer")
        self.root.configure(bg=BG_COLOR)
        self.root.geometry("1200x800")
        
        self.messages = []
        self.current_message = None
        self.schema_data = None
        
        self._create_ui()
        
        if schema_file:
            self._load_file(schema_file)
    
    def _create_ui(self):
        """Create the main UI layout."""
        # Main container
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill='both', expand=True, padx=10, pady=10)
        
        # Configure style
        style = ttk.Style()
        style.configure('Dark.TFrame', background=BG_COLOR)
        style.configure('Dark.TLabel', background=BG_COLOR, foreground=TEXT_COLOR)
        style.configure('Dark.TButton', background=PANEL_BG)
        
        # Left panel - Message list
        left_frame = ttk.Frame(main_frame, width=250)
        left_frame.pack(side='left', fill='y', padx=(0, 10))
        left_frame.pack_propagate(False)
        
        # Load button
        load_btn = ttk.Button(left_frame, text="Load Schema JSON", command=self._browse_file)
        load_btn.pack(fill='x', pady=(0, 10))
        
        # Category filter
        filter_frame = ttk.Frame(left_frame)
        filter_frame.pack(fill='x', pady=(0, 10))
        
        ttk.Label(filter_frame, text="Category:").pack(side='left')
        self.category_var = tk.StringVar(value="All")
        self.category_combo = ttk.Combobox(filter_frame, textvariable=self.category_var,
                                           state='readonly', width=15)
        self.category_combo.pack(side='left', padx=(5, 0))
        self.category_combo.bind('<<ComboboxSelected>>', self._filter_messages)
        
        # Message listbox
        ttk.Label(left_frame, text="Messages:").pack(anchor='w')
        
        list_frame = ttk.Frame(left_frame)
        list_frame.pack(fill='both', expand=True)
        
        scrollbar = ttk.Scrollbar(list_frame)
        scrollbar.pack(side='right', fill='y')
        
        self.message_list = tk.Listbox(list_frame, bg=PANEL_BG, fg=TEXT_COLOR,
                                       selectbackground='#3498db', 
                                       font=('Consolas', 8),
                                       yscrollcommand=scrollbar.set)
        self.message_list.pack(fill='both', expand=True)
        scrollbar.config(command=self.message_list.yview)
        
        self.message_list.bind('<<ListboxSelect>>', self._on_message_select)
        
        # Right panel - Details
        right_frame = ttk.Frame(main_frame)
        right_frame.pack(side='right', fill='both', expand=True)
        
        # Info panel
        self.info_frame = ttk.LabelFrame(right_frame, text="Message Info")
        self.info_frame.pack(fill='x', pady=(0, 10))
        
        self.info_text = tk.Text(self.info_frame, height=8, bg=PANEL_BG, fg=TEXT_COLOR,
                                font=('Consolas', 8), wrap='word', relief='flat')
        self.info_text.pack(fill='x', padx=5, pady=5)
        
        # Variable field controls panel
        self.var_controls_frame = ttk.LabelFrame(right_frame, text="Variable Field Controls")
        # Initially hidden, shown when variable-size message is selected
        
        self.var_controls_inner = ttk.Frame(self.var_controls_frame)
        self.var_controls_inner.pack(fill='both', expand=True, padx=5, pady=5)
        
        self.var_field_controls = {}  # Dict[field_index, (Label, Scale, Entry)]
        self.var_field_values = {}    # Dict[field_index, IntVar]
        
        # Runtime size display
        self.runtime_size_var = tk.StringVar(value="Runtime size: N/A")
        self.runtime_size_label = ttk.Label(self.var_controls_frame, 
                                            textvariable=self.runtime_size_var,
                                            font=('Consolas', 8, 'bold'))
        self.runtime_size_label.pack(fill='x', padx=5, pady=(0, 5))
        
        # Memory layout canvas
        layout_frame = ttk.LabelFrame(right_frame, text="Memory Layout")
        layout_frame.pack(fill='both', expand=True, pady=(0, 10))
        
        self.memory_canvas = MemoryCanvas(layout_frame, height=200)
        self.memory_canvas.pack(fill='both', expand=True, padx=5, pady=5)
        
        # Fields table
        fields_frame = ttk.LabelFrame(right_frame, text="Fields")
        fields_frame.pack(fill='both', expand=True, pady=(0, 10))
        
        # Create treeview for fields with larger row height
        columns = ('name', 'type', 'offset', 'size', 'packed', 'padding')
        
        # Configure style for larger rows and header
        style = ttk.Style()
        style.configure('Fields.Treeview', rowheight=28, font=('Consolas', 8))
        style.configure('Fields.Treeview.Heading', font=('Consolas', 8, 'bold'), padding=(5, 8))
        
        self.fields_tree = ttk.Treeview(fields_frame, columns=columns, show='headings', 
                                        height=10, style='Fields.Treeview')
        
        self.fields_tree.heading('name', text='Name')
        self.fields_tree.heading('type', text='Type')
        self.fields_tree.heading('offset', text='Offset')
        self.fields_tree.heading('size', text='Size')
        self.fields_tree.heading('packed', text='Packed')
        self.fields_tree.heading('padding', text='Padding')
        
        self.fields_tree.column('name', width=150)
        self.fields_tree.column('type', width=250)
        self.fields_tree.column('offset', width=70)
        self.fields_tree.column('size', width=70)
        self.fields_tree.column('packed', width=70)
        self.fields_tree.column('padding', width=70)
        
        tree_scroll = ttk.Scrollbar(fields_frame, orient='vertical', command=self.fields_tree.yview)
        self.fields_tree.configure(yscrollcommand=tree_scroll.set)
        
        self.fields_tree.pack(side='left', fill='both', expand=True)
        tree_scroll.pack(side='right', fill='y')
        
        # Copy operations - Two column layout
        copy_frame = ttk.LabelFrame(right_frame, text="Copy Operations")
        copy_frame.pack(fill='both', expand=True)
        
        # Create two-column layout
        copy_columns = ttk.Frame(copy_frame)
        copy_columns.pack(fill='both', expand=True, padx=5, pady=5)
        
        # Left column: Block execution order (static)
        left_col = ttk.Frame(copy_columns)
        left_col.pack(side='left', fill='both', expand=True, padx=(0, 5))
        
        ttk.Label(left_col, text="Block Execution Order", font=('Consolas', 8, 'bold')).pack(anchor='w')
        self.copy_text_static = tk.Text(left_col, height=8, bg=PANEL_BG, fg=TEXT_COLOR,
                                font=('Consolas', 7), wrap='none', relief='flat')
        self.copy_text_static.pack(fill='both', expand=True)
        
        # Right column: Runtime execution with actual sizes
        right_col = ttk.Frame(copy_columns)
        right_col.pack(side='right', fill='both', expand=True, padx=(5, 0))
        
        ttk.Label(right_col, text="Runtime Execution (Current Sliders)", font=('Consolas', 8, 'bold')).pack(anchor='w')
        self.copy_text_runtime = tk.Text(right_col, height=8, bg=PANEL_BG, fg=TEXT_COLOR,
                                font=('Consolas', 7), wrap='none', relief='flat')
        self.copy_text_runtime.pack(fill='both', expand=True)
        
        # Status bar
        self.status_var = tk.StringVar(value="Load a schema file to begin")
        status_bar = ttk.Label(self.root, textvariable=self.status_var, relief='sunken')
        status_bar.pack(fill='x', side='bottom')
    
    def _browse_file(self):
        """Open file browser to select schema JSON."""
        filepath = filedialog.askopenfilename(
            title="Select Schema JSON",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")]
        )
        if filepath:
            self._load_file(filepath)
    
    def _load_file(self, filepath):
        """Load schema file."""
        try:
            self.schema_data = load_schemas(filepath)
            self.messages = [parse_message(m) for m in self.schema_data.get('messages', [])]
            
            # Update category filter
            categories = sorted(set(m.category for m in self.messages))
            self.category_combo['values'] = ['All'] + categories
            self.category_var.set('All')
            
            self._update_message_list()
            
            version = self.schema_data.get('generator_version', 'unknown')
            generated = self.schema_data.get('generated_at', 'unknown')
            self.status_var.set(f"Loaded {len(self.messages)} messages | Generated: {generated} | Version: {version}")
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load schema: {e}")
    
    def _filter_messages(self, event=None):
        """Filter message list by category."""
        self._update_message_list()
    
    def _update_message_list(self):
        """Update the message listbox."""
        self.message_list.delete(0, tk.END)

        category = self.category_var.get()

        for msg in self.messages:
            if category == 'All' or msg.category == category:
                # [1] means single memcpy, o means multiple regions
                status = "[1]" if msg.can_single_memcpy else "o"
                padding = "P" if msg.has_padding else " "
                variable = "~" if msg.has_variable_fields else " "
                self.message_list.insert(tk.END, f"{status}{padding}{variable} {msg.name}")
    
    def _on_message_select(self, event):
        """Handle message selection."""
        selection = self.message_list.curselection()
        if not selection:
            return

        # Get selected message name (after status, padding, and variable markers)
        item = self.message_list.get(selection[0])
        # Split on first space to get the name
        parts = item.split(' ', 1)
        if len(parts) < 2:
            return
        name = parts[1].strip()

        # Find the message
        category = self.category_var.get()
        for msg in self.messages:
            if msg.name == name and (category == 'All' or msg.category == category):
                self._display_message(msg)
                break
    
    def _display_message(self, msg: MessageSchema):
        """Display message details."""
        self.current_message = msg

        # Update info panel
        self.info_text.delete('1.0', tk.END)

        memcpy_status = "[1] Single memcpy (fastest, no field splitting)" if msg.can_single_memcpy else f"o {msg.memcpy_region_count} memcpy regions (needs field splitting)"
        padding = "Has Padding" if msg.has_padding else "No Padding"

        info = f"""Name:           {msg.name}
Category:       {msg.category}
Fields:         {msg.field_count}

sizeof:         {msg.sizeof_bytes} bytes
packed_size:    {msg.packed_size} bytes {'(max)' if msg.has_variable_fields else ''}
padding_bytes:  {msg.padding_bytes} bytes

Copy Strategy:  {memcpy_status} | {padding}
"""
        
        # Add hybrid block information
        if msg.has_variable_fields:
            info += f"""
Hybrid Blocks:  {msg.fixed_block_count} Fixed, {msg.dynamic_block_count} Dynamic, {msg.runtime_offset_block_count} RuntimeOffset
Base size:      {msg.base_packed_size} bytes (before dynamic fields)
[VARIABLE]      Runtime size varies based on field content
"""
        
        info += "\nLegend: [1] = can be copied in one block (fastest), o = needs multiple copy regions, P = has padding\n"
        
        if msg.has_padding:
            savings = msg.padding_bytes
            pct = (savings / msg.sizeof_bytes * 100) if msg.sizeof_bytes > 0 else 0
            info += f"Space Saved:    {savings} bytes ({pct:.1f}%)\n"

        self.info_text.insert('1.0', info)

        # Update memory canvas
        self._draw_memory_layout(msg)

        # Update fields table
        self.fields_tree.delete(*self.fields_tree.get_children())
        for i, field in enumerate(msg.fields):
            color = FIELD_COLORS[i % len(FIELD_COLORS)]
            padding_str = f"+{field.padding_before}" if field.padding_before > 0 else ""
            # Mark variable-length fields
            name_display = f"~{field.name}" if field.is_variable_length else field.name
            size_display = f"{field.size}"
            if field.is_variable_length:
                size_display = f"<={field.size} ({field.max_elements}×{field.element_size})"
            self.fields_tree.insert('', 'end', values=(
                name_display, field.type, field.offset, size_display, field.packed_offset, padding_str
            ), tags=(f'color{i}',))
            self.fields_tree.tag_configure(f'color{i}', background=color)

        # Update copy operations - use blocks if available (HybridMemoryMap)
        self._update_copy_operations(msg)
        
        # Setup variable field controls
        self._setup_variable_controls(msg)
    
    def _update_copy_operations(self, msg: MessageSchema):
        """Update both static and runtime copy operation displays."""
        # Clear both text widgets
        self.copy_text_static.delete('1.0', tk.END)
        self.copy_text_runtime.delete('1.0', tk.END)
        
        if not msg.blocks or len(msg.blocks) == 0:
            # Fallback to old memcpy regions
            copy_info = f"Memcpy regions: {msg.memcpy_region_count}\n\n"
            for i, region in enumerate(msg.memcpy_regions[:10]):
                start_idx = region.field_start
                end_idx = min(start_idx + region.field_count, len(msg.fields))
                field_names = [msg.fields[j].name for j in range(start_idx, end_idx)]
                desc = ", ".join(field_names) if field_names else f"fields {start_idx}-{end_idx-1}"
                copy_info += f"{i+1}. {desc}\n   {region.size}B @ src+{region.src_offset}\n"
            self.copy_text_static.insert('1.0', copy_info)
            self.copy_text_runtime.insert('1.0', "N/A (no blocks)")
            return
        
        # LEFT COLUMN: Static block execution order
        static_info = ""
        for i, block in enumerate(msg.blocks):
            if block.type == "Fixed":
                field_names = []
                for fidx in range(block.field_start, block.field_start + block.field_count):
                    if fidx < len(msg.fields):
                        field_names.append(msg.fields[fidx].name)
                fields_str = ", ".join(field_names) if field_names else f"f{block.field_start}-{block.field_start+block.field_count-1}"
                
                static_info += f"{i+1}. Fixed: {fields_str}\n"
                static_info += f"   copy {block.size}B from src+{block.src_offset}\n\n"
            
            elif block.type == "Dynamic":
                field = msg.fields[block.field_index] if block.field_index < len(msg.fields) else None
                field_name = field.name if field else f"f{block.field_index}"
                elem_size = field.element_size if field else 0
                
                static_info += f"{i+1}. Dynamic: {field_name}\n"
                static_info += f"   write len (4B)\n"
                static_info += f"   copy N×{elem_size}B from src+{block.src_offset}\n\n"
            
            elif block.type == "RuntimeOffset":
                field_names = []
                for fidx in range(block.field_start, block.field_start + block.field_count):
                    if fidx < len(msg.fields):
                        field_names.append(msg.fields[fidx].name)
                fields_str = ", ".join(field_names) if field_names else f"f{block.field_start}-{block.field_start+block.field_count-1}"
                
                static_info += f"{i+1}. RuntimeOffset: {fields_str}\n"
                static_info += f"   copy {block.size}B from src+{block.src_offset}\n"
                static_info += f"   (dst varies)\n\n"
        
        self.copy_text_static.insert('1.0', static_info)
        
        # RIGHT COLUMN: Runtime execution with actual sizes
        self._update_runtime_copy_operations(msg)
    
    def _update_runtime_copy_operations(self, msg: MessageSchema):
        """Update runtime copy operations based on current slider values."""
        if not msg.blocks or len(msg.blocks) == 0:
            return
        
        self.copy_text_runtime.delete('1.0', tk.END)
        
        runtime_info = ""
        current_dst = 0
        
        for i, block in enumerate(msg.blocks):
            if block.type == "Fixed":
                runtime_info += f"{i+1}. dst[{current_dst}] ← src[{block.src_offset}] ({block.size}B)\n\n"
                current_dst += block.size
            
            elif block.type == "Dynamic":
                field = msg.fields[block.field_index] if block.field_index < len(msg.fields) else None
                if field:
                    count = self.var_field_values.get(block.field_index, tk.IntVar(value=0)).get()
                    data_size = count * field.element_size
                    
                    runtime_info += f"{i+1}. dst[{current_dst}] ← len={count} (4B)\n"
                    current_dst += 4
                    
                    if count > 0:
                        runtime_info += f"   dst[{current_dst}] ← src[{block.src_offset}]\n"
                        runtime_info += f"   ({count}×{field.element_size}B = {data_size}B)\n\n"
                        current_dst += data_size
                    else:
                        runtime_info += f"   (no data, count=0)\n\n"
            
            elif block.type == "RuntimeOffset":
                runtime_info += f"{i+1}. dst[{current_dst}] ← src[{block.src_offset}] ({block.size}B)\n\n"
                current_dst += block.size
        
        runtime_info += f"Total: {current_dst}B"
        self.copy_text_runtime.insert('1.0', runtime_info)
    
    def _setup_variable_controls(self, msg: MessageSchema):
        """Setup interactive controls for variable-size fields."""
        # Clear existing controls
        for widget in self.var_controls_inner.winfo_children():
            widget.destroy()
        self.var_field_controls.clear()
        self.var_field_values.clear()
        
        if not msg.has_variable_fields:
            # Hide the control panel
            self.var_controls_frame.pack_forget()
            return
        
        # Show the control panel
        self.var_controls_frame.pack(fill='x', pady=(0, 10), after=self.info_frame)
        
        # Create controls for each dynamic field
        row = 0
        for field in msg.fields:
            if not field.is_variable_length:
                continue
            
            # Create a frame for this field's controls
            field_frame = ttk.Frame(self.var_controls_inner)
            field_frame.grid(row=row, column=0, sticky='ew', pady=5)
            self.var_controls_inner.grid_columnconfigure(0, weight=1)
            
            # Field name label
            label = ttk.Label(field_frame, text=f"{field.name}:", font=('Consolas', 8))
            label.grid(row=0, column=0, sticky='w', padx=(0, 10))
            
            # IntVar for the value
            var = tk.IntVar(value=0)
            self.var_field_values[field.index] = var
            
            # Slider
            max_val = field.max_elements if field.max_elements > 0 else 100
            slider = ttk.Scale(field_frame, from_=0, to=max_val, 
                             orient='horizontal', variable=var,
                             command=lambda v, idx=field.index: self._on_var_field_change(idx))
            slider.grid(row=0, column=1, sticky='ew', padx=(0, 10))
            field_frame.grid_columnconfigure(1, weight=1)
            
            # Entry/spinbox for precise value
            spinbox = ttk.Spinbox(field_frame, from_=0, to=max_val, 
                                 textvariable=var, width=8,
                                 command=lambda idx=field.index: self._on_var_field_change(idx))
            spinbox.grid(row=0, column=2, sticky='e', padx=(0, 10))
            
            # Info label showing element size
            info_label = ttk.Label(field_frame, 
                                  text=f"(0-{max_val} × {field.element_size}B)",
                                  font=('Consolas', 8))
            info_label.grid(row=0, column=3, sticky='e')
            
            self.var_field_controls[field.index] = (label, slider, spinbox, info_label)
            row += 1
        
        # Initial update
        self._update_runtime_size()
    
    def _on_var_field_change(self, field_index: int):
        """Handle change in variable field control."""
        self._update_runtime_size()
        # Redraw memory layout with new sizes
        if self.current_message:
            self._draw_memory_layout(self.current_message)
            # Update runtime copy operations
            self._update_runtime_copy_operations(self.current_message)
    
    def _update_runtime_size(self):
        """Calculate and display runtime size based on current field values."""
        if not self.current_message or not self.current_message.has_variable_fields:
            self.runtime_size_var.set("Runtime size: N/A")
            return
        
        msg = self.current_message
        
        # Start with base packed size (fixed blocks + runtime offset blocks)
        runtime_size = msg.base_packed_size
        
        # Add dynamic field contributions
        dynamic_details = []
        for field in msg.fields:
            if not field.is_variable_length:
                continue
            
            count = self.var_field_values.get(field.index, tk.IntVar(value=0)).get()
            # Each dynamic field: length_prefix (4 bytes) + count * element_size
            field_size = field.header_size + (count * field.element_size)
            runtime_size += field_size
            dynamic_details.append(f"{field.name}={count}×{field.element_size}={count*field.element_size}B")
        
        # Format display
        display = f"Runtime size: {msg.base_packed_size}B (base)"
        if dynamic_details:
            display += f" + {' + '.join(dynamic_details)}"
        display += f" = {runtime_size}B total"
        
        self.runtime_size_var.set(display)
    
    def _draw_memory_layout(self, msg: MessageSchema):
        """Draw memory layout visualization."""
        self.memory_canvas.delete('all')
        self.memory_canvas.clear_regions()
        
        if not msg.fields:
            return
        
        canvas_width = self.memory_canvas.winfo_width() - 100
        if canvas_width < 200:
            canvas_width = 600
        
        bar_height = 40
        y_offset = 40
        
        # Calculate sizes for alignment
        struct_size = msg.sizeof_bytes
        packed_size = msg.packed_size
        
        # Calculate runtime packed size if variable fields present
        runtime_packed_size = msg.base_packed_size if msg.has_variable_fields else packed_size
        if msg.has_variable_fields:
            for field in msg.fields:
                if field.is_variable_length:
                    count = self.var_field_values.get(field.index, tk.IntVar(value=0)).get()
                    runtime_packed_size += field.header_size + (count * field.element_size)
        
        reference_size = max(struct_size, packed_size, runtime_packed_size)
        
        # Build struct regions
        struct_regions = []
        for i, field in enumerate(msg.fields):
            color = FIELD_COLORS[i % len(FIELD_COLORS)]
            
            if field.is_variable_length:
                # Split variable-length container into data_ and size_ parts
                # Layout: T data_[MaxSize]; size_type size_;
                data_size = field.element_size * field.max_elements
                size_field_size = field.size - data_size  # Typically 8 bytes (size_t)
                
                # Data array part
                data_info = f"Type: {field.type}\nOffset: {field.offset}\nData array: {data_size} bytes"
                data_info += f"\n[VARIABLE-LENGTH DATA]"
                data_info += f"\nElement size: {field.element_size} bytes"
                data_info += f"\nMax elements: {field.max_elements}"
                struct_regions.append((field.offset, data_size, color, f"{field.name}.data_", data_info,
                                       True, (field.element_size, field.max_elements)))
                
                # Size field part (at end of container)
                size_offset = field.offset + data_size
                size_info = f"Type: size_t\nOffset: {size_offset}\nSize: {size_field_size} bytes"
                size_info += f"\n[SIZE FIELD]"
                size_info += f"\nStores current element count (0-{field.max_elements})"
                struct_regions.append((size_offset, size_field_size, LENGTH_PREFIX_COLOR, 
                                      f"{field.name}.size_", size_info, False, None))
            else:
                # Regular field
                info = f"Type: {field.type}\nOffset: {field.offset}\nSize: {field.size} bytes"
                if field.padding_before > 0:
                    info += f"\nPadding before: {field.padding_before} bytes"
                struct_regions.append((field.offset, field.size, color, field.name, info, False, None))
        
        # Draw struct layout (with padding shown as gray)
        self.memory_canvas.draw_memory_bar(
            50, y_offset, canvas_width, bar_height,
            struct_size, struct_regions,
            "Struct (in memory) - striped = variable-length",
            show_empty_bg=True,
            reference_size=reference_size
        )
        
        # Build serialized regions using packed_offset (max capacity)
        serial_regions = []
        current_packed_offset = 0
        for i, field in enumerate(msg.fields):
            color = FIELD_COLORS[i % len(FIELD_COLORS)]
            
            # For variable-length fields, show length prefix + data only (not size_ field)
            if field.is_variable_length:
                # Add length prefix region
                prefix_info = f"[Length Prefix]\nField: {field.name}\nSize: {field.header_size}B"
                serial_regions.append((current_packed_offset, field.header_size, LENGTH_PREFIX_COLOR,
                                      f"len({field.name})", prefix_info, False, None))
                current_packed_offset += field.header_size
                
                # Add data region (max capacity = element_size * max_elements, NOT including size_ field)
                data_capacity = field.element_size * field.max_elements
                data_info = f"Field: {field.name}\nPacked offset: {field.packed_offset}\nData capacity: {data_capacity} bytes"
                data_info += f"\n[VARIABLE-LENGTH - size varies at runtime]"
                data_info += f"\nShown: max capacity ({field.max_elements} × {field.element_size}B)"
                serial_regions.append((current_packed_offset, data_capacity, color, field.name, data_info,
                                       True, (field.element_size, field.max_elements)))
                current_packed_offset += data_capacity
            else:
                # Fixed field - add normally
                info = f"Field: {field.name}\nPacked offset: {field.packed_offset}\nSize: {field.size} bytes"
                serial_regions.append((current_packed_offset, field.size, color, field.name, info, False, None))
                current_packed_offset += field.size
        
        # Draw serialized layout (max capacity)
        y_offset += bar_height + 50
        self.memory_canvas.draw_memory_bar(
            50, y_offset, canvas_width, bar_height,
            packed_size, serial_regions,
            "Serialized (max capacity) - striped = runtime variable size",
            show_empty_bg=False,
            reference_size=reference_size
        )
        
        # If variable fields, draw runtime serialized layout based on current slider values
        if msg.has_variable_fields:
            y_offset += bar_height + 50
            
            # Build runtime regions using blocks
            runtime_regions = []
            current_dst_offset = 0
            
            if msg.blocks:
                # Use block information for accurate layout
                for block in msg.blocks:
                    if block.type == "Fixed":
                        # Fixed block: copy multiple fields
                        for field_idx in range(block.field_start, block.field_start + block.field_count):
                            if field_idx < len(msg.fields):
                                field = msg.fields[field_idx]
                                color = FIELD_COLORS[field_idx % len(FIELD_COLORS)]
                                info = f"[Fixed Block]\nField: {field.name}\nSize: {field.size} bytes"
                                runtime_regions.append((current_dst_offset, field.size, color, field.name, info, False, None))
                                current_dst_offset += field.size
                    
                    elif block.type == "Dynamic":
                        # Dynamic block: variable-sized field
                        field = msg.fields[block.field_index]
                        color = FIELD_COLORS[block.field_index % len(FIELD_COLORS)]
                        count = self.var_field_values.get(block.field_index, tk.IntVar(value=0)).get()
                        
                        # Length prefix (4 bytes)
                        header_info = f"[Length Prefix]\nField: {field.name}\nSize: {field.header_size}B"
                        runtime_regions.append((current_dst_offset, field.header_size, LENGTH_PREFIX_COLOR, 
                                              f"len({field.name})", header_info, False, None))
                        current_dst_offset += field.header_size
                        
                        # Variable data
                        var_size = count * field.element_size
                        data_info = f"[Dynamic Block]\nField: {field.name}\nCount: {count}\nSize: {var_size}B ({count}×{field.element_size})"
                        if var_size > 0:
                            runtime_regions.append((current_dst_offset, var_size, color, field.name, 
                                                  data_info, True, (field.element_size, count)))
                        current_dst_offset += var_size
                    
                    elif block.type == "RuntimeOffset":
                        # RuntimeOffset block: fields that come after dynamic fields
                        for field_idx in range(block.field_start, block.field_start + block.field_count):
                            if field_idx < len(msg.fields):
                                field = msg.fields[field_idx]
                                color = FIELD_COLORS[field_idx % len(FIELD_COLORS)]
                                info = f"[Runtime Offset Block]\nField: {field.name}\nSize: {field.size} bytes\n(offset varies based on dynamic fields)"
                                runtime_regions.append((current_dst_offset, field.size, color, field.name, info, False, None))
                                current_dst_offset += field.size
            
            self.memory_canvas.draw_memory_bar(
                50, y_offset, canvas_width, bar_height,
                runtime_packed_size, runtime_regions,
                f"Runtime serialized ({runtime_packed_size}B) - current slider values",
                show_empty_bg=False,
                reference_size=reference_size
            )
        
        # Draw legend (multi-row if needed)
        y_offset += bar_height + 30
        self.memory_canvas.create_text(50, y_offset, text="Legend:", fill=TEXT_COLOR, 
                                       anchor='w', font=('Consolas', 8, 'bold'))
        
        # Calculate legend items per row based on canvas width
        max_x = 50 + canvas_width  # Use full canvas width
        x = 150
        row_height = 25  # Tighter spacing
        
        # Build all legend items first to better calculate spacing
        legend_items = []
        
        # Field colors
        for i, field in enumerate(msg.fields):
            color = FIELD_COLORS[i % len(FIELD_COLORS)]
            label = field.name
            if field.is_variable_length:
                label = f"~{field.name}"
            is_striped = field.is_variable_length
            legend_items.append(('field', color, label, is_striped))
        
        # Padding (if present)
        if msg.has_padding:
            legend_items.append(('simple', PADDING_COLOR, 'padding', False))
        
        # Length prefix (if variable fields)
        if msg.has_variable_fields:
            legend_items.append(('simple', LENGTH_PREFIX_COLOR, 'len/size', False))
        
        # Draw legend items with proper wrapping
        for item_type, color, label, is_striped in legend_items:
            item_width = max(len(label) * 12 + 50, 70)  # Smaller text calculation
            
            # Check if we need to wrap to next row
            if x + item_width > max_x:
                y_offset += row_height
                x = 150
            
            # Draw legend box
            self.memory_canvas.create_rectangle(x, y_offset - 8, x + 15, y_offset + 8, fill=color, outline='#333')
            
            # Add stripes if needed
            if is_striped:
                for sx in range(x, x + 15, 4):
                    self.memory_canvas.create_line(sx, y_offset + 8, sx + 8, y_offset - 8, 
                                                  fill='#000000', width=1)
            
            # Draw label
            self.memory_canvas.create_text(x + 20, y_offset, text=label, fill=TEXT_COLOR,
                                          anchor='w', font=('Consolas', 8))
            x += item_width
        
        # Variable-length indicator in legend
        has_variable = any(f.is_variable_length for f in msg.fields)
        if has_variable:
            y_offset += row_height
            self.memory_canvas.create_text(50, y_offset, 
                text="~ = variable-length (striped). Struct shows: data_[N] + size_. Serialized shows: len prefix + actual data",
                fill='#95a5a6', anchor='w', font=('Consolas', 7, 'italic'))


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='SeRTial Message Schema Visualizer (GUI)')
    parser.add_argument('schema_file', nargs='?', default=None,
                        help='JSON schema file to load')
    
    args = parser.parse_args()
    
    root = tk.Tk()
    app = SchemaVisualizerApp(root, args.schema_file)
    root.mainloop()


if __name__ == '__main__':
    main()
