# Current blocker

Owner: Claude root lane. Checked 2026-11-14 on `ci/all-green` at `2b377d955`
on the WSL tree; CI run 43515230821 is the last full run (at `59e40856b`).

## Objective card

- Objective: required CI green on one published commit, with the bootstrap
  and installed-driver jobs executed.
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Not yet seen on CI: contracts 3 with the probe built on the public route
   (131 s locally against 120 s, CI about twice as fast), the fixed-point
   join, and the policy corpus moved into it (`2b377d955`).
2. Native admits 28 negative fixtures that fail under ASan
   ([record](../evidence/2026-11-12_native_admits_unsafe_negative_fixtures.md)).
3. Locally, core step 5 fails only on WSL (`wslpath -w` gives a UNC repo
   root); a pass-through `wslpath` shim makes it pass.
4. The self-host MIR route carries no compiler `Clone`; native lands one for
   `collection_field_use_after_move`, the self-host refuses it.

## Done since `d78654846`

- `85c05361d`: a routine the scalar route cannot admit is a typed
  `CodegenRouteRefusal`; core step 24 and the public LLVM IR receipt gate
  pass. CI 43515230821: build-linux green, bootstrap CLI smoke green.
- `0d35d1238`: field restore, CFG-proved shallow mutation and borrowed or
  unknown binding moves admitted from their MIR facts.
- `6283b8aa3`: a store through a subject receiver is an in-place `assign`
  in self-host MIR, as in native MIR; the identity probe is built on the
  public route. Identity gate: 208 checks, 0 failures.
- `c7c9c9153`: a native move of unowned String elements needs only its
  descriptor; of 170 move/alias fixtures only the target changes verdict.
- `d4f601885`: the ownership gate's later sections match the compilers;
  contracts step 4 passes locally.
- Locally at `2b377d955`: core shard 55/55, CLI smoke, contracts 10/11
  (step 3 timed out once at 120 s on this tree, passed once).

## Falsifiers kept

- A drop on one branch still refuses the read after the join; own
  transfers of storage a constructor kept stay refused natively.
- A struct read after its String field moved into an `own` formal is
  refused by the self-host.
- A field restore whose MIR row names another field, and a binding move
  whose destination row names itself, are refused with no artifact.
