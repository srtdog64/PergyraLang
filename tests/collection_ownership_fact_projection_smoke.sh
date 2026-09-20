#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="collection-ownership-fact-projection"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
mkdir -p "$ROOT_DIR/.tmp"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/collection-ownership-fact-projection.XXXXXX")"

! grep -Fq 'string_array_ownership' "$ROOT_DIR/src/semantic/symbol_table.h" ||
    fail "Symbol still owns mutable Array<String> provenance"
! grep -Fq 'PGY_STRING_ARRAY_RETIRED' "$ROOT_DIR/src/semantic/symbol_table.h" ||
    fail "retirement is still encoded in Symbol ownership state"
grep -Eq 'PgyCollectionOwnershipFact[[:space:]]+\*collection_ownership_facts' \
    "$ROOT_DIR/src/semantic/semantic.h" ||
    fail "semantic result does not carry stable collection ownership rows"
grep -Eq 'HIRCollectionOwnershipFact[[:space:]]+\*collection_ownership_facts' \
    "$ROOT_DIR/src/compiler/hir.h" ||
    fail "HIR does not carry stable collection ownership rows"
grep -Eq 'MIRCollectionOwnershipFact[[:space:]]+\*collection_ownership_facts' \
    "$ROOT_DIR/src/compiler/mir_types.h" ||
    fail "MIR does not carry stable collection ownership rows"
grep -Fq 'collection_ownership_facts' \
    "$ROOT_DIR/src/compiler/mir_json_dump.c" ||
    fail "MIR JSON does not publish collection ownership rows"

OWNED_JSON="$WORK_DIR/owned.json"
BORROWED_JSON="$WORK_DIR/borrowed.json"

(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    tests/concept_semantics/hashmap/map_keys_owned_drop_valid.pgy) \
    >"$OWNED_JSON" 2>"$WORK_DIR/owned.err" ||
    fail "native oracle rejected the owned MapKeys fixture"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    tests/concept_semantics/hashmap/borrowed_string_array_fact_valid.pgy) \
    >"$BORROWED_JSON" 2>"$WORK_DIR/borrowed.err" ||
    fail "native oracle rejected the borrowed literal fixture"

python3 - "$OWNED_JSON" "$BORROWED_JSON" <<'PY'
import json
import sys


def main_routine(path):
    with open(path, encoding="utf-8") as handle:
        document = json.load(handle)
    routines = [row for row in document.get("routines", [])
                if row.get("name") == "Main"]
    if len(routines) != 1:
        raise SystemExit(
            f"{path}: expected one Main routine, found {len(routines)}")
    routine = routines[0]
    facts = routine.get("collection_ownership_facts")
    if not isinstance(facts, list):
        raise SystemExit(f"{path}: collection ownership rows are missing")
    if routine.get("collection_ownership_fact_count") != len(facts):
        raise SystemExit(
            f"{path}: declared collection ownership count mismatch")
    return facts


owned = main_routine(sys.argv[1])
if len(owned) != 1:
    raise SystemExit(f"owned row cardinality: {len(owned)}")
row = owned[0]
expected_fields = {
    "function_syntax_id", "binding_syntax_id", "origin_syntax_id",
    "source_binding_syntax_id", "element_ownership", "disposition", "origin",
}
if set(row) != expected_fields:
    raise SystemExit(f"owned row fields drifted: {sorted(row)}")
if not (row["function_syntax_id"] > 0 and
        row["binding_syntax_id"] > 0 and
        row["origin_syntax_id"] > 0 and
        row["source_binding_syntax_id"] == 0 and
        row["element_ownership"] == "map-keys-snapshot" and
        row["disposition"] == "retired" and
        row["origin"] == "map-keys"):
    raise SystemExit(f"owned row meaning drifted: {row}")

borrowed = main_routine(sys.argv[2])
if len(borrowed) != 1:
    raise SystemExit(f"borrowed row cardinality: {len(borrowed)}")
row = borrowed[0]
if set(row) != expected_fields:
    raise SystemExit(f"borrowed row fields drifted: {sorted(row)}")
if not (row["function_syntax_id"] > 0 and
        row["binding_syntax_id"] > 0 and
        row["origin_syntax_id"] > 0 and
        row["source_binding_syntax_id"] == 0 and
        row["element_ownership"] == "borrowed-elements" and
        row["disposition"] == "live" and
        row["origin"] == "borrowed-literal"):
    raise SystemExit(f"borrowed row meaning drifted: {row}")
PY

grep -Fxq '0 error(s), 0 warning(s)' "$WORK_DIR/owned.err" ||
    fail "owned oracle emitted diagnostics"
grep -Fxq '0 error(s), 0 warning(s)' "$WORK_DIR/borrowed.err" ||
    fail "borrowed oracle emitted diagnostics"

echo "[$LABEL] stable semantic -> HIR -> MIR rows and Symbol-owner deletion PASS"
