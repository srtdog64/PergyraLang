# Optimization-neutral MIR read outputs

Status: IMPLEMENTATION COMPLETE

Exact base: `645d6e42014cc7959416bc5c5aecbe4df3c4d46d`

## Shared objective card

- Objective: make public `--mir`, `--mir --error-format=json`, and
  `--mir-json` requests execute through their installed Pergyra owners when
  `--opt=dev` is present. Backend optimization policy must not select or alter
  these read-only MIR projections.
- Priority order: preserve exact installed-owner bytes; retain the existing
  text-diagnostic, JSON-diagnostic, and canonical-MIR request identities;
  remove the two C release-only admission checks; fail closed without native
  retry; then minimize the patch.
- Fact owner: `DriverRung2CliRequestFromArgsOrDie` admits the existing
  `DriverCliSourceMirDiagnosticStdout`,
  `DriverCliSourceMirJsonDiagnosticStdout`, and
  `DriverCliSourceMirStdout` variants. `DriverSourceMirDiagnosticPayloadOrDie`,
  `DriverSourceMirJsonDiagnosticPayloadOrDie`, and
  `DriverSourceMirCanonicalPayloadOrDie` own their payloads. C may classify the
  public option shape and relay bytes but may not assign profile-dependent MIR
  read semantics.
- Last legitimate consumer: `DriverRung2ExecuteReadRequest`; after it emits
  the selected payload, `driver_run_self_host_mir_diagnostic_request` and
  `driver_run_self_host_mir_json` are process relays only.
- Direct bypass to delete: the `PGY_OPT_RELEASE` predicates in
  `driver_self_host_mir_diagnostic_request_supported` and
  `driver_self_host_mir_json_request_supported`, which reject otherwise
  supported `--opt=dev` requests before installed Pergyra execution. Explicit
  `--native-pipeline` remains the declared native oracle opt-out.
- Forbidden fallback: retrying `driver_run_pipeline`, carrying optimization
  profile in any of the three typed Pergyra requests, changing bytes by
  profile, accepting `--verbose` or unrelated combinations, redefining MIR
  representation, or bundling machine-manifest/RIR/AIR/HIR/runtime-none work
  into this rung.
- Verification gate:
  `tests/self_hosted/parity/public_mir_opt_profile_owner.sh`. Each public dev
  request must equal its release request and direct installed owner, invoke
  exactly one installed child, and fail without native timing or partial
  output when the sibling is absent. `--verbose` remains rejected and the
  three request variants remain one-path shapes without optimization carriage.
- Falsifying case: any admitted MIR read changes stdout under `--opt=dev`,
  enters native timing without explicit opt-out, publishes partial output when
  its sibling is missing, or adds optimization profile to the typed request.

## Scope and integration

- Implementation scope:
  `src/compiler/self_host_mir_diagnostic_stdout_owner.c`,
  `src/compiler/driver_self_host_selection_owner.c`, and the focused gate plus
  its counting fixture.
- Ratchet scope: only the public option/profile boundary and the existing MIR
  read-owner topology. No MIR semantic or protocol change, native-only IR
  producer, machine-manifest admission, cache/query work, or unrelated SoT
  cleanup is authorized.
- Documentation scope: this directive, the collaboration lease, and the active
  handoff card. Registry counts/status change only if executable evidence
  actually closes a registered authority.
- Integration owner: the primary task owns edits, commit, push, and exact CI
  observation.
- Local budget: static owner checks within 60 seconds; focused parity within
  five minutes; existing MIR representation gates and component/hard contracts
  only after the focused gate is green.

## Evidence classification

Opening probes and this objective card are observations, not progress. Only a
green focused gate plus the existing MIR owner gates can promote this work to
an executable substitution. This card does not change the `88/183`,
`CLOSED=55 BRIDGE=32 ACTIVE=1` census or the 83% project forecast.

## Local implementation evidence

- The two C admission functions no longer read `opt_profile`; diagnostic
  format, runtime, machine declaration, verbose, and unrelated-mode boundaries
  remain explicit. No Pergyra request variant or MIR representation changed.
- A fresh receipt-bound Pergyra-built DRV-2 is installed. The focused gate
  passes all three typed requests: release/dev/direct bytes agree, the counting
  child observes exactly one invocation per request, missing children publish
  no output and never expose native timing, and all `--verbose` combinations
  remain rejected. The focused script itself took 5.7 seconds after the
  required DRV-2 rebuild.
- Existing public MIR diagnostic and MIR JSON owner gates pass. Reaching the
  latter exposed three stale static expectations: it still named the retired
  lexical canonicalizer, expected a third native pipeline dispatch already
  deleted from the production root, and did not classify the new focused gate
  as an installed-owner consumer. Its ratchets now name the final existing-file
  identity owner, require exactly the two declared native opt-outs, and exclude
  only this named installed-owner gate from the oracle scan.
- Component and hard self-host contracts pass. SoT edge, Gate single-owner,
  protocol registry, and substitution-velocity gates pass with `88/183`,
  `55/32/1`, ten protocol rows, and nine bounded blockers. This bounded
  substitution therefore changes neither registry status nor project forecast.
- The Make target correctly rebuilds the native launcher and receipt-bound
  DRV-2 before the focused gate. A second identical invocation was deliberately
  stopped during its unconditional phony self-host rebuild because the same
  rebuilt binaries had already passed the focused script.
- Implementation `1d76bd1cfdac736df706ef114813c9d272b4e288` is on
  `origin/main`. Exact run `33455765599` is green 30/30. Its full self-host log
  records `gen2 == gen3 (173074 lines)`, installs the receipt-bound Pergyra
  DRV-2, and passes both the prior source-inspection profile gate and the new
  MIR-read profile gate before installed CLI aggregation. Full self-host took
  33.70 minutes and build-linux 25.48 minutes; codegen bootstrap, sanitizers,
  Windows/macOS, Rocq, backend toolchain, and all 20 backend shards also pass.
  The publication falsifier is closed; no successor rung is inferred from this
  boundary or its timing.
