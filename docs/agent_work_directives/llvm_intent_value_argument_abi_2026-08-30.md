# LLVM intent value-argument ABI ownership

Status: `RETIRED — NAMED BLOCKER REMOVED; LIST_OPS PRODUCTION GATE GREEN`

Exact base: `fa09d02c` on local `main`, descended from published
`e19b15b2647664e4208f7d97e490318085f652d6`.

## Objective card

- Objective: let the production LLVM compiler driver pass a data-bearing enum
  intent `value` argument with the same by-value ABI declared by the intent
  function, so LLVM verification advances beyond `CompilePergyraCArtifact`.
- Priority order: preserve ordered MIR binding-kind identity; align call-site
  and declaration ABI; retain addressable participant behavior; fail closed on
  missing/invalid metadata; then minimize implementation and harness cost.
- Fact owner: each ordered `IntentBindingMetadataView` row owns both binding
  kind (`participant` or `value`) and declared type. Nominal pointer-self policy
  is applicable only after that owner identifies a participant binding.
- Last legitimate consumer: the LLVM intent call-argument loop in
  `llvm_emit_call_expr`, immediately before the registered intent function is
  called. The forward declaration already consumes the same row kind to choose
  participant pointer versus value aggregate ABI.
- Forbidden fallback: a `DriverSourceCRequest` or enum-name allowlist; loading
  every pointer-shaped argument after the fact; changing the Pergyra source;
  passing all nominal values indirectly; deriving value/participant identity
  from the AST when MIR metadata exists; accepting incomplete metadata; or
  using the C backend when LLVM verification fails.
- Verification gate: a focused intent with one addressable participant and one
  data-bearing enum value must compile and run with exact C/LLVM parity; a
  structural ratchet must keep pointer-self lookup behind the participant-kind
  branch; the filtered real LLVM driver-body gate must advance beyond the
  current aggregate-versus-pointer verifier failure.

## Reproduced executable gap

- Production entrypoint: native LLVM compilation of the import-composed
  `driver_rung2_main.pgy` compiler driver, filtered to the `list_ops` body
  fixture.
- Exact verifier mismatch: `CompilePergyraCArtifact` declares its final
  `%DriverSourceCRequest` parameter by value, but the call emitter supplies the
  local `%request` alloca as `ptr`.
- `DriverSourceCRequest` is a data-bearing enum and the MIR binding row is
  `value`. The call loop currently runs nominal pointer-self classification
  unconditionally after reading either binding kind, while its forward
  declaration path applies pointer-self classification only to participants.
- This is a reached bootstrap C/LLVM parity blocker. It does not delete a C
  compiler implementation and therefore does not change the hard
  `SUBSTITUTING` numerator, SoT census, project percentage, or decompilation
  release target.

## Edit scope and integration

- Allowed implementation: the existing intent call-argument loop and the
  minimum local binding-kind state required to gate participant addressing.
- Allowed tests: one data-bearing enum intent fixture, one focused C/LLVM
  execution/structural gate, and minimum Make/CI inventory wiring.
- Allowed coordination/evidence: this directive plus the top collaboration and
  handoff cards, followed by a bounded result update.
- Forbidden overlap: intent metadata production or schema redesign, general
  call ABI changes, enum layout redesign, Pergyra compiler-source rewrites,
  unrelated LLVM gaps, and unrelated SoT rows.
- Integration owner: the primary task at this exact base.

This directive is temporary coordination state. It does not own language
semantics, registry status, progress percentage, or completion evidence.

## Bounded result

- The intent call emitter now preserves ordered binding kind locally and runs
  nominal pointer-self classification only for participant rows. Value rows
  continue through ordinary expression emission and keep the declaration's
  by-value ABI.
- `make self-host-llvm-intent-value-argument-abi-test-smoke` passed exact
  C/LLVM `true` / `1` execution for an addressable participant plus a
  data-bearing enum value. Its structural ratchet keeps both MIR and non-MIR
  participant identification ahead of the pointer policy.
- The filtered real production gate passed:
  `backends=1 body_fixtures=0 mir_fixtures=1` with producer-first source/MIR
  parity for `list_ops`. The previous `%DriverSourceCRequest` aggregate-versus-
  pointer verifier mismatch is gone and the selected fixture executes.
- The official language-word inventory was regenerated after the new `.pgy`
  fixture. This result closes only the named ABI blocker; it neither proves the
  full 284-row matrix nor changes hard substitution, census, project
  percentage, or decompilation-hardness status.
- `self-host-component-contract-test-smoke` was stopped at the focused
  five-minute budget. Its nested source-MIR execution-action ratchet passed,
  but the parent gate did not finish and is not claimed green; merge CI remains
  responsible for that broader structural observation.
