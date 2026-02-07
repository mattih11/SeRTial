# SeRTial Schema Viewer

Interactive HTML viewer for SeRTial schema JSON files.

## Usage

### Option 1: Load from File
Open `viewer.html` in a browser and use the file picker to load a schema JSON file.

### Option 2: Load from URL
Pass the schema JSON URL as a query parameter:

```bash
# Local file (requires HTTP server)
viewer.html?schema=../../examples/schemas/example_schemas.json

# Remote URL (GitHub raw)
viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json

# GitHub Pages example
https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json
```

### Option 3: Serve Locally
```bash
cd tools/sertial-inspect
python3 -m http.server 8080
# Open: http://localhost:8080/viewer.html?schema=../../examples/schemas/example_schemas.json
```

## Features

- 🎨 **Interactive Visualization**: See struct layout, serialized layout, and operations
- 📊 **Variable Field Controls**: Adjust runtime sizes with sliders
- 🎬 **Animation**: Watch how packed size changes with different field sizes
- 🔦 **Hover Highlighting**: Hover over fields, memory regions, or operations to see connections
- 📂 **Collapsible Sections**: Fold/unfold different visualizations
- 🎯 **Multi-field Blocks**: Hover over copy operations shows all affected fields

## Embedding in GitHub README

### Using GitHub Pages
1. Enable GitHub Pages in repository settings (source: main branch, /docs or root)
2. Commit schema JSON to repository
3. Link to viewer with schema parameter:

```markdown
## Interactive Schema Viewer

[View Schema Interactively](https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json)
```

### Using HTML Embed (Limited Support)
GitHub Markdown doesn't support `<iframe>` for security reasons, but you can:

1. **Link to viewer**:
   ```markdown
   [📊 Interactive Schema Viewer](./tools/sertial-inspect/viewer.html?schema=./build/my_schemas.json)
   ```

2. **Use GitHub Actions** to generate static images/SVGs from the viewer

3. **Use external hosting**: Host on GitHub Pages, Netlify, or Vercel

### Static Screenshots Alternative
Generate screenshots for README:
```bash
# Using browser automation (e.g., Playwright)
playwright screenshot viewer.html --wait-for-selector=".details" --output=schema.png
```

Then embed in README:
```markdown
![Schema Visualization](docs/images/schema.png)

[View Interactive Version](https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html?schema=https://raw.githubusercontent.com/mattih11/SeRTial/main/examples/schemas/example_schemas.json)
```

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

## Schema JSON Format

Expected format:
```json
{
  "messages": [
    "{\"name\":\"MyType\",\"sizeof_bytes\":32,...}",
    "{\"name\":\"AnotherType\",...}"
  ]
}
```

Each message is a JSON-stringified schema object with fields:
- `name`: Type name
- `sizeof_bytes`: Struct size in memory
- `base_packed_size`: Minimum serialized size
- `max_packed_size`: Maximum serialized size (with variable fields at capacity)
- `field_names`: Array of field names
- `field_types`: Array of field type strings
- `field_sizes`: Array of field sizes in bytes
- `field_offsets`: Array of field offsets in struct
- `field_alignments`: Array of field alignment requirements
- `field_is_variable`: Array of booleans (true if variable-length)
- `capacities`: Array of max elements for variable fields
- `element_sizes`: Array of element sizes for variable fields

## CORS Considerations

When loading schemas from URLs, the server must support CORS. For local development:

```bash
# Python HTTP server (automatically sets CORS headers)
python3 -m http.server 8080 --bind 127.0.0.1

# Or use a proper CORS-enabled server
npx http-server -p 8080 --cors
```

For GitHub raw files, CORS is enabled by default via jsdelivr CDN:
```
https://cdn.jsdelivr.net/gh/mattih11/SeRTial@main/build/my_schemas.json
```
