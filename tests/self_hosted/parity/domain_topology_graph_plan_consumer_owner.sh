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
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
BUILD_DIR="${PGY_SELFHOST_DOMAIN_TOPOLOGY_PLAN_BUILD_DIR:-$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/domain_topology_graph_plan.XXXXXX")}"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_DOMAIN_TOPOLOGY_PLAN_DRIVER_BIN:-${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}}")"
FIXTURE="$ROOT_DIR/tests/cases/backend_compare/zone_layer_projection_runtime/main.pgy"

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
for term in 'MirDomainTopologyGraphPlanConsumptionFromAdmitted(admitted)' \
    'MirDomainTopologyGraphPlanAttachToC(' \
    'topology_receipt, admitted.domain_topology_plan,'; do
    grep -Fq -- "$term" "$GENERAL_OWNER" \
        || fail "general emitted-C production path lost its admitted plan receipt: $term"
done
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

pgy_require_runnable_binary_here "$LABEL-driver" "$DRIVER" \
    || fail "installed driver is not runnable; no gate-local rebuild fallback"

ZONE_MIR="$BUILD_DIR/zone.mir.json"
MUTATED="$BUILD_DIR/zone-plan-forged-edge.mir.json"
C_OUT="$BUILD_DIR/zone-plan.c"
DIGEST_PROBE_SRC="$ROOT_DIR/tests/self_hosted/parity/fixture/domain_topology_graph_plan_digest_probe.pgy"
DIGEST_PROBE="$BUILD_DIR/domain-topology-digest-probe.exe"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "${FIXTURE#"$ROOT_DIR/"}" -o \
    "${ZONE_MIR#"$ROOT_DIR/"}") \
    || fail "self-host fixture topology MIR production failed"
"$PYTHON_BIN" - "$ZONE_MIR" "$MUTATED" <<'PY'
import copy
import json
import sys

zone_path, mutated_path = sys.argv[1:]
with open(zone_path, encoding="utf-8") as stream:
    zone = json.load(stream)

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

(cd "$ROOT_DIR" && "$DRIVER" --mir-json \
    "${ZONE_MIR#"$ROOT_DIR/"}" -o "${C_OUT#"$ROOT_DIR/"}") \
    || fail "general C consumer rejected exact admitted plan"
[[ -s "$C_OUT" ]] || fail "general C consumer emitted no artifact"

for target in c llvm; do
    unsupported="$BUILD_DIR/unsupported-domain-topology.$target.artifact"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "${ZONE_MIR#"$ROOT_DIR/"}" -o "${unsupported#"$ROOT_DIR/"}") \
        >"$BUILD_DIR/unsupported.$target.out" \
        2>"$BUILD_DIR/unsupported.$target.err"; then
        fail "direct $target silently accepted unsupported multi-declaration topology"
    fi
    [[ ! -e "$unsupported" ]] || fail "direct $target published unsupported topology"
    grep -Eq 'direct MIR scalar program route rejected|direct MIR compile-time declaration erasure requires exactly one declaration' \
        "$BUILD_DIR/unsupported.$target.out" "$BUILD_DIR/unsupported.$target.err" \
        || fail "direct $target lost explicit unsupported multi-routine/declaration diagnostic"
done

"$PYTHON_BIN" - "$ZONE_MIR" "$C_OUT" <<'PY'
import json
import sys

mir_path, c_path = sys.argv[1:]
with open(mir_path, encoding="utf-8") as stream:
    doc = json.load(stream)
decls = {row["name"]: row for row in doc["decls"]}
fields = {row["name"]: row for row in decls["BattleZone"]["fields"]}
trust_id = fields["trust"]["source_syntax_id"]
player_id = fields["player"]["source_syntax_id"]
enemy_id = fields["enemy"]["source_syntax_id"]
text = open(c_path, encoding="utf-8").read()
assert "owner=BattleZone nodes=3 edges=2 depth=2 pass_limit=2 acyclic" in text
assert f"dep: trust <- player [to_id={trust_id} from_id={player_id}]" in text
assert f"dep: trust <- enemy [to_id={trust_id} from_id={enemy_id}]" in text
assert "_pgy_mir_domain_topology_plan_digest" in text
PY

(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$DIGEST_PROBE_SRC")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DIGEST_PROBE")" \
    >"$BUILD_DIR/digest-probe.compile.log" 2>&1) || {
    cat "$BUILD_DIR/digest-probe.compile.log" >&2 || true
    fail "digest mutation probe build failed"
}
(cd "$ROOT_DIR" && "$DIGEST_PROBE" \
    "${ZONE_MIR#"$ROOT_DIR/"}") \
    >"$BUILD_DIR/digest-probe.out" \
    || fail "digest mutation negative did not execute"
grep -Fq 'domain topology graph plan digest mutation rejected' \
    "$BUILD_DIR/digest-probe.out" \
    || fail "digest mutation rejection marker is missing"
grep -Fq 'absent domain topology graph plan residual arrays rejected' \
    "$BUILD_DIR/digest-probe.out" \
    || fail "absent-plan residual-array rejection marker is missing"

"$CC" -x c -std=c11 -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -pthread "$C_OUT" -o "$BUILD_DIR/zone-plan.c.exe" \
    >"$BUILD_DIR/c.compile.log" 2>&1 || {
    cat "$BUILD_DIR/c.compile.log" >&2 || true
    fail "C plan projection did not compile"
}
[[ "$(cd "$ROOT_DIR" && "$BUILD_DIR/zone-plan.c.exe" | tr -d '\r')" == $'7\ndst' ]] \
    || fail "C plan artifact runtime drifted"
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

echo "[$LABEL] self-produced BattleZone topology reached general C and executed; forged edge fails closed; direct C/LLVM multi-declaration route rejects explicitly"
