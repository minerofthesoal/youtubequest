cmake_minimum_required(VERSION 3.22)
project(youtubelivechat)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Standard QPM/Quest mod layout:
#   extern/  -- dependencies restored by `qpm restore` (beatsaber-hook,
#               custom-types, bs-cordl codegen, bsml, paper2_scotland2)
#   shared/  -- this mod's headers
#   src/     -- this mod's translation units, including the modloader entry
#               point in src/main.cpp
# Build with the Android NDK (r27) targeting arm64-v8a only -- the only ABI
# the Quest 2/3/3S run.
# ---------------------------------------------------------------------------

add_compile_definitions(NDEBUG)
add_compile_definitions(MOD_ID="youtube-live-chat" VERSION="0.2.0")

# RTTI stays ON (no -fno-rtti): beatsaber-hook, bs-cordl and custom-types all
# use typeid() internally (the exception-logging macros in il2cpp-utils.hpp,
# unity-utils.hpp's cast checks). Disabling it breaks those headers with "use
# of typeid requires -frtti", which then cascades into unrelated-looking
# errors wherever another translation unit includes the same header after the
# typeid error aborted its parse.
add_compile_options(-fexceptions -fdeclspec -fvisibility=hidden -Wno-invalid-offsetof)

file(GLOB_RECURSE MOD_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

if(NOT MOD_SOURCES)
    message(FATAL_ERROR "No sources found under src/ -- the glob below is the only thing that pulls this mod's code into the build.")
endif()

add_library(${PROJECT_NAME} SHARED ${MOD_SOURCES})

# beatsaber-hook ships its ARM64 inline-hook implementation as *source*, not
# inside libbeatsaber-hook.so: hooking.hpp calls A64HookFunction, which is
# defined in And64InlineHook.cpp. Every mod that installs a hook has to compile
# these into its own library, otherwise everything builds cleanly and then the
# link fails with "undefined symbol: A64HookFunction".
file(GLOB_RECURSE INLINE_HOOK_SOURCES
    ${CMAKE_SOURCE_DIR}/extern/includes/beatsaber-hook/shared/inline-hook/*.cpp
    ${CMAKE_SOURCE_DIR}/extern/includes/beatsaber-hook/shared/inline-hook/*.c
)
if(NOT INLINE_HOOK_SOURCES)
    message(FATAL_ERROR "No inline-hook sources under extern/includes/beatsaber-hook/shared/inline-hook -- did 'qpm restore' run?")
endif()
target_sources(${PROJECT_NAME} PRIVATE ${INLINE_HOOK_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/extern/includes
)

# RapidJSON is vendored as plain committed headers at
# extern-vendor/rapidjson/include, taken from the same upstream commit
# beatsaber-hook 6.4.2 bundles for its own use. It is deliberately NOT a qpm
# dependency: the registry's rapidjson-macros package restores to an empty
# directory (confirmed by a CI step that listed the restored tree), and a git
# submodule kept arriving empty in CI. Plain files in the repo have neither
# failure mode.
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/extern-vendor/rapidjson/include/rapidjson/document.h)
    message(FATAL_ERROR "extern-vendor/rapidjson/include/rapidjson/document.h is missing -- these are plain committed files, so this means the checkout is incomplete.")
endif()
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/extern-vendor/rapidjson/include)

# `qpm restore` writes qpm_defines.cmake and extern.cmake to the PROJECT ROOT
# (next to this file), not into extern/ -- extern/ only holds the downloaded
# headers and libs. Both are required: qpm_defines.cmake carries the
# restore-generated compile definitions and must be included first (it defines
# COMPILE_ID/EXTERN_DIR that extern.cmake depends on); extern.cmake carries the
# include and link setup. A missing file is fatal rather than a warning --
# continuing without them just defers the failure into a much more confusing
# "header not found" partway through compilation.
if(EXISTS ${CMAKE_SOURCE_DIR}/qpm_defines.cmake)
    include(${CMAKE_SOURCE_DIR}/qpm_defines.cmake)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/extern.cmake)
    include(${CMAKE_SOURCE_DIR}/extern.cmake)
else()
    message(FATAL_ERROR "extern.cmake not found -- run 'qpm restore' before building.")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE log)
