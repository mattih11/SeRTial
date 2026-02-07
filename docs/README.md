# SeRTial Documentation

Complete documentation for the SeRTial serialization library.

## Quick Navigation

### User Documentation
- **[User Guide](USER_GUIDE.md)** - Getting started, API basics, common patterns
- **[Container Guide](CONTAINER_GUIDE.md)** - Complete reference for bounded containers
- **[Examples](EXAMPLES.md)** - Comprehensive code examples and patterns
- **[Schema Viewer](SCHEMA_VIEWER.md)** - Interactive visualization tool

### Developer Documentation
- **[Adding Containers](ADDING_CONTAINERS.md)** - Extend SeRTial with custom containers
- **[Container Internals](CONTAINER_INTERNALS.md)** - Deep dive into implementation

### Technical References
- **[Serialization Mechanism](SERIALIZATION_MECHANISM.md)** - How block-based serialization works
- **[Reflector Architecture](REFLECTOR_BASED_SCHEMA.md)** - Schema generation internals
- **[Size Calculations](SIZE_CALCULATIONS.md)** - Compile-time size computation
- **[Template Patterns](TEMPLATE_PATTERNS.md)** - Metaprogramming techniques used

### API Documentation
- **[Doxygen API Reference](https://mattih11.github.io/SeRTial/)** - Complete API documentation (auto-generated from code)
- **Local API docs**: After building, open `docs/api/html/index.html` in your browser

## Building API Documentation Locally

### Prerequisites
```bash
sudo apt install doxygen graphviz  # Ubuntu/Debian
# or
brew install doxygen graphviz      # macOS
```

### Generate Documentation
```bash
# From repository root
doxygen Doxyfile

# Or via CMake (if configured)
cd build
make docs

# Open in browser
xdg-open docs/api/html/index.html  # Linux
open docs/api/html/index.html      # macOS
```

### Configuration
Documentation generation is controlled by `Doxyfile` in the repository root:
- **Output directory**: `docs/api/html/`
- **Input sources**: `include/sertial/`, `README.md`, `docs/USER_GUIDE.md`
- **Excluded paths**: `debug/`, `test/`, `examples/`, `build/`

## Documentation Structure

```
docs/
├── README.md                    # This file
├── USER_GUIDE.md               # Primary user documentation
├── CONTAINER_GUIDE.md          # Container reference
├── EXAMPLES.md                 # Code examples
├── SCHEMA_VIEWER.md            # Viewer guide
├── ADDING_CONTAINERS.md        # Extension guide
├── CONTAINER_INTERNALS.md      # Implementation details
├── SERIALIZATION_MECHANISM.md  # Technical deep-dive
├── REFLECTOR_BASED_SCHEMA.md   # Schema system architecture
├── SIZE_CALCULATIONS.md        # Size computation details
├── TEMPLATE_PATTERNS.md        # Metaprogramming patterns
├── SeRTial.png                 # Project logo
├── api/                        # Generated API documentation (git-ignored)
│   └── html/
│       └── index.html          # Doxygen entry point
└── archive/                    # Historical documentation
    └── v2.0_restructure/       # Pre-v2.0 docs
```

## Online Resources

- **GitHub Repository**: [https://github.com/mattih11/SeRTial](https://github.com/mattih11/SeRTial)
- **Interactive Viewer**: [https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html](https://mattih11.github.io/SeRTial/tools/sertial-inspect/viewer.html)
- **API Documentation**: [https://mattih11.github.io/SeRTial/](https://mattih11.github.io/SeRTial/)

## Contributing to Documentation

When contributing documentation:
1. Follow markdown best practices
2. Use code examples liberally
3. Link to related documentation
4. Update this README if adding new files
5. Run `doxygen Doxyfile` to verify API doc changes

### Documentation Conventions
- Use backticks for code elements: `serialize()`, `fixed_vector<T,N>`
- Use code blocks with language hints: \`\`\`cpp
- Link to other docs using relative paths: `[User Guide](USER_GUIDE.md)`
- Keep line length reasonable (~100 chars) for readability
- Use tables for structured data comparisons

## Documentation Tools

### Schema Viewer
Generate and visualize message schemas:
```bash
cd build
./schema_example my_schemas.json
# Open tools/sertial-inspect/viewer.html with the JSON file
```

### CLI Inspector
Terminal-based schema inspection:
```bash
cd tools/sertial-inspect
# See tools/sertial-inspect/README.md for usage
```

## License

Documentation licensed under the same terms as SeRTial (GPL v2).
