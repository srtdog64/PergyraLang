#!/usr/bin/env bash
# Native C used to emit C that gcc rejected for three shapes the default C
# route compiles, and to accept a zone parameter shape the default route
# refuses. tests/compare_backends.sh compares only native C with native
# LLVM, so this gate adds the default C route to each shape.
# - An inout subject parameter is a copy-in value local: its action receiver
#   and a default argument take its address (subject_inout_action_receiver).
# - A free-function prototype precedes the zone and subject bodies that call
#   it, whatever container types its signature names
#   (function_prototype_before_hosted_bodies).
# - A match-bound enum payload keeps its variant's type, so a tobject
#   payload's Array fields reach ArrayLength and index reads
#   (enum_payload_tobject_array_fields).
# - A zone parameter admits only the default borrow and `ref`; `inout` and
#   `own` are refused before emission on all four legs with one public
#   identity (zone_value_parameter_requires_transfer).
# The default LLVM route refuses the three runnable programs before emission
# (direct MIR subset), so it is not a leg for them.
set -euo pipefail

# The default legs are the self-hosted front end; an exported
# PGY_NATIVE_PIPELINE would silently turn them into native legs.
unset PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="default-route-inout-prototype-payload"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/default_route_inout_prototype_payload"
WORK_DIR="$ROOT_DIR/$WORK_REL"
CASES="tests/cases/backend_compare"
FIXTURES="tests/self_hosted/fixtures"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

compile() {
    local fixture="$1" leg="$2" out_rel="$3"
    shift 3
    local flags=()
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        native-llvm) flags=(--native-pipeline --backend=llvm) ;;
        default-c) flags=(--backend=c) ;;
        default-llvm) flags=(--backend=llvm) ;;
        *) fail "unknown leg $leg" ;;
    esac
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$fixture" "${flags[@]}" "$@" -o "$out_rel") \
        >"$ROOT_DIR/$out_rel.log" 2>&1
}

# expect_output NAME FIXTURE EXPECTED_FILE LEG...
expect_output() {
    local name="$1" fixture="$2" expected="$3"
    shift 3
    local leg out_rel
    for leg in "$@"; do
        out_rel="$WORK_REL/$name-$leg.exe"
        compile "$fixture" "$leg" "$out_rel" ||
            { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile $name"; }
        "$ROOT_DIR/$out_rel" | tr -d '\r' >"$WORK_DIR/$name-$leg.out" ||
            fail "$leg $name binary failed"
        cmp -s "$expected" "$WORK_DIR/$name-$leg.out" ||
            { diff -u "$expected" "$WORK_DIR/$name-$leg.out" >&2 || true
              fail "$leg printed another result for $name"; }
    done
}

for case_name in subject_inout_action_receiver \
                 function_prototype_before_hosted_bodies \
                 enum_payload_tobject_array_fields; do
    expect_output "$case_name" "$CASES/$case_name/main.pgy" \
        "$ROOT_DIR/$CASES/$case_name/expected.stdout" \
        native-c native-llvm default-c
done

# A `ref` zone borrow stays admitted on every leg that compiles it (the
# default-mode borrow is held by domain_runtime_zone_parameter_admission).
printf '7\n' >"$WORK_DIR/zone-ref.expected"
expect_output zone-ref "$FIXTURES/domain_runtime_zone_parameter_ref.pgy" \
    "$WORK_DIR/zone-ref.expected" native-c native-llvm default-c

# expect_zone_refusal NAME FIXTURE MODE
expect_zone_refusal() {
    local name="$1" fixture="$2" mode="$3"
    local leg out_rel fact
    for leg in native-c native-llvm default-c default-llvm; do
        out_rel="$WORK_REL/$name-$leg.exe"
        if compile "$fixture" "$leg" "$out_rel" --error-format=json; then
            fail "$leg accepted a zone '$mode' parameter ($name)"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] ||
            fail "$leg published a binary for a zone '$mode' parameter"
        for fact in '"stage":"semantic"' \
                    '"code":"PGY_SEM_ANCHORED_HANDLE_COPY"' \
                    '"cause_ir":"semantic:zone:parameter_carriage"' \
                    '"fix_source":"use-readonly-ref-or-admitted-transfer"' \
                    'zone_value_parameter_requires_transfer'; do
            grep -Fq "$fact" "$ROOT_DIR/$out_rel.log" ||
                { cat "$ROOT_DIR/$out_rel.log" >&2
                  fail "$leg lost $fact for a zone '$mode' parameter"; }
        done
    done
}

expect_zone_refusal zone-inout "$FIXTURES/zone_inout_parameter_rejected.pgy" inout
expect_zone_refusal zone-own "$FIXTURES/zone_own_parameter_rejected.pgy" own

echo "[$LABEL] inout subject receivers, staged function prototypes and match-bound tobject payload fields agree on native C, native LLVM and the default C route; zone inout/own parameters are refused with one semantic identity on all four legs: PASS"
