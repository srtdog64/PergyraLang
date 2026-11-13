# Current blocker

Owner: Claude root lane. Checked 2026-11-14 on `ci/all-green` at `0d35d1238`
on the WSL tree; CI run 43477748900 is the last full run (at `d78654846`).

## Objective card

- Objective: required CI green on one published commit, with the bootstrap
  and installed-driver jobs executed.
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Contracts 3: the identity cell probe's native build takes 300+ s locally
   (about 200 s estimated on CI) against 120 s; 33 landing rounds
   ([record](../evidence/2026-11-13_identity_probe_landing_rounds.md)).
   Built by the installed self-host driver instead, it emits C in 159 s
   (measured while another build ran) plus 24 s of gcc, at 12.8 GB peak RSS.
2. Contracts 4 ownership semantic: the gate now stops at
   `unknown_string_array_alias_without_drop` on the native C path. Native
   marks the callee's literal result an owned-element candidate
   (`return_elements_owned`), so `let alias = returned;` needs an owned-result
   proof that `semantic_array_storage_owned_string_result_current` does not
   find. Later gate sections have not run on CI since 2026-11-12.
3. Native admits 28 negative fixtures that fail under ASan
   ([record](../evidence/2026-11-12_native_admits_unsafe_negative_fixtures.md)).
4. Locally, core step 5 fails only on WSL (`wslpath -w` gives a UNC repo
   root); a pass-through `wslpath` shim makes it pass.
5. The self-host MIR route carries no compiler `Clone`; only its semantic C
   route does.

## Done since `d78654846`

- `85c05361d`: a routine the scalar route cannot admit is a typed
  `CodegenRouteRefusal`, not a Die; core step 24 and the public LLVM IR
  receipt gate pass. Core shard 55/55 and the CLI smoke pass locally.
- `0d35d1238`: field restore, CFG-proved shallow mutation and borrowed or
  unknown binding moves admitted from their MIR facts, with two negatives.
- `09990f45e`, `bbbadc926`, `5bf70e773`, `47fd47cc5`: Slice<Int> writes,
  generic summaries, role programs, unused routines, literal-index writes.

## Falsifiers kept

- A drop on one branch still refuses the read after the join; own
  transfers of storage a constructor kept stay refused natively.
- A struct read after its String field moved into an `own` formal is
  refused by the self-host.
- A field restore whose MIR row names another field, and a binding move
  whose destination row does not name its source, are refused on C and LLVM.
