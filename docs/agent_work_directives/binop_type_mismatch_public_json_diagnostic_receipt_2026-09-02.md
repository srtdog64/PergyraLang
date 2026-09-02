# Binary-Operator Type-Mismatch Public JSON Diagnostic Receipt

Status: ACTIVE — LOCAL GREEN, AWAITING PUBLICATION

Exact base revision: `6b720aa7d372696764719ec2d25c6333f4debf92`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, fuzz priority, or completion
claim.

## Shared objective card

- Objective: publish the existing Pergyra-owned `binop_type_mismatch` verdict
  as one exact public JSON identity through installed MIR, C, and LLVM requests
  instead of emitting an empty private receipt and collapsing at the C
  malformed-receipt boundary.
- Priority order: preserve the exact Pergyra code and `left_type=Int` /
  `right_type=String` facts across let, assignment, condition, and return
  contexts; admit only this exact code; reuse the shared wire/process owners;
  retain fail-closed evidence for unadmitted and missing-oracle codes.
- Fact owners: the existing semantic binary-expression graph and verdict
  owners retain operand facts and the Pergyra code. The semantic public receipt
  owner will own cause `semantic:binop:operand_types` and fix
  `align-operand-types-or-overload` for exactly `binop_type_mismatch`.
  The shared public wire renderer remains the sole serialization owner.
- Production entrypoints: public `pgy --mir --error-format=json SOURCE`, public
  C compile/artifact JSON requests, and public LLVM compile/artifact JSON
  requests.
- Direct bypass to delete: all four installed Pergyra binary-operator verdicts
  emit an empty private JSON receipt, after which the public C boundary reports
  only `self-host JSON diagnostic receipt is malformed`.
- Last legitimate consumers: the installed Pergyra semantic verdict is
  consumed by the existing MIR/artifact diagnostic process owner and public
  stderr boundary.
- Forbidden fallback: C semantic mapping, native retry or preflight, message
  parsing, admission by shared native public code, grouping this code with
  `compare_type_mismatch`, co-admitting `undefined_symbol` or
  `option_concrete_type_required`, changing text codes or operand facts, a
  second semantic pass, invented source locations, partial wire output, or
  another serializer.
- Focused gate:
  `tests/self_hosted/parity/public_binop_type_mismatch_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: `bad_arith_operand.pgy`, `bad_binop_assign.pgy`,
  `bad_binop_condition.pgy`, and `bad_binop_return.pgy` must retain exact
  `Int`/`String` facts; installed MIR/C/LLVM requests must relay exact receipts
  on stderr without stdout, artifact, or native timing; wording drift must not
  change identity. `bad_import_enum_variant` must remain unadmitted,
  `bad_issome_none_call` must remain fail-closed without a native receipt, and
  an unknown Pergyra code must not become ready.

## Opening evidence

- All four fixtures exit nonzero with exact Pergyra code
  `binop_type_mismatch`; their private JSON output is empty and public MIR JSON
  stops at the generic malformed-receipt boundary.
- Explicit native JSON fixes code `PGY_SEM_BINOP_TYPE_MISMATCH`, stage
  `semantic`, layer `type`, cause `semantic:binop:operand_types`, and fix
  `align-operand-types-or-overload` across all four contexts.
- Existing `compare_type_mismatch` legitimately carries the same public
  identity under a distinct Pergyra code. That existing admission is not
  authority to group or co-admit this code.
- `option_concrete_type_required` has no explicit-native diagnostic receipt on
  its current fixtures. It remains a missing-oracle negative rather than a
  guessed admission. `bad_import_enum_variant` remains a distinct unadmitted
  exact-code negative.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. `diagnostic.catalog` remains `BRIDGE`; implementation volume is
  21.24% and project forecast remains 83%.

## Local integration evidence

- A fresh Pergyra-built DRV-2 was installed from the changed receipt owners
  with SHA-256
  `CB38B9AED81B841DB58A976239BC5AECA084680CA3B46BA0C066D9443FF15BA9`.
- The focused gate passed all four let/assignment/condition/return contexts
  through installed MIR, C, and LLVM requests and retained undefined-symbol,
  missing-oracle, and unknown-code exclusion.
- The nine rebaselined receipt gates passed with `bad_import_enum_variant` as
  their unrelated fail-closed code where needed. The full installed CLI
  aggregate passed through every existing receipt family, parser, tokens, AST,
  LLVM IR, native opt-in, REPL, formatter, device manifest, typed argv, and the
  general MIR-C transaction boundary.
- Diagnostic registry, SoT edge, Gate single-owner, protocol registry, build
  source inventory, hard substitution, velocity, and progress gates passed.
  The observed implementation volume is 21.25%; SoT counts, blocker count, and
  project forecast remain unchanged.
- The broad local component inventory is intentionally omitted because its
  observed runtime exceeds the repository's 60-second static budget. Exact CI
  owns that Linux evidence after publication. Local green evidence does not by
  itself close this active lease.
- Delegated fuzzing remained outside the implementation scope and produced the
  read-only audit
  `docs/audits/vessel_method_argument_type_admission_differential_fuzz_audit_2026-09-02.md`.
  Its minimized vessel-method argument mismatch is a separately measurable
  successor candidate, not an edit or priority change in this rung.

## Coordination bounds

- Independent edit scope: the exact semantic public-receipt and contract
  owners, one focused four-context parity gate, Make/installed-aggregate/
  component wiring, affected receipt-gate negative fixtures, the
  `diagnostic.catalog` evidence row, and current coordination snapshots.
- Fuzz scope is independent and read-only: the delegated agent may inspect
  `F:/tex_bug`, run bounded production differentials, and write one audit under
  `docs/audits/`. It may not edit compiler source, SoT owners, registries,
  active handoff/lease files, CI wiring, or this implementation rung.
- Forbidden overlap: no other task may edit or publish this executable rung.
  C diagnostic transports, semantic binary-operator policy, native parser fuzz
  repair, stable public wire schema, and protected untracked paths are
  read-only.
- Allowed budgets: focused source/static checks within 60 seconds, focused
  parity within 5 minutes after one required DRV-2 rebuild, and one installed
  CLI integration shard within 30 minutes. Full matrices run only after
  publication.
- Integration owner and gate: the primary task owns integration; the focused
  gate above is the falsifier and
  `tests/self_hosted/parity/installed_driver_cli_mode_owner.sh` is the single
  local integration boundary.
- Outputs are implementation candidates until the primary task observes the
  focused and integration gates. Fuzz observations do not reorder this active
  rung and neither output changes SoT or progress status by itself.
