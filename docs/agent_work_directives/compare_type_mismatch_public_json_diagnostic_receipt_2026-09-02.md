# Compare-Type-Mismatch Public JSON Diagnostic Receipt

Status: ACTIVE — LOCAL IMPLEMENTATION GREEN, EXACT CI PENDING

Exact base revision: `215bcf0274831a1613b6505281485d13182409b5`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, or completion claim.

## Shared objective card

- Objective: make the existing Pergyra-owned `compare_type_mismatch` verdict
  publish its exact shared binary-operand public JSON identity through all
  three admitted semantic fixture contexts and the installed MIR, C, and LLVM
  requests instead of producing an empty private receipt and a generic C
  transport failure.
- Priority order: preserve the Pergyra text code and left/right facts; carry
  the native-agreed binary-operand identity for this exact owned code; prove
  assign, condition, and operand contexts; reuse the shared wire and process
  owners; keep `binop_type_mismatch` and other unadmitted codes fail-closed.
- Fact owners: `src/self_hosted/semantic/diagnostic_code_owner.pgy` continues to
  own the `PGY_SEM_BINOP_TYPE_MISMATCH` projection. The semantic public-receipt
  owner will own cause `semantic:binop:operand_types` and fix
  `align-operand-types-or-overload` for exactly `compare_type_mismatch`. The
  shared public wire renderer remains the sole serialization owner.
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
  parsing, treating a shared public identity as permission to co-admit
  `binop_type_mismatch`, changing Pergyra text code or facts, a second semantic
  pass, invented source location, partial wire output, or another serializer.
- Focused gate:
  `tests/self_hosted/parity/public_compare_type_mismatch_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: `bad_compare_assign.pgy`,
  `bad_compare_condition.pgy`, and `bad_compare_operand.pgy` must retain their
  normalized text code and left/right facts and produce the same owned private
  identity; installed MIR/C/LLVM requests must relay exact Pergyra receipts on
  stderr without stdout, artifact, or native timing; message drift must not
  change identity; `bad_binop_assign.pgy` must remain unadmitted.

## Opening evidence

- All three compare fixtures emit `Code: compare_type_mismatch`, `left: Int`,
  and `right: String` from the installed Pergyra semantic owner. Their private
  JSON requests exit nonzero with only a two-byte blank line, and public MIR
  JSON stops at `pgy: self-host JSON diagnostic receipt is malformed`.
- Explicit native JSON agrees across all three contexts on code
  `PGY_SEM_BINOP_TYPE_MISMATCH`, stage `semantic`, layer `type`, cause
  `semantic:binop:operand_types`, and fix
  `align-operand-types-or-overload`.
- `logical_operand_not_bool` legitimately projects to the same native public
  identity but retains a different Pergyra code and operand-side facts.
  `binop_type_mismatch` is still unadmitted; neither fact authorizes grouping
  by message or silently widening this rung.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. `diagnostic.catalog` remains `BRIDGE`; project forecast remains
  83%.

## Coordination bounds

- Independent edit scope: the semantic public-receipt and contract owners, one
  focused parity gate, its Make/installed-aggregate/component wiring, the
  `diagnostic.catalog` evidence row, and current coordination snapshots.
- Forbidden overlap: no other task may edit or publish this executable rung.
  C diagnostic transports, parser/lexer owners, logical/binop semantic owners,
  stable public wire schema, fuzz findings, and protected untracked paths are
  read-only.
- Allowed budgets: focused source/static checks within 60 seconds, focused
  parity within 5 minutes after one required DRV-2 rebuild, and one installed
  CLI integration shard within 30 minutes. Full matrices run only after
  publication.
- Integration owner and gate: the primary task owns integration; the focused
  gate above is the falsifier and
  `tests/self_hosted/parity/installed_driver_cli_mode_owner.sh` is the single
  local integration boundary.

## Local implementation evidence

- The semantic public-receipt owner admits only `compare_type_mismatch` with
  the native-agreed shared binary-operand cause and fix. All three compare
  fixtures preserve their exact normalized text diagnostics and left/right
  facts and produce owned private receipts.
- Public MIR uses the operand fixture, public C uses assign, and public LLVM
  uses condition. Each relays the exact corresponding Pergyra receipt on stderr
  without stdout, artifact, wire marker, or native timing. The admitted logical
  receipt retains the same public identity while the arithmetic mismatch
  remains unadmitted.
- A fresh Pergyra-built DRV-2 passes the focused compare gate, the rebaselined
  logical and unary gates, and the complete installed CLI aggregate including
  artifact, parser, token, AST, LLVM IR, native opt-in, REPL, formatter, and
  DeviceSlot boundaries.
- Diagnostic registry, SoT edge, Gate single-owner, protocol registry,
  build-source inventory, substitution velocity, hard self-host contract, and
  progress metric gates pass. The broad component inventory remains for exact
  CI because its repository-defined local budget is 60 seconds.
- The observed census remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. Implementation volume is 21.19%; neither measure changes the 83%
  project forecast or closes `diagnostic.catalog`.
