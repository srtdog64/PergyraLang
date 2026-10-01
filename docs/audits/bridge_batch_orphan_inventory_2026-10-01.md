# ABI and collection GraphPlan orphan inventory

Status: READ-ONLY deletion preparation, not an applied cleanup or a green gate.
Observed HEAD: `62a83a8256bf4fa34878cca8f1f14641736fdeba`.
Date: 2026-10-01, Asia/Seoul.
Source window: the shared tree while root prepares the ArrayString ABI batch.
Unrelated source and Git index are untouched. Only this report is written.

## Method and limits

- Scanned 116 compiler owner files matching ArrayString/collection and 299
  top-level function definitions. Two had no source call sites.
- Checked exact whole-word references across source, tests, build files and
  repository text; prefix matches such as `...ValueResultParameterReady` are
  not callers of `...ValueResultParameter`.
- Followed relative imports from the installed builder's actual entrypoint,
  `driver_bootstrap_main.pgy`: 1,798 reachable files, zero unresolved imports.
  Import reachability proves module inclusion, not function execution.
- Cross-checked `self_host_compiler_build.sh:29` and the Makefile builder route.
  The owners below are imported modules, not separate build targets or CLI
  entrypoint functions. No explicit extern/public entrypoint was found for the
  two unused plain lookup functions.
- Checked exact function and file references separately. A live module's
  `require_file`, cap, registry and import anchors do not keep every function
  alive; conversely a generated test call or export would defeat deletion.
- Counts below are non-definition, non-comment source call-site lines, not
  dynamic invocation counts. No compiler build or executable test ran in this
  review. Ignored historical/generated build artifacts are not live callers.
- This is not an exhaustive whole-compiler orphan inventory. Generated symbol
  ABI, external integrations and independently built tools require their own
  owner check before deleting a wider function or module.

## Deletion candidates: function only, never the owner file

### DirectMirScalarCfgCollectionValueRow

- Definition: `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_fact_owner.pgy:118`.
- Source callers: 0. Exact test anchors/calls: 0. Whole-repository exact-name
  search found only its definition.
- Entry/export status: plain internal lookup function; not Main, action,
  extern, CLI switch, registered producer term or standalone build entrypoint.
- Module status: imported by the installed driver; it owns used collection
  plan structs and other live queries. `OWNERS.md:4552`, the SoT derived row,
  and `self_hosted_component_contract_smoke.sh:26048` onward require the file
  and its live plan contracts, not this function.
- Responsibility/body: linear unique-string lookup in `plan.values.value_ids`.
  Current consumers use carried graph/global/definition identities instead;
  the remaining live queries and typed row joins must stay.
- Judgment: **function deletion candidate**, high confidence within the
  in-repository compiler import/build/test scope. Delete only the function
  after root's normal diff check and integrate into the existing single rebuild;
  do not remove the module, its owner identity, or collection fact rows.
- Verification after deletion: focused collection owner/static gate plus the
  selected one-MIR collection C/LLVM integration. No separate rebuild is
  justified solely by this unused lookup.

### DirectMirScalarProgramArrayStringValueResultParameter

- Definition: `src/self_hosted/compiler/direct_mir_scalar_program_array_string_abi_owner.pgy:285`.
- Source callers: 0. Exact test anchors/calls: 0. Whole-repository exact-name
  search found only its definition.
- Entry/export status: same internal/non-entrypoint classification above.
- Module status: installed-driver import closure contains it. ABI file/cap
  anchors and several parity scripts require the owner file; none requires
  this exact function. The near-name `...ValueResultParameterReady` is a
  different live callable-policy function and must not be removed.
- Responsibility/body: collapses one routine's value-result parameter rows
  to a single ordinal, returning -1 when multiple rows exist. Current C/LLVM
  consumers use `DirectMirScalarProgramArrayStringValueResultAt(fact, routine,
  parameter)`; that exact per-parameter query has 14 source call sites in eight
  files, including both value-result emitters. It supports multiple rows
  without the retired single-parameter convenience shape.
- Judgment: **function deletion candidate**, high confidence within the
  inspected scope. Keep the ABI fact owner, canonical-empty behavior, digest,
  cross-family admission, per-parameter query and live parameter-policy gate.
- Verification after deletion: root's ArrayString ABI/value-result focused
  C/LLVM gate, including two value-result parameters, through its one planned
  compiler rebuild. This report does not claim that deletion already ran.

## Live owners and wrappers that must not be mistaken for orphans

| Function and definition | Source call sites / files | Exact test files | Decision |
|---|---:|---:|---|
| `DirectMirArrayStringCapturedAbiReady`, `direct_mir_array_string_abi_fact_owner.pgy:6` | 6 / 4 | 0 | keep; exact captured-family admission owner, no test-name anchor is not deadness |
| `DirectMirArrayStringAbiProjectionReadyFor`, `direct_mir_array_string_abi_projection_owner.pgy:35` | 6 / 3 | 1 | keep; target-qualified physical projection acceptance |
| `DirectMirScalarProgramArrayStringAbiProjectionReadyForFact`, `direct_mir_scalar_program_array_string_abi_projection_owner.pgy:6` | 14 / 12 | 3 | keep/reuse; joins admitted program ABI fact to target projection, not a pass-through helper |
| `DirectMirScalarProgramArrayStringAbiProjectionFromFact`, same file `:29` | 2 / 2 | 2 | keep; C and LLVM emission both derive one projection and preserve canonical absence |
| `DirectMirScalarProgramCArrayStringCarrierType`, same file `:19` | 5 / 5 | 3 | keep; validates carried ABI/projection at C carrier boundary |
| `DirectMirScalarProgramArrayStringValueResultAt`, `direct_mir_scalar_program_array_string_abi_owner.pgy:268` | 14 / 8 | 1 | keep; actual copy-in/out and caller carriage query |
| `DirectMirArrayStringPublicCValueType`, `direct_mir_array_string_abi_projection_owner.pgy:28` | 2 / 1 | 0 | keep; named public target spelling `PgyArray_String` |
| `DirectMirArrayStringLlvmValueType`, same file `:31` | 2 / 1 | 1 | keep; named LLVM value spelling `%pgy.array.string` |

`CompilerAbiLayoutArrayStringCValueType` in `abi_layout_row_owner.pgy:265`
returns `pgy_as`, not `PgyArray_String`. Those two spelling owners are not
byte-equal duplicate wrappers. Merging them without tracing their distinct
carrier/projection boundaries would change codegen contracts.

For root's active ABI batch, reuse the existing captured-layout admission,
target projection and program-fact cross-seal owners above. A new generic
helper/helper bucket or one-use pass-through is unnecessary. Add a function
only if a new named cross-family/consumer responsibility actually needs one.

## Live old paths: migrate separately, do not delete as dead code

### Production mutation self-test

Definition: `direct_mir_collection_program_plan_owner.pgy:110`,
`DirectMirCollectionProgramPlanMutationRejected`.

Source callers: 1, at `DirectMirCollectionProgramPlanFromAdmitted:189`.
The installed import graph reaches this owner through the collection projection
route. Each successful plan production copies the plan, repairs digests after
deliberate target/carriage mutations, and invokes production readiness again.
This is an executable negative self-test, not an orphan.

Exact anchors exist in:

- `tests/self_host_hard_contract_smoke.sh:457`;
- `tests/self_hosted_component_contract_smoke.sh:18136`;
- `tests/self_hosted/parity/one_mir_array_param_projection.sh:57`.

Judgment: **test-owner migration candidate**, not immediate deletion in the
ArrayString ABI batch. Preserve production `PlanReady` admission; move mutation
construction/refusal checks into a real negative probe, migrate the presence
anchors to executable refusal evidence, then remove the production negative
copy/call and its now-unused function. Do not rename the same production
self-test into another owner and claim its repeated work disappeared.
No performance measurement was made, so cost reduction remains a proposal.

### Legacy ArrayString element-ownership fallback

Definition: `direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy:8`,
`DirectMirScalarProgramLegacyArrayStringLocalBorrowsElements`.

Source callers: 1, `DirectMirScalarProgramArrayStringCleanupDropSymbol:80`.
An exact structural anchor exists in `self_hosted_component_contract_smoke.sh`.
When the typed collection-ownership transition row is absent, cleanup revisits
local definitions/literal expression graphs; otherwise it consumes the typed
transition terminal state.

Judgment: **retain until owner-coverage migration**. It is not dead and cannot
be deleted merely because its name says Legacy. Supply complete transition
coverage for each admitted local at the semantic ownership boundary, refuse
missing required coverage, migrate cleanup consumers, and add the no-legacy-read
negative gate before deleting this path. ABI layout closure alone is not the
element-ownership proof needed here. Keep unknown/borrowed/owned terminal-state
behavior under its declared ownership contract; this review does not redefine it.

### Final ArrayString boundary reconstruction

Definition: `direct_mir_scalar_program_array_string_boundary_plan_readiness_owner.pgy:6`,
`DirectMirScalarProgramArrayStringBoundaryReadyForPlan`.

Source caller: `direct_mir_scalar_cfg_program_extension_readiness_owner.pgy:212`.
It rebuilds `ArrayStringBoundaryFactFromOwners` from sealed GraphPlan columns and
compares the rebuilt fact. Judgment: **live, review reconstruction separately**.
A receipt/digest cross-seal may eventually replace this repeated construction,
but removing its proof obligation without a stale/crossed-fact negative gate
would reopen admission. This is not a zero-caller cleanup candidate.

## Proposed integration order

1. Root may retire the two unused functions as a small cleanup within the one
   authorized ABI build batch after verifying their current source has not
   changed. Keep all file/registry owner identities and unrelated dirty edits.
2. Verify the actual requested ArrayString ABI consumer migration first; orphan
   deletion does not count as a BRIDGE closure or self-host substitution.
3. Leave production mutation self-tests and ownership fallbacks as explicitly
   separate owner migrations unless the active executable rung reaches them.
4. Record the final deleted symbols and observed gates in root's integration
   audit. This preparation report alone is not deletion or gate evidence.
