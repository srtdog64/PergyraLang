# Pre-Self-Host Expansion Ledger

Status: `pre-self-host-expansion-ledger` (2026-06-30)

Hard self-hosting should not start by copying the C compiler's fragmentation.
Before a Pergyra-written compiler slice can grow, the language and compiler
surface it needs must already have a named owner, a gate, and a no-hidden-
fallback rule.

This ledger is the "bring it now" list. It records which expansion surfaces are
ready, which are active blockers, and which are deliberately held out of the
hard self-host path.

## Expansion Import Rule

Every surface needed by hard self-hosting must be in exactly one state:

| State | Meaning |
|---|---|
| `READY` | A named owner exists, a gate proves the contract, and a hard rung may consume it. |
| `ACTIVE` | The owner shape is known, but a Pergyra compiler slice must not rely on it as complete. |
| `HOLD` | The surface is intentionally excluded from the hard self-host path for now. |

When a surface is ready only for the current self-host C subset but still
active for native C/LLVM/global consumption, it must be split into two scoped
rows: one `READY` row with the subset scope in its surface name, and one
`ACTIVE` row with the global scope in its surface name. A `READY` row must not
carry a hidden "still active globally" caveat in prose.

An unclassified surface is not allowed. If a hard rung needs it, add the owner
and gate first or reject the input visibly. This is the no-hidden-fallback
rule for pre-self-host expansion.

## Ready Surfaces

| Surface | Owner | Gate | Self-host use |
|---|---|---|---|
| Compiler world shape | `PgyCompilerWorld`, `CompilePergyraProgram` | `self-host-compiler-world-contract-test-smoke` | one visible compiler flow, not a C folder graph |
| Path/world binding facts | `path_manifest_owner.pgy`, `SelfHostPath` | `self-host-preparation-contract-test-smoke` | stable source/test/parity paths, import-relative paths, and active stage-to-zone/actor/intent rows |
| File IO basics | `FileExists`, `ReadFile`, `WriteFile`, `Exit`, `Args` | `self-host-codegen-parity-test-smoke`, semantic parity fixtures | standalone tools and compiler slices |
| Directory walk | `DirWalk(String)` sorted snapshot | `filesystem-directory-walk-test-smoke` | live inventories without committed file-list aliases |
| Deterministic collections | `MapKeys`, `SetValues` over scalar compiler keys | `stage4-determinism-test-smoke` | stable diagnostics, codegen, MIR JSON, cache keys |
| Allocator lanes | `AllocatorScratch`, `AllocatorResult`, `AllocatorPersistent`, `AllocatorDestroy` | `runtime-abi-lifetime-test-smoke`, `abi-ownership-shape-test-smoke` | scratch/result/persistent compiler-pass lanes |
| Diagnostic rendering | `src/self_hosted/lib/diagnostic.pgy` | diagnostic catalog and semantic parity gates | no raw diagnostic construction in entrypoints |
| JSON read/emit primitives | `src/self_hosted/lib/json_scan.pgy`, `src/self_hosted/lib/json.pgy`, `src/self_hosted/lib/json_fact_table.pgy`, `src/self_hosted/lib/json_emit.pgy` | component contract and real-source selfcheck | `json_scan.pgy` owns cursor/string scan primitives; `json.pgy` owns shared string/number/span reads and document number-field reads; `json_fact_table.pgy` owns bounded object and array-object boundary facts plus document string-field equality facts; `json_emit.pgy` owns JSON string escaping plus field/object/array emission for fact-shaped tools, and emit consumers import it directly |
| MIR body facts | MIR source-shape / expression / source-local facts | `cfg-body-dataflow-test-smoke`, `ast-read-surface-smoke`, `self-host-mir-json-parity-test-smoke` | fact-only MIR lowering for the supported subset |
| Raw/FFI policy | scoped raw/unsafe boundary documents and runtime gates | `raw-escape-contract-test-smoke` | normal compiler slices stay out of raw pointer escape |
| Bit/layout boundary | `bits(..., order=...)`, `reinterpret(..., layout/endian/abi/world=...)` policy | `abi-ownership-shape-test-smoke`, language contract gates | no hidden logical-bit or backend-local layout defaults |
| Runtime materialization policy | AIR/MIR evidence and runtime-frontier docs | AIR erasure/materialization gates | no hidden runtime calls on static hot paths |
| Target capability envelope (self-host C subset) | `target_capability_owner.pgy`, `target_capability_manifest.pgy`, `TargetCapabilityZone` | `self-host-compiler-world-contract-test-smoke`, `self-host-target-capability-envelope-parity-test-smoke`, real-source selfcheck | CPU/C/LLVM/self-hosted projection rows, target facts, and fallback reasons are named owner facts. `CompilerTargetCapabilityEnvelopeReady()` consumes those facts instead of comparing row indexes to string literals, and the runnable manifest emits a stable target-capability artifact with a missing-fact fail-closed case across C/LLVM-built self-host tools. The self-host C codegen run boundary consumes that envelope before `GenerateC`, so hidden CPU fallback cannot enter that hard rung by omission. |
| Sandbox capability/frame-budget envelope | `sandbox_capability_owner.pgy`, `sandbox_capability_manifest.pgy`, `SandboxCapabilityZone` | `self-host-compiler-world-contract-test-smoke`, `self-host-sandbox-capability-parity-test-smoke`, component contract | Filesystem, network, clock, random, subprocess, storage, render, input, per-frame fuel, host-call, command, memory, queue, stream, wall-clock, ambient-denial, and blocking host-call boundary facts are named owner rows. The runnable manifest emits a stable artifact and a missing-budget fail-closed JSON artifact across C/LLVM-built self-host tools. This does not claim the production sandbox runtime is complete; it prevents sandbox/frame-budget claims from living only in docs or shell. |
| Backend dumb-emitter contract | `backend_emitter_contract_owner.pgy`, `backend_emitter_contract_checker` | `self-host-backend-emitter-contract-parity-test-smoke`, `self-host-component-contract-test-smoke` | The first backend dumb-emitter rows are Pergyra-owned and runnable: selected backend files must contain MIR/ABI runtime-row consumer terms and must not contain backend-local runtime-name synthesis terms. The self-host checker consumes negative schema, count-field names, finding-kind vocabulary, owner-not-ready diagnostics, and the current C slot/resource fail-closed runtime-row callsites from the backend-emitter owner, then proves clean, missing-required, missing-input, and forbidden-hit paths across C/LLVM-built tools. This does not replace the full Bash `backend-fail-closed` gate yet; it moves more of that contract into hard self-host parity. |
| Backend AIR access contract | `backend_air_access_contract_owner.pgy`, `backend_air_access_checker`, TestHarness backend-contract paths | `self-host-backend-air-access-parity-test-smoke`, `self-host-component-contract-test-smoke` | AIR remains verification-only: backend sources under `src/codegen` must not include AIR headers or consume AIR graph node types. The contract owner now names the schema, scan root, source-file extensions, JSON count fields, forbidden AIR token list, finding kind, negative self-test path, and owner-not-ready diagnostic. The self-host checker consumes those facts, walks the backend source tree with `DirWalk`, emits `pgy.selfhost.backend-air-access.v1`, and proves the forbidden-hit negative artifact through both C-built and LLVM-built tools when LLVM is available. The broader Bash AIR non-impact gate remains the full production backstop while this contract moves into hard self-host parity. |
Backend AIR access report delta, 2026-07-08:
`backend_air_access_checker/report_owner.pgy` now owns the checker report JSON
shape, count rows, finding objects, and report-owner readiness predicate.
`main.pgy` walks backend source files and scans forbidden AIR terms only.
| Backend ABI layout contract | `abi_layout_row_owner.pgy`, `backend_abi_layout_contract_checker`, TestHarness backend-contract paths | `self-host-backend-abi-layout-contract-parity-test-smoke`, `self-host-component-contract-test-smoke` | Native/backend ABI layout closure now starts from the same compiler-world ABI row owner instead of a separate shell-only list. The checker requires selected native MIR ABI layout rows, runtime-function consumers, and native `MIRAbiTargetPolicy` rows carrying the current `selfhost-c` projection/fact/fallback policy. It rejects old alias/runtime-name synthesis terms such as `_rel` spellings and suffix extraction, consumes the negative schema, count-field names, and finding-kind vocabulary from the ABI owner, and proves clean, missing-required, missing-input, and forbidden-hit artifacts through both C-built and LLVM-built tools when LLVM is available. The broad `abi-ownership-shape-test-smoke` remains the full production backstop until native C, LLVM, and self-hosted backend consumers all read the same concrete row table. |
Backend ABI layout report delta, 2026-07-08:
`backend_abi_layout_contract_checker/report_owner.pgy` now owns the checker
report JSON shape, count rows, finding objects, and report-owner readiness
predicate. `main.pgy` scans ABI layout required/forbidden backend terms and
runs fail-closed self-test modes for missing-required, missing-input, and
forbidden-hit artifacts only.
| Self-host C symbol spelling | `src/self_hosted/compiler/symbol_table_owner.pgy`, `src/self_hosted/codegen/type_facts/type_env.pgy`, `src/self_hosted/codegen/emission/expr_binding_rewrite_owner.pgy` | component contract, real-source selfcheck, codegen parity | function/method/operator/enum names, namespace-qualified call spellings, struct field spellings for declarations, literals, and member access, source-to-C binding spellings, `inout` temporary parameter spellings, foreach loop temporary spellings, and try/match emission temporary spellings are consumed from owner rows inside the current self-host C subset; declarations append `cbind` rows and expression binding rewrite consumes those rows, with `c_reserved_binding` proving non-identical source/C local spelling |
| Self-host C ABI type spelling | `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy` | component contract, real-source selfcheck, codegen parity | parameter, return, local, field, and nominal struct C type spellings, empty parameter-list spelling, and bare-return default values are consumed from one owner inside the current self-host C subset |
| Self-host call parameter modes | `src/self_hosted/codegen/type_facts/type_env.pgy` | component contract, real-source selfcheck, codegen parity | function signature emission still records `pm` rows in the flat environment, but expression rewrite consumes parameter-mode count/index facts through `type_facts` instead of parsing the CSV locally |
| Self-host typed AST-text bridge | `src/self_hosted/codegen/input/ast_text_inventory_owner.pgy`, `src/self_hosted/codegen/input/ast_text_typed_arena_owner.pgy`, `src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy`, `src/self_hosted/codegen/input/ast_text_array_literal_owner.pgy`, `src/self_hosted/codegen/input/ast_text_enum_variant_owner.pgy`, `src/self_hosted/codegen/text/enum_literal_owner.pgy`, `src/self_hosted/codegen/text/expr_sequence_owner.pgy`, `src/self_hosted/codegen/text/struct_literal_call_owner.pgy`, `src/self_hosted/codegen/text/struct_literal_field_owner.pgy`, `src/self_hosted/codegen/typed_ast_node_skeleton.pgy`, plus `pgy --ast` parameter-mode preservation | component contract, real-source selfcheck, parser parity, codegen parity, compiler-world contract | raw `pgy --ast` line splitting, `CodegenAstTextNode` inventory, indentation, parent/kind/payload/name/type/value/aux-value/mode rows, marker-node predicates, blank-line filtering, `[export]` normalization, program-level typed routing, declaration collector prepasses, `CodegenAstTextRowFactInput`, function/return/role/nominal/enum-name/enum-variant/field/parameter/statement row facts, array literal initializer/element facts, payload-free enum variant payload facts, payload-free enum literal projection facts, top-level comma-separated expression sequence facts, struct literal call-envelope facts, typed struct literal field-entry fact rows, function signature/header facts, statement body reads, function/statement emission depth traversal and function/declaration name/type/mode plus statement name/value/aux-value and payload-free enum variant consumption through typed arena facts, `inout`/`own`/`ref` parameter-mode facts, cursor expectation checks, typed arena projection facts, `CodegenTypedAstBridgeReady`, and the typed AST arena payload contract are consumed from input/text fact owners during the transitional text bridge; `CompilerEmissionFactReady()` also makes the typed AST arena contract load-bearing for `ProgramEmitter` readiness in `PgyCompilerWorld`; the retired `ast_text_statement_owner.pgy` alias file must not return |
| Self-host C collection runtime symbols | `src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | `Array<Int>` / `Array<String>` helper call names and the bootstrap-only `Array<CodegenAstTextNode>` record-array lane are consumed from collection kind-code facts inside one owner in the current self-host C subset |
| Self-host C math/random symbols | `src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | math/random helper and target-library call names are consumed from one owner inside the current self-host C subset |
| Self-host C host I/O/process symbols | `src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | file, directory-walk, argv, C process entrypoint signature, and process-exit helper or target-library call names are consumed from one owner inside the current self-host C subset |
| Self-host C Option/Result runtime symbols | `src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | `Option<Int>` / `Option<String>` / `Option<Float>` / `Option<Double>` / `Result<Int>` helper call names and scalar option typedefs are consumed from one owner inside the current self-host C subset. `Option<Long>` uses the same `selfhost-c` value lane as `Option<Int>` because both rows map to `long long` on this target. |
| Self-host C string/text symbols | `src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | supported string/text helper, numeric log format strings, and conversion/printing target-library call names are consumed from one owner inside the current self-host C subset |
| Runtime call ABI row projection | `src/self_hosted/compiler/runtime_call_abi_row_owner.pgy`, `src/self_hosted/compiler/runtime_call_abi_row_manifest.pgy`, native `mir_abi_resource_runtime_row_*` accessors | `self-host-runtime-call-abi-row-parity-test-smoke`, `test-mir`, component contract, preparation parity | collection, Option/Result, math, string, host-I/O runtime helper or target-library function symbols, and the native Slot/SecureSlot/DeviceSlot MIR resource runtime-call table are projected into a stable `runtime_call_abi` artifact, with C/LLVM tool-output parity when LLVM is available. The native MIR ABI table also exposes row-count/domain/type/operation/symbol/target-kind/materialization/call-shape accessors plus `MIRResourceRuntimeRow` lookup APIs, and `test-mir` verifies representative rows across slot, secure slot, pin/unpin, device slot, submit-read, and constructed-resource lanes. `test-mir` also compares the committed self-host `runtime_call_abi` artifact's full `native-resource` range against the native row API, including call-shape facts, so the self-host projection and native MIR row table cannot drift silently. The component contract rejects direct quoted helper spellings under self-host codegen participant directories, so new self-host call names must pass through the domain runtime ABI owners first; native resource helper spellings now appear as `native-resource` rows with `mir_abi_resource_row` materialization and MIR-owned call shapes. C MIR resource-op emission, C MIR pin enter/exit emission, source-level C pin block cleanup attributes, source statement auto-release, source slot builtins, C expression slot runtime rows, C let-slot claim/sugar emission, LLVM slot auto-read, and LLVM slot/device builtin emission now consume row records and validate call shapes. This closes the runnable manifest gate for current self-host runtime call spelling and one more executable native resource runtime-call ABI projection seam, but remaining C/LLVM compiler backend callsites still need to consume the same concrete row table for the full production ABI blocker. |
| Basic nominal-record arrays | LLVM array registry `elem_name` facts plus raw record-array runtime exports | `backend_compare/record_array_basic` through C and LLVM | `Array<NominalRecord>` can be created, passed as a parameter, pushed, set, popped, indexed, and used for member access without reopening AST type guessing |
| DRV-0 artifact gate | `src/self_hosted/compiler/driver_rung0_owner.pgy`, `src/self_hosted/compiler/driver_rung0_main.pgy` | `self-host-driver-rung0-parity-test-smoke` | source path -> self-parser AST text -> self-codegen emitted C is assembled in one Pergyra owner boundary and compared against `pgy --ast` plus the current codegen oracle |
| Artifact Zone evidence | `src/self_hosted/compiler/artifact_zone_owner.pgy`, `ArtifactZone` | `self-host-component-contract-test-smoke`, parity artifact gates | Comparable diagnostics, LSP, AST text, AIR/MIR JSON, ABI/layout, runtime-call ABI, materialization, emitted C/LLVM/self-hosted, and run-output artifacts now route equality verdicts through `backend_output_comparator` with explicit artifact kinds. Consumer parity scripts must not recompute artifact equality in shell; the comparator's own parity rung is limited to expected-JSON bootstrap comparison plus mismatch and missing-input fixtures. |

## Active Blockers

These are the surfaces to bring in before claiming broader hard self-hosting.
They are not optional polish; each one prevents a common fallback shape.

| Blocker | Required owner | Why it matters |
|---|---|---|
| Mixed AST-like tree owner | Pergyra record/class/tagged-node owner plus traversal parity | The AST-text bridge now has typed line nodes with parent edges, coarse kind rows, and function/return/role/nominal/enum-name/enum-variant/field/parameter/statement payload rows. `ast_text_row_fact_owner.pgy` derives name/type/value/aux-value/mode rows from `CodegenAstTextRowFactInput` once during inventory construction, and `ast_text_typed_arena_owner.pgy` now projects those rows into arena atom/type/value/aux-value/mode facts. Function/declaration emission consumes those arena rows for names, return types, parameter names/types/modes, role target types, field names/types, and payload-free enum variant lists. `program_emit`, `function_emit`, and `stmt_emit` route emission traversal depth through typed arena indent/parent facts; program-level declaration routing, Main counting, event rejection, top-level function selection, declaration collector prepass routing, Parameters/Returns/Fields marker routing, statement-kind routing, and Body/Block/Then marker expectations consume typed arena kind/atom predicates; runtime/header usage facts consume typed arena type/kind facts and `CodegenExpressionUsageFacts` rows through `ast_usage_owner.pgy` rather than raw `CodegenAstTextNode` payload/kind scans, whole-AST scans, or line-text rescans. `stmt_emit` now consumes owner-owned statement facts instead of splitting AST lines locally; declaration facts via `ast_text_declaration_owner.pgy`, function signature facts via `ast_text_function_signature_owner.pgy`, `Let` name/type/initializer via `ast_text_local_binding_owner.pgy`, `Let` array-literal initializer via `ast_text_array_literal_owner.pgy`, `Let` try-initializer shape via `ast_text_try_let_owner.pgy`, payload-free enum variant payloads via `ast_text_enum_variant_owner.pgy`, `Assign` target/RHS via `ast_text_assignment_owner.pgy`, `ArrayPush`/`ArraySet` payloads via `ast_text_collection_stmt_owner.pgy`, `For` loop-var/start/end/collection via `ast_text_for_stmt_owner.pgy`, and single-payload statement facts via `ast_text_statement_payload_owner.pgy` consume typed arena rows in emission. Parameter mode facts survive into codegen. Emission owners are now ratcheted against direct `CodegenAstTextNode.text` access, function/statement raw indent reads, targeted function/declaration `CodegenAstText*Name/Type/Mode` accessors, targeted enum-variant accessors, targeted program/function declaration predicates, targeted function marker predicates, targeted marker expectation calls, targeted declaration payload accessors, targeted single-payload statement accessors, targeted function-signature payload accessors, targeted `Let`/`Assign`/`ArrayPush`/`ArraySet`/`For` payload accessors, repeated try-initializer payload reads, repeated array-literal initializer reads, repeated payload-free enum variant payload reads, repeated `For` range-end payload reads, and raw-node usage-fact bridges. Runtime/header builtin callee vocabulary and expression token matching now live in `ast_expression_usage_owner.pgy`; the blocker remains active because that owner still derives expression usage from transitional arena atom/value/aux text until owned typed/tagged expression rows replace line-text semantics. |
| Stable JSON parse/emit owner | schema-aware JSON reader/writer with diagnostics | Read primitives plus string/field/object/array emission are shared, all current self-hosted report schemas consume the object/array writer through the direct `json_emit.pgy` owner import, AIR/module validators consume schema or top-level field checks through owner-level fact tables rather than document-local helpers, AIR graph validator document-root schema equality consumes `JsonDocumentFactStringFieldEquals`, document-root required keys consume `JsonDocumentObjectFactTable` / `JsonObjectFactHasField`, and root `summary` count rows now consume `JsonObjectFactObjectTable` plus `JsonObjectFactNumberFieldOpt` instead of carrying raw summary bounds into the AIR scanner, AIR graph feature requirements (`compression_budget`, `compression_reason`, `execution_lane`, `boundary_capture`) are graph-wide scalar facts consumed through `AirGraphScalarFieldValues`, AIR graph live consumers share `AirGraphSummaryIntField` for summary count rows, AIR graph id/ref/reachability consumers share `AirGraphScalarFieldValues` for scalar graph facts instead of owning local `"id"`/`"from"`/`"to"`/`"root"` token scanners, `module_manifest_resolver` now consumes the root `modules` array and module-row count/field/equality facts through `JsonObjectFactTable` and `JsonArrayObjectFactTable` boundary facts so nested `"modules"` text cannot satisfy the root contract and resolver-local row scans cannot drift, and `mir_lower` schema validation consumes `MirDocumentSchemaEquals` from `json_fact_read.pgy` while declaration/routine root-array discovery/header/body-boundary/program assembly/routine CFG block/successor/instruction/source-local/statement-array/match-pattern lowering consumes MIR row/object/string/array facts through `json_fact_read.pgy`, `JsonArrayObjectFactTable`, and `routine_inventory_owner.pgy` instead of local schema substring, root `decls`/`routines` array scans, field-key, global name, `"blocks"` key, block marker, instruction kind, successor key, or suffix scans. Object string-field and number-field absence now have `Option<String>` fact APIs (`JsonObjectStringFieldOpt`, `JsonObjectNumberFieldOpt`, `JsonObjectFactStringFieldEquals`, `JsonDocumentFactStringFieldEquals`, `JsonObjectFactNumberFieldOpt`, `MirDocumentSchemaEquals`, `MirObjectStringFactOpt`, `MirObjectNumberFactOpt`), MIR fact graph and AIR graph summary/schema consumers use those facts instead of empty-string sentinels or document-local schema reads, and JSON emission has a separate `json_emit.pgy` owner instead of a transitive `json.pgy` import. This blocker remains active because most consumers still rely on bounded scan helpers rather than a complete shared JSON DOM/fact table. |
| Subprocess runner | `src/self_hosted/compiler/subprocess_runner_owner.pgy` | The capability envelope now names executable path, argv, cwd, env allowlist, timeout, stdout/stderr, and exit code facts. `CompilerSubprocessOracleComparePlanReady()` fixes the exact fact/use-case schema through named envelope fact accessors and named use-case facts for `oracle_compare`, `fixture_build`, and `artifact_probe`, and `CompilerSubprocessFactKnown()` / `CompilerSubprocessUseCaseKnown()` guard the row vocabulary before subprocess evidence is consumed. Oracle timeout is now a numeric owner fact projected to the report string, and the env allowlist is a count/index/known row set projected to CSV only at the report boundary. `backend_output_comparator` now records the oracle-compare use case, stream fact, exit fact, and nested `pgy.selfhost.subprocess-plan.v1` JSON through named owner functions instead of positional fact indexes. It remains active until a Pergyra runner executes against that envelope instead of shell-only logic. |
| Target capability envelope (native/global consumers) | `target_capability_owner.pgy`, `TargetCapabilityZone` | The current self-host C subset consumes the envelope, but native C/LLVM target-specific consumers still need to read the same envelope before the global surface is complete. AIR-overlapping target fact names remain target-envelope facts until the import/idempotence contract is checked and the target owner can consume `air_evidence_owner.pgy` directly. |
| Compatibility evolution envelope | `compatibility_evolution_owner.pgy`, `CompatibilityEvolutionZone` | Source, ABI/binary, behavior, diagnostic, AIR evidence, MIR JSON, runtime trace, capability profile, and stdlib module compatibility surfaces are now named self-host compiler facts. Obsolete migration metadata is also fixed as owner facts (`diagnostic_id`, replacement, migration URL, warning/error/remove versions, and codefix status), the warning/error/remove version ladder plus diagnostic-id, migration-URL, change-kind, row delimiter/count/index construction, corpus report schemas, count-field names, finding-kind vocabulary, and invalid-codefix self-test status are named owner facts, and the manifest now emits a seed breaking-change corpus row for every compatibility surface through the same owner. `compatibility_evolution_checker` is the first self-hosted consumer over that corpus: it reads the owner rows through the TestHarness manifest and fails if the seed corpus loses any compatibility surface, exact diagnostic-id owner policy, exact owner version ladder, exact migration-URL owner policy, exact change-kind owner policy, exact row-shape owner policy, exact codefix-status field, or obsolete migration envelope. Its negative artifacts now reject malformed change rows, invalid codefix status, invalid diagnostic id, invalid migration URL, invalid change kind, invalid obsolete migration envelope, and a corpus with every compatibility surface missing; those negative artifacts are checked through C-built and LLVM-built self-host tools when LLVM is available. This remains active until diagnostics, stable-subset, package, runtime trace, and native C/LLVM/self-host production gates consume these rows instead of carrying local compatibility lists. |
| Symbol/mangle owner | `src/self_hosted/compiler/symbol_table_owner.pgy` | The self-host C subset now consumes the compiler-world spelling row owner directly and fail-closes unless the exact row envelope is ready. Source owner/name, namespace path, C/LLVM/self-host symbol rows, and collision policy are named owner facts, and `CompilerSymbolTableReady()` consumes those facts instead of comparing row/projection indexes to string literals. This remains active until native C, LLVM, and self-hosted projections all consume the same concrete row table. |
| Cross-backend ABI/layout row projection | `src/self_hosted/compiler/abi_layout_row_owner.pgy`, `src/self_hosted/compiler/abi_layout_target_policy_owner.pgy`, `src/self_hosted/compiler/abi_layout_row_manifest.pgy`, plus current C-subset `abi_layout_owner.pgy` | The compiler-world owner now carries concrete C ABI row projections for the supported self-host subset (`Int`, `Long`, `Double`, `Bool`, `Float`, `String`, `Array<Int>`, `Array<Long>`, `Array<String>`, `Array<CodegenAstTextNode>`, `Result<Int>`, `Option<Int>`, `Option<Long>`, `Option<Float>`, `Option<Double>`, `Option<String>`), while the target-policy owner carries `selfhost-c` projection and fallback rows. `Array<Long>` and `Option<Long>` intentionally project to the `selfhost-c` `long long` lane, while `Option<Float>` and `Option<Double>` have explicit scalar option typedef/helper rows in the runtime ABI owner. Fact columns, canonical type spellings, compatibility type spellings, C value type spellings, field order, tag kind, niche, ownership shape, target ABI, target size/align policy, materialization lanes, and bare-return default values are named owner facts. `CompilerAbiLayoutRowsReady()` fixes the nine fact columns and pins the concrete-row envelope count plus representative field-order/tag/niche/ownership/target/size-align/materialization/default-return lanes through those facts, then consumes `abi_layout_target_policy_owner.pgy` so `selfhost-c` is tied to the accepted `cpu-c,self-hosted` projection set, required `layout_shape,materialization_reason` facts, and fallback reasons. The C subset cannot claim readiness with only the fact names present or reintroduce row-index literal comparisons. The same owner now also owns the `Void` return spelling, payload-free enum storage/default spelling, scalar bare-return defaults consumed by `abi_layout_owner.pgy`, and the first native/backend ABI layout contract rows consumed by `backend_abi_layout_contract_checker`. Statement emission consumes these ABI facts for match temporary storage/casts and bare returns instead of spelling C integer/storage/default facts directly. The runnable `abi_layout_row_manifest.pgy` projection emits the owner rows and target-policy rows as an `abi_layout` artifact, and `self-host-abi-layout-row-parity-test-smoke` compares it through the Pergyra backend-output comparator plus the C/LLVM tool-output leg when LLVM is available. Exact per-target size/align computation remains out of this C-subset slice and is represented honestly as the `target-c-default` policy fact. The self-host C ABI owner consumes those rows first and only consults `TypeEnvZone` for user structs. This remains active until native C, LLVM, and self-hosted backend consumers read the same concrete row table. |
| AIR evidence zone | `src/self_hosted/compiler/air_evidence_owner.pgy`, `AirEvidenceZone` | `PgyCompilerWorld` now owns the hard-rung evidence vocabulary for intent/effect/authority/coordination/slot/materialization/loss. Each evidence row is a named owner fact, and `CompilerAirEvidenceEnvelopeReady()` fixes the exact seven-fact order by consuming those facts instead of comparing row indexes to string literals. The AIR graph JSON validator run boundary now consumes that envelope before reading AIR fixtures. It remains active until hard rungs consume live AIR evidence rows rather than only the vocabulary envelope. |
| Test harness substrate | `src/self_hosted/compiler/test_harness_owner.pgy`, `src/self_hosted/compiler/test_harness_tool_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_driver_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_parser_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_mir_json_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy`, `TestHarnessZone` | Fixture and result row vocabulary is now Pergyra-owned. Source, diagnostic, AIR JSON, MIR JSON, ABI layout, stdout, exit, projection, C/LLVM/self-hosted projection names, and comparable fixture paths are named owner facts, and `CompilerTestHarnessReady()` consumes those facts instead of comparing row/projection indexes to string literals. `backend_output_comparator` records C/LLVM/self-hosted projection rows, comparable artifact paths, and finding caps by consuming this owner. `test_harness_tool_paths_owner.pgy` owns concrete parity tool/input path suites so the core TestHarness owner stays focused on row/projection/artifact vocabulary; `test_harness_driver_paths_owner.pgy` owns DRV-0/DRV-1 driver/parser/codegen source path suites separately; `test_harness_codegen_paths_owner.pgy` owns codegen parity tool/input directories separately; `test_harness_parser_paths_owner.pgy` owns parser parity tool/comparator/fixture/expected paths separately; `test_harness_semantic_paths_owner.pgy` owns semantic parity tool/comparator/fixture/expected/diagnostic-owner paths separately; `test_harness_mir_json_paths_owner.pgy` owns MIR JSON parity mir-lower/codegen/comparator input paths separately and reuses the codegen path owner facts for codegen/comparator paths so those strings do not fork; `test_harness_codegen_bootstrap_paths_owner.pgy` owns codegen bootstrap tool paths, the fuzz backend generator path suite, plus component/tool breadth rows so bootstrap and fuzz-generator runners do not synthesize self-host source paths in shell; `test_harness_lsp_paths_owner.pgy` owns LSP diagnostics, transport, request, response, session, document, state, and hover path suites so those scripts execute manifest rows instead of owning LSP source/expected path constants. `backend_output_tri_compare_parity.sh` now gets its smoke/extended backend case suites from the Pergyra `test_harness_manifest.pgy` projection over `TestHarnessZone` instead of owning the case arrays in shell. `linter_parity.sh` now gets its tool source, expected diagnostics, and fixture path from the same manifest and passes the fixture path into the compiled linter through `Args()[0]`. `module_manifest_resolver_parity.sh` now gets its tool source, expected JSON, and input manifest path from the manifest and passes the manifest path into the compiled resolver through `Args()[0]`. `stable_subset_section_checker_parity.sh` now gets its tool source, expected JSON, and input manifest path from the manifest and passes the manifest path into the compiled checker through `Args()[0]`. `examples_inventory_checker_parity.sh` now gets its tool source and expected JSON from the manifest before running the C/LLVM-built checker. It accepts artifact paths and projection row indexes through `Args()`, and `backend_output_tri_compare_parity.sh`, `linter_parity.sh`, `module_manifest_resolver_parity.sh`, `stable_subset_section_checker_parity.sh`, `examples_inventory_checker_parity.sh`, `llvm_leg_helpers.sh`, `codegen_parity.sh`, `parser_parity.sh`, `semantic_parity.sh`, `mir_json_parity.sh`, `codegen_bootstrap.sh`, `fuzz_backend_parity_generator_parity.sh`, `lexer_parity.sh`, the LSP parity scripts, plus AIR graph validator clean JSON parity now route verdicts through that Pergyra owner. It remains active until the remaining shell parity scripts are projections of these records instead of the primary harness owner. |

TypedAst delta, 2026-07-07: `typed_ast_node_skeleton.pgy` no longer uses a
single placeholder `nodes: Array<Int>` row. The owner now carries parallel
typed node facts (`kind`, `atom`, `has_atom`, child span, child edges, and atom
table), exposes `NodeId` lookup through `Option`-returning accessors, and proves
a Program -> FuncDecl(Main) -> Block traversal fixture. The component contract
ratchets those row facts and rejects the old placeholder. The matching
self-host C-emitter slice now permits struct fields that carry named
`Array<Int>` / `Array<String>` values, and member-array indexing consumes field
type facts before lowering through the collection runtime owner. The blocker
remains ACTIVE because codegen still receives `pgy --ast` line text through the
bridge; this slice only makes the replacement arena concrete enough for later
parser and codegen cutovers.

TypedAst delta, 2026-07-07: the AST-text bridge now projects the real
`CodegenAstTextNode` inventory into an `AstArena` with row-aligned kind, atom,
parent, indent, and child-edge facts. `CodegenTypedAstBridgeReady(...)` no
longer proves only the standalone fixture: it builds
`CodegenAstTextTypedArenaFromNodes(...)`, checks node count and per-row
kind/atom/parent/indent lookup through the typed arena accessors, and verifies
that a non-empty program has a root child edge. This keeps the mixed AST blocker
ACTIVE, but it makes the bridge's typed arena projection load-bearing on every
self-host C emission path.

TypedAst delta, 2026-07-07: `program_emit.pgy` now consumes the typed arena
projection for a real traversal decision. First-function indent, zero-artifact
descendant skipping, nominal method scanning, and role method scanning use
`CodegenAstArenaIndentOrDie(...)` and
`CodegenAstArenaIsDescendantOf(...)` from
`ast_text_typed_arena_owner.pgy` instead of direct raw `nodes[i].indent`
comparisons. The blocker remains ACTIVE because function/statement emission
still consumes transitional text-node rows.

TypedAst delta, 2026-07-07: `function_emit.pgy` and `stmt_emit.pgy` now consume
the same typed arena projection for emission-depth traversal. Function
parameter scans, owner-depth tracking, role operator scans, struct field scans,
prototype scans, statement-list depth checks, else-if nested depth, and match
case scans read `CodegenAstArenaIndentOrDie(...)` or
`CodegenAstArenaIsDescendantOf(...)` instead of direct raw
`CodegenAstTextNode.indent` reads. The blocker remains ACTIVE because payload
semantics still come from the transitional AST-text bridge.

TypedAst delta, 2026-07-07: `program_emit.pgy` is the single codegen owner that
builds the AST-text-to-typed-arena projection for emission. Function and
statement emitters receive `arena: AstArena` from that owner and are ratcheted
against rebuilding `CodegenAstTextTypedArenaFromNodes(nodes, count)` locally.
This keeps traversal facts single-projected even while the transitional
AST-text payload bridge remains active.

TypedAst delta, 2026-07-07: `AstArena` now carries type-name, value, and mode
rows in addition to kind, atom, parent, indent, and child-edge rows. Function and
declaration emission consumes arena atom/type/mode accessors for names, return
types, parameter signatures, role target types, enum names, and field rows. The
blocker remains ACTIVE because some expression and statement payload strings
still flow through transitional AST-text facts.

TypedAst delta, 2026-07-07: `EmitLet(...)`, `EmitTryLet(...)`, and
`EmitAssign(...)` now consume typed arena atom/type-name/value rows for `Let`
name/type/initializer and `Assign` target/RHS. The component contract rejects the
old targeted `CodegenAstTextLetInitializer(...)` and `CodegenAstTextAssign*`
payload accessors in `stmt_emit.pgy`.

TypedAst delta, 2026-07-09: local binding and assignment statement facts now
have dedicated owners. `ast_text_local_binding_owner.pgy` owns `Let` binding
name/type/initializer facts, and `ast_text_assignment_owner.pgy` owns `Assign`
target/RHS facts. `stmt_emit.pgy` consumes those accessors and is ratcheted
against reopening direct arena atom/type/value payload reads for those statement
forms.

TypedAst delta, 2026-07-09: function signature facts now have a dedicated
owner. `ast_text_function_signature_owner.pgy` owns function names, parameter
mode/name/type rows, and return type rows for function emission, global function
env construction, role-operator method lookup, and prototype emission.
`function_emit.pgy` consumes those accessors and is ratcheted against reopening
direct arena atom/mode/type reads for function-signature payloads.

TypedAst delta, 2026-07-09: declaration facts now have a dedicated owner.
`ast_text_declaration_owner.pgy` owns nominal names, role names/target types,
enum names, and field name/type rows. `function_emit.pgy` and `program_emit.pgy`
consume those accessors and are ratcheted against reopening direct arena
atom/type reads for declaration payloads.

TypedAst delta, 2026-07-07: `ArraySet` and `For` statement emission now consume
typed arena atom/value/aux-value rows. `ArraySet` maps receiver/index/value;
`For` maps loop variable plus range start/end or foreach collection.

TypedAst delta, 2026-07-07: single-payload statement emission moved off the
old targeted `CodegenAstText*` payload accessors in `stmt_emit.pgy`. Statement
emission payloads now route through typed arena rows; the mixed-tree blocker
remains active because the parser-to-codegen bridge still carries line text.

TypedAst delta, 2026-07-07: `program_emit.pgy` now consumes typed arena
kind/atom predicates for program-level declaration routing, Main counting, event
rejection, and top-level function selection. The component contract rejects the
old `CodegenAstTextIs*` declaration predicates in `program_emit.pgy`.

TypedAst delta, 2026-07-07: `function_emit.pgy` declaration collector
prepasses now consume the same typed arena declaration predicates for global
env/prototype/struct/enum/role-operator scans. The component contract rejects
the old declaration predicates in `function_emit.pgy`.

TypedAst delta, 2026-07-07: `function_emit.pgy` now consumes typed arena
Parameters/Returns/Fields marker predicates, and `CodegenTypedAstBridgeReady`
checks the Program root through the same typed arena kind fact. The component
contract rejects the old marker predicates at those consumption points.

TypedAst delta, 2026-07-07: `stmt_emit.pgy` now consumes typed arena
statement-kind predicates for statement dispatch, else-if routing, and match
case/default routing. The component contract rejects the old statement-kind
predicates in `stmt_emit.pgy`.

TypedAst delta, 2026-07-07: `function_emit.pgy` and `stmt_emit.pgy` now consume
typed arena marker expectations for Parameters/Body/Block/Then structural
checks. The component contract rejects `CodegenAstTextExpectNode(...)` in
emission participants.

TypedAst delta, 2026-07-07: runtime/header usage facts now consume the
already-built `AstArena` projection through `CodegenRuntimeUsageFactsFromArena`.
The old raw-node usage bridge is deleted, and `ast_usage_owner.pgy` is ratcheted
against direct `nodes[i].payload` / `nodes[i].aux_payload` scans. Builtin-name
detection still scanned arena atom/type/value/aux rows directly at this point.

TypedAst delta, 2026-07-07: runtime/header usage facts now split by arena lane.
Type/header requirements consume typed arena `type_name` rows, builtin-call
requirements consume only expression-bearing atom/value/aux rows through
string-literal-aware call matching, `None` uses a token-boundary expression
scan, and statement-only needs remain kind facts. The component contract rejects
the old generic `CodegenAstArenaContains(...)` whole-arena usage predicate.

TypedAst delta, 2026-07-09: runtime/header builtin callee vocabulary now lives
behind `CodegenUsageBuiltinGroup*` owner rows. `CodegenRuntimeUsageFactsFromArena`
consumes those rows through `CodegenAstArenaBuiltinGroupPresent(...)` instead of
spelling every builtin call in the runtime usage fact body. This reduces the
mixed AST-like tree blocker but does not close it: the matching engine still
checks transitional expression text until dedicated expression usage rows exist.

TypedAst delta, 2026-07-09: expression usage has a dedicated owner. The builtin
callee groups and `None` token scan moved to `ast_expression_usage_owner.pgy`,
which produces `CodegenExpressionUsageFacts`. `ast_usage_owner.pgy` now consumes
that fact row plus type/kind facts, and the component contract rejects reopening
payload scans or direct builtin group scans inside the runtime/header usage
owner. This is still transitional because the expression usage owner reads arena
atom/value/aux text until typed expression rows exist.

TypedAst delta, 2026-07-09: expression usage lane selection now has a single
fact seam. `ast_expression_usage_owner.pgy` builds a `CodegenExpressionParts`
row for each typed arena node, with explicit presence bits plus text for the
current self-host C ABI subset; token and builtin-call scans consume that row
instead of each reopening atom/value/aux lane reads. The component contract
rejects the old `TypedAstArena*Text(arena, i)` scan shape. This still does not
close the blocker because `CodegenExpressionParts` is backed by transitional
arena text until typed/tagged expression rows replace the bridge.

TypedAst delta, 2026-07-09: try-let initializer recognition now has a single
fact seam. `ast_text_try_let_owner.pgy` builds `CodegenLetTryInitializerFact`
once for a `Let` row, and try-presence plus inner-expression extraction consume
that fact through an `Option<String>` view instead of reopening the initializer
payload independently. The component contract fixes the owner/fact names and
rejects repeated direct initializer payload reads. This still does not close
the blocker because the fact remains backed by the transitional arena value row
until typed statement payload rows replace the bridge.

TypedAst delta, 2026-07-09: array-literal initializer recognition now has a
single fact seam. `ast_text_array_literal_owner.pgy` builds
`CodegenLetArrayLiteralFact` once for a `Let` row, and starts-with-array plus
literal-body extraction consume an `Option<String>` view of that fact instead
of reopening the initializer through the generic arena value accessor. The
component contract fixes the fact owner and rejects repeated direct initializer
payload reads.

TypedAst delta, 2026-07-09: payload-free enum variant payload recognition now
has a single fact seam. `ast_text_enum_variant_owner.pgy` builds
`CodegenEnumVariantPayloadFact` once for an enum row, and count/name accessors
consume an `Option<String>` view of that fact rather than treating missing
payload as an owner-local empty-string fact. The component contract fixes the
fact owner and rejects multiple direct enum-variant payload reads.

TypedAst delta, 2026-07-09: `For` statement payload facts now have a dedicated
owner. `ast_text_for_stmt_owner.pgy` owns loop variable, range-vs-foreach
classification, range start/end, and foreach collection facts. `stmt_emit.pgy`
consumes those accessors and is ratcheted against reopening direct
`TypedAstArenaAuxValueText(arena, idx)` reads for `For` lowering.

TypedAst delta, 2026-07-09: `For` range-vs-foreach classification now has a
single range-end fact seam. `ast_text_for_stmt_owner.pgy` builds
`CodegenForRangeEndFact` once for a `For` row, and range classification plus
range-end extraction consume an `Option<String>` view of that fact instead of
each reopening the auxiliary payload. The component contract now fixes the fact
owner and rejects multiple direct range-end payload reads.

TypedAst delta, 2026-07-09: single-payload statement facts now have a dedicated
owner. `ast_text_statement_payload_owner.pgy` owns `Log`, value `Return`,
`ArrayPop`, `Exit`, `While`, `If`, `Match`, match case, and bare-call payload
facts. `stmt_emit.pgy` consumes those accessors and is ratcheted against
reopening the direct `CodegenAstArenaAtomOrDie(arena, idx)` payload reads for
those statement forms.

TypedAst delta, 2026-07-09: collection mutation statement facts now have a
dedicated owner. `ast_text_collection_stmt_owner.pgy` owns `ArraySet`
target/index/value and `ArrayPush` target/value facts. `stmt_emit.pgy` consumes
those accessors and is ratcheted against reopening direct arena atom/value/aux
payload reads for those statement forms.

TestHarness delta, 2026-07-05: `test_harness_air_graph_paths_owner.pgy`
now owns the five AIR graph consumer path suites separately from the generic
tool-path owner. `air_graph_id_uniqueness_parity.sh`,
`air_graph_node_count_integrity_parity.sh`,
`air_graph_reachability_parity.sh`, `air_graph_ref_integrity_parity.sh`, and
`air_graph_ref_live_parity.sh` read tool source, shared scan owner, expected
JSON, and fixture paths from `test_harness_manifest.pgy`; the compiled
checkers receive the selected fixture path through `Args()[0]`.

TestHarness delta, 2026-07-06: `air_graph_node_count_integrity_parity.sh`
now gets the corrupted negative fixture path, summary field name, and corrupt
summary value from `test_harness_air_graph_paths_owner.pgy`. Shell remains the
scratch file writer, but no longer owns which AIR summary fact is corrupted to
prove the fail-closed `node_count_mismatch` path.

TestHarness delta, 2026-07-06: `air_graph_ref_live_parity.sh` now gets the
corrupted negative fixture path, reference field name, source value, and corrupt
target value from `test_harness_air_graph_paths_owner.pgy`. Shell remains the
scratch file writer, but no longer owns which live AIR back-reference is
corrupted to prove the fail-closed `dangling_reference` path.

TestHarness delta, 2026-07-06: `air_graph_json_validator_parity.sh` now gets
the missing top-level key name and expected missing-key JSON artifact from
`test_harness_tool_paths_owner.pgy`. Shell remains the scratch file writer, but
no longer owns which AIR graph root key is stripped, which finding kind appears,
or which counter proves the fail-closed missing-key path.

TestHarness delta, 2026-07-06: `stable_subset_section_checker_parity.sh` now
gets the missing-section anchor and expected missing-section JSON artifact from
`test_harness_tool_paths_owner.pgy`. Shell still strips the TestHarness-owned
anchor from a scratch document, but no longer owns the `"missing":1` diagnostic
shape or any other negative verdict field.

TestHarness delta, 2026-07-06: `runtime_boundary_checker_parity.sh` now gets
the missing-term fixture pair and expected missing-term JSON artifact from
`test_harness_tool_paths_owner.pgy`. Shell still strips the TestHarness-owned
runtime term from a scratch document, but no longer owns the `"missing":1`
diagnostic shape or any other negative verdict field.

TestHarness delta, 2026-07-06: the five AIR graph consumer parity runners now
compile and run their manifest-projected checker sources in place. They no
longer create build-dir `main.pgy` aliases or copy `scan_owner.pgy` and the
self-hosted `lib` tree beside those aliases before invoking the compiler.

TestHarness delta, 2026-07-06: the five AIR graph consumer parity runners now
get their expected negative verdict artifacts from
`test_harness_air_graph_paths_owner.pgy`. Shell still executes the negative
fixture path or scratch mutation, but no longer owns `ok:false` or finding-kind
interpretation for duplicate-id, node-count, reachability, edge-reference, or
live-reference fail-closed behavior.

ArtifactZone delta, 2026-07-06: `air_graph_id_uniqueness_parity.sh` no longer
recomputes the clean duplicate-id count with shell `grep`/`sort`/`uniq`.
The clean output oracle is the TestHarness-projected `expected/clean.json`, and
the duplicate negative verdict is the TestHarness-projected
`expected/duplicate.json`; both are compared through `backend_output_comparator`.
Shell remains only the process runner and duplicate-fixture `rc=1` checker.

AIR ID uniqueness delta, 2026-07-08: `report_owner.pgy` now owns
`pgy.selfhost.air-id-uniqueness.v1`, source/count row names, duplicate-ID
findings, input-error findings, and final report shape. `main.pgy` consumes
scanner facts and performs duplicate analysis only.

ArtifactZone delta, 2026-07-06: `air_graph_node_count_integrity_parity.sh` no
longer interprets the corrupted-summary verdict with shell `ok:false` or
finding-kind greps. The clean output oracle is the TestHarness-projected
`expected/clean.json`, and the corrupted-summary negative verdict is the
TestHarness-projected `expected/corrupt_summary.json`; both are compared
through `backend_output_comparator`. Shell remains only the process runner,
scratch mutator, and corrupted-fixture `rc=1` checker.

ArtifactZone delta, 2026-07-06: `air_graph_reachability_parity.sh` no longer
recomputes the clean node count with shell `grep`/`wc`. The clean output oracle
is the TestHarness-projected `expected/clean.json`, and the orphan negative
verdict is the TestHarness-projected `expected/orphan.json`; both are compared
through `backend_output_comparator`. Shell remains only the process runner and
orphan-fixture `rc=1` checker.

ArtifactZone delta, 2026-07-06: `air_graph_ref_integrity_parity.sh` no longer
recomputes clean dangling endpoint counts with shell `grep`/`comm`. The clean
output oracle is the TestHarness-projected `expected/clean.json`, and the
dangling-endpoint negative verdict is the TestHarness-projected
`expected/dangling.json`; both are compared through `backend_output_comparator`.
Shell remains only the process runner and dangling-fixture `rc=1` checker.

ArtifactZone delta, 2026-07-06: `air_graph_ref_live_parity.sh` no longer
interprets the corrupted-reference verdict with shell `ok:false` or
finding-kind greps. The clean output oracle is the TestHarness-projected
`expected/clean.json`, and the corrupted-reference negative verdict is the
TestHarness-projected `expected/corrupt_reference.json`; both are compared
through `backend_output_comparator`. Shell remains only the process runner,
scratch mutator, and corrupted-fixture `rc=1` checker.

TestHarness delta, 2026-07-05: backend_output_comparator_parity.sh now consumes its source, expected JSON, and comparable artifact paths from TestHarness through the `backend-output-comparator-paths` manifest suite. Shell no longer owns the comparator input path constants.

TestHarness delta, 2026-07-08: the `backend-output-comparator-paths` suite and
its comparable fixture paths are now owned by
`test_harness_comparator_paths_owner.pgy`. The core `test_harness_owner.pgy`
consumes `CompilerHarnessBackendOutputComparatorReady()` and the comparable
artifact facts, but no longer defines the concrete backend-output comparator
source, expected JSON, or fixture paths.

TestHarness delta, 2026-07-08: the `backend-tri-smoke` and
`backend-tri-extended` case suites are now owned by
`test_harness_backend_compare_paths_owner.pgy`. The core TestHarness owner
still requires `CompilerHarnessBackendTriSuiteReady()`, but it no longer owns
the concrete backend-compare fixture case inventory.

TestHarness delta, 2026-07-06: `backend_output_comparator_parity.sh` now gets
the expected mismatch and missing-input verdict artifacts from
`test_harness_owner.pgy`. Shell still creates the negative artifact fixtures and
does direct byte comparison because this is the comparator's own self-test, but
it no longer owns which comparator finding kind, `ok:false` flag, or mismatch
counter proves the mismatch or missing-input fail-closed paths.

ArtifactZone delta, 2026-07-06: `backend_output_comparator_parity.sh` no longer
performs a separate shell text-equivalence check over the clean fixture pair.
The comparator owns artifact equality; its own parity rung keeps only the
expected-JSON bootstrap comparison and the committed mismatch/missing-input
verdict artifacts.

SubprocessRunner delta, 2026-07-06: `backend_output_comparator` now embeds the
`pgy.selfhost.subprocess-plan.v1` plan emitted by
`CompilerSubprocessOracleComparePlanJson(...)`. Shell still launches processes,
so the blocker remains ACTIVE, but executable path, argv, cwd, env allowlist,
timeout, stream, and exit-code facts are now a structured Pergyra-owned plan
instead of free fields owned by the parity runner.

TestHarness delta, 2026-07-06: `backend_output_comparator_parity.sh` now gets
the argv-mode expected verdict artifact from `test_harness_owner.pgy`. Shell
still passes explicit artifact/projection arguments into the compiled
comparator, but it compares the full argv verdict against
`expected/arg_self_hosted.json` instead of grepping projection rows or
`ok:true`.

TestHarness delta, 2026-07-06: `backend_output_comparator_parity.sh` now
compiles and runs the manifest-projected comparator source in place. It no
longer creates a build-dir `main.pgy` alias or copies the self-hosted `lib` and
`compiler` owner tree beside that alias before invoking the compiler.

ABI layout delta, 2026-07-08: `abi_layout_row_manifest.pgy` now projects the
compiler-world ABI row owner into a runnable `abi_layout` artifact. The
`abi_layout_row_manifest_parity.sh` runner obtains the manifest source and
expected artifact through TestHarness, compares the emitted rows with the
Pergyra backend-output comparator, and runs the C/LLVM tool-output equality leg
when LLVM is available.

ABI target-policy delta, 2026-07-08: the ABI row artifact schema is now
`pgy.selfhost.abi-layout-row.v3`. `abi_layout_row_owner.pgy` imports the target
capability owner and projects a `target_policy` row that binds `selfhost-c` to
the accepted `cpu-c,self-hosted` projections, the required
`layout_shape,materialization_reason` facts, and the supported fallback reasons.
This keeps target acceptance out of backend-local ABI spelling decisions.

TestHarness delta, 2026-07-08: the compatibility-evolution manifest path suite
now lives in `test_harness_tool_paths_owner.pgy` beside the compatibility
corpus checker suite. The core `test_harness_owner.pgy` still consumes
`CompilerHarnessCompatibilityEvolutionReady()` as a readiness predicate, but it
no longer owns the concrete manifest source or expected artifact paths.

Compatibility corpus delta, 2026-07-08: `compatibility_evolution_checker` now
validates the compatibility row envelope by field position, not loose substring
presence. Diagnostic IDs, version ladder entries, migration URLs, codefix
statuses, and obsolete migration envelopes are counted from canonical fields,
and the TestHarness-projected parity suite now includes an
`invalid_codefix_status` negative artifact on both C and LLVM legs when LLVM is
available.

Compatibility corpus policy delta, 2026-07-09:
`compatibility_evolution_checker` now has TestHarness-projected invalid
diagnostic-id and invalid migration-URL artifacts. C-built and LLVM-built
self-host checker legs must reject rows that keep the 11-field shape but violate
the `CompatibilityEvolutionZone` diagnostic-id prefix or migration URL prefix.

Compatibility corpus missing-surface delta, 2026-07-08:
`compatibility_evolution_checker` now has a `missing_surface` negative artifact
projected through `TestHarnessZone`. The report owner emits all nine missing
surface findings through the same compatibility owner vocabulary, and the C and
LLVM checker legs must both fail closed with that artifact when LLVM is
available.

Compatibility corpus change-kind delta, 2026-07-08:
`compatibility_evolution_checker` now counts canonical change-kind rows through
`CompatibilityEvolutionZone` instead of accepting any row spelling in the
change-kind column. The TestHarness-projected parity suite includes an
`invalid_change_kind` artifact, and the C and LLVM checker legs must both fail
closed with the owner-owned `unknown_change_kind` finding when LLVM is
available.

Compatibility corpus obsolete-migration delta, 2026-07-08:
`compatibility_evolution_checker` now has an `invalid_obsolete_migration`
negative artifact projected through `TestHarnessZone`. The artifact keeps
obsolete rows from passing with only row shape, kind, and codefix status present:
the diagnostic ID, version ladder, replacement, migration URL, and codefix
status must still form the owner-defined obsolete migration envelope, and the C
and LLVM checker legs must fail closed with
`missing_obsolete_migration_envelope` when LLVM is available.

Compatibility corpus report delta, 2026-07-08:
`compatibility_evolution_checker/report_owner.pgy` now owns the corpus report
JSON shape, count rows, negative self-test reports, and finding objects.
`main.pgy` performs compatibility-row analysis and fail-closed self-test
dispatch only.

Compatibility corpus report-count delta, 2026-07-09:
`compatibility_evolution_checker/report_owner.pgy` now consumes
`CompilerCompatibilityChangeCount()` for complete-row readiness checks instead
of restating the seed corpus cardinality locally. The component contract rejects
the old repeated-literal call shape, so compatibility row cardinality remains a
`CompatibilityEvolutionZone` fact rather than a checker/report-owner alias.

Backend emitter contract delta, 2026-07-08: the self-host backend-emitter
parity gate now runs the missing-required and forbidden-hit negative artifacts
through an LLVM-built checker as well as the C-built checker. This makes the
documented C/LLVM negative coverage real for the first dumb-emitter contract
slice; the broad native `backend-fail-closed` gate remains the production
backstop.

Backend emitter missing-input delta, 2026-07-09:
`backend_emitter_contract_checker` now has a missing-input negative artifact
projected through `TestHarnessZone`. C-built and LLVM-built checker legs fail
closed when a contract row points at an absent backend source, so shell cannot
silently shrink the scan surface by omitting a file path.

Backend emitter runtime-string parse delta, 2026-07-08: the C MIR resource
emitter no longer recovers SecureSlot/DeviceSlot identity by parsing the
`pgy_claim_*` runtime function name returned from the ABI table. The
backend-emitter contract now forbids that `strncmp(fn, "pgy_claim_secure_")`
shape and requires the backend to keep consuming MIR/ABI type facts instead.
The paired ABI-owner delta is constructed resource runtime spellings:
`mir_abi_resource_runtime_fn_by_kind` now owns runtime function spellings for
nominal payload slots such as `Slot<Vec2>` and `SecureSlot<Vec2>`, so C/LLVM
emitters can ask the ABI owner instead of reconstructing `pgy_*` names. These
constructed spellings are deliberately separate from the concrete native
resource row table exposed by `mir_abi_resource_runtime_row_*`.

Backend emitter report delta, 2026-07-08:
`backend_emitter_contract_checker/report_owner.pgy` now owns the checker report
JSON shape, count rows, finding objects, and report-owner readiness predicate.
`main.pgy` scans required/forbidden backend source terms and runs fail-closed
self-test modes for missing-required, missing-input, and forbidden-hit artifacts
only.

TestHarness delta, 2026-07-08: the ABI-layout row and runtime-call ABI row
manifest path suites now live in `test_harness_tool_paths_owner.pgy`. The core
`test_harness_owner.pgy` still consumes their readiness predicates as part of
`CompilerTestHarnessReady()`, but it no longer owns those concrete manifest
source or expected artifact paths.

Completeness delta, 2026-07-09: `completeness_ledger_owner.pgy` now locks the
M2 minima at 195 for source inventory, lexer, parser, semantic, codegen,
lex+parse, lex+parse+semantic, and full-pipeline intersection. The latest broad
parity preparation run proved the same 195/195 ledger through C and LLVM
selfcheck legs, so new production self-host sources cannot enter the inventory
without passing the full staged completeness path.

TestHarness delta, 2026-07-06: `stable_subset_section_checker_parity.sh` now
compiles and runs the manifest-projected stable-subset checker source in place.
It no longer creates a build-dir `main.pgy` alias or copies the self-hosted
`lib` tree beside that alias before invoking the compiler.

ArtifactZone delta, 2026-07-06: `stable_subset_section_checker_parity.sh` no
longer recomputes the clean section count with shell `grep`. The clean output
oracle is the TestHarness-projected `expected/clean.json`, and the
missing-section negative verdict is the TestHarness-projected
`expected/missing_section.json`; both are compared through
`backend_output_comparator`. Shell remains only the process runner, scratch
mutator, and missing-fixture `rc=1` checker.

TestHarness delta, 2026-07-06: `doc_link_checker_parity.sh` now compiles and
runs the manifest-projected doc-link checker source in place. It no longer
creates a build-dir `main.pgy` alias or copies the self-hosted `lib` tree
beside that alias before invoking the compiler.

ArtifactZone delta, 2026-07-06: `doc_link_checker_parity.sh` no longer
recomputes clean total-link or markdown-link counts with shell `grep`/`wc`.
The clean output oracle is the TestHarness-projected `expected/clean.json`,
and the dead-link negative verdict is the TestHarness-projected
`expected/dead_link.json`; both are compared through
`backend_output_comparator`. Shell remains only the process runner,
negative-fixture mutator, and `rc=1` checker for the dead-link case.

TestHarness delta, 2026-07-06: `doc_link_checker_parity.sh` now gets the
dead-link fixture's live source target and missing replacement target from
`test_harness_tool_paths_owner.pgy`. Shell still rewrites the scratch
`INDEX.md`, but no longer owns which doc-link is broken for the negative
fixture.

TestHarness delta, 2026-07-06: `doc_link_checker_parity.sh` now also gets the
expected dead-link verdict JSON artifact from `test_harness_tool_paths_owner.pgy`.
Shell still executes the negative fixture, but no longer owns the `missing_link`
diagnostic shape or drift path interpretation that proves the fail-closed path.

TestHarness delta, 2026-07-06: `examples_inventory_checker_parity.sh` now
compiles and runs the manifest-projected examples-inventory checker source in
place. It no longer creates a build-dir `main.pgy` alias or copies the
self-hosted `lib` tree beside that alias before invoking the compiler.

ArtifactZone delta, 2026-07-06: `examples_inventory_checker_parity.sh` now
gets the synthetic count-drift expected JSON artifact from
`test_harness_inventory_paths_owner.pgy` and compares the full negative verdict
through `backend_output_comparator`. Shell still creates the missing-example
fixture and checks `rc=1`, but no longer owns the `inventory_count_drift`
finding-kind interpretation or re-extracts clean count fields from the JSON.

TestHarness delta, 2026-07-06: `production_c_size_checker_parity.sh` and
`production_header_size_checker_parity.sh` now compile and run their
manifest-projected checker sources in place. They no longer create build-dir
`main.pgy` aliases or copy the self-hosted `lib` tree beside those aliases
before invoking the compiler.

TestHarness delta, 2026-07-06: `production_c_size_checker_parity.sh` and
`production_header_size_checker_parity.sh` now get their synthetic over-cap
fixture path and line-count rows from `test_harness_tool_paths_owner.pgy`.
Shell still creates the scratch files, but no longer owns which production
artifact path or LOC boundary proves the fail-closed fixture.

ArtifactZone delta, 2026-07-06: `production_c_size_checker_parity.sh` and
`production_header_size_checker_parity.sh` now get their synthetic over-cap
expected JSON artifacts from `test_harness_size_paths_owner.pgy` and compare
the full negative verdicts through `backend_output_comparator`. Shell still
creates the oversized scratch files and checks `rc=1`, but no longer owns the
`c_over_cap` / `header_over_cap` finding-kind or path interpretation.

TestHarness delta, 2026-07-06: `linter_parity.sh` and
`runtime_boundary_checker_parity.sh` now compile and run their
manifest-projected checker sources in place. They no longer create build-dir
`main.pgy` aliases before invoking the compiler.

TestHarness delta, 2026-07-06: `module_manifest_resolver_parity.sh` and
`stdlib_dispatch_inventory_checker_parity.sh` now compile and run their
manifest-projected checker sources in place. They no longer create build-dir
`main.pgy` aliases or copy the self-hosted `lib` tree beside those aliases
before invoking the compiler.

ArtifactZone delta, 2026-07-06: `stdlib_dispatch_inventory_checker_parity.sh`
now gets its synthetic count-drift expected JSON artifact from
`test_harness_inventory_paths_owner.pgy` and compares the full negative verdict
through `backend_output_comparator`. Shell still strips owner-selected LLVM
dispatch rows to construct the fixture and checks `rc=1`, but no longer owns
the `count_drift` finding-kind interpretation.

TestHarness delta, 2026-07-06: `module_manifest_resolver_parity.sh` now also
gets its missing-modules, nested-modules, and nested-field negative fixture JSON
bodies plus their expected negative verdict JSON artifacts from
`test_harness_inventory_paths_owner.pgy`. Shell writes those fixture rows into
scratch files for execution, but no longer mutates or authors the JSON fixture
semantics and no longer interprets the resulting finding kind/key.

TestHarness delta, 2026-07-06: `stable_subset_section_checker_parity.sh` now
gets its missing-section negative anchor from `test_harness_tool_paths_owner.pgy`
instead of hardcoding the `Ownership Stable Subset` line in shell. Shell still
constructs the scratch copy, but the fixture meaning is a TestHarness row.

ArtifactZone delta, 2026-07-06: `module_manifest_resolver_parity.sh` no longer
recomputes clean module, beta-blocker, or stable-subset counts with shell
`grep`. The clean output oracle is the TestHarness-projected
`expected/clean.json`, and the three negative verdicts are
TestHarness-projected expected artifacts compared through
`backend_output_comparator`; shell remains only the process runner,
negative-fixture mutator, and `rc=1` checker.

TestHarness delta, 2026-07-05: lexer_parity.sh now consumes its lexer source, backend comparator source, and lexer fixture directory from TestHarness through the `lexer-parity-paths` manifest suite. The compiled lexer owner still emits the fixture source/expected row inventory, so shell executes the parity loop without owning either the tool path constants or the fixture mapping.

TestHarness delta, 2026-07-06: `codegen_parity.sh` now consumes its codegen
tool source, parser AST producer source, backend comparator source, fixture
directory, and expected-output directory from TestHarness through the
`codegen-parity-paths` manifest suite. The compiled codegen run owner still
emits fixture source/expected rows through `--fixture-manifest`, so path
ownership and fixture inventory ownership stay separate.

TestHarness delta, 2026-07-06: `parser_parity.sh` now consumes its parser tool
source, backend comparator source, fixture directory, and expected clean
fixture path from TestHarness through the `parser-parity-paths` manifest suite.
The compiled parser owner still emits the 188-row source/fixture inventory
through `--fixture-manifest`, so the shell runner no longer owns parser path
constants while fixture ownership stays in the parser owner.

TestHarness delta, 2026-07-06: `semantic_parity.sh` now consumes its semantic
tool source, backend comparator source, fixture directory, expected diagnostic
directory, diagnostic code owner, diagnostic renderer owner, and semantic
source directory from TestHarness through the `semantic-parity-paths` manifest
suite. The compiled semantic owner still emits the 108-row fixture/status
inventory through `--fixture-manifest`, so path ownership and diagnostic
fixture ownership remain separate.

TestHarness delta, 2026-07-06: `semantic_parity.sh` now compiles and runs the
manifest-projected semantic source in place. It no longer creates a build-dir
`main.pgy` alias or copies the semantic source tree and self-hosted `lib` tree
beside that alias before invoking the compiler.

TestHarness delta, 2026-07-06: `regen_expected.sh` now uses the same
`semantic-parity-paths` manifest suite before regenerating committed semantic
diagnostics. It compiles the manifest-projected semantic source in place and
writes the manifest-projected expected directory, so fixture maintenance no
longer has a separate hardcoded source/fixture/expected path owner or copied
`lib` tree.

TestHarness delta, 2026-07-06: `lexer_scale_probe.sh` and
`parser_scale_probe.sh` now compile their manifest-projected source-owner
entrypoints in place, and they compile the backend comparator source named by
the same TestHarness path suite. They are still coverage probes rather than
parity gates, but they no longer create build-dir `main.pgy` aliases, copy
lexer/parser/lib source trees, or own hardcoded lexer/parser source paths before
invoking the compiler. They also default to bounded execution
(`PGY_SCALE_PROBE_LIMIT=20`) so ordinary verification does not produce a full
corpus worth of scratch artifacts; use `--full` or `PGY_SCALE_PROBE_LIMIT=0`
for the historical full examples/backend-compare measurement.

TestHarness delta, 2026-07-06: `mir_json_parity.sh` now consumes its mir-lower
tool source, codegen tool source, and backend comparator source from
TestHarness through the `mir-json-parity-paths` manifest suite. The compiled
`mir_lower` owner still emits the 86-row fixture inventory through
`--fixture-manifest`, so path ownership and MIR fixture ownership remain
separate.

TestHarness delta, 2026-07-06: `mir_json_coverage_probe.sh` now consumes its
mir-lower and codegen tool sources from the same `mir-json-parity-paths`
manifest suite. The probe still owns its synthetic coverage cases, but it no
longer owns the executable stage-source paths it compiles for the measurement.
It accepts `PGY_MIR_COVERAGE_LIMIT` / `--limit=N` for quick source-owner wiring
checks while keeping the existing full nine-case coverage map as the default.

TestHarness scratch policy, 2026-07-06: `.tmp` remains the only writable scratch
zone for parity binaries, logs, and comparable artifacts. Source ownership must
come from TestHarness path suites or compiled fixture manifests, not from copied
source trees. Long-running campaign artifacts such as `pgy_backend_compare.*`
must stay opt-in or be cleaned by their owning runner; they are not evidence for
the default self-host path unless a gate explicitly names them. `make clean-scratch`
removes the ignored `.tmp` scratch zone when local scratch growth needs to be
reset. Self-host bootstrap C compiler logs are capped by
`PGY_SELFHOST_CC_LOG_LIMIT_BYTES` so malformed generated C cannot inflate a
single evidence log into a multi-hundred-megabyte artifact.

TestHarness delta, 2026-07-06: `codegen_bootstrap.sh` now consumes codegen,
parser, comparator, mir-lower, codegen fixture, MIR fixture, fuzz-generator,
and sample-source paths from TestHarness through `codegen-bootstrap-paths`.
It also consumes component and audit-tool breadth rows through
`codegen-bootstrap-components` and `codegen-bootstrap-tools`, plus fixed
codegen sample rows and MIR fixture rows through `codegen-bootstrap-samples`
and `codegen-bootstrap-mir-fixtures`, so the bootstrap runner executes
Pergyra-owned rows instead of synthesizing self-host source paths or breadth
fixture lists in shell.

TestHarness delta, 2026-07-06: `selfcheck_sources.sh` now consumes the semantic
checker source through `semantic-parity-paths` and the real-source
source-to-semantic-target rows through `self-host-completeness-semantic-targets`.
The selfcheck runner no longer owns a shell `SELF_SOURCES` array; it executes
the 155-source completeness inventory projected by `completeness_ledger_owner.pgy`.

TestHarness delta, 2026-07-06: `completeness_ledger.sh` now consumes the codegen
tool source through `codegen-parity-paths` before running the codegen stage.
The completeness owner still owns source, stage, and baseline rows; the runner
no longer owns the concrete `src/self_hosted/codegen/main.pgy` source identity.

TestHarness delta, 2026-07-06: `completeness_ledger.sh` now also consumes the
lexer, parser, and semantic tool sources through `lexer-parity-paths`,
`parser-parity-paths`, and `semantic-parity-paths` before compiling stage
tools. It no longer copies lexer/parser/semantic source trees or the shared
self-hosted `lib` tree into its build directory before invoking the compiler.

TestHarness delta, 2026-07-06: the LSP parity runners now consume LSP tool,
fixture, and expected-output paths from `test_harness_lsp_paths_owner.pgy`
through `test_harness_manifest.pgy`. Shell still executes the C/LLVM parity
loops, but diagnostics, transport, request, response, session, document,
state, and hover path constants are no longer shell-owned.

TestHarness delta, 2026-07-06: LSP document-store, session-state, and
hover-content request bodies now live in `test_harness_lsp_paths_owner.pgy`
rows as well. The shell parity scripts frame and compare those rows, but no
longer own the JSON-RPC body literals for the LSP stateful fixtures.

TestHarness delta, 2026-07-06: `fuzz_backend_parity_generator_parity.sh` now
consumes the fuzz backend generator source through the
`fuzz-backend-generator-paths` suite projected from
`test_harness_codegen_bootstrap_paths_owner.pgy`. The generator still produces
its corpus rows at runtime, but shell no longer owns the generator source path.

Fuzz generator delta, 2026-07-08: `manifest_owner.pgy` now owns the
`pgy.selfhost.backend-parity-fuzz-generator.v1` schema, manifest header rows,
case rows, and stdout summary shape. `main.pgy` remains the generator
entrypoint and source-program constructor, but it no longer owns artifact
vocabulary or report shape.

TestHarness delta, 2026-07-06: `backend_output_tri_compare_parity.sh` now
reuses the shared self-host TestHarness manifest compiler and reads the
backend-output comparator source through the `backend-output-comparator-paths`
suite before compiling the comparator through the shared ArtifactZone/TestHarness
helper. The runner still owns process orchestration for C/LLVM binaries, but it
no longer owns the TestHarness manifest source path or per-case comparator
source/lib/compiler copy.

ArtifactZone delta, 2026-07-06: `backend_output_tri_compare_parity.sh` now
expects the current seven-row backend-output comparator path suite and treats the
comparator process exit code plus schema emission as the dynamic C/LLVM artifact
equality boundary. It no longer re-reads the comparator JSON `ok:true` field in
shell for each stdout/stderr pair.

TestHarness delta, 2026-07-06: `llvm_leg_helpers.sh` now resolves the
TestHarness manifest source through the compiler-world path projection and
resolves the default backend-output comparator source through the
`backend-output-comparator-paths` suite when callers omit an explicit comparator
source. This keeps explicit source arguments for already-manifested runners, but
removes the shared helper's direct TestHarness manifest and comparator source
defaults.

TestHarness delta, 2026-07-06: `test_harness_manifest.pgy` no longer owns
`EmitSelfHostCompleteness*` forwarding wrappers. Its completeness suites dispatch
directly to `completeness_ledger_owner.pgy`, and the compiler-world contract now
keeps the manifest itself under the 600-line owner cap.

TestHarness delta, 2026-07-06: `runtime_boundary_checker_parity.sh` now gets
the checker source, expected clean JSON, and the missing-term fixture
`(path, term)`, plus the expected missing-term JSON artifact through the
`runtime-boundary-paths` manifest suite. It still gets the full required-term
list from the compiled Pergyra checker's `--terms` manifest, but only to set up
the scratch fixture. Shell remains the external parity runner, scratch mutator,
and missing-fixture `rc=1` checker; it no longer re-greps the clean documents or
owns `ok:false`, `missing:1`, or stripped-term verdict interpretation.

Plane note, 2026-07-06: this is repository-authoring guard work, not
Fortran-derived data-parallel language work. The former keeps future
LLM-written changes from drifting across owner boundaries; the latter remains a
Pergyra semantics/projection competitiveness axis in
`docs/168_fortran_parallel_evidence.md`.

TestHarness split, 2026-07-06: inventory checker suites now live in
`test_harness_inventory_paths_owner.pgy`, and production size checker suites
now live in `test_harness_size_paths_owner.pgy`. This keeps
`test_harness_tool_paths_owner.pgy` below the 600-line review cap and gives
negative finding-kind rows a responsibility owner rather than adding more shell
literal checks.

TestHarness delta, 2026-07-05: `doc_link_checker_parity.sh` now gets the
checker source, expected clean JSON, and `docs/INDEX.md` input path through the
`doc-link-checker-paths` manifest suite. The compiled checker receives that
input path through `Args()[0]` on both the C and LLVM parity legs, so the path
fact reaches the tool boundary instead of staying a shell constant.

TestHarness delta, 2026-07-05: `diagnostic_catalog_checker_parity.sh` now gets
the checker source, clean/missing expected JSON paths, diagnostic code owner,
docs owner, and C oracle path through the `diagnostic-catalog-paths` manifest
suite. The compiled checker receives the code/docs owner paths through
`Args()`, so the diagnostic catalog input boundary consumes the same Pergyra
path facts that the parity runner executes.

TestHarness delta, 2026-07-06: `diagnostic_catalog_checker_parity.sh` now
compiles and runs the manifest-projected checker source in place. It no longer
creates a build-dir `main.pgy` alias or copies the checker owner files and
self-hosted `lib` tree beside that alias before invoking the compiler.

TestHarness delta, 2026-07-05: concrete tool/input path suites now live in
`test_harness_tool_paths_owner.pgy`. `examples_inventory_checker_parity.sh`
gets its checker source and expected clean JSON through the
`examples-inventory-paths` manifest suite and runs the compiled checker for
both clean and drift fixtures.

TestHarness delta, 2026-07-06: `examples_inventory_checker_parity.sh` now gets
the expected `inventory_count_drift` verdict through the
`examples-inventory-paths` manifest suite as
`expected/count_drift.json`. Shell remains the synthetic fixture mutator, but
it no longer owns the semantic finding-kind string or negative verdict shape.
This is a repository-authoring/LLM guard, not the Fortran-derived
data-parallel language plane.

TestHarness delta, 2026-07-06: `module_manifest_resolver_parity.sh` now consumes
negative JSON fixture bodies and expected verdict artifacts from TestHarness,
`stdlib_dispatch_inventory_checker_parity.sh` now consumes its count-drift
expected verdict artifact from TestHarness, and the production size checkers
now consume their over-cap expected verdict artifacts from TestHarness. Shell
still constructs scratch fixtures, but the module-manifest, stdlib-dispatch,
and production size negative verdict shapes are no longer shell-owned.

TestHarness delta, 2026-07-05: `ast_read_surface_checker_parity.sh` now gets
the checker source, expected clean JSON, and `tests/ast_read_surface_ratchet.txt`
through the `ast-read-surface-paths` manifest suite, then runs the compiled
checker binary for clean and growth fixtures.

TestHarness delta, 2026-07-06: `ast_read_surface_checker_parity.sh` now
compiles and runs the manifest-projected checker source in place. It no longer
creates a build-dir `main.pgy` alias or copies the self-hosted `lib` tree
beside that alias before invoking the compiler.

TestHarness delta, 2026-07-06: `ast_read_surface_checker_parity.sh` now gets
the synthetic growth source path, growth source line, and growth ratchet row
through `test_harness_tool_paths_owner.pgy`. Shell still creates the scratch
growth fixture, but no longer owns the `source_ast` surface payload or ratchet
row used to prove fail-closed growth detection.

TestHarness delta, 2026-07-06: `ast_read_surface_checker_parity.sh` now also
gets the expected growth verdict JSON artifact from
`test_harness_tool_paths_owner.pgy`. Shell still executes the negative growth
fixture and checks `rc=1`, but no longer owns the `surface_growth` diagnostic
shape that proves the fail-closed path.

AST read surface delta, 2026-07-08: `report_owner.pgy` now owns
`pgy.selfhost.ast-read-surface.v1`, source/count row names, input-error
findings, surface-growth findings, and final report shape. `main.pgy` owns
ratchet parsing and live `DirWalk` counting only, then emits through the report
owner.

TestHarness delta, 2026-07-05: `air_graph_json_validator_parity.sh` now gets
the checker source, AIR evidence owner, expected clean JSON, committed AIR
fixtures, and live AIR source paths through the
`air-graph-json-validator-paths` manifest suite. The compiled checker receives
the committed fixture paths through `Args()`, so the AIR evidence validation
input boundary consumes the same Pergyra path facts that the parity runner
executes.

TestHarness delta, 2026-07-06: `air_graph_json_validator_parity.sh` now
compiles and runs the manifest-projected validator source in place. It no
longer creates a build-dir `main.pgy` alias or copies validator owner files,
self-hosted `lib`, or `compiler/air_evidence_owner.pgy` beside that alias
before invoking the compiler.

TestHarness delta, 2026-07-05: `production_c_size_checker_parity.sh` and
`production_header_size_checker_parity.sh` now get checker source and expected
clean JSON through `production-c-size-paths` and `production-header-size-paths`
manifest suites, then run compiled checker binaries for clean and over-cap
fixtures.

TestHarness delta, 2026-07-05: `stdlib_dispatch_inventory_checker_parity.sh`
now gets the checker source, expected clean JSON, C scalar dispatch, C unary
dispatch, and LLVM scalar/IO dispatch owner paths through the
`stdlib-dispatch-inventory-paths` manifest suite, then runs the compiled
checker binary for clean and dispatch-drift fixtures.

TestHarness delta, 2026-07-06: `stdlib_dispatch_inventory_checker_parity.sh`
now also gets its dispatch-drift strip pattern and strip count from
`test_harness_tool_paths_owner.pgy`. Shell still mutates the scratch LLVM
dispatch file, but no longer owns which dispatch row shape or deletion count
proves the drift fixture.

TestHarness delta, 2026-07-05: `semantic_parity.sh` no longer owns the
108-row `SOURCE_PAIRS` fixture inventory. The compiled semantic owner emits
`--fixture-manifest` rows from `DirWalk("src/self_hosted/semantic/fixture")`
plus paired `expected/*.diag` statuses, and the shell runner consumes those rows
before invoking the C oracle and C/LLVM-built semantic tools.

TestHarness delta, 2026-07-05: `codegen_parity.sh` no longer owns the 65-row
`FIXTURES` inventory. The compiled codegen owner emits `--fixture-manifest`
rows from `DirWalk("src/self_hosted/codegen/fixture")` plus paired
`expected/*_stdout.txt` existence, and the shell runner consumes those rows
before invoking the C oracle and C/LLVM-built codegen tools.

TestHarness delta, 2026-07-05: `lexer_parity.sh` no longer owns the eight-row
`SOURCE_PAIRS` source/fixture inventory. The compiled lexer owner emits
`--fixture-manifest` rows from `fixture_manifest_owner.pgy`, and the shell
runner consumes those rows before invoking C/LLVM lexer parity and live
`pgy --tokens` drift checks.

TestHarness delta, 2026-07-06: `lexer_parity.sh` now compiles and runs the
manifest-projected lexer source in place. It no longer creates a build-dir
`main.pgy` alias or copies lexer owner files beside that alias before invoking
the compiler.

TestHarness delta, 2026-07-05: `parser_parity.sh` no longer owns the 188-row
`SOURCE_PAIRS` source/fixture inventory. The compiled parser owner emits
`--fixture-manifest` rows from `fixture_manifest_owner.pgy`, and the shell
runner consumes those rows before invoking C/LLVM parser parity and live
`pgy --ast` drift checks.

TestHarness delta, 2026-07-06: `parser_parity.sh` now compiles and runs the
manifest-projected parser source in place. It no longer creates a build-dir
`main.pgy` alias or copies parser owner files and the self-hosted `lib` tree
beside that alias before invoking the compiler.

TestHarness delta, 2026-07-05: `driver_rung0_parity.sh` and
`driver_rung1_parity.sh` no longer own the three driver fixture paths. The
compiled driver owners emit `--fixture-manifest` rows from
`driver_rung0_owner.pgy`, and both shell runners consume that shared manifest
before comparing AST text and emitted C artifacts.

TestHarness delta, 2026-07-06: `test_harness_driver_paths_owner.pgy` now owns
the DRV-0/DRV-1 driver, parser, and codegen source path suites. The two driver
parity runners read those source paths through `test_harness_manifest.pgy`
before compiling tools, while fixture inventories remain owned by
`driver_rung0_owner.pgy`.

TestHarness delta, 2026-07-05: `mir_json_parity.sh` no longer owns the
86-row positive fixture inventory. The compiled `mir_lower` owner emits
`--fixture-manifest` rows from `fixture_manifest_owner.pgy`, and the shell
runner consumes those rows before invoking `pgy --mir-json`, `mir_lower`,
`codegen`, and the C oracle.

TestHarness delta, 2026-07-08: `self_host_execution_lane_parity_smoke.sh` no
longer owns the SEA execution-lane source/golden paths. The paths now come from
the `execution-lane-parity-paths` suite emitted by `test_harness_manifest.pgy`,
so the root smoke executes the self-hosted harness owner instead of carrying its
own `src/self_hosted/sea/...` path constants.

SEA executor delta, 2026-07-08: the same execution-lane path suite now also
owns `lane_executor_contract.pgy` plus clean and missing-term expected
artifacts. `lane_executor_contract_owner.pgy` owns the schema, runtime file set,
required runtime terms, lane rows, scaffold-depth label, and negative self-test
term; the self-hosted probe consumes those owner facts while reading
`src/runtime/pgy_lane_scheduler.{c,h}`. The emitted
`pgy.selfhost.lane-executor-contract.v1` artifact records the current
`depth=scaffold-synchronous` status and fail-closed `Reject` behavior. Its
missing-term negative artifact now runs through both C-built and LLVM-built
self-host probes when LLVM is available. This does not claim production
executor depth; it turns the current scaffold into a tracked artifact that must
change when dedicated lane executors land.

SEA producer-coverage delta, 2026-07-08: the native execution-lane policy proof
now pins every resource-capture family that conflicts with movability, not only
pin/raw-slot. `execution-lane-policy-test-smoke` covers pin, live-view, and
raw-channel rejection edges, and the self-host SEA parity artifact grew from
31 to 33 rows with `negative_live_view_requires_movability` and
`negative_raw_channel_requires_movability`. This tightens the
BoundaryCaptureFact fail-closed contract without claiming the production
executor split is complete.

SEA raw-channel producer delta, 2026-07-09: `air_execution_lane.c` and the
self-host SEA mirror now preserve `has_rir_raw_channel_capture_evidence` as a
boundary-local resource fact, not only for `AIR_BOUNDARY_CHANNEL`. The C
evidence proof adds parallel raw-channel pin/reject rows, and the self-host SEA
parity artifact grows from 33 to 35 rows with
`air_parallel_raw_channel_pins` and
`negative_parallel_raw_channel_requires_movability`. This closes a producer
gap where parallel/spawn-shaped raw-channel evidence could otherwise be erased
before `BoundaryCaptureFact` reached the lane classifier.

Artifact Zone delta, 2026-07-04: LSP transport outputs are now tracked as
their own comparable artifact kinds: `CompilerLspTransportFrameArtifactKind()`
for LSP-2a single-frame output and `CompilerLspTransportStreamArtifactKind()`
for LSP-2b buffered stream output. LSP request dispatch output is tracked by
`CompilerLspRequestDispatchArtifactKind()`, response-emission output is tracked
by `CompilerLspResponseEmissionArtifactKind()`, and buffered session replay is
tracked by `CompilerLspSessionReplayArtifactKind()`. Buffered document-store
state is tracked by `CompilerLspDocumentStoreArtifactKind()`, buffered
session-state output is tracked by `CompilerLspSessionStateArtifactKind()`, and
hover-content output is tracked by `CompilerLspHoverContentArtifactKind()`. Content-Length
transport, request-dispatch, response-emission, session-replay, and
document-store/session-state/hover-content parity must not reuse or alias the
LSP diagnostics artifact.

## Held Surfaces

These are explicitly not imported as default hard-self-host dependencies.

| Surface | State | Reason |
|---|---|---|
| General raw pointer API | `HOLD` | Would reopen the normal domain/codegen path to unmanaged aliasing. |
| Broad `extern "C"` FFI for compiler internals | `HOLD` | Parallel binaries and artifact comparison are safer until ABI contracts are stable. |
| Native runtime rewrite in Pergyra | `HOLD` | The runtime kernel is the target program's native support layer; rewriting it creates a bootstrap cycle. |
| Zero-runtime erasure claim | `HOLD` | Pergyra requires no-hidden-runtime evidence, not universal zero-cost erasure. |
| Direct WASM backend as self-host prerequisite | `HOLD` | C/LLVM remain the first projections; direct wasm is post-beta. |

## Work Order

1. Promote the mixed AST-like tree owner first. It is the largest remaining
   reason self-hosted parser/codegen slices still consume text artifacts.
2. Add a stable JSON fact owner before adding more IR/AIR tools.
3. Implement the subprocess runner against the capability envelope; do not add
   unrestricted shell escape.
4. Repoint C, LLVM, and self-hosted symbol/mangle and ABI/layout consumers to
   the compiler-world row owners before widening backend parity.
5. Repoint AIR evidence and TestHarness consumers from shell scripts into the
   `PgyCompilerWorld` zones before claiming three-way compiler self-proof.

## Rejection Rule

A hard rung must fail if it needs an `ACTIVE` or `HOLD` surface and tries to
continue by:

- parsing text or JSON to recover a fact whose owner should already exist;
- choosing C-like behavior when C and LLVM disagree;
- inventing ABI layout, symbol spelling, authority evidence, slot ownership, or
  materialization policy locally;
- building diagnostics or JSON in `main.pgy`;
- adding a fake zone around a function group that does not own a distinct
  resource.

This is the expansion guard: add the surface now, or reject the program. Do not
grow self-hosting by hiding another compatibility path.

## Nominal Record Array Scope

The `record_array_basic` backend-compare fixture closes the first
`Array<NominalRecord>` parity seam. The LLVM path now records the nominal
element name in the array registry and uses that fact for indexed member access
such as `rows[0].field`; it does not re-infer the element from the AST payload.
Nominal arrays use raw byte-array runtime exports for the currently supported
mutation surface.

This is intentionally not a claim that every collection algorithm accepts
nominal records. `ArrayMap`, `ArrayFilter`, `ArraySort`, slicing, and broader
generic collection algorithms remain primitive/scalar-owned until their ABI and
runtime facts have explicit owners and parity fixtures.
