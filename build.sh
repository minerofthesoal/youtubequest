#!/usr/bin/env bash
# Build script for YouTube Live Chat (Beat Saber Quest mod)
# Requires: qpm-rust, CMake >= 3.22, Ninja, Android NDK (r25c recommended)
set -euo pipefail

: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to your NDK r25c install first}"

echo "Restoring qpm dependencies..."
qpm restore

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
