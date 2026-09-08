#!/usr/bin/env bash
# The existing CFG identity owner hashes every byte with one length computation.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL=self-host-cfg-identity-digest
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
REFERENCE="$ROOT_DIR/tests/self_hosted/parity/direct_mir_cfg_identity_digest_reference.py"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
OWNER="$ROOT_DIR/src/self_hosted/air/mir_cfg_identity_owner.pgy"
python "$REFERENCE" "$OWNER"
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler"
command -v timeout >/dev/null 2>&1 || fail "missing bounded runtime tool"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/cfg-identity-digest.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
echo "[$LABEL] evidence: $REL"
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" tests/self_hosted/fixtures/direct_mir_cfg_identity_digest.pgy \
    --emit-c -o "$REL/probe.c") >"$WORK/emit.log" 2>&1 || {
    cat "$WORK/emit.log" >&2; fail "actual-owner probe emission failed";
}
! grep -Fq '[pipeline timing]' "$WORK/emit.log" || fail "native pipeline used"
python "$REFERENCE" "$OWNER" "$WORK/probe.c" "$WORK/expected.run"
(cd "$ROOT_DIR" && "$PGY" tests/self_hosted/fixtures/direct_mir_cfg_identity_digest.pgy \
    --native-pipeline --emit-c -o "$REL/probe.native.c") >"$WORK/native.emit.log" 2>&1 || {
    cat "$WORK/native.emit.log" >&2; fail "native-oracle probe emission failed";
}
for probe in probe probe.native; do
    command=("$CC" -x c -std=c11 -O0 -fwrapv -fno-strict-aliasing "$WORK/$probe.c")
    if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK/$probe.c"; then
        command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    command+=(-lm -o "$WORK/$probe.exe")
    "${command[@]}" >"$WORK/$probe.compile.log" 2>&1 || {
        cat "$WORK/$probe.compile.log" >&2; fail "$probe C compilation failed";
    }
    timeout 30 "$WORK/$probe.exe" | tr -d '\r' >"$WORK/$probe.run"
    cmp -s "$WORK/expected.run" "$WORK/$probe.run" || fail "$probe byte identity drifted"
done
"$CLANG" -std=c11 -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK/runtime.o" >"$WORK/runtime.compile.log" 2>&1
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" \
    tests/self_hosted/fixtures/direct_mir_cfg_identity_digest.pgy --backend=llvm --opt=dev \
    -o "$REL/public-llvm.exe") >"$WORK/public-llvm.compile.log" 2>&1 || {
    cat "$WORK/public-llvm.compile.log" >&2; fail "public LLVM hash compilation failed";
}
! grep -Fq '[pipeline timing]' "$WORK/public-llvm.compile.log" || fail "LLVM native fallback"
timeout 30 "$WORK/public-llvm.exe" | tr -d '\r' >"$WORK/public-llvm.run"
cmp -s "$WORK/expected.run" "$WORK/public-llvm.run" || fail "public LLVM hash drifted"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    tests/self_hosted/fixtures/direct_mir_cfg_identity_digest.pgy -o "$REL/probe.mir.json") \
    >"$WORK/mir-producer.log" 2>&1 || { cat "$WORK/mir-producer.log" >&2; fail "hash MIR production failed"; }
for backend in c llvm; do
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$REL/probe.mir.json" \
        -o "$REL/direct.$backend") >"$WORK/direct-$backend.project.log" 2>&1 || {
        cat "$WORK/direct-$backend.project.log" >&2; fail "$backend hash projection failed";
    }
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 -O0 -fwrapv -fno-strict-aliasing "$WORK/direct.c" \
            "-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread -lm -o "$WORK/direct-c.exe")
    else
        command=("$CLANG" -x ir "$WORK/direct.llvm" -x none "$WORK/runtime.o" -pthread -lm -o "$WORK/direct-llvm.exe")
    fi
    "${command[@]}" >"$WORK/direct-$backend.compile.log" 2>&1 || {
        cat "$WORK/direct-$backend.compile.log" >&2; fail "$backend hash artifact compilation failed";
    }
    timeout 30 "$WORK/direct-$backend.exe" | tr -d '\r' >"$WORK/direct-$backend.run"
    cmp -s "$WORK/expected.run" "$WORK/direct-$backend.run" || fail "$backend direct hash drifted"
done
echo "[$LABEL] native/public C, public LLVM and one-MIR C/LLVM nine-value identity: PASS"
