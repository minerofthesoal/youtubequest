# Build script for YouTube Live Chat (Beat Saber Quest mod)
# Requires: qpm-rust, CMake >= 3.22, Ninja, Android NDK (r25c recommended)
# Set $env:ANDROID_NDK_HOME before running, or edit $ndk below.

$ErrorActionPreference = "Stop"

$ndk = $env:ANDROID_NDK_HOME
if (-not $ndk) {
    Write-Error "ANDROID_NDK_HOME is not set. Install the NDK (r25c) and set this env var first."
    exit 1
}

Write-Host "Restoring qpm dependencies..."
qpm restore
if (-not (Test-Path "extern.cmake") -or -not (Test-Path "qpm_defines.cmake")) {
    Write-Error "qpm restore did not produce extern.cmake / qpm_defines.cmake -- dependencies were not restored. If you have a stale ./extern from an old checkout, try: Remove-Item -Recurse -Force extern; qpm restore"
    exit 1
}

$buildDir = "build"
if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }

cmake -G "Ninja" `
    -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" `
    -DANDROID_ABI=arm64-v8a `
    -DANDROID_PLATFORM=android-24 `
    -DCMAKE_BUILD_TYPE=Release `
    -B $buildDir .

cmake --build $buildDir -j

Write-Host "Built: $buildDir/libyoutubelivechat.so"
Write-Host "Next: ./createqmod.ps1 to package it into a .qmod"
