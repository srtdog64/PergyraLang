# Current blocker

Owner: Claude root lane. Checked 2026-11-11 21:40 KST on `ci/all-green`
(main `5c72599e2` plus `c7a67bd3f`).

## Objective card

- Objective: required CI green on one published commit, with the bootstrap
  and installed-driver jobs executed.
- Priority: semantic identity and one fact owner; fail-closed admission;
  C/LLVM parity; fixed point; installed execution; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Native analysis time (needs a user decision). The canonical driver's
   native C emission at `f1d4a0232` runs 67 analyses (semantic 1,844 s of
   1,963 s locally): 19 store waves, each resetting the copy plan, which
   then rebuilds over 6 to 10 analyses; the last 8 add one copy each.
   CI at `51fbdca00`: oracle emit 28 minutes; the fixed point job was
   cancelled at 60 minutes while compiling gen2. Copy retention saves 14%
   and changes the plan
   ([record](../evidence/2026-11-11_landing_copy_retention_rejected.md)).
2. Contracts shard (needs a user decision): 3 identity cell probe build
   (310 s locally) and 4, whose make chain builds 14 probes importing the
   semantic or direct-MIR admission owners, each over its 120 s limit.
3. Core shard: 5 role override. The general scalar route now claims the
   program first; it emits the unused override routine as a static
   function (`-Werror`) and accepts the MIR negative
   `direct_override_as_ability_impl`. 6: the self-host owned-String proof
   refuses `fact.value` into `own`. 24: the general emitter dies on
   `Clone(a)` of a subject (`stage=builtin-call`), so the JSON route has
   no receipt.
4. Backend compare 4 and 7: generic ownership precision. `Twice<T>`
   returning `[x, x]` is refused even for `T = Int` and with explicit
   `Clone(x)`; `Pick<T>` returning its fallback, then reusing it, needs a
   whole copy of `Result<Int, String>`. Generic bodies are checked once
   with `T` opaque.

## Fixed after `51fbdca00`

- 43 zone identity gate projects through the driver (`f1d4a0232`).
- 32 qubit stdlib calls emit by name again (`90dfbeac8`).
- 34 the parallel element binding has its header id (`b48136b5e`).
- Backend compare 14 and 16: a copy-only function-value call keeps the
  backing frame complete (`c7a67bd3f`, not yet on CI).

## Falsifiers kept

- Handing the Array itself to a function value is still refused.
- The zone/world value-carriage mutant is refused on both MIR backends
  with no artifact.
- A struct constructor that leaves a field out is refused by both front
  ends; `let` passed to inout stays refused for values.
