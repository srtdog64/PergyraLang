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

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE" \
    -o "$WORK_REL/self.json") >"$WORK_DIR/self.out" \
    2>"$WORK_DIR/self.err" ||
    fail "installed self-host MIR producer rejected the valid ownership fixture"
[[ -s "$WORK_DIR/self.json" ]] || fail "installed self-host emitted no MIR"

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

mkdir -p "$WORK_DIR/mutations"
python3 - "$WORK_DIR/native.json" "$WORK_DIR/mutations" <<'PY'
import copy
import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
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
[[ "$mutation_count" -ge 17 ]] ||
    fail "mutation corpus is incomplete: $mutation_count"

echo "[$LABEL] native/self-host producer-consumer parity and $mutation_count malformed-row refusals PASS"
