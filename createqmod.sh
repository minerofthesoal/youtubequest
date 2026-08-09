#!/usr/bin/env bash
# Packages the built .so + mod.json into YouTubeLiveChat.qmod (just a zip)
set -euo pipefail
out="YouTubeLiveChat.qmod"
rm -f "$out"
zip -j "$out" mod.json build/libyoutubelivechat.so
echo "Wrote $out"
