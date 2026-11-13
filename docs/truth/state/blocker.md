# Current blocker

Owner: Claude root lane. Checked 2026-11-13 on `ci/all-green` after
`bbbadc926`, with the working changes on the WSL side tree.

## Objective card

- Objective: required CI green on one published commit, with the bootstrap
  and installed-driver jobs executed.
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Native admits 28 negative fixtures that fail under ASan
   ([record](../evidence/2026-11-12_native_admits_unsafe_negative_fixtures.md)).
   Eight share the lending-callee cause (the fix belongs in the formal's
   write summary). Native also admits a String read after it moved into an
   `own` formal, local or struct field, in the known ArrayDropOwnedStrings
   family.
2. Core shard step 24 (containment): a direct-MIR routine refusal still
   ends in Die, so JSON mode prints "receipt is malformed". The fold proves
   `routine_state = DirectMirScalarCfgProgramAppendRoutine(...)` a field-pack
   move only when every reachable return carries each field; today the
   failure returns follow a Die and are unreachable. A non-dying refusal
   makes them reachable and the driver source is refused.
3. Bootstrap `self-host-array-index-type`: C passes; direct-MIR LLVM has no
   Slice element write. A `LocalSliceIntSet` operation reaches plan
   verification and fails an unnamed readiness check (`program_readiness=0`).
4. Locally, core step 5 fails only on WSL: `wslpath -w` turns the repo root
   into a `wsl.localhost` UNC path, so the struct argument gate's path check
   fails; with a pass-through `wslpath` the step passes.
5. Contracts 3: the identity cell probe build takes 300+ s against 120 s;
   33 landing rounds ([record](../evidence/2026-11-13_identity_probe_landing_rounds.md)).
6. Contracts 4 ownership semantic: the LLVM route refuses
   `collection_field_restore_valid` at `collection_ownership_transition`.
7. The self-host MIR route carries no compiler `Clone`; only its semantic C
   route does.

## Done since `6452bad92`

- `bbbadc926`: a generic routine's Some/UnwrapOption has a result relation,
  so its summary matches the single-type routine; backend compare 4 and 7
  pass locally (988/988).
- Local, not yet committed: unused attribute on direct-MIR C routines; Slice
  index type and element write on the semantic C route; read-only ref of a
  scalar or Slice passed by value on the direct-MIR route; a struct field
  String moved into an `own` formal admitted when the field read is the
  struct's only use (two negatives added to the inout gate); native reports
  a read-only ref passed inout once, as a borrow escape.

## Falsifiers kept

- A drop on one branch still refuses the read after the join; own
  transfers of storage a constructor kept stay refused natively.
- A struct read after its String field moved into an `own` formal is
  refused by the self-host.
