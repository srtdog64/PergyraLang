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
- `src/self_hosted/lib/mir_decl_field_kind_vocabulary_projection_owner.pgy` --
  generated stable MIR declaration-field wire spelling and AST-label projection.
- `src/self_hosted/lib/nominal_field_kind_owner.pgy` -- declaration-family
  compatibility and shape policy over the generated field-kind vocabulary.

## Lexer

- `src/self_hosted/lexer/main.pgy` -- entrypoint only.
- `src/self_hosted/lexer/char_owner.pgy` -- character/codepoint predicates.
- `src/self_hosted/lexer/fixture_manifest_owner.pgy` -- lexer parity
  source/fixture manifest rows.
- `src/self_hosted/lexer/language_keyword_registry_projection_owner.pgy` --
  generated compatibility import hub and cross-projection readiness; the
  existing consumer import path remains stable.
- `src/self_hosted/lexer/language_word_identity_projection_owner.pgy` --
  generated stable `LanguageWordId.Word*`, spelling, and length projection.
- `src/self_hosted/lexer/language_word_index_projection_owner.pgy` --
  generated ordered spelling and debug-identity index projection.
- `src/self_hosted/lexer/language_word_class_projection_owner.pgy` --
  generated lexical class metadata projection.
- `src/self_hosted/lexer/language_word_axis_projection_owner.pgy` --
  generated language-axis metadata projection.
- `src/self_hosted/lexer/language_word_semantic_projection_owner.pgy` --
  generated grammar-context and implementation-support projection.
- `src/self_hosted/lexer/language_word_tooling_projection_owner.pgy` --
  generated tooling, TextMate scope, and completion projection.
- `src/self_hosted/lexer/language_keyword_compatibility_projection_owner.pgy` --
  generated 71-row reserved lexer compatibility view. All generated lexer
  projections derive from `src/lexer/language_keyword_registry.def`, which
  remains the single language-word authority.
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
- `src/self_hosted/parser/domain_projection_map_owner.pgy` -- exact parser
  syntax carriage for projection `target <- source` map entries; semantic
  field identity is deliberately not resolved here.
- `src/self_hosted/parser/decl_enum_owner.pgy` -- enum declarations and
  canonical variant parameter-type preservation.
- `src/self_hosted/parser/decl_event_owner.pgy` -- event declarations.
- `src/self_hosted/parser/decl_intent_owner.pgy` -- intent declarations.
- `src/self_hosted/parser/decl_nominal_owner.pgy` -- class/subject/object/tobject/vessel declarations.
- `src/self_hosted/parser/decl_role_owner.pgy` -- role declarations.
- `src/self_hosted/parser/decl_type_owner.pgy` -- type declarations.
- `src/self_hosted/parser/decl_zone_owner.pgy` -- zone declarations.
- `src/self_hosted/parser/error_owner.pgy` -- parser diagnostic strings.
- `src/self_hosted/parser/generic_parameter_list_owner.pgy` -- declaration-site
  generic parameter/default type list parsing shared by functions, nominals,
  and abilities; malformed lists fail closed.
- `src/self_hosted/parser/expr_owner.pgy` -- expression grammar import boundary.
- `src/self_hosted/parser/expression_fact_owner.pgy` -- canonical parser
  expression result plus unclassified leaf construction.
- `src/self_hosted/parser/expression_scalar_fact_owner.pgy` -- scalar literal
  graph construction; rendered text and literal type identity remain separate
  facts.
- `src/self_hosted/parser/expression_graph_owner.pgy` -- parser-owned
  expression node/edge construction, verified subtree extraction, and
  statement-lane root accumulation.
- `src/self_hosted/parser/expression_set_literal_graph_owner.pgy` -- distinct
  Set-literal graph construction and ordered element edges.
- `src/self_hosted/parser/expression_set_literal_contract_owner.pgy` --
  executable non-empty/empty Set-literal parser-spine readiness contract.
- `src/self_hosted/parser/expression_generic_actual_owner.pgy` -- ordered
  explicit generic actual nodes and generic-callee spine construction.
- `src/self_hosted/parser/expr_postfix_owner.pgy` -- postfix call/index/try
  parsing, including canonical `AstExpressionNodeTry` ownership and its operand
  edge.
- `src/self_hosted/parser/expr_precedence_owner.pgy` -- precedence expression parsing.
- `src/self_hosted/parser/expr_primary_owner.pgy` -- primary expression parsing.
- `src/self_hosted/parser/expr_string_owner.pgy` -- string literal and
  interpolation graph construction; interpolation carries `Add` and `Call`
  nodes instead of a desugared text leaf.
- `src/self_hosted/parser/fixture_manifest_owner.pgy` -- parser parity
  source/fixture manifest rows.
- `src/self_hosted/parser/fixture/language_word_roles.pgy` -- positive parser
  fixture for registry-owned action/type/impl/ref/own roles.
- `src/self_hosted/parser/reject_fixture/systemic_slot.pgy` -- negative parser
  fixture proving unregistered `systemic` does not become a language word.
- `src/self_hosted/parser/function_decl_owner.pgy` -- function signatures and bodies.
- `src/self_hosted/parser/program_parse_owner.pgy` -- program-root assembly.
- `src/self_hosted/hir/ast_match_pattern_fact_owner.pgy` -- interprets the
  canonical typed `MatchCase` AST atom as one bounded pattern fact for semantic
  and MIR consumers; no parallel match-pattern graph may own the same identity.
- `src/self_hosted/parser/run_owner.pgy` -- parser CLI run boundary and mode
  selection.
- `src/self_hosted/parser/source_path_owner.pgy` -- source path/default and import read input.
- `src/self_hosted/parser/stmt_if_owner.pgy` -- if/if-let statements.
- `src/self_hosted/parser/stmt_call_graph_owner.pgy` -- parser-owned standalone
  call-statement graph classification, including member-call identity without
  dot-text recovery.
- `src/self_hosted/parser/stmt_collection_graph_owner.pgy` -- parser-owned
  `ArrayPush` value and `ArraySet` index/value expression-graph roots.
- `src/self_hosted/parser/stmt_destructure_owner.pgy` -- destructuring-let
  pattern and initializer graph production.
- `src/self_hosted/parser/stmt_loop_owner.pgy` -- loop statements.
- `src/self_hosted/parser/stmt_match_owner.pgy` -- match statements and their
  parser-owned scrutinee Atom graph roots.
- `src/self_hosted/parser/stmt_owner.pgy` -- statement dispatch.
- `src/self_hosted/parser/stmt_parallel_owner.pgy` -- parallel/async statements.
- `src/self_hosted/parser/tree_text_owner.pgy` -- compact AST text rendering
  and current AST payload contract consumed by `PgyCompilerWorld`.
- `src/self_hosted/parser/type_name_owner.pgy` -- written type-name parsing.

## Semantic

- `src/self_hosted/semantic/main.pgy` -- entrypoint only.
- `src/self_hosted/semantic/callable_receiver_carriage_policy_owner.pgy` --
  shared callable receiver-carriage vocabulary and nominal-kind policy consumed
  by semantic-derived codegen, MIR production, and machine admission.
- `src/self_hosted/semantic/domain_projection_assignability_owner.pgy` --
  canonical source-to-target projection assignability policy; MIR producers
  consume this verdict instead of owning a second type-compatibility table.
- `src/self_hosted/semantic/diagnostic_owner.pgy` -- structured semantic
  diagnostic rendering, vocabulary, fixture manifest, and audit facts.
- `src/self_hosted/semantic/diagnostic_contract_owner.pgy` -- executable
  payload-status and diagnostic-vocabulary completeness contract.
- `src/self_hosted/semantic/ast_artifact_verdict_owner.pgy` -- semantic
  evidence derived directly from the shared parser-owned `AstTreeArtifact`.
- `src/self_hosted/semantic/ast_signature_fact_owner.pgy` -- artifact-bound
  function owner, name, formal-generic, parameter, mode, and return signature
  facts, including ordered function node/name identity for entrypoint
  cardinality, selection, and top-level function declaration routing.
- `src/self_hosted/semantic/ast_action_contract_fact_owner.pgy` -- callable-
  identity-bound `func`/`action` variant, subject ownership, body handle,
  action-only `requires`/`within`/`causes`/`authorized by`, and callable
  caps/effects rows. Codegen and MIR consume this owner rather than skipping
  typed rows or inferring action identity from clauses.
- `src/self_hosted/semantic/ast_generic_parameter_fact_owner.pgy` -- typed
  generic-list node to ordered formal-parameter/default-type rows; nested type
  defaults are partitioned once by the delimited-range owner, and provenance
  parsing by expression consumers is forbidden.
- `src/self_hosted/semantic/ast_signature_type_expression_fact_owner.pgy` --
  one flat parameter/return type-expression arena captured with signature
  rows; generic call consumers unify and materialize nodes without reparsing
  source text.
- `src/self_hosted/semantic/ast_signature_artifact_match_owner.pgy` -- reverse
  artifact validation for signature rows; production and query ownership stays
  in the signature fact owner.
- `src/self_hosted/semantic/ast_signature_contract_owner.pgy` -- executable
  freshness, duplicate-row, owner, and runtime-callability contract for those
  signature facts.
- `src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy` --
  artifact-bound nominal constructor name, return type, and ordered effective
  field-type rows after generic-default substitution, consumed by expression
  typing and declaration routing; source constructor scans are forbidden.
- `src/self_hosted/semantic/nominal_constructor_argument_policy_owner.pgy` --
  semantic distinction between caller-supplied nominal constructor arguments
  and domain storage fields that require a topology/runtime materializer.
- `src/self_hosted/semantic/ast_local_binding_fact_owner.pgy` -- artifact-bound
  local binding node, function, scope, name, declared-type, initializer
  payload, and per-name ordinal facts, including array-literal body and `Let`
  statement-routing identity. MIR lexical binding identity must consume this
  owner; source-name-only SSA lookup is forbidden.
- `src/self_hosted/semantic/ast_destructure_binding_fact_owner.pgy` -- typed
  destructuring-let binding identity, scope, order, and initializer rows.
- `src/self_hosted/semantic/ast_function_scope_fact_owner.pgy` -- function
  scope interval rows consumed by local-binding and expression-environment
  owners; downstream consumers must not rescan function bodies.
- `src/self_hosted/semantic/try_expression_fact_owner.pgy` -- canonical prefix,
  wrapped, and postfix try-expression shape and operand bounds.
- `src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy` -- artifact-
  native initializer expression type verdicts joined from signature, scope,
  local-binding, and parser expression-graph facts without source re-scanning
  or projection-text recovery; declared List<T> sequence literals are
  contextualized here from the graph-owned element compatibility fact.
- `src/self_hosted/semantic/ast_initializer_environment_cursor_owner.pgy` --
  initializer-only sequential visibility cursor. Local identity, order, and
  scope remain owned by local-binding/typed-AST facts; this owner keeps the
  function base environment and active lexical-local suffix, publishes all
  bindings from one destructure node atomically, and removes row-by-row full
  function scans.
- `src/self_hosted/semantic/ast_initializer_type_function_table_bridge_owner.pgy`
  -- routes the shared callable-table fact into initializer base/refinement
  consumers without rebuilding it per pass.
- `src/self_hosted/semantic/ast_initializer_iteration_refinement_owner.pgy` --
  loop-body initializer refinement that consumes verified iteration binding
  facts after the header pass.
- `src/self_hosted/semantic/ast_expression_environment_owner.pgy` -- shared
  artifact-native function, parameter, visible-local, and lexical scope
  environment construction for expression verdict owners.
- `src/self_hosted/semantic/ast_expression_function_table_fact_owner.pgy` --
  shared immutable callable-table fact for body-analysis consumers; per-pass
  table rebuilding is forbidden.
- `src/self_hosted/semantic/ast_expression_owner_field_environment_owner.pgy`
  -- implicit method-field bindings derived from the function owner and
  nominal constructor field rows; source-text rewriting is forbidden.
- `src/self_hosted/semantic/ast_match_binding_environment_owner.pgy` --
  case-scoped `Option<T>` payload bindings derived from the canonical pattern
  fact and typed scrutinee graph.
- `src/self_hosted/semantic/ast_assignment_fact_owner.pgy` -- artifact-bound
  assignment node, function, scope, target/base/index, and RHS payload facts;
  assignment node identity also owns `Assign` statement routing.
- `src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy` -- fail-closed
  assignment type verdicts joined from assignment, initializer, signature,
  lexical environment, and parser expression-graph facts; target binding,
  member, collection-base, index, and RHS scalar types are graph-derived.
- `src/self_hosted/semantic/assignment_binding_mode_owner.pgy` -- canonical
  parameter-mode classification consumed by MIR assignment verification; the
  assignment type-fact owner remains the row owner.
- `src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy` -- fail-closed
  range/foreach header verdicts and lexical loop-binding type facts derived
  from parser-owned expression graph roots; non-identifier foreach iterables
  carry one explicit synthetic-hoist fact.
- `src/self_hosted/semantic/ast_iteration_graph_root_owner.pgy` -- requests
  HIR-owned topology extension for compiler-generated foreach collection
  locals, attaches semantic overlays, and owns their stable names/root handles;
  MIR may consume those handles but may not construct a sibling graph.
- `src/self_hosted/semantic/ast_statement_fact_owner.pgy` -- artifact-bound
  return, condition, loop, defer, break/continue, log, exit, match/default,
  array mutation, and bare-call kind/payload rows used for statement routing.
- `src/self_hosted/semantic/ast_expression_verdict_owner.pgy` -- ordered call,
  undefined-use, try, logical, binary, and graph-derived inferred-type
  expression verdicts, including owner-projected array-literal types.
- `src/self_hosted/semantic/ast_expression_graph_identifier_owner.pgy` --
  undefined-identifier evidence from parser graph node roles.
- `src/self_hosted/semantic/ast_expression_graph_call_view_owner.pgy` --
  canonical ordered callee/argument projection over parser-owned call spines;
  semantic and codegen consumers share this view.
- `src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy` --
  DRV-2 MIR fixture manifest rows and their count contract; the CLI consumes
  this owner for --mir-fixture-manifest while the driver owner keeps the
  compile/verify pipeline.
- `src/self_hosted/semantic/ast_body_type_bundle_contract_owner.pgy` --
  self-checking contract fixtures for the body-type bundle owner, consumed by
  driver readiness; the bundle owner keeps production and readiness checks.
- `src/self_hosted/semantic/ast_initializer_type_contract_owner.pgy` --
  self-checking contract fixture chain for the initializer-type fact owner,
  consumed by driver readiness; the fact owner keeps production/projection.
- `src/self_hosted/semantic/ast_expression_graph_collection_call_protocol_owner.pgy` --
  canonical collection call-name protocol shared by the family owners that
  validate graph facts and runtime ABI rows, so consumers cannot drift on
  operation spellings.
- `src/self_hosted/semantic/ast_contextual_builtin_type_owner.pgy` -- joins a
  graph-owned builtin call identity with its declared type context when the
  builtin signature alone cannot produce a concrete result type.
- `src/self_hosted/semantic/ast_expression_graph_array_literal_owner.pgy` --
  canonical ordered element projection, recursive homogeneous literal type,
  and declared-element compatibility over parser-owned array-literal spines;
  semantic iteration/initializer typing and codegen share this view without
  bracket trimming or argument splitting.
- `src/self_hosted/semantic/ast_expression_graph_set_literal_owner.pgy` --
  canonical Set-literal spine projection, homogeneous element typing, and
  declared `Set<T>` compatibility; empty literals remain contextual facts and
  do not guess an element ABI.
- `src/self_hosted/semantic/ast_expression_graph_member_view_owner.pgy` --
  canonical receiver/member handle projection over parser-owned member-access
  nodes; semantic and codegen consumers share this view.
- `src/self_hosted/semantic/ast_expression_graph_resolved_call_type_owner.pgy`
  -- canonical return-type projection from graph-owned direct, namespace, and
  receiver-bound call targets, the bounded `List<T>` call protocol over
  carried target/local-type facts, and the explicit concrete-scalar capability
  filter used by scalar validation.
- `src/self_hosted/semantic/ast_expression_graph_queue_call_owner.pgy` --
  canonical `Queue<T>` direct-call receiver, element, arity, argument, and
  return-type verdicts from graph-owned target and local-type facts.
- `src/self_hosted/semantic/ast_expression_graph_set_call_owner.pgy` --
  canonical `Set<T>` direct-call receiver, element, arity, argument, and
  return-type verdicts from graph-owned target and local-type facts.
- `src/self_hosted/semantic/ast_expression_graph_generic_call_owner.pgy` --
  exact and nested generic argument binding plus structured return
  substitution from one signature type-expression arena and graph argument
  handles, including owner-directed projection through `spawn`; source-text
  inference is forbidden.
- `src/self_hosted/semantic/ast_expression_graph_concrete_scalar_verdict_owner.pgy`
  -- capability, arity, operand, and argument-type verdicts for graph-owned
  scalar trees composed from leaves, operators, and concrete direct calls.
- `src/self_hosted/semantic/ast_expression_graph_struct_view_owner.pgy` --
  canonical nominal type, field-name, and value-handle projection over
  parser-owned struct literal spines; semantic and codegen share this view.
- `src/self_hosted/semantic/ast_expression_graph_struct_type_verdict_owner.pgy`
  -- nominal constructor field/cardinality/type verdicts over that graph view.
- `src/self_hosted/semantic/ast_expression_graph_field_type_owner.pgy` --
  graph-only aggregate field value type and assignability projection, including
  structural integer-literal widening.
- `src/self_hosted/semantic/ast_expression_graph_type_owner.pgy` -- intrinsic
  nominal result types carried by parser-owned expression graph nodes.
- `src/self_hosted/semantic/ast_expression_graph_scalar_type_owner.pgy` --
  scalar result-type projection from parser-owned expression node handles.
- `src/self_hosted/semantic/ast_expression_graph_scalar_shape_owner.pgy` --
  scalar graph shape and cast/operator ownership facts shared by type and
  verdict projections.
- `src/self_hosted/semantic/result_call_type_owner.pgy` -- Result constructor
  and projection type facts for the bounded source-text semantic lane.
- `src/self_hosted/semantic/ast_expression_graph_wrapper_value_owner.pgy` --
  graph-only Option/Result builtin type and diagnostic facts; carried call
  targets and typed signature rows are mandatory for the covered scalar lane.
- `src/self_hosted/semantic/ast_expression_graph_collection_mutation_owner.pgy`
  -- graph call-target and receiver projection for collection mutation policy;
  source argument text is not a semantic fallback.
- `src/self_hosted/semantic/ast_expression_graph_scalar_verdict_owner.pgy` --
  operand diagnostics for fully graph-owned scalar operator trees.
- `src/self_hosted/semantic/ast_expression_graph_view_owner.pgy` -- borrowed
  expression graph root handles over artifact-bound semantic surface facts.
- `src/self_hosted/semantic/ast_statement_type_fact_owner.pgy` -- fail-closed
  return, condition, call, match-scrutinee, and statement expression type
  verdict rows; graph-owned statement expressions cannot reopen projection
  text.
- `src/self_hosted/semantic/ast_bind_statement_type_fact_owner.pgy` --
  fail-closed party role-slot bind verdict from nominal slot, visible local,
  and role implementation facts; missing or mismatched facts are not guessed.
- `src/self_hosted/semantic/ast_statement_type_contract_owner.pgy` -- executable
  statement-type contracts, including graph-owned `Exit(Int)`, collection
  mutation, and match-scrutinee validation.
- `src/self_hosted/semantic/ast_statement_type_query_owner.pgy` -- stable
  node-handle lookup for verified statement result-type rows.
- `src/self_hosted/semantic/ast_body_verdict_owner.pgy` -- document-order body
  verdict across initializer, iteration, assignment, and statement owners.
- `src/self_hosted/semantic/ast_body_type_bundle_owner.pgy` -- canonical
  one-pass assembly of initializer, iteration, assignment, and statement type
  facts plus readiness diagnostics consumed by driver and codegen projections.
- `src/self_hosted/semantic/ast_body_call_target_resolution_owner.pgy` --
  body-fixpoint resolution of canonical expression call-target rows.
- `src/self_hosted/semantic/ast_expression_place_fact_owner.pgy` --
  body-fixpoint value-category and place-kind rows for ref/inout argument
  lowering; codegen consumes the carried node fact without binding lookup.
- `src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy` --
  semantic-owned direct generic call bindings keyed by expression call node;
  explicit calls and bounded inferred initializer calls share these rows.
- `src/self_hosted/semantic/ast_expression_call_identity_owner.pgy` -- stable
  statement SyntaxNodeId, expression lane, and local-call ordinal identity for
  semantic call rows that cross into MIR; global graph indexes are not IDs.
- `src/self_hosted/semantic/ast_type_name_canonical_owner.pgy` -- canonical
  semantic type names at signature/local artifact capture boundaries.
- `src/self_hosted/semantic/body_check_owner.pgy` -- statement/body checks.
- `src/self_hosted/semantic/builtin_signature_owner.pgy` -- canonical builtin
  name, return-type, and parameter-type rows shared by source and artifact
  semantic paths; stable aliases append rows without shifting existing builtin
  identity.
- `src/self_hosted/semantic/call_check_owner.pgy` -- call arity and argument checks.
- `src/self_hosted/semantic/callable_resolution_owner.pgy` -- exact-first,
  unique namespace-local callable resolution.
- `src/self_hosted/semantic/delimited_range_fact_owner.pgy` -- trimmed nested
  comma and flat signature ranges shared by call and type facts.
- `src/self_hosted/semantic/array_type_owner.pgy` -- canonical `Array<T>`
  direct index-access verdicts.
- `src/self_hosted/semantic/array_type_shape_owner.pgy` -- dependency-light
  canonical sequence element projection for Array/Slice/List/Queue shared by
  semantic and codegen views.
- `src/self_hosted/semantic/tuple_type_shape_owner.pgy` -- canonical positional
  tuple arity and element-type projection consumed by destructure initializer
  typing; source-expression recovery and array-only fallback are forbidden.
- `src/self_hosted/semantic/ast_enum_fact_owner.pgy` -- typed-arena enum
  declaration identity and variant facts for expression typing and codegen
  routing.
- `src/self_hosted/semantic/ast_role_fact_owner.pgy` -- artifact-bound role
  declaration identity, name, target type, ability generic constraint rows,
  and owned method `NodeId` rows.
- `src/self_hosted/semantic/ast_ability_generic_bound_verdict_owner.pgy` --
  ordered ability-bound validation for canonical generic defaults against
  role implementation facts; it does not re-read source text.
- `src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy` --
  artifact-bound atom/value/auxiliary expression surfaces, normalized
  top-level operator rows, and expression-graph bindings consumed by semantic
  and runtime projection.
- `src/self_hosted/semantic/ast_expression_surface_query_owner.pgy` --
  read-only, string-safe call/token queries over canonical expression surface
  rows; this is a consumer and not a second surface-fact owner.
- `src/self_hosted/semantic/ast_expression_surface_contract_owner.pgy` --
  executable compact-bridge and expression-topology contract kept outside the
  production fact owner.
- `src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy` -- normalized
  expression node handles and child edges consumed by recursive semantic and
  codegen projections; compact-text production remains an explicit bridge for
  non-migrated expression owners and legacy/native canonicalization, not an
  alternate hard-codegen authority.
- `src/self_hosted/semantic/ast_expression_graph_lane_policy_owner.pgy` --
  expression-graph lane lifetime policy; required lanes and producer-only
  collection-mutation receiver lanes are declared here rather than inferred by
  semantic or MIR consumers.
- `src/self_hosted/semantic/ast_expression_graph_build_owner.pgy` -- compact
  bridge row construction. Recursive calls carry the six row arrays directly;
  compiler-scale graph aggregates may not cross an `inout` ABI boundary.
- `src/self_hosted/semantic/ast_expression_graph_bridge_contract_owner.pgy` --
  executable topology contracts for the temporary compact-text graph bridge.
- `src/self_hosted/semantic/ast_expression_call_target_fact_owner.pgy` --
  canonical direct, namespace, and receiver-bound call identity derived from
  callable, local-type, and nominal field facts; direct, namespace, and
  receiver targets are carried by self MIR and consumed by hard codegen.
- `src/self_hosted/semantic/ast_expression_call_target_capture_owner.pgy` --
  signature-only initial capture of direct and namespace call-target rows;
  body fixpoint resolution remains with the canonical target fact owner.
- `src/self_hosted/semantic/ast_expression_call_target_contract_owner.pgy` --
  executable positive and missing-target contract kept outside the production
  call-target owner.
- `src/self_hosted/semantic/ast_expression_graph_receiver_type_owner.pgy` --
  read-only receiver type projection over expression handles and canonical
  nominal field facts; dotted source text and codegen type rows are forbidden.
- `src/self_hosted/semantic/ast_expression_graph_enum_payload_owner.pgy` --
  semantic enum payload member type projection from receiver graph identity
  and enum variant payload facts; payload type guesses and source rescans are
  forbidden.
- `src/self_hosted/semantic/ast_expression_typed_binding_owner.pgy` -- binds
  parser/HIR `(owner kind, lane, root)` rows to semantic expression slots.
- `src/self_hosted/semantic/ast_type_surface_fact_owner.pgy` -- canonical
  artifact type-name rows consumed by runtime projection.
- `src/self_hosted/semantic/ast_kind_surface_fact_owner.pgy` -- canonical
  artifact node-kind rows consumed by runtime projection.
- `src/self_hosted/semantic/diagnostic_code_owner.pgy` -- stable semantic diagnostic code vocabulary.
- `src/self_hosted/semantic/diagnostic_owner.pgy` -- semantic diagnostic blocks
  and verdict payload contract facts.
- `src/self_hosted/semantic/env_owner.pgy` -- scoped local environment.
- `src/self_hosted/semantic/expression_normalization_owner.pgy` -- semantic
  expression wrapper normalization shared before type and validation facts.
- `src/self_hosted/semantic/expression_cast_fact_owner.pgy` -- parser-type-
  owned target projection for a source expression whose outer operation is a
  cast; arithmetic tails and string contents cannot be mistaken for casts.
- `src/self_hosted/semantic/expression_operator_fact_owner.pgy` -- one
  string/parenthesis-aware top-level operator-position fact consumed by typing
  and logical/binary diagnostics.
- `src/self_hosted/semantic/expr_type_owner.pgy` -- expression type facts.
- `src/self_hosted/semantic/wrapper_type_owner.pgy` -- canonical
  Option/Result/Box type-shape and payload projection policy shared by legacy
  and graph lanes; nested Box payloads require a balanced outer wrapper.
- `src/self_hosted/semantic/collection_mutation_policy_owner.pgy` -- canonical
  mutator, collection type, and parameter-mode policy shared by source,
  statement-fact, and expression-graph consumers.
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
- `src/self_hosted/hir/ast_expression_graph_owner.pgy` -- canonical expression
  graph arena, statement-lane root rows, and structural/reachability validation.
- `src/self_hosted/hir/ast_destructure_graph_owner.pgy` -- parser graph to
  typed destructure pattern and initializer artifact binding.
- `src/self_hosted/hir/ast_match_pattern_fact_owner.pgy` -- canonical bounded
  scalar and `Some(binding)`/`None` pattern facts shared by semantic and MIR.
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

## Focused Substitution Probes

- `src/self_hosted/tools/generic_return_probe/main.pgy` -- executable
  exact/nested and explicit generic parameter/return projection,
  carried-target mutation, ordered-actual conflict, and structural mismatch
  proof.
- `src/self_hosted/tools/wrapper_policy_probe/main.pgy` -- executable
  Option/Result graph-policy projection, native C oracle parity, and missing
  carried-target rejection proof.
- `src/self_hosted/tools/collection_policy_probe/main.pgy` -- executable
  specialized-statement and graph-call collection mutation policy proof.
- `src/self_hosted/tools/aggregate_field_policy_probe/main.pgy` -- executable
  aggregate field graph typing, type-drift, and missing-child-fact proof.

## MIR Producer

- `src/self_hosted/mir/program_fact_owner.pgy` -- flat routine, block,
  instruction, source-local, use-row, and assembled program ownership.
- `src/self_hosted/mir/declaration_fact_owner.pgy` -- flat declaration,
  generic parameter, method parameter, party role-slot, and role implementation
  row ownership shared by producer, verifier, JSON, and MIR lowering.
- `src/self_hosted/mir/declaration_verify_owner.pgy` -- structural range and
  parallel-row verification for MIR declarations, including generic, method,
  role-slot, and role-implementation inventories.
- `src/self_hosted/mir/declaration_json_projection_owner.pgy` -- verified MIR
  declaration projection to `pgy.mir.v1`, including generic parameters,
  method parameters, party role slots, and role implementation ranges.
- `src/self_hosted/mir/runtime_call_abi_fact_owner.pgy` -- instruction-aligned
  MIR resource runtime-call ABI facts, including auxiliary operations; missing
  producer-declared rows fail before JSON projection or backend consumption.
- `src/self_hosted/mir/routine_build_owner.pgy` -- routine-local resource
  operation type projection; Claim resolves its ABI type from the result SSA
  binding's carried local type, never from a duplicated expression-text row.
- `src/self_hosted/mir/expression_runtime_abi_owner.pgy` -- projects plain
  `Slot<T>` runtime-call requirements from carried expression-graph call facts
  and typed local bindings without reparsing source text.
- `src/self_hosted/mir/cfg_instruction_mutation_owner.pgy` -- canonical use-row
  replacement and runtime-call ABI attachment state transformations.
- `src/self_hosted/mir/expression_fact_owner.pgy` -- expression identifier-use
  and source-shape classification for MIR facts.
- `src/self_hosted/mir/expression_graph_fact_owner.pgy` -- instruction-owned
  expression graph root/range handles over the program-owned semantic graph.
  The bridge reads structural and call-target facts only through semantic
  accessors and fails closed on missing or foreign graph handles.
- `src/self_hosted/mir/match_fact_owner.pgy` -- sparse instruction-keyed match
  pattern, variant, and binding facts; the scalar rung requires one pattern.
- `src/self_hosted/mir/destructure_fact_owner.pgy` -- sparse instruction-keyed
  destructure element type and ordered binding facts.
- `src/self_hosted/mir/destructure_type_fact_owner.pgy` -- routine-level
  semantic destructure binding type rows joined from local-binding and
  initializer facts; source re-inference is forbidden.
- `src/self_hosted/mir/destructure_cfg_owner.pgy` -- typed destructure fact
  attachment to the owning MIR instruction row.
- `src/self_hosted/mir/destructure_json_projection_owner.pgy` -- final MIR
  JSON element-type and ordered-binding projection from destructure facts.
- `src/self_hosted/mir/destructure_type_json_projection_owner.pgy` -- final
  MIR JSON projection of routine-level semantic destructure type rows.
- `src/self_hosted/mir/match_json_projection_owner.pgy` -- final MIR JSON field
  projection for match facts.
- `src/self_hosted/mir/expression_graph_kind_name_owner.pgy` -- stable MIR JSON
  names for expression graph node kinds.
- `src/self_hosted/mir/generic_specialization_owner.pgy` -- MIR-owned direct
  and member generic specialization rows keyed by stable source-call identity,
  including target kind, formal, actual, and emitted-symbol carriage.
- `src/self_hosted/mir/generic_specialization_json_projection_owner.pgy` --
  JSON projection of the MIR-owned stable identity and specialization rows.
- `src/self_hosted/mir/routine_input_owner.pgy` -- immutable typed-artifact and
  semantic-fact input bundle consumed by routine lowering.
- `src/self_hosted/mir/routine_local_inventory_owner.pgy` -- complete
  routine-local source inventory projected from semantic binding, initializer,
  and iteration facts; active CFG stack state is not a replacement authority.
- `src/self_hosted/mir/routine_expression_use_owner.pgy` -- expression-graph
  leaf binding to SSA-use projection for migrated routine consumers; source
  text identifier scans remain a legacy bridge outside that migration.
- `src/self_hosted/mir/routine_build_owner.pgy` -- routine CFG build state,
  block edges, instruction IDs, termination, and binding-identity keyed local
  SSA inventory with lexical scope restoration.
- `src/self_hosted/mir/routine_lower_owner.pgy` -- bounded typed-artifact CFG
  lowering dispatcher over read-only compiler-scale input and mutable routine
  build state, including block-exit local-inventory restoration.
- `src/self_hosted/mir/routine_if_owner.pgy` -- conditional branch topology,
  branch-local build threading, and merge-block ownership.
- `src/self_hosted/mir/routine_match_owner.pgy` -- scalar case/default CFG
  topology plus arm exit/version carriage into the merge owner.
- `src/self_hosted/mir/routine_match_pattern_owner.pgy` -- MIR projection of
  the HIR-owned bounded pattern fact; source/payload recovery is forbidden.
- `src/self_hosted/mir/routine_match_merge_owner.pgy` -- N-way live-arm SSA
  phi emission and post-match continuation version ownership.
- `src/self_hosted/mir/routine_while_owner.pgy` -- while-loop header, body,
  back-edge, and exit-block lowering.
- `src/self_hosted/mir/loop_reachability_fact_owner.pgy` -- loop-body exit and
  back-edge reachability facts consumed before header phi emission.
- `src/self_hosted/mir/routine_for_owner.pgy` -- typed iteration row and
  semantic source/branch graph views to loop-initializer, body, back-edge, and
  exit-block lowering.
- `src/self_hosted/mir/routine_iteration_owner.pgy` -- graph-owned collection
  hoist and foreach branch use projection; range loops retain explicit no-use
  semantics.
- `src/self_hosted/mir/routine_assignment_owner.pgy` -- semantic assignment
  row to SSA definition, projected-target graph, and receiver-use lowering.
- `src/self_hosted/mir/routine_control_transfer_owner.pgy` -- return, break,
  and continue instruction/edge lowering.
- `src/self_hosted/mir/routine_tracked_statement_owner.pgy` -- statement-kind
  dispatch after the statement fact owner has identified the tracked row;
  graph-complete simple statements are routed with one validated Atom view.
- `src/self_hosted/mir/routine_statement_owner.pgy` -- graph-owned
  Log/bare-call/Exit lowering plus the explicitly bounded collection-mutation
  bridge pending complete target/value/auxiliary owner lanes.
- `src/self_hosted/mir/routine_let_owner.pgy` -- semantic initializer row to
  MIR local declaration and SSA definition lowering.
- `src/self_hosted/mir/routine_destructure_owner.pgy` -- aligned semantic
  binding/type rows plus the semantic Value graph to one typed MIR destructure
  instruction; initializer uses are resolved before new bindings enter scope.
- `src/self_hosted/mir/routine_entry_owner.pgy` -- function-shell validation,
  signature parameter seeding, and routine-lowering entry.
- `src/self_hosted/mir/routine_receiver_carriage_owner.pgy` -- exact declaration
  owner join that projects the routine owner/kind/receiver-carriage tuple.
- `src/self_hosted/mir/routine_cfg_append_owner.pgy` -- CFG row append and
  instruction/use offset rebinding across routine-local MIR graphs.
- `src/self_hosted/mir/artifact_lower_owner.pgy` -- program assembly and
  deterministic instruction-ID canonicalization.
- `src/self_hosted/mir/program_verify_owner.pgy` -- MIR row range/topology and
  required-fact verification.
- `src/self_hosted/mir/enum_declaration_verify_owner.pgy` -- contiguous
  enum-variant payload start/count rows and concrete ordered payload-type
  verification.
- `src/self_hosted/mir/program_assignment_parameter_use_contract_owner.pgy`
  -- positive parameter-version-zero and negative local missing-use contract
  kept outside the production verifier.
- `src/self_hosted/mir/instruction_validation_owner.pgy` -- detailed MIR
  instruction-row shape diagnostics consumed by the program verifier.
- `src/self_hosted/mir/json_projection_owner.pgy` -- verified `pgy.mir.v1`
  projection; it cannot read AST provenance.
- `src/self_hosted/mir/instruction_json_artifact_writer_owner.pgy` --
  sequential file framing for unbounded instruction-local expression graphs,
  match/destructure lists, uses, and runtime-call ABI auxiliary rows; it reads
  only verified `SelfMirProgramFacts` and cannot establish semantic facts.
- `src/self_hosted/mir/program_json_artifact_writer_owner.pgy` -- bounded
  program/routine/block file-artifact framing of the same verified
  `pgy.mir.v1` row order; instruction-local unbounded rows are delegated to
  the sequential artifact writer and `SelfMirProgramFacts` remains the
  semantic owner.
- `src/self_hosted/mir/abi_layout_json_projection_owner.pgy` -- self-host
  producer ABI-layout tuple and explicit dynamic-row projection.
- `src/self_hosted/mir/machine_layer_json_projection_owner.pgy` -- machine
  layer object projection nested in the pgy.mir.v1 instruction row.
- `src/self_hosted/mir/runtime_call_abi_json_projection_owner.pgy` -- nested
  `runtime_call_abi` JSON projection from the instruction-owned fact row.
- `src/self_hosted/mir/json_projection_support_owner.pgy` -- shared optional
  scalar projection used by the MIR JSON facade.

## MIR Lower

- `src/self_hosted/mir_lower/main.pgy` -- entrypoint only.
- `src/self_hosted/mir_lower/decl_lower.pgy` -- declaration reconstruction,
  including MIR-carried generic parameter constraints and default types; it
  never guesses a missing declaration default.
- `src/self_hosted/mir_lower/declaration_method_contract_fact_owner.pgy` --
  single bounded `pgy.mir.v1` method-contract read, validation, and canonical
  AST-row projection for explicit `function`/`action` identity.
- `src/self_hosted/mir_lower/declaration_callable_lower_owner.pgy` -- binds
  nominal, ability, and role method rows to their one contract fact and, for
  executable methods, to the corresponding routine reconstruction.
- `src/self_hosted/mir_lower/error_owner.pgy` -- MIR-lower-specific
  `MirLowerFailClosed` diagnostic boundary; global `Die` aliases are forbidden.
- `src/self_hosted/mir_lower/expression_graph_fact_owner.pgy` -- schema-aware
  MIR instruction graph decoding and reconstructed-artifact NodeId binding.
- `src/self_hosted/mir_lower/expression_graph_instruction_policy_owner.pgy` --
  persisted MIR expression-graph slot requirements by instruction shape; this
  policy does not own or duplicate the program graph.
- `src/self_hosted/mir_lower/expression_graph_parser_bridge_owner.pgy` --
  bounded parser-owned reconstruction of producer-only collection receiver
  roots during MIR graph reconsumption; it is not a codegen fallback authority.
- `src/self_hosted/mir_lower/structured_expression_emission_order_owner.pgy` --
  stable MIR instruction/lane/derived-ordinal occurrence order captured at the
  structured AST emission boundary; repeated CFG visits remain distinct rows.
- `src/self_hosted/mir_lower/structured_condition_emission_owner.pgy` --
  CFG-owned branch conditions, `for` value/auxiliary lane identity, and derived
  match-binding occurrence order recorded at their actual AST emission point.
- `src/self_hosted/mir_lower/expression_graph_occurrence_owner.pgy` -- exact
  occurrence-key to MIR graph-slot selection; it rejects positional and textual
  lookup and appends directly into the one final sequence arena.
- `src/self_hosted/mir_lower/generic_specialization_fact_owner.pgy` --
  fail-closed MIR direct/member generic row decoder and final codegen-view
  projection; semantic rows are verifier evidence, not emitted-symbol input.
- `src/self_hosted/mir_lower/expression_graph_sequence_owner.pgy` -- bounded
  one-pass MIR JSON graph decoding, exact graph/node schema validation, and
  ordered graph-sequence construction.
- `src/self_hosted/mir_lower/expression_graph_match_owner.pgy` -- derived
  match-kind dispatch from carried MIR facts.
- `src/self_hosted/mir_lower/expression_graph_tagged_enum_match_owner.pgy` --
  ordered scalar/tagged-enum condition and `_N` payload projection graphs from
  carried match rows and enum declaration ownership.
- `src/self_hosted/mir_lower/expression_graph_option_match_owner.pgy` --
  `IsSome` / `UnwrapOption` unary graph construction for Option match rows.
- `src/self_hosted/mir_lower/destructure_expression_projection_owner.pgy` --
  canonical temp/index expression graphs derived from typed MIR destructure
  facts; source text and builtin-name inference are forbidden.
- `src/self_hosted/mir_lower/match_json_fact_owner.pgy` -- typed optional reads
  for match pattern arrays consumed during graph reconstruction.
- `src/self_hosted/mir_lower/match_binding_local_fact_owner.pgy` -- validates
  carried match binding/type rows and admits them into the routine-local view.
- `src/self_hosted/mir_lower/iteration_type_fact_owner.pgy` -- validates the
  routine-owned MIR iteration rows and admits Array/List foreach reconstruction
  only when binding, iterable, and element types agree.
- `src/self_hosted/mir_lower/match_binding_render_owner.pgy` -- reconstructs
  typed match binding statements and the oracle-only inferred legacy form.
- `src/self_hosted/mir_lower/phi_fact_owner.pgy` -- final-consumer typed phi
  predecessor arity, canonical SSA local/result identity, flattened incoming
  use, and unique definition-block validation without reopening raw `uses`.
- `src/self_hosted/mir_lower/ssa_identity_owner.pgy` -- consumer-side
  canonical `<source-local>.<version>` validation shared by phi and direct
  backend admission without importing producer version assignment internals.
- `src/self_hosted/mir_lower/fixture_manifest_owner.pgy` -- MIR parity
  source fixture manifest rows.
- `src/self_hosted/mir_lower/json_fact_read.pgy` -- bounded MIR JSON fact reads.
- `src/self_hosted/dir/domain_graph_fact_owner.pgy` -- bounded DIR census,
  graph-anchor identity, and topology-producer orchestration.
- `src/self_hosted/dir/domain_topology_row_owner.pgy` -- typed domain
  directive rows, exact declaration-field identity joins, and non-empty row
  validation. It is the self-host producer-side `dir.domain_graph` authority;
  MIR carries its facts without reconstructing them.
- `src/self_hosted/dir/domain_projection_map_row_owner.pgy` -- typed parent-
  directive association, unique target spelling, and source spelling for
  explicit projection-map rows consumed by the semantic assignment producer.
- `src/self_hosted/mir/domain_topology_fact_owner.pgy` -- MIR carrier for the
  DIR-owned graph identity and complete typed topology row arrays; projects
  integer producer syntax IDs to the MIR wire representation without changing
  their producer epoch.
- `src/self_hosted/mir/domain_runtime_assignment_fact_owner.pgy` -- derives
  exact effect bearer, relation endpoint, and implicit/explicit projection
  member/path assignments from typed declaration, topology, and DIR map facts
  without a backend same-name or ordinal policy.
- `src/self_hosted/mir/domain_runtime_assignment_verify_owner.pgy` -- one
  structural verifier for the self-produced runtime-assignment carrier.
- `src/self_hosted/mir/domain_runtime_assignment_json_owner.pgy` -- lossless
  `pgy.mir.v1` projection of the verified participant-role and member/path
  assignment rows.
- `src/self_hosted/mir_lower/domain_topology_fact_owner.pgy` -- derived typed
  admission view of the program-global DIR-owned topology carrier,
  relation/field-kind joins, stable row identity checks, and
  missing/unknown/duplicate fail-closed policy. `dir.domain_graph` remains the
  semantic authority.
- `src/self_hosted/mir_lower/domain_topology_graph_schedule_owner.pgy` --
  stable-field-ID node/edge storage and SCC-weighted target-neutral schedule;
  it consumes admitted identities and owns no MIR/AST/source read path.
- `src/self_hosted/mir_lower/domain_topology_graph_build_owner.pgy` -- exact
  directive-kind to dependency-edge mapping for one topology owner. Names are
  diagnostic payload only; stable field IDs retain edge identity.
- `src/self_hosted/mir_lower/domain_topology_graph_plan_owner.pgy` -- one
  program plan joining owner-local schedules, graph-derived depth/pass-limit,
  stable-ID edges, and a mutation-detecting digest. Machine admission is its
  sole full-plan validation boundary.
- `src/self_hosted/mir_lower/domain_runtime_participant_role_fact_owner.pgy`
  -- participant-role JSON parsing, cardinality, and exact declaration-field
  admission.
- `src/self_hosted/mir_lower/domain_runtime_assignment_fact_owner.pgy` --
  projection/path JSON admission plus the top-level runtime-assignment bundle;
  it composes the participant owner and exact topology joins.
- `src/self_hosted/mir_lower/domain_runtime_plan_owner.pgy` -- one-time,
  target-neutral admitted operation plan whose rows reference exact topology,
  participant-role, and projection-assignment indexes. Last consumers perform
  local lookups and never rerun whole-plan validation.
- `src/self_hosted/mir_lower/loop_flow_fact_owner.pgy` -- native
  LoopFlowSummary and stable-indexed entry/exit state fact parsing.
- `src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy` -- MIR fact
  graph payload contract facts.
- `src/self_hosted/mir_lower/mir_json_input_owner.pgy` -- MIR JSON input boundary.
- `src/self_hosted/mir_lower/mir_cfg_graph_owner.pgy` -- pure CFG distance,
  blocked-reachability, structural-merge, and dominator-edge queries used by
  the routine fact index.
- `src/self_hosted/mir_lower/machine_layer_fact_owner.pgy` -- checked
  machine-contact projection validation for MIR JSON rows; its admitted
  carrier preserves the already-built document index for final consumers.
- `src/self_hosted/mir_lower/parallel_capture_fact_owner.pgy` -- sealed parallel
  capture boundary/kind/writer fact validation for MIR JSON input.
- `src/self_hosted/mir_lower/program_declaration_index_owner.pgy` -- one
  document-order declaration identity/span inventory shared across canonical
  declaration-family projection phases; it composes the field identity index
  from those already-discovered spans rather than reopening the declaration
  array.
- `src/self_hosted/mir_lower/program_declaration_field_identity_index_owner.pgy`
  -- one flattened owner/name/source-ID/field-kind identity index built from
  program declaration spans; topology consumers must use its exact join and
  fail closed on missing, non-positive, or duplicate field identities.
- `src/self_hosted/lib/json_bounded_fact_read.pgy` -- exact-bound JSON object
  fact reads that consume structure-owner spans without rediscovering the full
  document length.
- `src/self_hosted/mir_lower/assignment_binding_mode_fact_owner.pgy` --
  fail-closed comparison of carried MIR assignment modes with semantic
  assignment type facts; the named C-oracle bridge is excluded.
- `src/self_hosted/mir_lower/program_lower.pgy` -- document-order program assembly.
- `src/self_hosted/mir_lower/routine_cfg_projection_owner.pgy` -- routine-local
  successor, block identity, loop-header, and loop-exit projection queries over
  the admitted routine fact index.
- `src/self_hosted/mir_lower/routine_instruction_view_owner.pgy` -- typed
  routine/block/instruction coordinate view over the program instruction
  identity; consumers cannot reopen kind/source/machine fields.
- `src/self_hosted/mir_lower/program_routine_index_owner.pgy` -- one admitted
  document-order routine/block/instruction structure view, including
  instruction identity and raw machine spans, shared by machine admission,
  declaration lookup, and routine reconstruction.
- `src/self_hosted/mir_lower/program_routine_receiver_identity_owner.pgy` --
  exact routine source-ID uniqueness, declaration-owner join, and receiver
  carriage admission shared by routine index construction and validation.
- `src/self_hosted/mir_lower/routine_instruction_fact_bundle_owner.pgy` -- one
  routine-local pass over admitted instruction spans that captures result and
  render scalars plus raw ABI value bounds without mixing local facts into the
  program-global index.
- `src/self_hosted/mir_lower/routine_instruction_use_fact_owner.pgy` -- one
  routine-local flattened view of admitted instruction `uses` arrays. It keeps
  use identity shared across backend consumers and rejects missing arrays or
  empty use identities instead of allowing backend-local raw JSON reads.
- `src/self_hosted/mir_lower/routine_result_definition_fact_owner.pgy` --
  unique routine-local SSA result definition identity and its global/block/
  instruction coordinates, derived from the admitted instruction bundle.
- `src/self_hosted/mir_lower/routine_instruction_scalar_capture_owner.pgy` --
  one bounded instruction-object walk that captures instruction name,
  routine-local render strings, and ABI value spans; ABI syntax and semantic
  validation remain with `abi_layout_fact_owner.pgy`.
- `src/self_hosted/mir_lower/run_owner.pgy` -- MIR-lower CLI run boundary and
  manifest mode selection.
- `src/self_hosted/mir_lower/routine_fact_index_owner.pgy` -- per-routine
  result, source-local, successor, backedge, structural-merge, and loop-flow
  facts layered on the admitted program structure view and consumed by
  recursive CFG reconstruction.
- `src/self_hosted/mir_lower/resource_flow_fact_owner.pgy` -- native
  ResourceFlowUniverse identity row parsing and count validation.
- `src/self_hosted/mir_lower/resource_runtime_abi_fact_owner.pgy` -- carried
  MIR resource runtime-call ABI row validation before self-host reconstruction.
- `src/self_hosted/mir_lower/abi_layout_fact_owner.pgy` -- carried static MIR
  ABI layout rows and stable-identity validation.
- `src/self_hosted/mir_lower/routine_inventory_owner.pgy` -- routine inventory facts.
- `src/self_hosted/mir_lower/routine_lower.pgy` -- routine CFG/body reconstruction.
- `src/self_hosted/mir_lower/stmt_render.pgy` -- instruction fact to AST text rendering.

## Codegen

- `src/self_hosted/codegen/main.pgy` -- entrypoint only.
- `src/self_hosted/codegen/input/ast_input_owner.pgy` -- AST path and read boundary.
- `src/self_hosted/compiler/driver_pipeline_owner.pgy` -- source-to-typed-AST
  composition boundary consumed by the hard codegen run path; codegen does not
  import parser implementation owners.
- `src/self_hosted/codegen/input/ast_arena_codegen_view_owner.pgy` -- codegen-only fail-closed predicates over shared `AstArena` facts.
- `src/self_hosted/parser/expression_graph_owner.pgy` -- owner of array-literal roots and ordered element edges consumed by hard codegen through the semantic expression graph view.
- `src/self_hosted/hir/program_graph_owner.pgy` -- storage owner for the stable
  `AstExpressionArena` topology shared by parser/HIR and semantic overlays;
  its isolated-node append API is the only allowed compiler-generated topology
  extension; call-target, place, type, MIR, and backend facts are not owned here.
- `src/self_hosted/codegen/input/semantic_enum_codegen_view_owner.pgy` -- fail-closed projection of semantic enum names, ordered variants, and payload arity.
- `src/self_hosted/codegen/input/semantic_signature_codegen_view_owner.pgy` --
  fail-closed codegen view over semantic function signature facts, including
  selected entrypoint or library function-node projection without an arena
  name scan.
- `src/self_hosted/codegen/input/callable_receiver_codegen_view_owner.pgy` --
  callable-identity receiver carriage rows shared by C definitions,
  prototypes, function environments, and member calls; MIR consumers exact-
  join admitted source ID/owner/name rows while source entrypoints derive the
  same fact from verified semantic declaration owners.
- `src/self_hosted/codegen/input/domain_runtime_codegen_view_owner.pgy` --
  admitted C-target view of owner identity to exact runtime method prologue;
  lookup consumes the once-validated view without whole-table revalidation.
- `src/self_hosted/codegen/input/generic_specialization_codegen_view_owner.pgy`
  -- ordered C specialization view shared by source entrypoints and MIR
  consumers; hard MIR codegen receives it from the MIR row decoder and does
  not reopen semantic rows to choose symbols or type actuals.
- `src/self_hosted/codegen/input/semantic_role_codegen_view_owner.pgy` --
  fail-closed role name, target-type, and method-identity projection from
  semantic role facts.
- `src/self_hosted/codegen/input/semantic_nominal_codegen_view_owner.pgy` --
  fail-closed codegen projection of semantic nominal names, declaration kinds,
  and ordered field name/type rows.
- `src/self_hosted/codegen/input/semantic_local_binding_codegen_view_owner.pgy` -- fail-closed codegen view over semantic local binding identity, name, and type facts, including `Let` routing.
- `src/self_hosted/codegen/input/semantic_assignment_codegen_view_owner.pgy` -- fail-closed codegen view over semantic assignment identity, target/base/index/RHS rows, and verified expected-type facts, including `Assign` routing.
- `src/self_hosted/codegen/input/semantic_body_type_codegen_view_owner.pgy` -- fail-closed projection of the semantic-owned body type bundle consumed by C emission; it does not synthesize semantic facts.
- `src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy` -- fail-closed codegen view over semantic statement kind/payload rows, including control-flow and collection routing.
- `src/self_hosted/codegen/input/semantic_expression_codegen_view_owner.pgy`
  -- fail-closed codegen view over semantic-owned expression shape and graph
  rows keyed by artifact node and payload lane, including try operand edges.
- `src/self_hosted/codegen/input/semantic_kind_codegen_view_owner.pgy` --
  fail-closed node-kind identity projection for ability/event declaration
  routing.
- `src/self_hosted/codegen/input/ast_expression_usage_owner.pgy` -- backend
  builtin-group vocabulary projected from semantic expression-surface facts.
- `src/self_hosted/codegen/input/ast_kind_usage_owner.pgy` -- backend runtime
  statement-kind projection from semantic kind-surface facts.
- `src/self_hosted/codegen/input/ast_type_usage_owner.pgy` -- backend runtime
  type-family projection from semantic type-surface facts.
- `src/self_hosted/codegen/input/nominal_array_usage_owner.pgy` -- declared
  nominal-record array usage facts derived from semantic type surfaces and the
  codegen type environment.
- `src/self_hosted/codegen/input/ast_usage_owner.pgy` -- runtime/header usage facts derived from expression/kind/type usage owner rows.
- `src/self_hosted/codegen/input/value_wrapper_usage_owner.pgy` -- canonical
  recursive by-value Option/Result wrapper inventory derived from semantic type
  surfaces.
- `src/self_hosted/codegen/emission/member_call_receiver_carriage_owner.pgy` --
  fail-closed member receiver ABI decision: role erasure, value carriage, and
  stable-address materialization for mutable identity share one consumer.
- `src/self_hosted/codegen/emission/role_receiver_binding_owner.pgy` --
  fail-closed concrete-self binding behind the erased direct-role method ABI;
  semantic nominal kind chooses pointer identity versus value carriage.
- `src/self_hosted/codegen/run/codegen_run_owner.pgy` -- codegen CLI run boundary.
- `src/self_hosted/codegen/text/text_owner.pgy` -- codegen expression scanning and unsupported-surface policy.
- `src/self_hosted/codegen/text/enum_literal_owner.pgy` -- payload-free enum literal projection facts.
- `src/self_hosted/codegen/text/expr_scan.pgy` -- expression text scanning.
- `src/self_hosted/codegen/text/expr_sequence_owner.pgy` -- top-level comma-separated expression sequence facts.
- `src/self_hosted/codegen/text/struct_literal_call_owner.pgy` -- struct literal call-envelope facts.
- `src/self_hosted/codegen/text/struct_literal_field_owner.pgy` -- struct literal field-name/value entry facts.
- `src/self_hosted/codegen/text/struct_field_access_owner.pgy` -- dotted member-access field spelling projection facts.
- `src/self_hosted/codegen/type_facts/type_env.pgy` -- type environment facts.
- `src/self_hosted/codegen/abi_layout/enum_abi_value_fact_owner.pgy` -- one
  semantic-enum-to-C-value/default ABI fact for payload-free and tagged enum
  consumers.
- `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy` -- self-host C ABI type spelling facts, including nominal struct type and empty parameter-list spelling.
- `src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy` -- self-host C collection runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/list_runtime_owner.pgy` -- canonical `List<T>` runtime ABI fact, supported element ABI, specialization macro, and constructor/operation-symbol projection.
- `src/self_hosted/codegen/runtime_abi/queue_runtime_owner.pgy` -- canonical
  `Queue<T>` runtime ABI fact, supported element ABI, and constructor/operation
  symbol projection.
- `src/self_hosted/codegen/runtime_abi/set_runtime_owner.pgy` -- canonical
  `Set<T>` runtime ABI fact, supported element ABI, and constructor/operation
  symbol projection.
- `src/self_hosted/codegen/runtime_abi/checked_arithmetic_runtime_owner.pgy` --
  fail-closed numeric conversion runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy` -- self-host C host file/argv/process entrypoint runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy` -- self-host C math/random runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy` -- self-host C Option/Result runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/result_runtime_owner.pgy` -- explicit
  `Result<T, E>` runtime ABI facts and specialized helper symbol ownership.
- `src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy` -- self-host C string/text runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/text_builder_runtime_owner.pgy` -- self-host C Allocator/TextBuilder symbol facts; implementation bodies remain owned by the canonical runtime inline headers.
- `src/self_hosted/codegen/runtime_abi/runtime_header_owner.pgy` --
  owner-directed canonical runtime header composition for allocator,
  TextBuilder, BoxArray, and collection consumers; it does not duplicate
  their C implementations.
- `src/self_hosted/codegen/runtime_abi/runtime_header_ownership_owner.pgy` --
  helper-ownership predicates for the selected runtime headers (checked
  arithmetic, scalar log, Bool-to-String); consumers ask this owner instead
  of re-deriving header capabilities.
- `src/self_hosted/codegen/runtime_abi/spawn_runtime_owner.pgy` -- bounded self-host C spawn/await runtime ABI facts for scalar Int/String async work through one tagged invocation descriptor, including named `Future<T>` handle materialization; unsupported payload/arity shapes fail closed.
- `src/self_hosted/codegen/runtime_abi/box_array_runtime_owner.pgy` -- self-host C allocator-backed Box<Array<T>> type and constructor ABI facts.
- `src/self_hosted/codegen/emission/expr_rewrite.pgy` -- expression rewrite/lowering.
- `src/self_hosted/codegen/emission/expr_semantic_graph_emit_owner.pgy` --
  recursive expression emission from semantic node handles and child edges;
  codegen does not split migrated payloads to rediscover precedence. It is the
  semantic-check cluster root for the mutually recursive call and composite
  literal projection owners below.
- `src/self_hosted/codegen/emission/expr_semantic_composite_literal_emit_owner.pgy` --
  canonical expected-type value emission from semantic graph handles and
  child edges, including arrays, Set, named structs, Result, resource, and
  List values; Option dispatch consumes the dedicated projection owner below.
- `src/self_hosted/codegen/emission/expr_semantic_option_value_owner.pgy` --
  expected-type `Option<T>` constructor identity, payload edge, and MIR-owned
  runtime ABI projection; it never reparses source text or chooses a default
  payload ABI.
- `src/self_hosted/codegen/emission/expr_semantic_type_owner.pgy` --
  expression type projection from semantic graph handles plus codegen type
  rows; migrated emitters must not reparse node text to recover these types.
- `src/self_hosted/codegen/emission/expr_semantic_call_type_owner.pgy` --
  call-spine return-type projection, including explicit Result error payloads,
  from semantic graph facts.
- `src/self_hosted/codegen/emission/expr_semantic_call_argument_owner.pgy` --
  shared call-argument value projection and graph-owned `ref`/`inout`
  addressability consumption; family emitters must not rebuild this policy.
- `src/self_hosted/codegen/emission/list_call_emit_owner.pgy` -- canonical
  `List<T>` operation lowering from semantic receiver type and List runtime ABI
  facts; source callee spelling is not an ABI fallback.
- `src/self_hosted/codegen/emission/queue_call_emit_owner.pgy` -- canonical
  `Queue<T>` operation lowering from semantic Queue-call facts and the Queue
  runtime ABI; source callee spelling is not an ABI fallback.
- `src/self_hosted/codegen/emission/set_call_emit_owner.pgy` -- canonical
  `Set<T>` operation lowering from semantic Set-call facts and the Set runtime
  ABI; source callee spelling is not an ABI fallback.
- `src/self_hosted/codegen/emission/foreach_collection_runtime_owner.pgy` --
  canonical Array/List for-each runtime projection, including collection and
  element C types, address-passing policy, and get/length ABI calls.
- `src/self_hosted/codegen/emission/list_call_type_owner.pgy` -- canonical
  `List<T>` return-type projection consumed by compound-expression codegen.
- `src/self_hosted/codegen/emission/queue_call_type_owner.pgy` -- canonical
  `Queue<T>` return-type projection consumed by compound-expression codegen.
- `src/self_hosted/codegen/emission/set_call_type_owner.pgy` -- canonical
  `Set<T>` return-type projection consumed by compound-expression codegen.
- `src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy` --
  call-spine and simple member-access consumption, ordered argument projection,
  parameter-mode handling, receiver insertion, and runtime/constructor/method
  symbol fact consumption, delegating List family calls to their named owner.
- `src/self_hosted/codegen/emission/expr_semantic_dynamic_ability_call_emit_owner.pgy`
  -- dynamic party role-slot call projection from semantic graph identity,
  dispatch ABI rows, and vtable field ownership; direct-call fallback is
  forbidden once a role-slot row exists.
- `src/self_hosted/codegen/emission/ability_bind_emit_owner.pgy` -- party
  role-slot bind C emission from semantic bind identity and dispatch ABI rows;
  missing bind facts fail closed.
- `src/self_hosted/codegen/emission/log_emit_owner.pgy` -- log expression
  graph-owned log expression projection and scalar formatting ABI consumption.
- `src/self_hosted/codegen/emission/option_value_emit_owner.pgy` --
  statement-level `Option<T>` adapter; constructor and payload selection
  delegates through the canonical expected-value dispatcher to the dedicated
  Option projection owner and MIR-owned runtime ABI rows.
- `src/self_hosted/codegen/emission/runtime_call_rewrite_owner.pgy` --
  single-pass source builtin call recognition projected through runtime symbol
  owners; stable source aliases such as `Concat`/`StringConcat` converge on one
  ABI symbol and string literals remain opaque.
- `src/self_hosted/codegen/emission/array_value_emit_owner.pgy` -- C aggregate
  emission from already-rendered array items; recursive expression evaluation
  remains owned by `expr_rewrite.pgy`.
- `src/self_hosted/codegen/emission/collection_element_emit_owner.pgy` --
  collection element value emission; graph-owned `ArrayPush` projection is
  separated from the still-explicit array-literal and `ArraySet` text bridges.
- `src/self_hosted/codegen/emission/box_array_let_emit_owner.pgy` --
  expected-type `Box<Array<T>>` initializer materialization from the
  semantic call spine and named allocator place/type facts.
- `src/self_hosted/codegen/emission/enum_emit_owner.pgy` -- enum declaration
  emission and semantic enum-value projection into the codegen environment.
- `src/self_hosted/codegen/emission/function_binding_env_owner.pgy` --
  one function-value binding fact for source identity, semantic type, runtime
  kind, C name, and environment rows, plus implicit owner-field C binding rows
  derived from semantic locals and MIR-carried nominal declaration facts.
- `src/self_hosted/codegen/emission/function_emit.pgy` -- function definition,
  signature-environment, and prototype emission.
- `src/self_hosted/codegen/emission/role_dispatch_emit_owner.pgy` -- ability
  vtable types and instances, role method ABI declarations, party bind
  boundaries, and value-to-receiver operator adapters from semantic role facts.
- `src/self_hosted/codegen/emission/nominal_struct_emit_owner.pgy` -- nominal C
  struct layout and environment rows, including dynamic party role-slot
  storage and its dispatch-vtable field identity.
- `src/self_hosted/codegen/emission/program_statement_shape_owner.pgy` --
  recursive statement/block shape admission before program-level C assembly;
  semantic kind rows remain authoritative for each accepted statement.
- `src/self_hosted/codegen/emission/generic_function_emit_owner.pgy` --
  generic-template suppression and concrete specialization emission.
- `src/self_hosted/codegen/emission/literal_rewrite.pgy` -- source literal lowering.
- `src/self_hosted/codegen/emission/option_match_owner.pgy` -- Option match
  condition and typed payload-binding projection from wrapper and pattern facts.
- `src/self_hosted/codegen/emission/program_emit.pgy` -- program emission and prepasses.
- `src/self_hosted/codegen/emission/result_runtime_emit_owner.pgy` -- one-node
  explicit `Result<T, E>` C declaration materialization for the declaration
  dependency owner.
- `src/self_hosted/codegen/emission/result_let_emit_owner.pgy` -- explicit
  `Result<T, E>` local binding materialization from expected semantic facts.
- `src/self_hosted/codegen/emission/program_entry_owner.pgy` -- public source,
  artifact, and verified semantic entrypoints into program emission.
- `src/self_hosted/codegen/emission/assign_emit_owner.pgy` -- assignment target
  and value projection over semantic rows and expression graph handles.
- `src/self_hosted/codegen/emission/stmt_emit.pgy` -- statement emission.
- `src/self_hosted/codegen/emission/tagged_enum_match_owner.pgy` -- tagged
  enum match tag conditions and typed payload-binding projection.
- `src/self_hosted/codegen/emission/type_declaration_emit_owner.pgy` --
  dependency-ordered nominal/enum/generated-wrapper C declaration emission
  from semantic field, payload, and Result-usage facts. Named `Option<T>` and
  explicit `Result<T,E>` materializations participate in the same graph;
  missing wrapper facts and direct by-value cycles fail closed.
- `src/self_hosted/codegen/emission/struct_value_emit.pgy` -- struct value emission.
- `src/self_hosted/codegen/emission/try_let_emit_owner.pgy` -- try-expression
  local-binding control flow from semantic graph edges and Option/Result ABI
  facts.
- `src/self_hosted/codegen/emission/value_return_emit_owner.pgy` -- expected-type Option and return value emission.

## Fuzz

- `src/self_hosted/fuzz/backend_parity_generator/main.pgy` -- backend parity
  fuzz source-program construction and generator entrypoint.
- `src/self_hosted/fuzz/backend_parity_generator/manifest_owner.pgy` -- backend
  parity fuzz JSONL manifest and stdout summary shape.

## Tools

- `src/self_hosted/tools/machine_layer_air_validator/main.pgy` -- AIR
  `machine_layer_sites` projection validator over the checked machine owner
  rows.
- `src/self_hosted/tools/machine_layer_rir_validator/main.pgy` -- native RIR
  `machine_contact` artifact validator over the same checked machine owner;
  it never recovers contact identity from source text.
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
- `src/self_hosted/tools/assignment_projection_probe/main.pgy` -- focused
  executable proof that scalar and indexed assignments consume semantic
  expected, target, and expression-graph facts under C/LLVM parity.
- `src/self_hosted/tools/initializer_projection_probe/main.pgy` -- focused
  executable proof that unannotated local types reach MIR through semantic
  initializer rows, direct scalar argument trees, and fail-closed graph damage.
- `src/self_hosted/tools/mir_json_instruction_writer_probe/main.pgy` -- raw
  byte parity between the fixture-only MIR String projection and production
  sequential artifact writer over the same verified facts, plus invalid-fact
  rejection before output open/truncation.
- `src/self_hosted/tools/gate_dashboard/main.pgy` -- Pergyra-owned gate
  dashboard CLI and manifest/result composition boundary.
- `src/self_hosted/tools/gate_dashboard/result_owner.pgy` -- fail-closed gate
  result artifact parser for IDs, outcomes, durations, and details.
- `src/self_hosted/tools/gate_dashboard/report_owner.pgy` -- stable dashboard
  JSON, health, budget, and summary projection.
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

## Parallel

- `src/self_hosted/parallel/chunk_policy_owner.pgy` -- auto-chunk policy SoT
  for the `parallel ... join` fan-out (WO-RT-4 B3): chunk-count policy,
  remainder-balanced split arithmetic, in-language cover invariants, and the
  stable-identifier pin list for every C/LLVM projection site.
- `src/self_hosted/parallel/chunk_policy_manifest.pgy` -- runnable projection
  over the chunk policy owner: prints the canonical count/split/cover table
  and the require rows for the expected-artifact diff on both compiler legs
  (`src/self_hosted/parallel/expected_chunk_policy_manifest.txt`, gate
  `tests/selfhost_parallel_chunk_policy_smoke.sh`).
- `src/self_hosted/parallel/lane_policy_owner.pgy` -- SEA execution-lane
  classification SoT (docs/146): the evidence record, the priority-ordered
  decision table, the in-language invariants it exists to guarantee
  (contradictory evidence rejects, rule order is the contract, the movable
  M:N lane needs its full conjunction), the declared codegen reachability
  (five of seven lanes since WO-PAR-NOVEL step 2 landed; Inline is
  impossible for a concurrent spawn boundary and Reject fail-closes the
  compile), and the pin list for every C projection site.
- `src/self_hosted/parallel/lane_policy_manifest.pgy` -- runnable projection
  over the lane policy owner: prints the canonical lane/invariant/reachable
  table and the require rows for the expected-artifact diff on both compiler
  legs (`src/self_hosted/parallel/expected_lane_policy_manifest.txt`, gate
  `tests/selfhost_parallel_lane_policy_smoke.sh`).
- `src/self_hosted/parallel/spawn_lane_plan_owner.pgy` -- verified spawn-lane
  plan bridge SoT (docs/146 WO-PAR-NOVEL step 2): how AIR-classified lanes
  TRAVEL to the backends. Owns the plan artifact contract (revision, legal
  row lanes -- never Reject or Inline), the producer's fail-closed refusals
  (a Reject-classified spawn boundary refuses the compile; conflicting lanes
  for one site refuse; duplicates collapse), the AST_SPAWN_EXPR filter,
  per-site fail-closed lookup, and the driver-produces / backend-consumes
  split. The decision table stays in lane_policy_owner (imported for lane
  tags -- one source).
- `src/self_hosted/parallel/spawn_lane_plan_manifest.pgy` -- runnable
  projection over the spawn-lane plan owner: prints the contract, re-runs the
  producer witnesses, and emits the require/forbid rows for the
  expected-artifact diff on both compiler legs
  (`src/self_hosted/parallel/expected_spawn_lane_plan_manifest.txt`, gate
  `tests/selfhost_spawn_lane_plan_smoke.sh`).

## Compiler World

- `src/self_hosted/compiler/codegen_callable_receiver_bridge_owner.pgy` --
  one-way bridge from the machine-admitted MIR routine inventory to the
  codegen callable-receiver fact; codegen never reopens MIR JSON.
- `src/self_hosted/compiler/domain_runtime_c_codegen_bridge_owner.pgy` --
  one-way renderer from the admitted target-neutral runtime plan to exact C
  role binding and projection-sync prologues; it owns no source, JSON, ordinal,
  or same-name recovery path.

- `src/self_hosted/compiler/reachability_owner.pgy` -- mechanism reachability
  contract: no mechanism without a consumer, or an explicit declaration that
  it has none. Each row names an entry symbol, its home, the scope that should
  consume it, and `live` (>= 1 consumer, so silent death fails) or
  `declared_only` (exactly 0, so a gap can neither widen nor close unrecorded).
  Two gaps are on record today: the AIR execution-lane fact that codegen drops,
  and the M:N fiber scheduler compiled into the binary with no caller.
- `src/self_hosted/compiler/reachability_manifest.pgy` -- runnable projection
  over that contract for the expected-artifact diff on both compiler legs
  (`src/self_hosted/compiler/expected_reachability_manifest.txt`, gate
  `tests/selfhost_reachability_contract_smoke.sh`).

- `src/self_hosted/compiler/world.pgy` -- `PgyCompilerWorld`, stage path
  manifest, and root compiler intent flow.
- `src/self_hosted/compiler/path_manifest_owner.pgy` -- self-host compiler
  source/test/parity path fact values.
- `src/self_hosted/compiler/stage_intents.pgy` -- derived compiler intent clusters.
- `src/self_hosted/compiler/target_capability_owner.pgy` -- target acceptance
  and fallback fact envelope for backend projections.
- `src/self_hosted/compiler/target_projection_fact_owner.pgy` -- derived
  projection carriage consumed by the hard emitter; target vocabulary remains
  owned by `target_capability_owner.pgy`.
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
  fact vocabulary for intent/effect/authority/coordination and MIR terminators.
- `src/self_hosted/air/mir_cfg_certificate_owner.pgy` -- MIR-bound AIR
  certificate issuer for the bounded direct CFG rung. It verifies the typed
  block/terminator/merge-phi, nested-conditional, while-loop, integer-range,
  or six-block loop-break inventory
  once, binds MIR, CFG, predecessor-resolved phi, nested-spine, and loop-spine
  facts, and permits only strict zero-fallback/zero-drift evidence with a
  fixed-size identity guard.
- `src/self_hosted/air/mir_cfg_certificate_readiness_owner.pgy` -- fixed-size
  post-issuance readiness for the common certificate. It consumes only carried
  identities and never reopens MIR, JSON, AST, or an expression graph.
- `src/self_hosted/air/mir_cfg_certificate_value_owner.pgy` -- immutable
  replacement constructors shared by issuance and repaired-digest negatives.
- `src/self_hosted/air/mir_cfg_certificate_mutation_owner.pgy` -- repaired-
  digest negative owner for outer, nested, while, range, and loop-break
  certificate facts.
- `src/self_hosted/air/mir_cfg_certificate_fact_owner.pgy` -- fixed-size v6
  certificate identity shared by issuance and the target-neutral plan; it
  carries nested, while, range, and loop-break digests without reopening MIR.
- `src/self_hosted/air/mir_cfg_identity_owner.pgy` -- stable digest functions
  over the already-built typed MIR routine index; it does not reopen a document
  or build another graph/certificate view.
- `src/self_hosted/air/mir_nested_cfg_certificate_fact_owner.pgy` -- bounded
  five-block nested-condition fact binding both branch rows to one entry SSA,
  the inner direct-false merge, and the forward edge to the outer merge.
- `src/self_hosted/air/mir_loop_cfg_certificate_fact_owner.pgy` -- bounded
  four-block while-loop fact binding preheader, header, body, exit, backedge,
  predecessor-resolved phi lanes, SSA uses, increment result, and loop-summary
  metadata from the already-built routine index.
- `src/self_hosted/air/mir_range_cfg_certificate_fact_owner.pgy` -- bounded
  four-block phi-free integer-range fact binding typed preheader/header/body/
  exit roles, the range backedge, loop summary, iteration verdict, source
  local, zero-use contract, and instruction identities from one routine index.
- `src/self_hosted/air/mir_break_cfg_certificate_fact_owner.pgy` -- bounded
  six-block loop-break certificate binding preheader/header/decision/break/
  empty-continuation/exit roles, the real continuation predecessor and its
  forwarded definition, one header phi, normal/break exit SSA lanes, two Log
  uses, break row identity, and one while summary from typed owners.
- `src/self_hosted/compiler/artifact_zone_owner.pgy` -- comparable artifact
  kinds consumed by C/LLVM/self-hosted parity.
- `src/self_hosted/compiler/region_plan_owner.pgy` -- verified region plan SoT
  (docs/197 WO-REG-1): how a certified escape verdict TRAVELS to the backends.
  Owns the plan artifact contract (revision, the two dispositions and no
  third), the producer's fail-closed refusals (a null site; conflicting scope
  or owning function for one site), duplicate collapse, the v1 certification
  rule (a string concat that is a DIRECT Print argument under the semantic
  BuiltinKind fact owner), the HIR/MIR carriage and driver completeness
  boundary, and the
  fail-closed ASYMMETRY that makes a narrow analysis safe to ship -- a lookup
  MISS is HEAP, today's byte-identical emission, so incompleteness costs
  performance and never correctness. Contrast the spawn-lane plan, where a
  miss must refuse because no safe default lane exists.
- `src/self_hosted/compiler/region_plan_manifest.pgy` -- runnable projection
  over the region plan owner: prints the contract, re-runs the producer and
  soundness witnesses, and emits the require/forbid rows for the
  expected-artifact diff on both compiler legs
  (`src/self_hosted/compiler/expected_region_plan_manifest.txt`, gate
  `tests/selfhost_region_plan_smoke.sh`).
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
- `src/self_hosted/compiler/gate_dashboard_owner.pgy` -- active hard self-host
  gate identity, Make target, tier, budget, declared state, and owner-fact rows.
- `src/self_hosted/compiler/incremental_fact_graph_owner.pgy` -- compiler-scale
  incremental fact graph schema, dependency axes, reusable artifact kinds, and
  clean/incremental verifier vocabulary. The current completeness cache remains
  rung0 and coarse; this owner is the contract for later precise invalidation.
- `src/self_hosted/compiler/abi_layout_row_owner.pgy` -- cross-backend ABI row
  fact vocabulary for field order, niche, tags, ownership, and layout.
- `src/self_hosted/compiler/abi_layout_nominal_array_owner.pgy` -- derived ABI
  layout and C symbol facts for arrays whose element is a declared nominal
  record.
- `src/self_hosted/compiler/backend_abi_layout_contract_owner.pgy` -- backend
  ABI-layout required/forbidden source contract rows tied to the ABI row owner.
- `src/self_hosted/compiler/abi_layout_target_policy_owner.pgy` -- ABI layout
  target projection and fallback-policy facts.
- `src/self_hosted/compiler/abi_layout_row_manifest.pgy` -- runnable ABI row
  projection over the ABI layout row owner for parity/golden comparison.
- `src/self_hosted/compiler/runtime_call_abi_row_owner.pgy` -- runtime helper
  and target-library call ABI row projection over the runtime ABI owners.
- `src/self_hosted/compiler/runtime_call_abi_structured_fact_owner.pgy` --
  typed native-resource and named target-library row-input projection consumed
  by self-host compiler paths; it does not parse the serialized row artifact
  back into facts.
- `src/self_hosted/compiler/machine_layer_runtime_projection_owner.pgy` --
  checked abstract machine-layer contact/runtime projection; physical
  declaration literals are forbidden here.
- `src/self_hosted/compiler/machine_layer_declaration_consumer.pgy` --
  native `pgy.machine-layer.declaration.v1` JSON consumer owning the
  self-hosted physical grant/provenance view and its fail-closed shape checks.
- `src/self_hosted/compiler/machine_layer_runtime_binding_owner.pgy` --
  final self-host C startup consumer that serializes the checked declaration
  fingerprints and selected grant window into the runtime mapping bind; it is
  not a second physical declaration owner.
- `src/self_hosted/compiler/runtime_call_abi_row_manifest.pgy` -- runnable
  runtime call ABI row projection for parity/golden comparison.
- `src/self_hosted/compiler/symbol_table_owner.pgy` -- cross-backend symbol row
  fact vocabulary for C/LLVM/self-hosted projections, including struct field,
  source-to-C binding, inout parameter, foreach loop temporary, and try/match
  emission temporary spelling.
- `src/self_hosted/codegen/fixture_manifest_owner.pgy` -- committed codegen
  parity fixture frontier shared by codegen parity, MIR parity, and driver
  artifact rungs.
- `src/self_hosted/codegen/reject_fixture/tagged_enum_equality.pgy` --
  TestHarness-owned negative codegen artifact proving that whole tagged-enum
  equality fails closed until a semantic equality operation owns that policy.
- `src/self_hosted/codegen/reject_fixture/event_decl.pgy` -- TestHarness-owned
  negative codegen artifact proving unsupported event declarations reject
  through semantic node-kind identity under C/LLVM tool parity.
- `src/self_hosted/codegen/reject_fixture/ref_temporary_member.pgy` --
  TestHarness-owned negative codegen artifact proving that a nominal field on
  temporary storage cannot cross a `ref` boundary under C/LLVM tool parity.
- `src/self_hosted/codegen/reject_fixture/cyclic_value_declarations.pgy` --
  TestHarness-owned negative artifact proving direct by-value nominal/enum
  declaration cycles fail closed in both the bootstrap C seed and self-host
  codegen tool.
- `src/self_hosted/codegen/reject_fixture/cyclic_result_value_declaration.pgy`
  -- TestHarness-owned negative artifact proving a nominal-to-Result-to-nominal
  by-value cycle fails closed in both declaration schedulers.
- `src/self_hosted/codegen/reject_fixture/cyclic_nested_option_result_value_declaration.pgy`
  -- TestHarness-owned negative artifact proving a nested
  nominal-to-Option-to-Result-to-nominal by-value cycle fails closed in both
  declaration schedulers.
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
  source/MIR/output-file boundary used by bounded producer parity and the
  integrated seed/oracle parity proof. The full stage2/stage3 consumer fixed
  point is an explicit gate; pipeline ownership remains in
  `driver_rung2_owner.pgy`.
- `src/self_hosted/compiler/driver_rung2_execution_owner.pgy` -- reachable
  identity-bearing action boundary for the direct-MIR rung. It owns request-
  to-target admission, exact emitted-artifact acceptance, output write, and
  execution stage/result facts plus the one-subject direct-MIR authority zone,
  while reusing the existing typed MIR and backend owners unchanged.
- `src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy` -- active-slice
  composition owner for the single `PgyCompilerWorld`. It constructs the
  direct-MIR zone and subject in one nested expression so no separate source
  binding survives, then delegates once through the world method. This is not
  a physical no-copy claim. It owns no target, MIR, backend, or artifact fact
  and may not declare another world.
- `src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy` --
  backend-neutral hard-substitution boundary that receives one admitted MIR
  graph, selects the bounded scalar or verified-CFG path, and creates one C or
  LLVM artifact without rebuilding AST/semantic artifacts or creating
  backend-specific MIR readers.
- `src/self_hosted/compiler/domain_topology_graph_plan_consumer_owner.pgy` --
  bounded production receipt plus C/LLVM projections of the one admitted
  target-neutral domain topology plan. It serializes exact ID edges and
  graph-derived bounds without revalidating or rebuilding the plan; digest
  mutation self-tests remain gate-only.
- `src/self_hosted/compiler/direct_mir_backend_emission_owner.pgy` -- one text
  emission dispatch responsibility containing both C and LLVM consumers. It
  is the last artifact-producing `DirectMirCfgPlan` consumer and passes only
  normalized facts to a responsibility-specific emitter; it owns no MIR/AIR
  read path.
- `src/self_hosted/compiler/direct_mir_cfg_plan_owner.pgy` -- target-neutral
  plan issuer. It derives the admitted bounded shape from typed owners and
  issues one verified plan; no full certificate survives issuance.
- `src/self_hosted/compiler/direct_mir_cfg_plan_value_owner.pgy` -- immutable
  fixed-plan replacement constructors used by the issuer and negative owner.
- `src/self_hosted/compiler/direct_mir_cfg_plan_mutation_owner.pgy` -- repaired-
  digest negative owner for target fingerprint, phi, nested, while, range, and
  loop-break plan bindings.
- `src/self_hosted/compiler/direct_mir_cfg_plan_fact_owner.pgy` -- fixed-size
  v6 plan identity/readiness contract binding AIR/MIR/CFG/phi/nested/while/
  range/loop-break digests, target capability, topology, and normalized shape
  before emission.
- `src/self_hosted/compiler/direct_mir_cfg_shape_fact_owner.pgy` -- normalized
  closed action facts for the same single CFG plan: literal-log arms or typed
  Int assignment arms with a predecessor-resolved merge phi and Log use. It is
  a derived plan payload, not a second plan or backend reader.
- `src/self_hosted/compiler/direct_mir_cfg_entry_fact_owner.pgy` -- shared
  typed entry-local/result/literal projection used by every direct CFG shape.
- `src/self_hosted/compiler/direct_mir_cfg_log_shape_owner.pgy` -- shared
  target-neutral `Log(ToString(local))` graph shape; each topology owner keeps
  its own SSA-use policy.
- `src/self_hosted/compiler/direct_mir_nested_cfg_shape_owner.pgy` -- derives
  outer/inner comparison literals and the nested Log payload from one issued
  topology fact and the existing typed index/use owners.
- `src/self_hosted/compiler/direct_mir_nested_cfg_emission_owner.pgy` -- one
  nested-condition text responsibility containing both C and LLVM emitters;
  it receives fixed facts, never a plan or MIR/AIR reader.
- `src/self_hosted/compiler/direct_mir_loop_cfg_shape_owner.pgy` -- derives one
  target-neutral while-loop payload from the issued loop certificate plus the
  existing index/use owners, including condition, Log, increment, and
  assignment-target graphs.
- `src/self_hosted/compiler/direct_mir_loop_cfg_plan_fact_owner.pgy` -- fixed
  loop certificate/shape compound fact carried by the one direct CFG plan;
  repaired certificate or shape digests fail readiness.
- `src/self_hosted/compiler/direct_mir_loop_cfg_emission_owner.pgy` -- one
  loop-text responsibility containing both structured C and predecessor-bound
  LLVM emission; it receives only fixed loop facts and the shared print ABI.
- `src/self_hosted/compiler/direct_mir_range_cfg_shape_owner.pgy` -- derives
  start and exclusive stop from their MIR graph lanes and owns the fixed Int
  less-than/+1 range policy plus zero-use Log binding.
- `src/self_hosted/compiler/direct_mir_range_cfg_plan_fact_owner.pgy` -- fixed
  range certificate/shape compound fact carried by the one outer CFG plan.
- `src/self_hosted/compiler/direct_mir_range_cfg_emission_owner.pgy` -- one
  range-text responsibility containing both C and LLVM; LLVM materializes an
  alloca/load/add/store loop without fabricating a MIR phi.
- `src/self_hosted/compiler/direct_mir_break_cfg_shape_owner.pgy` -- derives
  the four graph literals, forwarded SSA identities, two Log graphs/uses, and
  distinct normal/break exit values from the issued loop-break certificate.
- `src/self_hosted/compiler/direct_mir_break_cfg_plan_fact_owner.pgy` -- fixed
  loop-break certificate/shape compound fact carried by the one outer plan.
- `src/self_hosted/compiler/direct_mir_break_cfg_emission_owner.pgy` -- one
  loop-break text responsibility containing both C and LLVM. LLVM materializes
  a separately labelled backend-only exit phi without claiming another MIR phi.
- `src/self_hosted/compiler/direct_mir_llvm_text_format_owner.pgy` -- shared
  LLVM line-format byte encoding used by scalar and CFG text emitters.
- `src/self_hosted/compiler/direct_mir_scalar_graph_admission_owner.pgy` --
  backend-neutral validation of the bounded literal/local/arithmetic/direct-
  call graph facts consumed by direct projection. It owns neither target text
  emission nor a second program graph.
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
- `src/self_hosted/compiler/canonical_mir_identity_epoch_owner.pgy` --
  canonical MIR tree/directive identity adapter and program-level atomic
  composition boundary; rebinds nominal owners and topology directives into
  one reconstructed `AstTreeArtifact` epoch, then consumes the field identity
  epoch owner before publishing program facts.
- `src/self_hosted/compiler/canonical_mir_field_identity_epoch_owner.pgy` --
  declaration-field identity epoch owner; rebinds declaration fields and
  topology field references by exact `(owner, name, field_kind)` joins.
  Numeric equality, offsets, declaration order, and name-only fallback are
  forbidden.
- `src/self_hosted/compiler/driver_rung2_cli_owner.pgy` -- DRV-2 command-line
  mode selection and argument routing; consumes compiler-stage operations but
  owns no semantic or MIR facts.
- `src/self_hosted/compiler/driver_rung2_main.pgy` -- DRV-2 runnable hard
  semantic entrypoint; CLI ownership remains in `driver_rung2_cli_owner.pgy`
  and compiler-stage ownership remains in `driver_rung2_owner.pgy`.
- `src/self_hosted/compiler/authority_owner.pgy` -- authority contracts
  (abilities + roles) for the sensitive compiler-world boundaries: semantic
  verdict, C emission, subprocess planning, and parity judgement.

## LSP

- `src/self_hosted/lsp/main.pgy` -- LSP-0 runnable artifact boundary.
- `src/self_hosted/lsp/completion_owner.pgy` -- registry-directed LSP completion
  projection over all 145 language-word identities; exposure remains owned by
  the language keyword registry flags.
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
- `src/self_hosted/lsp/hover_content_projection_owner.pgy` -- generated
  presentation projection from `src/lsp/lsp_hover_content.def`; lowercase
  exposure remains owned by the language keyword registry HOVER flags.
