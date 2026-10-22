# Current blocker

Owner: GPT root lane for main integration and the one reached ownership
producer at a time. Checked 2026-10-23 KST at source commit `674fb5903`;
nothing is pushed while this state is red.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`. The fixed final-owner
census is now 310, down from 319 by nine semantic refusals with no semantic
addition and no hard census error
([receipt](../evidence/2026-10-23_mir_routine_and_transition_storage_census.md)).
The run took 1028.91 s, inside the 30-minute integration budget.

## Active executable rung

- Invariant: a result retained after its producer continues must own every
  mutable aggregate and String column it keeps.
- Closed slice: MIR routine String rows and DIR-retained intent transitions
  now cross that boundary through owned storage.
- Next falsifier: `AstTreeArtifactBindExpressionOwnerKinds` at
  `src/self_hosted/hir/ast_expression_owner_kind_binding.pgy:71` still cannot
  mutate escaped `atom_table` storage through `ArrayPushOwnedString`.
- Required before editing: map `atom_table`'s producer, all aliases, the last
  consumer, and its release point. Do not infer the contract from this first
  diagnostic alone.
- Forbidden fallback: broad `ref`, whole-analysis `Clone`, a per-program
  `direct_mir_*` route, or weakening the ownership refusal.
- Acceptance: the same fixed census removes the mapped semantic rows, adds no
  semantic refusal, and preserves the component and ownership negative gates.

The remaining 310 include String owner crossings, release sites, preserved
Array storage, move/loan proofs, and 42 `direct_mir_*` routes. They are not 310
independent defects. Bootstrap, installed-driver C/LLVM, gen2 == gen3 and CI
remain downstream and cannot be called green before canonical admission is
zero.
