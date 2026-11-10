# V.001 enum Result independent validation

Status: INDEPENDENT VALIDATION DELIVERED; production repair/integration is
still owned by main. Observations and test candidates are in
[the audit](../audits/v001_result_chain_and_remaining_gate_failures_2026-11-07.md).
Base: main `60fd1e26d8ea3987efbcbfc65f59b892b234701a`; integration candidate
staged tree `c397e5309af6e66e312b8ad5a2ff39b788cf1e0d`. Preserve the shared
dirty tree. Main chat owns compiler edits, installation, Git and published CI.

## Objective and closure boundary

- Objective: find the complete reached failure set behind the installed
  `Result<Int, E>` refusal, where E is a referenced payload-free enum, and
  validate the general producer-to-emitter repair on both C and LLVM.
- Priority: enum and callable identity; fail-closed ownership and ABI facts;
  equal output; preserved refusals; bounded verification cost.
- Fact owners: admitted MIR declaration/variant index, routine signatures,
  wrapper type owner, typed enum Result projection, expression graph.
- Last consumers: general C/LLVM signatures, constructors, calls, locals,
  builtins, match bindings and artifact publication.
- Forbidden fallback: per-program route, error enum cast to a string pointer,
  weakened storage safety, guessed identity, a second declaration schema
  authority, replacing an unreadable-input failure with semantic refusal.
- Shared integration gate: the existing full public ArrayDrop gate, plus
  this lane's two-producer enum Result and forged-fact controls. Main then
  owns full collection, canonical input, fixed point and exact-SHA CI.

## Reached chain and investigation

1. Native source-to-MIR and installed source-to-MIR issue declaration rows.
   Compare real issued rows before selecting a normalized contract: the
   frozen native fixture has eight enum fields; main observes five in the
   installed producer. The declaration index owns structural admission.
2. Routine return/parameter and instruction ABI names select referenced
   enums, including Result payload/error arguments. Variant ordinals and
   payload types come only from the existing variant index.
3. The typed Result projection derives the ABI and exact runtime symbols.
   A forged nonempty symbol is a falsifier of projection readiness.
4. Callable admission, contextual Ok/Err typing, local/parameter calls,
   builtins and match binds carry the same type to both general emitters.
5. The nominal two-routine route must decline enum declarations. No added
   nominal route may substitute for the general scalar owner.

## Scope and execution order

- Main edits the reached compiler owners and owner-cap rows. This lane does
  not edit those files while main is implementing.
- This lane owns this directive, a unique `.tmp/v001-enum-result-audit-20261107`
  directory, and new independent test fixtures/harness if proved useful.
- Pin candidate11 launcher/driver and source/input hashes. Enumerate the
  remaining existing public ArrayDrop positives without first-failure exit.
- Add bounded semantic probes for both enum ordinals, Ok/Err, parameters,
  locals, control flow, exact builtin symbols, imported identity and the
  existing string-error control. Deduplicate identical semantic inputs.
- Check missing/mismatched declaration and projection facts with positive
  and restoration controls; output sentinels must remain unchanged.
- Send reproduced owner-level causes to main; rerun on its identified fixed
  candidate. Promote only behavioral tests, never text-presence scans.
- Allowed commands: read-only Git/source inspection, scratch writes,
  compiler/test execution, exact artifact hashing, authorized chat handoff.
  Static gates 60s, focused checks 300s, integration shard 1800s. Timeouts
  terminate and drain child processes and remain UNKNOWN, never PASS.

The two actual declaration encodings now pass a fixed producer-to-projection
probe; missing/mismatched facts remain refused. The bounded source census,
original MIR gate, projection-symbol controls and retaining-call assertion
proposal are recorded in the audit. The census also reached the existing
late-forward/resource-return backing-summary obligation and the native phi /
installed C assignment consumers; they belong in main's same closure plan.

Unknowns: the next complete fixed install and its original integration gates.
Per main's coordination, this lane leaves shared code and gates unchanged and
delivers reviewed diffs and executable evidence for one integration owner.
