# Optimization-neutral LLVM IR publication

Status: IMPLEMENTATION COMPLETE

Exact base: `24dfa5e82ea0dc308d6d54fdd62cbbae8974272d`

## Shared objective card

- Objective: make public `SOURCE --emit-llvm --opt=dev` work for both stdout
  and `-o FILE` through the existing installed Pergyra source-to-LLVM intent.
  The current language contract applies optimization profile at final binary
  compile/link, not while publishing LLVM IR text.
- Priority order: preserve exact installed IR bytes and execution; retain one
  typed source-LLVM intent; remove the C release-only admission check; fail
  closed without native retry or partial file/stdout; preserve profile-sensitive
  final binary compilation; then minimize the patch.
- Fact owner: `DriverRung2CliRequestFromArgsOrDie` admits the existing
  `DriverCliSourceLlvmArtifact(String, String, Bool)` request.
  `DriverRung2InstalledPublishSourceLlvm` and
  `CompileSourceToLlvmThroughPgyCompilerWorld` own one source/MIR-to-LLVM
  publication intent and receipt. C owns only stdout/file selection, temporary
  workspace publication, and opaque byte streaming.
- Last legitimate consumer: `DriverRung2InstalledPublishSourceLlvm` validates
  the `DriverSourceLlvmIntentOutcome` and one canonical intent trace before the
  C file/stdout adapter observes the artifact.
- Direct bypass to delete: the `PGY_OPT_RELEASE` predicate in
  `driver_self_host_llvm_ir_request_supported`, which rejects both public dev
  forms before installed Pergyra execution.
- Forbidden fallback: native semantic/libLLVM retry, passing profile into the
  typed Pergyra request, profile-specific installed IR bytes, changing final
  executable `-O3/-O0` behavior, accepting JSON diagnostics/verbose/runtime-none
  combinations, buffering/reinterpreting stdout IR, or merging unrelated LLVM
  backend SoT work.
- Verification gate:
  `tests/self_hosted/parity/public_llvm_ir_opt_profile_owner.sh`. Public release
  and dev file/stdout forms must be byte-equal, invoke one installed intent per
  request, compile and execute correctly, and match the current native
  release/dev profile-neutral IR contract. Missing siblings and `--verbose`
  must fail without payload, artifact, or native timing. The typed request must
  not gain profile carriage, while final binary toolchain policy remains
  profile-sensitive.
- Falsifying case: dev changes installed IR, enters native timing, publishes a
  stale/partial file or stdout payload on failure, adds profile to the Pergyra
  request, or neutralizes final executable optimization.

## Scope and integration

- Implementation scope: the one release predicate in
  `src/compiler/driver_self_host_llvm_selection_owner.c`, one focused gate, and
  its explicit Make/component integration.
- Ratchet scope: only public emitted LLVM IR text in stdout/file forms. Default
  LLVM executable builds, runtime objects, JSON diagnostics, debug lines,
  physical manifests, runtime-none, and native-only IR modes stay outside.
- Documentation scope: this directive, the collaboration lease, and active
  handoff card. Registry status/counts move only on separate authority-closure
  evidence.
- Integration owner: the primary task owns edits, commit/push, and exact CI
  observation.
- Local budget: static owner checks within 60 seconds, focused parity within
  five minutes, existing LLVM file/stdout owner gates next, then component/hard
  integration.

## Evidence classification

Opening probes are observations only. Public dev is rejected; installed public
release IR differs representationally from the native oracle, while explicit
native release/dev IR is byte-identical. Existing owner gates already prove
installed file/stdout byte identity and runtime behavior. No progress or
registry change is claimed before the new profile gate passes. SoT remains
`88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`; project forecast remains 83%.

## Local implementation evidence

- `driver_self_host_llvm_ir_request_supported` no longer reads `opt_profile`;
  text diagnostics, runtime, debug/verbose, output-form, and unrelated-mode
  boundaries remain unchanged. Final executable C/LLVM compile/link policy
  still selects `-O3` for release and `-O0` for dev.
- The focused gate passes on the current launcher and receipt-bound installed
  DRV-2. Public release/dev file and stdout IR are byte-identical, dev file and
  stdout agree, the emitted program executes exact `7/11/5`, and explicit
  native release/dev IR remains byte-identical. Counting fixtures observe one
  installed intent for each dev form; missing siblings and `--verbose` publish
  no file/stdout and expose no native timing.
- Existing public LLVM file and stdout owner gates pass in parallel, preserving
  the installed `CompilePergyraProgram` intent, one-count topology, executable
  output, and native-fallback negatives. The focused native rebuild plus gate
  took 11.4 seconds; each existing gate took under seven seconds.
- Component, hard-substitution, SoT-edge, substitution-velocity, and
  documentation integration gates pass. The observed inventory remains
  `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9 executable/process
  blockers. No registry or project percentage change is claimed locally.
- Implementation `5af5261cf39de239ebb932c1bffc200c152572fa` is on
  `origin/main`. Exact run `33465080162` was green 30/30 and proved
  `gen2 == gen3 (173074 lines)` plus receipt-bound DRV-2 installation, but its
  logs did not contain the new LLVM profile gate marker. That run is not
  publication evidence for this rung.
- The missing integration edge was the exact-CI entrypoint:
  `self-host-bootstrap-linux` calls
  `self-host-installed-driver-cli-mode-test-smoke`, while the new gate was only
  downstream of the public LLVM replacement chain. The local repair adds the
  gate to that existing aggregate without a second workflow step or fixed-point
  build. Running the same aggregate locally succeeds and emits
  `[self-host-llvm-ir-opt] dev file/stdout publication is installed and
  profile-neutral`. Component, hard-substitution, SoT-edge, velocity, and
  documentation ratchets pass after the repair. The publication evidence below
  closes the remaining commit/push and exact-CI obligations.

## Publication evidence

- Implementation `5af5261cf39de239ebb932c1bffc200c152572fa` and exact-CI
  integration repair `bd13e1c1a47a2f334e64e20babfdf6108a272def` are on
  `origin/main`.
- Exact run `33468852139` is green 30/30. `build-linux` completed in 25m04s;
  `self-host-bootstrap-linux` completed in 33m42s. The latter log contains the
  required `[self-host-llvm-ir-opt] dev file/stdout publication is installed
  and profile-neutral` marker exactly once.
- The same exact log proves `gen2 == gen3 (173074 lines)`, adoption of the
  receipt-bound fixed-point driver, Pergyra-built DRV-2 installation, and the
  preceding machine-manifest/source-inspection/MIR profile gates. The final
  inventory remains `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, with 9 blockers;
  project forecast remains 83%.
