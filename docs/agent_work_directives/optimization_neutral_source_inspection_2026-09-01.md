# Optimization-neutral source inspection substitution

Status: LOCAL IMPLEMENTATION GREEN

Exact base: `f5f85f1ac2773079e98af6aa176ac061f2d76f60`

## Shared objective card

- Objective: make public `--tokens`, `--ast`, `--dir`, and
  `--capability-manifest` requests execute through their installed Pergyra
  owners when `--opt=dev` is present. These source-inspection artifacts are
  independent of backend optimization policy.
- Priority order: preserve exact inspection bytes; keep one Pergyra request
  owner; remove the C selector's release-only restriction; fail closed without
  native retry; then minimize the patch.
- Fact owner: `DriverRung2CliRequestFromArgsOrDie` owns each admitted typed
  inspection request. `LexerReadSource`/`LexContent`, `ParseRootProgram`,
  `CompileSourceCapabilityManifestVerified`, and
  `CompileSourceDirTextVerified` own the corresponding payloads. The C
  selection owner may classify the public option shape but may not assign
  profile-dependent inspection semantics.
- Last legitimate consumer: `DriverRung2ExecuteReadRequest`; after it emits
  the owner payload, `driver_run_self_host_source_stdout` is an opaque process
  relay only.
- Direct bypass to delete: the `PGY_OPT_RELEASE` predicate in
  `driver_self_host_source_stdout_mode` that rejects otherwise supported
  `--opt=dev` inspection requests before installed Pergyra execution. Explicit
  `--native-pipeline` remains the declared oracle opt-out, not a fallback.
- Forbidden fallback: retrying `driver_run_pipeline`, passing optimization
  profile into the Pergyra inspection request, profile-specific inspection
  bytes, accepting `--verbose` or unrelated option combinations, or treating
  the existing native-only RIR/AIR/HIR modes as part of this rung.
- Verification gate:
  `tests/self_hosted/parity/public_source_inspection_opt_profile_owner.sh`.
  Each dev-profile public request must equal the installed release-profile
  payload and the explicit native dev-profile stdout, invoke the installed
  sibling exactly once, and fail without native timing when that sibling is
  absent. A source-inspection request with `--verbose` remains rejected.
- Falsifying case: any of the four modes changes stdout under `--opt=dev`,
  reaches native timing without the explicit opt-out, publishes partial output
  when the sibling is missing, or makes optimization profile part of the typed
  inspection request.

## Scope and integration

- Implementation scope: `src/compiler/driver_self_host_selection_owner.c` and
  the focused gate above.
- Ratchet scope: only the exact public option/profile boundary and the existing
  source-inspection owner topology. No RIR/AIR/HIR producer, runtime-none
  lowering, query/cache work, or unrelated SoT cleanup is authorized.
- Documentation scope: this directive, the collaboration lease, the active
  handoff card, and the reached SoT row notes if executable evidence changes
  them.
- Integration owner: the primary task owns edits, commit, push, and exact CI
  observation.
- Local budget: static owner checks within 60 seconds; focused parity within
  five minutes; existing four public inspection gates and component/hard
  integration only after the focused gate is green.

## Evidence classification

The opening commands are observations. The focused executable gate promotes
the code change to local implementation evidence; publication still requires
exact remote CI. This card does not change the `88/183`,
`CLOSED=55 BRIDGE=32 ACTIVE=1` census or the 83% project forecast.

## Local implementation evidence

- `driver_self_host_source_stdout_mode` no longer reads optimization profile.
  Runtime, diagnostic format, machine declaration, verbose, and unrelated mode
  boundaries remain explicit.
- A fresh Pergyra-built DRV-2 is installed. The focused gate proves all four
  public dev-profile requests are byte-equal to the installed release payload
  and explicit native dev-profile stdout, invoke one installed child each, and
  fail without native timing when that child is absent. All four `--verbose`
  combinations remain rejected.
- The pre-existing public tokens, AST, capability-manifest, and DIR owner gates
  pass. Component and hard self-host contracts pass. SoT edge, Gate
  single-owner, protocol registry, and substitution-velocity gates pass with
  `88/183`, `55/32/1`, ten protocol rows, and nine bounded blockers.
- Exact remote CI has not yet been observed. Until it is green, this remains an
  active publication lease and no successor rung is inferred.
