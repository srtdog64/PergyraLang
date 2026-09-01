# LLVM IR Public JSON Diagnostic Receipt

Status: ACTIVE — LOCAL GREEN, EXACT CI PENDING

Exact base revision: `f6c4f2a04202b4f6be49f5f7991a9f090a26d9d2`

This directive coordinates one bounded executable replacement. It is not a
semantic owner, SoT registry, progress counter, or completion claim.

## Shared objective card

- Objective: make public `SOURCE --emit-llvm --error-format=json`, in both
  stdout and `-o FILE` forms, enter the existing installed Pergyra LLVM intent
  instead of failing at the C adapter's text-only selection predicate.
- Priority order: preserve parser/semantic diagnostic identity; preserve valid
  LLVM IR bytes; carry the admitted JSON request into the existing typed
  Pergyra request; keep C transport opaque and fail closed; minimize the
  reached edit surface.
- Fact owner: `DriverCliSourceLlvmArtifact(String, String, Bool)` owns the
  admitted request identity. Parser/semantic projectors own diagnostic meaning,
  and `SelfHostPublicDiagnosticReceiptWireFromOwnedFacts` owns public wire
  serialization.
- Production entrypoints: default installed
  `pgy SOURCE --emit-llvm --error-format=json` and
  `pgy SOURCE --emit-llvm --error-format=json -o FILE`.
- Direct C-owned bypass to delete: the `DIAG_FORMAT_TEXT` predicate in
  `driver_self_host_llvm_ir_request_supported` and the two LLVM IR wrappers'
  hard-coded `emit_json_diagnostic=false` materialization calls.
- Last legitimate consumers: the existing source-LLVM Pergyra intent, then
  `driver_run_self_host_artifact_process` as an opaque public receipt relay and
  the existing stdout/file publication boundary.
- Forbidden fallback: C diagnostic message parsing, a native retry or semantic
  preflight, a second source compile, dual text/JSON emission, mode inference
  from the environment, partial/stale file publication, or relaxation of
  runtime-none, verbose, debug-line, final-binary, and explicit native-pipeline
  contracts.
- Focused gate:
  `tests/self_hosted/parity/public_llvm_ir_json_diagnostic_receipt_owner.sh`.
- Falsifying cases: valid JSON-selected release/dev requests must retain the
  text-mode LLVM IR bytes; the reached duplicate callable-contract rejection
  must publish the exact parser-owned public JSON receipt on stderr only;
  missing/malformed/crosswired child receipts must fail without an artifact or
  native timing; text mode and unsupported option combinations must remain
  unchanged.

## Edit scopes and overlap

- C selection scope: admit both text and JSON diagnostic formats for the
  already-supported LLVM IR request shape only.
- C transport scope: pass one Bool into existing materialization; do not add a
  diagnostic identity, renderer, retry, or alternate execution lane.
- Pergyra scope: no new semantic owner or request variant is authorized; reuse
  the existing Bool and source-LLVM intent.
- Gate scope: one focused public stdout/file parity and negative gate plus the
  minimum installed-CLI CI aggregate wiring needed for exact observation.
- Documentation scope: refresh collaboration and handoff only from observed
  evidence. SoT status and the 83% forecast do not change from selection
  carriage alone.
- Protected unrelated untracked paths are outside inspection, edit, and
  staging: `docs/compiler_architectures/`, `pgy-80135c2c/`, and
  `pgy-91d769ec/`.

The primary task is the sole edit, integration, commit/push, and exact-CI
observation owner. Outputs before the focused gate passes are implementation
candidates, not completion evidence.

## Opening evidence

- With `PGY_SELF_DRIVER_BIN` bound to a missing sibling, public text-mode
  `--emit-llvm -o FILE` reaches the installed-driver availability failure.
  The otherwise-identical JSON request stops earlier at
  `--emit-llvm file options are outside the installed self-host driver
  contract`; it publishes no artifact.
- A fresh current-source DRV-2 accepts
  `--emit-source-llvm-ir-json-diagnostic-verified` and publishes
  `pgy.selfhost.public-diagnostic.v1` with exact parser-owned code, axis, and
  name for the duplicate callable-contract fixture. The repository `bin/`
  self-driver is older than that fresh isolated installation and is not used
  as semantic evidence.

## Observed local evidence

- The warning-clean native launcher build passes. A fresh current-source DRV-2
  and machine-layer manifest are installed beside an isolated copy of that
  launcher; repository `bin/pgy-self-driver` staleness is not hidden by the
  test setup.
- Valid text/JSON release and JSON dev stdout/file requests publish byte-equal
  LLVM IR. The reached invalid stdout/file requests fail on stderr only with
  the exact parser-owned JSON payload and no private marker, partial artifact,
  or native timing.
- The focused gate, existing LLVM stdout/profile gates, both preceding public
  JSON receipt gates, and the complete installed-driver CLI aggregate pass
  against the isolated sibling installation. The aggregate executes the new
  focused marker once.
- Missing, malformed, absent, and crosswired receipt paths fail closed. Static
  checks reject hard-coded text carriage, C diagnostic meaning, and native or
  string-shell fallback.
- Component/hard, SoT edge, Gate single-owner, protocol, substitution velocity,
  agent-boundary, object/action, and post-self-host manifest contracts pass.
  The component source inventory took about 13.5 minutes, exceeding its stated
  60-second static budget; this is recorded as performance evidence, not hidden
  as a failure or used to justify caching.
- SoT remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9 blockers. This is
  an executable consumer migration inside `diagnostic.catalog`, not whole-row
  closure; the project forecast remains 83%.
