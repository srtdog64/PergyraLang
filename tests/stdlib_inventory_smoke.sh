#!/usr/bin/env bash
#
# stdlib_inventory_smoke.sh — docs/148's wiring is a CONTRACT, not prose.
# Locks the §4 inventory AND the §3 module contracts against the tree:
#
#   G1 per-type: no generic `<T>` function signatures in stdlib (§3-1;
#      the compiler-side seam is locked by generic-nested-failclosed —
#      this leg keeps stdlib from depending on the unfinished seam).
#   G2 caps: an ACTIVE module that touches ambient builtins must declare
#      `with caps` on those functions (§3-2; sketch modules are exempt
#      until their WO-L4 doctrine-pass — that pass flips them active,
#      which arms this leg).
#   G3 namespace: any module beyond the 2026-07-04 grandfather set must
#      use a namespace block (§3-3).
#   G5 layering: stdlib never imports from src/ (§2 direction: stdlib is
#      the origin; self_hosted imports stdlib, never the reverse).
#   G4-lite: active modules appear in docs/138 (scope row duty, name-level).
#   Inventory: every stdlib/*.pgy has exactly one §4 row and vice versa;
#      active rows name a gate that exists; ledger vocabulary is closed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DOC="$ROOT_DIR/docs/148_stdlib_architecture.md"
SCOPE_DOC="$ROOT_DIR/docs/138_standard_library_scope.md"
STDLIB_DIR="$ROOT_DIR/stdlib"

# Modules that predate the namespace contract (docs/148 §3-3 grandfathers
# them; new or promoted modules must carry a namespace block).
GRANDFATHERED="option.pgy strview.pgy datetime.pgy device_adapter.pgy \
http.pgy ledger.pgy money.pgy obligation.pgy page.pgy spray.pgy \
storage.pgy timer.pgy versioning.pgy"

# Ambient builtins whose use demands a `with caps` declaration (maps to the
# runtime capability gates: io_read/io_write/clock/random/env/input).
AMBIENT_RE='ReadFile\(|WriteFile\(|FileOpen\(|FileExists\(|DirWalk\(|Now\(|Random\(|Args\(|Input\('

fail() { echo "[stdlib-inventory] FAIL: $*" >&2; exit 1; }

[ -f "$DOC" ] || fail "missing docs/148_stdlib_architecture.md"
[ -f "$SCOPE_DOC" ] || fail "missing docs/138_standard_library_scope.md"
[ -d "$STDLIB_DIR" ] || fail "missing stdlib/ directory"

rows="$(sed -n '/STDLIB-INVENTORY-BEGIN/,/STDLIB-INVENTORY-END/p' "$DOC" \
    | grep -E '^\| [a-z_]+\.pgy \|')"
[ -n "$rows" ] || fail "docs/148 inventory block has no module rows"

row_status() {
    printf '%s\n' "$rows" | grep -F "| $1 |" | awk -F'|' '{gsub(/ /,"",$4); print $4}'
}

# ---- per-file contract legs (run before inventory matching so a violating
# ---- new file reports its contract breach, not just a missing row) ----
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

    # G3: namespace duty for non-grandfathered modules.
    case " $GRANDFATHERED " in
        *" $base "*) ;;
        *)
            printf '%s\n' "$body" | grep -Eq '^[[:space:]]*namespace[[:space:]]+[A-Za-z]' ||
                fail "stdlib/$base is new/promoted and must use a namespace block (docs/148 §3-3)"
            ;;
    esac

    # G2: active + ambient builtins => every such use sits under `with caps`.
    status="$(row_status "$base")"
    if [ "$status" = "active" ] || [ "$status" = "stable-subset" ]; then
        if printf '%s\n' "$body" | grep -Eq "$AMBIENT_RE"; then
            printf '%s\n' "$body" | grep -q 'with caps' ||
                fail "stdlib/$base is $status and touches ambient builtins but declares no 'with caps' (docs/148 §3-2)"
        fi
        # G4-lite: scope-row duty, name-level. (plain -qi: this MSYS grep
        # core-dumps on the -F -i combination against this doc)
        name_stem="${base%.pgy}"
        grep -qi -- "$name_stem" "$SCOPE_DOC" ||
            fail "stdlib/$base is $status but docs/138 never mentions '$name_stem' (docs/148 §3-5 row duty)"
    fi
done

# ---- inventory matching ----
for f in "$STDLIB_DIR"/*.pgy; do
    base="$(basename "$f")"
    printf '%s\n' "$rows" | grep -Fq "| $base |" ||
        fail "stdlib/$base has no inventory row in docs/148 §4"
done

while IFS='|' read -r _ module layer status gate _; do
    module="$(echo "$module" | tr -d ' ')"
    layer="$(echo "$layer" | tr -d ' ')"
    status="$(echo "$status" | tr -d ' ')"
    gate="$(echo "$gate" | tr -d ' ')"
    [ -f "$STDLIB_DIR/$module" ] ||
        fail "inventory row '$module' has no file under stdlib/"
    case "$layer" in
        core|domain) ;;
        *) fail "$module: unknown layer '$layer'" ;;
    esac
    case "$status" in
        active)
            [ "$gate" != "-" ] || fail "$module is active but names no gate"
            [ -e "$ROOT_DIR/$gate" ] ||
                fail "$module: active gate '$gate' does not exist"
            ;;
        sketch)
            [ "$gate" = "-" ] || fail "$module is sketch but names a gate '$gate' (promote it to active instead)"
            ;;
        stable-subset) ;;
        *) fail "$module: unknown status '$status'" ;;
    esac
done < <(printf '%s\n' "$rows")

echo "[stdlib-inventory] inventory + contracts (per-type/caps/namespace/layering/scope-row) locked"
