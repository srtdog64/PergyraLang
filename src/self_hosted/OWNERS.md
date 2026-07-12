# Self-Hosted Owner Manifest

This file is the source-of-truth map for active self-hosted compiler-stage
modules. It prevents the track from collapsing back into "one directory plus a
large `main.pgy`". Each active `.pgy` file below must own one clear stage
responsibility. `main.pgy` files are entrypoints only: argv wiring,
orchestration, and no semantic decisions.

`tests/self_hosted_component_contract_smoke.sh` requires every active
compiler-stage `.pgy` source to be listed here.

This file inventories physical Pergyra modules. Top-level semantic authority is
defined separately by `docs/semantics/sot_owner_spine_registry.md`; this module
inventory must not become a second fact-family owner registry.

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
- `src/self_hosted/parser/decl_enum_owner.pgy` -- enum declarations and
  canonical variant parameter-type preservation.
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
- `src/self_hosted/semantic/diagnostic_owner.pgy` -- structured semantic
  diagnostic rendering, vocabulary, fixture manifest, and audit facts.
- `src/self_hosted/semantic/diagnostic_contract_owner.pgy` -- executable
  payload-status and diagnostic-vocabulary completeness contract.
- `src/self_hosted/semantic/ast_artifact_verdict_owner.pgy` -- semantic
  evidence derived directly from the shared parser-owned `AstTreeArtifact`.
- `src/self_hosted/semantic/ast_signature_fact_owner.pgy` -- artifact-bound
  function owner, name, parameter, mode, and return signature facts.
- `src/self_hosted/semantic/ast_signature_contract_owner.pgy` -- executable
  freshness, duplicate-row, owner, and runtime-callability contract for those
  signature facts.
- `src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy` --
  artifact-bound nominal constructor name, return type, and ordered field-type
  rows consumed by expression typing; source constructor scans are forbidden.
- `src/self_hosted/semantic/ast_local_binding_fact_owner.pgy` -- artifact-bound
  local binding node, function, scope, name, declared-type, and initializer
  payload facts, including array-literal body and try-operand shape rows.
- `src/self_hosted/semantic/try_expression_fact_owner.pgy` -- canonical prefix,
  wrapped, and postfix try-expression shape and operand bounds.
- `src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy` -- artifact-
  native initializer expression type verdicts joined from signature, scope,
  local-binding, and initializer payload facts without source re-scanning.
- `src/self_hosted/semantic/ast_expression_environment_owner.pgy` -- shared
  artifact-native function, parameter, visible-local, and lexical scope
  environment construction for expression verdict owners.
- `src/self_hosted/semantic/ast_assignment_fact_owner.pgy` -- artifact-bound
  assignment node, function, scope, target/base/index, and RHS payload facts.
- `src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy` -- fail-closed
  assignment type verdicts joined from assignment, initializer, signature,
  and lexical environment facts.
- `src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy` -- fail-closed
  range/foreach header verdicts and lexical loop-binding type facts.
- `src/self_hosted/semantic/ast_statement_fact_owner.pgy` -- artifact-bound
  return, condition, loop, log, exit, match, array-pop, and bare-call payload
  rows.
- `src/self_hosted/semantic/ast_expression_verdict_owner.pgy` -- ordered call,
  undefined-use, try, logical, binary, and inferred-type expression verdicts.
- `src/self_hosted/semantic/ast_statement_type_fact_owner.pgy` -- fail-closed
  return, condition, call, and statement expression type verdict rows.
- `src/self_hosted/semantic/ast_body_verdict_owner.pgy` -- document-order body
  verdict across initializer, iteration, assignment, and statement owners.
- `src/self_hosted/semantic/ast_type_name_canonical_owner.pgy` -- canonical
  semantic type names at signature/local artifact capture boundaries.
- `src/self_hosted/semantic/body_check_owner.pgy` -- statement/body checks.
- `src/self_hosted/semantic/builtin_signature_owner.pgy` -- canonical builtin
  name, return-type, and parameter-type rows shared by source and artifact
  semantic paths.
- `src/self_hosted/semantic/call_check_owner.pgy` -- call arity and argument checks.
- `src/self_hosted/semantic/callable_resolution_owner.pgy` -- exact-first,
  unique namespace-local callable resolution.
- `src/self_hosted/semantic/delimited_range_fact_owner.pgy` -- trimmed nested
  comma and flat signature ranges shared by call and type facts.
- `src/self_hosted/semantic/array_type_owner.pgy` -- canonical `Array<T>`
  element and direct index-access facts.
- `src/self_hosted/semantic/ast_enum_fact_owner.pgy` -- typed-arena enum
  declaration and variant facts for expression typing.
- `src/self_hosted/semantic/ast_role_fact_owner.pgy` -- artifact-bound role
  name, target type, and owned method `NodeId` rows.
- `src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy` --
  artifact-bound atom/value/auxiliary expression surfaces and string-safe
  call/token queries consumed by runtime projection.
- `src/self_hosted/semantic/ast_type_surface_fact_owner.pgy` -- canonical
  artifact type-name rows consumed by runtime projection.
- `src/self_hosted/semantic/projection_type_owner.pgy` -- nominal member,
  array-index, and contextual array-literal types from owner facts.
- `src/self_hosted/semantic/diagnostic_code_owner.pgy` -- stable semantic diagnostic code vocabulary.
- `src/self_hosted/semantic/diagnostic_owner.pgy` -- semantic diagnostic blocks
  and verdict payload contract facts.
- `src/self_hosted/semantic/env_owner.pgy` -- scoped local environment.
- `src/self_hosted/semantic/expression_normalization_owner.pgy` -- semantic
  expression wrapper normalization shared before type and validation facts.
- `src/self_hosted/semantic/expression_operator_fact_owner.pgy` -- one
  string/parenthesis-aware top-level operator-position fact consumed by typing
  and logical/binary diagnostics.
- `src/self_hosted/semantic/expr_type_owner.pgy` -- expression type facts.
- `src/self_hosted/semantic/expr_validation_owner.pgy` -- expression validation facts.
- `src/self_hosted/semantic/program_check_owner.pgy` -- program/function signature checks.
- `src/self_hosted/semantic/semantic_run_owner.pgy` -- semantic CLI run boundary.
- `src/self_hosted/semantic/source_bundle_owner.pgy` -- root/import source bundle.
- `src/self_hosted/semantic/text_scan_owner.pgy` -- semantic text scanning.

## Shared Source Scan

- `src/self_hosted/lib/source_scan_owner.pgy` -- allocation-free source byte,
  character-class, exact-window, and whitespace/comment traversal facts
  consumed by parser and semantic. Their distinct keyword/identifier cursor
  policies remain in stage owners; String-returning character access is a
  compatibility surface, not the hot scan path.

## HIR

- `src/self_hosted/hir/ast_node_kind_owner.pgy` -- canonical compact AST/HIR
  node-kind tags consumed by inventory, semantic, and codegen views.
- `src/self_hosted/hir/ast_text_scan_owner.pgy` -- compact AST-text scanning
  primitives shared by parser/HIR and codegen.
- `src/self_hosted/hir/ast_text_row_fact_owner.pgy` -- compact AST text
  name/type/value/aux-value/mode row facts.
- `src/self_hosted/hir/ast_text_inventory_owner.pgy` -- compact AST text line
  inventory and cursor expectation boundary.
- `src/self_hosted/hir/ast_text_arena_projection_owner.pgy` -- single
  `AstTreeArtifact` construction and compact inventory to arena projection.
- `src/self_hosted/hir/typed_ast_arena_owner.pgy` -- shared typed AST arena
  payload contract and `NodeId` lookup facts.

## MIR Producer

- `src/self_hosted/mir/program_fact_owner.pgy` -- flat declaration, routine,
  block, instruction, source-local, and use-row ownership.
- `src/self_hosted/mir/expression_fact_owner.pgy` -- expression identifier-use
  and source-shape classification for MIR facts.
- `src/self_hosted/mir/routine_input_owner.pgy` -- immutable typed-artifact and
  semantic-fact input bundle consumed by routine lowering.
- `src/self_hosted/mir/routine_build_owner.pgy` -- routine CFG build state,
  block edges, instruction IDs, termination, and local SSA version updates.
- `src/self_hosted/mir/routine_lower_owner.pgy` -- bounded typed-artifact CFG
  lowering as one value-state transformer with explicit loop/branch topology.
- `src/self_hosted/mir/artifact_lower_owner.pgy` -- program assembly and
  deterministic instruction-ID canonicalization.
- `src/self_hosted/mir/program_verify_owner.pgy` -- MIR row range/topology and
  required-fact verification.
- `src/self_hosted/mir/json_projection_owner.pgy` -- verified `pgy.mir.v1`
  projection; it cannot read AST provenance.

## MIR Lower

- `src/self_hosted/mir_lower/main.pgy` -- entrypoint only.
- `src/self_hosted/mir_lower/decl_lower.pgy` -- declaration reconstruction.
- `src/self_hosted/mir_lower/error_owner.pgy` -- MIR-lower-specific
  `MirLowerFailClosed` diagnostic boundary; global `Die` aliases are forbidden.
- `src/self_hosted/mir_lower/fixture_manifest_owner.pgy` -- MIR parity
  source fixture manifest rows.
- `src/self_hosted/mir_lower/json_fact_read.pgy` -- bounded MIR JSON fact reads.
- `src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy` -- MIR fact
  graph payload contract facts.
- `src/self_hosted/mir_lower/mir_json_input_owner.pgy` -- MIR JSON input boundary.
- `src/self_hosted/mir_lower/parallel_capture_fact_owner.pgy` -- sealed parallel
  capture boundary/kind/writer fact validation for MIR JSON input.
- `src/self_hosted/mir_lower/program_lower.pgy` -- document-order program assembly.
- `src/self_hosted/mir_lower/program_routine_index_owner.pgy` -- one
  document-order routine identity inventory shared by declaration and routine
  reconstruction.
- `src/self_hosted/mir_lower/run_owner.pgy` -- MIR-lower CLI run boundary and
  manifest mode selection.
- `src/self_hosted/mir_lower/routine_fact_index_owner.pgy` -- per-routine
  block, instruction, source-local, successor, backedge, and structural-merge
  facts consumed by recursive CFG reconstruction.
- `src/self_hosted/mir_lower/routine_inventory_owner.pgy` -- routine inventory facts.
- `src/self_hosted/mir_lower/routine_lower.pgy` -- routine CFG/body reconstruction.
- `src/self_hosted/mir_lower/stmt_render.pgy` -- instruction fact to AST text rendering.

## Codegen

- `src/self_hosted/codegen/main.pgy` -- entrypoint only.
- `src/self_hosted/codegen/input/ast_input_owner.pgy` -- AST path and read boundary.
- `src/self_hosted/codegen/input/ast_arena_codegen_view_owner.pgy` -- codegen-only fail-closed predicates over shared `AstArena` facts.
- `src/self_hosted/codegen/input/semantic_array_literal_codegen_view_owner.pgy` -- fail-closed projection of semantic-owned array-literal body facts into top-level emission items.
- `src/self_hosted/codegen/input/semantic_enum_codegen_view_owner.pgy` -- fail-closed projection of semantic enum names, ordered variants, and payload arity.
- `src/self_hosted/codegen/input/semantic_signature_codegen_view_owner.pgy` -- fail-closed codegen view over semantic function signature facts.
- `src/self_hosted/codegen/input/semantic_role_codegen_view_owner.pgy` --
  fail-closed role name, target-type, and method-identity projection from
  semantic role facts.
- `src/self_hosted/codegen/input/semantic_nominal_codegen_view_owner.pgy` --
  fail-closed codegen projection of semantic nominal names and ordered field
  name/type rows.
- `src/self_hosted/codegen/input/semantic_try_let_codegen_view_owner.pgy` -- fail-closed projection of semantic-owned try-let operand facts.
- `src/self_hosted/codegen/input/semantic_local_binding_codegen_view_owner.pgy` -- fail-closed codegen view over semantic local binding name/type facts.
- `src/self_hosted/codegen/input/semantic_assignment_codegen_view_owner.pgy` -- fail-closed codegen view over semantic assignment target/base/index/RHS facts.
- `src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy` -- fail-closed codegen view over semantic statement payload rows, including `For` loop and `ArrayPush` / `ArraySet` projections.
- `src/self_hosted/codegen/input/ast_expression_usage_owner.pgy` -- backend
  builtin-group vocabulary projected from semantic expression-surface facts.
- `src/self_hosted/codegen/input/ast_kind_usage_owner.pgy` -- statement-shape usage facts derived from typed arena kind rows.
- `src/self_hosted/codegen/input/ast_type_usage_owner.pgy` -- backend runtime
  type-family projection from semantic type-surface facts.
- `src/self_hosted/codegen/input/ast_usage_owner.pgy` -- runtime/header usage facts derived from expression/kind/type usage owner rows.
- `src/self_hosted/codegen/run/codegen_run_owner.pgy` -- codegen CLI run boundary.
- `src/self_hosted/codegen/text/text_owner.pgy` -- codegen expression scanning and unsupported-surface policy.
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
- `src/self_hosted/codegen/runtime_abi/text_builder_runtime_owner.pgy` -- self-host C Allocator/TextBuilder layout, lifecycle, and runtime-call projection facts.
- `src/self_hosted/codegen/emission/expr_rewrite.pgy` -- expression rewrite/lowering.
- `src/self_hosted/codegen/emission/runtime_call_rewrite_owner.pgy` --
  single-pass source builtin call recognition projected through runtime symbol
  owners; string literals remain opaque.
- `src/self_hosted/codegen/emission/array_value_emit_owner.pgy` -- expected-type array literal value emission.
- `src/self_hosted/codegen/emission/function_emit.pgy` -- function emission.
- `src/self_hosted/codegen/emission/literal_rewrite.pgy` -- source literal lowering.
- `src/self_hosted/codegen/emission/program_emit.pgy` -- program emission and prepasses.
- `src/self_hosted/codegen/emission/stmt_emit.pgy` -- statement emission.
- `src/self_hosted/codegen/emission/struct_value_emit.pgy` -- struct value emission.
- `src/self_hosted/codegen/emission/value_return_emit_owner.pgy` -- expected-type Option and return value emission.

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
- `src/self_hosted/tools/completeness_impact_planner/main.pgy` --
  completeness impact-row consumption and changed-path proof-gate planning.

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
- `src/self_hosted/compiler/test_harness_abi_paths_owner.pgy` -- ABI layout row
  and runtime-call ABI row path suites.
- `src/self_hosted/compiler/test_harness_execution_lane_paths_owner.pgy` -- SEA
  execution-lane parity source and golden path suites.
- `src/self_hosted/compiler/test_harness_compatibility_paths_owner.pgy` --
  compatibility evolution manifest and corpus checker path suites.
- `src/self_hosted/compiler/test_harness_linter_paths_owner.pgy` -- linter
  parity source, expected diagnostics, and fixture path suite.
- `src/self_hosted/compiler/test_harness_doc_link_paths_owner.pgy` -- doc-link
  checker clean and dead-link fixture path suite.
- `src/self_hosted/compiler/test_harness_inventory_paths_owner.pgy` --
  inventory checker path and negative-finding rows consumed by TestHarness.
- `src/self_hosted/compiler/test_harness_size_paths_owner.pgy` -- production
  size checker path and over-cap finding rows consumed by TestHarness.
- `src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy` -- AIR
  graph consumer path suites consumed by parity runners.
- `src/self_hosted/compiler/test_harness_diagnostic_paths_owner.pgy` --
  diagnostic catalog checker source, expected artifact, and oracle path suites.
- `src/self_hosted/compiler/test_harness_ast_surface_paths_owner.pgy` --
  AST/source-read surface checker source, ratchet, and growth fixture paths.
- `src/self_hosted/compiler/test_harness_lexer_paths_owner.pgy` -- lexer
  parity tool, comparator, and fixture-directory path suite.
- `src/self_hosted/compiler/test_harness_runtime_boundary_paths_owner.pgy` --
  runtime-boundary checker source, expected artifact, and missing-term path
  suite.
- `src/self_hosted/compiler/test_harness_stable_subset_paths_owner.pgy` --
  stable-subset checker source, expected artifact, input manifest, and
  missing-section path suite.
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
  cache facts, and completeness readiness.
- `src/self_hosted/compiler/completeness_impact_owner.pgy` -- rung0
  changed-path impact plan rows, proof-gate grouping, and impact planner path
  manifest facts.
- `src/self_hosted/compiler/incremental_fact_graph_owner.pgy` -- compiler-scale
  incremental fact graph schema, dependency axes, reusable artifact kinds, and
  clean/incremental verifier vocabulary. The current completeness cache remains
  rung0 and coarse; this owner is the contract for later precise invalidation.
- `src/self_hosted/compiler/abi_layout_row_owner.pgy` -- cross-backend ABI row
  fact vocabulary for field order, niche, tags, ownership, and layout.
- `src/self_hosted/compiler/backend_abi_layout_contract_owner.pgy` -- backend
  ABI-layout required/forbidden source contract rows tied to the ABI row owner.
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
- `src/self_hosted/codegen/fixture_manifest_owner.pgy` -- committed codegen
  parity fixture frontier shared by codegen parity, MIR parity, and driver
  artifact rungs.
- `src/self_hosted/codegen/reject_fixture/enum_payload.pgy` -- TestHarness-owned
  negative codegen artifact paired with the committed payload-enum diagnostic;
  it proves unsupported payload arity fails closed under C/LLVM tool parity.
- `src/self_hosted/codegen/role_fixture/operator_add.pgy` -- TestHarness-owned
  positive role operator artifact proving role target and method identity rows
  through C/LLVM codegen parity.
- `src/self_hosted/codegen/emission/expr_binding_rewrite_owner.pgy` -- local,
  parameter, and loop source-reference rewrite from `type_env` `cbind` rows.
- `src/self_hosted/compiler/stage_artifact_owner.pgy` -- stage artifact
  envelope facts that bind token, AST, semantic, and MIR stage actors to the
  compiler-world path manifest rows.
- `src/self_hosted/compiler/driver_pipeline_owner.pgy` -- executable
  source-to-AST-to-C pipeline shared by the user-facing driver and bootstrap;
  this is the single owner of parser/codegen composition.
- `src/self_hosted/compiler/driver_bootstrap_main.pgy` -- minimal runnable
  source/output-file boundary used to prove the integrated driver fixed point;
  pipeline ownership remains in `driver_pipeline_owner.pgy`.
- `src/self_hosted/compiler/driver_rung0_owner.pgy` -- DRV-0 in-process
  assembly owner that composes self-parser AST text and self-codegen C emission
  after consuming compiler-world readiness facts.
- `src/self_hosted/compiler/driver_rung0_main.pgy` -- DRV-0 runnable artifact
  boundary; ownership remains in `driver_rung0_owner.pgy`.
- `src/self_hosted/compiler/driver_cli_owner.pgy` -- DRV-1 CLI surface owner
  for source path, artifact mode, and optional output path.
- `src/self_hosted/compiler/driver_rung1_main.pgy` -- DRV-1 runnable artifact
  boundary; ownership remains in `driver_cli_owner.pgy`.
- `src/self_hosted/compiler/driver_rung2_owner.pgy` -- hard artifact-body
  semantic source/MIR-to-C owner; joins initializer, iteration, assignment,
  expression-use, call, return, and condition evidence after source and MIR
  JSON inputs converge on one `AstTreeArtifact` verifier.
- `src/self_hosted/compiler/driver_rung2_main.pgy` -- DRV-2 runnable hard
  semantic boundary; ownership remains in `driver_rung2_owner.pgy`.
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
