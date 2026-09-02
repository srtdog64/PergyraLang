# Spawn Runtime Context Propagation

Status: ACTIVE — LOCAL GREEN, PUBLICATION/EXACT CI PENDING

Exact base revision: `a6ba51a996438c7de089a6cd495b20b6a6b74844`

This directive coordinates the first executable rung adopted by
`docs/204_concurrency_direction_pscc_review.md`. It is temporary coordination
evidence, not semantic authority, a SoT row, or a completion claim.

## Shared objective card

- Objective: every task created through the lane facade captures the currently
  bound runtime capability/budget context and executes under that captured
  context instead of a worker thread's process-default TLS context.
- Priority order: capability non-forgery; one shared quantitative budget owner;
  exact context restoration across nested help and coroutine suspension;
  executor parity; patch size.
- Fact owner: `src/runtime/pgy_runtime_context.h` owns bound context capture and
  the shared budget-state reference. `PgyTask` and `PgyCoroTask` are carriers,
  not independent policy owners.
- Production entrypoint: compiler-emitted `spawn` and parallel work reach
  `pgy_lane_spawn_dispatch`, then Inline/Pinned, BlockingPool, LocalAsync,
  WorkerPool, or MovableScheduler execution.
- Last legitimate consumers: `pgy_pool_run_task` for thread/M:N tasks and the
  coroutine scheduler switch boundary for LocalAsync tasks.
- Direct bypass to delete: worker/fiber execution calls `task->fn(task->arg)`
  while `g_pgy_runtime_context_current` still names the executor thread's
  default or preceding context.
- Forbidden fallback: rereading environment grants in a child, allocating a
  new independent budget counter set, widening the parent's manifest mask,
  binding only WorkerPool while leaving another lane on default TLS, leaving a
  child context bound after return/yield/await, source syntax or backend-local
  context transport, and a silent inline retry after context capture failure.
- Verification gate:
  `tests/runtime_spawn_context_propagation_smoke.sh`, plus existing runtime
  context, lane scheduler, M:N, capability, budget, cancellation, and backend
  parity gates.
- Falsifying cases: a parent bound to a non-default instance with an
  `io_read`-only manifest must observe the same instance/mask in every spawned
  lane; a child charge must increment the parent's exact budget state; a nested
  help-run child and a yielding LocalAsync task must restore the surrounding
  task/host context; a child must never observe `PGY_CAP_ALL` merely because a
  worker began with default TLS.

## Scope

- Runtime context owner and task/coroutine carriers.
- One focused executable probe and Make/CI aggregation wiring.
- Canonical concurrency documentation and current handoff/collaboration state.
- No new syntax, detach surface, capability-flow keyword, AIR lowering,
  scheduler choice, or independent budget authority.

## Local evidence

- `runtime-spawn-context-propagation-test-smoke`: inline, C-extern, and LLVM
  runtime materializations; six lane observations; nested help-first;
  LocalAsync suspension restore; restricted-parent child IO_WRITE denial.
- Existing runtime-context, lane-scheduler, M:N, full sandbox, runtime BC and
  C-extern contracts, parallel-production, memory-concurrency,
  runtime-authority, ABI-lifetime, worker-boundary, and documentation gates are
  green on the local Windows/MSYS2 host.
- Exact CI remains required. Local Rocq/Coq is unavailable and no formal proof
  pass is claimed by this runtime rung.
