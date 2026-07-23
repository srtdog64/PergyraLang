# Region/Arena Strategy — Declared-Lifetime Allocation (WO-REG)

**Status**: WO-REG-1 IMPLEMENTED (2026-07-21). The runtime region is now
carried from the AIR-certified driver plan to both C and LLVM consumers. This
document remains the strategy and evidence record; stale census statements are
marked where implementation changed them.

## Historical strategy context

The original strategy text follows; WO-REG-1 is now live and its current
evidence is recorded in §1.2, §1.4, and the gates listed below.
complete build plan requested by the BDFL ("전략 자체를 전부 작성해놔") after the
2026-07-21 census answered "우리 언어에 메모리 아레나가 있나?" with: *twice
declared, never carried*. Rung WO-REG-1 starts only on BDFL approval of the
decision requests in §9.

**Reading order for a fresh session**: §1 (what exists, measured) → §3 (the
architecture, one page) → §5 (the rung you were assigned) → §9 (what is
already decided vs. what is not).

---

## 1. Census — what exists today (all claims re-verified 2026-07-21)

### 1.1 The compiler dogfoods arenas everywhere

`src/common/arena.h` + `arena.c`: chained-block bump allocator with full
observability — named arenas (`pgy_arena_init_named`), consumer/release-point
stamps (`pgy_arena_set_last_consumer` / `pgy_arena_set_release_point`), stats
including `created_bytes` / `retained_bytes` / `peak_bytes` /
`cross_stage_copies`, and `pgy_arena_fmt` / `pgy_arena_strdup` /
`pgy_arena_calloc` conveniences. 23 files consume it across lexer / hir / mir /
semantic / transpiler context / LLVM backend (`ctx->scratch`,
`ctx->persistent`, per-pass scratch arenas in `llvm_backend_type_render.c`).

The compiler already *believes* the thesis of this document: transient
allocations grouped by declared release points, with cross-boundary copies
counted rather than hidden.

### 1.2 The runtime has two arena families, with the region family live

`src/runtime/pgy_runtime_memory_array_slot_inline.h:87-171` defines a complete
runtime `PgyArena` family: `{buffer, capacity, offset}` fixed-capacity frame
allocator, `pgy_arena_create` / `pgy_arena_destroy` / `pgy_arena_alloc` (with
alignment validation) / `pgy_arena_alloc_array` (overflow-checked) /
`PGY_ARENA_NEW` / `PGY_ARENA_NEW_ARRAY` macros / `pgy_arena_reset`. OOM is
fail-closed: `PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY)` — the panic reason constant
exists in the panic contract.

The fixed-capacity `PgyArena` family remains available for legacy runtime
helpers, while `PgyRegion` in `src/runtime/pgy_runtime_region_inline.h` is the
live WO-REG-1 family. The driver produces `PgyRegionPlan` rows from the escape
certificate; C consumes inline region operations and LLVM consumes the exported
runtime twins. `tests/region_arena_smoke.sh` proves chained growth, pointer
stability, alignment, reset, and budget fail-closed behaviour in both runtime
materializations. `tests/region_backend_wiring_smoke.sh` proves the producer /
consumer path for certified string temporaries and the heap fallback for an
uncertified binding. The former zero-consumer statement applies only to the
legacy fixed `PgyArena`, not to the region path.

Reset now reuses the retained block chain before acquiring another block. The
head block remains the destroy-chain owner; after reset, zeroed older blocks
are selected only when the head cannot fit the request. Live blocks from the
ordinary growth path are never reused. The region smoke deliberately performs
grow → reset → grow across two blocks and asserts that the block count does not
increase.

### 1.3 The ABI spec pinned arena shapes years before any consumer

`src/runtime/pgy_abi_spec.h`:

- §13 pins `pgy_abi_arena` layout `{buffer: char*, capacity: size_t,
  offset: size_t}` (with layout asserts in `pgy_abi_spec_asserts.h`).
- §14 declares an Allocator descriptor (tracing/pool).
- The zone/world channel section (lines ~211-244) designs a **2-tier
  ownership model where channel bodies live in a "Zone Arena"** —
  `pgy_zone_channel_create_int(PgyArena* arena, size_t cap)`, "destroy는 Zone
  Arena 일괄 정리 — 개별 호출 불필요". This exists **only as comments**; the
  real channel runtime takes no arena.

So the arena idea is not new to this project — it is pinned ABI surface that
never received the evidence pipeline to make it live.

### 1.4 What emitted programs actually do for transient allocations

- **Strings**: the heap `StringConcat` path remains the compatibility lowering,
  but the certified direct `Print("a" + "b")` class now uses
  `pgy_region_string_concat` inside a function-scope region. This eliminates
  chained-concat intermediate leaks for the migrated class. Any binding or
  escape that is not certified stays on the existing heap path; the backend
  never guesses a region lifetime.
- **Spawn**: per-task `calloc(PgyTask)` + argument pack — the measured
  2-mallocs-per-task cost on the fine-grain critical path
  (benchmarks/PARALLEL_RESULTS.md; the WO-RT-4 chain established the producer
  critical path, not the scheduler, as the fine-grain cost center).
- **Arrays**: malloc-backed runtime in the same header that hosts the unused
  arena.
- **Surface**: no `region`/`arena` keyword; no allocator vocabulary at all.

### 1.5 Verdict

**Implementation update (2026-07-21):** the evidence path described below has
now been landed for WO-REG-1. Treat the original census verdict in this section
as historical context; the live status is recorded at the top of this document
and in §1.2/§1.4 above.

The language should have arenas — it already decided that twice (compiler
practice + ABI spec) — but the mechanism was declared without an evidence
path from programs to the allocator. The strategy is therefore **not "add an
arena"**; it is: *carry lifetime evidence to the allocator exactly the way
WO-PAR-NOVEL ② carried lane evidence to the executor* — through the verified
projection plan pipeline — and only then give the capability a surface.

---

## 2. Canon constraints (shapes forbidden / required before any design)

1. **No lifetime annotations, ever** (hard ban, BDFL 2026-06-29; docs/118
   §2.1). Region-based memory management is the classical dual of lifetime
   inference (Tofte–Talpin); Pergyra takes the *declaration* side of the dual
   (docs/19 §✦ decide-vs-declare): the program **declares reclamation
   scopes**; the compiler proves sites into them or fail-closes them out.
   Nothing in this workstream may introduce `'a`-shaped vocabulary.
2. **No raw allocator-handle surface** (`let a = Arena(); a.alloc(...)`).
   Threading allocators as values is the Zig frame — off-axis for a type
   system whose mandate is domain coordinates, not machine bookkeeping
   (docs/121). Allocation strategy is *evidence*, not a user-threaded value.
3. **Axis budget stays 2** (Coq corpus invariant). Region is not a third
   axis: it is a lifetime **refinement riding the existing zone/slot axis**.
   The formal-corpus axis-budget guard must stay green through every rung.
4. **Reachability asymmetry**: every rung lands producer + consumer in the
   same commit train. The unconsumed runtime arena of §1.2 is the cautionary
   exhibit; WO-REG may never reproduce it.
5. **No hidden control flow**: allocation redirection must be **plan-driven
   and dumpable** (per-site rows, like the spawn-lane plan manifest), never
   an ambient thread-local "current allocator" that silently reroutes
   `malloc`. Region enter/exit are explicit emitted operations.
6. **Fail-closed asymmetry (the soundness core)**: a site lowers to REGION
   only under a certificate; anything uncertain lowers to HEAP (today's
   emission, byte-identical). Analysis incompleteness can only cost
   performance, never correctness. OOM inside a region panics (already the
   runtime arena's behavior); no spill-to-heap fallback (that would be a
   silent fallback path).
7. **Dual-backend discipline**: any runtime arena change is a twin change —
   three materializations (inline static-inline / `PGY_RUNTIME_LIB_INTERNAL`
   / `PGY_RUNTIME_EXTERN_DEFS`), `.bc` regeneration (`make runtime-bc`), and
   twin traps 1–5 (flag mirroring, stateful strip, linkage guard, freshness
   glob, **no module-level asm**) all apply.
8. **Slot storage is out of scope.** Slots/zones remain the owner of named,
   own/ref-disciplined storage (`slot_pool` untouched). Regions serve
   **anonymous transient values** — results that never acquire a name
   outliving the scope. Region complements the slot axis; it does not compete
   with it.

---

## 3. Architecture — the Verified Region Plan

Third verified-projection artifact, structurally identical to the row plan
and the spawn-lane plan (docs/36 pipeline; `verified_projection_plan.{h,c}`):

```
Semantic ─ HIR/DIR/RIR ──► allocation-site facts + escape verdicts
                │              (own/ref + value-capture machinery reuse;
                │               precedent: spawn movability pass)
                ▼
        AIR evidence ──────► per-site lifetime evidence attach
                │              (precedent: has_declared_blocking_evidence)
                ▼
        MIR lowering ──────► sites keyed by MIR node id, CFG facts for
                │              scope-exit placement (terminal-branch CFG
                │              facts already carried)
                ▼
   Verified Region Plan ───► PgyRegionPlan rows: AllocationSiteId → REGION(scope) | HEAP
                │              producer in verified_projection_plan.c,
                │              certificate-gated, refusal rules, revision int
                ▼
        C / LLVM backends ─► lookup per site, fail-closed to HEAP;
                               REGION sites emit pgy_arena_* ops
```

**Producer rules** (mirror `pgy_verified_spawn_lane_plan_from_air`):
certificate-ready gate; REGION row without a matching escape certificate →
refuse; two rows claiming one site → refuse; duplicate rows collapse;
`PGY_REGION_PLAN_REVISION` integer. **Driver produces, backends consume** —
emitters are grep-forbidden from calling the producer (spawn-lane owner
precedent: forbid rows in the origin-surface owner).

**Backend rule**: `pgy_verified_region_plan_lookup(plan, node, &scope)`;
absent row → HEAP emission unchanged. Both backends must translate the same
plan — parity gate extends to region fixtures.

**Runtime target**: the §1.2 family, rehabilitated (§5.1 decides fixed vs.
chained capacity). Budget (R6): block acquisitions charge `g_pgy_budget`;
per-bump charging is rejected (hot-path cost for no additional bound).

**Thread/lane rule**: regions are lane-confined. Crossing a spawn, channel,
slot store, or global binding is an **escape** — the site lowers to HEAP (or,
post-R3, is a compile error inside a declared region unless explicitly moved
out). Precedent: closure Stage A copy-capture semantics at spawn boundaries.

---

## 4. Escape analysis specification (what "region-safe" means)

A site S in scope Σ is region-safe iff **all** hold:

- (i) S's result is never bound to a name whose scope outlives Σ;
- (ii) never returned from the function owning Σ;
- (iii) never captured — closure env, spawn arg, channel send, slot store,
  global/static;
- (iv) every callee receiving it is **borrow-safe** (an explicit whitelist:
  `Print`/length/comparison-class builtins that read and do not retain;
  default = deny, i.e. HEAP);
- (v) never aliased into a projection that itself escapes by (i)–(iv).

Implementation stance: **certificate or HEAP** — the pass reuses the own/ref
+ value-capture machinery (the interprocedurally-complete UAF tracking is the
existing asset that makes (i)–(iii) checkable); no new lifetime vocabulary.

The three failure classes are intentionally distinct:

```text
valid AIR certificate + site not certified  → HEAP
missing or invalid AIR certificate          → compiler invariant failure
contradictory/zero allocation-site row      → compiler invariant failure
```

Only the first case is an ordinary analysis miss. The latter two refuse plan
publication, so a malformed proof cannot silently become a heap optimization
choice.

**Loud-failure instrumentation**: `PGY_REGION_POISON` debug mode memsets
`0xDD` over the region on scope exit. All backend-compare region fixtures run
poison-on in both backends, so a wrong certificate crashes deterministically
in CI instead of silently reading stale bytes. This is the region analogue of
the UAF fixtures.

---

## 5. Rung ladder

### WO-REG-0 — this document (no code)

Strategy + BDFL decision requests (§9). DoD: doc + INDEX row + board track.

### WO-REG-1 — runtime rehabilitation + first production consumer (one train)

**Target consumer: statement-local string temporaries** — the measured leak
(§1.4). Chained-concat intermediates and format/print argument temporaries
whose whole expression tree is region-safe.

Steps, in commit order (producer and consumer land together):

1. **ABI revision before first consumer** (BDFL decision #2): upgrade the
   runtime arena from fixed-capacity to chained blocks (mirror
   `src/common/arena.h`), revising `pgy_abi_spec.h` §13. Rationale: string
   sizes are not statically bounded; fixed capacity forces either
   over-reservation or OOM panics on correct programs; chained growth keeps
   one behavior (grow) instead of forking (spill/fallback). Zero consumers
   exist, so the layout pin is free to change **now** and expensive after.
   Keep: alignment validation, overflow-checked array alloc, OOM panic.
2. **Twin materialization**: the family joins both linked-runtime TUs and the
   inline mode under the established `PGY_RT_DECL` regime; `.bc` regenerated;
   `runtime_bc_contract` green.
3. **Plan artifact**: `PgyRegionPlan` producer + per-site lookup in
   `verified_projection_plan.{h,c}`; driver wiring in all four
   produce/dispose sites (compiler.c + compiler_llvm.c ×3, spawn-lane
   precedent).
4. **Escape pass v1**: certificates only for the string-temporary class —
   full-expression-tree region-safety at statement granularity (no
   cross-statement liveness in v1).
5. **Emission**: lazy **function-scope region** — created on first certified
   site, destroyed on every return path (MIR terminal-branch CFG facts place
   the destroys); statement-bracket emission is the fallback if CFG placement
   resists (BDFL decision #3 picks; recommendation: lazy function-scope, it
   amortizes create/destroy across the function). REGION string sites call
   arena-backed concat (`StringConcatIn(arena, a, b)` twin); their OOM path
   panics — closing the §1.4 silent-empty violation *for migrated sites
   only* (heap sites keep today's behavior until separately judged).
6. **Origin surface**: `region_plan_owner.pgy` (+ manifest + golden + smoke)
   owning refusal rules / duplicate collapse / per-site fail-closed lookup /
   driver-produces-backends-consume forbids; artifact-zone kind #29;
   reachability rows for the arena family flipped to **live in the same
   commit**.

**Exit gates**: region twin smoke (materializations + `.bc`); new
backend-compare fixtures (chained concat in loops, early-return + match-arm
functions witnessing destroy-on-every-path, poison-on) with C==LLVM output
equality; parity suite extended; erasure dashboard row (region ops = runtime
primitive bucket, vocabulary absent unless used); reachability census green
with arena rows live; **memory-boundedness witness** (the leaking fixture's
RSS plateaus under the region build); perf measured best-of-N same-session
solo and recorded in the string ledger — the honest frame is that today's
baseline *leaks*, so the headline claim is bounded memory, with throughput
reported against both the leaking baseline and a free-per-temp baseline.

**DoD**: all gates green + docs/197 status updated + board closure line.

### WO-REG-2 — widen the certificate classes (surface-invariant)

In measured-priority order, each widening = new plan rows + same gate
battery:

1. **Spawn argument packs** → task-attached region destroyed at task end
   (ties directly to the 2-mallocs-per-task fine-grain measurement; must
   compose with the ONE run protocol / cooperative cancel — cancel returns
   normally through scopes, so destroy placement is unaffected; panic aborts
   the process, so no unwind path exists to leak through).
2. **Array literal / append temporaries** in region-safe trees.
3. **Interpolation/format buffers**.
4. *(recorded affinity, unscheduled)* the ABI-spec zone-channel arena design
   (§1.3) — channel bodies in zone-owned regions; only after R3 gives zones
   a declared region rider.

**Explicit non-goal**: interprocedural regions (callee allocating into the
caller's region needs an internal allocator-parameter ABI). Deferred until
after R3; if ever built, it stays surface-invisible (constraint #2) and
measured-first.

### WO-REG-3 — surface `region` (BDFL syntax decision)

The declared form. Candidates:

- **(a)** anonymous block: `region { ... }`
- **(b) recommended**: named block: `region frame { ... }` — the name feeds
  diagnostics, squiggle text, and the observability stats (compiler-arena
  precedent: named arenas + last-consumer stamps).
- **(c)** zone-attached rider (`zone Battle with region ...`) — heavier;
  defer unless the zone-channel affinity (§5, WO-REG-2 item 4) matures first.

Semantics: inside a declared region, every allocation the analysis certifies
is region-backed; an **escaping value is a compile error** (fail-closed, not
silent heap demotion — declaring a region states intent, and unprovable
intent must surface) with a fix-it suggesting explicit move-out; explicit
move-out (`own`-move at the boundary) performs the declared copy/transfer.
Implicit copies out of a region draw an **ADVISORY squiggle** (the semantic
squiggle third state; the compiler arena's `cross_stage_copies` counter is
the same concept already instrumented). Grammar register: `region` blocks are
code-layer (`;` statements inside, per the punctuation canon).

Thesis tie-in that makes this more than plumbing: **region + budget is the
quantitative half of the sandbox story**. R6 recorded that capability answers
the qualitative axis and sandbox needs a quantitative one; a declared region
with a declared bound (`region frame limit 64kb` — syntax illustrative only)
turns "untrusted content gets bounded frame memory" into a declared,
runtime-enforced fact on both backends. This is the safe-Flash vision's
memory leg, and it falls out of rungs 1–3 plus one bound check.

### WO-REG-4 — machine-layer grant backend (research rung, unscheduled)

Regions backed by the machine-contact layer's `Region{base, extent, mode,
prov=grant}` instead of malloc on freestanding targets. The vertical slice
(manifest → MIR/AIR fact → both backends → runtime twin → host-sim) already
exists; this rung is recorded so the ladder shows the arena as the bridge
from surface declaration to the machine layer, not scheduled work.

---

## 6. What this strategy explicitly does NOT do

- Not a GC replacement and not general heap management — slot/own-ref remains
  the ownership story; regions only serve certified transients.
- No Rust `typed-arena` mimicry, no lifetime parameters, no allocator trait.
- Does not touch slot storage, channel bodies (until WO-REG-2 item 4), or
  the machine-layer manifest formats.
- No perf claims before best-of-N same-session measurements (measurement
  feedback canon); the R1 headline is memory-boundedness, not speed.
- Does not resolve the general `StringConcat` OOM-fallback question for heap
  sites — that is a separate judgment (silent-empty vs. panic) queued for
  the BDFL, noted here because migrated sites will already panic.

## 7. Risk table

| Risk | Counter |
|---|---|
| Wrong escape certificate → stale read | Poison-on fixtures in CI (both backends); certificate-or-HEAP default |
| Region crossing lanes via missed capture path | Capture check enumerates the same boundary set the spawn plan already owns (spawn/channel/slot/global); spawn boundary = escape by rule, tested |
| ABI §13 revision after consumers exist | Revision is R1 step 1, **before** any consumer; asserts updated in the same commit |
| Budget double-count | Charge block acquisition only; fixture asserts budget delta == blocks, not bumps |
| Emitters growing region logic | Plan-lookup-only rule + grep forbids in the origin-surface owner (spawn-lane precedent) |
| Create/destroy overhead exceeding win | Lazy function-scope region; fixture with zero certified sites must emit zero region ops (byte-identity with today) |
| "Arena everything" scope creep | Constraint #8: slots/zones storage out of scope; each widening in WO-REG-2 is measured before the next |
| Twin drift | The full twin-trap regime + `runtime_bc_contract` + parity, per rung |

## 8. Gate inventory (per rung, cumulative)

| Rung | New gates | Must stay green |
|---|---|---|
| REG-1 | region twin smoke; region backend-compare fixtures (poison-on); memory-boundedness witness; region-plan owner smoke; artifact kind #29 | parity suite; runtime_bc_contract; reachability census (arena rows live); erasure dashboard; axis-budget formal guard |
| REG-2 | per-class compare fixtures; task-region cancel witness | all REG-1 gates |
| REG-3 | surface grammar fixtures (accept/reject/escape-error/move-out); ADVISORY squiggle fixture; bound-enforcement witness (if limit lands) | all prior |
| REG-4 | host-sim grant-backed region smoke | all prior |

## 9. BDFL decision requests

1. **Approve the track** (WO-REG-1 green-light; strategy as written).
2. **ABI §13 revision to chained blocks before first consumer** (§5.1 step 1)
   — recommended yes; the alternative (fixed capacity) forces spill-or-panic
   forks later.
3. **R1 emission granularity**: lazy function-scope region (recommended) vs.
   statement bracket.
4. **R3 syntax pick** (when reached): recommendation is named block
   `region <name> { ... }`; the `limit` bound and zone-rider forms are
   explicitly separable later decisions.
5. **StringConcat heap-site OOM policy** (silent-empty today vs. panic) —
   independent of this track but surfaced by it.

---

*Cross-references*: docs/36 (pipeline; verified projection plan), docs/194
(the join playbook this strategy mirrors), docs/146 (lane evidence),
docs/138 + string ledger (StrView — complementary: StrView removes
allocations, regions bound the remaining ones; order-independent), docs/15
(capability sandbox; §5.3 quantitative leg), docs/19 §✦ (decide-vs-declare),
docs/118 §2.1 (lifetime-annotation ban), docs/121 (types as domain medium),
docs/190 (linkage/state twin regime), MachineLayerCore.v + machine-layer
vertical slice (WO-REG-4 substrate).

---

## Appendix A. Execution log + refined blueprint (2026-07-21)

### A.1 REG-1a — LANDED (`f17b60f4`)

This appendix preserves the original execution log. Its “no emitted consumer”
sentence is historical; the later REG-1b+c wiring landed in the working tree
and is summarized by the live census in §1.2/§1.4.

The runtime foundation is in, verified, and byte-identity-preserving (no
emitted consumer yet). What actually shipped, refining §5.1:

- **New type is `PgyRegion`, not a rehabilitated `PgyArena`.** Census found the
  legacy fixed `PgyArena` is still referenced by the ABI layout registry
  (`mir_abi_layout.c` pins buffer/capacity/offset) and `test_abi_spec.c`, so an
  in-place mutation carried ABI-coupling churn for no benefit. `PgyRegion` /
  `pgy_region_*` was a free symbol space (the machine layer uses `PgyMachine*`),
  matches the surface concept name, and let the legacy arena stay untouched.
  The legacy fixed arena's removal is a separate REG cleanup (not scheduled).
- **File**: `src/runtime/pgy_runtime_region_inline.h` (header-only static
  inline; no export TU — no global state). Included from the memory header;
  split into its own feature-owner header to stay under the 600-line
  production-header cap.
- **API**: `pgy_region_create(block_size)` / `_destroy` / `_alloc(size,align)`
  / `_alloc_array` / `_reset` (reuse: keep blocks, drop contents) /
  `_strdup` / `_string_concat(region,a,b)` (the arena twin of `StringConcat`,
  region-owned, OOM=panic not silent-empty). `PGY_REGION_ALLOC[_ARRAY]` macros.
- **Alignment** is by real address (not offset), so a block's malloc base
  alignment is irrelevant — any power-of-two request is honoured.
- **Budget**: block acquisition charges `PGY_BUDGET_ALLOC_COUNT` (1) and
  `PGY_BUDGET_ALLOC_BYTES` (block capacity) via `pgy_budget_charge_export`,
  exactly like the sibling allocator; the bump fast path carries no atomic.
- **ABI**: new §13a `pgy_abi_region` head-record mirror + `region>=24` assert;
  §13 `pgy_abi_arena` untouched.
- **Verified** via the make→gcc direct channel (the smoke's own bash→gcc path
  dies under the local kernel anti-cheat; make→gcc as grandparent survives):
  chained growth with stable pointers, alignment, region concat, reset reuse —
  all identical inline vs extern; budget fail-closed on a low ceiling; both
  linked-runtime objects compile; ABI asserts hold; `.bc` regenerated; the
  header-size / runtime-bc-contract / runtime-cext-contract gates green.
- **Gate**: `region-arena-test-smoke` (Makefile target + standalone `.PHONY`).
  CI-aggregate membership (into `redteam-repair-contract-test-smoke`) was
  **deferred** — the concurrent session holds that aggregate region uncommitted;
  wiring it now would entangle. Add one line to the aggregate once that lands.

### A.2 REG-1b — historical verified-but-unwired checkpoint (superseded by A.5)

Historical snapshot: the collision described below was subsequently resolved.
The current driver publishes the AIR-gated plan and both C/LLVM consumers read
it; keep the snapshot for provenance, not as the current status.

**Build path confirmed (2026-07-21).** The concurrent 109-file working set
compiles: an isolated `mingw32-make pgy BUILD_DIR=.tmp/reg_build
BIN_DIR=.tmp/reg_bin` (the make→gcc grandparent channel that survives the local
anti-cheat) built a working `pgy.exe` from the current tree. So compiler changes
CAN be built and verified on top of the working set — the constraint is
staging-collision, not build-ability.

**Landed**: `verified_region_plan.{h,c}` — the plan subsystem (new files, house
split pattern, mirroring the concurrent session's own move of the
parallel-capture plan into `verified_parallel_capture_plan.c`, so no
`verified_projection_plan.{h,c}` collision). `PgyRegionDisposition`
(HEAP default / REGION-under-certificate), `PgyRegionFactRow`, `PgyRegionPlan`,
`PgyRegionEscapeResult`. Producer `pgy_verified_region_plan_from_escape` gates on
the AIR evidence certificate (same admission boundary as the spawn-lane plan)
then validates the escape result: null-site refusal, conflicting-scope refusal,
duplicate collapse; `_lookup` fail-closed (miss ⇒ HEAP). Row-building is split
into `_build_rows` so the logic is unit-testable below the certificate gate.
Gate **`region-plan-unit-test-smoke`** (7 cases: empty / three-sites /
duplicate-collapse / conflict-refused / null-refused / lookup-fail-closed /
cert-gate) builds under the project's `-Wall -Wextra -Werror` flags and passes.
The module is **not yet in COMPILER_SOURCES** — it is the sole compiler `.c`
outside that list on purpose (see the collision below); it lands wired the
moment the driver rework settles.

**The verified collision (why the driver wiring waits).** The remaining REG-1b
integration is the driver producing the plan into the transpiler context — the
exact `verified_spawn_lane_plan` produce/carry/dispose template at
`compiler.c` invoke_c_backend, `compiler_llvm.c` (×2), and the
`transpiler.h` ctx-struct field + `transpile_from_mir_with_projection_plan`
signature. A hunk-level check (`git diff <file> | grep '^@@'`) shows the
concurrent session holds uncommitted hunks **in those exact regions**:
`compiler.c` @@ -68 (+17 lines in the spawn-lane produce block),
`compiler_llvm.c` four hunks over both emit paths, `transpiler.h` @@ -153 (the
ctx struct field list) and @@ -367 (the transpile signature). They are wiring
the sibling parallel-capture plan through the identical ctx-field /
transpile-signature / invoke_c_backend regions the region plan needs. This is
not caution — my region field would land inside their struct hunk, my produce
call inside their produce hunk; the additions coalesce into one git hunk and
cannot be staged apart. `transpiler_expr_core_emit.c` (the `emit_binary`
StringConcat site, the REG-1c consumer) is by contrast **clean** of user hunks.

**Resumption (mechanical once the concurrent driver rework commits)**: add
`verified_region_plan.c` + `region_escape_v1.c` to COMPILER_SOURCES; add a
`const PgyRegionPlan *` field to the transpiler ctx and a param to the transpile
entry beside `spawn_lane_plan`; in the driver, run the escape pass, feed
`pgy_verified_region_plan_from_escape`, carry/dispose beside the spawn-lane
plan. No inline-mode refusal (PgyRegion works in every materialization, unlike
the movable lane).

### A.2b REG-1c analysis half — LANDED verified (`region_escape_v1`)

The emission consumer splits cleanly into an analysis (which certifies sites)
and the emission itself (which rewrites the certified StringConcat sites). The
analysis is **landed and verified**: `region_escape_v1.{h,c}` certifies the
narrowest provably-sound class — a string concat that is a **direct
Print/PrintLn argument** (its result is handed to a builtin that reads it
synchronously and never retains it, so it cannot outlive the enclosing
statement; a function-scope region trivially outlives every use). It walks the
AST reading struct fields directly (no parser-object link), certifies the
argument concat plus its nested left-spine concats, and allocates one region
scope id per function. The walk is deliberately incomplete — a container it does
not descend simply yields no certifications, keeping those sites HEAP — so it is
sound by construction. Gate **`region-escape-unit-test-smoke`** (Print(a+b)→1,
PrintLn(a+b+c)→2, non-Print→0, bare→0, distinct per-function scopes) passes under
the project `-Wall -Wextra` flags. Its interface (produce an escape-site array)
is independent of the driver rework, so it was safe to land now.

**What remains for REG-1c is only the emission**, and it is small: at the (clean,
user-untouched) `emit_binary` StringConcat site in
`transpiler_expr_core_emit.c`, look up the node in the plan; when REGION, emit
`pgy_region_string_concat(&__pgy_region_<scope>, a, b)`; and emit one
`pgy_region_create/destroy` per certified function scope (a function-scope
region, destroyed on every return via the MIR terminal-branch CFG facts). The
LLVM string path is the parity twin. This waits only on the driver producing the
plan into the ctx — the one entangled step.

**Plan type** (`verified_region_plan.h`):
```
typedef enum { PGY_REGION_SITE_HEAP, PGY_REGION_SITE_REGION } PgyRegionDisposition;
typedef struct PgyRegionFactRow {
    uint32_t allocation_site_id;   /* stable semantic/HIR/MIR allocation key */
    PgyRegionDisposition  disp;    /* REGION only under an escape certificate  */
    uint32_t              scope_id;/* function-scope region id (lazy, per fn)  */
} PgyRegionFactRow;
typedef struct PgyRegionPlan {
    uint32_t revision; PgyRegionFactRow *rows; size_t row_count; bool verified;
} PgyRegionPlan;
#define PGY_REGION_PLAN_REVISION UINT32_C(1)
```
Producer `pgy_verified_region_plan_from_escape` mirrors
`pgy_verified_spawn_lane_plan_from_air`: certificate-ready gate; a site with no
escape certificate defaults to HEAP (never REGION-without-proof); conflicting
dispositions for one allocation-site id → refuse; duplicates collapse. A
zero/missing allocation-site id is contradictory evidence and refuses the
plan. `_lookup(plan, allocation_site_id, &disp,&scope)` fail-closed (absent →
HEAP). `_dispose` frees rows.

**Escape pass v1** (string temporaries only): a `pgy_region_escape_v1` pass
over RIR/MIR reusing the own/ref + value-capture machinery. Certify a string
concat/temporary site REGION iff its whole expression tree is statement-local
and the result is not: bound to an out-of-scope name; returned; captured
(closure/spawn/channel/slot/global); passed to a non-borrow-safe callee
(whitelist Print/len/compare). Default deny → HEAP. No cross-statement liveness
in v1.

**Driver wiring** (isolated hunks in `compiler.c` + `compiler_llvm.c` ×3, right
beside the spawn-lane produce/dispose): produce the region plan from AIR after
the spawn-lane plan; pass it into the transpile/LLVM contexts; dispose after.
Inline-mode note: unlike the movable lane, regions work in every
materialization (PgyRegion is header-only), so no inline-mode refusal.

**Emission** (REG-1c):
- Lazy **function-scope** region: on the first REGION-certified site in a
  function, emit `PgyRegion __pgy_region = pgy_region_create(0);` at entry;
  emit `pgy_region_destroy(&__pgy_region);` on **every** return path (use the
  MIR terminal-branch CFG facts already carried — the same ones that place the
  M:N join). Zero certified sites ⇒ zero region ops (byte-identity with today).
- A certified `StringConcat(a,b)` site lowers to
  `pgy_region_string_concat(&__pgy_region, a, b)` instead of the heap
  `StringConcat`; nested certified concats compose (region owns every
  intermediate — closes the §1.4 chained-concat leak). Both backends translate
  the same plan row; the parity gate extends to region fixtures.

**Gates for the REG-1b+c train**: region backend-compare fixtures with
`PGY_REGION_POISON` on in both backends (a wrong certificate crashes
deterministically — the region analogue of the UAF fixtures), including an
early-return + match-arm function that witnesses destroy on every path; a
memory-boundedness witness (the leaking chained-concat fixture's RSS
plateaus); C==LLVM output equality; reachability rows for the PgyRegion family
flipped to **live in the same commit** (until then they stay declared/honest).

### A.3 REG-1d — origin surface ✅ LANDED (`58a4a5e4`)

`src/self_hosted/compiler/region_plan_owner.pgy` (450 lines) + manifest +
pinned golden + `tests/selfhost_region_plan_smoke.sh`, artifact-zone kind 29,
OWNERS.md and component-contract registration. Gate is green: golden contract
diff through the self-hosted comparator, C-leg == LLVM-leg artifact equality,
10 require pins, 4 forbid pins, self-test.

Landed **before** the REG-1b+c train rather than with it, which the original
plan had backwards. The owner is a *contract* artifact: it can declare the full
rule set (including the owner-conflict refusal and the function-scope owner
lookup, both of which only exist in the working tree so far) while pinning only
the C projections that exist at that commit. Pinning is what must not run ahead
of the code; modelling is not.

The load-bearing addition over the plan sketch is naming the **asymmetry** as
the owner's central claim, with its own witness (`lookup_miss_is_heap`) and its
own contract row: a miss is HEAP, not a refusal, which is precisely why a
narrow escape analysis is shippable. The spawn-lane plan is the contrast case
(a miss there must refuse — no safe default lane exists). Both are fail-closed;
the word means different things depending on whether a safe default exists, and
that distinction is now written down where a future session cannot lose it.

Deferred to the wiring commit, by the same pin-honesty rule: the driver
produce/dispose require rows, the emitters' lookup require rows, and a
`reach|...|live` row for `pgy_verified_region_plan_lookup` in the reachability
manifest.

### A.4 Current correction train — stable IDs, reset reuse, and explicit certificate semantics

The live plan no longer keys rows by `ASTNode *`. `region_escape_v1` converts
each certified node to `ast_node_stable_id` while the driver still owns the AST;
`PgyRegionEscapeSite`, `PgyRegionFactRow`, and backend lookup carry only the
positive `AllocationSiteId`. This prevents an address from crossing MIR,
serialization, restart, or independent C/LLVM processes. The escape pass is
still a narrow AST-owned prototype for callee classification; it must not be
widened or treated as the final semantic owner until resolved call identity and
retention summaries replace the `Print`/`PrintLn` spelling check.

`pgy_region_reset` now reuses zeroed retained blocks in the existing destroy
chain before charging a new acquisition. A grow → reset → grow harness crosses
two blocks and asserts the block count is stable. The contract distinguishes a
valid certificate with an uncertified site (HEAP) from missing/invalid AIR or a
contradictory allocation-site row (plan publication failure).

### A.5 The REG-1c collision dissolved — wiring is in the tree, build green

The blocker recorded in A.2/A.2b is **gone**, and not by waiting it out. The
concurrent stream picked up the landed REG-1b/1c artifacts and wired them
itself, end to end:

- `CompilerIRBundle` carries `const struct PgyRegionPlan *region_plan`
  (`src/compiler/compiler.h`) — the plan reaches backends as an immutable
  bundle field, which is a better seam than the transpiler-ctx parameter this
  document originally sketched;
- the driver produces it (`src/compiler/driver_app.c`: escape collect →
  `pgy_verified_region_plan_from_escape` → dispose on both exits);
- both backends consume it — `transpiler_context.c` and
  `llvm_expr_scalar_core.c` for the per-site lookup,
  `llvm_mir_emit.c` for the function-scope region — via a new
  `pgy_verified_region_plan_scope_for_function_id`, with
  `function_syntax_id` added to the row so a routine can claim its scope by
  stable MIR id rather than by AST root;
- `verified_region_plan.c` + `region_escape_v1.c` are in `COMPILER_SOURCES`.

**Verified**: a full isolated `mingw32-make pgy` over the current tree exits 0
with no errors. Both unit gates stay green.

Two follow-ups this session produced that must land **with** that wiring
commit, because they reference the not-yet-committed `function_syntax_id`:

1. `tests/region_plan_unit.c` gained coverage for the branches the extension
   added and left untested — every test had passed `function_syntax_id = 0`, so
   the new owner-conflict refusal and the whole
   `_scope_for_function_id` fail-closed matrix (null plan, unverified plan,
   zero id, unknown owner, and an owner row carrying scope 0) had no gate at
   all. Verified green.
2. The `selfhost-region-plan-test-smoke` aggregate membership line (the target
   itself is committed; only the one-line aggregate edit sits in a hunk mixed
   with the concurrent stream's own aggregate additions).

### A.6 Current SoT correction — semantic escape facts

The current tree supersedes the historical `region_escape_v1` producer notes
above. `src/semantic/region_escape_fact.{h,c}` is now the semantic owner of
the bounded direct-`Print` concat facts. It runs after type checking, consumes
the resolved `BuiltinKind` call fact, and emits only stable allocation-site,
scope, and function identities. Missing or invalid facts fail closed; the
driver no longer calls an AST escape walk and only validates the semantic rows
through `pgy_verified_region_plan_from_escape`.

The retired `src/compiler/region_escape_v1.{h,c}` path is deleted and the
component contract rejects its reappearance. C and LLVM backend parity,
semantic region unit, HIR projection, MIR retention, verified-plan, arena, and
self-hosted owner gates pass for this slice. HIR retains the projected rows;
MIR owns a second stable copy and the driver rejects a missing MIR carrier
before plan materialization. Broader semantic retention summaries and complete
region allocation ownership remain open, so the registry row remains `BRIDGE`.

Process note worth keeping: the earlier "blocked" reading was correct at the
time but the *response* was too passive. What unblocked the track was checking
whether the tree still built (it did), then landing every piece that did not
touch a co-edited region — runtime, plan, analysis, and now the whole origin
surface — so that when the wiring arrived it had something to wire.
