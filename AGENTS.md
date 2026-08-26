# Agent Rules

This repository is in beta-closure mode. Prefer source-of-truth closure over
cosmetic reshuffling.

## Resume Context

- Start a resumed session with `docs/current_work_handoff.md`, then verify its
  checkpoint with `git status`, the named owner document, and the named
  focused gate.
- Resume only from the handoff's top `Active self-host context` card. Everything
  below its historical archive boundary is evidence for lookup, not an active
  work queue. Do not revive an older checkpoint because it is longer or more
  detailed than the active card.
- External-project provenance, library-adoption research, architecture reviews,
  and performance proposals are inactive unless the active card names one as
  the exact blocker or the user explicitly reopens it. In particular, Insere
  and Zeno are bounded provenance, not compiler context owners.
- `docs/current_work_handoff.md` is a navigation snapshot, not semantic
  authority. The SoT registry, active rung owner, protocol/ABI registry, and
  executable gates remain authoritative when a handoff and the current tree
  disagree.
- Refresh the handoff after a material work session with the exact HEAD/dirty
  state, active executable rung, last green gate, next falsifying fixture, and
  any blocker. Do not record an inferred continuation as completed work.

## Developer Experience Is A Core Invariant

- The language slogan is: `개발자가 즐거워야 유저도 즐겁다.`
- Preserve strong typing, evidence, ownership, and fail-closed behavior without
  exposing proof strategy, execution lane, materialization, layout, or backend
  mechanisms as routine user choices.
- Prefer one sound default. Add explicit syntax only for a real authority,
  interoperability, observable-cost, loss-tolerance, or ownership boundary.
- Derived choices must remain inspectable through diagnostics or IR facts.
  Convenience must not hide failure until runtime.
- Treat repeated Option/Result rituals, one-element out-parameter arrays,
  namespace prefixes, and string-concatenation pyramids as language DX debt,
  not as failures of user discipline.

## Objective Function Before Structure

- Before a structural change, state the objective, priority order, fact owner,
  last legitimate consumer, forbidden fallback, and verification gate.
- Do not treat a familiar compiler architecture as a neutral default. Import
  its invariant only after mapping that invariant to Pergyra ownership and
  evidence lifetime; otherwise it is a reference, not an implementation plan.
- If the prompt leaves priorities implicit, use this repository order:
  semantic identity and one SoT, owner-directed facts, fallback removal,
  negative ratchet, then patch size and conventional architecture.
- See `docs/131_ai_coding_atomic_units.md` section 3.1 for the objective-card
  template. An AI-probable next step is not proof of the right next step.
- When ownership moves as the compiler grows, follow
  `docs/180_compiler_logical_spine_handles_gates.md`: keep identity stable,
  migrate consumers, fail closed, delete the old owner, and ratchet the old
  read path. Do not leave dual-read or `new ? old` compatibility authority.
- Top-level compiler fact families must be declared in
  `docs/semantics/sot_owner_spine_registry.md`. A registry row fixes owner
  identity; it may be marked `CLOSED` only after consumer migration, missing-
  fact failure, old-path deletion, and a negative gate all exist.

## Do Not Add These Anti-Patterns

- Keep `helper` narrowly literal: a helper may contain only minimal stateless,
  policy-free utility code with no semantic or backend decision, state/stage
  transition, registry, cache, fallback, ownership choice, or dispatch. Code
  that owns any such responsibility is an owner and must be named for it.
- Do not add unnecessary helper functions or new generic `*_helper*` buckets.
  A one-off wrapper that only hides local logic is debt. Existing legacy helper
  paths are a shrink-only inventory: they may be renamed into responsibility-
  named owners or removed, but must not justify another helper bucket.
- Do not add empty `try` / `catch` blocks, silent catch-all handlers, or
  "ignore and continue" error paths. Use `Result`, a structured diagnostic, an
  explicit invariant, or a documented panic boundary.
- Do not re-scan AST/program roots when a typed owner seam, MIR fact, AIR
  evidence node, or DAG metadata fact already owns the answer.
- Do not concatenate a large program-global fact serialization with per-routine
  local rows. Keep global and local facts in a structured view, preserve the
  owner's explicit lookup order, and measure the fixed compiler-scale input.
  A byte-equal output alone does not excuse repeated whole-program copies.
- Do not keep a borrowed `LLVMVarEntry *` from `llvm_scope_lookup()` across
  `llvm_scope_push`, `llvm_scope_pop`, or `llvm_scope_declare`. Snapshot
  `alloca` / `type` first; scope declarations may realloc the frame storage.
- Do not pass growable runtime container storage (`Array`, `Slice`, `List`,
  `Queue`, `Set`, `HashMap`) across `parallel` / `async` / worker boundaries by
  raw pointer. Use a channel/result boundary, an explicit copy, or a documented
  pinned read-only view owner. Rehash/grow plus concurrent read is UB.
- Do not let detached `async { ... }` capture local storage by pointer,
  including `Channel<T>`. `Channel<T>` is a mutex/condvar-backed value today,
  not a copyable shared handle. Use `parallel`, `async func`, or an explicit
  handoff boundary instead.

## Default Work Loop

1. Name the source-of-truth seam being closed.
2. Make the smallest code change that moves the decision behind that owner.
3. Add or tighten a smoke gate so the old path cannot reappear.
4. Run the narrow gate first; run broader tests only after the slice is stable.

## Hard Self-Host Progress Guard

- Keep one active self-host rung in this order: production entrypoint, direct C
  bypass to delete, Pergyra fact owner, last orchestration consumer, focused
  parity/negative gate, installed-driver evidence, then bounded performance
  evidence when execution is the blocker.
- Do not start a general query engine, cache architecture, library adoption, or
  unrelated SoT cleanup while an executable rung is open. Instrument only the
  reached Pergyra owner needed to identify the next falsifying case.
- SoT is a hard-substitution rung condition, not an independent cleanup track.
  Close only the semantic seam reached by the one active executable rung.
- Do not make more than two consecutive SoT-only commits. Before a third, land
  an executable replacement delta or record the exact missing fact, owner,
  last consumer, and falsifying fixture as BLOCKED.
- Count progress only when a Pergyra implementation replaces a real C-owned
  compiler path. Owner files, tests, documents, and LOC are supporting evidence,
  not substitution progress by themselves.
- Treat a test as a falsifier of a named ownership claim, not as the objective
  function. A green row cannot excuse dual authority, an undeleted fallback,
  or repeated reconstruction of an admitted artifact.
- `tests/self_hosted_component_contract_smoke.sh` is only a structural source
  inventory and old-path residue ratchet. Do not add behavioral correctness
  claims to it; executable parity and focused negative gates own those claims.
- Count completeness and performance work by semantic execution target. When
  multiple inventory rows project to one import-composed program, execute that
  program once per stage/run, record unique checks and reuses, and attribute
  the result back to the rows. Validate a cumulative graph once at its owner
  boundary.
- Before delegating parallel work, fix the objective card, owner boundaries,
  independent edit scopes, and one integration gate. Do not use agent count,
  token volume, fixture count, or generated files as progress evidence, and do
  not open parallel implementation tracks on the active executable rung.
- Agent work directives are temporary coordination artifacts, not numbered
  architecture documents. Store them under `docs/agent_work_directives/` with
  descriptive filenames without a leading document number, and follow that
  directory's `README.md`.
  Put read-only findings under `docs/audits/`; neither location owns compiler
  semantics, progress, registry status, or a successor implementation rung.
- If wall time, memory, or artifact count grows faster than the semantic input,
  stop expansion and identify the repeated owned operation before adding a
  cache, shard, worker, timeout, or memory allowance.
- Budget the edit loop: 60 seconds for static owner gates, 5 minutes for focused
  parity, and 30 minutes for an integration shard. Full matrices belong at
  scheduled or merge boundaries.

## Hard Pergyra-Native Dogfood Guard

- Follow `docs/self_hosted/17_pergyra_native_dogfood_contract.md`. A parsed or
  statically gated `world`, `zone`, `subject`, `action`, or `intent` is only
  surface support until a production self-host entrypoint reaches it.
- Record Pergyra-native evidence as `SURFACE`, `REACHABLE`, or `SUBSTITUTING`.
  Only `SUBSTITUTING` counts as hard self-host replacement progress.
- Do not use keyword counts, fixtures, generated projections, parser probes,
  readiness-only actions, or an unimported `world.pgy` as executable dogfood
  evidence.
- Keep pure computation and value facts in `func`/`struct` when that is their
  responsibility. Use `subject/action` for identity-bearing authority or
  state/stage transitions and `zone` for real resource boundaries. Use
  `intent` only when one real-world purpose is closed with explicit
  success/failure meaning and its participant, coordination, authority,
  effect, boundary, compensation, and trace obligations must be attributed to
  one source-level binder. Action count is neither necessary nor sufficient;
  follow `docs/01_intent_first_design.md` and
  `docs/173_intent_axis_strengthening.md` instead of inferring intent from a
  fixture's number of steps or actions.
- Each migration rung names the production entrypoint, the direct bypass being
  deleted, the existing fact owner, the last orchestration consumer, and one
  execution/parity/negative gate. Do not leave `Main -> old function` as a
  fallback beside the Pergyra-native path.
- `PgyCompilerWorld` is the executable composition root only for the
  `--mir-json-backend` direct-MIR slice already reached from
  `driver_bootstrap_main.pgy`. It remains the target root for source/MIR-to-C
  modes and for a canonical compiler-purpose intent until the production root
  reaches a real-purpose intent and deletes its direct bypass. Do not promote
  slice reachability into whole-root dogfood.
