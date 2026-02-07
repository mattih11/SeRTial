# SeRTial Schema Viewer

**Navigation**: [Main README](../../README.md) | [User Guide](../../docs/USER_GUIDE.md) | [Schema Viewer Guide](../../docs/SCHEMA_VIEWER.md) | [Examples](../../docs/EXAMPLES.md)

---

Interactive HTML viewer for SeRTial schema JSON files.

## Usage

Open `viewer.html` in a browser and use the file picker to load a schema JSON file, or pass the schema as a URL parameter:

```
https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json
```

## Features

- **Interactive Visualization**: See struct layout, serialized layout, and operations
- **Variable Field Controls**: Adjust runtime sizes with sliders
- **Animation**: Watch how packed size changes with different field sizes
- **Hover Highlighting**: Hover over fields, memory regions, or operations to see connections
- **Collapsible Sections**: Fold/unfold different visualizations
- **Multi-field Blocks**: Hover over copy operations shows all affected fields

## Generating Schema Files

To generate schema JSON from your types:

```bash
# Build the schema example
cd build
make schema_example

# Copy to examples directory (committed to git)
cp my_schemas.json ../examples/schemas/example_schemas.json
```

Or use the schema generation API in your own code (see `examples/schema_example.cpp`).
