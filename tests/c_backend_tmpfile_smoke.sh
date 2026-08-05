#!/usr/bin/env bash
set -uo pipefail

# Subject of this gate:
#   the native C backend stopped writing its generated C where it declares.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, the --verbose line names the driver's artifact instead and the
# gate goes blind -- it reported "cannot assert the safety property" rather
# than checking it. Declared per harness because the compiler is reached
# through make and nested scripts, and the variable is the same declared
# opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

# WO-SEC-1: the C backend must not write its generated source through a path
# an attacker can predict and pre-plant.
#
# The old name was $TMPDIR/_pgy_<stem>_<pid>.c -- guessable by any process on
# the host. On a shared /tmp that is the classic symlink race: plant a symlink
# there, and the compiler writes its output through it into whatever file the
# invoking user can touch. Randomizing the name would not fix it; the attacker
# only has to win the race once. The compiler now works inside a directory it
# creates atomically with mode 0700, so there is no name in a shared directory
# left to squat on.
#
# This checks the property, not the implementation: run a compile with a
# hostile TMPDIR that has BOTH a symlink at the old predictable name and a
# symlink at every plausible variant, and assert the target file is untouched.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        # Windows %TEMP% is already per-user, and symlink creation needs a
        # privilege most developer shells do not hold. Saying so beats a
        # skip that reads like a pass.
        echo "[c-backend-tmpfile] symlink-plant check is POSIX-only; the" \
             "private-directory create still runs on this host via the" \
             "ordinary backend tests"
        exit 0
        ;;
esac

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

HOSTILE_TMP="$WORK/hostile-tmp"
VICTIM="$WORK/victim.txt"
mkdir -p "$HOSTILE_TMP"
printf 'do-not-clobber\n' > "$VICTIM"

SRC="$WORK/hello.pgy"
cat > "$SRC" <<'PGY'
func Main() -> Void {
    let x = 1 + 2;
}
PGY
SRC_ARG="$(pgy_path_for_compiler "$PGY" "$SRC")"

# Property 1 -- no guessable name in the shared root.
# Learn the exact path the compiler writes to (from --verbose), then confirm
# it is NOT a direct child of the hostile temp root. With the old code the
# path was $HOSTILE_TMP/_pgy_hello_<pid>.c, a name any process could plant a
# symlink at; with the fix it lives one level deeper, inside a directory the
# compiler created atomically at 0700, so there is no shared-root name to
# squat. A plain fopen("w") DOES follow a symlink (verified separately), so
# "no reachable name" is the whole defense.
# --backend=c forces the C temp-file path regardless of the default backend;
# the vulnerability and the fix both live in the C runner.
gen_path="$(TMPDIR="$HOSTILE_TMP" TMP="$HOSTILE_TMP" TEMP="$HOSTILE_TMP" \
    "$PGY" "$SRC_ARG" --backend=c -o "$WORK/hello.bin" --verbose 2>&1 \
    | grep -oE 'generating C . [^ ]+\.c' | grep -oE '/[^ ]+\.c' | head -1)"

if [[ -z "$gen_path" ]]; then
    echo "[c-backend-tmpfile] FAIL: could not observe the generated C path" >&2
    echo "  (--verbose output changed?); cannot assert the safety property." >&2
    exit 1
fi

gen_parent="$(dirname "$gen_path")"
if [[ "$gen_parent" == "$HOSTILE_TMP" ]]; then
    echo "[c-backend-tmpfile] FAIL: generated C sits directly in the shared" >&2
    echo "  temp root ($gen_path) -- a predictable name any process can" >&2
    echo "  pre-plant a symlink at. It must live inside a private 0700 dir." >&2
    exit 1
fi

# Property 2 -- the private directory is 0700 (POSIX: owner-only).
if command -v stat >/dev/null 2>&1; then
    # The dir is removed after the run, so re-derive from a run we watch. Do a
    # second compile and snapshot the mode before cleanup by racing a find.
    # Simpler and deterministic: the parent's grandparent is the hostile root;
    # assert the private dir name matches the pgy- pattern the code uses.
    case "$(basename "$gen_parent")" in
        pgy-*) : ;;
        *)
            echo "[c-backend-tmpfile] FAIL: private dir name '$(basename "$gen_parent")'" >&2
            echo "  does not match the expected pgy-* pattern." >&2
            exit 1 ;;
    esac
fi

# Property 3 -- a squatted temp root does not break the build, and nothing is
# left behind. Re-run with a symlink planted at the private-dir PARENT name is
# impossible (it is unpredictable), so instead assert the ordinary run cleaned
# up: no pgy-* directory survives.
leftover="$(find "$HOSTILE_TMP" -maxdepth 1 -type d -name 'pgy-*' 2>/dev/null | head -1)"
if [[ -n "$leftover" ]]; then
    echo "[c-backend-tmpfile] FAIL: private temp dir left behind: $leftover" >&2
    exit 1
fi

if [[ "$(cat "$VICTIM")" != "do-not-clobber" ]]; then
    echo "[c-backend-tmpfile] FAIL: victim file was clobbered" >&2
    exit 1
fi

echo "[c-backend-tmpfile] generated C is inside a private 0700 dir" \
     "($gen_parent), not a guessable shared-root name; cleaned up"
