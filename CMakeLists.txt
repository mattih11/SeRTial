cmake_minimum_required(VERSION 3.20)
project(SeRTial VERSION 2.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include standard installation directories
include(GNUInstallDirs)

# Enable CTest
include(CTest)

# SeRTial schema-gen helper (available to downstream projects via find_package)
include(cmake/SeRTialSchemaGen.cmake)

# Find reflect-cpp package
find_package(reflectcpp REQUIRED)

# ============================================================================
# Core Library (Header-Only)
# ============================================================================

add_library(sertial INTERFACE)
target_include_directories(sertial INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(sertial INTERFACE reflectcpp)
target_compile_features(sertial INTERFACE cxx_std_20)

# Alias so cmake/SeRTialSchemaGen.cmake can use SeRTial::sertial in both
# the build tree and after find_package(SeRTial).
add_library(SeRTial::sertial ALIAS sertial)

# ============================================================================
# Examples Library (Header-Only, for user message types)
# ============================================================================

add_library(sertial_examples INTERFACE)
target_include_directories(sertial_examples INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/examples>
    $<INSTALL_INTERFACE:examples>
)
target_link_libraries(sertial_examples INTERFACE sertial)

# ============================================================================
# Example Executables (User-defined message types)
# ============================================================================

# Schema generation example - one-liner API
add_executable(schema_example examples/schema_example.cpp)
target_link_libraries(schema_example PRIVATE sertial_examples)

# Smoke-test sertial_generate_schema(): auto-generate example schemas post-build
sertial_generate_schema(
    TARGET          schema_example
    REGISTRY_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/examples/example_collection.hpp"
    COLLECTION_TYPE ExampleMessages
    OUTPUT          "${CMAKE_CURRENT_BINARY_DIR}/example_schemas_autogen.json"
)

# Test example - one-liner API  
add_executable(test_example examples/test_example.cpp)
target_link_libraries(test_example PRIVATE sertial_examples)

# Serialization example - comprehensive API demo
add_executable(serialization_example examples/serialization_example.cpp)
target_link_libraries(serialization_example PRIVATE sertial_examples)

# Ring buffer example - CommRaT-style circular buffer
add_executable(ring_buffer_example examples/ring_buffer_example.cpp)
target_link_libraries(ring_buffer_example PRIVATE sertial)

# Fixed string compile-time example - constexpr string operations
add_executable(fixed_string_compile_time examples/fixed_string_compile_time.cpp)
target_link_libraries(fixed_string_compile_time PRIVATE sertial)

# Fixed string concatenation example - operator+ with size deduction
add_executable(fixed_string_concatenation examples/fixed_string_concatenation.cpp)
target_link_libraries(fixed_string_concatenation PRIVATE sertial)

# Fixed vector erase/remove example - filtering and element removal
add_executable(fixed_vector_erase_remove examples/fixed_vector_erase_remove.cpp)
target_link_libraries(fixed_vector_erase_remove PRIVATE sertial)

# Main demo executable
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sertial)

# ============================================================================
# CLI Tools
# ============================================================================

# sertial-inspect - CLI schema viewer (replaces Python CLI)
add_executable(sertial-inspect 
    tools/sertial-inspect/main.cpp
)
target_link_libraries(sertial-inspect PRIVATE sertial)
target_include_directories(sertial-inspect PRIVATE tools/sertial-inspect)

# ============================================================================
# Test Executables
# ============================================================================

set(TEST_SOURCES
    test_foundation
    test_serialization
    test_padding
    test_endianness
    test_struct_layout
    test_ring_buffer
    test_ring_buffer_serialization
    test_element_padding
)

foreach(test_name ${TEST_SOURCES})
    add_executable(${test_name} test/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE sertial)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Register example-based test
add_test(NAME test_example COMMAND test_example)

# ============================================================================
# Compiler Warnings
# ============================================================================

set(ALL_TARGETS ${PROJECT_NAME} ${TEST_SOURCES} schema_example test_example serialization_example ring_buffer_example)

foreach(target ${ALL_TARGETS})
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

# ============================================================================
# Custom Targets
# ============================================================================

# Run all tests (use 'ctest' or 'cmake --build . --target test' instead)
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${TEST_SOURCES} test_example
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running all tests via CTest..."
)

# Run example tests only
add_custom_target(run_example_tests
    COMMAND test_example
    DEPENDS test_example
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running example tests..."
)

# Generate schemas from examples
add_custom_target(generate_example_schemas
    COMMAND schema_example ${CMAKE_SOURCE_DIR}/scripts/example_schemas.json
    DEPENDS schema_example
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating example schemas..."
)

# Alias for backwards compatibility
add_custom_target(generate_schemas
    COMMAND schema_example ${CMAKE_SOURCE_DIR}/scripts/message_schemas.json
    DEPENDS schema_example
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating message schemas..."
)

# Open HTML viewer in browser (generates schemas first)
add_custom_target(viewer
    COMMAND ${CMAKE_COMMAND} -E echo "Generating schemas..."
    COMMAND schema_example
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/my_schemas.json ${CMAKE_SOURCE_DIR}/examples/schemas/example_schemas.json
    COMMAND ${CMAKE_COMMAND} -E echo "Schema generated: examples/schemas/example_schemas.json"
    COMMAND ${CMAKE_COMMAND} -E echo "Open tools/sertial-inspect/viewer.html in your browser to view"
    DEPENDS schema_example
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating schemas for HTML viewer..."
)

# ============================================================================
# Installation
# ============================================================================

# Install header files
install(DIRECTORY include/sertial
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# Install example message types (optional, users may want these as references)
install(DIRECTORY examples/defines examples/messages
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sertial/examples
    FILES_MATCHING PATTERN "*.hpp"
)

# Install C++ CLI tool
install(TARGETS sertial-inspect
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install HTML viewer and example schemas
install(DIRECTORY tools/sertial-inspect
    DESTINATION ${CMAKE_INSTALL_DATADIR}/sertial
    FILES_MATCHING 
    PATTERN "*.html"
    PATTERN "*.md"
)

install(FILES examples/schemas/example_schemas.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/sertial/examples
)

# Install CMake package configuration
include(CMakePackageConfigHelpers)

# Generate SeRTialConfig.cmake
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SeRTialConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SeRTialConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SeRTial
)

# Generate version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SeRTialConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install CMake config files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SeRTialConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SeRTialConfigVersion.cmake
    cmake/SeRTialSchemaGen.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SeRTial
)

# Install schema-gen driver source so downstream projects can compile it
install(FILES tools/schema_gen/schema_gen_driver.cpp
    DESTINATION ${CMAKE_INSTALL_DATADIR}/sertial/schema_gen
)

# Install target
install(TARGETS sertial
    EXPORT SeRTialTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install export
install(EXPORT SeRTialTargets
    FILE SeRTialTargets.cmake
    NAMESPACE SeRTial::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SeRTial
)

# ============================================================================
# Documentation (Doxygen)
# ============================================================================

find_package(Doxygen OPTIONAL_COMPONENTS dot)

if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE}")
    
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
    
    message(STATUS "Use 'make docs' to generate API documentation")
else()
    message(STATUS "Doxygen not found - 'make docs' target will not be available")
endif()

# ============================================================================
# Packaging (CPack — produces sertial_<version>_amd64.deb)
# ============================================================================

set(CPACK_PACKAGE_NAME              "sertial")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR            "Matthias Haase")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "SeRTial: zero-allocation C++20 binary serialization library")
set(CPACK_PACKAGE_HOMEPAGE_URL      "https://github.com/mattih11/SeRTial")

# Debian-specific settings
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER  "Matthias Haase <mattihaae@proton.me>")
set(CPACK_DEBIAN_PACKAGE_SECTION     "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY    "optional")
# reflectcpp is header-only and installed from its own deb; no runtime deps.
set(CPACK_DEBIAN_PACKAGE_DEPENDS     "")

# Output filename: sertial_<version>_amd64.deb
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

set(CPACK_GENERATOR "DEB")

include(CPack)
