# cmake/isar-sdk-toolchain.cmake
#
# CMake toolchain for building SeRTial against the RaTOS ISAR SDK.
#
# The RaTOS SDK is a Debian Trixie amd64 sysroot containing libevl, reflect-cpp,
# and all SeRTial dependencies.  The SDK ships its own gcc-14 with a
# sysroot-wrapper (gcc-sysroot-wrapper.sh) that auto-injects
# --sysroot=<sdk> into every compiler invocation.
#
# Usage (typically invoked via scripts/evl-build.sh, not directly):
#   scripts/evl-build.sh --cross          # auto-downloads + relocates SDK
#   scripts/evl-build.sh --cross --test   # compile + test in QEMU
#
# If invoking cmake directly, export EVL_SDK_DIR first:
#   export EVL_SDK_DIR=.evl-cache/sdk && cmake --preset evl-cross
#
# SDK extraction + relocation (done automatically by scripts/evl-build.sh --cross):
#   mkdir -p .evl-cache/sdk
#   tar -xJf ratos-dev-sdk-container-amd64.xz -C .evl-cache/sdk --strip-components=1
#   .evl-cache/sdk/relocate-sdk.sh  (or: sed GCC_SYSROOT if patchelf unavailable)

if(DEFINED ENV{EVL_SDK_DIR})
    if(IS_ABSOLUTE "$ENV{EVL_SDK_DIR}")
        set(_EVL_SDK "$ENV{EVL_SDK_DIR}" CACHE PATH "RaTOS ISAR SDK root" FORCE)
    else()
        # Resolve relative paths against the source tree so TryCompile works.
        set(_EVL_SDK "${CMAKE_SOURCE_DIR}/$ENV{EVL_SDK_DIR}" CACHE PATH "RaTOS ISAR SDK root" FORCE)
    endif()
elseif(NOT DEFINED CACHE{_EVL_SDK})
    message(FATAL_ERROR
        "EVL_SDK_DIR environment variable is not set.\n"
        "Run 'scripts/evl-build.sh --cross' to auto-download the SDK to .evl-cache/sdk,\n"
        "or export EVL_SDK_DIR=/path/to/sdk before invoking cmake directly.")
endif()

# Use the SDK's own cross-compiler.  After relocation, gcc-sysroot-wrapper.sh
# auto-injects --sysroot=<sdk> so all system headers and libraries are resolved
# from the SDK sysroot, not the host.
set(CMAKE_C_COMPILER   "${_EVL_SDK}/usr/bin/x86_64-linux-gnu-gcc" CACHE FILEPATH "C compiler"   FORCE)
set(CMAKE_CXX_COMPILER "${_EVL_SDK}/usr/bin/x86_64-linux-gnu-g++" CACHE FILEPATH "C++ compiler" FORCE)

# Do NOT set CMAKE_SYSROOT: cmake would inject -isystem <sdk>/usr/include which
# disrupts the C++ stdlib #include_next chain.  The gcc wrapper handles sysroot.
set(CMAKE_FIND_ROOT_PATH "${_EVL_SDK}")

# reflect-cpp, SeRTial itself (when installed into the SDK), and other deps
# are installed under the SDK's usr/ prefix.
list(PREPEND CMAKE_PREFIX_PATH "${_EVL_SDK}/usr")

# Search headers/libraries inside the sysroot; allow cmake to find host programs.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
