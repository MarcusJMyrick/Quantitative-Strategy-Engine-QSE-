#!/usr/bin/env bash
# Static-analysis gate (C3): clang-tidy over every built TU under src/.
# compile_commands.json is the source of truth, so dead files and generated
# protobuf code are automatically out of scope. Checks and suppressions live
# in .clang-tidy; any finding fails via WarningsAsErrors.
#
# CI (Linux):  scripts/run_clang_tidy.sh
# macOS local: CLANG_TIDY=venv/bin/clang-tidy scripts/run_clang_tidy.sh \
#                  --extra-arg="-isysroot$(xcrun --show-sdk-path)"
set -euo pipefail
cd "$(dirname "$0")/.."

CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"

if [ ! -f build/compile_commands.json ]; then
    echo "build/compile_commands.json not found - configure with" >&2
    echo "  cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
    exit 1
fi

python3 - <<'PY' > /tmp/qse_tidy_files.txt
import json
import os

root = os.getcwd()
cmds = json.load(open("build/compile_commands.json"))
files = sorted({c["file"] for c in cmds if c["file"].startswith(os.path.join(root, "src") + os.sep)})
print("\n".join(f for f in files if not f.endswith(".pb.cc")))
PY

echo "clang-tidy over $(wc -l < /tmp/qse_tidy_files.txt | tr -d ' ') translation units..."
xargs -P "$(getconf _NPROCESSORS_ONLN)" -n 1 "$CLANG_TIDY" -p build --quiet "$@" \
    < /tmp/qse_tidy_files.txt
echo "clang-tidy: clean"
