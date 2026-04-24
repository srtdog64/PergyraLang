# 05. Parallel / Execution Proof Obligations

Last updated: 2026-04-25

Status: `IN PROGRESS`

Keywords: `parallel`, plus execution-family surfaces such as `spawn`, `async`, `await`, `select`, `channel`, and cancellation.

## Stable Surface

- `parallel` as the core execution primitive.
- Conflict/failure baseline for slot/resource/effect interaction.
- Channel transport ownership checks where covered by current semantic regression.
- C/LLVM lowering parity for stable execution cases.

Out of beta or compatibility surface:

- Fiber and coroutine fairness theorem.
- Full scheduler proof.
- Advanced cancellation algebra.
- Rich async library combinators.

## Judgments

```text
Gamma; ResourceState |- parallel { steps } ok
ResourceState |- conflict_free(steps)
ZoneState; ResourceState; History |- parallel_step => ZoneState'; ResourceState'; History'; outcome
```

## Theorem: Parallel Conflict Soundness

If a stable `parallel` block is accepted, resource/effect conflicts that are statically visible in the frozen subset are either rejected or represented as recoverable runtime failure.

Assumptions:

- Resource/effect conflict rules are derived from the same authority-resource-effect order used by relation/effect/projection semantics.
- Channel transport rules preserve ownership boundaries.

Current evidence:

- Channel transport validator/reporting has been separated.
- `parallel` is documented as core, while fiber/coroutine are runtime mechanisms below the core surface.

Remaining proof obligation:

- Tie `parallel` conflict checks directly to the relation/effect proof vocabulary.

## Theorem: Execution Backend Parity

Stable execution-family lowering must produce equivalent C and LLVM observable behavior.

Current evidence:

- Backend compare covers current stable examples.
- Unsupported LLVM stmt/expr fallback is moving toward structured backend error.

Remaining proof obligation:

- Add more execution edge cases around cancellation, channel readiness, and resource conflict.
