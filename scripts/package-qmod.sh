#!/usr/bin/env bash
# Renders mod.template.json into mod.json and packages the .qmod.
#
# mod.json is generated rather than hand-maintained so that the dependency
# list -- ids, version ranges and downloadIfMissing URLs -- always comes from
# what qpm actually resolved. Hand-written URLs rot silently when an upstream
# repo moves owner, which is how this mod ended up shipping a manifest whose
# download links 404'd.
set -euo pipefail

cd "$(dirname "$0")/.."

SO_NAME="libyoutubelivechat.so"
OUT="${1:-YouTubeLiveChat.qmod}"

if [ ! -f "build/${SO_NAME}" ]; then
    echo "error: build/${SO_NAME} not found -- build first." >&2
    exit 1
fi

# scotland2 is excluded because it is the modloader, not a library this mod
# ships. It is a private dependency in qpm.json, which otherwise makes qpm
# list libsl2.so under libraryFiles and pack the loader binary into the
# archive -- a mod must never carry a copy of the loader that loads it.
qpm qmod manifest --exclude_libs scotland2

# The archive is built here rather than by `qpm qmod zip` so its contents are
# exactly the files mod.json declares, and that correspondence is checked
# rather than assumed: a manifest naming a file the archive doesn't contain
# fails at install time on the headset, which is a slow place to find out.
python3 - "$SO_NAME" <<'PYEOF'
import json, sys

so_name = sys.argv[1]
manifest = json.load(open("mod.json"))
declared = (manifest.get("modFiles", []) + manifest.get("lateModFiles", []) +
            manifest.get("libraryFiles", []))
if declared != [so_name]:
    sys.exit(f"mod.json declares {declared}, expected exactly ['{so_name}']")

deps = [d.get("id") for d in manifest.get("dependencies", [])]
for required in ("beatsaber-hook", "custom-types", "bsml", "paper2_scotland2"):
    if required not in deps:
        sys.exit(f"mod.json is missing the {required} dependency (got {deps})")
print(f"manifest ok: {so_name}, dependencies {deps}")
PYEOF

rm -f "$OUT"
zip -j "$OUT" mod.json "build/${SO_NAME}"
unzip -l "$OUT"
