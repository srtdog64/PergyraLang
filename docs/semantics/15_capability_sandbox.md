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

- **Self-imposing manifest at load.** The manifest is computed and inspectable;
  the remaining step for a real loader is to call `pgy_cap_set_manifest_export`
  with a host-chosen subset at content startup. The runtime API exists; the
  policy (who decides the grant, signed manifest format) is the product surface
  below.
- **Real media runtime.** RENDER/AUDIO/INPUT are gated but headless (call-count
  stubs). The canvas/WebGL/WebAudio/input backend behind them lands with the
  browser/WASM target; only then does the API do anything visible.
- **More ambient chokepoints.** The handle-based file API (FileOpen/FileRead/
  FileWrite) and raw network are not yet gated; they are intentionally absent
  from the manifest table until their runtime paths carry a gate (listing them
  would be a manifest the runtime does not back). NETWORK/ENV bits exist for when
  those paths are gated.
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
