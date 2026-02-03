#!/usr/bin/env python3
"""
SeRTial GUI Widgets

Reusable GUI components for the schema visualizer.
Extracted from the full-featured GUI implementation.
"""

import tkinter as tk
from tkinter import ttk
from typing import List, Tuple, Callable, Dict, Any, Optional


# Color constants matching the main GUI
FIELD_COLORS = [
    "#c21300",  # Red
    "#0067ab",  # Blue
    "#039540",  # Green
    "#66008f",  # Purple
    "#c29b00",  # Yellow
    "#ac5000",  # Orange
    "#009a7b",  # Turquoise
    "#004386",  # Dark Blue
]

PADDING_COLOR = '#7f8c8d'       # Gray
LENGTH_PREFIX_COLOR = "#ddff00"  # Yellow - distinct from padding
BG_COLOR = '#2c3e50'            # Dark background
TEXT_COLOR = '#ecf0f1'          # Light text
PANEL_BG = '#34495e'            # Panel background


class MemoryCanvas(tk.Canvas):
    """
    Canvas for drawing memory layout visualizations with full features.
    
    Supports:
    - Multiple memory bars with alignment
    - Color-coded regions
    - Diagonal stripes for variable-length fields
    - Interactive tooltips
    - Reference scaling for aligned bars
    """
    
    def __init__(self, parent, **kwargs):
        super().__init__(parent, bg=PANEL_BG, highlightthickness=0, **kwargs)
        self.tooltip = None
        self.tooltip_id = None
        self.all_regions = []  # Store all regions from all bars for tooltips
        
        # Bind mouse events for tooltips
        self.bind('<Motion>', self._on_motion)
        self.bind('<Leave>', self._hide_tooltip)
    
    def clear_regions(self):
        """Clear all stored regions."""
        self.all_regions = []
    
    def draw_memory_bar(self, x: int, y: int, width: int, height: int, 
                       total_size: int, regions: List[Tuple], label: str = "", 
                       show_empty_bg: bool = True, reference_size: Optional[int] = None):
        """
        Draw a memory bar with colored regions.
        
        Args:
            x, y: Position
            width, height: Bar dimensions
            total_size: Total size in bytes
            regions: List of (start, length, color, name, info, is_variable, var_info)
                    where var_info is (element_size, max_elements) or None
                    Can also be (start, length, color, name, info) for compatibility
            label: Bar label
            show_empty_bg: If True, show gray background for unused space
            reference_size: If provided, scale positions to this size for alignment
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
        
        # Draw each region (scaled to reference_size so bars align)
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
    
    def _draw_stripes(self, x1: float, y1: float, x2: float, y2: float, base_color: str):
        """Draw diagonal stripes to indicate variable-length region."""
        stripe_spacing = 8
        stripe_width = 3
        
        # Darken the base color for stripes
        r = int(base_color[1:3], 16)
        g = int(base_color[3:5], 16)
        b = int(base_color[5:7], 16)
        dark_color = f'#{max(0,r-40):02x}{max(0,g-40):02x}{max(0,b-40):02x}'
        
        # Draw diagonal stripes from bottom-left to top-right
        for offset in range(-int(y2-y1), int(x2-x1) + int(y2-y1), stripe_spacing):
            lx1 = x1 + offset
            ly1 = y2
            lx2 = x1 + offset + (y2 - y1)
            ly2 = y1
            
            # Clip to rectangle bounds
            if lx2 < x1 or lx1 > x2:
                continue
            
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
    
    def _show_tooltip(self, x: int, y: int, name: str, info: str):
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


class VariableFieldControl(ttk.Frame):
    """Widget for controlling a variable-size field with a slider."""
    
    def __init__(self, parent, field_name: str, max_elements: int,
                 on_change: Callable[[int], None] = None):
        super().__init__(parent)
        
        self.field_name = field_name
        self.max_elements = max_elements
        self.on_change = on_change
        
        # Create widgets
        self.label = ttk.Label(self, text=f"{field_name}:", width=25)
        self.label.pack(side='left', padx=(0, 5))
        
        self.value_var = tk.IntVar(value=0)
        
        self.slider = ttk.Scale(
            self, from_=0, to=max_elements,
            orient='horizontal',
            variable=self.value_var,
            command=self._on_slider_change
        )
        self.slider.pack(side='left', fill='x', expand=True, padx=5)
        
        self.value_label = ttk.Label(self, text="0", width=10, anchor='e')
        self.value_label.pack(side='left', padx=(5, 0))
        
        self.max_label = ttk.Label(self, text=f"(max: {max_elements})",
                                   font=('Arial', 8))
        self.max_label.pack(side='left', padx=(5, 0))
    
    def _on_slider_change(self, value):
        """Handle slider value change."""
        int_value = int(float(value))
        self.value_label.config(text=str(int_value))
        
        if self.on_change:
            self.on_change(int_value)
    
    def get_value(self) -> int:
        """Get current slider value."""
        return int(self.value_var.get())
    
    def set_value(self, value: int):
        """Set slider value."""
        self.value_var.set(value)


class MessageListBox(ttk.Frame):
    """Enhanced listbox for displaying messages with filtering."""
    
    def __init__(self, parent, on_select: Callable[[str], None] = None):
        super().__init__(parent)
        
        self.on_select = on_select
        self.messages = []  # Store original message data
        
        # Search box
        search_frame = ttk.Frame(self)
        search_frame.pack(fill='x', pady=(0, 5))
        
        ttk.Label(search_frame, text="Search:").pack(side='left')
        self.search_var = tk.StringVar()
        self.search_var.trace_add('write', lambda *args: self._filter())
        
        search_entry = ttk.Entry(search_frame, textvariable=self.search_var)
        search_entry.pack(side='left', fill='x', expand=True, padx=(5, 0))
        
        # Listbox with scrollbar
        list_frame = ttk.Frame(self)
        list_frame.pack(fill='both', expand=True)
        
        scrollbar = ttk.Scrollbar(list_frame, orient='vertical')
        scrollbar.pack(side='right', fill='y')
        
        self.listbox = tk.Listbox(
            list_frame,
            font=('Consolas', 9),
            yscrollcommand=scrollbar.set
        )
        self.listbox.pack(side='left', fill='both', expand=True)
        scrollbar.config(command=self.listbox.yview)
        
        self.listbox.bind('<<ListboxSelect>>', self._on_select)
    
    def set_messages(self, messages: List[Dict[str, Any]]):
        """Set the list of messages to display."""
        self.messages = messages
        self._filter()
    
    def _filter(self):
        """Filter messages based on search text."""
        self.listbox.delete(0, 'end')
        search_term = self.search_var.get().lower()
        
        for msg in self.messages:
            name = msg.get('name', '')
            if search_term and search_term not in name.lower():
                continue
            
            # Format display string with flags
            flags = []
            if msg.get('can_single_memcpy', False):
                flags.append('1')
            if msg.get('has_padding', False):
                flags.append('P')
            if msg.get('has_variable_fields', False):
                flags.append('~')
            
            flag_str = ''.join(flags) if flags else ' '
            display = f"[{flag_str:3s}] {name}"
            self.listbox.insert('end', display)
    
    def _on_select(self, event):
        """Handle selection event."""
        selection = self.listbox.curselection()
        if not selection:
            return
        
        # Extract message name from display string
        display = self.listbox.get(selection[0])
        name = display.split('] ', 1)[1] if ']' in display else display
        
        if self.on_select:
            self.on_select(name)


class FieldsTable(ttk.Frame):
    """Table widget for displaying field information."""
    
    def __init__(self, parent):
        super().__init__(parent)
        
        # Configure treeview style
        style = ttk.Style()
        style.configure('Fields.Treeview', rowheight=28, font=('Consolas', 9))
        style.configure('Fields.Treeview.Heading',
                       font=('Consolas', 9, 'bold'), padding=(5, 8))
        
        # Create treeview
        columns = ('type', 'offset', 'size', 'packed', 'padding')
        self.tree = ttk.Treeview(
            self,
            columns=columns,
            show='tree headings',
            height=10,
            style='Fields.Treeview'
        )
        
        # Configure columns
        self.tree.heading('#0', text='Name')
        self.tree.heading('type', text='Type')
        self.tree.heading('offset', text='Offset')
        self.tree.heading('size', text='Size')
        self.tree.heading('packed', text='Packed')
        self.tree.heading('padding', text='Padding')
        
        self.tree.column('#0', width=180)
        self.tree.column('type', width=280)
        self.tree.column('offset', width=80, anchor='e')
        self.tree.column('size', width=80, anchor='e')
        self.tree.column('packed', width=80, anchor='e')
        self.tree.column('padding', width=80, anchor='e')
        
        # Scrollbar
        scrollbar = ttk.Scrollbar(self, orient='vertical',
                                 command=self.tree.yview)
        self.tree.configure(yscrollcommand=scrollbar.set)
        
        self.tree.pack(side='left', fill='both', expand=True)
        scrollbar.pack(side='right', fill='y')
    
    def clear(self):
        """Clear all rows."""
        for item in self.tree.get_children():
            self.tree.delete(item)
    
    def add_field(self, name: str, ftype: str, offset: int, size: int,
                  packed: int, padding: int = 0):
        """Add a field row."""
        padding_str = str(padding) if padding > 0 else ''
        values = (ftype, offset, size, packed, padding_str)
        self.tree.insert('', 'end', text=name, values=values)
