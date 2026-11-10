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
   - Keeping copies across store growth, then deriving them once from none:
     14 analyses instead of 25 on the arity probe, same plan digests in
     `PGY_VERIFY_LANDING_FIXED_POINT` mode. Uncommitted in
     `semantic_landing_fixed_point_owner.c`; the full-compiler run is the
     open measurement (rounds carry about 3,950 copies there).
2. Contracts shard (local, tip driver): 3 identity probe timeout; 4 arity
   probe timeout; 7 slice_copy (payload enum and Slice expression kinds
   shared ids 139 to 142, fixed in `b8dadb0ca`, not yet rebuilt);
   10 `effect_array_transform_valid` native refusal; 11 next sub-gate after
   the enum fixtures.
3. Core shard (local, tip driver): 5 role override (general C emitter
   claims it; test pins the per-shape names and `-Werror` trips on the
   unused override); 6 copy-only control `fact.value` into `own`; 24
   containment LLVM `Die` in expression admission (no JSON receipt); 32
   qubit builtins have no C emission (also in the 11-07 tree); 39 string
   window negative accepted (11-07 driver refused it; owners differ only by
   `let` to `var`).
4. Fixed point after gen2 == gen3: legacy intent LLVM probe, not rerun on
   the binder probe fix.

## Falsifiers kept

- A struct constructor that leaves a field out is refused by both front
  ends; domain nominals may still omit trailing fields.
- `let` passed to inout stays refused for values; identity hosts follow
  native's field and action authority.
- Two AST nodes with one stable id are refused by the publication store.
