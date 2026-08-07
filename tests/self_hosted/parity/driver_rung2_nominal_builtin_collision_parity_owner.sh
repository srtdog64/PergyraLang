#!/usr/bin/env bash
# Owns user nominal identity when its name collides with a generic builtin.

pgy_selfhost_nominal_collision_reject_missing_target() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local kind="$5" target="$6" missing_target
    missing_target="$BUILD_DIR/${base}_${backend}.${target}.missing-target.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$missing_target" \
        "\"call_target_kind\":\"$kind\",\"call_target_name\":\"$target\"" \
        '"call_target_kind":"none","call_target_name":""'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_target")" \
        >"$missing_target.out" 2>"$missing_target.err"); then
        echo "[self-host-parity:driver-rung2] $backend nominal collision target accepted: $target" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$missing_target.err" "$missing_target.out" || {
        echo "[self-host-parity:driver-rung2] $backend nominal collision target diagnostic drifted: $target" >&2
        cat "$missing_target.out" "$missing_target.err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_nominal_builtin_collision() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact row kind target expected actual missing_return
    [[ "$base" == "class_user_box" ]] || return 0

    for fact in \
        '"kind":"class","nominal_kind":"class","name":"Box"' \
        '"name":"WithWeight","kind":"method"' \
        '"name":"Heavy","kind":"method"' \
        '"name":"New","kind":"function"' \
        '"name":"self","type":null,"carriage":"value"' \
        '"owner":"Box"' \
        '"kind":"member_access","text":"New(4).WithWeight(0).weight"' \
        '"kind":"member_access","text":"New(5).WithWeight(75).label"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend nominal collision fact drifted: $fact" >&2
            exit 1
        }
    done
    for row in direct:Box:2 direct:New:4 member:Box_WithWeight:4 member:Box_Heavy:2; do
        IFS=: read -r kind target expected <<<"$row"
        actual="$(grep -Fo "\"call_target_kind\":\"$kind\",\"call_target_name\":\"$target\"" \
            "$self_mir_json" | wc -l | tr -d ' ')"
        [[ "$actual" -eq "$expected" ]] || {
            echo "[self-host-parity:driver-rung2] $backend nominal collision target count drifted: $target=$actual/$expected" >&2
            exit 1
        }
        pgy_selfhost_nominal_collision_reject_missing_target \
            "$backend" "$base" "$self_mir_json" "$driver_bin" "$kind" "$target"
    done

    missing_return="$BUILD_DIR/${base}_${backend}.missing-nominal-return.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$missing_return" \
        '"name":"WithWeight","kind":"method"' \
        '"name":"WithWeight","kind":"method_removed"'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_return")" \
        >"$missing_return.out" 2>"$missing_return.err"); then
        echo "[self-host-parity:driver-rung2] $backend nominal method identity removal was accepted" >&2
        exit 1
    fi
    # The machine-layer admission fail-closes the mutated document before
    # the class-method routine link can be judged.
    grep -Fq "MIR machine-layer facts are missing or invalid" \
        "$missing_return.err" "$missing_return.out" || {
        echo "[self-host-parity:driver-rung2] $backend nominal method identity diagnostic drifted" >&2
        cat "$missing_return.out" "$missing_return.err" >&2
        exit 1
    }
}
