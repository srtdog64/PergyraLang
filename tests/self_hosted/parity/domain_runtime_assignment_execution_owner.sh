#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[self-host-parity:domain-runtime-assignment] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "domain-runtime-assignment" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "python3/python is required"
    fi
fi

CC_BIN="${CC:-gcc}"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"

FIXTURE_REL="tests/cases/backend_compare/zone_layer_projection_runtime/main.pgy"
FIXTURE="$ROOT_DIR/$FIXTURE_REL"
BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/domain_runtime_assignment_execution"
DRIVER="$BUILD_DIR/driver_rung2.exe"
SELF_MIR="$BUILD_DIR/self.mir.json"
NATIVE_MIR="$BUILD_DIR/native.mir.json"
SELF_C="$BUILD_DIR/self.mir.c"
DIRECT_C="$BUILD_DIR/self.direct.c"
SELF_EXE="$BUILD_DIR/self.exe"
DIRECT_EXE="$BUILD_DIR/self.direct.exe"
mkdir -p "$BUILD_DIR"

if [[ -n "${PGY_SELFHOST_PREBUILT_DRIVER:-}" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$PGY_SELFHOST_PREBUILT_DRIVER")"
    [[ -x "$DRIVER" ]] || fail "prebuilt driver is not runnable: $DRIVER"
else
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) \
        || { cat "$BUILD_DIR/driver.compile.log" >&2; fail "driver build failed"; }
fi

for term in \
    'struct MirDomainRuntimeAssignmentFacts' \
    'MirDomainRuntimeAssignmentFactsFromDocument' \
    'struct MirDomainRuntimePlan' \
    'MirDomainRuntimePlanFromFacts' \
    'CodegenDomainRuntimeFactsFromAdmittedOrDie'; do
    grep -R -Fq -- "$term" \
        "$ROOT_DIR/src/self_hosted/mir_lower" \
        "$ROOT_DIR/src/self_hosted/compiler" \
        || fail "missing runtime-assignment owner term: $term"
done

"$PYTHON_BIN" - \
    "$ROOT_DIR/src/self_hosted/mir_lower/domain_runtime_plan_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/codegen/input/domain_runtime_codegen_view_owner.pgy" <<'PY'
import pathlib
import sys

plan_text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
view_text = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8")
def function_slice(text, name):
    start = text.find(f"func {name}(")
    assert start >= 0, name
    end = text.find("\nfunc ", start + 1)
    return text[start:] if end < 0 else text[start:end]

for name in (
    "MirDomainRuntimePlanOperationCountForOwner",
    "MirDomainRuntimePlanOperationRowForOwnerAt",
):
    assert "MirDomainRuntimePlanReady(" not in function_slice(plan_text, name), name
assert "CodegenDomainRuntimeFactsReady(" not in function_slice(
    view_text, "CodegenDomainRuntimeMethodPrologue"
)
PY

(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" \
    2>"$BUILD_DIR/native.mir.err" | tr -d '\r' >"$NATIVE_MIR") \
    || { cat "$BUILD_DIR/native.mir.err" >&2; fail "native MIR production failed"; }
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$FIXTURE_REL" \
    2>"$BUILD_DIR/self.mir.err" | tr -d '\r' >"$SELF_MIR") \
    || { cat "$BUILD_DIR/self.mir.err" >&2; fail "self MIR production failed"; }

"$PYTHON_BIN" - "$NATIVE_MIR" "$SELF_MIR" "$BUILD_DIR" <<'PY'
import copy
import json
import pathlib
import sys

native_path, self_path, output_dir = sys.argv[1:]
documents = []
for path in (native_path, self_path):
    with open(path, encoding="utf-8") as stream:
        document = json.load(stream)
    assert document["schema"] == "pgy.mir.v1"
    runtime = document["domain_runtime_assignments"]
    roles = {
        (row["owner_name"], row["role"], row["field_name"], row["field_type_name"])
        for row in runtime["participant_roles"]
    }
    assert roles == {
        ("Poisoned", "effect-bearer", "bearer", "Player"),
        ("TrustedLink", "relation-source", "source", "Player"),
        ("TrustedLink", "relation-target", "target", "Player"),
    }, roles
    projections = {
        (
            row["owner_name"], row["operation"],
            row["projection_slot_name"], row["target_field_name"],
            row["source_slot_name"], row["source_path"],
            row["target_field_type_name"], row["source_leaf_type_name"],
            row["explicit_map"],
        )
        for row in runtime["projection_members"]
    }
    assert projections == {
        ("Poisoned", "refresh", "view", "hp", "bearer", "hp", "Int", "Int", False),
        ("TrustedLink", "publish", "packet", "name", "target", "name", "String", "String", False),
    }, projections
    for row in runtime["projection_members"]:
        assert len(row["source_path_segments"]) == 1
        segment = row["source_path_segments"][0]
        assert segment["field_syntax_id"] > 0
        assert segment["field_name"] == row["source_path"]
        assert segment["field_type_name"] == row["source_leaf_type_name"]
    documents.append(document)

base = documents[1]
runtime = base["domain_runtime_assignments"]
decls = {row["name"]: row for row in base["decls"]}
poison_fields = {row["name"]: row for row in decls["Poisoned"]["fields"]}
player_fields = {row["name"]: row for row in decls["Player"]["fields"]}

mutations = {}

doc = copy.deepcopy(base)
doc.pop("domain_runtime_assignments")
mutations["missing-runtime-namespace"] = doc

doc = copy.deepcopy(base)
doc["domain_runtime_assignments"]["participant_roles"].append(
    copy.deepcopy(doc["domain_runtime_assignments"]["participant_roles"][0])
)
mutations["duplicate-participant-role"] = doc

doc = copy.deepcopy(base)
doc["domain_runtime_assignments"]["participant_roles"][0]["role"] = "unknown"
mutations["unknown-participant-role"] = doc

doc = copy.deepcopy(base)
row = doc["domain_runtime_assignments"]["participant_roles"][0]
row["field_name"] = "view"
row["field_syntax_id"] = poison_fields["view"]["source_syntax_id"]
row["field_type_name"] = poison_fields["view"]["type"]
mutations["foreign-valid-bearer-field"] = doc

doc = copy.deepcopy(base)
row = doc["domain_runtime_assignments"]["projection_members"][0]
row["source_path_segments"][0]["field_syntax_id"] = \
    player_fields["name"]["source_syntax_id"]
mutations["foreign-valid-source-segment"] = doc

doc = copy.deepcopy(base)
doc["domain_runtime_assignments"]["projection_members"][0]["operation"] = "publish"
mutations["operation-topology-mismatch"] = doc

doc = copy.deepcopy(base)
doc["domain_runtime_assignments"]["projection_members"].append(
    copy.deepcopy(doc["domain_runtime_assignments"]["projection_members"][0])
)
mutations["duplicate-projection-assignment"] = doc

doc = copy.deepcopy(base)
doc["domain_runtime_assignments"]["projection_members"].pop()
mutations["missing-projection-assignment"] = doc

doc = copy.deepcopy(base)
rows = doc["domain_runtime_assignments"]["projection_members"]
rows[0]["directive_syntax_id"] = rows[1]["directive_syntax_id"]
mutations["foreign-valid-directive-id"] = doc

doc = copy.deepcopy(base)
doc["domain_runtime_assignments"]["projection_members"][0]["program_syntax_id"] += 1
mutations["program-epoch-drift"] = doc

output = pathlib.Path(output_dir)
for name, document in mutations.items():
    (output / f"negative-{name}.mir.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )
PY

(cd "$ROOT_DIR" && "$DRIVER" --mir-json \
    "${SELF_MIR#$ROOT_DIR/}" >"$SELF_C" 2>"$BUILD_DIR/self.c.err") \
    || { cat "$SELF_C" "$BUILD_DIR/self.c.err" >&2; fail "valid self MIR was rejected"; }
(cd "$ROOT_DIR" && "$DRIVER" "$FIXTURE_REL" --emit-c-verified \
    >"$DIRECT_C" 2>"$BUILD_DIR/self.direct.err") \
    || { cat "$BUILD_DIR/self.direct.err" >&2; fail "direct source entrypoint failed"; }

cmp -s "$SELF_C" "$DIRECT_C" \
    || fail "direct source entrypoint bypassed the admitted MIR runtime plan"
for assignment in \
    '(*self).poison.bearer = (*self).player;' \
    '(*self).poison.view.hp = (*self).poison.bearer.hp;' \
    '(*self).trust.source = (*self).player;' \
    '(*self).trust.target = (*self).enemy;' \
    '(*self).trust.packet.name = (*self).trust.target.name;'; do
    grep -Fq -- "$assignment" "$SELF_C" \
        || fail "emitted C lost exact runtime assignment: $assignment"
done

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$SELF_C" -o "$SELF_EXE"
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$DIRECT_C" -o "$DIRECT_EXE"
"$SELF_EXE" | tr -d '\r' >"$BUILD_DIR/self.run"
"$DIRECT_EXE" | tr -d '\r' >"$BUILD_DIR/self.direct.run"
[[ "$(cat "$BUILD_DIR/self.run")" == $'7\ndst' ]] \
    || { cat "$BUILD_DIR/self.run" >&2; fail "self MIR runtime output drifted"; }
cmp -s "$BUILD_DIR/self.run" "$BUILD_DIR/self.direct.run" \
    || fail "direct source and explicit MIR runtime outputs differ"

for negative in "$BUILD_DIR"/negative-*.mir.json; do
    name="$(basename "$negative" .mir.json)"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json \
        "${negative#$ROOT_DIR/}" >"$negative.out" 2>"$negative.err"); then
        fail "$name was accepted"
    fi
    if grep -Eq '^#include|^typedef|BattleZone_Show' \
        "$negative.out" "$negative.err"; then
        fail "$name emitted a partial C artifact before failure"
    fi
    grep -Eq 'MIR (machine-layer|domain runtime assignment) facts are missing or invalid' \
        "$negative.out" "$negative.err" \
        || { cat "$negative.out" "$negative.err" >&2; fail "$name diagnostic drifted"; }
done

PGY_SELFHOST_PREBUILT_DRIVER="$DRIVER" PGY_BIN="$PGY" \
    "$ROOT_DIR/tests/self_hosted/parity/domain_runtime_explicit_map_execution_owner.sh"

echo "[self-host-parity:domain-runtime-assignment] PASS"
