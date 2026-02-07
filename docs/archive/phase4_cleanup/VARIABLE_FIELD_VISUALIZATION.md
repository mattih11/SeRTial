# Variable Field Visualization - Interactive GUI Enhancement

## Overview

Enhanced the SeRTial schema visualizer GUI with interactive controls for variable-size fields, enabling real-time visualization of dynamic serialization behavior.

## What Was Added

### 1. Schema Data Structures

**New BlockInfo dataclass** (`visualize_schema_gui.py`):
```python
@dataclass
class BlockInfo:
    type: str           # "Fixed", "Padding", "Dynamic", "RuntimeOffset"
    src_offset: int
    dst_offset: int
    size: int
    field_index: int
    field_start: int
    field_count: int
    is_variable: bool
```

**Enhanced MessageSchema** with hybrid memory map fields:
- `has_variable_fields: bool`
- `base_packed_size: int` - Size before dynamic fields
- `fixed_block_count`, `dynamic_block_count`, `runtime_offset_block_count: int`
- `blocks: List[BlockInfo]` - Complete block execution order

### 2. Interactive Controls Panel

**Variable Field Controls** (appears when variable-size message is selected):
- **Sliders**: Horizontal slider for each variable field (0 to max_elements)
- **Spinboxes**: Precise numeric input for element counts
- **Info Labels**: Show element size and range (e.g., "0-256 × 12B")
- **Runtime Size Display**: Real-time calculation showing breakdown:
  ```
  Runtime size: 32B (base) + points=10×12=120B = 156B total
  ```

### 3. Enhanced Memory Visualization

**Three-Bar Layout** for variable-size messages:

1. **Struct (in memory)**: Shows maximum capacity with variable fields striped
2. **Serialized (max capacity)**: Maximum possible serialized size
3. **Runtime serialized**: **NEW!** - Actual layout based on current slider values
   - Shows length prefixes (4-byte headers) in gray
   - Dynamic blocks scaled to actual element count
   - Runtime offset blocks shift position correctly

**Visual Enhancements**:
- Striped pattern for variable-size fields
- Gray header boxes for 4-byte length prefixes
- Color-coded blocks matching field legend
- Hover tooltips showing block details

### 4. Updated UI Elements

**Message List Indicators**:
- `[1]` = Single memcpy (fixed-size)
- `o` = Multiple memcpy regions
- `P` = Has padding
- `~` = **NEW!** Has variable-size fields

**Info Panel Additions**:
```
Hybrid Blocks:  1 Fixed, 1 Dynamic, 0 RuntimeOffset
Base size:      32 bytes (before dynamic fields)
[VARIABLE]      Runtime size varies based on field content
```

## Example: PointCloud Message

### Schema Structure
```
Message: PointCloud<Point3D<float>, 256>
  Fields: 2
    [0] header: Header (32 bytes) - Fixed
    [1] points: vector<Point3D> (max 256×12B = 3072B) - Variable

  Blocks: 2
    [0] Fixed:   src=0, dst=0, size=32B
    [1] Dynamic: src=32, field_index=1
```

### Runtime Size Calculation

| Slider Value | Calculation | Total Size |
|--------------|-------------|------------|
| 0 elements   | 32B + (4B + 0×12B) | **36B** |
| 10 elements  | 32B + (4B + 10×12B) | **156B** |
| 100 elements | 32B + (4B + 100×12B) | **1236B** |
| 256 elements | 32B + (4B + 256×12B) | **3108B** |

### Visual Layout at 10 Elements

```
┌─────────────────────────┬───────┬────────────────────────────┐
│   Header (32B)          │ len:4B│  points data (10×12B = 120B) │
└─────────────────────────┴───────┴────────────────────────────┘
Offset:  0                 32      36                          156
```

## Usage

### Starting the GUI
```bash
cd build
python3 ../scripts/visualize_schema_gui.py my_schemas.json
```

### Interactive Workflow
1. **Select a variable-size message** (marked with `~` in the list)
2. **Variable Field Controls panel appears** below the info panel
3. **Adjust sliders** to set element counts (0 to max_elements)
4. **Watch real-time updates**:
   - Runtime size display recalculates
   - Third memory bar redraws with new layout
   - Length prefixes and data blocks resize appropriately

### Features in Action

**For PointCloud message**:
- Slider labeled "points: [0────────────256]"
- Spinbox for precise input
- Info: "(0-256 × 12B)"
- Runtime size: "32B (base) + points=100×12=1200B = 1236B total"

**Memory bars show**:
1. Struct: Full 3104B capacity (32B header + 3072B max vector)
2. Max serialized: 3108B (includes 4B length prefix)
3. **Runtime**: 1236B actual (header + prefix + 100 elements)

## Implementation Details

### Runtime Size Calculation
```python
def _update_runtime_size(self):
    runtime_size = msg.base_packed_size  # Fixed blocks + RuntimeOffset blocks
    
    for field in msg.fields:
        if field.is_variable_length:
            count = self.var_field_values[field.index].get()
            field_size = field.header_size + (count * field.element_size)
            runtime_size += field_size
    
    # Display: "32B (base) + points=10×12=120B = 156B total"
```

### Block-Based Layout Generation
```python
def _draw_memory_layout(self, msg):
    current_offset = 0
    
    for block in msg.blocks:
        if block.type == "Fixed":
            # Draw fixed-size fields
            for field_idx in range(block.field_start, block.field_start + block.field_count):
                draw_field(current_offset, field.size, color)
                current_offset += field.size
        
        elif block.type == "Dynamic":
            # Draw length prefix
            draw_header(current_offset, 4, HEADER_COLOR)
            current_offset += 4
            
            # Draw variable data
            count = slider_value[block.field_index]
            data_size = count * element_size
            draw_variable_field(current_offset, data_size, color, striped=True)
            current_offset += data_size
        
        elif block.type == "RuntimeOffset":
            # Draw fields that shift with dynamic content
            draw_fields(current_offset, block.size, color)
            current_offset += block.size
```

## Benefits

1. **Educational**: Visualizes how HybridMemoryMap serialization works
2. **Debugging**: Verify runtime size calculations match expectations
3. **Optimization**: See memory impact of different container capacities
4. **Documentation**: Interactive demonstration of variable-size serialization

## Files Modified

1. **`scripts/visualize_schema_gui.py`** (951 lines)
   - Added `BlockInfo` dataclass
   - Enhanced `MessageSchema` with hybrid fields
   - Added `_setup_variable_controls()` method
   - Added `_update_runtime_size()` method
   - Enhanced `_draw_memory_layout()` with block-based rendering
   - Updated message list indicators and info panel

2. **`README.md`**
   - Added "Interactive Variable-Size Field Controls" section
   - Documented new GUI features and usage
   - Added example walkthrough for PointCloud

## Testing

### Validation Test
```bash
cd build

# Test schema parsing
python3 -c "
import sys
sys.path.insert(0, '../scripts')
from visualize_schema_gui import load_schemas, parse_message

data = load_schemas('my_schemas.json')
messages = [parse_message(m) for m in data['messages']]

for msg in messages:
    if msg.has_variable_fields:
        print(f'{msg.name}: {msg.dynamic_block_count} dynamic blocks')
"
```

**Expected Output**:
```
PointCloud<Point3D<float>, 256>: 1 dynamic blocks
IMUMeasurements<10>: 1 dynamic blocks
CameraFrame<1024>: 1 dynamic blocks
```

### Runtime Size Simulation
```python
# Simulate different element counts
for count in [0, 10, 100, 256]:
    runtime_size = base_size + (4 + count * element_size)
    print(f"Count={count}: {runtime_size}B")
```

**Results** (for PointCloud):
```
Count=0: 36B
Count=10: 156B
Count=100: 1236B
Count=256: 3108B
```

All calculations match expected values!

## Future Enhancements

Potential additions:
- [ ] Export snapshot of current configuration to JSON
- [ ] Preset configurations (empty, half-full, full)
- [ ] Multiple dynamic fields with independent controls
- [ ] Bandwidth calculator (runtime_size × message_rate)
- [ ] Compare two different fill levels side-by-side
- [ ] Animation showing serialization process block-by-block

## Conclusion

The enhanced GUI visualizer provides complete interactive control over variable-size field visualization, enabling users to:
- **See** how dynamic fields affect serialized size
- **Understand** block-based serialization execution order
- **Verify** runtime size calculations are correct
- **Optimize** message designs based on actual usage patterns

The implementation successfully integrates with the existing HybridMemoryMap infrastructure, reading block information directly from the schema JSON and providing an intuitive, real-time interface for exploring dynamic serialization behavior.
