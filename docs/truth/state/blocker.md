# Current blocker

Owner: Claude lane (main-line work since 2026-10-18,
[decision](../decisions/2026-10-18_native_checker_in_v1.md)). Checked
2026-10-19 KST at `9b9b1ba0f` on branch `claude/p0-native-checker`.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`, and the first CI
step's manifest program (`compiler/test_harness_manifest.pgy`). Method and
counts: [first census](../evidence/2026-10-18_native_refusal_census.md),
[flow progress](../evidence/2026-10-18_native_census_flow_progress.md).

## Open fronts, in order

1. **World member storage.** The flow analysis reaches 10938 entries and
   fails closed at `CompileSourceToC` (`compiler/world.pgy:333`):
   `Clone(source_c)` names a world member (a host field of `self`) that no
   frame row holds. Mapping a host field to the `self` parameter is not
   enough: a `ref self` method returning a member that holds an Array is
   then admitted, because no borrow-escape report covers `self`. Needs a
   design for `self` and world-member lifetime (and whether `Clone` of a
   zone member is a deep, declared copy at runtime).
2. **String the proof route cannot alias.**
   [OPEN decision](../decisions/2026-10-18_open_string_push_landing.md):
   copy or move landing. 438 opaque-boundary sites remain (249 push, 124
   constructor field, 46 plain store, 19 set); the manifest program needs
   two, a push and a retained call argument.
3. **Allocator births.** A pool or builder created from Int arguments has
   no checked relation, and that gap is what refuses returning a String
   from a destroyed pool (`pool_builder_birth_invalid`). Modelling it means
   modelling allocator destruction.
4. **Deferred Array storage refusals.** 1475; re-count once front 1 is
   closed.

## Lesson on the record

An incomplete frame can be the only guard for an unmodelled hazard. Two
completeness changes this session (a scalar-argument skip and the host
field mapping) each admitted an unsafe program and were reverted before
commit. Every completeness change needs an adversarial negative for the
hazard the old incompleteness covered.

## Known red at base, unchanged on the branch

`aggregate_storage_transfer_smoke` (12 C/LLVM checks) and
`hashmap_owned_string_provenance_smoke` (`map_keys_double_drop`) fail the
same way at `7cd004fab`. Bootstrap, installed driver, gen2 == gen3 and
required CI are not established on this branch.
