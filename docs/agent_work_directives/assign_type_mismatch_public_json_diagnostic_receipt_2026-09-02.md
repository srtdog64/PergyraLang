# Assign-Type-Mismatch Public JSON Diagnostic Receipt

Status: DONE — IMPLEMENTATION AND EXACT CI GREEN

Exact base revision: `c1ebd599519b43c9298b5ed7d1d7479db9f67e76`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, or completion claim.

## Shared objective card

- Objective: make the existing Pergyra-owned `assign_type_mismatch` verdict
  publish its exact assignability public JSON identity through installed MIR,
  C, and LLVM requests instead of producing an empty private receipt and a
  generic C transport failure.
- Priority order: preserve the Pergyra text code and expected/actual facts;
  carry the native-agreed identity for this exact code; reuse the shared wire
  and process owners; keep other unadmitted mismatch codes fail-closed.
- Fact owners: `src/self_hosted/semantic/diagnostic_code_owner.pgy` continues to
  own the `PGY_SEM_TYPE_MISMATCH` projection. The semantic public-receipt owner
  will own cause `semantic:assignability_check` and fix `annotate-or-convert`
  for exactly `assign_type_mismatch`. The shared public wire renderer remains
  the sole serialization owner.
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
  parsing, grouping by the shared `PGY_SEM_TYPE_MISMATCH` identity, co-admitting
  `call_arg_type_mismatch`, changing Pergyra text code or facts, a second
  semantic pass, invented source location, partial wire output, or another
  serializer.
- Focused gate:
  `tests/self_hosted/parity/public_assign_type_mismatch_json_diagnostic_receipt_owner.sh`.
- Falsifying case: `bad_assign_type.pgy` must retain its normalized text code
  and expected/actual facts and produce the owned private identity; installed
  MIR/C/LLVM requests must relay its exact Pergyra receipt on stderr without
  stdout, artifact, or native timing; wording drift must not change identity;
  the already admitted let verdict must remain distinct by Pergyra code and
  `bad_user_arg.pgy` must remain unadmitted.

## Opening evidence

- The installed Pergyra semantic owner emits `assign_type_mismatch`,
  `expected: Int`, and `actual: String`. Its private JSON request exits
  nonzero with only two whitespace bytes, and public MIR JSON stops at
  `pgy: self-host JSON diagnostic receipt is malformed`.
- Explicit native JSON fixes code `PGY_SEM_TYPE_MISMATCH`, stage `semantic`,
  layer `type`, cause `semantic:assignability_check`, and fix
  `annotate-or-convert`.
- The admitted return and let verdicts legitimately carry the same public
  identity but retain different Pergyra codes. That does not authorize public-
  code grouping or widening this rung to `call_arg_type_mismatch`.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. `diagnostic.catalog` remains `BRIDGE`; project forecast remains
  83%.

## Coordination bounds

- Independent edit scope: the semantic public-receipt and contract owners, one
  focused parity gate, its Make/installed-aggregate/component wiring, affected
  admitted-family negative fixtures, the `diagnostic.catalog` evidence row,
  and current coordination snapshots.
- Forbidden overlap: no other task may edit or publish this executable rung.
  C diagnostic transports, parser/lexer owners, semantic verdict generation,
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

- The semantic public-receipt owner admits only `assign_type_mismatch` with the
  native-agreed assignability cause and fix. The fixture preserves its exact
  normalized text diagnostic and expected/actual facts and now produces an
  owned private receipt.
- Public MIR, C, and LLVM requests each relay that exact Pergyra receipt on
  stderr without stdout, artifact, wire marker, or native timing. The admitted
  let receipt retains the shared public identity while
  `call_arg_type_mismatch` remains unadmitted.
- A fresh Pergyra-built DRV-2 passes the focused assignment gate, the
  rebaselined return/logical/let negatives, and the complete installed CLI
  aggregate including artifact, parser, token, AST, LLVM IR, native opt-in,
  REPL, formatter, and DeviceSlot boundaries.
- Diagnostic registry, SoT edge, Gate single-owner, protocol registry,
  build-source inventory, substitution velocity, hard self-host contract, and
  progress metric gates pass. The broad component inventory remains for exact
  CI because its repository-defined local budget is 60 seconds.
- The observed census remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. Implementation volume is 21.20%; neither measure changes the 83%
  project forecast or closes `diagnostic.catalog`.

## Publication evidence

- Implementation `c6e940de9b430de907accb507324d894e3fb97f7` is on
  `origin/main`. Exact-head CI run `33551646367`, attempt 2, is green 30/30
  with backend comparison shards 20/20.
- Attempt 1 had one provisioning-only failure: backend shard 7 received HTTP
  403 from two preinstalled Microsoft apt repositories before downloading the
  compiler pair. The failed job alone was rerun; dependency installation,
  compiler-pair admission, and its 46/46 backend comparison then passed.
- `build-linux` passed in 25m34s and actually invoked
  `tests/self_hosted_component_contract_smoke.sh`; the structural source
  inventory and removed-path ratchets reported PASS.
- `self-host-bootstrap-linux` passed in 35m21s and records exactly one
  173264-line `gen2 == gen3` fixed point, exactly one receipt-bound driver
  adoption, exactly one Pergyra-built DRV-2 installation, and exactly one
  focused assignment-receipt PASS marker.
- This directive is complete and authorizes no successor rung. A new edit lease
  requires a fresh production executable falsifier. The malformed-enum fuzz
  finding remains separate waiting work.
