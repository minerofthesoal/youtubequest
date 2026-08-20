# Renders mod.json from mod.template.json and packages the .qmod.
# Shells out to the same script CI uses, so the two can't drift apart.
$ErrorActionPreference = "Stop"
bash ./scripts/package-qmod.sh @args
