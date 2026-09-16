#!/usr/bin/env bash
set -euo pipefail

# SUBSTITUTING gate for selfhost.zone_authority_rows: production MIR seals the
# exact actor/authority Zone slots, and C emission consumes that receipt plus
# the shared domain-runtime sync owner.  The deleted semantic/type-env binding
# path is a forbidden fallback.
# CLOSED fallback vocabulary pinned by this executable gate:
# authority_from_participant_type, undeclared_subject_as_authority,
# where_using_zone_drift, ambiguous_subject_slot_success,
# missing_subject_slot_success, AST_domain_rescan, authority_child_rescan,
# ability_name_only_join, intent_step_binding_owner.pgy,
# intent_step_binding_contract_owner.pgy, intent_zone_subject_slot_owner.pgy,
# final_emitter_semantic_zone_authority_read,
# final_emitter_codegen_type_env_slot_selection, missing_transition_success,
# crossed_actor_authority_transition, non_zone_authority_rows_success.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-zone-authority] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}"
DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
pgy_require_runnable_binary_here "self-host-intent-zone-authority" "$PGY" \
    || fail "PGY_BIN is not runnable"
pgy_require_runnable_binary_here "self-host-intent-zone-authority" "$DRIVER" \
    || fail "self-host driver is not runnable"

PYTHON_BIN="${PYTHON_BIN:-python3}"
CC_BIN="${CC:-gcc}"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"

FIXTURE_REL="tests/self_hosted/parity/fixture/intent_zone_authority_transition.pgy"
BUILD_PARENT="$ROOT_DIR/.tmp/self_hosted"
mkdir -p "$BUILD_PARENT"
BUILD_DIR="$(mktemp -d "$BUILD_PARENT/intent_zone_authority_transition.XXXXXX")"
MIR_REL="${BUILD_DIR#"$ROOT_DIR/"}/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$FIXTURE_REL" \
    -o "$MIR_REL" >"$BUILD_DIR/mir.out" 2>"$BUILD_DIR/mir.err") \
    || { cat "$BUILD_DIR/mir.out" "$BUILD_DIR/mir.err" >&2; fail "MIR production failed"; }
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$MIR_REL" \
    >"$BUILD_DIR/from-mir.c" 2>"$BUILD_DIR/from-mir.err") \
    || { cat "$BUILD_DIR/from-mir.err" >&2; fail "MIR C emission failed"; }
(cd "$ROOT_DIR" && "$DRIVER" "$FIXTURE_REL" --emit-c-verified \
    >"$BUILD_DIR/from-source.c" 2>"$BUILD_DIR/from-source.err") \
    || { cat "$BUILD_DIR/from-source.err" >&2; fail "source C emission failed"; }
cmp -s "$BUILD_DIR/from-mir.c" "$BUILD_DIR/from-source.c" \
    || fail "source entrypoint bypassed the admitted MIR transition"

for anchor in \
    'bool RunAuthorized(GateZone *gate, Worker *worker, Approver *approver)' \
    '(*gate).worker = (*worker);' \
    'Worker_Bump(&((*gate).worker));' \
    '(*worker) = (*gate).worker;' \
    'GateZone_sync(gate);'; do
    grep -Fq -- "$anchor" "$BUILD_DIR/from-source.c" \
        || fail "emitted C lost transition anchor: $anchor"
done

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$BUILD_DIR/from-source.c" -o "$BUILD_DIR/self.exe"
(cd "$ROOT_DIR" && "$PGY" "$FIXTURE_REL" --native-pipeline \
    --backend=c -o "$BUILD_DIR/native-c.exe" \
    >"$BUILD_DIR/native-c.compile.out" 2>"$BUILD_DIR/native-c.compile.err") \
    || { cat "$BUILD_DIR/native-c.compile.out" "$BUILD_DIR/native-c.compile.err" >&2; fail "native C compile failed"; }
(cd "$ROOT_DIR" && "$PGY" "$FIXTURE_REL" --native-pipeline \
    --backend=llvm -o "$BUILD_DIR/native-llvm.exe" \
    >"$BUILD_DIR/native-llvm.compile.out" 2>"$BUILD_DIR/native-llvm.compile.err") \
    || { cat "$BUILD_DIR/native-llvm.compile.out" "$BUILD_DIR/native-llvm.compile.err" >&2; fail "native LLVM compile failed"; }

"$BUILD_DIR/self.exe" | tr -d '\r' >"$BUILD_DIR/self.run"
"$BUILD_DIR/native-c.exe" | tr -d '\r' >"$BUILD_DIR/native-c.run"
"$BUILD_DIR/native-llvm.exe" | tr -d '\r' >"$BUILD_DIR/native-llvm.run"
printf '%s\n' 'ok=true' 'worker=2' 'gate=2' >"$BUILD_DIR/expected.run"
cmp -s "$BUILD_DIR/expected.run" "$BUILD_DIR/self.run" \
    || { cat "$BUILD_DIR/self.run" >&2; fail "self runtime output drifted"; }
cmp -s "$BUILD_DIR/self.run" "$BUILD_DIR/native-c.run" \
    || fail "self/native C output differs"
cmp -s "$BUILD_DIR/self.run" "$BUILD_DIR/native-llvm.run" \
    || fail "self/native LLVM output differs"

"$PYTHON_BIN" - "$MIR" "$BUILD_DIR" <<'PY'
import copy
import json
from pathlib import Path
import sys

source = Path(sys.argv[1])
out = Path(sys.argv[2])
document = json.loads(source.read_text(encoding="utf-8"))
zone = next(row for row in document["decls"] if row.get("name") == "GateZone")

missing = copy.deepcopy(document)
next(row for row in missing["decls"] if row.get("name") == "GateZone")["zone_authorities"] = []
(out / "authority-missing.json").write_text(json.dumps(missing, separators=(",", ":")), encoding="utf-8")

drift = copy.deepcopy(document)
next(row for row in drift["decls"] if row.get("name") == "GateZone")["zone_authorities"][0]["subject_slot"] = "worker"
(out / "authority-drift.json").write_text(json.dumps(drift, separators=(",", ":")), encoding="utf-8")

foreign = copy.deepcopy(document)
next(row for row in foreign["decls"] if row.get("name") == "Worker")["zone_authorities"] = []
(out / "non-zone-authority.json").write_text(json.dumps(foreign, separators=(",", ":")), encoding="utf-8")
PY

reject_mutation() {
    local label="$1"
    local expected="$2"
    local rel="${BUILD_DIR#"$ROOT_DIR/"}/$label.json"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json "$rel" \
        >"$BUILD_DIR/$label.c" 2>"$BUILD_DIR/$label.err"); then
        fail "$label mutation was accepted"
    fi
    if grep -Fq '#include <stdio.h>' "$BUILD_DIR/$label.c"; then
        fail "$label mutation published a C artifact"
    fi
    grep -Fq -- "$expected" \
        "$BUILD_DIR/$label.c" "$BUILD_DIR/$label.err" \
        || { cat "$BUILD_DIR/$label.c" "$BUILD_DIR/$label.err" >&2; fail "$label diagnostic drifted"; }
}

reject_mutation authority-missing \
    'MIR intent authority slot is not declared by the Zone'
reject_mutation authority-drift \
    'MIR intent authority slot is not declared by the Zone'
reject_mutation non-zone-authority \
    'MIR domain topology facts are missing or invalid'

for old in \
    src/self_hosted/codegen/emission/intent_step_binding_owner.pgy \
    src/self_hosted/codegen/emission/intent_step_binding_contract_owner.pgy \
    src/self_hosted/codegen/emission/intent_zone_subject_slot_owner.pgy \
    src/self_hosted/tools/intent_step_binding_contract/main.pgy; do
    [[ ! -e "$ROOT_DIR/$old" ]] || fail "legacy binding owner remains: $old"
done
if rg -n 'SemanticAstZoneAuthorityFacts|ast_zone_authority_fact_owner' \
    "$ROOT_DIR/src/self_hosted/codegen/emission" >/dev/null; then
    fail "C emission still reads semantic Zone authority facts"
fi

echo "[self-host-intent-zone-authority] MIR actor/authority transition + shared Zone sync + three no-artifact negatives: PASS"
