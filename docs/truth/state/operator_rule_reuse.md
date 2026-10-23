# Native operator reuse and subject ABI slice

Owner: operator-reuse lane, user-authorized 2026-10-24. Main integration,
shared ownership changes and push remain with the main worker.

- Earlier vocabulary and free-over-role work:
  `646e74af757c6c8508b9fc478fb3b1ec01852173` and
  `0c4e5812d71e6420a3107dc595e3ffe19f7f4f8e`;
  [earlier receipt](../evidence/2026-10-24_native_operator_rule_reuse.md).
- Current ABI base: `7313e9dfc6e45ffab10e7d5f665e576af4623f3d`.
- Common subject storage/constructor policy:
  `ce7af5fa06317322098e042bc2637a1761d2ba0b`.
- Operator consumers, behavioral fixtures and this state land together in the
  commit containing this update. Validation source was the preceding commit
  plus this lane's five operator source/header edits; exact executable hashes
  and limits are in the [ABI receipt](../evidence/2026-10-24_native_subject_operator_abi.md).
- [Bounded chain/plan](../../agent_work_directives/subject_operator_argument_abi_2026-10-24.md).
- Scoped ABI fix complete: fresh subject storage survives the call; existing
  subjects retain identity; C and LLVM use ordinary-call ABI policy for free
  operator arguments. Copy-in/out temporaries remain refused.
- Last green: operator runtime/rejection gate on C and LLVM, emitted-C
  sanitizers, semantic and C emitter units; copy-identity mutation detected.
- Next falsifier for integration: rerun those gates after combining the main
  worker's ownership edits. They were deliberately excluded from this build.
- Broader compiler-boundary gate remains red on a baseline String ownership
  refusal. A separate subject constructor-store fixture also fails in its
  baseline ownership summary. Neither is changed or claimed fixed here.
- Remaining obligation: main worker owns combined-tree validation, push and
  exact-SHA CI. No push or CI-green claim from this lane.
- No new worktree, installed-binary replacement, canonical census, bootstrap
  or installed-driver run. No operator-target SoT or self-host closure claim.
