#!/usr/bin/env bash
#
# symlink_write_nofollow_smoke.sh — regression for finding 2026-07-05-001.
#
# The sandbox write path (PGY_IO_ROOT) must refuse to write THROUGH a symlink
# whose final component points out of the sandbox. The runtime resolves +
# lstat-checks, then opens with O_NOFOLLOW (pgy_runtime_secure_open.h), so a
# symlinked target fails closed at open time.
#
# POSIX only: symlink creation is required. SKIPs cleanly where symlinks are
# unavailable (Windows, or a filesystem that refuses ln -s) so the gate stays
# load-bearing where it can and never falsely fails where it cannot run.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

skip() { echo "[symlink-nofollow] SKIP: $*"; exit 0; }
fail() { echo "[symlink-nofollow] FAIL: $*" >&2; exit 1; }

# Platform gate first: O_NOFOLLOW symlink refusal is a POSIX property.
case "$(uname -s 2>/dev/null || echo unknown)" in
    *MINGW*|*MSYS*|*CYGWIN*|Windows*|unknown)
        skip "symlink semantics not exercised on this platform (open O_NOFOLLOW is POSIX)" ;;
esac

source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || skip "pgy binary not found at $PGY"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

SANDBOX="$WORK/sandbox"
OUTSIDE="$WORK/outside"
mkdir -p "$SANDBOX" "$OUTSIDE"
printf 'ORIGINAL-SECRET\n' > "$OUTSIDE/secret.txt"
printf 'ORIGINAL-HANDLE-SECRET\n' > "$OUTSIDE/handle_secret.txt"

# Plant a symlink AT the write target, pointing out of the sandbox.
if ! ln -s "$OUTSIDE/secret.txt" "$SANDBOX/results.txt" 2>/dev/null; then
    skip "ln -s unavailable on this filesystem"
fi
ln -s "$OUTSIDE/handle_secret.txt" "$SANDBOX/handle.txt" 2>/dev/null \
    || skip "ln -s unavailable on this filesystem"

# A minimal program that writes to symlinked sandbox targets through both whole-file
# and handle-based I/O. Both paths must route through the secure open owner.
cat > "$WORK/writer.pgy" <<'PGY'
func Main() -> Void {
    WriteFile("results.txt", "PWNED-BY-SANDBOX-WRITE\n");
    let fd: Int = FileOpen("handle.txt", "w");
    if fd >= 0 {
        FileWrite(fd, "PWNED-BY-HANDLE-OPEN\n");
        FileClose(fd);
    }
    Log("write returned");
}
PGY

SRC="$(pgy_path_for_compiler "$PGY" "$WORK/writer.pgy")"
OUT="$(pgy_path_for_compiler "$PGY" "$WORK/writer")"
(cd "$ROOT_DIR" && "$PGY" "$SRC" --backend=c -o "$OUT") >"$WORK/build.log" 2>&1 \
    || fail "writer.pgy must compile: $(tail -2 "$WORK/build.log")"

# Run with the sandbox root set. The write must NOT reach the outside file.
PGY_IO_ROOT="$SANDBOX" "$OUT" >"$WORK/run.log" 2>&1 || true

if grep -q 'PWNED-BY-SANDBOX-WRITE' "$OUTSIDE/secret.txt"; then
    fail "sandbox write followed the symlink and clobbered an outside file (O_NOFOLLOW regression)"
fi
if [[ "$(cat "$OUTSIDE/secret.txt")" != "ORIGINAL-SECRET" ]]; then
    fail "outside file was modified through the symlinked sandbox target"
fi
if grep -q 'PWNED-BY-HANDLE-OPEN' "$OUTSIDE/handle_secret.txt"; then
    fail "sandbox FileOpen followed the symlink and clobbered an outside file (O_NOFOLLOW regression)"
fi
if [[ "$(cat "$OUTSIDE/handle_secret.txt")" != "ORIGINAL-HANDLE-SECRET" ]]; then
    fail "outside handle file was modified through the symlinked sandbox target"
fi

echo "[symlink-nofollow] ok: sandbox write refused to follow a symlink out of PGY_IO_ROOT"
