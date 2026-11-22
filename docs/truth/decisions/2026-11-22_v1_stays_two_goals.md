# 2026-11-22: V1 stays two goals

Decided by: the user. A parallel Claude session relayed the decision on
2026-11-22, and the user restated the goal in this lane the same day:
closing the SoT rows and self-hosting.

V1 keeps the definition of [V1 is two goals](2026-10-14_v1_scope.md), as
amended by the [2026-10-25 Clone decision](2026-10-25_real_sharing_site_may_take_explicit_clone.md):
ownership automation and self-host generality, both holding on one published
commit. V1 is not split into an ownership-only release with generality
deferred.

## Goal-2 measurement

Progress on self-host generality is read from numbers, not from closed SoT
rows (`AGENTS.md`, "Not in V1"):

- the default-route LLVM sweep against native: equal programs, non-equal
  programs among those native compiles, and their first-error clusters;
- the size of `src/self_hosted/compiler/direct_mir/`, which goes to zero as
  the general emitter replaces per-shape routes;
- the first refusal ordinal of the full DRV-2 MIR.

The baseline and later values are in [measurements](../state/measurements.md).
