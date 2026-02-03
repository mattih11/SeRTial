#!/usr/bin/env python3
"""
SeRTial CLI Renderer

Terminal-based visualization of message schemas.
"""

from typing import List, Tuple
from sertial_common import MessageSchema, SchemaAnalyzer


class Colors:
    """ANSI color codes for terminal output."""
    RESET = '\033[0m'
    BOLD = '\033[1m'
    DIM = '\033[2m'
    
    # Field colors (cycle through these for different fields)
    FIELD_COLORS = [
        '\033[48;5;31m',   # Blue
        '\033[48;5;35m',   # Green  
        '\033[48;5;136m',  # Yellow/Orange
        '\033[48;5;132m',  # Magenta
        '\033[48;5;37m',   # Cyan
        '\033[48;5;167m',  # Red
        '\033[48;5;98m',   # Purple
        '\033[48;5;29m',   # Teal
    ]
    
    PADDING = '\033[48;5;236m'  # Dark gray for padding
    HEADER = '\033[48;5;240m'   # Medium gray for headers
    
    # Text colors
    WHITE = '\033[97m'
    BLACK = '\033[30m'
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    CYAN = '\033[96m'


class CLIRenderer:
    """Render message schemas in the terminal."""
    
    def __init__(self, use_color: bool = True):
        self.use_color = use_color
    
    def _color(self, code: str) -> str:
        """Return color code if colors are enabled."""
        return code if self.use_color else ''
    
    def render_memory_bar(self, size: int, regions: List[Tuple], label: str = "", width: int = 60) -> str:
        """
        Render a visual memory layout bar with colors.
        
        Args:
            size: Total size in bytes
            regions: List of (start, length, color, label, is_variable) tuples
            label: Optional label for the bar
            width: Display width in characters
        """
        if size == 0:
            return ""
        
        # Create bar with proper scaling
        scale = width / size
        bar = [' '] * width
        colors_list = [''] * width
        is_variable = [False] * width
        boundaries = set()  # Track color boundaries
        
        for region_data in regions:
            start, length, color, field_label = region_data[:4]
            var_field = region_data[4] if len(region_data) > 4 else False
            
            bar_start = int(start * scale)
            bar_end = min(int((start + length) * scale), width)
            
            # Ensure at least 1 character is shown for any non-zero region
            if bar_end <= bar_start and length > 0:
                bar_end = bar_start + 1
            
            if bar_end > bar_start:
                boundaries.add(bar_start)
                boundaries.add(bar_end)
            
            for i in range(bar_start, bar_end):
                if i < width:  # Safety check
                    # Use different character for variable fields
                    bar[i] = '≈' if var_field else '█'
                    colors_list[i] = self._color(color) + self._color(Colors.WHITE)
                    is_variable[i] = var_field
        
        # Build colored string with field boundaries
        result = []
        current_color = ''
        
        for i, char in enumerate(bar):
            # Add a subtle boundary marker between fields
            if i > 0 and i in boundaries and bar[i] != ' ' and bar[i-1] != ' ':
                if colors_list[i] != colors_list[i-1]:
                    result.append(self._color(Colors.RESET))
                    result.append(self._color(Colors.DIM) + '│' + self._color(Colors.RESET))
                    current_color = ''
            
            if colors_list[i] != current_color:
                if current_color:
                    result.append(self._color(Colors.RESET))
                result.append(colors_list[i])
                current_color = colors_list[i]
            result.append(char)
        
        if current_color:
            result.append(self._color(Colors.RESET))
        
        bar_str = ''.join(result)
        
        if label:
            return f"  {label:20s} {bar_str}  {self._color(Colors.DIM)}{size}B{self._color(Colors.RESET)}"
        return f"  {bar_str}  {self._color(Colors.DIM)}{size}B{self._color(Colors.RESET)}"
    
    def visualize_message(self, msg: MessageSchema):
        """Display detailed visualization of a message."""
        print(f"\n{self._color(Colors.BOLD)}{self._color(Colors.CYAN)}"
              f"{'='*80}{self._color(Colors.RESET)}")
        print(f"{self._color(Colors.BOLD)}{msg.name}{self._color(Colors.RESET)} "
              f"({msg.category})")
        print(f"{self._color(Colors.CYAN)}{'='*80}{self._color(Colors.RESET)}\n")
        
        # Basic info
        print(f"  sizeof:           {msg.sizeof_bytes} bytes")
        print(f"  packed size:      {msg.packed_size} bytes")
        if msg.has_variable_fields:
            print(f"  base packed:      {msg.base_packed_size} bytes")
        print(f"  has padding:      {msg.has_padding}")
        print(f"  variable fields:  {msg.has_variable_fields}")
        print(f"  single memcpy:    {msg.can_single_memcpy}")
        print(f"  memcpy regions:   {msg.memcpy_region_count}")
        
        if msg.has_padding:
            print(f"\n  {self._color(Colors.YELLOW)}⚠ Padding: {msg.padding_bytes} bytes "
                  f"eliminated in serialization{self._color(Colors.RESET)}")
        
        # Field layout with color legend
        print(f"\n{self._color(Colors.BOLD)}Fields:{self._color(Colors.RESET)}")
        print(f"  {'Name':<20} {'Type':<30} {'Offset':>6} {'Size':>6} {'Packed':>6}")
        print(f"  {'-'*78}")
        
        for i, field in enumerate(msg.fields):
            color = Colors.FIELD_COLORS[i % len(Colors.FIELD_COLORS)]
            color_block = f"{self._color(color)} {self._color(Colors.RESET)}"
            
            padding_marker = ""
            if field.padding_before > 0:
                padding_marker = f" {self._color(Colors.YELLOW)}[+{field.padding_before}]{self._color(Colors.RESET)}"
            
            var_marker = f"{self._color(Colors.CYAN)}~{self._color(Colors.RESET)}" if field.is_variable_length else " "
            field_name_colored = f"{self._color(color)}{self._color(Colors.WHITE)}{field.name}{self._color(Colors.RESET)}"
            
            # Calculate spacing to account for ANSI codes
            name_display_len = len(field.name) + 1  # +1 for var_marker
            spacing = ' ' * (20 - name_display_len)
            
            print(f"  {color_block}{var_marker}{field_name_colored}{spacing}{field.type:<30} "
                  f"{field.offset:>6} {field.size:>6} {field.packed_offset:>6}{padding_marker}")
        
        # Memory visualization
        print(f"\n{self._color(Colors.BOLD)}Memory Layout:{self._color(Colors.RESET)}")
        
        # Calculate reference size for alignment (use the larger of struct/packed)
        max_size = max(msg.sizeof_bytes, msg.packed_size)
        max_width = 70
        
        # Build regions for struct layout with padding
        struct_regions = []
        for i, field in enumerate(msg.fields):
            color = Colors.FIELD_COLORS[i % len(Colors.FIELD_COLORS)]
            if field.padding_before > 0:
                pad_color = Colors.PADDING
                struct_regions.append((field.offset - field.padding_before, field.padding_before, pad_color, "padding", False))
            struct_regions.append((field.offset, field.size, color, field.name, field.is_variable_length))
        
        # Calculate actual width based on proportion
        struct_width = int((msg.sizeof_bytes / max_size) * max_width)
        print(self.render_memory_bar(msg.sizeof_bytes, struct_regions, label="Struct layout", width=struct_width))
        
        # Show padding legend if applicable
        if msg.has_padding:
            print(f"    {self._color(Colors.PADDING)}{self._color(Colors.WHITE)} {self._color(Colors.RESET)} = padding")
        
        # Show variable field legend if applicable
        if msg.has_variable_fields:
            print(f"    {self._color(Colors.CYAN)}≈{self._color(Colors.RESET)} = variable-length field")
        
        # Build regions for packed layout (no padding)
        packed_regions = []
        for i, field in enumerate(msg.fields):
            color = Colors.FIELD_COLORS[i % len(Colors.FIELD_COLORS)]
            packed_regions.append((field.packed_offset, field.size, color, field.name, field.is_variable_length))
        
        # Calculate actual width based on proportion
        packed_width = int((msg.packed_size / max_size) * max_width)
        print(self.render_memory_bar(msg.packed_size, packed_regions, label="Packed layout", width=packed_width))
        
        # Memcpy regions
        if msg.memcpy_regions:
            print(f"\n{self._color(Colors.BOLD)}Memcpy Regions:{self._color(Colors.RESET)}")
            for i, region in enumerate(msg.memcpy_regions):
                # Get field names for this region
                field_names = []
                for field_idx in range(region.field_start, region.field_start + region.field_count):
                    if field_idx < len(msg.fields):
                        field = msg.fields[field_idx]
                        var_marker = "~" if field.is_variable_length else ""
                        field_names.append(f"{var_marker}{field.name}")
                
                fields_str = ", ".join(field_names) if field_names else "(none)"
                print(f"  Region {i}: "
                      f"{self._color(Colors.CYAN)}src={region.src_offset}{self._color(Colors.RESET)} → "
                      f"{self._color(Colors.GREEN)}dst={region.dst_offset}{self._color(Colors.RESET)} "
                      f"size={region.size}B [{fields_str}]")
        
        # HybridMemoryMap blocks
        if msg.blocks:
            print(f"\n{self._color(Colors.BOLD)}HybridMemoryMap Blocks:{self._color(Colors.RESET)}")
            for block in msg.blocks:
                if block.type == "fixed":
                    print(f"  {self._color(Colors.GREEN)}[FIXED]{self._color(Colors.RESET)} "
                          f"src={block.src_offset} dst={block.dst_offset} size={block.size}")
                elif block.type == "padding":
                    print(f"  {self._color(Colors.YELLOW)}[PADDING]{self._color(Colors.RESET)} "
                          f"src={block.src_offset} size={block.size}")
                elif block.type == "dynamic":
                    print(f"  {self._color(Colors.CYAN)}[DYNAMIC]{self._color(Colors.RESET)} "
                          f"{block.field_name} max={block.max_elements}")
                elif block.type == "runtime_offset":
                    print(f"  {self._color(Colors.MAGENTA)}[RUNTIME]{self._color(Colors.RESET)} "
                          f"{block.field_name} size={block.size}")
    
    def print_summary(self, messages: List[MessageSchema]):
        """Print summary table of all messages."""
        print(f"\n{self._color(Colors.BOLD)}{self._color(Colors.CYAN)}"
              f"{'='*90}{self._color(Colors.RESET)}")
        print(f"{self._color(Colors.BOLD)}Schema Summary{self._color(Colors.RESET)}")
        print(f"{self._color(Colors.CYAN)}{'='*90}{self._color(Colors.RESET)}\n")
        
        # Table header
        print(f"{'Name':<30} {'Category':<12} {'sizeof':>7} {'packed':>7} {'pad':>4} "
              f"{'blks':>4} {'dyn':>3} {'Flags':<15}")
        print('-' * 90)
        
        # Table rows
        for msg in messages:
            dyn_count = sum(1 for b in msg.blocks if b.type == "Dynamic") if msg.blocks else 0
            flags = SchemaAnalyzer.get_flags(msg)
            
            print(f"{msg.name:<30} {msg.category:<12} {msg.sizeof_bytes:>7} "
                  f"{msg.packed_size:>7} {msg.padding_bytes:>4} {msg.memcpy_region_count:>4} "
                  f"{dyn_count:>3} {flags:<15}")
        
        # Statistics
        print('-' * 90)
        total_padding = SchemaAnalyzer.compute_padding_eliminated(messages)
        single_memcpy = SchemaAnalyzer.count_single_memcpy(messages)
        variable_count = SchemaAnalyzer.count_variable_messages(messages)
        avg_blocks = SchemaAnalyzer.average_blocks(messages)
        
        print(f"\n{self._color(Colors.BOLD)}Statistics:{self._color(Colors.RESET)}")
        print(f"  Total messages:          {len(messages)}")
        print(f"  Padding eliminated:      {total_padding} bytes")
        print(f"  Single-memcpy messages:  {single_memcpy}/{len(messages)}")
        print(f"  Variable-size messages:  {variable_count}/{len(messages)}")
        print(f"  Average blocks/message:  {avg_blocks:.2f}")
