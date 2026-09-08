#!/usr/bin/env bash
# Real-owner query controls; this is not external-MIR admission or full bootstrap.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL=self-host-record-shape-query
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_expression_owner.pgy"
PYTHON="${PYTHON_BIN:-python}"
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
fail() { echo "[$LABEL] $*" >&2; exit 1; }
"$PYTHON" "$ROOT_DIR/tests/self_hosted/parity/logical_record_shape_query_order.py" "$OWNER"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/record-shape-query.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
SOURCE=tests/self_hosted/fixtures/logical_record_shape_query.pgy
echo "[$LABEL] evidence: $REL"
sha256sum "$PGY" "$DRIVER" "$ROOT_DIR/$SOURCE" "$OWNER" >"$WORK/input.sha256"
printf 'true\ntrue\ntrue\ntrue\nfalse\nfalse\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\n' >"$WORK/expected.run"
for origin in native public; do
    for backend in c llvm; do
        stem="$origin-$backend"
        command=("$PGY" "$SOURCE")
        [[ "$origin" == native ]] && command+=(--native-pipeline)
        command+=("--backend=$backend" --opt=dev -o "$REL/$stem.exe")
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 \
            PGY_SELF_DRIVER_BIN="$DRIVER" "${command[@]}") >"$WORK/$stem.compile.log" 2>&1 || {
            cat "$WORK/$stem.compile.log" >&2; fail "$stem compilation failed";
        }
        if [[ "$origin" == public ]]; then
            ! grep -Fq '[pipeline timing]' "$WORK/$stem.compile.log" || fail "$stem used native fallback"
        fi
        timeout 30s "$WORK/$stem.exe" >"$WORK/$stem.raw" 2>"$WORK/$stem.err" || {
            cat "$WORK/$stem.err" >&2; fail "$stem query execution failed";
        }
        [[ ! -s "$WORK/$stem.err" ]] || fail "$stem emitted an unexpected diagnostic"
        tr -d '\r' <"$WORK/$stem.raw" >"$WORK/$stem.run"
        cmp -s "$WORK/expected.run" "$WORK/$stem.run" || {
            diff -u "$WORK/expected.run" "$WORK/$stem.run" >&2; fail "$stem query outcomes differed";
        }
        echo "[$LABEL] $stem: 17 identity/shape/digest/ABI controls PASS"
    done
done
"$PYTHON" "$ROOT_DIR/tests/self_hosted/parity/logical_record_shape_query_pressure.py" "$WORK/native-c.exe"
echo "[$LABEL] native/public C/LLVM query parity: PASS"
