# Zone Parameter Ref Semantic Admission — 2026-08-31

Status: `LOCAL FIX GREEN — REPUBLICATION AND EXACT CI PENDING`

Exact base: `8ef7039459711e64ac07da12b8473057968c0451`, equal to
`origin/main` when this rung opened.

This directive coordinates one reached production ownership seam. It does not
define zone move/return/embedding semantics or invent a fixed lifetime.

## Objective card

- Objective: reject non-receiver zone parameters unless their admitted mode is
  read-only `ref`, delete the function-emission `#error`, and migrate the real
  `ProgramEmitter.Emit` self-host path from by-value zone copies to explicit
  ref carriage.
- Priority order: preserve zone identity; use admitted signature type/mode and
  nominal-kind facts; fail before artifact publication; replace the reached
  compiler dogfood copy; delete backend policy reconstruction; retain stable
  diagnostics and existing fresh-local lifecycle.
- Production entrypoints: current-source
  `pgy-self-driver --emit-c-artifact-verified SOURCE OUTPUT` and the DRV-2 build
  rooted at `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- Fact owners: `SemanticAstFunctionSignatureFacts` owns callable parameter type
  and mode; `SemanticAstNominalConstructorFacts` owns zone kind.
  `SemanticAstZoneParameterBoundaryVerdict` is a subordinate decision inside
  the existing `SFSemanticAstArtifactAdmission` family.
- Last legitimate consumer:
  `SemanticAstBodyTypeBundleFromAdmittedAnalysis*` joins those admitted facts
  once. Function C emission consumes the admitted signature and may project a
  ref ABI but may not decide whether copying a zone is legal.
- Direct bypasses to delete: the thread-safe-only zone parameter guard in
  `function_emit.pgy` and the three by-value zone parameters on the production
  `ProgramEmitter.Emit` action.
- Forbidden fallback: single-threaded by-value success; backend
  `LookupKindType`; a nominal-name allowlist for compiler types; accepting
  default, `inout`, or `own` as an implicit move; automatic lock copying or
  reinitialization; or a user-visible fixed lifetime.
- Verification gate: the default parameter fixture must fail before C
  publication with `zone_value_parameter_requires_transfer`; a `ref` fixture
  must compile and execute exact `7` in single and thread-safe modes; the old
  backend error string must be absent; and a current-source DRV-2 candidate
  must build with `ProgramEmitter.Emit` projected through ref pointers.

## Scope and budget

- Allowed edits: one parameter-boundary semantic owner, its body-bundle join,
  diagnostic vocabulary, deletion of the parameter backend guard,
  `ProgramEmitter.Emit` parameter modes, one negative and one ref fixture, the
  focused zone gate, component/SoT ratchets, and coordination documents.
- Independent edit scopes: none. The primary task is the sole implementation,
  integration, and publication owner.
- Forbidden overlap: zone return or world-embedding semantics, transfer syntax,
  inout/own zone ABI, general lifetime calculus, unrelated artifact admission,
  or another SoT row.
- Integration owner and gate: the existing
  `self-host-domain-runtime-zone-sync-test-smoke` target must preserve local
  lifecycle and execute the parameter gate in Linux CI; the fresh DRV-2 build
  is the production substitution boundary.

## Baseline evidence

- A fresh default zone-parameter probe exits zero and publishes C containing
  `Pergyra zone by-value parameter requires an admitted transfer plan`.
- A first blanket-rejection candidate correctly rejected that probe but then
  falsified its own scope while building DRV-2: production action
  `ProgramEmitter.Emit` carries `types: TypeEnvZone` by value at syntax node
  190925. This is the reached replacement path, not an exception allowlist.
- `PgyCompilerWorld` also embeds executable zones. Return and embedding are
  therefore separate future rungs and retain their current fail-closed backend
  guards in this change.
- SoT remains 88 authorities / 183 carriers /
  `CLOSED=55 BRIDGE=32 ACTIVE=1`; this bounded parameter seam does not by itself
  close the broader semantic-artifact family or change the 83% forecast.

## Current candidate evidence

- `SemanticAstZoneParameterBoundaryVerdictFromAdmittedFacts` now rejects every
  non-receiver zone parameter whose admitted mode is not read-only `ref`.
  Default, `inout`, and `own` cannot silently copy or claim transfer semantics.
- Function C emission no longer contains the zone parameter `#error` or a
  backend nominal-kind decision. Zone return and world embedding guards remain
  unchanged and explicitly outside this rung.
- The focused current-source target is green: existing copy/reassignment
  negatives pass, default parameter admission fails with the stable semantic
  code and no C, and ref carriage executes exact `7` in both single and
  thread-safe builds under `-Werror=discarded-qualifiers`. Existing fresh-local
  and `Clone` lifecycle execution also remains green.
- `ProgramEmitter.Emit` now declares `TypeEnvZone`, `AbiLayoutZone`, and
  `TargetCapabilityZone` as `ref`. A fresh DRV-2 candidate builds successfully;
  its C prototype uses three `const ... *` parameters and its production intent
  call passes the existing stable zone addresses. No aggregate zone copy or
  lock reinitialization remains on that reached path.
- Candidate SHA-256 is
  `AB377C4A31F84789C600C65419C2A83A27F91431860696C9D1260184169FBBC3`.
  Through that candidate, the default fixture exits 1 with no artifact and the
  ref fixture publishes C that executes exact `7` in both modes.
- Component, compiler-world, SoT edge/live-binding, hard-contract, build-source
  inventory, documentation-quality, and agent-sentinel gates are green. The
  broad incremental-size gate still reports the unrelated unmodified
  `src/parser/ast_expr_control_accessors.c` 725/699 violation. The local
  authority adequacy gate records an explicit missing-Rocq skip; exact CI's
  `formal-proofs-rocq9` remains the proof owner.
- First publication `672990d2a4eaf58a2a67d065aa904b3995676983` reached
  exact CI run `33345542503`. Five jobs that first construct the installed
  self-host toolchain failed on one shared strict diagnostic: the new owner
  forwarded `ref signatures` through by-value signature accessors, which the
  transitive borrow summary correctly rejected. This was not a failure of the
  `ProgramEmitter.Emit` ref ABI.
- The corrected owner retains a generated
  `const SemanticAstFunctionSignatureFacts *` and reads the deeply admitted
  parallel rows directly. A negative source ratchet rejects renewed signature
  accessor forwarding. The exact failed seed bootstrap now reports zero
  diagnostics and produces gen2 plus the parser AST producer; the current-gen2
  focused zone gate is green. Replacement publication and exact CI are pending.
