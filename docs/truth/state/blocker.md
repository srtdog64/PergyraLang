# Current blocker

Owner: Claude lane (main-line work since 2026-10-18,
[decision](../decisions/2026-10-18_native_checker_in_v1.md)). Checked
2026-10-20 KST at `db8350709` on branch `claude/p0-native-checker`
(local commits, not pushed).

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`: 2870 downgraded
refusals with every final owner run (4401 at `2d30ec766`;
[census record](../evidence/2026-10-20_native_census_landings.md)).
A census runs up to four analyses of about 41 minutes each. The CI
harness manifest compiles natively since `e23761a3a`.

## Open fronts, in order

1. **`direct_mir_*` routes:** 808 refusals, removed with the routes as
   the general emitter covers their programs (self-host generality).
2. **Moves (1247):**
   - 571 getters whose loan proof fails;
   - 431 callees that change, keep or release;
   - 177 read-and-return callees whose storage proof fails;
   - 69 getters on locals.
3. **Borrowed `ref` escapes (945):** mostly a `ref` formal whose own
   summary has a call escape.
4. **Strings that leave their owner.** Owned copies land for a formal's
   String stored into an `inout` output or a local, and for an indexed
   String pushed into an Array
   ([decision](../decisions/2026-10-19_string_landing_copy_then_move.md)).
   Left: 265 opaque crossings, 131 released elements, 25 owned pushes on
   escaped storage.
5. **Hidden owners:** `collection_owned_element_requirements_finalize`
   fails on the canonical. It has three refusals, from a field taken out
   before an `own` retire
   ([OPEN](../decisions/2026-10-19_open_field_take_before_own_handoff.md)).
   So a normal compile never runs the move, lane and named-call owners.
6. **Codegen gaps the checker admits:** a by-value `self` method on a `ref`
   receiver, and `graph.blocks[0].name = x` (C emits a non-lvalue).

## Lessons on the record

- **An incomplete frame is not a safety rule.** Main admitted a pool's text
  read after `AllocatorDestroy` (ASan heap-use-after-free); `bc2aefb30`
  refuses it by an owner.
- **Proofs must walk every place a formal's storage can go.** Main skipped
  a `for` loop's iterable, so pushing onto each element of a by-value
  Array was admitted (ASan heap-use-after-free); `99a31fe0f` follows it.
- **Census each commit that changes what a frame models or what lands.**
  Two censuses on this branch were invalid (`98f4b7747`, `5cf9d7553`): one
  failed flow store turns every later refusal into fail-closed noise.
- **Emitted code frees nothing** on `ArraySet`, reassignment or a move yet,
  so most owner violations leak instead of failing at run time.

## CI comparison

`make ci-push-linux` at `2333da64c` failed the same 49 of 65 steps as main
`7cd004fab` (not rerun since). Bootstrap, installed driver, gen2 == gen3
and required CI are not established on this branch.
