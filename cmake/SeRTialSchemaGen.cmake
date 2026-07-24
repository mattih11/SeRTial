# SeRTialSchemaGen.cmake
# ----------------------
# Provides sertial_generate_schema() — a CMake helper that compiles the
# SeRTial schema-gen driver against a user-supplied collection header and
# runs it as a post-build step to produce a JSON schema file.
#
# Usage (minimal — SeRTial MessageLayoutRecord only):
#
#   include(SeRTialSchemaGen)
#
#   sertial_generate_schema(
#       TARGET          my_app              # existing CMake target to attach to
#       REGISTRY_HEADER /src/my_registry.hpp  # header defining the collection
#       COLLECTION_TYPE MyApp               # C++ type name of the collection
#       OUTPUT          ${CMAKE_BINARY_DIR}/my_app_schemas.json
#   )
#
# Usage (CommRaT / custom record type):
#
#   sertial_generate_schema(
#       TARGET          my_app
#       REGISTRY_HEADER /src/my_registry.hpp
#       COLLECTION_TYPE MyApp
#       OUTPUT          ${CMAKE_BINARY_DIR}/my_app_schemas.json
#       RECORD_HEADER   /src/commrat_schema_output.hpp  # defines CommRaTMessageRecord
#       RECORD_TYPE     commrat::CommRaTMessageRecord
#   )
#
# What it does
# ------------
# 1. Adds a new executable target  <TARGET>_schema_gen  that compiles
#    schema_gen_driver.cpp with the supplied defines.
# 2. Links it against SeRTial::sertial (and whatever <TARGET> links against,
#    so that template instantiation of the collection type works).
# 3. Adds a post-build command on <TARGET> that runs the driver and writes
#    the OUTPUT file.
#
# The driver uses generate_typed_data<RecordType>() which calls
# rfl::json::write() on the full Record struct, so CommRaT fields
# (or any downstream extension) are serialized automatically — no manual
# JSON merging required.

include_guard(GLOBAL)

# Capture this module's directory at include time — CMAKE_CURRENT_LIST_DIR
# inside a function body resolves to the *caller's* directory, not this file's.
set(_SERTIAL_SCHEMA_GEN_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "")

function(sertial_generate_schema)
    cmake_parse_arguments(
        SGS                          # prefix
        ""                           # options (none)
        "TARGET;REGISTRY_HEADER;COLLECTION_TYPE;OUTPUT;RECORD_HEADER;RECORD_TYPE"
        ""                           # multi-value (none)
        ${ARGN}
    )

    # ---- Validate required arguments ----------------------------------------
    foreach(req TARGET REGISTRY_HEADER COLLECTION_TYPE OUTPUT)
        if(NOT DEFINED SGS_${req})
            message(FATAL_ERROR "sertial_generate_schema: ${req} is required")
        endif()
    endforeach()

    if(NOT TARGET ${SGS_TARGET})
        message(FATAL_ERROR "sertial_generate_schema: TARGET '${SGS_TARGET}' is not a known CMake target")
    endif()

    # ---- Locate the driver source --------------------------------------------
    # Prefer an explicit override, then the source tree (relative to this
    # module file's directory, captured at include time), then the installed
    # share path.
    if(SERTIAL_SCHEMA_GEN_DRIVER)
        set(_driver_src "${SERTIAL_SCHEMA_GEN_DRIVER}")
    elseif(EXISTS "${_SERTIAL_SCHEMA_GEN_MODULE_DIR}/../tools/schema_gen/schema_gen_driver.cpp")
        get_filename_component(_driver_src
            "${_SERTIAL_SCHEMA_GEN_MODULE_DIR}/../tools/schema_gen/schema_gen_driver.cpp"
            ABSOLUTE)
    else()
        # Installed path: <prefix>/share/sertial/schema_gen/schema_gen_driver.cpp
        find_file(_driver_src schema_gen_driver.cpp
            PATHS "${CMAKE_INSTALL_PREFIX}/share/sertial/schema_gen"
            NO_DEFAULT_PATH)
        if(NOT _driver_src)
            message(FATAL_ERROR
                "sertial_generate_schema: cannot locate schema_gen_driver.cpp. "
                "Set SERTIAL_SCHEMA_GEN_DRIVER to its absolute path.")
        endif()
    endif()

    # ---- Build the driver executable ----------------------------------------
    set(_gen_target "${SGS_TARGET}_schema_gen")

    add_executable(${_gen_target} "${_driver_src}")

    # The driver only needs:
    #   - SeRTial headers + reflectcpp (via SeRTial::sertial, header-only)
    #   - Include dirs from the user's target so that REGISTRY_HEADER and
    #     RECORD_HEADER can be found (no need to inherit link libraries).
    target_link_libraries(${_gen_target} PRIVATE SeRTial::sertial)
    target_include_directories(${_gen_target}
        PRIVATE
            $<TARGET_PROPERTY:${SGS_TARGET},INCLUDE_DIRECTORIES>
            $<TARGET_PROPERTY:${SGS_TARGET},INTERFACE_INCLUDE_DIRECTORIES>
    )

    # ---- Compile definitions ------------------------------------------------
    # Wrap the header paths in escaped quotes so they become string literals
    # usable as #include arguments inside the driver.
    target_compile_definitions(${_gen_target}
        PRIVATE
            "SERTIAL_REGISTRY_HEADER=\"${SGS_REGISTRY_HEADER}\""
            "SERTIAL_COLLECTION_TYPE=${SGS_COLLECTION_TYPE}"
            "SERTIAL_OUTPUT_FILE=\"${SGS_OUTPUT}\""
    )

    if(DEFINED SGS_RECORD_HEADER)
        target_compile_definitions(${_gen_target} PRIVATE
            "SERTIAL_RECORD_HEADER=\"${SGS_RECORD_HEADER}\""
        )
    endif()

    if(DEFINED SGS_RECORD_TYPE)
        target_compile_definitions(${_gen_target} PRIVATE
            "SERTIAL_RECORD_TYPE=${SGS_RECORD_TYPE}"
        )
    endif()

    # ---- Post-build: run the driver to emit the JSON ------------------------
    add_custom_command(
        TARGET ${SGS_TARGET} POST_BUILD
        COMMAND ${_gen_target}
        BYPRODUCTS "${SGS_OUTPUT}"
        COMMENT "Generating schema: ${SGS_OUTPUT}"
        VERBATIM
    )

    # Make the user's target depend on the driver being built first.
    add_dependencies(${SGS_TARGET} ${_gen_target})
endfunction()
