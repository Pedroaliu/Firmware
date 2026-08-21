#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

python3 "${ROOT_DIR}/verification/model/ipc_model_check.py"
python3 "${ROOT_DIR}/verification/model/scheduler_model_check.py"

echo "Jixia microkernel executable models: PASS"
