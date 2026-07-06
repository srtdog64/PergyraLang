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
| Target capability envelope (self-host C subset) | `target_capability_owner.pgy`, `TargetCapabilityZone` | `self-host-compiler-world-contract-test-smoke`, real-source selfcheck | CPU/C/LLVM/self-hosted projection rows, target facts, and fallback reasons are named owner facts. `CompilerTargetCapabilityEnvelopeReady()` consumes those facts instead of comparing row indexes to string literals. The self-host C codegen run boundary consumes that envelope before `GenerateC`, so hidden CPU fallback cannot enter that hard rung by omission. |
| Self-host C symbol spelling | `src/self_hosted/compiler/symbol_table_owner.pgy` | component contract, real-source selfcheck, codegen parity | function/method/operator/enum names and namespace-qualified call spellings are consumed from the compiler-world row owner inside the current self-host C subset |
| Self-host C ABI type spelling | `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy` | component contract, real-source selfcheck, codegen parity | parameter, return, local, and field C type spellings are consumed from one owner inside the current self-host C subset |
| Self-host call parameter modes | `src/self_hosted/codegen/type_facts/type_env.pgy` | component contract, real-source selfcheck, codegen parity | function signature emission still records `pm` rows in the flat environment, but expression rewrite consumes parameter-mode count/index facts through `type_facts` instead of parsing the CSV locally |
| Self-host typed AST-text bridge | `src/self_hosted/codegen/input/ast_text_inventory_owner.pgy`, `src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy`, `src/self_hosted/codegen/input/ast_text_statement_owner.pgy`, `src/self_hosted/codegen/typed_ast_node_skeleton.pgy`, plus `pgy --ast` parameter-mode preservation | component contract, real-source selfcheck, parser parity, codegen parity, compiler-world contract | raw `pgy --ast` line splitting, `CodegenAstTextNode` inventory, indentation, parent/kind/payload/name/type/mode rows, marker-node predicates, blank-line filtering, `[export]` normalization, program-level typed routing, declaration collector prepasses, `CodegenAstTextRowFactInput`, function/return/role/nominal/enum-name/enum-variant/field/parameter/`Let` row facts, statement payload fields, `Let`/`Assign` fact accessors, `Log`/`Return`/`ArrayPop`/`Exit` simple-statement facts, `ArraySet`/`ArrayPush` collection mutation statement payload facts, `For`/`While`/`If`/`Else` control-flow statement fact accessors, bare-call statement kind/payload facts, function signature/header facts, statement body reads, `inout`/`own`/`ref` parameter-mode facts, cursor expectation checks, `CodegenTypedAstBridgeReady`, and the typed AST arena payload contract are consumed from input fact owners during the transitional text bridge; `CompilerEmissionFactReady()` also makes the typed AST arena contract load-bearing for `ProgramEmitter` readiness in `PgyCompilerWorld` |
| Self-host C collection runtime symbols | `src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | `Array<Int>` / `Array<String>` helper call names and the bootstrap-only `Array<CodegenAstTextNode>` record-array lane are consumed from one owner inside the current self-host C subset |
| Self-host C math/random symbols | `src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | math/random helper and target-library call names are consumed from one owner inside the current self-host C subset |
| Self-host C host I/O/process symbols | `src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | file, directory-walk, argv, and process-exit helper or target-library call names are consumed from one owner inside the current self-host C subset |
| Self-host C Option/Result runtime symbols | `src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | `Option<Int>` / `Option<String>` / `Result<Int>` helper call names are consumed from one owner inside the current self-host C subset |
| Self-host C string/text symbols | `src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy` | component contract, real-source selfcheck, codegen parity | supported string/text helper and conversion target-library call names are consumed from one owner inside the current self-host C subset |
| Basic nominal-record arrays | LLVM array registry `elem_name` facts plus raw record-array runtime exports | `backend_compare/record_array_basic` through C and LLVM | `Array<NominalRecord>` can be created, passed as a parameter, pushed, set, popped, indexed, and used for member access without reopening AST type guessing |
| DRV-0 artifact gate | `src/self_hosted/compiler/driver_rung0_owner.pgy`, `src/self_hosted/compiler/driver_rung0_main.pgy` | `self-host-driver-rung0-parity-test-smoke` | source path -> self-parser AST text -> self-codegen emitted C is assembled in one Pergyra owner boundary and compared against `pgy --ast` plus the current codegen oracle |
| Artifact Zone evidence | `src/self_hosted/compiler/artifact_zone_owner.pgy`, `ArtifactZone` | `self-host-component-contract-test-smoke`, parity artifact gates | Comparable diagnostics, LSP, AST text, AIR/MIR JSON, ABI/layout, materialization, emitted C/LLVM/self-hosted, and run-output artifacts now route equality verdicts through `backend_output_comparator` with explicit artifact kinds. The only direct shell comparison left under `tests/self_hosted/parity` is `backend_output_comparator_parity.sh`, where shell is the comparator's own external oracle rather than a consumer fallback. |

## Active Blockers

These are the surfaces to bring in before claiming broader hard self-hosting.
They are not optional polish; each one prevents a common fallback shape.

| Blocker | Required owner | Why it matters |
|---|---|---|
| Mixed AST-like tree owner | Pergyra record/class/tagged-node owner plus traversal parity | The AST-text bridge now has typed line nodes with parent edges, coarse kind rows, and function/return/role/nominal/enum-name/enum-variant/field/parameter/statement payload rows. `ast_text_row_fact_owner.pgy` derives name/type/mode rows from `CodegenAstTextRowFactInput` once during inventory construction, so function, return, role, nominal, enum, field, parameter, and `Let` name/type consumers read `CodegenAstTextNode.name`, `type_name`, and `mode` instead of reparsing payload text. `program_emit` and `function_emit` route declaration categories through owner-owned kind predicates, declaration collectors consume typed nodes for global env/prototype/struct/enum prepasses, runtime/header usage facts are derived from typed node payload/kind facts through `ast_usage_owner.pgy` rather than whole-AST or line-text rescans, and structural marker checks (`Program:`, `Body:`, `Block:`, `Then:`) plus function/return/role/enum-name/nominal/enum-variant/field/parameter payload reads are centralized behind input owner accessors and owner-owned kind/name/type/mode facts. `stmt_emit` now consumes owner-owned `Let`, `Assign`, `Log`, `Return`, `Defer`, `ArrayPop`, `ArraySet`, `ArrayPush`, `Exit`, `Break`, `Continue`, `For`, `While`, `If`, `Else`/`else if`, and bare call statement facts from `ast_text_statement_owner.pgy` instead of splitting those AST lines locally, with `Let` name/type read from row facts and statement predicates/payloads consuming inventory-owned kind/payload facts; parameter mode facts survive into codegen. Emission owners are now ratcheted against direct `CodegenAstTextNode.text` access and can only ask the input owner for diagnostic provenance text. This blocker remains active because `CodegenAstTextNode.text` is still a line-text payload inside the bridge owner; it closes only when owned typed/tagged AST data replaces line-text semantics. |
| Stable JSON parse/emit owner | schema-aware JSON reader/writer with diagnostics | Read primitives plus string/field/object/array emission are shared, all current self-hosted report schemas consume the object/array writer through the direct `json_emit.pgy` owner import, AIR/module validators consume schema or top-level field checks through owner-level fact tables rather than document-local helpers, AIR graph validator document-root schema equality consumes `JsonDocumentFactStringFieldEquals`, document-root required keys consume `JsonDocumentObjectFactTable` / `JsonObjectFactHasField`, and root `summary` count rows now consume `JsonObjectFactObjectTable` plus `JsonObjectFactNumberFieldOpt` instead of carrying raw summary bounds into the AIR scanner, AIR graph feature requirements (`compression_budget`, `compression_reason`, `execution_lane`, `boundary_capture`) are graph-wide scalar facts consumed through `AirGraphScalarFieldValues`, AIR graph live consumers share `AirGraphSummaryIntField` for summary count rows, AIR graph id/ref/reachability consumers share `AirGraphScalarFieldValues` for scalar graph facts instead of owning local `"id"`/`"from"`/`"to"`/`"root"` token scanners, `module_manifest_resolver` now consumes the root `modules` array and module-row count/field/equality facts through `JsonObjectFactTable` and `JsonArrayObjectFactTable` boundary facts so nested `"modules"` text cannot satisfy the root contract and resolver-local row scans cannot drift, and `mir_lower` schema validation consumes `MirDocumentSchemaEquals` from `json_fact_read.pgy` while declaration/routine root-array discovery/header/body-boundary/program assembly/routine CFG block/successor/instruction/source-local/statement-array/match-pattern lowering consumes MIR row/object/string/array facts through `json_fact_read.pgy`, `JsonArrayObjectFactTable`, and `routine_inventory_owner.pgy` instead of local schema substring, root `decls`/`routines` array scans, field-key, global name, `"blocks"` key, block marker, instruction kind, successor key, or suffix scans. Object string-field and number-field absence now have `Option<String>` fact APIs (`JsonObjectStringFieldOpt`, `JsonObjectNumberFieldOpt`, `JsonObjectFactStringFieldEquals`, `JsonDocumentFactStringFieldEquals`, `JsonObjectFactNumberFieldOpt`, `MirDocumentSchemaEquals`, `MirObjectStringFactOpt`, `MirObjectNumberFactOpt`), MIR fact graph and AIR graph summary/schema consumers use those facts instead of empty-string sentinels or document-local schema reads, and JSON emission has a separate `json_emit.pgy` owner instead of a transitive `json.pgy` import. This blocker remains active because most consumers still rely on bounded scan helpers rather than a complete shared JSON DOM/fact table. |
| Subprocess runner | `src/self_hosted/compiler/subprocess_runner_owner.pgy` | The capability envelope now names executable path, argv, cwd, env allowlist, timeout, stdout/stderr, and exit code facts. `CompilerSubprocessOracleComparePlanReady()` fixes the exact fact/use-case schema through named envelope fact accessors and named use-case facts for `oracle_compare`, `fixture_build`, and `artifact_probe`, and `CompilerSubprocessFactKnown()` / `CompilerSubprocessUseCaseKnown()` guard the row vocabulary before subprocess evidence is consumed. Oracle timeout is now a numeric owner fact projected to the report string, and the env allowlist is a count/index/known row set projected to CSV only at the report boundary. `backend_output_comparator` now records the oracle-compare use case, stream fact, and exit fact through named owner functions instead of positional fact indexes. It remains active until a Pergyra runner executes against that envelope instead of shell-only logic. |
| Target capability envelope (native/global consumers) | `target_capability_owner.pgy`, `TargetCapabilityZone` | The current self-host C subset consumes the envelope, but native C/LLVM target-specific consumers still need to read the same envelope before the global surface is complete. AIR-overlapping target fact names remain target-envelope facts until the import/idempotence contract is checked and the target owner can consume `air_evidence_owner.pgy` directly. |
| Symbol/mangle owner | `src/self_hosted/compiler/symbol_table_owner.pgy` | The self-host C subset now consumes the compiler-world spelling row owner directly and fail-closes unless the exact row envelope is ready. Source owner/name, namespace path, C/LLVM/self-host symbol rows, and collision policy are named owner facts, and `CompilerSymbolTableReady()` consumes those facts instead of comparing row/projection indexes to string literals. This remains active until native C, LLVM, and self-hosted projections all consume the same concrete row table. |
| Cross-backend ABI/layout row projection | `src/self_hosted/compiler/abi_layout_row_owner.pgy` plus current C-subset `abi_layout_owner.pgy` | The compiler-world owner now carries concrete C ABI row projections for the supported self-host subset (`Int`, `Bool`, `Float`, `String`, `Array<Int>`, `Array<String>`, `Array<CodegenAstTextNode>`, `Result<Int>`, `Option<Int>`, `Option<String>`) plus materialization policy. Fact columns, canonical type spellings, compatibility type spellings, C value type spellings, and materialization lanes are named owner facts. `CompilerAbiLayoutRowsReady()` fixes the eight fact columns and pins the concrete-row envelope count plus first/last materialization lanes through those facts, so the C subset cannot claim readiness with only the fact names present or reintroduce row-index literal comparisons. The self-host C ABI owner consumes those rows first and only consults `TypeEnvZone` for user structs. This remains active until native C/LLVM and self-hosted consumers read the same concrete row table. |
| AIR evidence zone | `src/self_hosted/compiler/air_evidence_owner.pgy`, `AirEvidenceZone` | `PgyCompilerWorld` now owns the hard-rung evidence vocabulary for intent/effect/authority/coordination/slot/materialization/loss. Each evidence row is a named owner fact, and `CompilerAirEvidenceEnvelopeReady()` fixes the exact seven-fact order by consuming those facts instead of comparing row indexes to string literals. The AIR graph JSON validator run boundary now consumes that envelope before reading AIR fixtures. It remains active until hard rungs consume live AIR evidence rows rather than only the vocabulary envelope. |
| Test harness substrate | `src/self_hosted/compiler/test_harness_owner.pgy`, `src/self_hosted/compiler/test_harness_tool_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_driver_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_parser_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_mir_json_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy`, `src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy`, `TestHarnessZone` | Fixture and result row vocabulary is now Pergyra-owned. Source, diagnostic, AIR JSON, MIR JSON, ABI layout, stdout, exit, projection, C/LLVM/self-hosted projection names, and comparable fixture paths are named owner facts, and `CompilerTestHarnessReady()` consumes those facts instead of comparing row/projection indexes to string literals. `backend_output_comparator` records C/LLVM/self-hosted projection rows, comparable artifact paths, and finding caps by consuming this owner. `test_harness_tool_paths_owner.pgy` owns concrete parity tool/input path suites so the core TestHarness owner stays focused on row/projection/artifact vocabulary; `test_harness_driver_paths_owner.pgy` owns DRV-0/DRV-1 driver/parser/codegen source path suites separately; `test_harness_codegen_paths_owner.pgy` owns codegen parity tool/input directories separately; `test_harness_parser_paths_owner.pgy` owns parser parity tool/comparator/fixture/expected paths separately; `test_harness_semantic_paths_owner.pgy` owns semantic parity tool/comparator/fixture/expected/diagnostic-owner paths separately; `test_harness_mir_json_paths_owner.pgy` owns MIR JSON parity mir-lower/codegen/comparator input paths separately and reuses the codegen path owner facts for codegen/comparator paths so those strings do not fork; `test_harness_codegen_bootstrap_paths_owner.pgy` owns codegen bootstrap tool paths, the fuzz backend generator path suite, plus component/tool breadth rows so bootstrap and fuzz-generator runners do not synthesize self-host source paths in shell; `test_harness_lsp_paths_owner.pgy` owns LSP diagnostics, transport, request, response, session, document, state, and hover path suites so those scripts execute manifest rows instead of owning LSP source/expected path constants. `backend_output_tri_compare_parity.sh` now gets its smoke/extended backend case suites from the Pergyra `test_harness_manifest.pgy` projection over `TestHarnessZone` instead of owning the case arrays in shell. `linter_parity.sh` now gets its tool source, expected diagnostics, and fixture path from the same manifest and passes the fixture path into the compiled linter through `Args()[0]`. `module_manifest_resolver_parity.sh` now gets its tool source, expected JSON, and input manifest path from the manifest and passes the manifest path into the compiled resolver through `Args()[0]`. `stable_subset_section_checker_parity.sh` now gets its tool source, expected JSON, and input manifest path from the manifest and passes the manifest path into the compiled checker through `Args()[0]`. `examples_inventory_checker_parity.sh` now gets its tool source and expected JSON from the manifest before running the C/LLVM-built checker. It accepts artifact paths and projection row indexes through `Args()`, and `backend_output_tri_compare_parity.sh`, `linter_parity.sh`, `module_manifest_resolver_parity.sh`, `stable_subset_section_checker_parity.sh`, `examples_inventory_checker_parity.sh`, `llvm_leg_helpers.sh`, `codegen_parity.sh`, `parser_parity.sh`, `semantic_parity.sh`, `mir_json_parity.sh`, `codegen_bootstrap.sh`, `fuzz_backend_parity_generator_parity.sh`, `lexer_parity.sh`, the LSP parity scripts, plus AIR graph validator clean JSON parity now route verdicts through that Pergyra owner. It remains active until the remaining shell parity scripts are projections of these records instead of the primary harness owner. |

TestHarness delta, 2026-07-05: `test_harness_air_graph_paths_owner.pgy`
now owns the five AIR graph consumer path suites separately from the generic
tool-path owner. `air_graph_id_uniqueness_parity.sh`,
`air_graph_node_count_integrity_parity.sh`,
`air_graph_reachability_parity.sh`, `air_graph_ref_integrity_parity.sh`, and
`air_graph_ref_live_parity.sh` read tool source, shared scan owner, expected
JSON, and fixture paths from `test_harness_manifest.pgy`; the compiled
checkers receive the selected fixture path through `Args()[0]`.

TestHarness delta, 2026-07-06: the five AIR graph consumer parity runners now
compile and run their manifest-projected checker sources in place. They no
longer create build-dir `main.pgy` aliases or copy `scan_owner.pgy` and the
self-hosted `lib` tree beside those aliases before invoking the compiler.

ArtifactZone delta, 2026-07-06: `air_graph_id_uniqueness_parity.sh` no longer
recomputes the clean duplicate-id count with shell `grep`/`sort`/`uniq`.
The clean output oracle is the TestHarness-projected `expected/clean.json`
compared through `backend_output_comparator`; shell remains only the process
runner and negative-fixture executor for the duplicate-id case.

ArtifactZone delta, 2026-07-06: `air_graph_reachability_parity.sh` no longer
recomputes the clean node count with shell `grep`/`wc`. The clean output oracle
is the TestHarness-projected `expected/clean.json` compared through
`backend_output_comparator`; shell remains only the process runner and
negative-fixture executor for the orphan-node case.

ArtifactZone delta, 2026-07-06: `air_graph_ref_integrity_parity.sh` no longer
recomputes clean dangling endpoint counts with shell `grep`/`comm`. The clean
output oracle is the TestHarness-projected `expected/clean.json` compared
through `backend_output_comparator`; shell remains only the process runner and
negative-fixture executor for the dangling-endpoint case.

TestHarness delta, 2026-07-05: backend_output_comparator_parity.sh now consumes its source, expected JSON, and comparable artifact paths from TestHarness through the `backend-output-comparator-paths` manifest suite. Shell is still the comparator's own external text-equivalence oracle, but it no longer owns the comparator input path constants.

TestHarness delta, 2026-07-06: `backend_output_comparator_parity.sh` now
compiles and runs the manifest-projected comparator source in place. It no
longer creates a build-dir `main.pgy` alias or copies the self-hosted `lib` and
`compiler` owner tree beside that alias before invoking the compiler.

TestHarness delta, 2026-07-06: `stable_subset_section_checker_parity.sh` now
compiles and runs the manifest-projected stable-subset checker source in place.
It no longer creates a build-dir `main.pgy` alias or copies the self-hosted
`lib` tree beside that alias before invoking the compiler.

ArtifactZone delta, 2026-07-06: `stable_subset_section_checker_parity.sh` no
longer recomputes the clean section count with shell `grep`. The clean output
oracle is the TestHarness-projected `expected/clean.json` compared through
`backend_output_comparator`; shell remains only the process runner and the
negative-fixture mutator for the missing-section case.

TestHarness delta, 2026-07-06: `doc_link_checker_parity.sh` now compiles and
runs the manifest-projected doc-link checker source in place. It no longer
creates a build-dir `main.pgy` alias or copies the self-hosted `lib` tree
beside that alias before invoking the compiler.

ArtifactZone delta, 2026-07-06: `doc_link_checker_parity.sh` no longer
recomputes clean total-link or markdown-link counts with shell `grep`/`wc`.
The clean output oracle is the TestHarness-projected `expected/clean.json`
compared through `backend_output_comparator`; shell remains only the process
runner and negative-fixture mutator for the dead-link case.

TestHarness delta, 2026-07-06: `examples_inventory_checker_parity.sh` now
compiles and runs the manifest-projected examples-inventory checker source in
place. It no longer creates a build-dir `main.pgy` alias or copies the
self-hosted `lib` tree beside that alias before invoking the compiler.

TestHarness delta, 2026-07-06: `production_c_size_checker_parity.sh` and
`production_header_size_checker_parity.sh` now compile and run their
manifest-projected checker sources in place. They no longer create build-dir
`main.pgy` aliases or copy the self-hosted `lib` tree beside those aliases
before invoking the compiler.

TestHarness delta, 2026-07-06: `linter_parity.sh` and
`runtime_boundary_checker_parity.sh` now compile and run their
manifest-projected checker sources in place. They no longer create build-dir
`main.pgy` aliases before invoking the compiler.

TestHarness delta, 2026-07-06: `module_manifest_resolver_parity.sh` and
`stdlib_dispatch_inventory_checker_parity.sh` now compile and run their
manifest-projected checker sources in place. They no longer create build-dir
`main.pgy` aliases or copy the self-hosted `lib` tree beside those aliases
before invoking the compiler.

ArtifactZone delta, 2026-07-06: `module_manifest_resolver_parity.sh` no longer
recomputes clean module, beta-blocker, or stable-subset counts with shell
`grep`. The clean output oracle is the TestHarness-projected
`expected/clean.json` compared through `backend_output_comparator`; shell
remains only the process runner and negative-fixture mutator.

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

TestHarness delta, 2026-07-06: `fuzz_backend_parity_generator_parity.sh` now
consumes the fuzz backend generator source through the
`fuzz-backend-generator-paths` suite projected from
`test_harness_codegen_bootstrap_paths_owner.pgy`. The generator still produces
its corpus rows at runtime, but shell no longer owns the generator source path.

TestHarness delta, 2026-07-06: `backend_output_tri_compare_parity.sh` now
reuses the shared self-host TestHarness manifest compiler and reads the
backend-output comparator source through the `backend-output-comparator-paths`
suite before compiling the comparator through the shared ArtifactZone/TestHarness
helper. The runner still owns process orchestration for C/LLVM binaries, but it
no longer owns the TestHarness manifest source path or per-case comparator
source/lib/compiler copy.

TestHarness delta, 2026-07-06: `llvm_leg_helpers.sh` now resolves the default
backend-output comparator source through the `backend-output-comparator-paths`
suite when callers omit an explicit comparator source. This keeps explicit
source arguments for already-manifested runners, but removes the shared helper's
direct comparator source default.

TestHarness delta, 2026-07-05: `runtime_boundary_checker_parity.sh` now gets
the checker source and expected clean JSON through the
`runtime-boundary-paths` manifest suite, and it gets the required `(path, term)`
rows from the compiled Pergyra checker's `--terms` manifest. Shell remains the
external parity runner, but it no longer owns the runtime-boundary required-term
list.

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

TestHarness delta, 2026-07-05: `ast_read_surface_checker_parity.sh` now gets
the checker source, expected clean JSON, and `tests/ast_read_surface_ratchet.txt`
through the `ast-read-surface-paths` manifest suite, then runs the compiled
checker binary for clean and growth fixtures.

TestHarness delta, 2026-07-06: `ast_read_surface_checker_parity.sh` now
compiles and runs the manifest-projected checker source in place. It no longer
creates a build-dir `main.pgy` alias or copies the self-hosted `lib` tree
beside that alias before invoking the compiler.

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
