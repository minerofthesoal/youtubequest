#!/usr/bin/env bash
# Build script for YouTube Live Chat (Beat Saber Quest mod)
# Requires: qpm-rust, CMake >= 3.22, Ninja, Android NDK r27 (r27d tested)
set -euo pipefail

: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to your NDK r27 install first}"

echo "Fetching submodules (extern-vendor/rapidjson)..."
git submodule update --init --recursive

echo "Restoring qpm dependencies..."
qpm restore
if [ ! -f extern.cmake ] || [ ! -f qpm_defines.cmake ]; then
    echo "error: qpm restore did not produce extern.cmake / qpm_defines.cmake -- dependencies were not restored." >&2
    echo "If you have a stale ./extern from an old checkout, try: rm -rf extern && qpm restore" >&2
    exit 1
fi

mkdir -p build
cmake -G "Ninja" \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release \
    -B build .

cmake --build build -j"$(nproc)"

echo "Built: build/libyoutubelivechat.so"
echo "Next: ./createqmod.sh to package it into a .qmod"
