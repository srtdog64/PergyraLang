# Current blocker

Owner: Claude lane (main-line work since 2026-10-18,
[decision](../decisions/2026-10-18_native_checker_in_v1.md)). Checked
2026-10-19 KST at `ae75a2467` on branch `claude/p0-native-checker`.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`, and the first CI
step's manifest program (`compiler/test_harness_manifest.pgy`). Method and
counts: [first census](../evidence/2026-10-18_native_refusal_census.md),
[flow progress](../evidence/2026-10-18_native_census_flow_progress.md).

## Open fronts, in order

1. **Intent calls.** The flow analysis reaches 10938 entries. A host field
   now reads as `self` storage, so `Clone(source_c)` no longer stops it;
   it fails closed one call later, at the intent call
   `CompilePergyraCArtifact(...)` inside `CompileSourceToC` (frames #0-#3,
   reported at `compiler/compiler_world_direct_mir_owner.pgy:63`). An
   intent is not a routine the analysis can enter. Same shape, scratch
   repro: a world member passing a path to an intent whose step action
   stores it in a subject.
2. **World member storage, still open.** Stores into a host field fail
   closed (a subject's `self` is its identity, so a write through it is an
   output). Deep struct `Clone` is decided but not emitted; until it is, a
   nominal `Clone` result is checked as dependent on its argument. A
   by-value receiver its method grows in place is not moved, at
   `7cd004fab` too: a class method `Add(self, v)` doing
   `ArrayPush(items, v)`, called twice; the method's slot summary does not
   see host-field uses.
3. **String the proof route cannot alias.** Decided: owned-copy landing.
   `ArrayPush` lands (`61f1e9358`). Constructor field, plain store and
   `ArraySet` sites are not landed; not re-counted since. The manifest
   program still needs a retained call argument (265:55).
4. **Allocator births.** A pool or builder created from Int arguments has
   no checked relation, and that gap is what refuses returning a String
   from a destroyed pool (`pool_builder_birth_invalid`).
5. **Deferred Array storage refusals.** 1475; re-count after front 1.

## Lesson on the record

An incomplete frame can be the only guard for an unmodelled hazard. The
host field mapping was landed only with the two escape reports it
exposed (`ref self` returns, by-value `self` Array returns), each pinned
by a negative whose exact refusal a scratch mutation of that guard loses.

## Known red at base, unchanged on the branch

`aggregate_storage_transfer_smoke` (12 C/LLVM checks) and
`hashmap_owned_string_provenance_smoke` (`map_keys_double_drop`) fail the
same way at `7cd004fab`. Bootstrap, installed driver, gen2 == gen3 and
required CI are not established on this branch.
