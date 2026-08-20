#!/usr/bin/env bash
# Renders mod.json from mod.template.json and packages the .qmod.
set -euo pipefail
exec "$(dirname "$0")/scripts/package-qmod.sh" "$@"
