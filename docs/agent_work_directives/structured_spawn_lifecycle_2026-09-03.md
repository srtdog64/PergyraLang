# Structured Spawn Lifecycle

Status: DONE — PUBLISHED AND EXACT CI GREEN

Exact base revision: `a1d3e116f40566f164aef98ba98b931dc01cbe5f`

Published revisions: `cf66092b594f9e83525d3df9da68e56e7446186f`,
`74416398f2e53b02a7f60424e477255c0c524f9d`

Exact replacement CI: `33708971493`, 30/30 green in 36m28s.

This directive coordinates the lifecycle rung adopted by
`docs/204_concurrency_direction_pscc_review.md`. It is temporary coordination
evidence, not compiler semantics, a SoT row, or a completion claim.

## Shared objective card

- Objective: no `Future<T>` or `RemoteFuture<T>` completion handle created or
  owned by a lexical/function scope may disappear while still live. Every
  normal exit must observe one completion join or an explicit ownership
  transfer to an `own Future<T>` parameter.
- Priority order: task containment; path-sensitive branch/loop correctness;
  cancellation-request versus completion separation; affine handle identity;
  minimal surface.
- Fact owner: the semantic symbol's future lifecycle state, carried through
  `ResourceConsumeSnapshot`. `Future<T>` and `RemoteFuture<T>` values carry the
  fact; runtime task records and backends do not independently decide whether
  a source scope retired its handle.
- Production entrypoint: the native semantic pass reached by ordinary
  compiler emission. Direct let-spawn, immediate await-spawn, named await,
  return, lexical scope exit, function exit, and owned parameter transfer are
  the bounded consumers.
- Last legitimate consumers: return-flow checking and the lexical/function
  scope boundary immediately before its symbols are destroyed.
- Direct bypass to delete: a `LIVE` future vanishes when `scope_exit()` frees
  its symbol, or branch `is_consumed` OR-merge treats one-path-only await as a
  total join.
- Forbidden fallback: runtime `AsyncScope` as a second lifecycle authority;
  implicit cancel/drain/finalizer insertion; treating `Cancel` as completion;
  backend cleanup; a function-only scan after nested symbols are destroyed;
  branch OR false closure; borrowed/default Future parameters; return escape;
  a new `async scope` or detach syntax.
- Verification gate: `make structured-spawn-lifecycle-test-smoke`, followed by
  semantic, diagnostic-registry, memory/concurrency, documentation, and
  backend fixture parity gates.
- Falsifying cases: `let pending = spawn Worker(); return;`, a nested block
  that drops `pending`, cancel-only, bare spawn, mutable Future, Future alias,
  loop break/zero-iteration loss, and a branch that awaits on only one normal
  path must fail before codegen with `PGY_SEM_TASK_LIFECYCLE`; use after an
  `own` transfer must fail with `PGY_SEM_MOVE_FROM_RELEASED`; an `own Future`
  or `own RemoteFuture` parameter that falls through live must also fail;
  cancel-then-await,
  both-branch await, a single parallel arm join, inline await-spawn, and
  `own Future<T>`/
  `own RemoteFuture<T>` transfers whose callees await must pass; static
  `if true`/`if false`, unreachable returns and loop exits, exact zero- and
  one-iteration ranges including `continue`, `while true` plus `break`, and
  literal `match` must exclude impossible zero/alternate states. Repeated use
  of one unavailable handle must emit one owned lifecycle/move diagnostic,
  not a secondary type-mismatch cascade. Three ABI
  transfer fixtures, including same-named local/remote parameters in one
  program, execute with exact expected output on the production C and LLVM
  backends; two parallel arms joining one handle must fail as a resource
  conflict.

## Scope

- Semantic lifecycle state, flow snapshot carriage, alternative and parallel
  merge rules, lexical/return/function boundary consumers, and Future
  ownership transfer.
- Focused production-entrypoint fixtures and Make/CI aggregation wiring.
- Canonical async/concurrency contract, diagnostic registry, review
  correction, and current handoff/collaboration state.
- No new source syntax, detach capability, backend lifetime policy, runtime
  scheduler change, AIR dependency, or general query/cache architecture.

## Local evidence

- `test-semantic`: 2863 passed, 0 failed. This includes the CFG distinction
  between a reachable value-return fallthrough and a statically non-returning
  infinite loop.
- `structured-spawn-lifecycle-test-smoke`: 19 positive and 18 fail-closed
  fixtures; C/LLVM exact runtime output, bounded execution, and stable JSON
  diagnostic identity.
- Diagnostic registry, semantic owner/size gates, CFG body dataflow, loop and
  parameter summaries, ownership relocation, memory/concurrency, source
  inventory, compiler owner cluster, build-pressure contract, post-selfhost
  manifest, the full LLVM smoke suite, and `test-all` are green. Legacy
  transpiler, RIR, and MIR fixtures that discarded a bare/borrowed Future were
  migrated to immediate await or an explicit `own` parameter without changing
  the downstream fact each fixture observes.
- The repository-wide backend-include and production-C size gates remain
  omitted here because their unrelated opening-base violations are
  `src/runtime/pgy_runtime_lib_io_string_exports.h` 618/600 and
  `src/parser/ast_expr_control_accessors.c` 725/699. This lifecycle rung does
  not change either file.
