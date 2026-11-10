# Current blocker

Owner: Claude root lane. Checked 2026-11-11 06:00 KST on `ci/all-green`
(main `e875e9e90` plus the branch commits).

## Objective card

- Objective: required CI green on one published commit, with the bootstrap
  and installed-driver jobs executed.
- Priority: semantic identity and one fact owner; fail-closed admission;
  C/LLVM parity; fixed point; installed execution; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Native analysis time. It blocks the fixed point (oracle C emit 26 of 59
   minutes on CI), the identity cell gate (probe build 444 s against 120 s)
   and the nominal source arity probe (174 s against 120 s).
   - The landing fixed point is the cost: 25 analyses on the arity probe.
     An analysis right after a store wave, with the copy plan reset, spends
     43% in flow summary demands and takes 5x a cheap one.
   - Keeping copies across store growth was rejected: on the canonical
     driver it saved 14% and reached a different plan
     ([record](../evidence/2026-11-11_landing_copy_retention_rejected.md)).
   - Hashing the rows currency walk (`69cfcea20`) cut the arity probe from
     260 s to 194 s with byte-identical C. Next: the per-parameter slot
     escape walk repeated on every summary evaluation.
2. Contracts shard (local, tip driver): 3 identity probe timeout; 4 arity
   probe timeout; 10 `effect_array_transform_valid` (the landed Clone of
   the `ArraySort` receiver is not a mutable place); 11 sub-gates after the
   enum fixtures, not run yet. Step 7 (slice_copy) passes at `b8dadb0ca`.
3. Core shard (local, driver at `7d9f224c8`): 5 role override (the general
   C emitter claims it, lacks the erased role receiver ABI the test pins,
   and `-Werror` trips on the unused override); 6 copy-only control
   `fact.value` into `own`; 24 containment LLVM `Die` in expression
   admission (no JSON receipt); 32 qubit builtins have no C emission;
   34 diagnostic position: `native_only_parallel_join_stmt` no longer
   compiles natively; 39 string window negative accepted (the 11-07 driver
   refused it).
4. Backend compare: five native refusals of valid programs, the same in
   the 11-07 tree (`generic_default_ability_bind_dispatch`,
   `generic_shadowed_argument_binding`, `option_of_result_consumers`,
   `lambda_reduce`, `function_pointer_reduce`).
5. Fixed point after gen2 == gen3: legacy intent LLVM binder probe
   baseline fails on the tip driver.

## Falsifiers kept

- A struct constructor that leaves a field out is refused by both front
  ends; domain nominals may still omit trailing fields.
- `let` passed to inout stays refused for values; identity hosts follow
  native's field and action authority.
- Two AST nodes with one stable id are refused by the publication store.
