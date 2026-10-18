# Current blocker

Owner: Claude lane (main-line work since 2026-10-18,
[decision](../decisions/2026-10-18_native_checker_in_v1.md)). Checked
2026-10-18 KST at `b336f4ce7` on branch `claude/p0-native-checker`.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`. Method and counts:
[first census](../evidence/2026-10-18_native_refusal_census.md),
[flow progress](../evidence/2026-10-18_native_census_flow_progress.md).

## Open fronts, in order

1. **Flow engine completion.** The analysis now reaches 8390 flow entries
   and fails closed at
   `DirectMirScalarProgramIndexedAssignmentRouteFromOwners#1`
   (`compiler/direct_mir_scalar_program_indexed_assignment_route_owner.pgy:168`):
   a store of that input's backing into a non-local binding with no
   output destination. An import wrapper of the owner passes; only the
   full program demands this relation. Next: the store target, from a
   full-program store trace.
2. **String element precision (proof route, user choice).** 767
   opaque-boundary sites, mostly `ArrayPush(dst, src[i])` between local
   owned arrays. GPT's untracked 2026-10-18 directive
   (`indexed_string_array_push_ownership_chain`) lays out the copy-or-move
   decision and its MIR/ABI and cleanup chain.
3. **Deferred Array storage refusals.** 1475 "Array argument call
   requires preserved array storage"; re-count once front 1 is closed.

The flow work cap is settled:
[decision](../decisions/2026-10-18_flow_budget_counts_closure_entries.md).

## Known red at base, unchanged on the branch

`aggregate_storage_transfer_smoke` (12 C/LLVM checks) and
`hashmap_owned_string_provenance_smoke` (`map_keys_double_drop`) fail the
same way at `7cd004fab`. The broader backend and parity failures in
[gates](gates.md) are not re-measured here. Bootstrap, installed driver,
gen2 == gen3 and required CI are not established on this branch.
