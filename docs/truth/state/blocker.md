# Current blocker

Owner: Claude root lane. Checked 2026-11-12 on `ci/all-green` at `912788ef9`.

## Objective card

- Objective: required CI green on one published commit, with the bootstrap
  and installed-driver jobs executed.
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. The native compiler admits 40 negative fixtures that fail under ASan
   (33 use-after-free, 6 double free, 1 unbounded recursion), among them
   12 `MapGet` views read after `MapSet`/`MapRemove`. Seen since at least
   `3dac8b4ac`. They cannot serve as the V1 "native admits" oracle
   ([record](../evidence/2026-11-12_native_admits_unsafe_negative_fixtures.md)).
2. collection_inout_effect, whose verdict loop was never reached before the
   merged probe: 59 C-backend mismatches (37 negatives the self-host now
   admits, 15 positives it refuses, 7 other codes). The self-host driver's C
   for `owned_element_retaining_text_negative` reads freed memory.
3. The self-host owned-element requirement covers inout formals only:
   `ArrayPop` on an `own` formal is an artifact error, and an `own` formal
   handed to a dropping `own` callee is refused. Native admits both. This
   fails collection_formal_key input 0 and several inout-effect inputs.
4. Self-host focus recovery refuses a `var` field focus with a `defer`
   (`focus_recovery_defer_return_capture`) or inside a loop
   (`exclusive_map_loop_branch_restore_positive`); also at `1acc31c5b`.
5. Linux CI runners cannot resolve the Ubuntu mirrors since 2026-11-11
   14:47 UTC: codegen bootstrap fails at apt, every Linux job is skipped.
6. Contracts 3: identity cell probe build (310 s) over its 120 s limit.
7. Core 5, 6, 24 and backend compare 4, 7, as recorded at `51fbdca00`.

## Self-host generality gaps found (native admits, self-host refuses)

- Owned `Array<String>` reassigned under a branch or loop (semantic), or
  reassigned then dropped in straight-line code (CODEGEN ERROR route).
- Constructor and capture stores that native copies (field_ctor_*,
  member_capture_constructor*, struct literal, nominal Clone).

## Fixed since `51fbdca00` (local unless CI is named)

- Fixed point split into oracle/seed/join jobs (`61ebf046b`); semantic
  owner probes built once per job, 497 s locally (`891a1f23a`).
- Native: array reassigned under a branch or loop admitted on C and LLVM
  (`3cd211b94`); aggregate storage smoke gains two positives the previous
  compiler refused and one negative.
- Self-host: a callable local named like a collection mutator owns its call
  (`60f718914`).
- Gates now pass locally: aggregate release source, binding move prefix,
  native storage escape (copy landings run on C/LLVM).

## Falsifiers kept

- A drop on one branch still refuses the read after the join.
- Own transfers of storage a constructor kept stay refused natively.
- The zone/world value-carriage mutant is refused on both MIR backends.
