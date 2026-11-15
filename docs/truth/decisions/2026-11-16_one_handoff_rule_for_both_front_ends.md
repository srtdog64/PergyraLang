# 2026-11-16: one hand-off rule for both front ends

The user decided these points from the Claude root lane's design study.
The study (research proposal A) addresses the problem that the native and
self-host front ends each compute their own call hand-off rule.
- On the lifetime fixtures that both front ends admit, the hand-off
  verdicts agree on 119 programs, 38 are native-only and 29 are
  self-host-only (see `f1456722d`).
- Because of that disagreement, direct MIR keeps a backend-made sharing
  refusal, and `semantic.hashmap_collection_ownership` cannot close.

## The rule

- For each default-mode aggregate formal, compute the set of its callee
  effects: read, return, grow, keep, release, unproved. The set also
  counts the formal's local aliases (no landed `Clone`, decision
  2026-11-12) and records built from it.
- A call moves the argument when the set contains grow, keep, release or
  unproved. A moved argument with no later use, at a site that does not
  repeat, is handed off (decision 2026-11-15). Otherwise it takes a whole
  `Clone` (decision 2026-10-22) or is refused.
- **Unproved effects.** In a synchronous call they count as a move. In a
  detached context (`async`, a worker boundary) the call is refused. The
  user rejected refusing every unproved effect: too few programs could be
  written.

## One source

- The rule table lives in one `.def` file. It is rendered to a C header and
  to a `.pgy` owner, and a `render_* --check` drift gate keeps them equal,
  as `builtin_argument_retention_registry.def` does.
- The self-host cannot own the rule alone, because native compiles the
  self-host first.
- Both front ends publish a per-formal effect-class row in MIR. Only
  validation reads it. Once the two front ends agree, the direct MIR
  by-value sharing branch and the String-element preservation mode are
  deleted.

## Gates

- A differential gate over the 868 lifetime fixtures: native and
  self-host hand-off verdicts must agree (currently 38 and 29 programs
  apart).
- A forged or missing hand-off or class row is refused.
- Every table cell has a positive and a negative fixture.
- LLVM legs build at `-O0`.

## Still open

The getter loan on a local used again is open:
[OPEN file](2026-11-16_open_getter_loan_on_reused_local.md).
