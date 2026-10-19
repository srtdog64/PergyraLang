# Current blocker

Owner: Claude lane (main-line work since 2026-10-18,
[decision](../decisions/2026-10-18_native_checker_in_v1.md)). Checked
2026-10-19 KST at `bc2aefb30` on branch `claude/p0-native-checker`
(local commits, not pushed).

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`: 5511 downgraded
refusals with every final owner run (6915 at `a4e2649c8`).
[Census record](../evidence/2026-10-19_native_census_final_owners.md).
The CI test harness manifest, which every codegen bootstrap builds first,
compiles on the native pipeline since `e23761a3a`; main refuses it.

## Open fronts, in order

1. **`direct_mir_*` routes:** 1318 refusals, removed with the routes as
   the general emitter covers their programs (self-host generality).
2. **Moves (2786).** 803 formal getters whose loan proof fails; 768
   callees that change, keep or release; 647 getters on locals, moved by
   [decision](../decisions/2026-10-19_by_value_getter_lends_when_used_again.md)
   until a transfer is refused while a loan of the same local lives; 586
   read-and-return callees whose Array descriptor proof fails.
3. **Borrowed `ref` escapes (1430):** mostly definition-site call escapes.
4. **String elements:** 315 opaque crossings, 169 preserved-storage, 131
   released elements, 112 owned pushes. The [String
   landing](../decisions/2026-10-19_string_landing_copy_then_move.md)
   (owned copy at stores no proof admits) is not started.
5. **Hidden owners:** `collection_owned_element_requirements_finalize`
   fails on the canonical (three refusals, a field taken out before an
   `own` retire: [OPEN](../decisions/2026-10-19_open_field_take_before_own_handoff.md)),
   so a normal compile never runs the move, allocator lane and named-call
   owners.
6. **Codegen gaps the checker admits:** a by-value `self` method on a `ref`
   receiver, and `graph.blocks[0].name = x` (C emits a non-lvalue).

## Lessons on the record

- An incomplete frame is not a safety rule. Main admitted a pool's text
  read after `AllocatorDestroy` (ASan heap-use-after-free) even though the
  allocator path was unsupported; `bc2aefb30` refuses it by an owner.
- A loan is proved at call rows. A shape with no row (a receiver) cannot
  be lent: `4f1c33b93` let a `ref self` String getter lend, fixed in
  `aa4fc2022`.
- A struct formal is a shallow copy: a store through `arena.atoms` changes
  the caller's storage. Until `b68ae872c` such a callee read as read-only
  and its caller read freed memory (ASan, on main too).
- Emitted code frees nothing on `ArraySet`, reassignment or a move yet, so
  most owner violations leak instead of failing at run time.

## CI comparison

`make ci-push-linux` at `2333da64c` failed the same 49 of 65 steps as main
`7cd004fab`. Its first self-host step stopped at the harness manifest,
which builds since `e23761a3a`; not rerun since.
`aggregate_storage_transfer_smoke` and `hashmap_owned_string_provenance_smoke`
were red at `7cd004fab` already. Bootstrap, installed driver, gen2 == gen3
and required CI are not established on this branch.
