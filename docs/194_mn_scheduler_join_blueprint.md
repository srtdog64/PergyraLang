# M:N scheduler join blueprint (WO-MN-1)

Status: R0–R2 LANDED 2026-07-21 (rename; materialization into both
linked-runtime objects and the bitcode twin; MovableScheduler dispatch backed
by the M:N worker set at run-to-completion depth). R3 remains a surface
decision. Mandate: BDFL 2026-07-21 ("전부 시작해").

**Measured correction from the first run (2026-07-21):** the fiber context
core was never runnable. `pgy_mn_fiber_create` saved a setjmp context in its
own stack frame and "resumed" it after the frame had returned, and the
assembly `pgy_mn_fiber_switch_context` restored a context that create never
seeded (no entry trampoline on the allocated fiber stack) — the first
dispatch segfaulted immediately. The census claim "a completed M:N scheduler
with no caller" therefore inverts on one axis: the queues, workers, stealing,
parking, and lifecycle were real; the per-fiber context switch was a design
document in code. R2 landed at **run-to-completion depth**: workers run each
submitted routine directly on the worker stack (submitted movable tasks never
yield mid-body), the unseeded context switch is no longer engaged, and a
fiber that reports a yield fails closed. **WO-MN-2 — the context layer** — is
the follow-on rung: seed a real entry trampoline (the in-house precedent is
the coroutine layer's CreateFiber/SwitchToFiber path) so fibers can start on
their own stacks and yield; only then do Blocked/Suspended become schedulable
states and the io/timer surfaces become reachable.

## Destination

`src/runtime/async/` — the completed M:N fiber scheduler (scheduler.c,
fiber.c, concurrent_queue.c, scheduler_fiber_ops.c; `_WIN32` fiber paths
present) — becomes the **dedicated executor behind the MovableScheduler
lane** of the SEA lane facade in *emitted programs*. This is the SEA design's
own sentence: M:N is not the centre of the language, it is one lane's
backend, unlocked by evidence (docs/146). Today the scheduler is compiled
into the compiler binary with zero callers; the reachability contract pins
that state as `declared_only` and this workstream is the declared way it
flips.

## Measured inventory (2026-07-21)

These are the facts a landing must respect; each was verified, not assumed.

1. **Symbols are pre-house-style.** The public API is PascalCase
   (`SchedulerCreate`, `SchedulerSpawn`, `SchedulerYield`, ...). The
   reachability row originally pinned `pgy_scheduler_create`, a symbol that
   does not exist — discovering that added the home-existence check to the
   census gate. Renaming is therefore rung R0, and it is riskless: there are
   zero consumers to break.
2. **Link topology.** `ASYNC_SOURCES` feed `RUNTIME_SOURCES` → objects in
   the *compiler binary only*. Emitted programs link a different runtime
   family with **three materializations**:
   - the shared separately-compiled runtime object
     (`compiler_runtime_cache.c` builds it from `pgy_runtime_lib.c`; both
     the LLVM leg and the C extern leg link it — one recipe, no drift);
   - the optional bitcode mode (`PGY_RUNTIME_BC`, `build_runtime_bc.sh`,
     gated by `backend-compare-bc-on`);
   - the legacy inline mode (`PGY_RUNTIME_INLINE` opt-out), where the
     emitted TU re-parses the runtime headers and links no object.
   Any symbol the lane dispatch references must resolve in **all three**, or
   fail closed observably in the ones it does not support.
3. **State discipline.** The scheduler is heavily stateful (worker table,
   global queue, park mutex, TLS current-scheduler). docs/190 measured what
   half-migrated singletons do (two-instance classes); every global must land
   single-homed under the `PGY_RT_GLOBAL` discipline or stay object-local.
4. **Traffic prerequisite is a surface decision, not plumbing.** Measured
   dead-end: intent clauses reject `spawn`/`await` as control-transfer
   constructs, and authority names attach only to zone/world boundaries — so
   no evidence plumbing can classify a real-source spawn as
   `MovableScheduler` today. The unblocking surface is the docs/181
   `on (lane)` reactive-block rung R3 (or an equivalent declared authority
   marker on spawn sites), which is a BDFL surface decision. Until then the
   lane is dispatchable (the verified spawn-lane plan can carry it, the
   facade runs it) but no real program produces it.
5. **Cancellation is cooperative on every lane** (landed 2026-07-20): the
   M:N executor must run tasks under the same run protocol — set the
   cancel-scope TLS around `fn`, publish `result` + `DONE` + broadcast — so
   await/cancel machinery and the executor-invariance contract hold
   unchanged.

## Rung ladder

Each rung is separately landable, separately gated, and leaves no
half-state. A rung that cannot meet its exit gate does not land.

### R0 — rename into the house family (safe now)

- `SchedulerCreate` → `pgy_mn_scheduler_create` family; C# PascalCase
  comments/naming converted to house style; `async_runtime.h`'s parallel
  AsyncTask universe either renamed with intent or explicitly excluded from
  the public seam.
- Zero consumers ⇒ zero behavioural risk. Compiles in the compiler binary
  as before.
- Exit gates: build green; naming-contract grep (no `SchedulerCreate`
  remains); reachability row symbol updated in the same commit (the
  home-existence check forces this).

### R1 — materialize into the emitted-program runtime family

- Extend the shared runtime-object recipe so the M:N core compiles into the
  linked runtime (second object or source aggregation — decided by measuring
  the object build time), and `build_runtime_bc.sh` llvm-links the same core
  into the bitcode twin.
- **Inline-mode design decision (the one open design question):** the lane
  dispatch body must not create an unresolvable symbol reference in inline
  TUs. Options:
  (a) weak/registration indirection — rejected: hidden variance between
      materializations, ctor magic;
  (b) **compile-time fail-closed (recommended)** — in inline runtime mode
      the backend refuses a `MovableScheduler` spawn-lane row with a
      diagnostic ("movable lane requires the extern runtime"); observable,
      cheap, and inline mode is the legacy opt-out path;
  (c) include the core in the inline header set — rejected: re-parses ~3k
      more lines per TU, reopening the docs/189 C14 compile-speed closure.
- Exit gates: `runtime-bc-contract`, `runtime-cext-contract`, inline-mode
  refusal witness, and a link witness on both legs.

### R2 — dispatch joins the executor

- `pgy_lane_spawn_dispatch` `MOVABLE_SCHEDULER` case routes to the M:N
  executor through a run-protocol shim (cancel-scope TLS + result/DONE
  broadcast), so `pgy_await`/cancel work unchanged. Lazy singleton init
  under the single-home discipline; shutdown joined at exit.
- `lane_executor_contract_owner.pgy` drifts by design: MovableScheduler
  moves from `worker_join_scaffold` to a dedicated executor row.
- Exit gates: executor-invariance witnesses (same results pool vs M:N on
  the same task set, including cancel-during-queue), lane-scheduler smoke
  extended to engage the real executor, join/cancel battery, TSan lane.
- The reachability row `async_fiber_scheduler` flips to `live` in this
  rung's commit — the census forces the same-commit promotion.

### R3 — surface evidence connects (BDFL gate)

- The `on (lane)` surface (docs/181 R3) or an equivalent declared marker
  produces `MovableScheduler` classifications from real source; the verified
  spawn-lane plan carries them; fixtures land in the SEA golden and
  backend-compare corpus.
- Exit gates: lane golden gains a real Movable row produced by source
  evidence (never synthetic AIR state); backend compare on the new fixtures.

## Non-goals and refusals

- No session-tail half-migration: R1..R2 land only with their exit gates in
  the same commit (docs/190 records what the alternative costs).
- No test-as-consumer promotion: the census excludes the test harness, so
  the reachability row cannot be flipped by writing a unit test.
- No M:N-by-default: rule (5) of the lane policy stays the strictest gate —
  M:N remains an evidence-unlocked optimization, never a default.

## Appendix — R3 surface proposal (BDFL decision, 2026-07-21 draft)

The traffic prerequisite is a DECLARED surface from which the classifier can
derive `crosses_authority_boundary` for a spawn site. Canon constraint
(docs/181): the user declares **authority**, never a lane — `spawn movable`
would be manual placement and is rejected up front. Three shapes were
compared:

- **(A) Role reactive block** (`parallel on (lane) { every(d) ... }`,
  docs/181): the destination surface. The role is itself an
  authority-bearing participant, so the block supplies both the declared
  eligibility AND the authority context; the classifier still verifies the
  body's capture evidence (declaration is checked, not trusted). Cost: rides
  the docs/181 rung ladder (duration literals, virtual clock, every/
  continuous) — not a quick slice.
- **(B) Spawn authority clause — recommended first producer:**
  `spawn F(x) by <participant>` (vocabulary mirrors the intent step's
  `authorized by:`). Declares the authority participant at the spawn site;
  AIR copies it onto the spawn boundary exactly as intent steps do for
  zone/world boundaries; rule (5) then requires the FULL conjunction as
  today (declared authority alone never reaches Movable without value-only
  capture evidence — the declaration adds a conjunct, it cannot override
  one). Parser cost is one optional suffix clause; no new evidence rules.
- **(C) Zone-scoped inheritance** (spawns inside an authority-bearing zone
  inherit its authority): no new syntax, but implicit propagation to every
  spawn in the zone is the same over-pin family docs/146 already rejected
  for routine-level correlation. Rejected.

Recommendation: land **B** as the minimal Movable producer (unblocks real-
source traffic, the lane golden, and a backend-compare fixture through the
M:N executor), keep **A** as the destination that subsumes B for reactive
workloads. Syntax is a BDFL call; nothing here lands without it.

## Contract owners this workstream touches

| Owner | Drift expected at |
|---|---|
| `src/self_hosted/parallel/lane_policy_owner.pgy` | none (reachable set already includes Movable) |
| `src/self_hosted/parallel/spawn_lane_plan_owner.pgy` | none (plan already carries Movable rows) |
| `src/self_hosted/sea/lane_executor_contract_owner.pgy` | R2 (scaffold → dedicated) |
| `src/self_hosted/compiler/reachability_owner.pgy` | R0 (symbol), R2 (declared_only → live) |
| `docs/146` Remaining | R3 (Movable produced from real source) |
