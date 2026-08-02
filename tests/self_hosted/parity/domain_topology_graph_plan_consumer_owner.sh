#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-domain-topology-graph-plan"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
PYTHON_BIN="${PYTHON:-}"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
BUILD_DIR="${PGY_SELFHOST_DOMAIN_TOPOLOGY_PLAN_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/domain_topology_graph_plan}"
DRIVER="${PGY_SELFHOST_DOMAIN_TOPOLOGY_PLAN_DRIVER_BIN:-$BUILD_DIR/driver.exe}"
FIXTURE="$ROOT_DIR/tests/cases/backend_compare/zone_layer_projection_runtime/main.pgy"
HELLO="$ROOT_DIR/examples/hello.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "python3/python is required"
    fi
fi

pgy_require_runnable_binary_here "$LABEL" "$PGY" || fail "PGY_BIN is not runnable"
command -v "$CC" >/dev/null 2>&1 || fail "C compiler is required: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "clang is required for LLVM evidence"
mkdir -p "$BUILD_DIR"

PLAN_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/domain_topology_graph_plan_owner.pgy"
BUILD_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/domain_topology_graph_build_owner.pgy"
SCHEDULE_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/domain_topology_graph_schedule_owner.pgy"
CONSUMER_OWNER="$ROOT_DIR/src/self_hosted/compiler/domain_topology_graph_plan_consumer_owner.pgy"
MACHINE_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/machine_layer_fact_owner.pgy"
DIRECT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy"
GENERAL_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"

for file in "$PLAN_OWNER" "$BUILD_OWNER" "$SCHEDULE_OWNER" \
    "$CONSUMER_OWNER" "$MACHINE_OWNER" "$DIRECT_OWNER" "$GENERAL_OWNER"; do
    [[ -f "$file" ]] || fail "missing owner: ${file#"$ROOT_DIR/"}"
done
for term in 'struct MirDomainTopologyGraphPlan' \
    'MirDomainTopologyGraphPlanDigest' \
    'MirDomainTopologyGraphPlanFromFacts' \
    'MirDomainTopologyGraphPlanReady'; do
    grep -Fq -- "$term" "$PLAN_OWNER" || fail "plan owner missing: $term"
done
for term in 'MirDomainTopologyOwnerGraphAddEdge' \
    'MirDomainTopologyOwnerGraphSchedule'; do
    grep -Fq -- "$term" "$SCHEDULE_OWNER" || fail "schedule owner missing: $term"
done
grep -Fq -- 'kind == "apply-effect"' "$BUILD_OWNER" \
    || fail "owner graph builder does not explicitly admit apply-effect"
grep -Fq -- 'edge_ok = true' "$BUILD_OWNER" \
    || fail "apply-effect no-edge lifecycle admission is missing"
for term in 'MirDomainTopologyGraphPlanConsumptionFromAdmitted' \
    'MirDomainTopologyGraphPlanConsumptionMutationRejected' \
    'MirDomainTopologyGraphPlanAttachToC' \
    'MirDomainTopologyGraphPlanAttachToLlvm'; do
    grep -Fq -- "$term" "$CONSUMER_OWNER" || fail "consumer owner missing: $term"
done
grep -Fq -- 'domain_topology_plan: MirDomainTopologyGraphPlan;' \
    "$MACHINE_OWNER" || fail "admission does not carry the one graph plan"
grep -Fq -- 'MirDomainTopologyGraphPlanAttachAdmittedToLlvm' "$DIRECT_OWNER" \
    || fail "direct C/LLVM production path does not consume the plan"
grep -Fq -- 'MirDomainTopologyGraphPlanAttachAdmittedToC' "$GENERAL_OWNER" \
    || fail "general emitted-C production path does not consume the plan"
if grep -Fq -- 'MirDomainTopologyGraphPlanReady(' \
    "$CONSUMER_OWNER" "$DIRECT_OWNER" "$GENERAL_OWNER"; then
    fail "production consumer repeated full graph-plan validation after admission"
fi
[[ "$(grep -Fc -- 'MirDomainTopologyGraphPlanReady(domain_topology_plan)' "$MACHINE_OWNER")" == 1 ]] \
    || fail "machine admission must be the one full graph-plan validation boundary"
[[ "$(grep -R --include='*.pgy' -Fh -- 'MirDomainTopologyGraphPlanReady(' \
    "$ROOT_DIR/src/self_hosted" | wc -l)" == 2 ]] \
    || fail "full graph-plan validation escaped its definition and one admission call"
if grep -Eiq -- 'AstTree|source_path|ReadFile\(|count_floor|from_zone' \
    "$PLAN_OWNER" "$BUILD_OWNER" "$SCHEDULE_OWNER" "$CONSUMER_OWNER"; then
    fail "graph plan reopened AST/source/backend or count-floor authority"
fi
for file in "$PLAN_OWNER" "$BUILD_OWNER" "$SCHEDULE_OWNER" "$CONSUMER_OWNER"; do
    lines="$(wc -l <"$file")"
    (( lines <= 600 )) || fail "owner exceeds 600-line component limit: ${file#"$ROOT_DIR/"} ($lines)"
done

if [[ ! -x "$DRIVER" || "${PGY_SELFHOST_DOMAIN_TOPOLOGY_PLAN_REBUILD:-1}" == "1" ]]; then
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) || {
        cat "$BUILD_DIR/driver.compile.log" >&2 || true
        fail "bootstrap driver build failed"
    }
fi
pgy_require_runnable_binary_here "$LABEL-driver" "$DRIVER" \
    || fail "built driver is not runnable"

ZONE_MIR="$BUILD_DIR/zone.mir.json"
HELLO_MIR="$BUILD_DIR/hello.self.mir.json"
COMBINED="$BUILD_DIR/zone-plan-hello.self.mir.json"
MUTATED="$BUILD_DIR/zone-plan-forged-edge.mir.json"
C_OUT="$BUILD_DIR/zone-plan.c"
LLVM_OUT="$BUILD_DIR/zone-plan.ll"
DIGEST_PROBE_SRC="$ROOT_DIR/tests/self_hosted/parity/fixture/domain_topology_graph_plan_digest_probe.pgy"
DIGEST_PROBE="$BUILD_DIR/domain-topology-digest-probe.exe"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "${FIXTURE#"$ROOT_DIR/"}" -o \
    "${ZONE_MIR#"$ROOT_DIR/"}") \
    || fail "self-host fixture topology MIR production failed"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "${HELLO#"$ROOT_DIR/"}" -o \
    "${HELLO_MIR#"$ROOT_DIR/"}") \
    || fail "self-produced supported routine MIR failed"

"$PYTHON_BIN" - "$ZONE_MIR" "$HELLO_MIR" "$COMBINED" "$MUTATED" <<'PY'
import copy
import json
import sys

zone_path, hello_path, combined_path, mutated_path = sys.argv[1:]
with open(zone_path, encoding="utf-8") as stream:
    zone = json.load(stream)
with open(hello_path, encoding="utf-8") as stream:
    hello = json.load(stream)
zone["routines"] = hello["routines"]
with open(combined_path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(zone, stream, separators=(",", ":"))
    stream.write("\n")

declarations = {row["name"]: row for row in zone["decls"]}
fields = {row["name"]: row for row in declarations["BattleZone"]["fields"]}
player_id = fields["player"]["source_syntax_id"]
enemy_id = fields["enemy"]["source_syntax_id"]
assert player_id != enemy_id
bad = copy.deepcopy(zone)
link = next(
    row for row in bad["domain_topology"]["rows"]
    if row["owner_name"] == "BattleZone" and row["kind"] == "link-relation"
)
assert link["left_slot_name"] == "player"
assert link["left_slot_source_syntax_id"] == player_id
link["left_slot_source_syntax_id"] = enemy_id
with open(mutated_path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(bad, stream, separators=(",", ":"))
    stream.write("\n")
PY

for target in c llvm; do
    output="$C_OUT"
    [[ "$target" == c ]] || output="$LLVM_OUT"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "${COMBINED#"$ROOT_DIR/"}" -o \
        "${output#"$ROOT_DIR/"}") \
        || fail "$target production consumer rejected exact admitted plan"
    [[ -s "$output" ]] || fail "$target production consumer emitted no artifact"
done

"$PYTHON_BIN" - "$COMBINED" "$C_OUT" "$LLVM_OUT" <<'PY'
import json
import sys

mir_path, c_path, llvm_path = sys.argv[1:]
with open(mir_path, encoding="utf-8") as stream:
    doc = json.load(stream)
decls = {row["name"]: row for row in doc["decls"]}
fields = {row["name"]: row for row in decls["BattleZone"]["fields"]}
trust_id = fields["trust"]["source_syntax_id"]
player_id = fields["player"]["source_syntax_id"]
enemy_id = fields["enemy"]["source_syntax_id"]
for path in (c_path, llvm_path):
    text = open(path, encoding="utf-8").read()
    assert "owner=BattleZone nodes=3 edges=2 depth=2 pass_limit=2 acyclic" in text
    assert f"dep: trust <- player [to_id={trust_id} from_id={player_id}]" in text
    assert f"dep: trust <- enemy [to_id={trust_id} from_id={enemy_id}]" in text
assert "_pgy_mir_domain_topology_plan_digest" in open(c_path, encoding="utf-8").read()
assert "_pgy.mir.domain.topology.plan.digest" in open(llvm_path, encoding="utf-8").read()
PY

(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$DIGEST_PROBE_SRC")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DIGEST_PROBE")" \
    >"$BUILD_DIR/digest-probe.compile.log" 2>&1) || {
    cat "$BUILD_DIR/digest-probe.compile.log" >&2 || true
    fail "digest mutation probe build failed"
}
(cd "$ROOT_DIR" && "$DIGEST_PROBE" \
    "${COMBINED#"$ROOT_DIR/"}") \
    >"$BUILD_DIR/digest-probe.out" \
    || fail "digest mutation negative did not execute"
grep -Fq 'domain topology graph plan digest mutation rejected' \
    "$BUILD_DIR/digest-probe.out" \
    || fail "digest mutation rejection marker is missing"
grep -Fq 'absent domain topology graph plan residual arrays rejected' \
    "$BUILD_DIR/digest-probe.out" \
    || fail "absent-plan residual-array rejection marker is missing"

"$CC" -x c -std=c11 "$C_OUT" -o "$BUILD_DIR/zone-plan.c.exe" \
    >"$BUILD_DIR/c.compile.log" 2>&1 || {
    cat "$BUILD_DIR/c.compile.log" >&2 || true
    fail "C plan projection did not compile"
}
"$CLANG" -x ir "$LLVM_OUT" -o "$BUILD_DIR/zone-plan.llvm.exe" \
    >"$BUILD_DIR/llvm.compile.log" 2>&1 || {
    cat "$BUILD_DIR/llvm.compile.log" >&2 || true
    fail "LLVM plan projection did not compile"
}
[[ "$(cd "$ROOT_DIR" && "$BUILD_DIR/zone-plan.c.exe" | tr -d '\r')" == 'Hello, Pergyra!' ]] \
    || fail "C plan artifact runtime drifted"
[[ "$(cd "$ROOT_DIR" && "$BUILD_DIR/zone-plan.llvm.exe" | tr -d '\r')" == 'Hello, Pergyra!' ]] \
    || fail "LLVM plan artifact runtime drifted"
bad_general="$BUILD_DIR/forged-edge.general-c.artifact"
rm -f "$bad_general"
if (cd "$ROOT_DIR" && "$DRIVER" --mir-json \
    "${MUTATED#"$ROOT_DIR/"}" -o \
    "${bad_general#"$ROOT_DIR/"}") \
    >"$BUILD_DIR/forged-edge.general-c.out" \
    2>"$BUILD_DIR/forged-edge.general-c.err"; then
    fail "general emitted-C path admitted player-name/enemy-ID edge"
fi
[[ ! -e "$bad_general" ]] || fail "general C emitted before rejecting forged edge"
grep -Fq 'MIR domain topology facts are missing or invalid' \
    "$BUILD_DIR/forged-edge.general-c.out" \
    "$BUILD_DIR/forged-edge.general-c.err" \
    || fail "general C forged edge did not fail at topology admission"

for target in c llvm; do
    bad_out="$BUILD_DIR/forged-edge.$target.artifact"
    rm -f "$bad_out"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "${MUTATED#"$ROOT_DIR/"}" -o \
        "${bad_out#"$ROOT_DIR/"}") \
        >"$BUILD_DIR/forged-edge.$target.out" \
        2>"$BUILD_DIR/forged-edge.$target.err"; then
        fail "$target production path admitted player-name/enemy-ID edge"
    fi
    [[ ! -e "$bad_out" ]] || fail "$target emitted before rejecting forged edge"
    grep -Fq 'MIR domain topology facts are missing or invalid' \
        "$BUILD_DIR/forged-edge.$target.out" \
        "$BUILD_DIR/forged-edge.$target.err" \
        || fail "$target forged edge did not fail at topology admission"
done

echo "[$LABEL] self-produced topology reached one target-neutral C/LLVM plan: BattleZone nodes=3 edges=2 depth=2 pass_limit=2; exact edge and gate-only digest mutations fail closed; layer runtime materialization remains the next rung"
