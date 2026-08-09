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

# rapidjson-macros (the qpm package providing <rapidjson/document.h> for the
# beatsaber-hook 6.x line -- see qpm.json) doesn't declare its own
# info.additionalData.compileOptions.includePaths the way bs-cordl/fmt/
# paper2_scotland2/libil2cpp do, so qpm's generated extern.cmake never adds
# an include entry for it and a plain `#include "rapidjson/document.h"`
# won't resolve on its own. Find wherever `qpm restore` actually put the
# `rapidjson/` header folder and add its parent dir explicitly, rather than
# hardcode a nested path under extern/includes/rapidjson-macros/... that
# could shift if that package's own layout changes. Note: a single `*`
# wildcard segment (e.g. extern/includes/*/rapidjson/document.h) only
# matches exactly one directory of nesting and would silently miss a
# deeper layout -- verified empirically before relying on it -- so this
# globs for the bare filename (which GLOB_RECURSE matches at any depth)
# and filters to entries whose parent directory is literally "rapidjson".
file(GLOB_RECURSE ALL_DOCUMENT_H ${CMAKE_SOURCE_DIR}/extern/includes/document.h)
set(RAPIDJSON_DOCUMENT_H "")
foreach(candidate ${ALL_DOCUMENT_H})
    get_filename_component(candidate_parent_dir ${candidate} DIRECTORY)
    get_filename_component(candidate_parent_dir_name ${candidate_parent_dir} NAME)
    if(candidate_parent_dir_name STREQUAL "rapidjson")
        list(APPEND RAPIDJSON_DOCUMENT_H ${candidate})
    endif()
endforeach()
if(RAPIDJSON_DOCUMENT_H)
    list(GET RAPIDJSON_DOCUMENT_H 0 RAPIDJSON_DOCUMENT_H_FIRST)
    get_filename_component(RAPIDJSON_PARENT_DIR ${RAPIDJSON_DOCUMENT_H_FIRST} DIRECTORY) # .../rapidjson
    get_filename_component(RAPIDJSON_PARENT_DIR ${RAPIDJSON_PARENT_DIR} DIRECTORY)       # its parent
    target_include_directories(${PROJECT_NAME} PRIVATE ${RAPIDJSON_PARENT_DIR})
else()
    message(WARNING "Could not find rapidjson/document.h anywhere under extern/includes -- has `qpm restore` run, and does rapidjson-macros still ship headers at that path?")
endif()

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
