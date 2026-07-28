#!/usr/bin/env bash
set -euo pipefail

# Executable legacy intent contract: an On action completes before its
# guard/expect/post predicates. Failure runs completed steps in reverse and
# each step's compensate expressions in reverse. Typed variant success-only
# completion is a later rung and is not claimed here.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-compensation] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-compensation" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON_BIN:-python3}"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"
CC_BIN="${CC:-gcc}"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"

FIXTURE_REL="tests/self_hosted/parity/fixture/intent_guard_post_compensation_execution.pgy"
BUILD_DIR="${PGY_SELFHOST_INTENT_COMPENSATION_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_guard_post_compensation}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
mkdir -p "$BUILD_DIR"

if [[ -n "$DRIVER" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "self-host-intent-compensation" "$DRIVER" \
        || fail "prebuilt driver is not runnable"
else
    DRIVER="$BUILD_DIR/driver_rung2.exe"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) \
        || { tail -c 65536 "$BUILD_DIR/driver.compile.log" >&2; fail "driver build failed"; }
fi

SELF_MIR="$BUILD_DIR/self.mir.json"
SELF_FROM_MIR_C="$BUILD_DIR/self.from-mir.c"
SELF_DIRECT_C="$BUILD_DIR/self.direct.c"
SELF_EXE="$BUILD_DIR/self.exe"
NATIVE_C_EXE="$BUILD_DIR/native.c.exe"
NATIVE_LLVM_EXE="$BUILD_DIR/native.llvm.exe"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$FIXTURE_REL" \
    >"$SELF_MIR" 2>"$BUILD_DIR/self.mir.err") \
    || { cat "$SELF_MIR" "$BUILD_DIR/self.mir.err" >&2; fail "self MIR production failed"; }
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "${SELF_MIR#"$ROOT_DIR/"}" \
    >"$SELF_FROM_MIR_C" 2>"$BUILD_DIR/self.from-mir.err") \
    || { cat "$SELF_FROM_MIR_C" "$BUILD_DIR/self.from-mir.err" >&2; fail "admitted MIR C emission failed"; }
(cd "$ROOT_DIR" && "$DRIVER" "$FIXTURE_REL" --emit-c-verified \
    >"$SELF_DIRECT_C" 2>"$BUILD_DIR/self.direct.err") \
    || { cat "$SELF_DIRECT_C" "$BUILD_DIR/self.direct.err" >&2; fail "direct source C emission failed"; }
cmp -s "$SELF_FROM_MIR_C" "$SELF_DIRECT_C" \
    || fail "direct source entrypoint bypassed admitted intent control MIR"

"$PYTHON_BIN" - "$SELF_DIRECT_C" <<'PY'
from pathlib import Path
import sys

text = Path(sys.argv[1]).read_text(encoding="utf-8")
start = text.index("bool RunWorkflow(")
end = text.index("\n}\n\n", start)
body = text[start:end]
assert body.count("__intent_step_completed[0] = true;") == 1, body
assert body.count("__intent_step_completed[1] = true;") == 1, body
assert body.count("goto __intent_cleanup;") >= 3, body
assert body.count("if (__intent_failed)") == 1, body
cleanup = body.index("__intent_cleanup:")
second = body.index("WorkflowActor_UndoBSecond", cleanup)
first = body.index("WorkflowActor_UndoBFirst", second)
undo_a = body.index("WorkflowActor_UndoA", first)
assert cleanup < second < first < undo_a, body
PY

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$SELF_DIRECT_C" -o "$SELF_EXE"
(cd "$ROOT_DIR" && "$PGY" "$FIXTURE_REL" --backend=c -o "$NATIVE_C_EXE" \
    >"$BUILD_DIR/native.c.compile.log" 2>&1) \
    || { cat "$BUILD_DIR/native.c.compile.log" >&2; fail "native C compile failed"; }
(cd "$ROOT_DIR" && "$PGY" "$FIXTURE_REL" --backend=llvm -o "$NATIVE_LLVM_EXE" \
    >"$BUILD_DIR/native.llvm.compile.log" 2>&1) \
    || { cat "$BUILD_DIR/native.llvm.compile.log" >&2; fail "native LLVM compile failed"; }

"$SELF_EXE" | tr -d '\r' >"$BUILD_DIR/self.run"
"$NATIVE_C_EXE" | tr -d '\r' >"$BUILD_DIR/native.c.run"
"$NATIVE_LLVM_EXE" | tr -d '\r' >"$BUILD_DIR/native.llvm.run"
printf '%s\n' \
    'success.ok=true' \
    'success.state=13' \
    'success.trace=12' \
    'success.a_calls=1' \
    'success.b_calls=1' \
    'success.undo_a=0' \
    'success.undo_b_first=0' \
    'success.undo_b_second=0' \
    'first_guard.ok=false' \
    'first_guard.state=0' \
    'first_guard.trace=15' \
    'first_guard.a_calls=1' \
    'first_guard.b_calls=0' \
    'first_guard.undo_a=1' \
    'first_guard.undo_b_first=0' \
    'first_guard.undo_b_second=0' \
    'guard.ok=false' \
    'guard.state=0' \
    'guard.trace=12345' \
    'guard.a_calls=1' \
    'guard.b_calls=1' \
    'guard.undo_a=1' \
    'guard.undo_b_first=1' \
    'guard.undo_b_second=1' \
    'expect.ok=false' \
    'expect.state=0' \
    'expect.trace=12345' \
    'expect.a_calls=1' \
    'expect.b_calls=1' \
    'expect.undo_a=1' \
    'expect.undo_b_first=1' \
    'expect.undo_b_second=1' \
    'post.ok=false' \
    'post.state=0' \
    'post.trace=12345' \
    'post.a_calls=1' \
    'post.b_calls=1' \
    'post.undo_a=1' \
    'post.undo_b_first=1' \
    'post.undo_b_second=1' >"$BUILD_DIR/expected.run"
cmp -s "$BUILD_DIR/expected.run" "$BUILD_DIR/self.run" \
    || { cat "$BUILD_DIR/self.run" >&2; fail "self runtime output drifted"; }
cmp -s "$BUILD_DIR/self.run" "$BUILD_DIR/native.c.run" \
    || fail "self/native C compensation execution differs"
cmp -s "$BUILD_DIR/self.run" "$BUILD_DIR/native.llvm.run" \
    || fail "self/native LLVM compensation execution differs"

echo "[self-host-intent-compensation] guard/expect/post failure + future-step exclusion + ordered compensation parity: PASS"
