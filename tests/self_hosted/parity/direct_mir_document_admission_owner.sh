#!/usr/bin/env bash
# Exact root grammar and routine-local InstructionId admission precede emission.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-document-admission"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_scalar_bool_sub_equals_short_circuit.pgy"
WORK_REL=".tmp/self_hosted/direct_mir_document_admission"
WORK_DIR="$ROOT_DIR/$WORK_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_document_admission_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v python >/dev/null 2>&1 || fail "missing python"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/seed.mir.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "valid seed production failed"
[[ -s "$WORK_DIR/seed.mir.json" ]] || fail "valid seed is empty"

for backend in c llvm; do
    extension="$backend"
    [[ "$backend" == llvm ]] && extension="ll"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$WORK_REL/seed.mir.json" -o "$WORK_REL/control.$extension") \
        >"$WORK_DIR/control.$backend.out" \
        2>"$WORK_DIR/control.$backend.err" ||
        fail "valid $backend control projection failed"
    [[ -s "$WORK_DIR/control.$extension" ]] ||
        fail "valid $backend control emitted no artifact"
done

python "$MUTATIONS" "$WORK_DIR/seed.mir.json" "$WORK_DIR"
for mutation in leading-comma extra-root-close duplicate-instruction-id; do
    expected="input is not a pgy.mir.v1 MIR-JSON document"
    [[ "$mutation" == duplicate-instruction-id ]] &&
        expected="MIR instruction identities are missing or duplicated"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/$mutation.mir.json" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
        grep -Fq "$expected" "$WORK_DIR/$mutation.$backend.out" \
            "$WORK_DIR/$mutation.$backend.err" || {
            cat "$WORK_DIR/$mutation.$backend.out" \
                "$WORK_DIR/$mutation.$backend.err" >&2
            fail "$backend $mutation diagnostic drifted"
        }
    done
done

echo "[$LABEL] root syntax/EOF and routine-scoped InstructionId refusal: PASS"
