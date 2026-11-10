# Current blocker

Owner: Claude root lane. Checked 2026-11-11 09:00 KST on `ci/all-green`
(main `1acc31c5b` plus the branch commits after it).

## Objective card

- Objective: required CI green on one published commit, with the bootstrap
  and installed-driver jobs executed.
- Priority: semantic identity and one fact owner; fail-closed admission;
  C/LLVM parity; fixed point; installed execution; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Native analysis time. CI at `1acc31c5b`: oracle C emit 24 minutes, the
   fixed point job 58 of 60 minutes. The identity cell gate (probe build
   about 7 minutes locally) and the nominal source arity probe (about 2.5
   minutes) time out under their 120 s limits on CI as well.
   - Kept: hashed rows currency walk (`69cfcea20`). Not kept: a hashed
     type resolution edge index and sorted landing id lookups; byte-identical
     on the canonical driver but under 1% faster (1,324 s against 1,333 s).
   - Rejected: copy retention across store growth
     ([record](../evidence/2026-11-11_landing_copy_retention_rejected.md)).
2. Fixed point after gen2 == gen3 (all passed on CI up to it):
   `self-host-array-index-type` needs Slice parameters (`ref`/`inout`) in
   both self-host emitters; the fixture never compiled on any driver.
3. Contracts shard on CI: 3 and 4 are the timeouts above; 10
   `effect_array_transform_valid` (landed Clone on an `ArraySort`
   receiver is not a mutable place); 11 fixed on the branch (registry,
   cap, parser fixtures, cleanup classification).
4. Core shard on CI: 5 role override (the general C emitter lacks the
   erased role receiver ABI); 6 `fact.value` into `own` refused by the
   self-host owned-String proof; 24 LLVM `Clone` of a subject ends in
   `Die`; 32 qubit builtins have no C emission; 34 native parallel join
   slot analysis fails closed; 39 fixed on the branch.
5. Backend compare on CI: four shards red; the native refusals of
   `generic_shadowed_argument_binding`, `option_of_result_consumers`,
   `lambda_reduce` and `function_pointer_reduce` (also in the 11-07 tree);
   `generic_default_ability_bind_dispatch` fixed on the branch.

## Falsifiers kept

- A struct constructor that leaves a field out is refused by both front
  ends; domain nominals may still omit trailing fields.
- `let` passed to inout stays refused for values; identity hosts follow
  native's field and action authority.
- An ordinary routine signature carrying the Intent `participant`
  carriage is refused by the signature schema.
- The string window negative `reject_cloned_record_source_unproven` is
  refused again on the self-host route.
