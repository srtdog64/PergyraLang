#!/usr/bin/env bash
# Surface boundary hygiene gate (docs/189 C11 + C12).
#
# Pins two fail-closed surface rules:
#   C11 — C reserved words cannot name functions or parameters (they are
#         emitted verbatim into the C backend; the self-hosted compiler's
#         CompilerSymbolCReservedWord list is the SoT twin). Let-bound
#         locals stay legal: they are SSA-renamed on emission and the
#         corpus uses names like `let double`.
#   C12 — Channel<T> is identity-bearing: a let may only be born from a
#         fresh immutable Channel(...) constructor, and a Channel cannot
#         cross a return boundary. Aliased descriptors drift (silent
#         split deliveries), so every copy edge fails closed until the
#         representation ruling (board WO-RT-6) lands.
#
# Subject of this gate:
#   native semantic C-boundary diagnostics and native Channel ownership rules.
# Delegating would turn a self-host surface gap into a native boundary-policy
# regression. This is the declared in-process opt-out, never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-surface-hygiene.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

expect_reject() {
    local name="$1"
    local needle="$2"
    local out rc
    set +e
    out="$("$PGY" "$(pgy_path_for_compiler "$PGY" "$WORK_DIR/$name.pgy")" \
        -o "$(pgy_path_for_compiler "$PGY" "$WORK_DIR/$name.out")" 2>&1)"
    rc=$?
    set -e
    if [[ "$rc" -eq 0 ]]; then
        echo "[surface-hygiene] $name unexpectedly compiled" >&2
        echo "$out" >&2
        exit 1
    fi
    if ! grep -Fq "$needle" <<<"$out"; then
        echo "[surface-hygiene] $name missing diagnostic '$needle'" >&2
        echo "$out" >&2
        exit 1
    fi
}

expect_accept_run() {
    local name="$1"
    local backend="$2"
    local expected="$3"
    local out rc exe
    exe="$WORK_DIR/$name-$backend.out"
    set +e
    "$PGY" "$(pgy_path_for_compiler "$PGY" "$WORK_DIR/$name.pgy")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$exe")" >/dev/null 2>&1
    rc=$?
    set -e
    if [[ "$rc" -ne 0 ]]; then
        echo "[surface-hygiene] $name/$backend failed to compile" >&2
        exit 1
    fi
    set +e
    out="$("$exe")"
    rc=$?
    set -e
    if [[ "$rc" -ne 0 || "$out" != "$expected" ]]; then
        echo "[surface-hygiene] $name/$backend expected '$expected' rc=0, got '$out' rc=$rc" >&2
        exit 1
    fi
}

cat > "$WORK_DIR/param_reserved.pgy" <<'PGY'
func Register(union: Int) -> Int {
    return union;
}
func Main() -> Int {
    return Register(1);
}
PGY

cat > "$WORK_DIR/func_reserved.pgy" <<'PGY'
func typedef() -> Int {
    return 1;
}
func Main() -> Int {
    return typedef();
}
PGY

cat > "$WORK_DIR/channel_let_copy.pgy" <<'PGY'
func Main() -> Int {
    let ch: Channel<Int> = Channel(4);
    let c2 = ch;
    c2 <- 1;
    return 0;
}
PGY

cat > "$WORK_DIR/channel_return.pgy" <<'PGY'
func Make() -> Channel<Int> {
    let ch: Channel<Int> = Channel(4);
    return ch;
}
func Main() -> Int {
    return 0;
}
PGY

cat > "$WORK_DIR/channel_mut.pgy" <<'PGY'
func Main() -> Int {
    let mut ch: Channel<Int> = Channel(4);
    ch <- 1;
    return 0;
}
PGY

cat > "$WORK_DIR/channel_param.pgy" <<'PGY'
func Feed(ch: Channel<Int>) -> Void {
    ch <- 1;
}
func Main() -> Int {
    let ch: Channel<Int> = Channel(4);
    Feed(ch);
    return 0;
}
PGY

cat > "$WORK_DIR/legal_surface.pgy" <<'PGY'
func Main() -> Int {
    let double: Int = 3;
    let ch: Channel<Int> = Channel(2);
    ch <- double;
    let v = <- ch;
    Log("v=" + ToString(v));
    return 0;
}
PGY

expect_reject "param_reserved" "is a C reserved word"
expect_reject "func_reserved" "is a C reserved word"
expect_reject "channel_let_copy" "identity-bearing"
expect_reject "channel_return" "Channel cannot cross a return boundary"
expect_reject "channel_mut" "identity-bearing"
expect_reject "channel_param" "Channel parameters are not supported"
expect_accept_run "legal_surface" "c" "v=3"
expect_accept_run "legal_surface" "llvm" "v=3"

echo "[surface-hygiene] C-reserved-word and Channel copy-edge rules hold; SSA-renamed lets and constructor-born channels stay legal"
