# Host Task Slot

`stdlib/host_task_slot.pgy` owns key-and-generation authority for a host
adapter that restarts tasks under a stable logical key. It is an immutable
state machine, not a scheduler, and does not replace Pergyra's existing
`spawn`/`await`, `Future<T>`, or runtime slot primitives.

## Contract

- `HostTasks.Open(key)` creates generation 1 in `running` phase.
- `HostTasks.ApplyPolicy(slot, policy)` is the single admission decision for a
  known slot. The policy is the typed `HostTaskApplyPolicy`, not a string.
- On an active slot, `skip` is a normal non-applied `skipped` decision and
  preserves the generation; duplicate `spawn` is a rejected
  `task_already_exists` decision and also preserves the generation; only
  `restart` advances the generation and returns `restarted`.
- On a cleaned/vacant slot, every known policy starts the next generation and
  returns `started`. Invalid phase or negative generation is rejected without
  changing the slot. Generation zero is also invalid because `Open` issues
  generation 1. A transition that would exceed the `Int` maximum is rejected
  as `generation_exhausted`; active `skip` remains a safe non-advancing no-op.
- `HostTasks.Replace(slot, key)` accepts only the same non-empty key and
  delegates its generation transition to restart policy. Its returned ticket
  is the new publication token.
- `PublishWait` and `PublishFinal` require exact key-and-generation identity.
- `Cleanup` also requires the current generation to be in `final` phase;
  `applied == true` is permission for the host adapter to remove its entry.
- `Phase` returns `invalid` for unknown phase facts; it never aliases malformed
  state to `vacant`. `IsCurrent` is true only for a known active phase.
- A rejected transition returns the input slot unchanged and a stable reason.
  There is no key-only fallback.

The host adapter may retain its existing runtime future beside a
`HostTaskTicket`, but must re-read the latest slot and apply a guarded
transition immediately before publication or cleanup. A task handle is absent
because Pergyra does not yet expose a non-generic first-class source task
handle suitable for this record. Promise, Generator, AbortSignal, a scheduler,
or a mailbox here would create a second concurrency model.

Admission is pure immutable policy computation, so this module deliberately
does not introduce `subject`, `action`, or `intent`. It also does not publish a
`tobject`: there is no detached host-boundary receipt lifecycle yet. A future
adapter may materialize a receipt only when a real handoff boundary owns and
consumes it.

## Falsifying Case

Open `projection` (generation 1), replace it with the same key (generation 2),
then attempt generation-1 wait publication, final publication, and cleanup.
All three must be rejected with `stale_generation` while generation 2 remains
current. The generation-2 ticket must still complete waiting, final, and
cleanup. `tests/host_task_slot_smoke.sh` executes this case on C and LLVM.

The policy falsifier applies `skip`, `spawn`, and `restart` in that order to
the same active generation. The first two must preserve both the slot and the
current ticket, while only restart invalidates the old ticket. After cleanup,
spawn must start the next generation; malformed phase/generation facts must be
rejected unchanged. `tests/host_task_policy_smoke.sh` executes the exact trace
on C and LLVM. `make stdlib-test-smoke` runs the unrelated aggregate stdlib
surface first, then both stable-`use host_task_slot;` lifecycle and policy
gates. `make host-task-slot-test-smoke` and
`make host-task-policy-test-smoke` expose the focused targets directly.
