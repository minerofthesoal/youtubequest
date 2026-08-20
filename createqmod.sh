#!/usr/bin/env bash
# Renders mod.template.json into mod.json (filling in the dependency list from
# what qpm actually resolved) and zips it up with the built .so.
set -euo pipefail

if [ ! -f build/libyoutubelivechat.so ]; then
    echo "error: build/libyoutubelivechat.so not found -- run ./build.sh first." >&2
    exit 1
fi

qpm qmod manifest
qpm qmod zip --skip_build YouTubeLiveChat.qmod
unzip -l YouTubeLiveChat.qmod
