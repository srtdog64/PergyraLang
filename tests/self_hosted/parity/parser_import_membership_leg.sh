#!/usr/bin/env bash
# Extra import-composition falsifiers, using the parser already built by parity.
run_parser_import_membership_leg() {
    local backend="$1" tool_bin="$2"
    local fixtures="tests/self_hosted/parity/fixture/parser_import_membership"
    local prefix="$PERGYRA_TOOL_BUILD_DIR/import_membership_${backend}"
    local input expected_code expected_span rc

    (cd "$ROOT_DIR" && timeout 30 "$tool_bin" "$fixtures/main.pgy" \
        >"$prefix.main.raw" 2>"$prefix.main.err")
    tr -d '\r' <"$prefix.main.raw" >"$prefix.main.out"
    sed -n -E 's/^  (\[export\] )?Function: (.*)$/\2/p' \
        "$prefix.main.out" >"$prefix.names"
    printf '%s\n' SharedImported LeftImported RightImported Main >"$prefix.expected_names"
    if ! cmp -s "$prefix.expected_names" "$prefix.names"; then
        echo "[self-host-parity:parser] backend=$backend shared-import count/order FAIL" >&2
        cat "$prefix.main.out" >&2
        return 1
    fi

    for input in cycle_a missing; do
        expected_code=import_cycle
        expected_span=cycle_b.pgy:1:1
        if [[ "$input" == missing ]]; then
            expected_code=import_source_missing
            expected_span=missing.pgy:3:1
        fi
        rc=0
        (cd "$ROOT_DIR" && timeout 30 "$tool_bin" "$fixtures/$input.pgy" \
            >"$prefix.$input.raw" 2>"$prefix.$input.err") || rc=$?
        tr -d '\r' <"$prefix.$input.raw" >"$prefix.$input.out"
        if [[ "$rc" -ne 1 ]] ||
            ! grep -Fxq 'Status: error' "$prefix.$input.out" ||
            ! grep -Fxq "Code: $expected_code" "$prefix.$input.out" ||
            ! grep -Fxq "Span: $expected_span" "$prefix.$input.out"; then
            echo "[self-host-parity:parser] backend=$backend $input refusal/position FAIL (rc=$rc)" >&2
            cat "$prefix.$input.out" "$prefix.$input.err" >&2
            return 1
        fi
    done
    echo "[self-host-parity:parser] backend=$backend normalized diamond, cycle-before-seen and missing-import position PASS"
}
