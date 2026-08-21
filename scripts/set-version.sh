#!/usr/bin/env bash
# Sets the mod version in qpm.json, which is the single source of truth:
# CMakeLists.txt reads it for the VERSION compile definition, and
# `qpm qmod manifest` renders it into mod.json's version field.
#
#   ./scripts/set-version.sh 0.3.0
set -euo pipefail

cd "$(dirname "$0")/.."

version="${1:-}"
if [ -z "$version" ]; then
    echo "usage: $0 <version>   (for example: $0 0.3.0)" >&2
    exit 1
fi

# Leading 'v' is easy to type out of habit; accept and strip it.
version="${version#v}"

# qpm requires a real semver triple, and a bad value here would otherwise fail
# much later inside `qpm restore` with a far less obvious message.
if ! printf '%s' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+([-+][0-9A-Za-z.-]+)?$'; then
    echo "error: '$version' is not a semver version (expected X.Y.Z, e.g. 0.3.0)" >&2
    exit 1
fi

python3 - "$version" <<'PYEOF'
import json, sys

version = sys.argv[1]
with open("qpm.json") as f:
    manifest = json.load(f)

previous = manifest["info"].get("version")
manifest["info"]["version"] = version

with open("qpm.json", "w") as f:
    json.dump(manifest, f, indent=2)
    f.write("\n")

print(f"qpm.json: version {previous} -> {version}")
PYEOF
