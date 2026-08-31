# Zone Parameter Ref Semantic Admission — 2026-08-31

Status: `PUBLISHED — EXACT CI GREEN`

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
  focused zone gate is green. Replacement publication and exact CI were pending
  at that checkpoint.
- Correction publication `bda2d4cff42a4ae85b817f507c1dd1387b4ec2ff`
  completed run `33347320011` at 29/30. Every semantic/executable target in this
  directive passed, including the three Linux zone lines, backend shards 20/20,
  Rocq 9, codegen bootstrap, and full self-host. The integrated fixed point was
  `gen2 == gen3 (172580 lines)` followed by receipt-bound Pergyra-built DRV-2
  adoption and three expected `out_of_subset` policy rows.
- The sole failure was the generated language-word implementation inventory,
  whose `.pgy` use counts changed with this rung. The official registry
  renderer regenerated only that derived projection; its exact gate is green
  locally, as is the full preparation-contract parent with a declared local
  missing-Coq skip. Exact CI already proved the Rocq 9 job. Inventory-only
  publication and exact CI were pending at that checkpoint.
- Inventory commit `c3b1286b3c9565e99b82545d207bed2f061f9272` produced
  green run `33349837888`, but its Markdown-only target omitted the exact
  language-word generator check. The run therefore does not close the one red
  from `33347320011` by itself.
- The Markdown contract now runs `language_keyword_registry_smoke.sh`, and the
  CI-profile owner ratchets that invocation. The complete local Markdown
  command list is green with the regenerated inventory. Publication and one
  remote registry PASS were pending at that checkpoint.

## Publication evidence

- Implementation `672990d2a4eaf58a2a67d065aa904b3995676983`, borrow-view
  repair `bda2d4cff42a4ae85b817f507c1dd1387b4ec2ff`, generated inventory
  `c3b1286b3c9565e99b82545d207bed2f061f9272`, and CI ratchet
  `3988644ae307fb9d24f718e1503691089b90a47f` are published on `origin/main`.
- Exact run `33350057083` completed green 30/30. Linux logged copy/reassignment
  admission PASS, default-negative plus ref execution PASS, aggregate zone-sync
  PASS, and the 146-row language-word registry PASS. Full self-host proved
  `gen2 == gen3 (172580 lines)`, adopted the receipt-bound fixed-point driver,
  installed Pergyra-built DRV-2, and recorded all three policy sources as
  `out_of_subset`.
- The SoT census remains 88 authorities / 183 carriers / `55/32/1`. Return and
  world-embedding transfer remain separate guarded seams. This directive does
  not select a successor rung.
