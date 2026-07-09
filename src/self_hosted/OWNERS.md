# Self-Hosted Owner Manifest

This file is the source-of-truth map for active self-hosted compiler-stage
modules. It prevents the track from collapsing back into "one directory plus a
large `main.pgy`". Each active `.pgy` file below must own one clear stage
responsibility. `main.pgy` files are entrypoints only: argv wiring,
orchestration, and no semantic decisions.

`tests/self_hosted_component_contract_smoke.sh` requires every active
compiler-stage `.pgy` source to be listed here.

## Shared Lib

- `src/self_hosted/lib/diagnostic.pgy` -- stable diagnostic-block rendering.
- `src/self_hosted/lib/json_scan.pgy` -- shared JSON cursor/string scan
  primitives.
- `src/self_hosted/lib/json_emit.pgy` -- shared JSON string escaping and
  emission primitives for fact-shaped tools.
- `src/self_hosted/lib/json.pgy` -- shared bounded JSON string-read, top-level
  object/value bounds, and array-object row iteration.
- `src/self_hosted/lib/json_fact_table.pgy` -- shared object, array-object,
  and recursive scalar-field facts over bounded JSON spans.
- `src/self_hosted/lib/path.pgy` -- self-hosted source/import path string facts.
- `src/self_hosted/lib/text_scan.pgy` -- shared text-scan helpers.

## Lexer

- `src/self_hosted/lexer/main.pgy` -- entrypoint only.
- `src/self_hosted/lexer/char_owner.pgy` -- character/codepoint predicates.
- `src/self_hosted/lexer/fixture_manifest_owner.pgy` -- lexer parity
  source/fixture manifest rows.
- `src/self_hosted/lexer/run_owner.pgy` -- lexer CLI run boundary and mode
  selection.
- `src/self_hosted/lexer/source_input_owner.pgy` -- source path and file input.
- `src/self_hosted/lexer/scan_owner.pgy` -- token scan loop state.
- `src/self_hosted/lexer/token_owner.pgy` -- token classification, rendering,
  and token-stream payload contract facts.

## Parser

- `src/self_hosted/parser/main.pgy` -- entrypoint only.
- `src/self_hosted/parser/cursor_owner.pgy` -- parser cursor/token stream.
- `src/self_hosted/parser/decl_ability_owner.pgy` -- ability declarations.
- `src/self_hosted/parser/decl_dispatch_owner.pgy` -- top-level declaration dispatch.
- `src/self_hosted/parser/decl_effect_relation_owner.pgy` -- effect and relation declarations.
- `src/self_hosted/parser/decl_enum_owner.pgy` -- enum declarations.
- `src/self_hosted/parser/decl_event_owner.pgy` -- event declarations.
- `src/self_hosted/parser/decl_intent_owner.pgy` -- intent declarations.
- `src/self_hosted/parser/decl_nominal_owner.pgy` -- class/subject/object/tobject/vessel declarations.
- `src/self_hosted/parser/decl_role_owner.pgy` -- role declarations.
- `src/self_hosted/parser/decl_type_owner.pgy` -- type declarations.
- `src/self_hosted/parser/decl_zone_owner.pgy` -- zone declarations.
- `src/self_hosted/parser/error_owner.pgy` -- parser diagnostic strings.
- `src/self_hosted/parser/expr_owner.pgy` -- expression grammar import boundary.
- `src/self_hosted/parser/expr_postfix_owner.pgy` -- postfix expression parsing.
- `src/self_hosted/parser/expr_precedence_owner.pgy` -- precedence expression parsing.
- `src/self_hosted/parser/expr_primary_owner.pgy` -- primary expression parsing.
- `src/self_hosted/parser/expr_string_owner.pgy` -- string literal expression parsing.
- `src/self_hosted/parser/fixture_manifest_owner.pgy` -- parser parity
  source/fixture manifest rows.
- `src/self_hosted/parser/function_decl_owner.pgy` -- function signatures and bodies.
- `src/self_hosted/parser/program_parse_owner.pgy` -- program-root assembly.
- `src/self_hosted/parser/run_owner.pgy` -- parser CLI run boundary and mode
  selection.
- `src/self_hosted/parser/source_path_owner.pgy` -- source path/default and import read input.
- `src/self_hosted/parser/stmt_if_owner.pgy` -- if/if-let statements.
- `src/self_hosted/parser/stmt_loop_owner.pgy` -- loop statements.
- `src/self_hosted/parser/stmt_match_owner.pgy` -- match statements.
- `src/self_hosted/parser/stmt_owner.pgy` -- statement dispatch.
- `src/self_hosted/parser/stmt_parallel_owner.pgy` -- parallel/async statements.
- `src/self_hosted/parser/tree_text_owner.pgy` -- compact AST text rendering
  and current AST payload contract consumed by `PgyCompilerWorld`.
- `src/self_hosted/parser/type_name_owner.pgy` -- written type-name parsing.

## Semantic

- `src/self_hosted/semantic/main.pgy` -- entrypoint only.
- `src/self_hosted/semantic/body_check_owner.pgy` -- statement/body checks.
- `src/self_hosted/semantic/call_check_owner.pgy` -- call arity and argument checks.
- `src/self_hosted/semantic/diagnostic_code_owner.pgy` -- stable semantic diagnostic code vocabulary.
- `src/self_hosted/semantic/diagnostic_owner.pgy` -- semantic diagnostic blocks
  and verdict payload contract facts.
- `src/self_hosted/semantic/env_owner.pgy` -- scoped local environment.
- `src/self_hosted/semantic/expr_type_owner.pgy` -- expression type facts.
- `src/self_hosted/semantic/expr_validation_owner.pgy` -- expression validation facts.
- `src/self_hosted/semantic/program_check_owner.pgy` -- program/function signature checks.
- `src/self_hosted/semantic/semantic_run_owner.pgy` -- semantic CLI run boundary.
- `src/self_hosted/semantic/source_bundle_owner.pgy` -- root/import source bundle.
- `src/self_hosted/semantic/text_scan_owner.pgy` -- semantic text scanning.

## MIR Lower

- `src/self_hosted/mir_lower/main.pgy` -- entrypoint only.
- `src/self_hosted/mir_lower/decl_lower.pgy` -- declaration reconstruction.
- `src/self_hosted/mir_lower/error_owner.pgy` -- MIR-lower diagnostics.
- `src/self_hosted/mir_lower/fixture_manifest_owner.pgy` -- MIR parity
  source fixture manifest rows.
- `src/self_hosted/mir_lower/json_fact_read.pgy` -- bounded MIR JSON fact reads.
- `src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy` -- MIR fact
  graph payload contract facts.
- `src/self_hosted/mir_lower/mir_json_input_owner.pgy` -- MIR JSON input boundary.
- `src/self_hosted/mir_lower/program_lower.pgy` -- document-order program assembly.
- `src/self_hosted/mir_lower/run_owner.pgy` -- MIR-lower CLI run boundary and
  manifest mode selection.
- `src/self_hosted/mir_lower/routine_inventory_owner.pgy` -- routine inventory facts.
- `src/self_hosted/mir_lower/routine_lower.pgy` -- routine CFG/body reconstruction.
- `src/self_hosted/mir_lower/stmt_render.pgy` -- instruction fact to AST text rendering.

## Codegen

- `src/self_hosted/codegen/main.pgy` -- entrypoint only.
- `src/self_hosted/codegen/input/ast_input_owner.pgy` -- AST path and read boundary.
- `src/self_hosted/codegen/input/ast_text_inventory_owner.pgy` -- AST text line inventory, kind row facts, and cursor expectation boundary.
- `src/self_hosted/codegen/input/ast_text_typed_arena_owner.pgy` -- AST text inventory to typed arena projection, including parent and indent rows.
- `src/self_hosted/codegen/input/ast_text_row_fact_owner.pgy` -- AST text name/type/value/aux-value/mode row facts derived from inventory payloads.
- `src/self_hosted/codegen/input/ast_text_array_literal_owner.pgy` -- AST text array-literal initializer shape and top-level element facts.
- `src/self_hosted/codegen/input/ast_text_enum_variant_owner.pgy` -- AST text enum declaration variant-list facts for the supported payload-free enum subset.
- `src/self_hosted/codegen/input/ast_text_function_signature_owner.pgy` -- AST text function name, parameter, and return signature facts.
- `src/self_hosted/codegen/input/ast_text_declaration_owner.pgy` -- AST text nominal, role, enum, and field declaration facts.
- `src/self_hosted/codegen/input/ast_text_try_let_owner.pgy` -- AST text try-let initializer shape and inner-expression facts.
- `src/self_hosted/codegen/input/ast_text_indexed_assignment_owner.pgy` -- AST text indexed assignment target shape, receiver, and index-expression facts.
- `src/self_hosted/codegen/input/ast_text_local_binding_owner.pgy` -- AST text local binding name/type/initializer facts.
- `src/self_hosted/codegen/input/ast_text_assignment_owner.pgy` -- AST text assignment target and RHS facts.
- `src/self_hosted/codegen/input/ast_text_for_stmt_owner.pgy` -- AST text `For` loop variable, range, and foreach collection facts.
- `src/self_hosted/codegen/input/ast_text_statement_payload_owner.pgy` -- AST text single-payload statement argument/condition facts.
- `src/self_hosted/codegen/input/ast_text_collection_stmt_owner.pgy` -- AST text `ArrayPush` and `ArraySet` statement payload facts.
- `src/self_hosted/codegen/input/ast_expression_usage_owner.pgy` -- expression usage facts and builtin-callee group rows derived from typed arena payload rows.
- `src/self_hosted/codegen/input/ast_kind_usage_owner.pgy` -- statement-shape usage facts derived from typed arena kind rows.
- `src/self_hosted/codegen/input/ast_type_usage_owner.pgy` -- type-surface usage facts derived from typed arena type-name rows.
- `src/self_hosted/codegen/input/ast_usage_owner.pgy` -- runtime/header usage facts derived from expression/kind/type usage owner rows.
- `src/self_hosted/codegen/typed_ast_node_skeleton.pgy` -- typed AST arena
  payload contract, `NodeId` lookup facts, and migration target for retiring
  the AST text bridge.
- `src/self_hosted/codegen/run/codegen_run_owner.pgy` -- codegen CLI run boundary.
- `src/self_hosted/codegen/text/text_owner.pgy` -- text/token scanning primitives.
- `src/self_hosted/codegen/text/enum_literal_owner.pgy` -- payload-free enum literal projection facts.
- `src/self_hosted/codegen/text/expr_scan.pgy` -- expression text scanning.
- `src/self_hosted/codegen/text/expr_sequence_owner.pgy` -- top-level comma-separated expression sequence facts.
- `src/self_hosted/codegen/text/struct_literal_call_owner.pgy` -- struct literal call-envelope facts.
- `src/self_hosted/codegen/text/struct_literal_field_owner.pgy` -- struct literal field-name/value entry facts.
- `src/self_hosted/codegen/text/struct_field_access_owner.pgy` -- dotted member-access field spelling projection facts.
- `src/self_hosted/codegen/type_facts/type_env.pgy` -- type environment facts.
- `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy` -- self-host C ABI type spelling facts, including nominal struct type and empty parameter-list spelling.
- `src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy` -- self-host C collection runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy` -- self-host C host file/argv/process entrypoint runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy` -- self-host C math/random runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy` -- self-host C Option/Result runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy` -- self-host C string/text runtime symbol facts.
- `src/self_hosted/codegen/emission/expr_rewrite.pgy` -- expression rewrite/lowering.
- `src/self_hosted/codegen/emission/function_emit.pgy` -- function emission.
- `src/self_hosted/codegen/emission/literal_rewrite.pgy` -- source literal lowering.
- `src/self_hosted/codegen/emission/program_emit.pgy` -- program emission and prepasses.
- `src/self_hosted/codegen/emission/stmt_emit.pgy` -- statement emission.
- `src/self_hosted/codegen/emission/struct_value_emit.pgy` -- struct value emission.

## Fuzz

- `src/self_hosted/fuzz/backend_parity_generator/main.pgy` -- backend parity
  fuzz source-program construction and generator entrypoint.
- `src/self_hosted/fuzz/backend_parity_generator/manifest_owner.pgy` -- backend
  parity fuzz JSONL manifest and stdout summary shape.

## Tools

- `src/self_hosted/tools/air_graph_id_uniqueness/main.pgy` -- AIR graph node ID
  duplicate analysis over scanner-owned AIR graph facts.
- `src/self_hosted/tools/air_graph_id_uniqueness/report_owner.pgy` -- AIR graph
  ID uniqueness JSON schema, count rows, source rows, and finding shapes.
- `src/self_hosted/tools/ast_read_surface_checker/main.pgy` -- AST read surface
  ratchet parsing, live source counting, and checker entrypoint.
- `src/self_hosted/tools/ast_read_surface_checker/report_owner.pgy` -- AST read
  surface checker JSON schema, count rows, source rows, and finding shapes.
- `src/self_hosted/tools/backend_air_access_checker/main.pgy` -- backend AIR
  forbidden-token source scan and fail-closed self-test entrypoint.
- `src/self_hosted/tools/backend_air_access_checker/report_owner.pgy` --
  backend AIR access report schema, count rows, and finding shapes.
- `src/self_hosted/tools/backend_abi_layout_contract_checker/main.pgy` --
  backend ABI-layout required/forbidden source scan and fail-closed self-test
  entrypoint.
- `src/self_hosted/tools/backend_abi_layout_contract_checker/report_owner.pgy`
  -- backend ABI-layout contract report schema, count rows, and finding shapes.
- `src/self_hosted/tools/backend_emitter_contract_checker/main.pgy` --
  backend-emitter required/forbidden source scan and fail-closed self-test
  entrypoint.
- `src/self_hosted/tools/backend_emitter_contract_checker/report_owner.pgy` --
  backend-emitter contract report schema, count rows, and finding shapes.
- `src/self_hosted/tools/compatibility_evolution_checker/main.pgy` --
  compatibility row analysis, fail-closed self-test modes, and checker
  entrypoint.
- `src/self_hosted/tools/compatibility_evolution_checker/report_owner.pgy` --
  compatibility corpus report schema, count rows, and finding shapes.

## SEA

- `src/self_hosted/sea/execution_lane.pgy` -- typed
  `BoundaryCaptureFact` to `ExecutionLane` classifier mirror.
- `src/self_hosted/sea/lane_executor_contract_owner.pgy` -- runtime executor
  facade contract facts over `PgyLaneScheduler`.
- `src/self_hosted/sea/lane_executor_contract.pgy` -- runtime executor facade
  contract probe over the lane executor owner facts.

## Compiler World

- `src/self_hosted/compiler/world.pgy` -- `PgyCompilerWorld`, stage path
  manifest, and root compiler intent flow.
- `src/self_hosted/compiler/path_manifest_owner.pgy` -- self-host compiler
  source/test/parity path fact values.
- `src/self_hosted/compiler/stage_intents.pgy` -- derived compiler intent clusters.
- `src/self_hosted/compiler/target_capability_owner.pgy` -- target acceptance
  and fallback fact envelope for backend projections.
- `src/self_hosted/compiler/target_capability_manifest.pgy` -- runnable
  target-capability artifact projection over owner facts.
- `src/self_hosted/compiler/sandbox_capability_owner.pgy` -- sandbox
  capability and frame-budget fact envelope for interactive/runtime claims.
- `src/self_hosted/compiler/sandbox_capability_manifest.pgy` -- runnable
  sandbox capability/frame-budget artifact projection over owner facts.
- `src/self_hosted/compiler/backend_emitter_contract_owner.pgy` -- backend
  dumb-emitter required/forbidden fact-consumer rows.
- `src/self_hosted/compiler/backend_air_access_contract_owner.pgy` -- backend
  AIR verification-only forbidden-token and scan-boundary contract facts.
- `src/self_hosted/compiler/compatibility_evolution_owner.pgy` -- versioned
  compatibility surface, obsolete-migration metadata, and seed
  breaking-change corpus rows.
- `src/self_hosted/compiler/compatibility_evolution_manifest.pgy` -- runnable
  compatibility-evolution artifact projection over the owner rows.
- `src/self_hosted/compiler/air_evidence_owner.pgy` -- hard-rung AIR evidence
  fact vocabulary for intent/effect/authority/coordination.
- `src/self_hosted/compiler/artifact_zone_owner.pgy` -- comparable artifact
  kinds consumed by C/LLVM/self-hosted parity.
- `src/self_hosted/compiler/test_harness_owner.pgy` -- fixture/result row
  vocabulary for Pergyra-owned parity harness work.
- `src/self_hosted/compiler/test_harness_target_paths_owner.pgy` -- target
  acceptance artifact source and expected path suite.
- `src/self_hosted/compiler/test_harness_backend_contract_paths_owner.pgy` --
  backend-emitter contract checker source and expected path suite.
- `src/self_hosted/compiler/test_harness_comparator_paths_owner.pgy` --
  backend-output comparator source, expected verdict, and fixture path suite.
- `src/self_hosted/compiler/test_harness_backend_compare_paths_owner.pgy` --
  backend compare smoke and extended fixture case suites.
- `src/self_hosted/compiler/test_harness_tool_paths_owner.pgy` -- shared
  parity tool/input path suites consumed by test harness manifests.
- `src/self_hosted/compiler/test_harness_inventory_paths_owner.pgy` --
  inventory checker path and negative-finding rows consumed by TestHarness.
- `src/self_hosted/compiler/test_harness_size_paths_owner.pgy` -- production
  size checker path and over-cap finding rows consumed by TestHarness.
- `src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy` -- AIR
  graph consumer path suites consumed by parity runners.
- `src/self_hosted/compiler/test_harness_driver_paths_owner.pgy` -- DRV-0/DRV-1
  driver, parser, and codegen source path suites consumed by parity runners.
- `src/self_hosted/compiler/test_harness_codegen_paths_owner.pgy` -- codegen
  parity tool, parser, comparator, fixture, and expected-output path suites.
- `src/self_hosted/compiler/test_harness_parser_paths_owner.pgy` -- parser
  parity tool, comparator, fixture, and expected clean fixture path suites.
- `src/self_hosted/compiler/test_harness_semantic_paths_owner.pgy` -- semantic
  parity tool, comparator, fixture, diagnostic, and source-directory path
  suites.
- `src/self_hosted/compiler/test_harness_mir_json_paths_owner.pgy` -- MIR JSON
  mir-lower, codegen, and comparator source path suites.
- `src/self_hosted/compiler/test_harness_codegen_bootstrap_paths_owner.pgy` --
  fixed-point bootstrap source paths plus component/tool breadth rows.
- `src/self_hosted/compiler/test_harness_lsp_paths_owner.pgy` -- LSP
  diagnostics, transport, request, response, session, document, state, and
  hover path suites.
- `src/self_hosted/compiler/test_harness_manifest.pgy` -- runnable manifest
  projection over TestHarnessZone path and completeness suites.
- `src/self_hosted/compiler/subprocess_runner_owner.pgy` -- capability envelope
  for oracle subprocess execution without raw shell escape.
- `src/self_hosted/compiler/completeness_ledger_owner.pgy` -- M2 source
  inventory, semantic target mapping, monotone stage-pass minima, incremental
  cache facts, and rung0 impact-plan rows.
- `src/self_hosted/compiler/abi_layout_row_owner.pgy` -- cross-backend ABI row
  fact vocabulary for field order, niche, tags, ownership, and layout.
- `src/self_hosted/compiler/abi_layout_target_policy_owner.pgy` -- ABI layout
  target projection and fallback-policy facts.
- `src/self_hosted/compiler/abi_layout_row_manifest.pgy` -- runnable ABI row
  projection over the ABI layout row owner for parity/golden comparison.
- `src/self_hosted/compiler/runtime_call_abi_row_owner.pgy` -- runtime helper
  and target-library call ABI row projection over the runtime ABI owners.
- `src/self_hosted/compiler/runtime_call_abi_row_manifest.pgy` -- runnable
  runtime call ABI row projection for parity/golden comparison.
- `src/self_hosted/compiler/symbol_table_owner.pgy` -- cross-backend symbol row
  fact vocabulary for C/LLVM/self-hosted projections, including struct field,
  source-to-C binding, inout parameter, foreach loop temporary, and try/match
  emission temporary spelling.
- `src/self_hosted/codegen/emission/expr_binding_rewrite_owner.pgy` -- local,
  parameter, and loop source-reference rewrite from `type_env` `cbind` rows.
- `src/self_hosted/compiler/stage_artifact_owner.pgy` -- stage artifact
  envelope facts that bind token, AST, semantic, and MIR stage actors to the
  compiler-world path manifest rows.
- `src/self_hosted/compiler/driver_rung0_owner.pgy` -- DRV-0 in-process
  assembly owner that composes self-parser AST text and self-codegen C emission
  after consuming compiler-world readiness facts.
- `src/self_hosted/compiler/driver_rung0_main.pgy` -- DRV-0 runnable artifact
  boundary; ownership remains in `driver_rung0_owner.pgy`.
- `src/self_hosted/compiler/driver_cli_owner.pgy` -- DRV-1 CLI surface owner
  for source path, artifact mode, and optional output path.
- `src/self_hosted/compiler/driver_rung1_main.pgy` -- DRV-1 runnable artifact
  boundary; ownership remains in `driver_cli_owner.pgy`.
- `src/self_hosted/compiler/authority_owner.pgy` -- authority contracts
  (abilities + roles) for the sensitive compiler-world boundaries: semantic
  verdict, C emission, subprocess planning, and parity judgement.

## LSP

- `src/self_hosted/lsp/main.pgy` -- LSP-0 runnable artifact boundary.
- `src/self_hosted/lsp/diagnostics_owner.pgy` -- semantic diagnostic block to
  `publishDiagnostics` JSON payload projection.
- `src/self_hosted/lsp/document_store_owner.pgy` -- LSP-2f buffered
  didOpen/didChange multi-document state projection.
- `src/self_hosted/lsp/feature_owner.pgy` -- LSP-2g advertised textDocument
  no-index response shapes.
- `src/self_hosted/lsp/squiggle_owner.pgy` -- LSP-1 diagnostic code/fact to
  squiggle-class policy.
- `src/self_hosted/lsp/transport_owner.pgy` -- LSP-2a single JSON-RPC
  Content-Length frame boundary and LSP-2b buffered stream consumption over the
  byte-count stdin substrate.
- `src/self_hosted/lsp/request_owner.pgy` -- LSP-2c buffered JSON-RPC request
  dispatch plan over transport bodies and JSON fact tables.
- `src/self_hosted/lsp/response_owner.pgy` -- LSP-2d buffered response body and
  Content-Length frame emission plan over request bodies.
- `src/self_hosted/lsp/session_owner.pgy` -- LSP-2e buffered session replay
  over transport bodies and response frames.
- `src/self_hosted/lsp/session_state_owner.pgy` -- LSP-2h buffered session
  state projection over response replay plus document-store facts.
- `src/self_hosted/lsp/hover_content_owner.pgy` -- LSP-2i bounded hover
  content over buffered document snapshots and hover requests.
