# Current blocker

Owner: Claude root lane. Checked 2026-11-14 on `ci/all-green` at `08d7301f9`
on the WSL tree; CI run 43541429551 is the last full run (at `ac0205b71`).

## Objective card

- Objective: required CI green on one published commit, with the bootstrap
  and installed-driver jobs executed.
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Not yet seen on CI: the contracts job with the native landing change
   (`87d3223b6`) and the identity probe in the shared probe build
   (`08d7301f9`). Locally the shard passes in 1014 s; at `ac0205b71` CI
   cancelled it at its 30 minute limit.
2. Native admits 28 negative fixtures that fail under ASan
   ([record](../evidence/2026-11-12_native_admits_unsafe_negative_fixtures.md)).
3. Locally, core step 5 fails only on WSL (`wslpath -w` gives a UNC repo
   root); a pass-through `wslpath` shim makes it pass.
4. The self-host MIR route carries no compiler `Clone`; native lands one for
   `collection_field_use_after_move`, the self-host refuses it.

## Done since `d78654846`

- `85c05361d`, `0d35d1238`: typed routine refusals; field restore, CFG
  shallow mutation and borrowed or unknown binding moves on the direct-MIR
  route. CI at `59e40856b`: build-linux green.
- `6283b8aa3`, `c7c9c9153`, `d4f601885`, `2b377d955`: subject receiver
  stores as in-place `assign`; native moves of unowned elements; ownership
  gate sections; policy corpus in the join job. CI at `ac0205b71`: every
  job green except contracts.
- `87d3223b6`: the native landing fixed point keeps its copy plan across
  store growth; the semantic owner probe takes 12 analyses instead of 51
  ([record](../evidence/2026-11-14_landing_copy_plan_retention.md)).
- `08d7301f9`: the identity probe is one program of the shared build.
- Locally at `08d7301f9`: native semantic tests 3394/3394, core shard 55/55,
  CLI smoke, contracts shard 11/11.

## Falsifiers kept

- A drop on one branch still refuses the read after the join; own
  transfers of storage a constructor kept stay refused natively.
- A struct read after its String field moved into an `own` formal is
  refused by the self-host.
- A field restore whose MIR row names another field, and a binding move
  whose destination row names itself, are refused with no artifact.
