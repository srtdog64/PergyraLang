# Current blocker

Owner: Claude lane (main-line work since 2026-10-18,
[decision](../decisions/2026-10-18_native_checker_in_v1.md)). Checked
2026-10-19 KST at `73e3a3187` on branch `claude/p0-native-checker`.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`. Its parameter-flow
analysis now completes for the whole program (no fail-closed summary);
9730 downgraded refusals of other owners remain. Counts and history:
[flow complete](../evidence/2026-10-19_native_census_flow_complete.md).

## Open fronts, in order

1. **Conservative summaries behind unmodelled stdlib calls.** About 8400
   of the refusals are borrowed `ref` values escaping by call, return or
   channel, mostly through summaries that are unsupported (every escape)
   because a frame reaches `ToString`, `Split` or `xs.Slice`. Relations
   for those three are written but not landed: once the C emitter's
   recursive SCC (`EmitStmtList#5`) is supported, the row-currency check
   refuses its declared-result edges while their callees are pending in
   the same recursion, and the analysis fails closed earlier.
2. **Array storage and String owners.** 633 preserved-storage refusals,
   192 opaque String crossings, 131 released-element uses, 112 owned
   pushes on escaped storage. String landings beyond `ArrayPush`
   (constructor field, plain store, `ArraySet`) are not landed.
3. **World member storage, still open.** Stores into a host field fail
   closed; deep struct `Clone` is decided but not emitted; a by-value
   receiver its method grows in place is not moved (a class method
   `Add(self, v)` doing `ArrayPush(items, v)`, called twice, at
   `7cd004fab` too).
4. **Allocator births.** A pool or builder created from scalar arguments
   has no checked relation, and that gap is what refuses
   `pool_builder_birth_invalid`.

## Lesson on the record

An incomplete frame can be the only guard for an unmodelled hazard. This
session it was: a host field read through `self` (guarded `ref self`
returns and by-value `self` Array returns), a match binding (guarded
enum moves), and scalar builtin actuals (guarded allocator births). Each
was landed only with the refusal it exposed, pinned by a negative.

## Known red at base, unchanged on the branch

`aggregate_storage_transfer_smoke` (12 C/LLVM checks) and
`hashmap_owned_string_provenance_smoke` (`map_keys_double_drop`) fail the
same way at `7cd004fab`. Bootstrap, installed driver, gen2 == gen3 and
required CI are not established on this branch.
