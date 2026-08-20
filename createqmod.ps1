# Renders mod.template.json into mod.json (filling in the dependency list from
# what qpm actually resolved) and zips it up with the built .so.
$ErrorActionPreference = "Stop"

if (-not (Test-Path "build/libyoutubelivechat.so")) {
    Write-Error "build/libyoutubelivechat.so not found -- run ./build.ps1 first."
    exit 1
}

qpm qmod manifest
qpm qmod zip --skip_build YouTubeLiveChat.qmod
