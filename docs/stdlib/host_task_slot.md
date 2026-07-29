# Host Task Slot

`stdlib/host_task_slot.pgy` owns key-and-generation authority for a host
adapter that restarts tasks under a stable logical key. It is an immutable
state machine, not a scheduler, and does not replace Pergyra's existing
`spawn`/`await`, `Future<T>`, or runtime slot primitives.

## Contract

- `HostTasks.Open(key)` creates generation 1 in `running` phase.
- `HostTasks.Replace(slot, key)` accepts only the same non-empty key and
  advances the generation. Its returned ticket is the new publication token.
- `PublishWait` and `PublishFinal` require exact key-and-generation identity.
- `Cleanup` also requires the current generation to be in `final` phase;
  `applied == true` is permission for the host adapter to remove its entry.
- A rejected transition returns the input slot unchanged and a stable reason.
  There is no key-only fallback.

The host adapter may retain its existing runtime future beside a
`HostTaskTicket`, but must re-read the latest slot and apply a guarded
transition immediately before publication or cleanup. A task handle is absent
because Pergyra does not yet expose a non-generic first-class source task
handle suitable for this record. Promise, Generator, AbortSignal, a scheduler,
or a mailbox here would create a second concurrency model.

## Falsifying Case

Open `projection` (generation 1), replace it with the same key (generation 2),
then attempt generation-1 wait publication, final publication, and cleanup.
All three must be rejected with `stale_generation` while generation 2 remains
current. The generation-2 ticket must still complete waiting, final, and
cleanup. `tests/host_task_slot_smoke.sh` executes this case on C and LLVM.
