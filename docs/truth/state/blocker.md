# Current blocker

Owner: Claude lane (main-line work since 2026-10-18,
[decision](../decisions/2026-10-18_native_checker_in_v1.md)). Checked
2026-10-18 KST on branch `claude/p0-native-checker`.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`. Census method and
counts: [census record](../evidence/2026-10-18_native_refusal_census.md).
GPT's V10 records and the `ArrayPush(codes, found[j])` plan remain in
`evidence/` and the 2026-10-18 directive.

## Three open fronts, in order

1. **Flow engine completion.** With the work budget lifted, the full
   analysis still fails closed: "required return backing relation is
   incomplete or stale" after the `SelfMirLowerLetFromArtifact` closure.
   Next: the failing entry and condition, from a
   `mir/routine_let_owner.pgy` wrapper reproduction.
2. **Flow work budget.** The cap is a fixed 4,096 per demand closure; the
   largest canonical closure needs about 28,000 (3 passes over 65
   bodies). The flow smoke gate pins 4,096 and a 4,096-leaf fan-out that
   must fail. A size-proportional bound needs a matching gate change.
3. **String element precision (proof route, user choice).** 767
   opaque-boundary sites in 182 functions remain, mostly names copied
   from one fact table into another. Next: carry where a String value
   came from through locals, calls and fields, so a value that cannot be
   an element of a local owned array is not checked against one.

The broader backend and parity failures GPT recorded in [gates](gates.md)
are not re-measured here. Bootstrap, installed driver, gen2 == gen3 and
required CI are not established on this branch.
