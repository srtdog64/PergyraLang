# Agent Rules

This repository is in beta-closure mode. Prefer source-of-truth closure over
cosmetic reshuffling.

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

- Do not add unnecessary helper functions. A helper must name a real owner
  responsibility, isolate a repeated contract, or remove a source-of-truth
  seam. A one-off wrapper that only hides local logic is debt.
- Do not create generic `*_helpers` buckets when a layer is getting too large.
  If a helper owner grows, split by responsibility and name the new owner after
  the responsibility.
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

- SoT is a hard-substitution rung condition, not an independent cleanup track.
  Close only the semantic seam reached by the one active executable rung.
- Do not make more than two consecutive SoT-only commits. Before a third, land
  an executable replacement delta or record the exact missing fact, owner,
  last consumer, and falsifying fixture as BLOCKED.
- Count progress only when a Pergyra implementation replaces a real C-owned
  compiler path. Owner files, tests, documents, and LOC are supporting evidence,
  not substitution progress by themselves.
- Budget the edit loop: 60 seconds for static owner gates, 5 minutes for focused
  parity, and 30 minutes for an integration shard. Full matrices belong at
  scheduled or merge boundaries.
