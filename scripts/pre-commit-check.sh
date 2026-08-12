#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

cd "${ROOT_DIR}"

echo "[Jixia pre-commit] checking staged whitespace"
git diff --cached --check

echo "[Jixia pre-commit] checking staged C/C++ formatting"
bash scripts/check-format.sh --cached

echo "[Jixia pre-commit] PASS"
