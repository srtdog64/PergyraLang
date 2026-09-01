# Not-Operand Public JSON Diagnostic Receipt

Status: ACTIVE — LOCAL IMPLEMENTATION GREEN, EXACT CI PENDING

Exact base revision: `b916fbf6fb247b99853f9dd45f46da6091b9365d`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, or completion claim.

## Shared objective card

- Objective: make the existing Pergyra-owned `not_operand_not_bool` verdict
  publish one exact public JSON identity through the installed MIR, C, and LLVM
  requests instead of producing an empty private receipt and a generic C
  transport failure.
- Priority order: preserve the Pergyra text code and actual-type fact; carry the
  one native-agreed unary-operand identity for this exact owned code; reuse the
  shared wire and process owners; keep condition, logical, return, and other
  type-mismatch identities distinct and fail-closed.
- Fact owners: `src/self_hosted/semantic/diagnostic_code_owner.pgy` continues to
  own the `PGY_SEM_UNOP_TYPE_MISMATCH` projection. The semantic public-receipt
  owner will own cause `semantic:unary_operator:operand` and fix
  `align-operand-type` for exactly `not_operand_not_bool`. The shared public
  wire renderer remains the sole serialization owner.
- Production entrypoints: public `pgy --mir --error-format=json SOURCE`, public
  C compile/artifact JSON requests, and public LLVM compile/artifact JSON
  requests.
- Direct bypass to delete: the empty private JSON receipt for this owned code
  and the resulting generic C-side malformed/missing diagnostic. Native
  compilation is not an acceptable replacement.
- Last legitimate consumers: the installed Pergyra semantic verdict projection,
  followed by the existing MIR/artifact diagnostic process owner and public
  stderr boundary.
- Forbidden fallback: C semantic mapping, native retry or preflight, message
  parsing, grouping logical/condition/return or another mismatch code under
  this identity, a second semantic pass, invented source location, changed
  text wording or facts, partial wire output, or another serializer.
- Focused gate:
  `tests/self_hosted/parity/public_not_operand_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: `bad_not_operand.pgy` must retain its normalized text
  diagnostic and actual-type fact and produce the owned private identity;
  installed MIR/C/LLVM requests must relay that exact Pergyra receipt on stderr
  without stdout, artifact, or native timing; message drift must not change
  identity; condition, logical, and return receipts must remain distinct.

## Opening evidence

- `bad_not_operand.pgy` emits `Code: not_operand_not_bool` and `actual: Int`
  from the installed Pergyra semantic owner. Its private JSON request exits
  nonzero with only a two-byte blank line, and public MIR JSON stops at
  `pgy: self-host JSON diagnostic receipt is malformed`.
- Explicit native JSON fixes code `PGY_SEM_UNOP_TYPE_MISMATCH`, stage
  `semantic`, layer `type`, cause `semantic:unary_operator:operand`, and fix
  `align-operand-type`. Native wording is not identity and the Pergyra verdict
  owns no source span, so this rung must not invent either.
- Condition, logical-operand, and return-type receipts already own different
  causes/fixes and are hard distinctions, not unary family members.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. `diagnostic.catalog` remains `BRIDGE`; project forecast remains
  83%.

## Coordination bounds

- Independent edit scope: the semantic public-receipt and contract owners, one
  focused parity gate, its Make/installed-aggregate/component wiring, the
  `diagnostic.catalog` evidence row, and current coordination snapshots.
- Forbidden overlap: no other task may edit or publish this executable rung.
  C diagnostic transports, parser/lexer owners, condition/logical/return
  semantic identities, stable public wire schema, fuzz findings, and protected
  untracked paths are read-only.
- Allowed budgets: focused source/static checks within 60 seconds, focused
  parity within 5 minutes after one required DRV-2 rebuild, and one installed
  CLI integration shard within 30 minutes. Full matrices run only after
  publication.
- Integration owner and gate: the primary task owns integration; the focused
  gate above is the falsifier and
  `tests/self_hosted/parity/installed_driver_cli_mode_owner.sh` is the single
  local integration boundary.

## Local implementation evidence

- The semantic public-receipt owner admits only `not_operand_not_bool` with the
  native-agreed unary-operand cause and fix. The committed fixture preserves
  its exact normalized text diagnostic and actual-type fact and produces an
  owned private receipt.
- Public MIR/C/LLVM requests relay the exact Pergyra receipt on stderr without
  stdout, artifact, wire marker, or native timing. Condition, logical, and
  return receipts retain their own causes, while the comparison mismatch
  remains unadmitted.
- A fresh Pergyra-built DRV-2 passes the focused gate, the rebaselined condition
  and logical gates, and the complete installed CLI aggregate including
  artifact, parser, token, AST, LLVM IR, native opt-in, REPL, formatter, and
  DeviceSlot boundaries.
- Diagnostic registry, SoT edge, Gate single-owner, protocol registry,
  build-source inventory, substitution velocity, hard self-host contract, and
  progress metric gates pass. The broad component inventory remains for exact
  CI because its repository-defined local budget is 60 seconds.
- The observed census remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. Implementation volume is 21.18%; neither measure changes the 83%
  project forecast or closes `diagnostic.catalog`.
