#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
ulimit -c 0
for lane in inline linked; do
    defs=()
    if [[ "$lane" == linked ]]; then defs+=(-DPGY_HOST_CLOCK_LINKED); fi
    for clock in mock real; do
        real=()
        if [[ "$clock" == real ]]; then real+=(-DPGY_HOST_CLOCK_REAL); fi
        "${CC:-cc}" -std=gnu11 -D_POSIX_C_SOURCE=200809L -O1 \
            -ffunction-sections -fdata-sections "${defs[@]}" "${real[@]}" \
            -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
            "$ROOT_DIR/tests/runtime_host_clock_probe.c" \
            -Wl,--gc-sections -pthread -lm -o "$work/$lane-$clock"
        env -u PGY_VIRTUAL_CLOCK -u PGY_CAP_GRANT "$work/$lane-$clock" ok
    done
    modes=(failure negative invalid-ns overflow deny invalid-unit)
    if [[ "$lane" == linked ]]; then
        env -u PGY_VIRTUAL_CLOCK -u PGY_CAP_GRANT "$work/$lane-mock" virtual
        modes+=(ns-overflow real-advance)
    fi
    for mode in "${modes[@]}"; do
        case "$mode" in
            overflow|ns-overflow) expected=arithmetic-overflow ;;
            deny) expected=capability-denied ;;
            real-advance) expected=invalid-lifecycle-state ;;
            *) expected=internal-invariant ;;
        esac
        set +e
        env -u PGY_VIRTUAL_CLOCK -u PGY_CAP_GRANT "$work/$lane-mock" "$mode" >"$work/$mode.out" 2>&1
        rc=$?
        set -e
        if [[ "$rc" != 134 ]] || ! grep -Fq "class=$expected" "$work/$mode.out"; then
            cat "$work/$mode.out" >&2
            echo "[host-clock] $lane $mode wrong refusal: rc=$rc" >&2
            exit 1
        fi
    done
    echo "[host-clock] $lane: real + injected Long/clock/failure/capability probes PASS"
done
