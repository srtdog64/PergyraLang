# Parser Callable-Contract Public JSON Receipt

Status: PUBLICATION COMPLETE

Exact base revision: `50df573a7effa8b7103a921b0fd52daa6c247edf`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, or completion claim.

## Shared objective card

- Objective: make the already-reached Pergyra parser rejection for duplicate
  callable-contract names publish one typed public JSON diagnostic through the
  default installed compiler, instead of collapsing at the C transport into
  `self-host JSON diagnostic receipt is malformed`.
- Priority order: preserve parser-owned rejection identity; carry the admitted
  JSON request explicitly; keep one public wire renderer; keep C transport
  opaque and fail closed; minimize the reached edit surface.
- Fact owner: `ParseDiagnosticRowForCode` plus the parser caller's owned
  `code`, `axis`, and `name`. The public catalog identity remains a projection
  of the repository diagnostic catalog owned by `src/semantic/diag_codes.h`.
- Production entrypoints: default installed
  `pgy --mir --error-format=json SOURCE` and JSON-selected C/LLVM artifact
  compilation for
  `tests/cases/callable_contract_vocabulary/duplicate_cap/main.pgy`.
- Last legitimate consumers: the parser diagnostic publication boundary, then
  `driver_self_host_public_diagnostic_wire_relay` as an opaque C envelope
  relay.
- Reached C-owned path being displaced: the concrete duplicate-capability
  failure currently reaches the generic `self-host JSON diagnostic receipt is
  malformed` branch. The generic malformed-wire guard remains required for
  genuinely missing, malformed, and crosswired child output.
- Forbidden fallback: C message parsing, `driver_diag_code_from_message`, a
  native retry or diagnostic preflight, a second parse, root/source rescanning,
  emitting both text and JSON envelopes, inferring mode from `Args()` or the
  environment, process-global mutable diagnostic mode, or changing explicit
  native-pipeline behavior.
- Focused gate:
  `tests/self_hosted/parity/public_parser_callable_contract_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: stable public code/stage/layer/cause/fix must survive parser
  wording changes; text mode must retain the existing parser envelope; valid
  requests must retain byte behavior; missing/malformed/crosswired children
  must remain fail closed; no native timing or partial artifact may appear.

## Edit scopes and overlap

- Parser scope: explicit diagnostic projection carriage from compiler parse
  entry to callable-contract reporting; no unrelated bare parser exits are in
  scope.
- Public wire scope: one shared Pergyra receipt value/renderer used by semantic
  and parser projectors; domain-to-public identity mapping stays with each
  domain projector.
- Compiler scope: pass the existing admitted JSON request into the parser; do
  not add CLI syntax or a second execution lane.
- Gate scope: one focused executable gate plus the minimum existing aggregate
  wiring needed for exact CI observation.
- Documentation scope: collaboration ledger and handoff only after observed
  evidence. Registry status and the 83% forecast do not change from this patch
  alone.
- Protected unrelated untracked paths are outside inspection, edit, and
  staging: `docs/compiler_architectures/`, `pgy-80135c2c/`, and
  `pgy-91d769ec/`.

The primary task is the sole integration, commit, push, and exact-CI
observation owner. Outputs before the focused gate passes are implementation
candidates, not completion evidence.

## Validation budget

- Static owner/component gates: at most 60 seconds each.
- Focused parser public-receipt parity: at most five minutes after a fresh
  receipt-bound DRV-2 is available.
- Existing public MIR/artifact receipt gates and installed CLI aggregate:
  integration boundary only.
- Full matrix: exact CI after the focused and repository ratchets are green.

## Local evidence

- A fresh Pergyra-built DRV-2 was emitted from the current source graph,
  compiled by GCC, passed its bounded source smoke and machine-manifest
  round-trip, and was installed under
  `.tmp/self_hosted/parser-public-json/` with an artifact receipt.
- The focused gate passes on that driver. Direct JSON publishes the wire
  envelope; default public MIR, C, and LLVM relay byte-equal JSON on stderr;
  direct text retains `pgy.selfhost.parse.v1`; all four failing modes return
  non-zero and publish no artifact.
- Existing public MIR JSON semantic-receipt and public C/LLVM artifact-receipt
  gates pass. The full installed-driver CLI aggregate passes from an isolated
  sibling installation and executes the new parser gate exactly once.
- Callable-contract vocabulary, compiler-world, component, hard-substitution,
  SoT-edge, Gate single-owner, protocol registry, substitution-velocity,
  agent-boundary, object/action-boundary, and post-selfhost manifest gates
  pass. SoT remains `88/183`, `55/32/1`, with 9 blockers; the project forecast
  remains 83%.
- `routine_build_storage_lifetime_owner.sh` was observed failing before its
  source-MIR assertion on five unrelated body-type leaf-coverage rows. No
  lifetime file was changed. Full `documentation-quality-test-smoke` was not
  run because it recursively scans the protected unrelated untracked
  `docs/compiler_architectures/`; its safe component contracts named above
  were run separately.
- Implementation `45f10ff4c1aafdbf441742ec57221c037ff5d3b7` and Bash 3.2
  gate repair `3f6a63363218fddad90f900251a3f5e78a0bf5b8` are on
  `origin/main`. The first run `33476664357` proved the full self-host path and
  new marker but finished 29/30 because the new gate used Bash 4-only
  `mapfile`; it is not publication evidence.
- Exact repair run `33478794002` is green 30/30. Its self-host log records
  `gen2 == gen3 (173194 lines)`, receipt-bound fixed-point adoption, a
  Pergyra-built DRV-2 installation, and exactly one
  `[self-host-public-parser-callable-contract-json-diagnostic]` marker.
  `build-linux` took 29m09s and `self-host-bootstrap-linux` took 34m28s.
  Publication is complete without a SoT or project-percentage change.
