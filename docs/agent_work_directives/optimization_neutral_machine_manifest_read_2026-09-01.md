# Optimization-neutral machine-manifest read

Status: LOCAL IMPLEMENTATION COMPLETE

Exact base: `3e536a82caa33f1c6ee4c16fc1c1a9c787a456b0`

## Shared objective card

- Objective: make public `--machine-manifest-json --opt=dev` replay the same
  immutable installed companion through the existing Pergyra owner as the
  release-profile request. Backend optimization policy is not a fact of this
  read-only machine declaration artifact.
- Priority order: preserve exact companion bytes; keep the existing installed
  Pergyra request identity; remove the C release-only admission check; fail
  closed on a missing or invalid companion without native retry; then minimize
  the patch.
- Fact owner: the installed companion is the immutable artifact source;
  `DriverRung2CliRequestFromArgsOrDie` admits
  `DriverCliMachineManifestStdout(String)`, and
  `SelfHostMachineLayerDeclarationArtifactPayloadFromPathVerified` validates
  and returns its payload. C selects the packaged companion path and relays
  bytes but may not assign profile-dependent declaration semantics.
- Last legitimate consumer: `DriverRung2ExecuteReadRequest`; after it emits the
  verified payload, `driver_write_self_host_machine_manifest` is an artifact
  path/process relay only.
- Direct bypass to delete: the `PGY_OPT_RELEASE` predicate in
  `driver_self_host_machine_manifest_request_supported`, which rejects an
  otherwise supported dev-profile public request before installed Pergyra
  verification.
- Forbidden fallback: native retry, reconstructing the manifest in Pergyra or
  C, passing optimization profile into `DriverCliMachineManifestStdout`,
  profile-specific bytes, accepting `--verbose`, or changing physical-manifest
  override/runtime/backend behavior.
- Verification gate:
  `tests/self_hosted/parity/public_machine_manifest_installed_self_host_owner.sh`.
  Public dev output must equal release, direct installed, and explicit native
  dev output from any caller working directory. Missing/invalid companions and
  `--verbose` under dev must fail without payload or native timing; the typed
  request remains a one-path variant without profile carriage.
- Falsifying case: dev changes bytes, succeeds without the packaged companion,
  accepts an invalid declaration, reaches native timing, or adds profile to the
  typed request.

## Scope and integration

- Implementation scope: the one release predicate in
  `src/compiler/driver_self_host_selection_owner.c` and the existing public
  machine-manifest owner gate.
- Ratchet scope: only installed companion replay. No MIR read, target-envelope,
  physical manifest override, runtime-none, backend, cache/query, or unrelated
  SoT work is authorized.
- Documentation scope: this directive, collaboration lease, and active handoff
  card. Registry counts/status change only if executable evidence closes a
  declared authority.
- Integration owner: the primary task owns edits, commit/push, and exact CI
  observation.
- Local budget: static owner checks within 60 seconds, the existing focused
  gate within five minutes, and component/hard/SoT contracts only after it is
  green.

## Evidence classification

The opening probe is observation only: release and explicit native dev bytes
agree while public dev fails before delegation. No progress or registry change
is claimed until the installed-owner gate passes. SoT remains `88/183`,
`CLOSED=55 BRIDGE=32 ACTIVE=1`; project forecast remains 83%.

## Local implementation evidence

- `driver_self_host_machine_manifest_request_supported` no longer reads
  `opt_profile`; diagnostic format, runtime, backend/mode exclusions, physical
  manifest override, and verbose boundaries remain unchanged. The typed
  `DriverCliMachineManifestStdout(String)` request remains one-path and carries
  no optimization policy.
- The existing public owner gate passes on the current native launcher and
  receipt-bound installed DRV-2. Release, public dev from two working
  directories, direct installed, and explicit native dev outputs are byte
  equal. Missing and invalid companions under dev publish no manifest and do
  not expose native timing; dev plus `--verbose` remains rejected.
- The focused native rebuild plus gate took 9.2 seconds. Component/hard
  contracts, SoT edge, Gate single-owner, protocol registry, substitution
  velocity, and documentation quality pass with `88/183`, `55/32/1`, ten
  protocol rows, and nine bounded blockers. No registry or project percentage
  change is claimed locally.
