# Agent Rules

This repository is in beta-closure mode. Prefer source-of-truth closure over
cosmetic reshuffling.

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
