# Current blocker

Owner: Claude root lane. Checked 2026-11-11 10:30 KST on `ci/all-green`
(main `ec109c765` plus the branch commits after it).

## Objective card

- Objective: required CI green on one published commit, with the bootstrap
  and installed-driver jobs executed.
- Priority: semantic identity and one fact owner; fail-closed admission;
  C/LLVM parity; fixed point; installed execution; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Native analysis time. CI at `1acc31c5b`: oracle C emit 24 minutes, the
   fixed point job 58 of 60 minutes. Probe builds measured locally:
   - `nominal_constructor_source_arity_probe`: 160 s; with only the parser
     and body type owner imported, 155 s (139 s of it native semantic
     analysis: 25 landing rounds of about 5.5 s, nine store waves each
     resetting the copy plan). Fewer imports do not help. The installed driver refuses to
     build it (`borrow_boundary_escape` at
     `owned_string_local_reassignment_unit.pgy:26`).
   - `identity_cell_receiver_probe` without the projection owner: 310 s
     (was about 420 s); built by the installed driver: 126 s at 12.7 GB.
2. Fixed point after gen2 == gen3 (all passed on CI up to it):
   `self-host-array-index-type` needs Slice parameters (`ref`/`inout`) in
   both self-host emitters; the fixture never compiled on any driver.
3. Contracts shard: 3 identity cell probe build (item 1); behind it, 17
   public-origin rows fail locally at `stage=local_inventory ordinal=1` on
   the driver's own MIR (driver of an earlier local build). 4: the nominal
   gate now uses the installed driver (`49a6b92af`); the next gates in the
   same make chain build the arity probe (item 1). 10 fixed (`9f91cb3c1`).
4. Core shard on CI: 5 role override (the general C emitter lacks the
   erased role receiver ABI); 6 `fact.value` into `own` refused by the
   self-host owned-String proof; 24 default LLVM JSON refusal lacks
   `"stage":"llvm_codegen"`; 32 qubit builtins have no C emission; 34
   native parallel join slot analysis fails closed.
5. Backend compare: four shards; native ownership refusals of valid
   programs: `generic_shadowed_argument_binding` (array return from a
   generic non-own parameter), `option_of_result_consumers` (storage
   escape after `Pick`), `lambda_reduce` and `function_pointer_reduce`
   (preserved array storage across a function-value call).

## Falsifiers kept

- A struct constructor that leaves a field out is refused by both front
  ends; domain nominals may still omit trailing fields.
- `let` passed to inout stays refused for values; identity hosts follow
  native's field and action authority.
- An ordinary routine signature carrying the Intent `participant`
  carriage is refused by the signature schema.
- The string window negative `reject_cloned_record_source_unproven` is
  refused again on the self-host route.
- `effect_array_transform_valid` prints `values[0] == 1` after the sort, so
  a copy landed on the receiver instead of the result fails the gate.
