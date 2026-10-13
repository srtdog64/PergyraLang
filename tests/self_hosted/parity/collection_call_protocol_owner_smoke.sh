#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
LABEL="self-host-parity:collection-call-protocol"
"${PYTHON_BIN:-python3}" "$ROOT_DIR/scripts/render_collection_key_policy.py" --check
echo "[$LABEL] collection key policy projection matches its registry"
