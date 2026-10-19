# Current blocker

Owner: Claude lane (main-line work since 2026-10-18,
[decision](../decisions/2026-10-18_native_checker_in_v1.md)). Checked
2026-10-19 KST at `aa4fc2022` on branch `claude/p0-native-checker`.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`. Its parameter-flow
analysis completes for the whole program. Downgraded refusals of other
owners: 3667 (4957 before results lent `ref` inputs, 7778 before
`ToString` was modelled). Counts by commit:
[loans and ToString](../evidence/2026-10-19_native_census_loans_tostring.md).

## Open fronts, in order

1. **`direct_mir_*` routes.** 1360 of the refusals are inside the
   per-program-shape `direct_mir_*` modules. V1 deletes those routes as
   the general emitter covers their programs (self-host generality).
   Their refusals are not ownership work.
2. **Borrowed text stored past its owner.** Most of the rest are borrowed
   Strings or loans stored into a container that outlives the owner:
   pushed into an `inout` Array, set into a field or constructor argument
   of a value the caller keeps. The [String
   landing](../decisions/2026-10-19_string_landing_copy_then_move.md)
   applies there: a compiler-selected owned copy, visible in MIR, at the
   stores no loan proof admits. Not started; it spans the flow owner, MIR
   lowering and both emitters.
3. **Array storage owners.** 320 preserved-storage refusals, 192 opaque String
   crossings, 131 released-element uses, 112 owned pushes on escaped
   storage.
4. **World member storage, still open.** Stores into a host field fail
   closed; deep struct `Clone` is decided but not emitted; a by-value
   receiver its method grows in place is not moved.
5. **Allocator births.** A pool or builder created from scalar arguments
   has no checked relation (`pool_builder_birth_invalid`).
6. **Receiver codegen.** A by-value `self` method called on a `ref`
   receiver does not build on C or LLVM; the checker admits that shape at
   `7cd004fab` too.

## Lessons on the record

- An incomplete frame can be the only guard for an unmodelled hazard; land
  a model only with the refusal it exposes.
- A loan is proved at call rows. A shape with no row (a receiver) cannot
  be lent: `4f1c33b93` let a `ref self` String getter lend and admitted a
  caller returning past its receiver, fixed in `aa4fc2022`.
- A returned loan is safe only while its owner is unchanged. Emitted code
  frees nothing on `ArraySet`, reassignment or a move yet, so a violation
  leaks instead of failing. The aggregate route checks the owner; the
  `ref String` route does not (as at `7cd004fab`).

## Known red at base, unchanged on the branch

`aggregate_storage_transfer_smoke` (12 C/LLVM checks) and
`hashmap_owned_string_provenance_smoke` (`map_keys_double_drop`) fail the
same way at `7cd004fab`. Bootstrap, installed driver, gen2 == gen3 and
required CI are not established on this branch.
