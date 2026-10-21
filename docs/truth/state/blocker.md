# Current blocker

Owner: Claude lane, main-line work since 2026-10-18
([decision](../decisions/2026-10-18_native_checker_in_v1.md)). Checked
2026-10-21 KST on branch `claude/p0-native-checker` (local, not pushed).

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`: 1050 downgraded
refusals with every final owner run
([census record](../evidence/2026-10-21_native_census_unit_variant_holds_no_storage.md)).
A census runs four analyses in about 820 s on an idle machine. The CI
harness manifest compiles natively since `e23761a3a`.

## Open fronts, in order

1. **`direct_mir_*` routes:** 306 refusals, removed with the routes as
   the general emitter covers their programs (self-host generality).
2. **Moves (266 outside `direct_mir_*`)**, mostly callees that change,
   keep or release ([OPEN](../decisions/2026-10-21_open_copy_a_moved_argument_used_again.md)),
   and 70 local copies whose other side changed (`inout` counted whole).
3. **Borrowed `ref` escapes (150 outside `direct_mir_*`):** seeded by stores
   into an `inout` output, builtins with no relation, and own/inout handoffs.
4. **Strings that leave their owner.** Owned copies land for a formal's
   String stored into an `inout` output or a local, and for an indexed
   String pushed into an Array
   ([decision](../decisions/2026-10-19_string_landing_copy_then_move.md)).
   Left: 86 opaque crossings, 8 released elements, 23 owned pushes.
5. **Hidden owners:** `collection_owned_element_requirements_finalize`
   has three refusals on the canonical, from a field taken out before an
   `own` retire ([OPEN](../decisions/2026-10-19_open_field_take_before_own_handoff.md)).
   So a normal compile never runs the move, lane and named-call owners.
6. **Codegen gaps the checker admits:** a by-value `self` method on a `ref`
   receiver, `graph.blocks[0].name = x` (C emits a non-lvalue), and an
   `Array<String>` born `[]` in a `while` body: MIR retires one after builtin
   uses (GPT `c9513d5b2`); declared calls wait on GPT carrying the published
   [Array verdict](../decisions/2026-10-21_array_preservation_published_per_formal.md).
   A re-entered inner loop no longer stops HIR ([rule](../decisions/2026-10-22_reentered_loop_exports_last_transfer.md)).
7. **Preserved Array storage (43 outside `direct_mir_*`):** calls after an
   earlier call that may keep or rebind the Array (`EmitStmtList` 8).

## Lessons on the record

- **An incomplete frame is not a safety rule.** Main admitted a pool's text
  read after `AllocatorDestroy` (ASan use-after-free); `bc2aefb30` refuses it.
- **Proofs must walk every place a formal's storage can go:** a `for`
  iterable (`99a31fe0f`) and a getter's result (ASan use-after-free;
  [rule](../decisions/2026-10-20_call_result_holding_formal_storage_is_its_path.md)).
- **Local copies share storage:** a push through `let b = a.inner`, then a read
  of `a` ([rule](../decisions/2026-10-21_local_copies_refuse_the_other_side_after_a_change.md)).
- **A defer runs at every exit, not where it is written.** The branch admitted
  `defer { ArrayDrop(rows); }` then `return Keep(rows)` (ASan heap-use-after-free;
  [record](../evidence/2026-10-21_defer_release_admitted_at_each_exit.md)).
- **Emitted code frees nothing** on `ArraySet`, reassignment or a move: most
  owner violations leak instead of failing at run time.

## CI comparison

`make ci-push-linux` at `2333da64c` failed the same 49 of 65 steps as main
`7cd004fab` (not rerun since). Bootstrap, installed driver, gen2 == gen3
and required CI are not established on this branch.
