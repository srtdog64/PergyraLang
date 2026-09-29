#!/usr/bin/env bash
# Stable collection ownership identity must survive native and installed
# self-host MIR production, and its Pergyra reader must reject malformed rows.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-collection-ownership-carrier"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
SOURCE="tests/concept_semantics/hashmap/map_keys_owned_drop_valid.pgy"
EMPTY_SOURCE="tests/concept_semantics/hashmap/empty_string_array_fact_valid.pgy"
UNKNOWN_SOURCE="tests/concept_semantics/hashmap/unknown_string_array_drop.pgy"
CALL_RESULT_SOURCE="tests/concept_semantics/hashmap/call_result_string_array_fact_valid.pgy"
PROBE="tests/self_hosted/parity/fixture/collection_ownership_fact_reader_probe.pgy"
CC="${PGY_SELFHOST_CC:-gcc}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection-ownership-carrier.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"

(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$SOURCE") \
    >"$WORK_DIR/native.json" 2>"$WORK_DIR/native.err" ||
    fail "native MIR producer rejected the valid ownership fixture"
grep -Fxq '0 error(s), 0 warning(s)' "$WORK_DIR/native.err" ||
    fail "native MIR producer emitted diagnostics"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$EMPTY_SOURCE") \
    >"$WORK_DIR/native-empty.json" 2>"$WORK_DIR/native-empty.err" ||
    fail "native MIR producer rejected the empty-literal origin fixture"
grep -Fxq '0 error(s), 0 warning(s)' "$WORK_DIR/native-empty.err" ||
    fail "native empty MIR producer emitted diagnostics"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$UNKNOWN_SOURCE") \
    >"$WORK_DIR/native-unknown.json" 2>"$WORK_DIR/native-unknown.err" ||
    fail "native MIR producer rejected the unknown-origin control"
grep -Fxq '0 error(s), 0 warning(s)' "$WORK_DIR/native-unknown.err" ||
    fail "native unknown-origin producer emitted diagnostics"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE" \
    -o "$WORK_REL/self.json") >"$WORK_DIR/self.out" \
    2>"$WORK_DIR/self.err" ||
    fail "installed self-host MIR producer rejected the valid ownership fixture"
[[ -s "$WORK_DIR/self.json" ]] || fail "installed self-host emitted no MIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$EMPTY_SOURCE" \
    -o "$WORK_REL/self-empty.json") >"$WORK_DIR/self-empty.out" \
    2>"$WORK_DIR/self-empty.err" ||
    fail "installed self-host MIR producer rejected the empty-literal origin fixture"
[[ -s "$WORK_DIR/self-empty.json" ]] ||
    fail "installed self-host emitted no empty-ownership MIR"
printf '%s\n' preserved:unknown >"$WORK_DIR/self-unknown.json"
if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$UNKNOWN_SOURCE" \
    -o "$WORK_REL/self-unknown.json") >"$WORK_DIR/self-unknown.out" \
    2>"$WORK_DIR/self-unknown.err"; then
    fail "installed self-host admitted an unknown-provenance deep drop"
fi
[[ "$(cat "$WORK_DIR/self-unknown.json")" == preserved:unknown ]] ||
    fail "unknown-provenance refusal replaced the prior artifact"
grep -Fq 'borrow_boundary_escape' \
    "$WORK_DIR/self-unknown.out" "$WORK_DIR/self-unknown.err" ||
    fail "unknown-provenance refusal lost its semantic diagnostic"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$CALL_RESULT_SOURCE" -o "$WORK_REL/self-call-result.json") \
    >"$WORK_DIR/self-call-result.out" 2>"$WORK_DIR/self-call-result.err" ||
    fail "installed self-host rejected the call-result provenance control"
[[ "$(grep -Fc '"origin":"call-result"' \
    "$WORK_DIR/self-call-result.json")" == 1 ]] ||
    fail "call-result provenance did not attach exactly once"

(cd "$ROOT_DIR" && "$PGY" "$PROBE" --native-pipeline --backend=c \
    -o "$WORK_REL/probe-native.exe") >"$WORK_DIR/probe-native.compile" 2>&1 || {
    tail -c 65536 "$WORK_DIR/probe-native.compile" >&2
    fail "native pipeline could not build the Pergyra carrier reader"
}
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "$PGY" "$PROBE" \
    --backend=c -o "$WORK_REL/probe-self.exe") \
    >"$WORK_DIR/probe-self.compile" 2>&1 || {
    tail -c 65536 "$WORK_DIR/probe-self.compile" >&2
    fail "installed self-host could not build the Pergyra carrier reader"
}

for producer in native self; do
    for consumer in native self; do
        (cd "$ROOT_DIR" && "$WORK_DIR/probe-$consumer.exe" \
            "$WORK_REL/$producer.json") \
            >"$WORK_DIR/$producer-$consumer.out" \
            2>"$WORK_DIR/$producer-$consumer.err" || {
            cat "$WORK_DIR/$producer-$consumer.out" \
                "$WORK_DIR/$producer-$consumer.err" >&2
            fail "$consumer reader rejected $producer MIR"
        }
        tr -d '\r' <"$WORK_DIR/$producer-$consumer.out" \
            >"$WORK_DIR/$producer-$consumer.normalized"
    done
done

for evidence in native-native native-self self-native self-self; do
    row="$(cat "$WORK_DIR/$evidence.normalized")"
    [[ "$row" =~ ^[1-9][0-9]*:[1-9][0-9]*:[1-9][0-9]*:0:map-keys-snapshot:retired:map-keys$ ]] ||
        fail "$evidence has a malformed stable identity row: $row"
done
# Both Pergyra reader binaries must preserve every ID in one producer's
# namespace. The native and self-host parsers currently allocate different AST
# ID spaces, so cross-producer parity is the carried ownership meaning plus the
# reader-enforced local/routine joins, not accidental integer equality.
cmp -s "$WORK_DIR/native-native.normalized" \
    "$WORK_DIR/native-self.normalized" ||
    fail "native MIR identity changed between Pergyra reader builds"
cmp -s "$WORK_DIR/self-native.normalized" \
    "$WORK_DIR/self-self.normalized" ||
    fail "self-host MIR identity changed between Pergyra reader builds"
cut -d: -f4- "$WORK_DIR/native-native.normalized" \
    >"$WORK_DIR/native-meaning"
cut -d: -f4- "$WORK_DIR/self-native.normalized" \
    >"$WORK_DIR/self-meaning"
cmp -s "$WORK_DIR/native-meaning" "$WORK_DIR/self-meaning" || {
    diff -u "$WORK_DIR/native-meaning" "$WORK_DIR/self-meaning" >&2 || true
    fail "native/self-host collection ownership meaning drifted"
}

for producer in native-empty self-empty; do
    for consumer in native self; do
        (cd "$ROOT_DIR" && "$WORK_DIR/probe-$consumer.exe" \
            "$WORK_REL/$producer.json") \
            >"$WORK_DIR/$producer-$consumer.out" \
            2>"$WORK_DIR/$producer-$consumer.err" || {
            cat "$WORK_DIR/$producer-$consumer.out" \
                "$WORK_DIR/$producer-$consumer.err" >&2
            fail "$consumer reader rejected $producer MIR"
        }
        tr -d '\r' <"$WORK_DIR/$producer-$consumer.out" \
            >"$WORK_DIR/$producer-$consumer.normalized"
        row="$(cat "$WORK_DIR/$producer-$consumer.normalized")"
        [[ "$row" =~ ^[1-9][0-9]*:[1-9][0-9]*:[1-9][0-9]*:0:unknown:live:empty-literal$ ]] ||
            fail "$producer-$consumer lost the empty-literal origin: $row"
    done
done
cmp -s "$WORK_DIR/native-empty-native.normalized" \
    "$WORK_DIR/native-empty-self.normalized" ||
    fail "native empty MIR identity changed between Pergyra reader builds"
cmp -s "$WORK_DIR/self-empty-native.normalized" \
    "$WORK_DIR/self-empty-self.normalized" ||
    fail "self-host empty MIR identity changed between Pergyra reader builds"
cut -d: -f4- "$WORK_DIR/native-empty-native.normalized" \
    >"$WORK_DIR/native-empty-meaning"
cut -d: -f4- "$WORK_DIR/self-empty-native.normalized" \
    >"$WORK_DIR/self-empty-meaning"
cmp -s "$WORK_DIR/native-empty-meaning" \
    "$WORK_DIR/self-empty-meaning" ||
    fail "native/self-host empty-literal origin meaning drifted"

mkdir -p "$WORK_DIR/mutations"
python3 - "$WORK_DIR/native.json" "$WORK_DIR/native-empty.json" \
    "$WORK_DIR/native-unknown.json" "$WORK_DIR/self-call-result.json" \
    "$WORK_DIR/mutations" <<'PY'
import copy
import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
empty_source = pathlib.Path(sys.argv[2])
unknown_path = pathlib.Path(sys.argv[3])
call_result_path = pathlib.Path(sys.argv[4])
target = pathlib.Path(sys.argv[5])
document = json.loads(source.read_text(encoding="utf-8"))
mains = [row for row in document.get("routines", []) if row.get("name") == "Main"]
if len(mains) != 1:
    raise SystemExit(f"expected one Main routine, found {len(mains)}")
main_index = document["routines"].index(mains[0])
main = mains[0]
facts = main.get("collection_ownership_facts")
if main.get("collection_ownership_fact_count") != 1 or not isinstance(facts, list) or len(facts) != 1:
    raise SystemExit("baseline does not own exactly one collection row")
row = facts[0]
required = {
    "function_syntax_id", "binding_syntax_id", "origin_syntax_id",
    "source_binding_syntax_id", "element_ownership", "disposition", "origin",
}
if set(row) != required:
    raise SystemExit(f"baseline row fields drifted: {sorted(row)}")
if not any(local.get("binding_syntax_id") == row["binding_syntax_id"] and
           local.get("type") == "Array<String>" for local in main.get("source_locals", [])):
    raise SystemExit("baseline row does not join an Array<String> local")

def emit(name, mutate):
    candidate = copy.deepcopy(document)
    mutate(candidate["routines"][main_index])
    (target / f"{name}.json").write_text(
        json.dumps(candidate, separators=(",", ":")), encoding="utf-8")

emit("count_mismatch", lambda routine: routine.__setitem__(
    "collection_ownership_fact_count", 2))
emit("missing_count", lambda routine: routine.pop(
    "collection_ownership_fact_count"))
emit("missing_rows", lambda routine: routine.pop("collection_ownership_facts"))
emit("function_zero", lambda routine: routine["collection_ownership_facts"][0].__setitem__(
    "function_syntax_id", 0))
emit("binding_zero", lambda routine: routine["collection_ownership_facts"][0].__setitem__(
    "binding_syntax_id", 0))
emit("origin_zero", lambda routine: routine["collection_ownership_facts"][0].__setitem__(
    "origin_syntax_id", 0))

def self_source(routine):
    fact = routine["collection_ownership_facts"][0]
    fact["source_binding_syntax_id"] = fact["binding_syntax_id"]
    fact["origin"] = "binding"
    fact["disposition"] = "live"
emit("self_source", self_source)

emit("unknown_ownership", lambda routine: routine["collection_ownership_facts"][0].__setitem__(
    "element_ownership", "foreign"))
emit("unknown_disposition", lambda routine: routine["collection_ownership_facts"][0].__setitem__(
    "disposition", "forgotten"))
emit("unknown_origin", lambda routine: routine["collection_ownership_facts"][0].__setitem__(
    "origin", "foreign"))

def unknown_owned(routine):
    fact = routine["collection_ownership_facts"][0]
    fact["element_ownership"] = "owned-elements"
    fact["disposition"] = "live"
    fact["origin"] = "unknown"
    fact["source_binding_syntax_id"] = 0
emit("unknown_owned", unknown_owned)

def retired_unknown(routine):
    fact = routine["collection_ownership_facts"][0]
    fact["element_ownership"] = "unknown"
    fact["disposition"] = "retired"
    fact["origin"] = "unknown"
    fact["source_binding_syntax_id"] = 0
emit("retired_unknown", retired_unknown)

def retired_borrowed(routine):
    fact = routine["collection_ownership_facts"][0]
    fact["element_ownership"] = "borrowed-elements"
    fact["origin"] = "borrowed-literal"
emit("retired_borrowed", retired_borrowed)

def unknown_source(routine):
    fact = routine["collection_ownership_facts"][0]
    fact["source_binding_syntax_id"] = 999999
    fact["origin"] = "binding"
    fact["disposition"] = "live"
emit("unknown_source", unknown_source)

def wrong_local_type(routine):
    binding = routine["collection_ownership_facts"][0]["binding_syntax_id"]
    for local in routine["source_locals"]:
        if local["binding_syntax_id"] == binding:
            local["type"] = "Int"
emit("wrong_local_type", wrong_local_type)

def duplicate_row(routine):
    routine["collection_ownership_facts"].append(
        copy.deepcopy(routine["collection_ownership_facts"][0]))
    routine["collection_ownership_fact_count"] = 2
emit("duplicate_row", duplicate_row)

def forward_source(routine):
    fact = routine["collection_ownership_facts"][0]
    source_id = fact["binding_syntax_id"] + 1
    routine["source_locals"].append({
        "name": "later", "type": "Array<String>",
        "binding_syntax_id": source_id,
    })
    fact["source_binding_syntax_id"] = source_id
    fact["origin"] = "binding"
    fact["disposition"] = "live"
    later = copy.deepcopy(fact)
    later["binding_syntax_id"] = source_id
    later["origin_syntax_id"] += 1
    later["source_binding_syntax_id"] = 0
    later["origin"] = "map-keys"
    routine["collection_ownership_facts"].append(later)
    routine["collection_ownership_fact_count"] = 2
emit("forward_source", forward_source)

def ownership_mismatch(routine):
    target_row = routine["collection_ownership_facts"][0]
    source_id = target_row["binding_syntax_id"] + 1
    routine["source_locals"].append({
        "name": "source", "type": "Array<String>",
        "binding_syntax_id": source_id,
    })
    source_row = copy.deepcopy(target_row)
    source_row["binding_syntax_id"] = source_id
    source_row["origin_syntax_id"] += 1
    source_row["element_ownership"] = "owned-elements"
    source_row["disposition"] = "live"
    source_row["origin"] = "unknown"
    target_row["source_binding_syntax_id"] = source_id
    target_row["disposition"] = "live"
    target_row["origin"] = "binding"
    routine["collection_ownership_facts"] = [source_row, target_row]
    routine["collection_ownership_fact_count"] = 2
emit("ownership_mismatch", ownership_mismatch)

emit("fractional_binding", lambda routine: routine["collection_ownership_facts"][0].__setitem__(
    "binding_syntax_id", 1.5))

raw = json.dumps(document, separators=(",", ":"))
needle = '"origin":"map-keys"'
if raw.count(needle) != 1:
    raise SystemExit("cannot construct duplicate-field mutation")
(target / "duplicate_field.json").write_text(
    raw.replace(needle, '"origin":"map-keys","origin":"map-keys"', 1),
    encoding="utf-8")

empty_document = json.loads(empty_source.read_text(encoding="utf-8"))
empty_mains = [
    row for row in empty_document.get("routines", [])
    if row.get("name") == "Main"
]
if len(empty_mains) != 1:
    raise SystemExit(
        f"expected one empty Main routine, found {len(empty_mains)}")
empty_facts = empty_mains[0].get("collection_ownership_facts")
if (empty_mains[0].get("collection_ownership_fact_count") != 1 or
        not isinstance(empty_facts, list) or len(empty_facts) != 1):
    raise SystemExit("empty baseline does not own exactly one collection row")
empty_owned_document = copy.deepcopy(empty_document)
empty_owned_main = next(
    row for row in empty_owned_document["routines"]
    if row.get("name") == "Main"
)
empty_owned_main["collection_ownership_facts"][0][
    "element_ownership"
] = "owned-elements"
(target / "empty_literal_owned.json").write_text(
    json.dumps(empty_owned_document, separators=(",", ":")),
    encoding="utf-8")

empty_graph_document = copy.deepcopy(empty_document)
empty_graph_main = next(
    row for row in empty_graph_document["routines"]
    if row.get("name") == "Main"
)
empty_binding = empty_graph_main["collection_ownership_facts"][0][
    "binding_syntax_id"
]
empty_ref = f"declaration:{empty_binding}:0"
empty_definitions = [
    instruction
    for block in empty_graph_main.get("blocks", [])
    for instruction in block.get("instructions", [])
    if instruction.get("local_ref") == empty_ref
]
if len(empty_definitions) != 1:
    raise SystemExit(
        f"empty baseline definition receipt count: {len(empty_definitions)}")
empty_graph = empty_definitions[0].get("expr0_graph")
if (not isinstance(empty_graph, dict) or
        len(empty_graph.get("nodes", [])) != 1 or
        empty_graph["nodes"][0].get("kind") != "array_literal"):
    raise SystemExit("empty baseline does not carry one literal graph")
empty_graph["nodes"][0]["kind"] = "leaf"
(target / "empty_literal_graph_kind.json").write_text(
    json.dumps(empty_graph_document, separators=(",", ":")),
    encoding="utf-8")

unknown_document = json.loads(unknown_path.read_text(encoding="utf-8"))
unknown_mains = [
    row for row in unknown_document.get("routines", [])
    if row.get("name") == "Main"
]
if len(unknown_mains) != 1:
    raise SystemExit(
        f"expected one unknown Main routine, found {len(unknown_mains)}")
unknown_facts = unknown_mains[0].get("collection_ownership_facts")
if (unknown_mains[0].get("collection_ownership_fact_count") != 1 or
        not isinstance(unknown_facts, list) or len(unknown_facts) != 1 or
        unknown_facts[0].get("origin") != "unknown"):
    raise SystemExit("unknown baseline does not own one unknown row")
unknown_facts[0]["origin"] = "empty-literal"
(target / "unknown_relabelled_empty.json").write_text(
    json.dumps(unknown_document, separators=(",", ":")),
    encoding="utf-8")

call_result_document = json.loads(call_result_path.read_text(encoding="utf-8"))
call_result_main = next(
    row for row in call_result_document.get("routines", [])
    if row.get("name") == "Main"
)
call_result_facts = call_result_main.get("collection_ownership_facts")
if (call_result_main.get("collection_ownership_fact_count") != 1 or
        not isinstance(call_result_facts, list) or
        len(call_result_facts) != 1 or
        call_result_facts[0].get("origin") != "call-result"):
    raise SystemExit("call-result baseline does not own one carried row")

call_result_unknown = copy.deepcopy(call_result_document)
unknown_main = next(
    row for row in call_result_unknown["routines"] if row.get("name") == "Main")
unknown_main["collection_ownership_facts"][0]["origin_syntax_id"] = 999999
(target / "call_result_unknown_target.json").write_text(
    json.dumps(call_result_unknown, separators=(",", ":")), encoding="utf-8")

call_result_wrong_return = copy.deepcopy(call_result_document)
wrong_main = next(
    row for row in call_result_wrong_return["routines"] if row.get("name") == "Main")
wrong_main["collection_ownership_facts"][0]["origin_syntax_id"] = \
    wrong_main["source_syntax_id"]
(target / "call_result_wrong_return_target.json").write_text(
    json.dumps(call_result_wrong_return, separators=(",", ":")), encoding="utf-8")

call_result_owned = copy.deepcopy(call_result_document)
owned_main = next(
    row for row in call_result_owned["routines"] if row.get("name") == "Main")
owned_main["collection_ownership_facts"][0]["element_ownership"] = \
    "owned-elements"
(target / "call_result_owned_without_summary.json").write_text(
    json.dumps(call_result_owned, separators=(",", ":")), encoding="utf-8")
PY

mutation_count=0
for mutation in "$WORK_DIR"/mutations/*.json; do
    mutation_count=$((mutation_count + 1))
    if (cd "$ROOT_DIR" && "$WORK_DIR/probe-self.exe" \
        "${mutation#"$ROOT_DIR/"}") \
        >"$mutation.out" 2>"$mutation.err"; then
        fail "self-host reader accepted mutation $(basename "$mutation")"
    fi
    grep -Fq 'collection ownership probe rejected collection_ownership_' \
        "$mutation.out" "$mutation.err" || {
        cat "$mutation.out" "$mutation.err" >&2
        fail "mutation $(basename "$mutation") did not fail at the ownership reader"
    }
done
[[ "$mutation_count" -ge 26 ]] ||
    fail "mutation corpus is incomplete: $mutation_count"

echo "[$LABEL] native/self-host producer-consumer parity and $mutation_count malformed-row refusals PASS"
