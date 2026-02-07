cmake_minimum_required(VERSION 3.20)
project(SeRTial VERSION 2.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include standard installation directories
include(GNUInstallDirs)

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

# Test example - one-liner API  
add_executable(test_example examples/test_example.cpp)
target_link_libraries(test_example PRIVATE sertial_examples)

# Serialization example - comprehensive API demo
add_executable(serialization_example examples/serialization_example.cpp)
target_link_libraries(serialization_example PRIVATE sertial_examples)

# Ring buffer example - CommRaT-style circular buffer
add_executable(ring_buffer_example examples/ring_buffer_example.cpp)
target_link_libraries(ring_buffer_example PRIVATE sertial)

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
endforeach()

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

# Run all tests
add_custom_target(run_tests
    COMMAND test_foundation
    COMMAND test_serialization
    COMMAND test_padding
    COMMAND test_endianness
    COMMAND test_struct_layout
    COMMAND test_ring_buffer
    COMMAND test_ring_buffer_serialization
    COMMAND test_element_padding
    COMMAND test_example
    DEPENDS ${TEST_SOURCES} test_example
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running all tests..."
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
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SeRTial
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

