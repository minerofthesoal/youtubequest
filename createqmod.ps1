# Packages the built .so + mod.json into YouTubeLiveChat.qmod (just a zip)
$ErrorActionPreference = "Stop"
$out = "YouTubeLiveChat.qmod"
if (Test-Path $out) { Remove-Item $out }
Compress-Archive -Path "mod.json","build/libyoutubelivechat.so" -DestinationPath $out
Write-Host "Wrote $out"
