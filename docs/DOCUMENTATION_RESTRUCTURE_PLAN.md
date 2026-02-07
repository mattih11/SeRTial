# SeRTial Documentation Restructuring Plan

**Date**: February 7, 2026  
**Status**: Analysis & Proposal

## Current Documentation Analysis

### Active Documentation Files

1. **README.md** (857 lines) - Main entry point
   - Quick Start, Installation, Building
   - Schema Viewer (too long, duplicates sertial-inspect README)
   - Examples (needs own guide)
   - API Reference (outdated, missing files)
   - Container sections (RingBuffer/static_buffer outdated, duplicates)
   - Project structure (missing test files)
   - Introspection section (duplicates reflector docs, lines 631+)
   - Custom containers (should be in separate guide)

2. **tools/sertial-inspect/README.md** (58 lines) - Viewer tool documentation
   - Usage examples
   - Generating schemas
   - Good structure, stays focused

3. **docs/REFLECTOR_BASED_SCHEMA.md** (350+ lines) - Phase 4 architecture
   - Deep technical explanation
   - Reflector mechanism
   - Design rationale
   - **Should be**: Technical deep-dive reference

4. **docs/CONTAINER_HANDLING.md** (927 lines) - Container integration
   - Container types overview
   - SerializableContainer concept
   - Adding new containers
   - Element padding details
   - Schema generation
   - **Issues**: Mixes user guide with implementation details

5. **docs/SERIALIZATION_MECHANISM.md** (433 lines) - How serialization works
   - Block-based execution
   - StructLayout details
   - std::span usage
   - Performance characteristics
   - **Good**: Technical reference

6. **docs/SIZE_CALCULATIONS.md** (580 lines) - Size computation
   - Compile-time vs runtime
   - Formulas and examples
   - **Good**: Technical reference

7. **docs/TEMPLATE_PATTERNS.md** (400+ lines) - Metaprogramming patterns
   - C++20 concepts
   - SFINAE patterns
   - Template techniques
   - **Good**: Developer reference

8. **RELEASE_NOTES_v2.0.0.md** - Release documentation (keep as-is)

9. **.github/copilot-instructions.md** - AI assistant context (keep as-is)

## Problems Identified

### Content Duplication
1. **Container information** appears in:
   - README.md (RingBuffer section, static_buffer section, custom containers)
   - CONTAINER_HANDLING.md
   - API Reference section
   - Introspection section

2. **Schema viewer information** appears in:
   - README.md (long Schema Viewer section)
   - tools/sertial-inspect/README.md
   - Introspection section (lines 631+)
   - REFLECTOR_BASED_SCHEMA.md

3. **Reflector mechanism** explained in:
   - README.md (Introspection section)
   - REFLECTOR_BASED_SCHEMA.md
   - tools/sertial-inspect/README.md (briefly)

### Outdated Content
1. README.md API Reference:
   - Missing test files
   - Old HMM references (found via grep)
   - Outdated RingBuffer/static_buffer examples
   - Old GUI documentation references

2. Container sections use outdated examples

3. Project structure outdated

### Structure Issues
1. README.md is too long (857 lines) - should be ~200-300 lines max
2. No clear separation: User Guide vs Technical Reference
3. Examples mixed throughout instead of dedicated guide
4. No clear navigation between docs

## Proposed New Structure

### 1. README.md (Main Entry - Target: ~250 lines)
**Purpose**: First impression, getting started, high-level overview

**Contents**:
```markdown
- Logo & Brief Description (2-3 sentences)
- License
- Key Features (bullet points only)
- Quick Start (minimal example)
- Prerequisites & Installation
- Building
- Basic Usage (serialize/deserialize example)
- Containers Overview (1-2 sentences + link to guide)
- Schema Viewer (1-2 sentences + live demo link)
- Documentation Map (links to all guides)
- Development Status (condensed, at bottom)
- Contributing
- Author & Acknowledgments
```

### 2. docs/USER_GUIDE.md (NEW - Target: ~300 lines)
**Purpose**: Practical guide for using SeRTial

**Contents**:
```markdown
# SeRTial User Guide

## Basic Usage
- serialize() / deserialize()
- Error handling (std::optional)
- Buffer management

## Working with Containers
- fixed_vector<T, N>
- fixed_string<N>
- RingBuffer<T, N>
- static_buffer<N>
- Link to: CONTAINER_GUIDE.md for details

## Message Types
- Simple structs
- Nested structs
- Variable-size fields
- Best practices

## Schema Generation
- Generating schemas
- Using the viewer
- Link to: tools/sertial-inspect/README.md

## Common Patterns
- Real-time constraints
- Buffer sizing
- Performance tips

## Troubleshooting
- Common errors
- Compile-time checks
- Type requirements
```

### 3. docs/CONTAINER_GUIDE.md (NEW - Target: ~200 lines)
**Purpose**: Complete container reference for users

**Contents**:
```markdown
# Container Guide

## Overview
Brief intro to bounded containers

## Container Types
### fixed_vector<T, N>
- Usage examples
- API reference
- Serialization behavior

### fixed_string<N>
- Usage examples
- API reference
- Serialization behavior

### RingBuffer<T, N>
- Usage examples
- FIFO behavior
- Wrap-around handling

### static_buffer<N>
- Usage examples
- Binary data handling

## Choosing a Container
Decision guide

## Link to: ADDING_CONTAINERS.md for extending
```

### 4. docs/ADDING_CONTAINERS.md (NEW - Target: ~150 lines)
**Purpose**: Guide for adding custom containers

**Contents**:
```markdown
# Adding Custom Containers

## Quick Start
Minimal example with SerializableContainer concept

## Requirements
- value_type
- size(), capacity(), data()
- data_unsafe(), set_size_unsafe()
- max_size static member

## Registration
- container_registration.hpp
- serialization_view_provider<T>
- container_type_name<T>

## Testing
What to test

## Link to: CONTAINER_INTERNALS.md for deep dive
```

### 5. docs/CONTAINER_INTERNALS.md (REFACTOR from CONTAINER_HANDLING.md)
**Purpose**: Deep technical reference on container implementation

**Contents**:
```markdown
# Container Implementation Internals

## SerializableContainer Concept
Technical details

## Span-Based Serialization
How get_serialization_spans() works

## Element Padding
Memory layout details

## Multi-Span Containers
RingBuffer wrap-around handling

## Schema Generation
Container metadata export

## Links:
- User Guide: CONTAINER_GUIDE.md
- Adding Containers: ADDING_CONTAINERS.md
```

### 6. docs/SCHEMA_VIEWER.md (NEW - consolidate viewer docs)
**Purpose**: Complete schema viewer documentation

**Contents**:
```markdown
# Schema Viewer Documentation

## Overview
What it is, why it's useful

## Quick Start
- Live demo link
- Local usage

## Features
- Interactive visualization
- Variable field controls
- Animation mode
- Hover highlighting

## Generating Schemas
- From code
- CLI tool usage
- Link to: tools/sertial-inspect/README.md

## Understanding the Visualization
- Struct layout
- Serialized layout
- Block operations

## Technical Details
- Link to: REFLECTOR_BASED_SCHEMA.md
```

### 7. tools/sertial-inspect/README.md (KEEP - minor updates)
**Purpose**: CLI tool and HTML viewer usage

**Contents**: (mostly good as-is)
```markdown
- Usage (CLI and HTML)
- Generating schemas
- Link to: docs/SCHEMA_VIEWER.md for details
- Link to: docs/REFLECTOR_BASED_SCHEMA.md for internals
```

### 8. docs/REFLECTOR_BASED_SCHEMA.md (REFINE - keep technical depth)
**Purpose**: Deep-dive on reflector architecture

**Contents**: (mostly good, add links)
```markdown
- Current content is good
- Add links to:
  - SCHEMA_VIEWER.md (user-facing)
  - tools/sertial-inspect/README.md (usage)
  - SERIALIZATION_MECHANISM.md (how it integrates)
```

### 9. docs/EXAMPLES.md (NEW - extract from README)
**Purpose**: Comprehensive examples collection

**Contents**:
```markdown
# SeRTial Examples

## Basic Examples
### Simple Struct
### With Variable Fields
### Nested Structs

## Container Examples
### Using fixed_vector
### Using RingBuffer
### Multiple Containers

## Advanced Examples
### Custom Containers
### Schema Generation
### Performance Optimization

## Real-World Patterns
### Message Protocols
### Data Logging
### Network Communication

## Links to example code in examples/
```

### 10. Keep Technical References (minor updates)
- **SERIALIZATION_MECHANISM.md** - Add navigation links
- **SIZE_CALCULATIONS.md** - Add navigation links  
- **TEMPLATE_PATTERNS.md** - Add navigation links

## Implementation Plan

### Phase 1: Create New Structure
1. Create USER_GUIDE.md (extract from README)
2. Create CONTAINER_GUIDE.md (user-facing from CONTAINER_HANDLING.md)
3. Create ADDING_CONTAINERS.md (extract from README + CONTAINER_HANDLING.md)
4. Create CONTAINER_INTERNALS.md (refactor CONTAINER_HANDLING.md)
5. Create SCHEMA_VIEWER.md (consolidate viewer docs)
6. Create EXAMPLES.md (extract from README)

### Phase 2: Update Existing Files
1. **README.md**: Drastically reduce to ~250 lines
   - Remove long sections
   - Add documentation map with links
   - Keep only essential quick start

2. **CONTAINER_HANDLING.md** → **CONTAINER_INTERNALS.md**
   - Remove user-guide content (moved to CONTAINER_GUIDE.md)
   - Focus on implementation details
   - Add navigation links

3. **REFLECTOR_BASED_SCHEMA.md**: Add links

4. **tools/sertial-inspect/README.md**: Add links to new docs

5. **Technical references**: Add navigation sections

### Phase 3: Add Navigation
Create consistent navigation structure in all docs:

```markdown
## Documentation

**User Guides**:
- [User Guide](docs/USER_GUIDE.md) - Getting started and common patterns
- [Container Guide](docs/CONTAINER_GUIDE.md) - Working with containers
- [Examples](docs/EXAMPLES.md) - Comprehensive examples
- [Schema Viewer](docs/SCHEMA_VIEWER.md) - Interactive visualization

**Developer Guides**:
- [Adding Containers](docs/ADDING_CONTAINERS.md) - Extend with custom containers
- [Container Internals](docs/CONTAINER_INTERNALS.md) - Implementation details

**Technical References**:
- [Serialization Mechanism](docs/SERIALIZATION_MECHANISM.md) - How it works
- [Reflector Architecture](docs/REFLECTOR_BASED_SCHEMA.md) - Schema generation
- [Size Calculations](docs/SIZE_CALCULATIONS.md) - Compile-time sizing
- [Template Patterns](docs/TEMPLATE_PATTERNS.md) - Metaprogramming techniques
```

### Phase 4: Verify & Test
1. Check all internal links work
2. Verify no content duplication
3. Ensure logical flow between documents
4. Update API references with correct file paths
5. Remove all outdated references (HMM, old GUIs, etc.)

## Content Migration Map

### From README.md → New Locations

| Current Section (README) | New Location | Keep in README? |
|--------------------------|--------------|-----------------|
| Quick Start | USER_GUIDE.md basics | Yes (minimal) |
| Examples | EXAMPLES.md | Yes (1 tiny example) |
| Schema Viewer (long) | SCHEMA_VIEWER.md | No (1 sentence + link) |
| RingBuffer section | CONTAINER_GUIDE.md | No (link only) |
| static_buffer section | CONTAINER_GUIDE.md | No (link only) |
| Custom Containers | ADDING_CONTAINERS.md | No (link only) |
| API Reference | Update in place | Yes (fix outdated) |
| Project Structure | Update in place | Yes (fix missing) |
| Introspection (631+) | SCHEMA_VIEWER.md + REFLECTOR_BASED_SCHEMA.md | No (link only) |

### From CONTAINER_HANDLING.md → Split Into

| Current Section | New Location |
|-----------------|--------------|
| Container types overview | CONTAINER_GUIDE.md |
| Using containers | CONTAINER_GUIDE.md |
| SerializableContainer concept | CONTAINER_INTERNALS.md |
| Adding new containers | ADDING_CONTAINERS.md |
| Implementation details | CONTAINER_INTERNALS.md |
| Element padding | CONTAINER_INTERNALS.md |

### Consolidate Viewer Docs Into SCHEMA_VIEWER.md

From:
- README.md Schema Viewer section
- README.md Introspection section
- tools/sertial-inspect/README.md (usage parts)
- REFLECTOR_BASED_SCHEMA.md (link only)

## Success Criteria

1. **README.md < 300 lines** - Quick entry point, not overwhelming
2. **No content duplication** - Each topic explained once, linked elsewhere
3. **Clear separation**: User Guide vs Technical Reference
4. **Easy navigation** - Links between related documents
5. **All references current** - No HMM, no old GUI mentions
6. **Complete coverage** - Everything documented somewhere logical
7. **Consistent structure** - Same navigation format in all docs

## Benefits

1. **For New Users**: Clear path from README → USER_GUIDE → specific topics
2. **For Developers**: Easy to find implementation details
3. **For Contributors**: Clear where to add new content
4. **For Maintenance**: Single source of truth for each topic
5. **For Discovery**: Documentation map shows what's available

## Next Steps

1. Review this plan
2. Create new files (Phase 1)
3. Refactor existing files (Phase 2)
4. Add navigation (Phase 3)
5. Verify everything (Phase 4)
6. Commit as: "docs: Restructure documentation for better clarity and navigation"

## Critical Implementation Rules

### Source of Truth: CODE, Not Documentation
**When writing documentation, ALWAYS:**
1. Read actual `.hpp` and `.cpp` files for APIs, signatures, and behavior
2. Verify examples compile with current code
3. Check actual file paths in the repository
4. Review test files to understand usage patterns
5. **Never trust old markdown** - docs drift, code is truth

### Documentation Maintenance Guidelines
Add to `.github/copilot-instructions.md`:

```markdown
## Documentation Update Triggers

When modifying these files, update corresponding documentation:

### Code → Documentation Mapping

**include/sertial/containers/*.hpp** → Update:
- docs/CONTAINER_GUIDE.md (user-facing changes)
- docs/CONTAINER_INTERNALS.md (implementation changes)
- docs/USER_GUIDE.md (if API changes)

**include/sertial/core/layout/struct_layout.hpp** → Update:
- docs/SERIALIZATION_MECHANISM.md
- docs/SIZE_CALCULATIONS.md (if sizing logic changes)

**include/sertial/reflector/*.hpp** → Update:
- docs/REFLECTOR_BASED_SCHEMA.md
- docs/SCHEMA_VIEWER.md (if schema format changes)

**include/sertial/io/unified_binary.hpp** → Update:
- docs/USER_GUIDE.md (public API section)
- README.md (if main API changes)

**tools/sertial-inspect/viewer.html** → Update:
- docs/SCHEMA_VIEWER.md
- tools/sertial-inspect/README.md

**tools/sertial-inspect/main.cpp** → Update:
- tools/sertial-inspect/README.md (CLI usage)

**examples/*.cpp** → Update:
- docs/EXAMPLES.md
- README.md (if Quick Start example changes)

### Documentation Review Checklist
Before committing documentation changes:
- [ ] Verify APIs against actual header files
- [ ] Test code examples compile
- [ ] Check file paths exist
- [ ] Confirm namespace usage is current
- [ ] Review recent commits for API changes
```

## Additional Tasks

### Doxygen Documentation
**Priority**: Add after restructure complete

**Task**: Create Doxygen configuration for API documentation
- File: `Doxyfile` in root
- Output: `docs/api/` (add to .gitignore)
- Focus on public API headers
- Link from README and USER_GUIDE

**Doxygen Config Priorities:**
1. Include only public headers (`include/sertial/*.hpp`, not `include/sertial/core/`)
2. Extract all: classes, functions, enums
3. Generate HTML and search
4. Use DOT for class diagrams
5. Add examples from `examples/` directory

**Integration:**
- Add "API Reference" link in README → points to Doxygen HTML
- CMake target: `make docs` generates Doxygen
- CI: Generate and deploy to GitHub Pages

### Phase 0: Pre-Execution Verification
Before creating any documentation:

1. **Inventory actual files:**
   ```bash
   find include/ -name "*.hpp" | sort
   find test/ -name "*.cpp" | sort
   find examples/ -name "*.cpp" | sort
   ```

2. **Verify current APIs:**
   - Check serialize() signature in `include/sertial/io/unified_binary.hpp`
   - Check StructLayout<T> interface in `include/sertial/core/layout/struct_layout.hpp`
   - Check container requirements in `include/sertial/containers/container_registration.hpp`

3. **Test example compilation:**
   - Verify examples/serialization_example.cpp compiles
   - Check what APIs it actually uses

4. **Review recent changes:**
   - Check git log for API changes since last doc update
   - Verify no obsolete APIs documented

## Execution Order

### Phase 0: Verify Source of Truth (COMPLETE)
- Inventory files
- Check APIs
- Test examples

### Phase 1: Create New Documentation Files
(Each file: Check code first, then write)

### Phase 2: Update Existing Files
(Each update: Verify against code)

### Phase 3: Add Navigation
(Verify all links point to real files)

### Phase 4: Doxygen Setup
(After all markdown is correct)

### Phase 5: Update Copilot Instructions
(Add documentation maintenance rules)

### Phase 6: Final Verification
- Test all examples
- Check all links
- Verify API accuracy
- Build Doxygen

## Next Steps

1. (COMPLETE) Review this plan
2. **Execute Phase 0**: Verify source of truth
3. **Execute Phase 1-6**: As outlined above
4. Commit as: "docs: Restructure documentation for better clarity and navigation"
