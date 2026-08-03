#!/usr/bin/env bash
# Owns the first self-host DIR->MIR executable producer rung: the exact
# effect/zone/role/authority census for a domain document with no topology
# rows. Non-empty topology has its own typed producer owner/gate.

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
    local empty_topology_c="$BUILD_DIR/empty_topology_${backend}.c"
    local empty_topology_err="$BUILD_DIR/empty_topology_${backend}.err"
    local self_mir_rel="${self_mir_json#$ROOT_DIR/}"
    if ! (cd "$ROOT_DIR" && "$driver_bin" --mir-json "$self_mir_rel" \
            >"$empty_topology_c" 2>"$empty_topology_err"); then
        cat "$empty_topology_c" "$empty_topology_err" >&2
        echo "[self-host-parity:driver-rung2] $backend empty topology MIR was rejected" >&2
        return 1
    fi
    [[ -s "$empty_topology_c" ]] || {
        echo "[self-host-parity:driver-rung2] $backend empty topology MIR emitted no C" >&2
        return 1
    }

    local optional_source="tests/self_hosted/fixtures/driver_execution_action_abi_probe.pgy"
    local optional_self="$BUILD_DIR/optional_authority_${backend}.self.mir.json"
    local optional_native="$BUILD_DIR/optional_authority_${backend}.native.mir.json"
    if ! (cd "$ROOT_DIR" && "$driver_bin" --emit-mir-json-verified \
            "$optional_source" >"$optional_self" \
            2>"$optional_self.err"); then
        cat "$optional_self" "$optional_self.err" >&2
        echo "[self-host-parity:driver-rung2] $backend bare authority was rejected" >&2
        return 1
    fi
    if ! (cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
            "$optional_source" \
            >"$optional_native" 2>"$optional_native.err"); then
        cat "$optional_native" "$optional_native.err" >&2
        echo "[self-host-parity:driver-rung2] native bare authority oracle failed" >&2
        return 1
    fi
    local optional_topology='"domain_topology":{"domain_graph_id":14937234930791904899,"rows":[]}'
    for optional_mir in "$optional_native" "$optional_self"; do
        grep -Fq "$optional_topology" "$optional_mir" || {
            echo "[self-host-parity:driver-rung2] $backend bare authority 10-node/9-edge anchor drifted" >&2
            return 1
        }
    done

}
