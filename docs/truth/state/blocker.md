# Current blocker

Owner: GPT root lane for main integration and the one reached ownership
producer at a time. Checked 2026-10-23 KST at source commit `4716f9539`;
nothing is pushed while this state is red.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`. The fixed final-owner
census is now 309. The graph-atom slice removed exactly one semantic refusal
from the 310 baseline, added none and produced no hard census error
([receipt](../evidence/2026-10-23_ast_graph_atom_prebind_census.md)). The run
took 1432.40 s, inside the 30-minute integration budget.

## Active executable rung

- Invariant: borrowed String descriptors and their source owner travel through
  one artifact, and no consumer grows the escaped descriptor array.
- Closed slice: graph-text atom slots are filled before escape; both binders
  consume validated scalar indices and preserve the empty sentinel.
- Next falsifier: `AstDestructurePatternBindingNameRowsDrop` cannot release
  escaped owned-String storage in `names`.
- Required before editing: map all eight remaining `ArrayDropOwnedStrings`
  rows to their producers, element provenance, aliases and last consumers.
  Group them only if source proves one common ownership contract.
- Forbidden fallback: broad `ref`, whole-analysis `Clone`, a per-program
  `direct_mir_*` route, borrowed deep drop, or weakening the refusal.
- Acceptance: one mapped release contract, focused positive/negative evidence,
  and the same fixed census with no semantic addition.

The remaining 309 include String owner crossings, release sites, preserved
Array storage, move/loan proofs, and 42 `direct_mir_*` routes. They are not 309
independent defects. Bootstrap, installed-driver C/LLVM, gen2 == gen3 and CI
remain downstream and cannot be called green before canonical admission is
zero.
