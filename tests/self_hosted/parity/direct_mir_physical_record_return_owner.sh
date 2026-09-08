#!/usr/bin/env bash
# Required record layouts survive real compiler returns and non-first copyouts.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-physical-record-return"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
SOURCE="tests/self_hosted/fixtures/direct_mir_physical_record_return.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/physical-record-return.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"
echo "[$LABEL] evidence: $WORK_REL"
printf '1\n7\n1\n-2\n14\n-11\n-13\n17\n-6\n9\n' >"$WORK_DIR/expected.run"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE" \
    -o "$WORK_REL/program.mir.json") >"$WORK_DIR/producer.log" 2>&1 || {
    cat "$WORK_DIR/producer.log" >&2; fail "verified MIR producer rejected the source";
}
for backend in c llvm; do
    artifact="$WORK_DIR/program.$backend"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$WORK_REL/program.mir.json" -o "$WORK_REL/program.$backend") \
        >"$WORK_DIR/$backend.project.log" 2>&1 || {
        cat "$WORK_DIR/$backend.project.log" >&2; fail "$backend projection failed";
    }
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$WORK_DIR/direct-c.exe")
    else
        command=("$CLANG" -x ir "$artifact" -o "$WORK_DIR/direct-llvm.exe")
    fi
    "${command[@]}" >"$WORK_DIR/$backend.compile.log" 2>&1 || {
        cat "$WORK_DIR/$backend.compile.log" >&2; fail "$backend artifact compilation failed";
    }
    "$WORK_DIR/direct-$backend.exe" | tr -d '\r' >"$WORK_DIR/direct-$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/direct-$backend.run" ||
        fail "$backend direct MIR runtime differed"
    (cd "$ROOT_DIR" && "$PGY" "$SOURCE" "--backend=$backend" --opt=dev \
        -o "$WORK_REL/public-$backend.exe") >"$WORK_DIR/$backend.public.log" 2>&1 || {
        cat "$WORK_DIR/$backend.public.log" >&2; fail "$backend public compilation failed";
    }
    "$WORK_DIR/public-$backend.exe" | tr -d '\r' >"$WORK_DIR/public-$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/public-$backend.run" ||
        fail "$backend public runtime differed"
done
for mutation in declaration-missing-layout declaration-absent-layout \
    declaration-field-identity declaration-layout-width parameter-missing-layout \
    parameter-absent-layout parameter-layout-width instruction-missing-layout \
    instruction-absent-layout instruction-layout-id coherent-wide-layout \
    coherent-logical-physical; do
    python "$ROOT_DIR/tests/self_hosted/parity/direct_mir_physical_record_return_mutations.py" \
        "$WORK_DIR/program.mir.json" "$mutation" "$WORK_DIR/$mutation.mir.json"
    case "$mutation" in
        coherent-*) diagnostic='CODEGEN ERROR: direct MIR logical record target projection is invalid' ;;
        declaration-field-identity) diagnostic='MIR-LOWER ERROR: MIR machine-layer facts are missing or invalid' ;;
        instruction-*) diagnostic='CODEGEN ERROR: .*stage=logical_record_abi' ;;
        *) diagnostic='CODEGEN ERROR: direct MIR scalar program route rejected: owner=' ;;
    esac
    for backend in c llvm; do
        output="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/$mutation.mir.json" -o "$output") \
            >"$WORK_DIR/$mutation.$backend.log" 2>&1; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output" ]] || fail "$backend published $mutation"
        grep -Eq "$diagnostic" "$WORK_DIR/$mutation.$backend.log" || {
            cat "$WORK_DIR/$mutation.$backend.log" >&2
            fail "$backend did not reject $mutation through its fact owner"
        }
    done
done
echo "[$LABEL] real owner + physical widths + nested/non-first copyout + missing-fact gates: PASS"
