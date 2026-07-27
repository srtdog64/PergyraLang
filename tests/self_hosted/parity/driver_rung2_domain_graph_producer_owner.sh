#!/usr/bin/env bash
# Owns the first self-host DIR->MIR executable producer rung: the exact
# effect/zone/role/authority census for a domain document with no topology
# rows.  Non-empty directives remain fail-closed until their typed row owner
# and runtime plan land.

domain_graph_owner="$ROOT_DIR/src/self_hosted/dir/domain_graph_fact_owner.pgy"
kind_owner="$ROOT_DIR/src/self_hosted/hir/ast_node_kind_owner.pgy"

for term in 'struct SelfDirDomainGraphFacts' \
    'SelfDirDomainGraphFactsFromArtifact' \
    'SelfDirDomainGraphAnchor' \
    'SelfDirDomainAuthorityEdges'; do
    grep -Fq -- "$term" "$domain_graph_owner" || {
        echo "[self-host-parity:driver-rung2] missing self DIR owner term: $term" >&2
        return 1 2>/dev/null || exit 1
    }
done
for term in TypedAstKindDomainRefreshTag TypedAstKindDomainPublishTag \
    TypedAstKindDomainProjectionBindTag TypedAstKindDomainMaintainTag \
    TypedAstKindDomainLinkTag TypedAstKindDomainApplyTag \
    TypedAstKindDomainDetachTag TypedAstKindDomainUnlinkTag \
    TypedAstKindDomainStateTag; do
    grep -Fq -- "$term" "$kind_owner" || {
        echo "[self-host-parity:driver-rung2] domain directive kind identity was lost: $term" >&2
        return 1 2>/dev/null || exit 1
    }
done
if grep -Eq 'TypedAstArenaProvenanceText|native_oracle|domain_graph_id = "?1"?' \
    "$domain_graph_owner"; then
    echo "[self-host-parity:driver-rung2] self DIR owner reopened text/oracle/constant fallback" >&2
    return 1 2>/dev/null || exit 1
fi

pgy_selfhost_verify_driver_rung2_domain_graph_producer() {
    local backend="$1" base="$2" native_mir_json="$3"
    local self_mir_json="$4" driver_bin="$5"
    [[ "$base" == "function_clause_order_minimal" ]] || return 0

    local topology='"domain_topology":{"domain_graph_id":14937235029576152731,"rows":[]}'
    grep -Fq "$topology" "$native_mir_json" || {
        echo "[self-host-parity:driver-rung2] native 9-node/16-edge DIR anchor drifted" >&2
        return 1
    }
    grep -Fq "$topology" "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend self DIR census did not reproduce the native anchor" >&2
        return 1
    }

    local rejected_source="$ROOT_DIR/tests/self_hosted/parity/fixture/domain_topology_nonempty_rejected.pgy"
    local rejected_out="$BUILD_DIR/${base}_${backend}.nonempty-topology.out"
    local rejected_err="$BUILD_DIR/${base}_${backend}.nonempty-topology.err"
    if (cd "$ROOT_DIR" && "$driver_bin" --emit-mir-json-verified \
        "tests/self_hosted/parity/fixture/domain_topology_nonempty_rejected.pgy" \
        >"$rejected_out" 2>"$rejected_err"); then
        echo "[self-host-parity:driver-rung2] $backend non-empty domain directive was downgraded to empty topology" >&2
        return 1
    fi
    [[ -f "$rejected_source" ]] || {
        echo "[self-host-parity:driver-rung2] missing non-empty topology falsifier" >&2
        return 1
    }
    grep -Fq 'self-host DIR topology directive or authority shape is unsupported' \
        "$rejected_out" "$rejected_err" || {
        echo "[self-host-parity:driver-rung2] $backend non-empty topology failed outside its DIR owner" >&2
        return 1
    }
    if grep -Fq '"schema":"pgy.mir.v1"' "$rejected_out"; then
        echo "[self-host-parity:driver-rung2] $backend rejected directive still emitted MIR" >&2
        return 1
    fi
}
