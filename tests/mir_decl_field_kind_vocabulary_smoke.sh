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

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
sys.path.insert(0, str(root / "scripts"))
import render_mir_decl_field_kind_vocabulary as registry

rows = registry.load_rows(
    root / "src/compiler/mir_decl_field_kind_vocabulary.def"
)
assert len(rows) == 14
assert {row.spelling for row in rows} >= {
    "field", "shared_field", "subject_slot", "object_slot", "tobject_slot",
    "binding_slot", "effect_slot", "relation_slot", "effect_pool", "relation_pool",
}
binding = next(row for row in rows if row.spelling == "binding_slot")
assert binding.ast_label == "BindingSlot"

native = (root / "src/compiler/mir_json_dump_decl.c").read_text(encoding="utf-8")
assert '#include "mir_decl_field_kind_vocabulary.def"' in native
assert "case MIR_DECL_FIELD_SHARED:\n        return kMirDeclFieldKindSHARED_FIELD;" in native
for row in rows:
    assert f"kMirDeclFieldKind{row.identity}" in native

consumers = [
    "src/compiler/mir_json_dump_decl.c",
    "src/self_hosted/lib/nominal_field_kind_owner.pgy",
    "src/self_hosted/hir/ast_text_inventory_owner.pgy",
    "src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy",
    "src/self_hosted/mir/declaration_verify_owner.pgy",
    "src/self_hosted/mir_lower/decl_lower.pgy",
]
for relative in consumers:
    text = (root / relative).read_text(encoding="utf-8")
    for row in rows:
        assert f'"{row.spelling}"' not in text, (relative, row.spelling)
PY

grep -Fq 'import "mir_decl_field_kind_vocabulary_projection_owner.pgy";' \
    "$ROOT_DIR/src/self_hosted/lib/nominal_field_kind_owner.pgy"
grep -Fq 'StartsWith(text, "Shared: ") { return NominalFieldKindSharedField(); }' \
    "$ROOT_DIR/src/self_hosted/hir/ast_text_inventory_owner.pgy"

echo "[mir-decl-field-kind-vocabulary] 14 wire identities and projections: ok"
