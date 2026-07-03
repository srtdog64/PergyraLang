#!/usr/bin/env bash
#
# stdlib_inventory_smoke.sh — docs/148 §4 inventory is a CONTRACT, not prose.
# Locks three facts so the stdlib wiring doc cannot drift from the tree:
#   1. every stdlib/*.pgy file has exactly one inventory row (and vice versa);
#   2. every `active` row names a gate that actually exists on disk;
#   3. every row's status is one of the ledger states (active|sketch|stable-subset).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DOC="$ROOT_DIR/docs/148_stdlib_architecture.md"
STDLIB_DIR="$ROOT_DIR/stdlib"

fail() { echo "[stdlib-inventory] FAIL: $*" >&2; exit 1; }

[ -f "$DOC" ] || fail "missing docs/148_stdlib_architecture.md"
[ -d "$STDLIB_DIR" ] || fail "missing stdlib/ directory"

# Extract the inventory block rows: "| module.pgy | layer | status | gate |"
rows="$(sed -n '/STDLIB-INVENTORY-BEGIN/,/STDLIB-INVENTORY-END/p' "$DOC" \
    | grep -E '^\| [a-z_]+\.pgy \|')"
[ -n "$rows" ] || fail "docs/148 inventory block has no module rows"

# 1a. every tree file has a row.
for f in "$STDLIB_DIR"/*.pgy; do
    base="$(basename "$f")"
    printf '%s\n' "$rows" | grep -Fq "| $base |" ||
        fail "stdlib/$base has no inventory row in docs/148 §4"
done

# 1b. every row has a tree file; 2. active rows have a real gate; 3. status vocab.
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

echo "[stdlib-inventory] docs/148 inventory == tree; active gates exist"
