# Build script for YouTube Live Chat (Beat Saber Quest mod)
# Requires: qpm, CMake >= 3.22, Ninja, Android NDK r27 (r27d tested)
# Set $env:ANDROID_NDK_HOME before running.

$ErrorActionPreference = "Stop"

$ndk = $env:ANDROID_NDK_HOME
if (-not $ndk) {
    Write-Error "ANDROID_NDK_HOME is not set. Install the NDK (r27) and set this env var first."
    exit 1
}

Write-Host "Restoring qpm dependencies..."
qpm restore
if (-not (Test-Path "extern.cmake") -or -not (Test-Path "qpm_defines.cmake")) {
    Write-Error "qpm restore did not produce extern.cmake / qpm_defines.cmake -- dependencies were not restored. If you have a stale ./extern from an old checkout, try: Remove-Item -Recurse -Force extern; qpm restore"
    exit 1
}

cmake -G "Ninja" `
    -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" `
    -DANDROID_ABI=arm64-v8a `
    -DANDROID_PLATFORM=android-24 `
    -DCMAKE_BUILD_TYPE=Release `
    -B build .

cmake --build build -j

Write-Host "Built: build/libyoutubelivechat.so"
Write-Host "Next: ./createqmod.ps1 to package it into a .qmod"
