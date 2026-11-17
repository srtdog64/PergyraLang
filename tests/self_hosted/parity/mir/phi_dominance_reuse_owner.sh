#!/usr/bin/env bash
# One invocation owns dominance proofs; no reuse crosses a routine generation.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/phi-dominance-reuse.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[phi-dominance-reuse] $*; evidence=$REL" >&2; exit 1; }
"$DRIVER" --emit-mir-json-verified src/self_hosted/mir_lower/fixture/if_else_assign.pgy \
    -o "$REL/input.json" >"$WORK/produce.log" 2>&1 || fail "producer refused the phi input"
for backend in c llvm; do
    probe="$WORK/probe-$backend.exe"
    "$PGY" --native-pipeline --backend="$backend" --opt=dev \
        tests/self_hosted/fixtures/mir_phi_dominance_reuse_probe.pgy \
        -o "$probe" >"$WORK/$backend.compile" 2>&1 || {
            cat "$WORK/$backend.compile" >&2; fail "$backend probe did not compile";
        }
    "$probe" "$REL/input.json" >"$WORK/$backend.run" 2>&1 || fail "$backend replay failed"
    grep -Fxq 'MIR PHI REUSE PASS' "$WORK/$backend.run" || fail "$backend did not reach a phi"
    if "$probe" "$REL/input.json" corrupt >"$WORK/$backend.corrupt" 2>&1; then
        fail "$backend accepted a corrupted proof"
    fi
    grep -Fq 'MIR phi dominance reuse diverged' "$WORK/$backend.corrupt" ||
        fail "$backend did not reject at proof reuse"
done
"${PYTHON_BIN:-python3}" - "$WORK" <<'PY'
import json
import pathlib
import sys
work = pathlib.Path(sys.argv[1])
document = json.loads((work / 'input.json').read_text())
changed = 0
for routine in document['routines']:
    for block in routine['blocks']:
        for instruction in block['instructions']:
            if instruction['kind'] == 'phi':
                assert len(instruction['uses']) == 2
                instruction['uses'][1] = instruction['uses'][0]
                changed += 1
assert changed > 0
(work / 'duplicate.json').write_text(json.dumps(document, separators=(',', ':')))
PY
for backend in c llvm; do
    suffix=c
    [[ "$backend" != llvm ]] || suffix=ll
    "$DRIVER" "--mir-json-backend=$backend" "$REL/input.json" \
        -o "$REL/output.$suffix" >"$WORK/$backend.emit" 2>&1 || fail "$backend refused valid bindings"
    if "$DRIVER" "--mir-json-backend=$backend" "$REL/duplicate.json" \
        -o "$REL/forged.$suffix" >"$WORK/$backend.refuse" 2>&1; then
        fail "$backend accepted an uncovered predecessor"
    fi
    [[ ! -e "$WORK/forged.$suffix" ]] || fail "$backend published forged bindings"
    grep -Eiq 'phi|predecessor' "$WORK/$backend.refuse" || fail "$backend lost the phi refusal"
done
c_command=("${CC:-gcc}" -std=c11 "$WORK/output.c")
if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK/output.c"; then
    c_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
c_command+=(-o "$WORK/output-c.exe")
"${c_command[@]}" || fail "emitted C did not compile"
"${PGY_SELFHOST_CLANG:-clang}" -x ir "$WORK/output.ll" -o "$WORK/output-llvm.exe" \
    >"$WORK/llvm.link" 2>&1 || { cat "$WORK/llvm.link" >&2; fail "LLVM link failed"; }
for backend in c llvm; do
    "$WORK/output-$backend.exe" >"$WORK/output-$backend.run"
    [[ "$(tr -d '\r' <"$WORK/output-$backend.run")" == 2 ]] || fail "$backend phi value changed"
done
echo "[phi-dominance-reuse] PASS: SSA boundaries, C/LLVM replay, forged proof and uncovered predecessor; evidence=$REL"
