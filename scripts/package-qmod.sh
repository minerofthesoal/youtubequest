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

qpm qmod manifest

# The archive is built here rather than by `qpm qmod zip` so its contents are
# exactly the files mod.json declares, and that correspondence is checked
# rather than assumed: a manifest naming a file the archive doesn't contain
# fails at install time on the headset, which is a slow place to find out.
python3 - "$SO_NAME" <<'PYEOF'
import json, sys

so_name = sys.argv[1]
# libsl2.so is Scotland2, the modloader itself. qpm lists it under
# libraryFiles because scotland2 is a private dependency in qpm.json (it is
# there for its headers -- setup/late_load and modloader paths). A mod must
# never carry a copy of the loader that loads it: installing this qmod would
# drop our pinned libsl2.so into the modloader directory and could downgrade
# or replace the one the user actually patched their game with.
# `qpm qmod manifest --exclude_libs scotland2` does not drop it, so it is
# removed here explicitly.
MODLOADER_LIB = "libsl2.so"

with open("mod.json") as f:
    manifest = json.load(f)

libs = manifest.get("libraryFiles", [])
if MODLOADER_LIB in libs:
    manifest["libraryFiles"] = [lib for lib in libs if lib != MODLOADER_LIB]
    print(f"dropped {MODLOADER_LIB} (the modloader) from libraryFiles")
    with open("mod.json", "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

declared = (manifest.get("modFiles", []) + manifest.get("lateModFiles", []) +
            manifest.get("libraryFiles", []))
if declared != [so_name]:
    sys.exit(f"mod.json declares {declared}, expected exactly ['{so_name}']")

deps = [d.get("id") for d in manifest.get("dependencies", [])]
for required in ("beatsaber-hook", "custom-types", "bsml", "paper2_scotland2"):
    if required not in deps:
        sys.exit(f"mod.json is missing the {required} dependency (got {deps})")

for dep in manifest.get("dependencies", []):
    if not dep.get("downloadIfMissing"):
        sys.exit(f"dependency {dep.get('id')} has no downloadIfMissing URL")

print(f"manifest ok: {so_name}, dependencies {deps}")
PYEOF

echo "--- mod.json ---"
cat mod.json
echo

rm -f "$OUT"
zip -j "$OUT" mod.json "build/${SO_NAME}"
unzip -l "$OUT"
