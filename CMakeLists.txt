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
add_compile_options(-fno-rtti -fexceptions -fdeclspec -fvisibility=hidden -Wno-invalid-offsetof)

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

# qpm-restored static/shared libs get linked in automatically once you run
# `qpm restore` -- it drops a qpm_defines.cmake / auto-generated link list
# into extern/. Keeping this explicit stub here so the project is inspectable
# without having run qpm yet; delete the placeholder line below once restore
# has generated the real one.
if(EXISTS ${CMAKE_SOURCE_DIR}/extern/extern.cmake)
    include(${CMAKE_SOURCE_DIR}/extern/extern.cmake)
else()
    message(WARNING "extern/extern.cmake not found yet -- run 'qpm restore' before building.")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE log)
