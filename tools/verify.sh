#!/usr/bin/env bash
# Build + test simcore and run the Python prototype tests (macOS / Linux).
# Run from anywhere:  ./tools/verify.sh
set -euo pipefail
cd "$(dirname "$0")/.."

echo "==> simcore: configure + build (Release)"
cmake -S simcore -B simcore/build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build simcore/build --config Release -j

echo "==> simcore: tests"
ctest --test-dir simcore/build -C Release --output-on-failure

echo "==> simcore: engagement demo"
mkdir -p out
./simcore/build/engagement_demo out/engagement_demo.acmi

echo "==> prototype: pytest"
PY=python3
if ! "$PY" -m pytest --version >/dev/null 2>&1; then
  if [ ! -d .venv ]; then "$PY" -m venv .venv; fi
  ./.venv/bin/pip install -q pytest
  PY=./.venv/bin/python
fi
"$PY" -m pytest prototype -q

echo
echo "All checks passed. Tacview replay: out/engagement_demo.acmi"
