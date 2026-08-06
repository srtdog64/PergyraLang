#!/usr/bin/env bash
# Owns admitted callable receiver carriage consumption in general self C.

receiver_fact_owner="$ROOT_DIR/src/self_hosted/codegen/input/callable_receiver_codegen_view_owner.pgy"
receiver_bridge_owner="$ROOT_DIR/src/self_hosted/compiler/codegen_callable_receiver_bridge_owner.pgy"
receiver_function_owner="$ROOT_DIR/src/self_hosted/codegen/emission/function_emit.pgy"
# Env construction moved up to the program emitter; function_emit stays the
# per-function consumer that must not re-read role targets.
receiver_env_owner="$ROOT_DIR/src/self_hosted/codegen/emission/program_emit.pgy"
receiver_call_owner="$ROOT_DIR/src/self_hosted/codegen/emission/member_call_receiver_carriage_owner.pgy"
receiver_semantic_call_owner="$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy"

for term in 'struct CodegenCallableReceiverFacts' \
    'role_target_types: Array<String>' \
    'role_target_carriages: Array<String>' \
    'CodegenRoleReceiverTargetCarriageOrDie' \
    'CodegenCallableReceiverFactsFromAdmittedRowsOrDie' \
    'routine_source_syntax_ids[r] == facts.source_syntax_ids[i]' \
    'routine_receiver_carriages[found] != facts.carriages[i]' \
    'CodegenCallableReceiverEnvRows'; do
    grep -Fq -- "$term" "$receiver_fact_owner" || {
        echo "[self-host-parity:driver-rung2] callable receiver owner drifted: $term" >&2
        return 1 2>/dev/null || exit 1
    }
done
if grep -Fq 'CodegenSemanticRoleReceiverType(' "$receiver_function_owner"; then
    echo "[self-host-parity:driver-rung2] function emission re-read role targets" >&2
    return 1 2>/dev/null || exit 1
fi
grep -Fq 'admitted.routines.receiver_carriages' "$receiver_bridge_owner" || {
    echo "[self-host-parity:driver-rung2] admitted receiver bridge drifted" >&2
    return 1 2>/dev/null || exit 1
}
grep -Fq 'BuildFunctionEnv(' "$receiver_env_owner" || {
    echo "[self-host-parity:driver-rung2] function env lost receiver carriage" >&2
    return 1 2>/dev/null || exit 1
}
grep -Fq 'mutable-identity member call requires a stable receiver address' \
    "$receiver_call_owner" || {
    echo "[self-host-parity:driver-rung2] member call lost stable receiver address" >&2
    return 1 2>/dev/null || exit 1
}
for term in 'SemanticExpressionGraphPlaceKind(' \
    'SemanticExpressionPlaceKindAddressable('; do
    grep -Fq -- "$term" "$receiver_semantic_call_owner" || {
        echo "[self-host-parity:driver-rung2] semantic receiver place fact drifted: $term" >&2
        return 1 2>/dev/null || exit 1
    }
done
if grep -Fq 'SemanticExpressionGraphNodeKind(graph, member.receiver_node)' \
    "$receiver_semantic_call_owner"; then
    echo "[self-host-parity:driver-rung2] receiver addressability was re-inferred from syntax" >&2
    return 1 2>/dev/null || exit 1
fi

pgy_selfhost_verify_driver_rung2_callable_receiver_carriage() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    [[ "$base" == "zone_layer_projection_runtime" ]] || return 0

    local emitted="$BUILD_DIR/${base}_${backend}.receiver.c"
    local emitted_err="$BUILD_DIR/${base}_${backend}.receiver.err"
    local mutated="$BUILD_DIR/${base}_${backend}.receiver-mutated.mir.json"
    local mutated_out="$BUILD_DIR/${base}_${backend}.receiver-mutated.out"
    local mutated_err="$BUILD_DIR/${base}_${backend}.receiver-mutated.err"
    local temporary_source="tests/self_hosted/fixtures/mutable_receiver_temporary_member_reject.pgy"
    local temporary_out="$BUILD_DIR/${base}_${backend}.receiver-temporary.out"
    local temporary_err="$BUILD_DIR/${base}_${backend}.receiver-temporary.err"
    local role_probe_source="$ROOT_DIR/tests/self_hosted/fixtures/codegen_role_receiver_carriage_owner.pgy"
    local role_probe_bin="$BUILD_DIR/callable_receiver_role_probe.exe"
    local role_probe_out="$BUILD_DIR/${base}_${backend}.receiver-role-probe.out"
    local direct_codegen_source="$ROOT_DIR/src/self_hosted/codegen/main.pgy"
    local direct_codegen_bin="$BUILD_DIR/callable_receiver_codegen.exe"
    local role_source="tests/self_hosted/fixtures/mutable_role_receiver_runtime.pgy"
    local role_c="$BUILD_DIR/${base}_${backend}.receiver-role.c"
    local role_c_err="$BUILD_DIR/${base}_${backend}.receiver-role.err"
    local role_cc_err="$BUILD_DIR/${base}_${backend}.receiver-role.cc.err"
    local python_bin="${PYTHON_BIN:-python3}"

    "$python_bin" - "$self_mir_json" "$mutated" <<'PY'
import json, pathlib, sys
source = pathlib.Path(sys.argv[1])
mutated = pathlib.Path(sys.argv[2])
doc = json.loads(source.read_text(encoding="utf-8"))
show = [r for r in doc["routines"] if r.get("owner") == "BattleZone" and r["name"] == "Show"]
main = [r for r in doc["routines"] if not r.get("owner") and r["name"] == "Main"]
assert len(show) == 1 and show[0]["receiver_carriage"] == "mutable-identity", show
assert len(main) == 1 and main[0]["receiver_carriage"] == "none", main
show[0]["receiver_carriage"] = "value"
mutated.write_text(json.dumps(doc, separators=(",", ":")), encoding="utf-8")
PY

    rm -f "$emitted"
    (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$self_mir_json")" \
        >"$emitted" 2>"$emitted_err") || {
        echo "[self-host-parity:driver-rung2] $backend receiver C emission failed" >&2
        cat "$emitted_err" >&2
        return 1
    }
    grep -Fq 'BattleZone_Show(BattleZone *self)' "$emitted" || {
        echo "[self-host-parity:driver-rung2] $backend BattleZone_Show is not pointer-self" >&2
        return 1
    }
    grep -Fq 'BattleZone_Show(&(battle))' "$emitted" || {
        echo "[self-host-parity:driver-rung2] $backend BattleZone_Show call lacks stable address" >&2
        return 1
    }

    rm -f "$mutated_out"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        >"$mutated_out" 2>"$mutated_err"); then
        echo "[self-host-parity:driver-rung2] $backend accepted mutated receiver carriage" >&2
        return 1
    fi
    if grep -Fq '#include <stdio.h>' "$mutated_out" ||
        grep -Fq 'BattleZone_Show(' "$mutated_out"; then
        echo "[self-host-parity:driver-rung2] $backend emitted C before receiver rejection" >&2
        return 1
    fi

    rm -f "$temporary_out"
    if (cd "$ROOT_DIR" && "$driver_bin" "$temporary_source" \
        --emit-c-verified >"$temporary_out" 2>"$temporary_err"); then
        echo "[self-host-parity:driver-rung2] $backend accepted a temporary mutable receiver" >&2
        return 1
    fi
    grep -Fq 'mutable-identity member call requires a stable receiver address' \
        "$temporary_out" "$temporary_err" || {
        echo "[self-host-parity:driver-rung2] $backend lost temporary receiver diagnostic" >&2
        return 1
    }
    if grep -Fq '#include <stdio.h>' "$temporary_out" ||
        grep -Fq 'Cell_Touch(' "$temporary_out"; then
        echo "[self-host-parity:driver-rung2] $backend emitted C for a temporary receiver" >&2
        return 1
    fi

    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$role_probe_source")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$role_probe_bin")" \
        >"$role_probe_out.compile" 2>&1) || {
        echo "[self-host-parity:driver-rung2] role receiver owner probe build failed" >&2
        cat "$role_probe_out.compile" >&2
        return 1
    }
    "$role_probe_bin" >"$role_probe_out" 2>&1 || {
        echo "[self-host-parity:driver-rung2] stable erased role receiver failed" >&2
        cat "$role_probe_out" >&2
        return 1
    }
    grep -Fxq '&(player)' "$role_probe_out" || {
        echo "[self-host-parity:driver-rung2] erased role receiver lost stable address" >&2
        return 1
    }
    "$role_probe_bin" --scalar-value >"$role_probe_out" 2>&1 || {
        echo "[self-host-parity:driver-rung2] scalar role receiver binding failed" >&2
        cat "$role_probe_out" >&2
        return 1
    }
    grep -Fxq 'scalar-value' "$role_probe_out" || {
        echo "[self-host-parity:driver-rung2] scalar role target was not value-bound" >&2
        return 1
    }
    local role_mode=""
    for role_mode in --temporary --value-role --missing-kind; do
        if "$role_probe_bin" "$role_mode" >"$role_probe_out" 2>&1; then
            echo "[self-host-parity:driver-rung2] role receiver negative accepted: $role_mode" >&2
            return 1
        fi
    done

    # Production role calls remain blocked by the separately owned canonical
    # role identity drift (source id 13 versus reconstructed id 6).  This
    # local closure proves the ordinary self-codegen definition ABI instead.
    if [[ ! -x "$direct_codegen_bin" ]]; then
        (cd "$ROOT_DIR" && "$PGY" \
            "$(pgy_path_for_compiler "$PGY" "$direct_codegen_source")" \
            --backend=c -o "$(pgy_path_for_compiler "$PGY" "$direct_codegen_bin")" \
            >"$role_c_err.build" 2>&1) || {
            echo "[self-host-parity:driver-rung2] direct codegen build failed" >&2
            cat "$role_c_err.build" >&2
            return 1
        }
    fi
    (cd "$ROOT_DIR" && "$direct_codegen_bin" --source "$role_source" \
        >"$role_c" 2>"$role_c_err") || {
        echo "[self-host-parity:driver-rung2] role receiver C definition failed" >&2
        cat "$role_c_err" >&2
        return 1
    }
    grep -Fq 'PlayerDamageable_CurrentHealth(void *_pgy_raw_self)' "$role_c" || {
        echo "[self-host-parity:driver-rung2] erased role signature drifted" >&2
        return 1
    }
    grep -Fq 'Player *self = (Player *)_pgy_raw_self;' "$role_c" || {
        echo "[self-host-parity:driver-rung2] mutable role target was copied" >&2
        return 1
    }
    grep -Fq 'Player self = ' "$role_c" && {
        echo "[self-host-parity:driver-rung2] mutable role target value fallback returned" >&2
        return 1
    }
    "$CC" -std=c11 -Wall -Wextra -fsyntax-only \
        -I"$ROOT_DIR/src/runtime" "$role_c" \
        >"$role_cc_err" 2>&1 || {
        echo "[self-host-parity:driver-rung2] generated role receiver C syntax failed" >&2
        cat "$role_cc_err" >&2
        return 1
    }
}
