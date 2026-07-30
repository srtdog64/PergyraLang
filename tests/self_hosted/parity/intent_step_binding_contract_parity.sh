#!/usr/bin/env bash
# Public scaffold entry for the focused intent-step binding owner gate.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/self_hosted/parity/intent_step_binding_contract_owner.sh"
