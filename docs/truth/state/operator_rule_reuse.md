# Native operator reuse slice

Owner: operator-reuse lane, user-authorized 2026-10-24. Main integration,
shared ownership changes and push remain with the main worker.

- Base: `ad920678bdaef3865f2d27f88e47a9cbb1d7d83e`.
- Vocabulary: `646e74af757c6c8508b9fc478fb3b1ec01852173`.
- LLVM priority correction and executable regression gate: the commit that
  contains this state; exact binaries and runs are in the [receipt](../evidence/2026-10-24_native_operator_rule_reuse.md).
- Plan and boundary: [operator reuse](../../agent_work_directives/operator_rule_reuse_2026-10-24.md).
- The native semantic checker and both emitters now consume one overload
  vocabulary. No backend-local suffix/alias/supported-token list remains.
- Scoped implementation is complete. Native operator-target resolution is
  still inventory-based; this is not its MIR-identity SoT closure.
- Focused vocabulary, exact-output C/LLVM, rejection, wrong-rule mutation,
  semantic and C emitter gates passed; see receipt for exact counts/scope.
- Broader compiler-boundary gate remains red on a String ownership failure
  reproduced with the unchanged baseline. Do not claim full CI green.
- A separate native C temporary-subject/free-operator ABI falsifier remains
  open and is reproduced in the receipt, not converted into an accepted test.
- No new worktree, installed-binary replacement, push, canonical census,
  bootstrap, installed-driver run or self-host closure claim.
- Next integration obligation: main worker combines the separate dirty
  changes, reruns the broader gates, and owns publication/CI.
