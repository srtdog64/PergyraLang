# ContentInstance runtime context — first executable rung

## Objective card

- Objective: stop capability and quantitative budget state from being presented
  as a process-global content grant when a host binds more than one content
  instance in one process.
- Owner: `src/runtime/pgy_runtime_context.h` owns the context ABI and TLS
  binding; the capability and budget gates consume its current slot.
- Last consumer: C inline runtime gates and the LLVM-linked runtime export
  wrappers (`pgy_cap_*_export`, `pgy_budget_*_export`).
- Forbidden fallback: a zero-initialized or unbound context, or a second
  `g_pgy_*` singleton that can disagree with the bound instance.
- Gate: `runtime-context-test-smoke` plus C capability/budget enforcement and
  C/LLVM source-policy parity when the LLVM toolchain is available.

## ABI

`PgyRuntimeContext` carries the capability mask pair, a root `PgyBudgetState`,
a reference to the effective budget owner, and a host-supplied `instance_id`.
The host must call:

```c
PgyRuntimeContext ctx;
pgy_runtime_context_init(&ctx, instance_id);
if (!pgy_runtime_context_bind(&ctx)) { /* reject */ }
```

`pgy_runtime_context_current()` is TLS-backed. `pgy_runtime_context_unbind()`
returns to the trusted default context (`instance_id == 0`). An uninitialized
context cannot be bound, so missing initialization fails closed before a gate
can observe a zeroed budget or grant.

The C-inline and LLVM-linked implementations use the same header-level policy;
the LLVM file supplies ABI wrappers and does not own a parallel capability or
budget global. `tests/runtime_context_smoke.c` proves that two bound root
contexts retain independent manifests and counters.

## Spawn carriage

`pgy_runtime_context_capture_task()` snapshots the bound capability masks and
instance identity when a lane task is created, while sharing the root
context's exact `budget_owner`. Every execution lane binds the captured task
context before user code and restores the surrounding TLS afterward. Local
coroutines perform the same handoff at yield/await suspension boundaries.

The executable owner gate is
`make runtime-spawn-context-propagation-test-smoke`. It covers Inline,
PinnedZone, BlockingPool, LocalAsync, WorkerPool, and MovableScheduler in the
inline, C-extern, and LLVM runtime materializations, including nested
help-first execution. A new task-local budget or worker-default capability
grant is a fail-closed regression, not a fallback.

## Boundary that remains open

This rung does not claim complete multi-tenant sandboxing. Capability and
budget authority cross runtime lane tasks, and the named-spawn surface now
keeps the parent context live indirectly by rejecting a live Future at lexical
or function exit. That containment is owned by
`type_checker_future_lifecycle.c`, not by the unused runtime `AsyncScope` API,
and it deliberately provides no implicit drain fallback. Cancellation roots,
scheduler instances, unsupported detached tasks, asset namespaces, linear
memory, random streams, and diagnostics still have process/TLS owners or
incomplete cross-instance evidence. They require their own carriage and
negative tests before `ContentInstance` can be called a complete isolation
boundary. The current context is therefore a runtime ownership rung, not a
release-security claim.
