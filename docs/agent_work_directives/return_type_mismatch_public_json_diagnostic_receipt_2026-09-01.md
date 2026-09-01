# Return-Type Mismatch Public JSON Diagnostic Receipt

Status: DONE — EXACT CI GREEN

Exact base revision: `02d93d4271f23f58ae3bfac32f37260e8a13a96a`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, or completion claim.

## Shared objective card

- Objective: make an installed Pergyra `return_type_mismatch` publish one exact
  public JSON receipt through MIR, C, and LLVM production requests instead of
  exiting with an empty private payload that the C transport can report only as
  malformed or missing.
- Priority order: preserve the existing Pergyra semantic code and text
  diagnostic; assign one stable public identity to exactly
  `return_type_mismatch`; reuse the existing shared wire and MIR/artifact
  process owners; keep all other semantic codes fail-closed.
- Fact owners: `src/self_hosted/semantic/diagnostic_code_owner.pgy` continues to
  own the `PGY_SEM_TYPE_MISMATCH` projection. The existing
  `src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy` owns the exact
  `semantic:assignability_check` cause and `annotate-or-convert` fix for the
  Pergyra-owned `return_type_mismatch` code. The shared public wire renderer
  remains the sole serialization owner.
- Production entrypoints: public `pgy --mir --error-format=json SOURCE`, public
  C compile/artifact JSON requests, and public LLVM compile/IR JSON requests.
- Direct bypass to delete: the empty private receipt for this owned code and the
  resulting generic C-side malformed/missing diagnostic. No native compiler
  execution is an acceptable replacement.
- Last legitimate consumers: the installed Pergyra semantic verdict projection,
  followed by the existing MIR/artifact diagnostic process owner and the public
  stderr boundary.
- Forbidden fallback: C semantic mapping, native retry or preflight, diagnostic
  message parsing, grouping every `PGY_SEM_TYPE_MISMATCH` source code under this
  identity, a second semantic pass, invented source location, changed text-mode
  wording, partial/private wire publication, or another serializer.
- Focused gate:
  `tests/self_hosted/parity/public_return_type_mismatch_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: the current return-type fixture must retain its Pergyra text
  payload; direct private JSON must carry one wire; public MIR/C/LLVM JSON must
  emit exact code/stage/layer/cause/fix on stderr with no stdout, artifact, or
  native timing; wording drift must not change identity; unrelated semantic
  mismatch codes must remain unadmitted until separately owned.

## Opening evidence

- `src/self_hosted/semantic/fixture/bad_return_type.pgy` reaches the installed
  semantic owner and emits code `return_type_mismatch` in text mode. The direct
  private JSON request exits non-zero with no semantic payload, while public
  MIR/C/LLVM JSON requests fail as malformed or missing receipts.
- Explicit native MIR/C/LLVM JSON independently agree on
  `PGY_SEM_TYPE_MISMATCH`, stage `semantic`, layer `type`, cause
  `semantic:assignability_check`, and fix `annotate-or-convert`. Native wording
  and location are oracle observations, not Pergyra identity owners.
- The invalid-character AST/MIR/C/LLVM probe is a real separate blocker, but it
  would require parser-wide lexical admission or a duplicate source scan. It is
  not folded into this semantic-code rung.
- SoT opens unchanged at `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. `diagnostic.catalog` remains `BRIDGE`; project forecast remains
  83%.

The primary task is the sole edit, integration, commit/push, and exact-CI
observation owner. Protected unrelated untracked paths remain outside
inspection, edit, and staging: `docs/compiler_architectures/`,
`pgy-80135c2c/`, and `pgy-91d769ec/`.

## Coordination bounds

- Independent edit scope: the semantic public-receipt and contract owners, one
  focused parity gate, its Make/installed-aggregate/component wiring, the
  `diagnostic.catalog` evidence row, and current coordination snapshots.
- Forbidden overlap: no other task may edit or publish this executable rung.
  C diagnostic transports, parser/lexer owners, the stable public wire schema,
  unrelated semantic codes, and protected untracked paths are read-only.
- Allowed commands and budgets: focused source/static checks within 60 seconds,
  focused parity within 5 minutes after one required DRV-2 rebuild, and one
  installed CLI integration shard within 30 minutes. Full CI matrices run only
  after publication.
- Integration owner and gate: the primary task owns integration; the focused
  gate above is the falsifier and
  `tests/self_hosted/parity/installed_driver_cli_mode_owner.sh` is the single
  local integration boundary.
- Every local and CI output in this directive is an observation. The source
  change remains an implementation candidate until exact-head CI is green; it
  is not a new semantic owner, completion proof, or successor-rung proposal.

## Local implementation evidence

- The semantic public-receipt owner admits only the existing Pergyra code
  `return_type_mismatch` with cause `semantic:assignability_check` and fix
  `annotate-or-convert`. Other semantic verdicts that share
  `PGY_SEM_TYPE_MISMATCH` are not grouped into that identity.
- A fresh Pergyra-built DRV-2 is installed. The focused gate passes direct text
  preservation, direct private wire production, exact public MIR/C/LLVM stderr
  relay, no artifact, no native timing, wording independence, and the
  `logical_operand_not_bool` unadmitted negative.
- Existing public MIR, artifact C/LLVM, callable-contract parser, and LLVM IR
  JSON receipt gates pass. The complete installed CLI aggregate also passes,
  including token, AST, REPL, formatter, native-opt-in, and DeviceSlot manifest
  boundaries.
- Diagnostic registry, SoT edge, Gate single-owner, protocol registry,
  substitution-velocity, build-source inventory, and hard self-host contract
  gates pass. The broad component inventory exceeded its 60-second local
  static-gate budget and was stopped without an observed verdict; exact CI must
  provide the integration result.
- The observed census remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9
  blockers. This executable delta does not by itself close
  `diagnostic.catalog` or change the 83% project forecast.

## Publication evidence

- Implementation `b51d3cfc11695895423fecb72fd16524925910f0` is published on
  `origin/main`. Exact run `33511109765` is green 30/30 with all 20 backend
  comparison shards green.
- `build-linux` took 26m01s and passed the structural component contract. Full
  self-host took 24m37s and records exactly one each of
  `gen2 == gen3 (173234 lines)`, receipt-bound fixed-point adoption,
  Pergyra-built DRV-2 installation, and the focused return-type marker.
- This directive is closed coordination history. It owns no successor rung and
  does not close the `diagnostic.catalog` registry row.
