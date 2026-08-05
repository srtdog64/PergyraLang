#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate:
#   the host task slot surface changed.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
MODULE="$ROOT_DIR/stdlib/host_task_slot.pgy"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"

fail() { echo "[host-task-slot] FAIL: $*" >&2; exit 1; }

[[ -x "$PGY" ]] || fail "missing compiler binary: $PGY"
pgy_require_runnable_binary_here "host-task-slot" "$PGY"
[[ -f "$MODULE" ]] || fail "missing $MODULE"
grep -Fq 'namespace HostTasks {' "$MODULE" || fail "missing HostTasks namespace"
grep -Fq 'ticket.generation != slot.generation' "$MODULE" || fail "missing generation guard"
grep -Fq 'func PublishWait(' "$MODULE" || fail "missing wait publication owner"
grep -Fq 'func PublishFinal(' "$MODULE" || fail "missing final publication owner"
grep -Fq 'func Cleanup(' "$MODULE" || fail "missing cleanup owner"
if grep -Eq 'Promise|Generator|AbortSignal|scheduler|mailbox|spawn[[:space:]]*[(]' "$MODULE"; then
    fail "module introduced a second task/scheduler surface"
fi

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_host_task_slot.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
FIXTURE="$WORK_DIR/main.pgy"

cat > "$FIXTURE" <<'PGY'
use host_task_slot;

func Main() -> Void
{
    let opened: HostTaskSlotTransition = HostTasks.Open("projection");
    let first: HostTaskTicket = opened.ticket;
    let replaced: HostTaskSlotTransition = HostTasks.Replace(opened.slot, "projection");
    let second: HostTaskTicket = replaced.ticket;

    Log(opened.slot.generation);
    Log(replaced.applied);
    Log(replaced.slot.generation);

    let wrong: HostTaskSlotTransition = HostTasks.Replace(replaced.slot, "other");
    Log(wrong.applied);
    Log(wrong.reason);
    Log(wrong.slot.generation);

    let early: HostTaskSlotTransition = HostTasks.Cleanup(replaced.slot, second);
    Log(early.applied);
    Log(early.reason);

    let oldWait: HostTaskSlotTransition = HostTasks.PublishWait(replaced.slot, first, "old-wait");
    let oldFinal: HostTaskSlotTransition = HostTasks.PublishFinal(replaced.slot, first, "old-final");
    let oldCleanup: HostTaskSlotTransition = HostTasks.Cleanup(replaced.slot, first);
    Log(oldWait.applied);
    Log(oldWait.reason);
    Log(oldWait.slot.generation);
    Log(HostTasks.Phase(oldWait.slot));
    Log(oldFinal.applied);
    Log(oldFinal.reason);
    Log(oldCleanup.applied);
    Log(oldCleanup.reason);

    let currentWait: HostTaskSlotTransition = HostTasks.PublishWait(replaced.slot, second, "new-wait");
    Log(currentWait.applied);
    Log(currentWait.slot.waitOutcome);
    Log(HostTasks.Phase(currentWait.slot));

    let finished: HostTaskSlotTransition = HostTasks.PublishFinal(currentWait.slot, second, "new-final");
    Log(finished.applied);
    Log(finished.slot.finalOutcome);
    Log(HostTasks.Phase(finished.slot));

    let cleaned: HostTaskSlotTransition = HostTasks.Cleanup(finished.slot, second);
    Log(cleaned.applied);
    Log(HostTasks.Phase(cleaned.slot));
    Log(HostTasks.IsCurrent(cleaned.slot, second));
}
PGY

EXPECTED="$WORK_DIR/expected.txt"
cat > "$EXPECTED" <<'OUT'
1
true
2
false
invalid_key
2
false
invalid_phase
false
stale_generation
2
running
false
stale_generation
false
stale_generation
true
new-wait
waiting
true
new-final
final
true
vacant
false
OUT

fixture_arg="$(pgy_path_for_compiler "$PGY" "$FIXTURE")"
for backend in ${PGY_HOST_TASK_SLOT_BACKENDS:-c llvm}; do
    actual="$WORK_DIR/$backend.out"
    raw="$WORK_DIR/$backend.raw"
    if ! (cd "$ROOT_DIR" && "$PGY" "$fixture_arg" --backend="$backend" --run) >"$raw" 2>&1; then
        sed -n '1,160p' "$raw" >&2
        fail "backend=$backend compile/run failed"
    fi
    tr -d '\r' < "$raw" | sed \
        -e '/^0 error(s), 0 warning(s)$/d' \
        -e '/^pgy: compiled/d' \
        | awk 'seen || length($0) > 0 { print; seen = 1 }' > "$actual"
    expected_text=""
    actual_text=""
    # NUL is absent from these text artifacts. read -d '' keeps trailing
    # newlines that command substitution would strip; EOF is the expected stop.
    IFS= read -r -d '' expected_text < "$EXPECTED" || true
    IFS= read -r -d '' actual_text < "$actual" || true
    if [[ "$expected_text" != "$actual_text" ]]; then
        echo "--- expected" >&2
        sed -n '1,160p' "$EXPECTED" >&2
        echo "+++ actual" >&2
        sed -n '1,160p' "$actual" >&2
        fail "backend=$backend output mismatch"
    fi
    echo "[host-task-slot] backend=$backend generation authority locked"
done
