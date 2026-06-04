# Agent Rules

This repository is in beta-closure mode. Prefer source-of-truth closure over
cosmetic reshuffling.

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

## Default Work Loop

1. Name the source-of-truth seam being closed.
2. Make the smallest code change that moves the decision behind that owner.
3. Add or tighten a smoke gate so the old path cannot reappear.
4. Run the narrow gate first; run broader tests only after the slice is stable.
