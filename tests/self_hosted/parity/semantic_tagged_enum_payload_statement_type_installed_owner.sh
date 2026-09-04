#!/usr/bin/env bash
# Direct tagged-enum payload reads carry their declaration-owned type through
# semantic statement facts into the installed public C emitter.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-semantic-tagged-enum-payload-statement-type"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/semantic_tagged_enum_payload_statement_type"
WORK_DIR="$ROOT_DIR/$WORK_REL"
POSITIVE_REL="tests/cases/backend_compare/tagged_union/main.pgy"
ADJACENT_REL="src/self_hosted/mir_lower/fixture/enum_multi_payload.pgy"
NEGATIVE_REL="tests/self_hosted/parity/fixture/tagged_enum_payload_ordinal_rejected.pgy"
STATEMENT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_statement_type_fact_owner.pgy"
INITIALIZER_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy"
PAYLOAD_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_enum_payload_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$SELF_DRIVER" || exit 1

PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
installed_name="pgy-self-driver"
suffix=""
if [[ "$PGY" == *.exe ]]; then
    installed_name="pgy-self-driver.exe"
    suffix=".exe"
fi
[[ "$SELF_DRIVER" == "$(dirname "$PGY")/$installed_name" ]] ||
    fail "self-host driver is not installed beside the public launcher"

grep -Fq 'import "ast_expression_graph_enum_payload_owner.pgy";' \
    "$STATEMENT_OWNER" || fail "statement facts do not import the payload owner"
grep -Fq 'SemanticExpressionGraphEnumPayloadTypeName(' "$STATEMENT_OWNER" ||
    fail "statement facts bypass the payload type projection"
grep -Fq 'SemanticExpressionGraphEnumPayloadTypeName(' "$INITIALIZER_OWNER" ||
    fail "initializer and statement consumers no longer share one projection"
[[ "$(wc -l <"$STATEMENT_OWNER" | tr -d ' ')" -le 540 ]] ||
    fail "statement fact owner exceeds its component limit"
! grep -Eq 'tagged_union|(^|[^[:alnum:]_])(Shape|Circle|Rect)([^[:alnum:]_]|$)|\._0|\._1' \
    "$STATEMENT_OWNER" "$PAYLOAD_OWNER" ||
    fail "fixture spelling leaked into the semantic payload path"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
printf '10\n4\n5\n7\n' >"$WORK_DIR/expected.run"
printf '0\n75\n28\n120\n81\n' >"$WORK_DIR/adjacent.expected.run"

compile_run() {
    local mode="$1" source="$2" stem="$3" expected="$4"
    local output_rel="$WORK_REL/$stem$suffix"
    local args=("$source" --backend=c -o "$output_rel")
    [[ "$mode" == native ]] && args+=(--native-pipeline)
    if [[ "$mode" == public ]]; then
        (cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN &&
            PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "${args[@]}") \
            >"$WORK_DIR/$stem.compile.out" 2>"$WORK_DIR/$stem.compile.err" ||
            fail "$stem public C compilation failed"
        ! grep -Fq '[pipeline timing]' "$WORK_DIR/$stem.compile.out" \
            "$WORK_DIR/$stem.compile.err" || fail "$stem retried native"
    else
        (cd "$ROOT_DIR" && "$PGY" "${args[@]}") \
            >"$WORK_DIR/$stem.compile.out" 2>"$WORK_DIR/$stem.compile.err" ||
            fail "$stem native C oracle failed"
    fi
    "$WORK_DIR/$stem$suffix" | tr -d '\r' >"$WORK_DIR/$stem.run"
    cmp -s "$expected" "$WORK_DIR/$stem.run" ||
        fail "$stem runtime output drifted"
}

compile_run public "$POSITIVE_REL" tagged-public "$WORK_DIR/expected.run"
compile_run native "$POSITIVE_REL" tagged-native "$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/tagged-public.run" "$WORK_DIR/tagged-native.run" ||
    fail "installed payload reads diverged from the native oracle"
compile_run public "$ADJACENT_REL" adjacent-public \
    "$WORK_DIR/adjacent.expected.run"

set +e
(cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN &&
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$NEGATIVE_REL" --backend=c \
    -o "$WORK_REL/rejected-public$suffix") \
    >"$WORK_DIR/rejected-public.out" 2>"$WORK_DIR/rejected-public.err"
public_rc=$?
(cd "$ROOT_DIR" && "$PGY" "$NEGATIVE_REL" --backend=c --native-pipeline \
    -o "$WORK_REL/rejected-native$suffix") \
    >"$WORK_DIR/rejected-native.out" 2>"$WORK_DIR/rejected-native.err"
native_rc=$?
set -e

[[ "$public_rc" -ne 0 && "$native_rc" -ne 0 ]] ||
    fail "invalid payload ordinal was accepted"
[[ ! -e "$WORK_DIR/rejected-public$suffix" &&
    ! -e "$WORK_DIR/rejected-native$suffix" ]] ||
    fail "invalid payload ordinal published an artifact"
grep -Fq 'missing semantic statement result type' \
    "$WORK_DIR/rejected-public.out" "$WORK_DIR/rejected-public.err" ||
    fail "installed rejection escaped the statement type consumer"
grep -Fq 'Unknown enum payload field' \
    "$WORK_DIR/rejected-native.out" "$WORK_DIR/rejected-native.err" ||
    fail "native oracle no longer rejects the invalid ordinal"
! grep -Fq '[pipeline timing]' "$WORK_DIR/rejected-public.out" \
    "$WORK_DIR/rejected-public.err" || fail "public rejection retried native"

echo "[$LABEL] installed/native C 10+4+5+7, adjacent enum, and ordinal negative: PASS"
