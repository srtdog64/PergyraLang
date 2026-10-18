# Current blocker

Owner: Claude lane (main-line work since 2026-10-18,
[decision](../decisions/2026-10-18_native_checker_in_v1.md)). Checked
2026-10-19 KST at `ebf6f2742` on branch `claude/p0-native-checker`.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`. Its parameter-flow
analysis now completes for the whole program (no fail-closed summary);
8397 downgraded refusals of other owners remain (9730 before Split and
Slice views were modelled in `ebf6f2742`). Counts and history:
[flow complete](../evidence/2026-10-19_native_census_flow_complete.md).

## Open fronts, in order

1. **Borrowed String returns and `ToString`.** About 7100 of the
   refusals are borrowed `ref` values escaping by call (5155), return
   (1236) or channel (747). The callees most blamed are getters that
   return a String element of a `ref` container, such as
   `SemanticExpressionGraphNodeText` (`Some(facts.arena.node_texts[i])`):
   a return-site owned-copy landing under the String decision would cut
   that relation. `ToString` is still unmodelled: with its relation the
   C emitter's recursive SCC (`EmitStmtList#5`) becomes supported, a
   callee it reaches (`CompilerRuntimeCallAbiFactFromRowInput#0`) is
   unproved during the solve and proved only at its last evaluation, and
   the analysis fails closed at 4600 entries. Two solver relaxations did
   not change that; both are kept in scratch.
2. **Array storage and String owners.** 633 preserved-storage refusals,
   192 opaque String crossings, 131 released-element uses, 112 owned
   pushes on escaped storage (565 preserved-storage refusals since
   `ebf6f2742`). String landings beyond `ArrayPush`
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
