#!/usr/bin/env bash
# The target capability envelope and the target profile
# (registry row target.capability_profile):
# - the probe checks the envelope's fail-closed paths and that the host's
#   profile row matches the host, refusing forged rows;
# - the native LLVM backend emits the profile's triple, which no longer comes
#   from LLVM's host default;
# - a C compiler command (PGY_CC) or an LLVM IR compiler command
#   (PGY_LLVM_CLANG) that names another target is refused, not adopted.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="target-capability"
PROBE="${PGY_TARGET_CAPABILITY_PROBE:-$ROOT_DIR/bin/test_target_capability_contract}"
if [[ "$PROBE" != *.exe && -x "${PROBE}.exe" ]]; then
    PROBE="${PROBE}.exe"
fi
if [[ ! -x "$PROBE" ]]; then
    echo "[$LABEL] missing probe: $PROBE" >&2
    exit 1
fi
pgy_require_runnable_binary_here "$LABEL" "$PROBE"
fail() { echo "[$LABEL] $*" >&2; exit 1; }

# The self-host projection of the profile rows matches the definition.
"${PYTHON_BIN:-python3}" -B "$ROOT_DIR/scripts/render_target_profile_rows.py" \
    "$ROOT_DIR/src/common/target_profile.def" \
    "$ROOT_DIR/src/self_hosted/compiler/target_profile_row_projection_owner.pgy" --check ||
    fail "the self-host target profile projection drifted"

probe_out="$("$PROBE")" || fail "the probe failed"
printf '%s\n' "$probe_out"
profile_line="$(printf '%s\n' "$probe_out" | grep '^target profile: ' | head -1)"
[[ -n "$profile_line" ]] || fail "the probe printed no target profile"
read -r _ _ profile_name profile_triple _ <<<"$profile_line"

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
WORK_REL=".tmp/target_capability"
WORK_DIR="$ROOT_DIR/$WORK_REL"
rm -rf "$WORK_DIR"; mkdir -p "$WORK_DIR"
cat >"$WORK_DIR/hello.pgy" <<'PGY'
func Main() -> Void { Log("hello"); }
PGY

# The native LLVM backend emits its object from a module that carries the
# profile's triple (PGY_LLVM_DUMP_OPT_IR writes that module).
if (cd "$ROOT_DIR" && PGY_LLVM_DUMP_OPT_IR="$WORK_DIR/emitted.ll" "$PGY" --native-pipeline \
        --backend=llvm "$WORK_REL/hello.pgy" -o "$WORK_REL/hello-llvm.exe") >"$WORK_DIR/llvm.log" 2>&1; then
    grep -Fqx "target triple = \"$profile_triple\"" "$WORK_DIR/emitted.ll" ||
        { grep -F 'target triple' "$WORK_DIR/emitted.ll" >&2 || true; fail "the emitted LLVM module does not carry the profile triple $profile_triple"; }
    llvm_checked=1
else
    grep -Fq 'LLVM' "$WORK_DIR/llvm.log" || { cat "$WORK_DIR/llvm.log" >&2; fail "native LLVM compilation failed"; }
    llvm_checked=0
fi

# A command naming another target is refused with the profile it differs from.
refused_target() {
    local variable="$1" value="$2" route="$3" status=0
    (cd "$ROOT_DIR" && env "$variable=$value" "$PGY" $route "$WORK_REL/hello.pgy" \
        -o "$WORK_REL/refused.exe") >"$WORK_DIR/refused.log" 2>&1 || status=$?
    [[ "$status" != 0 ]] || fail "$variable naming another target was accepted"
    tr -d '\r' <"$WORK_DIR/refused.log" | grep -Fq "$variable names --target=riscv64-unknown-elf; this compiler emits for $profile_triple" ||
        { cat "$WORK_DIR/refused.log" >&2; fail "$variable naming another target was not refused by the profile"; }
}
refused_target PGY_CC "cc --target=riscv64-unknown-elf" "--native-pipeline --backend=c"
if [[ "$llvm_checked" == 1 ]]; then
    refused_target PGY_LLVM_CLANG "clang --target=riscv64-unknown-elf" "--backend=llvm"
fi

echo "[$LABEL] envelope and profile probe, $profile_name LLVM triple, foreign-target refusals: PASS"
