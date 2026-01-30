cmake_minimum_required(VERSION 3.20)
project(SeRTial VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Main demo executable
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sertial)

# ============================================================================
# Test Executables
# ============================================================================

set(TEST_SOURCES
    test_foundation
    test_serialization
    test_padding
    test_endianness
    test_hybrid_binary
)

foreach(test_name ${TEST_SOURCES})
    add_executable(${test_name} test/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE sertial)
endforeach()

# ============================================================================
# Compiler Warnings
# ============================================================================

set(ALL_TARGETS ${PROJECT_NAME} ${TEST_SOURCES} schema_example test_example serialization_example)

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
    COMMAND test_hybrid_binary
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

add_custom_target(visualize
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/visualize_schema.py ${CMAKE_SOURCE_DIR}/scripts/message_schemas.json
    DEPENDS generate_schemas
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Visualizing message schemas..."
)

# Launch GUI schema viewer (generates schemas first)
add_custom_target(viewer
    COMMAND ${CMAKE_COMMAND} -E echo "Generating schemas..."
    COMMAND schema_example ${CMAKE_SOURCE_DIR}/scripts/message_schemas.json
    COMMAND ${CMAKE_COMMAND} -E echo "Launching GUI viewer..."
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/visualize_schema_gui.py ${CMAKE_SOURCE_DIR}/scripts/message_schemas.json
    DEPENDS schema_example
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating schemas and launching GUI viewer..."
)
