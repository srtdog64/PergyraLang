# 15. Content Capability Sandbox (the runtime-enforced effect boundary)

This is the linchpin of the "safe distributable interactive content" direction
(a Flash-class authoring/distribution experience, reimagined so that content
received from a stranger is *verifiably bounded*). Flash died of its plugin's
unbounded ambient authority; the answer is not "trust the author" but "bound
what any content can do, and prove the bound." Pergyra already declares effects
per function; this doc adds the half that was missing — a **runtime-enforced
capability boundary** — so an effect is no longer just a static label but an
enforced gate.

## 0. The model (declare → manifest → gate → prove)

1. **Declare** — content declares, per function, the authorities it uses: the
   coarse effect set (`with effects`) and/or the fine capability set
   (`with caps io_read, clock, …`). Both are optional surface.
2. **Manifest** — the program carries a capability manifest: the union of every
   capability it can exercise, a single mask over `pgy_runtime_capability.h` bits
   (IO_READ, IO_WRITE, NETWORK, CLOCK, RANDOM, ENV, RENDER, AUDIO, INPUT). It is
   *inferred* by the type checker, not hand-maintained.
3. **Gate (runtime)** — a loader sets the process-wide granted set to the
   manifest before running the content; every authority operation calls
   `pgy_cap_require_export(cap, op)`, which panics fail-closed (class
   `capability-denied`) if `cap` is not granted.
4. **Prove (static, type checker)** — capabilities are inferred bottom-up and
   propagated through calls like effects, and a `with caps` declaration is checked
   *declared ⊇ used* at compile time. So a content that declares its capabilities
   cannot under-declare: it cannot use one it did not list, including through a
   call. The gate enforces the bound at runtime; the inference + check make the
   manifest honest before it ever runs (sound for the static call graph; the gate
   backstops the dynamic-dispatch residual — see §1 soundness note).

The bits that matter for untrusted content are the fingerprinting/exfiltration
surface — CLOCK (timing fingerprint), NETWORK (exfiltration), IO_WRITE
(persistence), ENV — exactly the things a "fun web game from a stranger" can
abuse today on raw HTML5/JS.

## 1. Implemented (the runtime gate + static manifest)

- Capability vocabulary: `pgy_runtime_capability.h` (one bit per capability).
- Process-wide granted set + gate, dual-twinned like the lifecycle/checked-arith
  runtime: static-inline in `pgy_runtime_panic_checked_inline.h` for the
  self-contained C output, external in `pgy_runtime_lib_authority_file_core.h`
  for the LLVM-linked runtime object. Both excluded from inlined bitcode
  (`llvm_fn_is_capability_runtime`) so the abort path lowers correctly and the
  one granted mask is shared (not split across inlined copies).
  - `pgy_cap_set_manifest_export(mask)` — loader imposes the manifest.
  - `pgy_cap_grant_all_export()` / default — trusted programs are unaffected.
  - `pgy_cap_require_export(cap, op)` — fail-closed gate.
  - `pgy_cap_granted_export()` — introspection.
- Gated ambient operations (both backends, the dual definitions): file read /
  console input / dir-walk (`PGY_CAP_IO_READ`), file write (`PGY_CAP_IO_WRITE`),
  wall-clock (`PGY_CAP_CLOCK`), RNG seed/draw (`PGY_CAP_RANDOM`).
- Media host API — headless capability-gated stubs in
  `pgy_runtime_media_stub.h`: render (`PGY_CAP_RENDER`), audio
  (`PGY_CAP_AUDIO`), input (`PGY_CAP_INPUT`). The real backend
  (canvas/WebGL/WebAudio) lands with the browser/WASM target; the stubs give the
  capability model a *complete* gated surface today (a manifest that omits RENDER
  literally cannot draw), and the API shape is fixed for the future backend.
- **Capability is a first-class refinement of effects** (not a separate
  best-effort pass). Each gated builtin records its fine `PGY_CAP_*` bit beside
  its coarse `EFFECT_*` family (`semantic_record_capability`); the function
  `Type` carries a `capability_mask`; and a call propagates its callee's
  capabilities into the caller exactly as it does effects
  (`type_checker_helpers_late.c`). So a function's used-capability set is
  **inferred interprocedurally** by the same proven machinery that infers
  effects — no parallel analysis, no AST re-walk.
- Capability manifest: `SemanticResult.program_capabilities` is the union of
  every capability the program can exercise. `pgy --capability-manifest <file>`
  prints it as a stable JSON document (`pgy.capability.manifest.v1`) — the
  artifact a host reads to decide what to grant.
- Surface declaration + **declared ⊇ used** check (the `with caps` contract):
  a function may declare `with caps io_read, clock, …`; the type checker then
  requires its declared set to cover every capability its body uses — including
  capabilities reached *through calls*. An under-declaration is an ordinary
  semantic error (`Function 'f' is missing declared capabilities: clock …`),
  enforced on every build, fail-closed. This is a precise, fine-grained refinement
  of the coarse `with effects` check (which still independently enforces effect
  families).
- Panic class `capability-denied` with a traceable record
  (`op=… required=0x… granted=0x…`).
- Regression:
  - `make test-capability` — granted-path passes; a restricted manifest that
    omits CLOCK makes a clock op panic, and a no-RENDER manifest makes a render
    op panic (`capability-denied`).
  - `make test-capability-manifest` — clean / `with caps`-declared-ok fixtures
    pass; the violation fixture and the **interprocedural** fixture (a function
    declaring `io_read` that calls a helper using `clock`) both fail the gate
    naming the missing capability.

Default grant is `PGY_CAP_ALL`: existing programs and tests are unchanged until a
loader imposes a manifest. The sandbox is opt-in by the *host*, not a tax on
ordinary builds. `with caps` is optional: a function without it is never rejected,
its capabilities are still inferred into the program manifest and still enforced
at runtime.

### Soundness note (deliberate, honest)

The capability set is inferred **interprocedurally and is sound for the static
call graph** — the per-function `declared ⊇ used` check sees capabilities reached
through ordinary calls, not just locally-named builtins. What it cannot close,
*no static analysis can*: **dynamic dispatch** (ability/witness dynamic, function
values/lambdas invoked indirectly), **FFI**, and **`unsafe`** make the call graph
incomplete (Rice's theorem). At those boundaries the inferred set is a lower
bound, and the **runtime capability gate is the ground truth**: it fail-closes
regardless, so static imprecision is always *safe*, never permissive. The two
layers compose — inference/manifest for inspection-before-run, the gate for
enforcement-at-run.

This is why capability and effect are kept as independent masks rather than one
derived from the other: a builtin records both its fine capability and its coarse
effect family, so the two checks are each precise in their own dimension (e.g.
`Now` is `CLOCK`+`NONDETERMINISTIC`; `Input` is `IO_READ`+`NONDETERMINISTIC`).
The earlier coarse effect→capability cross-check is retired.

## 2. Not yet (the honest roadmap)

- **Host grant channel — built (parity-verified).** A host now restricts the
  granted set out-of-band via `PGY_CAP_GRANT` (e.g. `PGY_CAP_GRANT="io_read,clock"`),
  the symmetric mirror of the budget's `PGY_BUDGET_*` channel. A shared parser
  (`pgy_cap_env_grant` in `pgy_runtime_capability.h`) and a once-latch in both
  the inline and extern twins apply it before the first gated op; unset leaves
  the default `PGY_CAP_ALL` so trusted programs are unaffected. This is what made
  the previously-dormant runtime gate actually enforce on real programs and on
  *both* backends (it depends on the single-instance `g_pgy_cap_granted` fix
  above — before it, LLVM read a grant-all copy and never denied). Verified:
  `cap_random_demo.pgy` (`Random` gates `PGY_CAP_RANDOM`) runs under
  unset/`random`, fail-closes `capability-denied` under `clock`/`none`,
  identically on C and LLVM.
- **Self-imposing manifest at load.** The env channel is the host-out-of-band
  path; the remaining step for an *in-content* loader is to call
  `pgy_cap_set_manifest_export` with a host-chosen subset at content startup.
  The runtime API exists; the policy (who decides the grant, signed manifest
  format) is the product surface below.
- **Real media runtime.** RENDER/AUDIO/INPUT are gated but headless (call-count
  stubs). The canvas/WebGL/WebAudio/input backend behind them lands with the
  browser/WASM target; only then does the API do anything visible.
- **More ambient chokepoints.** The handle-based file API (FileOpen/FileRead/
  FileWrite) and raw network are not yet gated; they are intentionally absent
  from the manifest table until their runtime paths carry a gate (listing them
  would be a manifest the runtime does not back). NETWORK/ENV bits exist for when
  those paths are gated.
- **Resource budget / DoS model — the missing quantitative axis (external
  red-team R6, docs/134).** The capability model is *qualitative*: it answers
  "can this content do X at all?" (yes/no per capability). A sandbox for
  untrusted content also needs the *quantitative* answer — "how much?". A
  granted capability says nothing about an infinite loop, memory exhaustion, a
  fork-bomb of spawns, or unbounded allocation: capability `RENDER` granted does
  not bound how many frames or how much memory. Slot/authority/capability do not
  touch *how much*.
  - **Built (slice 1) — the runtime gate.** `pgy_runtime_budget.h` is the
    quantitative twin of the capability gate: a per-kind budget
    (ALLOC_BYTES / ALLOC_COUNT / SPAWN_COUNT / CHANNEL_COUNT) the loader imposes
    (`pgy_budget_set_limit_export`), with `pgy_budget_charge_export(kind, n, op)`
    panicking fail-closed (class `budget-exceeded`) the first time a kind's
    running total passes its ceiling. Same dual-twin/bitcode pattern as the
    capability gate; default per-kind limit is unlimited so trusted programs are
    unaffected (opt-in by host). Saturating add so a near-overflow charge cannot
    wrap past the ceiling. Verified: `make test-budget` (granted-path + deny-alloc
    + deny-spawn fail-closed).
  - **Built (slice 2) — the allocator is metered.** The arena allocator's
    existing per-allocation accounting hook (`pgy_allocator_record_alloc`)
    charges ALLOC_COUNT + ALLOC_BYTES behind an `imposed` fast-path (trusted
    programs that set no budget pay nothing). `pgy_allocator_record_alloc` lives
    in `allocator_inline.h`, which both twin chains include
    (pgy_runtime.h → inline_core → memory_array_slot for C output/tests; and
    `lib_allocator_exports` → the `.bc` for LLVM), so the charge resolves to the
    inline twin in C output and the extern twin in the linked runtime — one
    counter per backend context, exactly like the capability gated ops. Verified:
    a real `pgy_alloc` over the imposed ceiling panics `budget-exceeded`; with no
    budget the fast-path skips (used=0); both backends run unchanged on ordinary
    programs (regression-clean). `.bc` regenerated.
  - **Built (slice 2.5) — C/LLVM parity + the host env channel.** The host
    imposes a budget out-of-band via `PGY_BUDGET_ALLOC_BYTES` (and the
    ALLOC_COUNT / SPAWN_COUNT / CHANNEL_COUNT siblings); when set, the metered
    allocator fail-closes on overrun. This was the slice that exposed a real
    backend-divergence bug: the gate state lived in a `static PgyBudgetState
    g_pgy_budget` in a header compiled into *three* objects on the LLVM path
    (the inlined `.bc` copy, the native runtime cache object, and the program
    after llvm-link) — `objdump` showed three `g_pgy_budget` symbols. `is_imposed`
    read the env-imposed copy while `charge` accumulated into a default-unlimited
    copy, so **LLVM never fail-closed** even though C did. Fix: a
    `PGY_RUNTIME_BC_BUILD` guard so only the native cache object *defines*
    `g_pgy_budget` (and `g_pgy_cap_granted`); the `.bc` build declares them
    `extern`, collapsing to one instance. The capability gated ops are
    bitcode-stripped to that one object (`llvm_fn_is_budget_runtime`,
    `llvm_fn_is_capability_runtime`) and kept `noinline` so no inlined copy can
    re-split the state. Verified: `objdump` now shows a single `g_pgy_budget`;
    a `while`-pushed `List` (`budget_alloc_demo4`) fail-closes identically on C
    and LLVM under `PGY_BUDGET_ALLOC_BYTES=64` (used=128). A small fixed `Array`
    (`budget_alloc_demo3`) legitimately stack-promotes (SROA) on LLVM so it has
    no heap charge — a real lowering divergence, not a gate defect; the
    forced-heap List case is the parity test.
  - **Built (slice 3) — SPAWN_COUNT (the fork-bomb bound).** Every spawn funnels
    through one of two runtime chokepoints that *both* backends share — `pgy_spawn`
    (pool tasks / `parallel {}`) and `pgy_async_spawn` (the coroutine model behind
    `spawn expr` / `async`; it does its own fiber creation and does **not** route
    through `pgy_spawn`, so it carries its own charge). Each charges SPAWN_COUNT
    once behind the imposed fast-path, deny-before-allocate, so a fork-bomb
    fail-closes on the spawn that crosses the host's ceiling. Unlike the allocator,
    spawn was *not* the IR-reimplementation problem: LLVM emits the lane-owned
    `pgy_lane_spawn_dispatch_export`, which calls the shared runtime dispatcher,
    so a single runtime charge covers both backends through the
    capability-gated-op pattern. The one wiring cost was include order: the spawn
    headers (`pgy_parallel.h`) are pulled in before the budget twin in both chains,
    so the inline twin is now pulled into the C-only `platform_io_core.h` ahead of
    them, and the extern twin is forward-declared in `authority_file_core.h` (the
    extern-twin TU) ahead of them. `panic_checked_inline.h` gained the include
    guard it was missing. Verified: `budget_spawn_demo` (4 spawns) runs free with
    no budget and fail-closes on the 4th under `PGY_BUDGET_SPAWN_COUNT=3`,
    identically on C and LLVM. Charge counter is atomic (sound under concurrent
    spawn). Gated by `make test-capability-runtime`.
  - **Built (slice 3 cont.) — CHANNEL_COUNT.** Channel creation turned out to be
    a clean chokepoint after all (the initial guess that it was the collection
    IR-reimplementation case was wrong): both backends call the shared runtime
    `pgy_channel_init_<T>` — LLVM emits a call to it
    (`llvm_mir_source_resource_defs.c`), it does not reimplement channel init in
    IR. The only wrinkle is that init has a dual definition like the gate twins:
    a `static inline` macro instantiation (`pgy_channel_inline.h`, C path) and a
    non-inline export (`pgy_runtime_lib_channel_*_exports.h`, LLVM/.bc path), so
    the charge lives in both (Int and String types). Each charges CHANNEL_COUNT
    once, deny-before-allocate, behind the imposed fast-path. Verified:
    `budget_channel_demo` (4 channels) runs free with no budget and fail-closes
    on the 4th under `PGY_BUDGET_CHANNEL_COUNT=3`, identically on C and LLVM.
    Gated by `make test-capability-runtime`.
  - **Built (slice 4) — wall-clock deadline (the time axis).** A real DoS bound
    for runaway time, including a tight `while(true){}` that never allocates,
    spawns, or opens a channel — the case the per-operation counters cannot see.
    No instruction sampling: a detached watchdog thread sleeps `PGY_BUDGET_WALL_MS`
    then fail-closes the whole process (`budget-exceeded`, op=wall-time); abort
    terminates from any thread, so the bound holds regardless of what the main
    thread is doing. Armed once at main entry via codegen
    (`pgy_budget_wall_arm_export`, emitted by transpiler.c for C and
    llvm_main_wrapper.c for LLVM) — not a constructor, which would duplicate
    across the twin/multi-TU landscape (the same trap as the gate state). It is a
    wall-clock deadline, not a CPU-time budget (a sleeping program counts against
    it) — labelled as such. Verified: `budget_wall_demo` (an infinite toggle
    loop) is bounded at the deadline identically on C and LLVM; normal programs
    are unaffected (the arm is a no-op when the env is unset — 60/60 fuzz parity
    with the arm emitted in every `main`).
  - **Not yet (slice 4+).** In-content loader self-imposition (vs the host env
    channel, which is built) remains. So "safe new Flash" stays a vision label
    until the product surface (below) lands — but all five budget axes now meter
    with C/LLVM parity: ALLOC_BYTES, ALLOC_COUNT, SPAWN_COUNT, CHANNEL_COUNT
    (discrete counters) and the wall-clock deadline (time).
- **Deterministic asset/runtime boundary + WASM/native equivalence.** Trust also
  requires that the same content under the same manifest behaves the same across
  the native and WASM backends (the C/LLVM/wasm parity story, docs/134 R2),
  and that asset/runtime boundaries are deterministic. Neither is proven yet.
- **Distribution + trust format.** A WASM-in-browser bundle + a signed,
  user-visible manifest + a loader is the actual product surface; none of it is
  built. The browser provides the memory sandbox; this capability boundary is the
  layer above it that makes the *behavior* bounded and inspectable.
- **Thread model.** The granted set is set-once-at-load then read-only; a
  loader that re-imposes manifests per-content concurrently would need
  synchronisation (today it is a single set before run).

## 3. Why this is the right first move

Graphics is a crowded, losing battle (Unity/Godot/Phaser already export to web).
The unoccupied niche is **content you can run from strangers because its
capabilities are bounded and inspectable** — and that is precisely what
Pergyra's effect/authority/fail-closed thesis is the machinery for. The runtime
gate is the smallest piece that turns "effect = static label" into "effect =
enforced boundary," which is the foundation everything else (manifest, proof,
loader) builds on. See the lost-meaning thesis and the dungeon-crawler killer
use-case, of which this is the generalization.
