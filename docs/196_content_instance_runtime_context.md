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

`PgyRuntimeContext` carries the capability mask pair, `PgyBudgetState`, and a
host-supplied `instance_id`. The host must call:

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
budget global. `tests/runtime_context_smoke.c` proves that two bound contexts
retain independent manifests and counters.

## Boundary that remains open

This rung does not claim complete multi-tenant sandboxing. Cancellation roots,
schedulers, task handles, asset namespaces, linear memory, random streams, and
diagnostics still have their own process/TLS owners. They require explicit
context-carriage and cross-instance negative tests before `ContentInstance` can
be called a complete isolation boundary. The current context is therefore a
runtime ownership rung, not a release-security claim.
