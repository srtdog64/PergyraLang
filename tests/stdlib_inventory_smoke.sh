#!/usr/bin/env bash
#
# stdlib_inventory_smoke.sh — tree-wide stdlib source rules (docs/148 §2/§3):
#
#   G1 per-type: no generic `<T>` function signatures in stdlib (§3-1;
#      the compiler-side seam is locked by generic-nested-failclosed —
#      this leg keeps stdlib from depending on the unfinished seam).
#   G5 layering: stdlib never imports from src/ (§2 direction: stdlib is
#      the origin; self_hosted imports stdlib, never the reverse).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
STDLIB_DIR="$ROOT_DIR/stdlib"

fail() { echo "[stdlib-inventory] FAIL: $*" >&2; exit 1; }

[ -d "$STDLIB_DIR" ] || fail "missing stdlib/ directory"

# ---- per-file contract legs ----
for f in "$STDLIB_DIR"/*.pgy; do
    base="$(basename "$f")"
    body="$(sed 's://.*$::' "$f")"

    # G1: no generic function signatures in stdlib source.
    if printf '%s\n' "$body" | grep -Eq '^[[:space:]]*(export[[:space:]]+)?func[[:space:]]+[A-Za-z0-9_]+<'; then
        fail "stdlib/$base declares a generic function; docs/148 §3-1 forbids <T> in stdlib until the constructed-type specialization seam lands (use per-type functions)"
    fi

    # G5: stdlib is the origin layer — it never imports from src/.
    if printf '%s\n' "$body" | grep -Eq '^[[:space:]]*import[[:space:]]+"[^"]*src/'; then
        fail "stdlib/$base imports from src/; docs/148 §2 forbids the reverse dependency (self_hosted imports stdlib, never the other way)"
    fi
done

echo "[stdlib-inventory] per-type and layering contracts locked"
