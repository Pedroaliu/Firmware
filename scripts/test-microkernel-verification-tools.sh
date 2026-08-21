#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

bash "${ROOT_DIR}/scripts/test-microkernel-models.sh"
python3 "${ROOT_DIR}/verification/host/trace_checker_test.py"

echo "Jixia microkernel verification tools: PASS"
