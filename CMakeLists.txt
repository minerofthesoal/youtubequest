cmake_minimum_required(VERSION 3.22)
project(youtubelivechat)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# This project follows the standard QPM/Quest mod layout:
#   - `extern/` holds restored dependencies (beatsaber-hook, custom-types,
#     bs-cordl codegen headers, bsml, paper2_scotland2) after `qpm restore`.
#   - `shared/` holds this mod's own headers + implementation.
#   - `src/main.cpp` is the modloader entry point.
# Build with the Android NDK (r25+) targeting arm64-v8a only -- that is the
# only ABI the Quest 2/3/3S run.
# ---------------------------------------------------------------------------

add_compile_definitions(NDEBUG)
add_compile_definitions(MOD_ID="youtube-live-chat" VERSION="0.1.0")
# RTTI is intentionally left ON (no -fno-rtti): beatsaber-hook, bs-cordl, and
# custom-types all use typeid() internally (exception-logging macros in
# il2cpp-utils.hpp, unity-utils.hpp's type checks). Disabling RTTI breaks
# those headers with "use of typeid requires -frtti", which then cascades
# into unrelated-looking errors (e.g. "unknown type name 'Logger'") wherever
# a translation unit pulls in the same header after the typeid error aborts
# that header's parse.
add_compile_options(-fexceptions -fdeclspec -fvisibility=hidden -Wno-invalid-offsetof)

file(GLOB_RECURSE MOD_SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/shared/*.cpp
)

add_library(${PROJECT_NAME} SHARED ${MOD_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/extern/includes
)

# RapidJSON is vendored directly as plain committed header files at
# extern-vendor/rapidjson/include (from the exact commit beatsaber-hook
# 6.4.2 itself uses for its own bundled copy: 5ec44fb9206695e5293f610b0a
# 46d21851d0c966) -- NOT restored via qpm. The qpm registry package for
# this (rapidjson-macros) was tried first but its restore silently
# produces an empty directory (confirmed via a CI debug step listing the
# actual restored tree) -- it appears to be stale or broken upstream. A
# git submodule was tried next, but kept arriving broken/empty in CI
# despite the checkout step fetching it correctly, for reasons that were
# taking too many round-trips to pin down -- so the headers are just
# plain files in this repo now, like any other source file. No special
# setup needed beyond a normal clone.
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/extern-vendor/rapidjson/include/rapidjson/document.h)
    message(FATAL_ERROR "extern-vendor/rapidjson/include/rapidjson/document.h is missing -- these are plain committed files, so this means they weren't extracted/committed into the repo correctly.")
endif()
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/extern-vendor/rapidjson/include)

# qpm restore writes qpm_defines.cmake and extern.cmake to the PROJECT ROOT
# (next to this file), not into extern/ -- extern/ only holds the downloaded
# headers and libs. Both are required: qpm_defines.cmake carries
# restore-generated compile definitions and must be included first (it
# defines COMPILE_ID/EXTERN_DIR that extern.cmake depends on); extern.cmake
# carries the include/link setup. Treat either being missing as fatal (not a
# warning) -- continuing to configure without them just defers the failure
# to a much more confusing "header not found" error partway through
# compilation instead of a clear message here.
if(EXISTS ${CMAKE_SOURCE_DIR}/qpm_defines.cmake)
    include(${CMAKE_SOURCE_DIR}/qpm_defines.cmake)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/extern.cmake)
    include(${CMAKE_SOURCE_DIR}/extern.cmake)
else()
    message(FATAL_ERROR "extern.cmake not found -- run 'qpm restore' before building.")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE log)
