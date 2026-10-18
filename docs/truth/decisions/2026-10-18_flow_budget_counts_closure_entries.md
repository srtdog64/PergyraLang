# 2026-10-18: the flow work cap counts closure entries, not frame nodes

Decided by: the Claude lane, under the user's request to take over the
main-line work and make required CI green. The user may reverse it.

## Rule

The parameter-flow demand closure keeps its fixed cap of 4096 work units.
A unit is one entry evaluation or one no-store graph member visit. A
returned-backing frame's node visits are no longer units:

- each frame round visits a node at most once;
- a frame repeats a round only after a binding's origin or destination bit
  grew, so a build stops within `bindings × (origins + destinations) + 1`
  rounds, and a build that does not is refused ("backing frame rounds
  exceeded its growth bound");
- a loan-seed walk keeps its own budget.

## Why

The full canonical program had six closures above the cap. The largest was
`SemanticAstCollectionExclusiveClassify`, at 28258 units for 65 entries.
It is a 26-parameter recursive function: each input has its own frame
over the same ~360-node body, for three passes. Charging those visits made
the cap depend on parameter count times body size instead of closure
size, so legitimate closures failed closed.

- A larger or program-proportional cap would have needed the gate's
  4096-leaf fan-out control rewritten. Under this rule that control is
  unchanged and still fails at exactly 4096 units.
- At 8192 leaves the flow engine's work is linear (16388 units). Most of
  the time goes to other passes (AIR/RIR, MIR validation, type
  resolution).
- The cap did hide one repeated operation. A recursive pair with 20 Array
  inputs took 12.8 s once uncapped: each rows validation re-checked a
  callee's whole claim set for every frame, call and argument (587M
  publication lookups). One validation pass now reuses the indexes it has
  already found current; `PGY_VERIFY_FLOW_CURRENCY_MEMO` re-checks every
  reuse. The same program now takes 2.5 s. The canonical program's
  analysis time did not change (about 208 s on the census build).
- Merging per-input frames into one walk would remove the repeated
  bodies. Frames carry one origin by design, so that is a redesign.

Gate: the flow summary gate's wide-frames control, a recursive pair with six
Array inputs and 100-line bodies. It is admitted with 26 units while one
closure visits 5040 frame nodes; before this rule it failed at the cap.
The unchanged 4096-leaf fan-out control still fails at exactly 4096.

Replaces: nothing.
