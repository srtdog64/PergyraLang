#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
MODULE="$ROOT_DIR/stdlib/host_task_slot.pgy"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"

fail() { echo "[host-task-policy] FAIL: $*" >&2; exit 1; }

[[ -x "$PGY" ]] || fail "missing compiler binary: $PGY"
pgy_require_runnable_binary_here "host-task-policy" "$PGY"
[[ -f "$MODULE" ]] || fail "missing $MODULE"
grep -Fq 'export enum HostTaskApplyPolicy' "$MODULE" ||
    fail "missing typed policy vocabulary"
grep -Fq 'func ApplyPolicy(' "$MODULE" ||
    fail "missing policy admission owner"
grep -Fq '"task_already_exists"' "$MODULE" ||
    fail "duplicate spawn is not distinguishable"
grep -Fq '"skipped"' "$MODULE" ||
    fail "normal skip is not distinguishable"
if grep -Eq 'action[[:space:]]+[A-Za-z]|subject[[:space:]]+[A-Za-z]|Promise|Generator|AbortSignal|scheduler' "$MODULE"; then
    fail "pure admission policy introduced authority or a second concurrency model"
fi
if grep -Eq 'policy[[:space:]]*:[[:space:]]*String' "$MODULE"; then
    fail "policy regressed to string dispatch"
fi

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_host_task_policy.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
FIXTURE="$WORK_DIR/main.pgy"

cat > "$FIXTURE" <<'PGY'
use host_task_slot;

func Main() -> Void
{
    let opened: HostTaskSlotTransition = HostTasks.Open("projection");
    let first: HostTaskTicket = opened.ticket;

    let skipped: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        opened.slot,
        HostTasks.SkipPolicy()
    );
    Log(skipped.status);
    Log(skipped.transition.applied);
    Log(skipped.transition.reason);
    Log(skipped.transition.slot.generation);
    Log(HostTasks.IsCurrent(skipped.transition.slot, first));

    let duplicate: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        skipped.transition.slot,
        HostTasks.SpawnPolicy()
    );
    Log(duplicate.status);
    Log(duplicate.transition.applied);
    Log(duplicate.transition.reason);
    Log(duplicate.transition.slot.generation);
    Log(HostTasks.IsCurrent(duplicate.transition.slot, first));

    let restarted: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        duplicate.transition.slot,
        HostTasks.RestartPolicy()
    );
    Log(restarted.status);
    Log(restarted.transition.applied);
    Log(restarted.transition.slot.generation);
    Log(HostTasks.IsCurrent(restarted.transition.slot, first));
    Log(HostTasks.IsCurrent(restarted.transition.slot, restarted.transition.ticket));

    let finished: HostTaskSlotTransition = HostTasks.PublishFinal(
        restarted.transition.slot,
        restarted.transition.ticket,
        "done"
    );
    let cleaned: HostTaskSlotTransition = HostTasks.Cleanup(
        finished.slot,
        finished.ticket
    );
    Log(HostTasks.Phase(cleaned.slot));

    let vacantSpawn: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        cleaned.slot,
        HostTasks.SpawnPolicy()
    );
    Log(vacantSpawn.status);
    Log(vacantSpawn.transition.applied);
    Log(vacantSpawn.transition.slot.generation);
    Log(HostTasks.IsCurrent(vacantSpawn.transition.slot, restarted.transition.ticket));
    Log(HostTasks.IsCurrent(vacantSpawn.transition.slot, vacantSpawn.transition.ticket));

    let vacantRestart: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        cleaned.slot,
        HostTasks.RestartPolicy()
    );
    Log(vacantRestart.status);
    Log(vacantRestart.transition.applied);
    Log(vacantRestart.transition.slot.generation);
    Log(HostTasks.IsCurrent(vacantRestart.transition.slot, restarted.transition.ticket));
    Log(HostTasks.IsCurrent(vacantRestart.transition.slot, vacantRestart.transition.ticket));

    let vacantSkip: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        cleaned.slot,
        HostTasks.SkipPolicy()
    );
    Log(vacantSkip.status);
    Log(vacantSkip.transition.applied);
    Log(vacantSkip.transition.slot.generation);
    Log(HostTasks.IsCurrent(vacantSkip.transition.slot, restarted.transition.ticket));
    Log(HostTasks.IsCurrent(vacantSkip.transition.slot, vacantSkip.transition.ticket));

    let malformed: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        HostTaskSlot("malformed", 7, 99, "", ""),
        HostTasks.RestartPolicy()
    );
    Log(malformed.status);
    Log(malformed.transition.applied);
    Log(malformed.transition.reason);
    Log(malformed.transition.slot.generation);
    Log(HostTasks.Phase(malformed.transition.slot));
    Log(HostTasks.IsCurrent(
        malformed.transition.slot,
        malformed.transition.ticket
    ));

    let invalidGeneration: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        HostTaskSlot("zero", 0, 1, "", ""),
        HostTasks.SkipPolicy()
    );
    Log(invalidGeneration.status);
    Log(invalidGeneration.transition.applied);
    Log(invalidGeneration.transition.reason);
    Log(invalidGeneration.transition.slot.generation);
    Log(HostTasks.IsCurrent(
        invalidGeneration.transition.slot,
        invalidGeneration.transition.ticket
    ));

    let negativeGeneration: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        HostTaskSlot("negative", -1, 4, "", ""),
        HostTasks.SpawnPolicy()
    );
    Log(negativeGeneration.status);
    Log(negativeGeneration.transition.applied);
    Log(negativeGeneration.transition.reason);
    Log(negativeGeneration.transition.slot.generation);

    let exhaustedRestart: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        HostTaskSlot("max-active", 2147483647, 1, "", ""),
        HostTasks.RestartPolicy()
    );
    Log(exhaustedRestart.status);
    Log(exhaustedRestart.transition.applied);
    Log(exhaustedRestart.transition.reason);
    Log(exhaustedRestart.transition.slot.generation);

    let exhaustedSkip: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        exhaustedRestart.transition.slot,
        HostTasks.SkipPolicy()
    );
    Log(exhaustedSkip.status);
    Log(exhaustedSkip.transition.applied);
    Log(exhaustedSkip.transition.reason);
    Log(exhaustedSkip.transition.slot.generation);

    let exhaustedVacant: HostTaskPolicyDecision = HostTasks.ApplyPolicy(
        HostTaskSlot("max-vacant", 2147483647, 4, "", ""),
        HostTasks.SpawnPolicy()
    );
    Log(exhaustedVacant.status);
    Log(exhaustedVacant.transition.applied);
    Log(exhaustedVacant.transition.reason);
    Log(exhaustedVacant.transition.slot.generation);
}
PGY

EXPECTED="$WORK_DIR/expected.txt"
cat > "$EXPECTED" <<'OUT'
skipped
false
skipped
1
true
rejected
false
task_already_exists
1
true
restarted
true
2
false
true
vacant
started
true
3
false
true
started
true
3
false
true
started
true
3
false
true
rejected
false
invalid_phase
7
invalid
false
rejected
false
invalid_generation
0
false
rejected
false
invalid_generation
-1
rejected
false
generation_exhausted
2147483647
skipped
false
skipped
2147483647
rejected
false
generation_exhausted
2147483647
OUT

fixture_arg="$(pgy_path_for_compiler "$PGY" "$FIXTURE")"
for backend in ${PGY_HOST_TASK_POLICY_BACKENDS:-c llvm}; do
    actual="$WORK_DIR/$backend.out"
    raw="$WORK_DIR/$backend.raw"
    if ! (cd "$ROOT_DIR" && "$PGY" "$fixture_arg" --backend="$backend" --run) >"$raw" 2>&1; then
        sed -n '1,180p' "$raw" >&2
        fail "backend=$backend compile/run failed"
    fi
    tr -d '\r' < "$raw" | sed \
        -e '/^0 error(s), 0 warning(s)$/d' \
        -e '/^pgy: compiled/d' \
        | awk 'seen || length($0) > 0 { print; seen = 1 }' > "$actual"
    if ! cmp -s "$EXPECTED" "$actual"; then
        diff -u "$EXPECTED" "$actual" >&2 || true
        fail "backend=$backend output mismatch"
    fi
    echo "[host-task-policy] backend=$backend typed admission parity locked"
done
