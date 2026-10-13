#!/usr/bin/env bash
set -euo pipefail

# CLOSED fallback identities: native_field_kind_string_table,
# selfhost_field_kind_string_table, stale_field_kind_projection.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REGISTRY="$ROOT_DIR/src/compiler/mir_decl_field_kind_vocabulary.def"
PROJECTION="$ROOT_DIR/src/self_hosted/lib/mir_decl_field_kind_vocabulary_projection_owner.pgy"
PYTHON_BIN="${PYTHON_BIN:-python3}"

"$PYTHON_BIN" "$ROOT_DIR/scripts/render_mir_decl_field_kind_vocabulary.py" \
    "$REGISTRY" "$PROJECTION" --check

echo "[mir-decl-field-kind-vocabulary] 14 wire identities and projections: ok"
