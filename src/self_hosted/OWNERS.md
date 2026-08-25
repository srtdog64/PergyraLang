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
- `src/self_hosted/lib/json_emit.pgy` -- shared JSON string escaping,
  call-local file quoting, and synchronous last-consumer retirement of owned
  renderer fragments for fact-shaped tools.
- `src/self_hosted/lib/json.pgy` -- shared bounded JSON string-read, top-level
  object/value bounds, and array-object row iteration.
- `src/self_hosted/lib/json_fact_table.pgy` -- shared object, array-object,
  and recursive scalar-field facts over bounded JSON spans.
- `src/self_hosted/lib/path.pgy` -- self-hosted source/import path string facts.
- `src/self_hosted/lib/text_scan.pgy` -- shared text-scan helpers.
- `src/self_hosted/lib/mir_decl_field_kind_vocabulary_projection_owner.pgy` --
  generated stable MIR declaration-field wire spelling and AST-label projection.
- `src/self_hosted/lib/intent_observability_abi_projection_owner.pgy` --
  generated complete stable-ID, source/runtime name, parameter-shape, result,
  and semantic-signature row from the intent observability ABI registry. The
  semantic signature owner and self-host C usage/symbol-rewrite owners consume
  this row; they may not recreate a builtin spelling table.
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
- `src/self_hosted/lexer/language_word_row_projection_owner.pgy` -- generated
  immutable complete ordered metadata row. Registry readiness and LSP bind one
  row per index instead of reopening eleven parallel 146-branch projections.
- `src/self_hosted/lexer/language_keyword_compatibility_projection_owner.pgy` --
  generated 70-row reserved lexer compatibility view. All generated lexer
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
- `src/self_hosted/parser/decl_intent_owner.pgy` -- intent declarations,
  singleton guard/post/expect admission, ordered compensation rows, and their
  parser-owned expression graphs.
- `src/self_hosted/parser/intent_parameter_resolution_owner.pgy` -- one final
  intent-header parameter-role projection after the complete import graph has
  supplied subject/zone identities; neutral parser rows may not escape it.
- `src/self_hosted/parser/intent_variant_binding_owner.pgy` -- exact
  one-payload variant-pattern syntax for typed intent step transitions.
- `src/self_hosted/parser/intent_terminal_clause_owner.pgy` -- exact legacy or
  step-labelled terminal intent expression clause parsing.
- `src/self_hosted/parser/intent_policy_clause_owner.pgy` -- intent mode and
  optional priority clause state, native-compatible AST rows, and the
  parser-owned priority expression graph.
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
- `src/self_hosted/parser/expression_graph_contract_owner.pgy` -- executable
  falsifying fixtures for parser expression graph construction; it owns no
  production graph facts.
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
- `src/self_hosted/parser/diagnostic_owner.pgy` -- parse-stage code/reason/fix
  table and the `pgy.selfhost.parse.v1` envelope. The parser used to reject in
  silence; a rejection now reports its reason before the caller stops. Output
  shape stays in `lib/diagnostic.pgy`.
- `src/self_hosted/parser/program_parse_owner.pgy` -- program-root assembly.
- `src/self_hosted/hir/ast_match_pattern_fact_owner.pgy` -- interprets the
  canonical typed `MatchCase` spelling as one bounded pattern fact. Semantic
  admission proves the full artifact once, then the ready-artifact projection
  keeps only local node/kind/atom checks for nested use sites. MIR consumes the
  admitted statement payload through the same text owner instead of rereading
  the typed-AST arena.
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
  evidence derived directly from the shared parser-owned `AstTreeArtifact`,
  including the canonical intent signature bundle consumed by later codegen.
- `src/self_hosted/semantic/ast_artifact_verdict_contract_owner.pgy` --
  executable valid, stale-identity, stale-expression-graph, missing-entrypoint,
  and duplicate-entrypoint witnesses for the artifact verdict owner.
- `src/self_hosted/semantic/ast_signature_fact_owner.pgy` -- artifact-bound
  function owner, name, formal-generic, parameter, mode, and return signature
  facts, including ordered function node/name identity for entrypoint
  cardinality, selection, and top-level function declaration routing.
- `src/self_hosted/semantic/ast_action_contract_fact_owner.pgy` -- callable-
  identity-bound `func`/`action` variant, subject ownership, body handle,
  action-only `requires`/`within`/`causes`/`authorized by`, and callable
  caps/effects rows. Codegen and MIR consume this owner rather than skipping
  typed rows or inferring action identity from clauses.
- `src/self_hosted/semantic/builtin_capability_projection_owner.pgy` --
  generated builtin-name and FileOpen-mode capability policy derived from the
  native registries; semantic consumers cannot reintroduce literal masks.
- `src/self_hosted/semantic/ast_capability_fact_owner.pgy` -- admitted
  expression-call capability facts, declared-vs-used validation, and
  interprocedural callable propagation. Manifest rendering may consume its
  masks but may not rescan builtin or source call spellings.
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
- `src/self_hosted/semantic/ast_nominal_constructor_lookup_owner.pgy` --
  duplicate-rejecting read-only nominal declaration lookup over the canonical
  constructor fact rows.
- `src/self_hosted/semantic/ast_nominal_constructor_artifact_match_owner.pgy`
  -- reverse artifact-consistency verdict and contract fixture for canonical
  nominal constructor rows; it imports the fact producer and never becomes a
  second producer.
- `src/self_hosted/semantic/nominal_constructor_argument_policy_owner.pgy` --
  semantic distinction between caller-supplied nominal constructor arguments
  and domain storage fields that require a topology/runtime materializer.
- `src/self_hosted/semantic/ast_expression_graph_nominal_constructor_call_owner.pgy`
  -- graph-owned nominal constructor prefix arity and argument-type verdicts;
  ordinary function exact-arity policy is not a constructor fallback.
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
- `src/self_hosted/semantic/ast_initializer_type_query_owner.pgy` -- read-only
  verification, first-diagnostic, and artifact-compatibility queries over
  initializer type facts; it does not produce or repair verdict rows.
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
  fact and typed scrutinee graph. Its admitted hot path consumes the
  ready-artifact pattern projection and must not reopen whole-artifact graph
  readiness per use-site ancestor.
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
- `src/self_hosted/semantic/ast_domain_query_protocol_owner.pgy` -- closed
  semantic protocol names, family, and arity for declaration-scoped domain
  observability calls; it does not own the keyword registry.
- `src/self_hosted/semantic/ast_expression_graph_domain_query_owner.pgy` --
  exact world/zone field-kind validation and symbolic leaf-node evidence for
  domain queries; ordinary value-environment injection is forbidden.
- `src/self_hosted/semantic/ast_expression_graph_call_view_owner.pgy` --
  canonical ordered callee/argument projection over parser-owned call spines;
  semantic and codegen consumers share this view. The owner restores source
  order in place and returns the same argument/generic-actual backings; a
  second ordered-array reconstruction is forbidden.
- `src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy` --
  DRV-2 MIR fixture manifest rows and their count contract; the CLI consumes
  this owner for --mir-fixture-manifest while the driver owner keeps the
  compile/verify pipeline.
- `src/self_hosted/semantic/ast_body_type_bundle_contract_owner.pgy` --
  self-checking contract fixtures for the body-type bundle owner, consumed by
  driver readiness; the bundle owner keeps production and readiness checks.
- `src/self_hosted/semantic/ast_body_analysis_admission_contract_owner.pgy` --
  fail-closed stale-identity and malformed producer-row witnesses for the
  one-time body analysis admission boundary.
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
  node-handle lookup plus read-only readiness, first-diagnostic, and artifact-
  compatibility queries over statement type facts; it does not produce rows.
- `src/self_hosted/semantic/ast_body_verdict_owner.pgy` -- document-order body
  verdict across initializer, iteration, assignment, and statement owners.
- `src/self_hosted/semantic/ast_body_analysis_admission_owner.pgy` -- one-time
  identity and parallel-row-shape admission for the producer-sealed semantic
  analysis consumed by body materialization; stage owners may not reconstruct
  the artifact proof behind this boundary.
- `src/self_hosted/semantic/ast_body_analysis_shape_owner.pgy` --
  reconstruction-free signature, role, intent, constructor, enum, and span
  shape proof consumed exactly once by body analysis admission.
- `src/self_hosted/semantic/ast_body_type_bundle_owner.pgy` -- canonical
  one-pass assembly of initializer, iteration, assignment, and statement type
  facts consumed by driver and codegen projections.
- `src/self_hosted/semantic/ast_body_type_bundle_readiness_owner.pgy` -- the
  exact reason a bundle is not ready. The `Ready` predicate stays with the
  bundle owner; naming every way a bundle can fail is a separate diagnostic
  responsibility, and it is the part that grows with each new checked fact.
- `src/self_hosted/semantic/ast_body_role_operator_resolution_owner.pgy` --
  body-level role-operator target overlay. It first detects whether the admitted
  declarations contain any role operator, resolves only that reached family,
  and otherwise checks that no carried role target escaped without rebuilding
  or revalidating the cumulative expression graph.
- `src/self_hosted/semantic/ast_body_type_bundle_admission_receipt_owner.pgy` --
  graph-wide body-type readiness admission performed once at the driver seam;
  downstream codegen consumes its fixed-size receipt instead of revalidating
  the cumulative semantic bundle.
- `src/self_hosted/semantic/ast_body_call_target_resolution_owner.pgy` --
  body-fixpoint resolution of canonical expression call-target rows.
- `src/self_hosted/semantic/ast_expression_call_return_type_owner.pgy` --
  one-time concrete call-result overlay derived from admitted signature and
  generic-specialization facts; empty rows are limited to compiler structural
  or builtin protocol targets absent from the source signature owner, and
  recursive codegen may not reopen flat rows.
- `src/self_hosted/semantic/ast_expression_call_return_type_diagnostic_owner.pgy`
  -- read-only node/target/projection/carried context for a rejected call-result
  overlay at the codegen boundary; it diagnoses but never produces graph rows.
- `src/self_hosted/semantic/ast_expression_place_fact_owner.pgy` --
  body-fixpoint value-category and place-kind rows for ref/inout argument
  lowering; codegen consumes the carried node fact without binding lookup.
- `src/self_hosted/semantic/ast_expression_identity_fact_owner.pgy` and
  `src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy`
  -- final source-syntax call
  target IDs and formal-parameter ordinals over semantic graph handles. They
  run after place/call-return closure; persisted MIR consumers may not recover
  either identity from node display text.
- `src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy` --
  semantic-owned direct generic call bindings keyed by expression call node;
  explicit calls and bounded inferred initializer calls share these rows.
- `src/self_hosted/semantic/ast_generic_specialization_query_owner.pgy` --
  read-only count, actual-type, shape, and expression-identity queries over
  generic specialization facts; it does not produce or infer bindings.
- `src/self_hosted/semantic/ast_expression_call_identity_owner.pgy` -- stable
  statement SyntaxNodeId, expression lane, and local-call ordinal identity for
  semantic call rows that cross into MIR; global graph indexes are not IDs.
- `src/self_hosted/semantic/ast_intent_signature_fact_owner.pgy` -- exact
  parser-artifact projection of intent declaration identity and ordered
  `involves`/`value` parameter facts used to admit intent calls without
  pretending that an intent is a function or action declaration.
- `src/self_hosted/semantic/ast_intent_call_fact_owner.pgy` -- exact direct
  nested-intent call admission by the carried expression-graph target,
  argument handles, and unique signature identity; source text is not reparsed
  and the owner never populates the action table.
- `src/self_hosted/semantic/ast_zone_authority_fact_owner.pgy` -- exact
  parser-artifact zone authority rows bound to one declared subject slot;
  required ability ranges carry role-owned node identity instead of DIR scans,
  and participant type or same-name fields cannot invent authority.
- `src/self_hosted/semantic/ast_zone_authority_validation_owner.pgy` -- deep
  AST cross-seal and fixed-carriage validation for admitted authority rows;
  lower consumers import this boundary and never rediscover authority syntax.
- `src/self_hosted/semantic/ast_intent_transition_fact_owner.pgy` -- exact
  enum-scoped step variant, explicit predecessor, and labelled terminal
  payload identity for typed intents; spelling-only and source-order fallback
  are forbidden.
- `src/self_hosted/semantic/ast_intent_transition_row_owner.pgy` -- canonical
  typed-intent row parsing, exact ordered step-header identity, and
  enum-node/local-index variant seals consumed by semantic transition and DIR.
- `src/self_hosted/semantic/ast_intent_expression_environment_owner.pgy` --
  intent-body expression environment dispatcher; it identifies the owning
  intent step and composes parameter plus step-local outcome facts.
- `src/self_hosted/semantic/ast_intent_parameter_environment_owner.pgy` --
  exact ordered participant/value aliases, types, and environment modes derived
  from the intent signature owner.
- `src/self_hosted/semantic/ast_intent_action_call_fact_owner.pgy` -- exact
  participant receiver to subject-action signature and return-type join for one
  intent `on` expression, plus the native-order explicit/receiver/step-action/
  sole-participant actor derivation consumed by legacy intent emission.
- `src/self_hosted/semantic/ast_intent_outcome_environment_owner.pgy` --
  step-local outcome plus explicit-predecessor and terminal payload bindings
  exposed only at their typed intent expression boundaries.
- `src/self_hosted/semantic/ast_body_expression_environment_owner.pgy` -- one
  body environment dispatcher that seeds either ordinary callable scope facts
  or exact intent participant facts before call-target resolution.
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
- `src/self_hosted/semantic/ast_role_artifact_match_owner.pgy` -- exact
  reconstruction-free cross-seal between role rows and the admitted parser
  artifact, including method ranges and aligned ability names.
- `src/self_hosted/semantic/role_operator_vocabulary_owner.pgy` -- canonical
  self-host role-operator kind, alias, method-name, suffix, and encoded target
  identity vocabulary. Semantic graph admission and legacy self-host codegen
  consume this owner instead of maintaining private operator tables.
- `src/self_hosted/semantic/role_operator_resolution_owner.pgy` -- exact
  expression-node role-operator resolution from admitted ability, role, impl,
  method signature, receiver, and graph facts. It publishes the carried target
  identity; operator spelling or a sole visible role cannot substitute for the
  join.
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
  alternate hard-codegen authority. Owns building and validating the graph.
- `src/self_hosted/semantic/ast_expression_graph_node_view_owner.pgy` --
  read-only per-node projection over a built graph: node text, node kind, call
  return type and target, binding kind/ordinal. Reading a node is a separate
  responsibility from proving the arena well-formed.
- `src/self_hosted/semantic/ast_expression_place_kind_owner.pgy` -- place
  classification: which place kind a node carries, and what that kind permits.
  Addressability and direct-binding are properties of the KIND, not of any one
  graph, so they are vocabulary rather than projection.
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
- `src/self_hosted/semantic/expression_declared_context_type_owner.pgy` --
  source-text declared-context projection for zero-argument collection
  constructors; builtin identity/type compatibility stays owned by
  `SemanticContextualBuiltinReturnTypeOpt`.
- `src/self_hosted/semantic/wrapper_type_owner.pgy` -- canonical
  Option/Result/Box type-shape and payload projection policy shared by legacy
  and graph lanes; nested Box payloads require a balanced outer wrapper.
- `src/self_hosted/semantic/collection_mutation_policy_owner.pgy` -- canonical
  mutator, collection type, and parameter-mode policy shared by source,
  statement-fact, and expression-graph consumers.
- `src/self_hosted/semantic/compiler_internal_builtin_caller_registry_owner.pgy`
  -- generated projection of the common compiler-internal caller registry;
  owns complete module/function/signature admission rows and path-boundary
  matching, not a consumer-local allowlist.
- `src/self_hosted/semantic/expr_validation_owner.pgy` -- expression validation facts.
- `src/self_hosted/semantic/program_check_owner.pgy` -- program/function and
  nominal constructor signature checks, including exact zone/world field rows.
- `src/self_hosted/semantic/enum_callable_signature_owner.pgy` -- atomic
  lightweight-checker projection of local and imported enum variant callable/
  value rows; it follows parser-owned comma-optional variants, erases payload
  labels, skips enum methods, and fails closed before publishing partial rows.
- `src/self_hosted/semantic/semantic_run_owner.pgy` -- semantic CLI run boundary.
- `src/self_hosted/semantic/source_bundle_owner.pgy` -- root/import source
  bundle ordering and one-pass TextBuilder assembly over a sealed source length.
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
  scalar and `Some(binding)`/`None` pattern facts shared by semantic and MIR;
  checked and ready-artifact entrypoints share the same local projection.
- `src/self_hosted/hir/ast_text_scan_owner.pgy` -- compact AST-text scanning
  primitives shared by parser/HIR and codegen.
- `src/self_hosted/hir/ast_text_row_fact_owner.pgy` -- compact AST text
  name/type/value/aux-value/mode row facts.
- `src/self_hosted/hir/ast_text_inventory_owner.pgy` -- compact AST text line
  inventory and cursor expectation boundary.
- `src/self_hosted/hir/ast_text_arena_projection_owner.pgy` -- single
  `AstTreeArtifact` construction and compact inventory to arena projection.
- `src/self_hosted/hir/ast_source_module_fact_owner.pgy` -- parser-owned
  top-level declaration-to-module provenance carried by the artifact identity
  and projected into callable signature facts.
- `src/self_hosted/hir/typed_ast_arena_owner.pgy` -- shared typed AST arena
  payload contract and `NodeId` lookup facts.

## Focused Substitution Probes

- `src/self_hosted/tools/generic_return_probe/main.pgy` -- executable
  exact/nested and explicit generic parameter/return projection,
  carried-target mutation, ordered-actual conflict, shared callable-signature
  identity, and structural mismatch proof.
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
  row ownership shared by producer, verifier, JSON, and MIR lowering; explicit
  zone authority rows remain a responsibility-named nested owner.
- `src/self_hosted/mir/declaration_callable_rows_owner.pgy` -- callable
  parameter, return, and action-contract declaration row projection. It owns
  no nominal or zone policy.
- `src/self_hosted/mir/declaration_generic_rows_owner.pgy` -- unerased
  nominal generic-parameter row projection from the typed AST node; default
  substitution stays a semantic consumer concern.
- `src/self_hosted/mir/declaration_zone_authority_rows_owner.pgy` -- aligns the
  semantic owner's explicit zone subject-slot and required-ability facts with
  one MIR declaration inventory; it never infers authority from actions.
- `src/self_hosted/mir/declaration_verify_owner.pgy` -- structural range and
  parallel-row verification for MIR declarations, including generic, method,
  role-slot, and role-implementation inventories.
- `src/self_hosted/mir/declaration_json_projection_owner.pgy` -- verified MIR
  declaration projection to `pgy.mir.v1`, including generic parameters,
  method parameters, party role slots, and role implementation ranges.
- `src/self_hosted/mir/runtime_call_abi_fact_owner.pgy` -- instruction-aligned
  runtime-call ABI carrier. Resource primary/auxiliary rows and the distinct
  runtime-value row carrier remain separate schemas and cannot overlap.
- `src/self_hosted/mir/runtime_value_call_abi_fact_owner.pgy` -- exact
  Allocator/TextBuilder instruction rows projected from the canonical runtime-
  value call fact, including stable internal identity and both target shapes.
- `src/self_hosted/mir/routine_expression_runtime_abi_owner.pgy` -- the one
  routine-expression attachment boundary for resource and runtime-value call
  rows. Claim resolves its type from the result SSA binding; runtime values
  require a direct builtin graph identity and never a spelling-only fallback.
- `src/self_hosted/mir/expression_runtime_abi_owner.pgy` -- projects plain
  `Slot<T>` requirements and root runtime-value builtin identity from carried
  expression-graph facts without reparsing source text.
- `src/self_hosted/mir/cfg_instruction_mutation_owner.pgy` -- canonical use-row
  replacement and runtime-call ABI attachment state transformations.
- `src/self_hosted/mir/expression_fact_owner.pgy` -- expression identifier-use
  and source-shape classification for MIR facts.
- `src/self_hosted/mir/expression_graph_fact_owner.pgy` -- instruction-owned
  expression graph root/range handles over the program-owned semantic graph.
  The bridge reads structural and call-target facts only through semantic
  accessors and fails closed on missing or foreign graph handles.
- `src/self_hosted/mir/expression_identity_json_projection_owner.pgy` -- the
  shared streaming/String projection of semantic call-target SyntaxNodeId and
  formal-parameter ordinal rows into each persisted expression node.
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
  value-leaf binding to SSA-use projection for migrated routine consumers;
  member-selector leaves are excluded by graph topology even when their
  spelling matches an active local, while source-text identifier scans remain
  a legacy bridge outside that migration.
- `src/self_hosted/mir/routine_build_owner.pgy` -- routine CFG build state,
  block edges, instruction IDs, termination, and binding-identity keyed local
  SSA inventory with lexical scope restoration.
- `src/self_hosted/mir/routine_lower_owner.pgy` -- bounded typed-artifact CFG
  lowering dispatcher over read-only compiler-scale input and mutable routine
  build state, including block-exit local-inventory restoration.
- `src/self_hosted/mir/routine_if_owner.pgy` -- conditional branch topology,
  branch-local build threading, iterative else-if tail frames, and reverse
  merge-block ownership without source-depth call-stack recursion.
- `src/self_hosted/mir/routine_match_owner.pgy` -- scalar case/default CFG
  topology plus arm exit/version carriage into the merge owner.
- `src/self_hosted/mir/routine_match_pattern_owner.pgy` -- MIR projection of
  the HIR-owned bounded pattern fact; source/payload recovery is forbidden.
- `src/self_hosted/mir/routine_match_merge_owner.pgy` -- N-way live-arm SSA
  phi emission and post-match continuation version ownership.
- `src/self_hosted/mir/routine_while_owner.pgy` -- while-loop header, body,
  back-edge, and exit-block lowering, including producer-owned exit merge
  dispatch after CFG predecessor closure.
- `src/self_hosted/mir/routine_local_predecessor_snapshot_owner.pgy` -- exact
  CFG predecessor identity and local SSA-version snapshots captured at break,
  continue, or fallthrough transfer boundaries.
- `src/self_hosted/mir/routine_loop_header_phi_owner.pgy` -- loop-header
  local-version preparation before the body is lowered.
- `src/self_hosted/mir/routine_loop_header_backedge_binding_owner.pgy` --
  producer-captured continue and fallthrough predecessor versions bound to the
  matching loop-header phi rows after CFG edges exist.
- `src/self_hosted/mir/routine_loop_exit_phi_owner.pgy` -- completion and
  break-exit local-version merge shared by while/range lowering; it emits the
  exit phi before any backend can observe the graph.
- `src/self_hosted/mir/loop_reachability_fact_owner.pgy` -- loop-body exit and
  back-edge reachability facts consumed before header phi emission.
- `src/self_hosted/mir/routine_for_owner.pgy` -- typed iteration row and
  semantic source/branch graph views to loop-initializer, body, header/backedge
  local phis, producer-owned break/normal exit merge, and exit-block lowering.
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
- `src/self_hosted/mir/routine_build_storage_lifetime_owner.pgy` -- the
  bounded final-carrier lifetime boundary that retires 88 copied CFG Array
  backings plus 5 already-dead routine-state backings, without freeing shared
  semantic/program-owned String elements. Earlier build scratch remains open.
- `src/self_hosted/mir/ast_arena_storage_lifetime_owner.pgy` -- the typed-AST
  arena carrier lifetime boundary. It retires 13 non-traversal backings after
  domain projection, carries the five traversal lanes plus parser-owned source
  module provenance through routine and intent materialization, then retires
  those seven backings after their final consumers. String elements and
  expression-graph facts remain borrowed and are never deep-freed here.
- `src/self_hosted/mir/body_type_bundle_storage_lifetime_owner.pgy` -- the
  post-MIR boundary for the 40 semantic body-type Array backings (20 Int and
  20 String). MIR rows keep borrowed String elements, so this owner retires
  carrier storage only after the final program-fact consumer.
- `src/self_hosted/mir/artifact_lower_owner.pgy` -- program assembly and
  deterministic instruction-ID canonicalization.
- `src/self_hosted/mir/intent_routine_owner.pgy` -- lossless typed MIR carrier
  for intent identity, participant/zone bindings, ordered steps, action
  receiver, authorization, effects, phase/rollback, mode, priority, and commit
  boundaries.
- `src/self_hosted/mir/intent_mode_carriage_owner.pgy` -- exact singleton
  `IntentMode` MIR row carrying the DIR-admitted `exclusive` or `concurrent`
  policy without source or name-table recovery.
- `src/self_hosted/mir/intent_priority_carriage_owner.pgy` -- exact zero-or-one
  `IntentEval(priority)` MIR row carrying the DIR-admitted priority expression
  surface, root identity, graph, source type, and uses without source rescans.
- `src/self_hosted/mir/intent_resource_lifetime_owner.pgy` -- MIR cleanup
  lifetime projection for DIR-owned intent resources: zone handles are retired
  once per zone participant, caused effects once per step, and placement
  invalidation only for a step with an admitted alias. It owns no source scan
  or fallback placement inference.
- `src/self_hosted/mir/intent_execution_fact_owner.pgy` -- target-neutral
  `mir.intent_step_transition` and `mir.intent_terminal_transition` execution
  validation: exact enum/payload identities, explicit predecessor handles,
  success-only completion, compensation action/graph seals, terminal result
  construction, and digest cross-sealing.  It does not infer branch roles from
  variant spelling or predecessor identity from row position.
- `src/self_hosted/mir/intent_execution_schema_owner.pgy` -- exact typed step,
  compensation, terminal, and plan carriers plus independently allocated empty
  facts.  It owns shape only; it does not validate roles or derive identity.
- `src/self_hosted/mir/intent_execution_digest_owner.pgy` -- native-order
  mutation digest projection over every carried intent execution field.  It
  owns hash order and arithmetic, not transition semantics or JSON admission.
- `src/self_hosted/mir/intent_instruction_append_owner.pgy` -- canonical
  intent instruction append plus atomic result, slot-anchor, and ABI type-name
  scalar attachment; callers cannot leave a partially-carried outcome row.
- `src/self_hosted/mir/intent_phase_contract_owner.pgy` -- producer-side
  `IntentCheck`/`IntentEval` phase vocabulary, exact step slot, graph presence,
  and on-only result/type shape checked before MIR artifact commit.
- `src/self_hosted/mir/intent_phase_emission_owner.pgy` -- graph-owned DIR
  guard/expect/post/on/ordered-compensate clauses projected to the native MIR
  phase wire without a source or AST reread.
- `src/self_hosted/mir/program_verify_owner.pgy` -- MIR row range/topology and
  required-fact verification.
- `src/self_hosted/mir/local_ref_fact_owner.pgy` -- canonical lexical binding
  identity and aligned instruction/direct-expression LocalRef rows. It owns
  `(role, owner_syntax_id, binding_index)` carriage, not display spelling.
- `src/self_hosted/mir/local_ref_identity_owner.pgy` -- the single canonical
  LocalRef role vocabulary, textual grammar, constructor, and validator shared
  by producer verification and admitted wire consumption.
- `src/self_hosted/mir/local_ref_json_projection_owner.pgy` -- conditional
  LocalRef JSON projection for routines whose source-local spellings collide;
  unique-spelling MIR retains its existing wire shape.
- `src/self_hosted/mir/routine_local_ref_attachment_owner.pgy` -- the builder
  boundary that attaches the active binding's canonical LocalRef to the last
  emitted definition, phi, loop-init, or loop-branch instruction.
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
- `src/self_hosted/mir/routine_param_json_projection_owner.pgy` -- the one
  routine-parameter JSON row shared by whole-string and streaming projection,
  including the complete physical ABI receipt when the type requires one.
- `src/self_hosted/mir/instruction_json_artifact_writer_owner.pgy` --
  sequential file framing for unbounded instruction-local expression graphs,
  match/destructure lists, and uses; runtime-call rows are delegated to their
  schema owner. It reads
  only verified `SelfMirProgramFacts`, retires owned row fragments immediately
  after synchronous writes, and cannot establish semantic facts.
- `src/self_hosted/mir/program_json_artifact_writer_owner.pgy` -- bounded
  program/routine/block file-artifact framing of the same verified
  `pgy.mir.v1` row order; instruction-local unbounded rows are delegated to
  the sequential artifact writer, renderer-owned fragments are retired at the
  write boundary, and `SelfMirProgramFacts` remains the semantic owner.
- `src/self_hosted/mir/abi_layout_json_projection_owner.pgy` -- self-host
  producer ABI-layout tuple and explicit dynamic-row projection.
- `src/self_hosted/mir/machine_layer_json_projection_owner.pgy` -- machine
  layer object projection nested in the pgy.mir.v1 instruction row.
- `src/self_hosted/mir/runtime_call_abi_json_projection_owner.pgy` -- nested
  resource and runtime-value `runtime_call_abi` String/streaming projections
  from their distinct instruction-owned fact rows.
- `src/self_hosted/mir/json_projection_support_owner.pgy` -- shared optional
  scalar projection used by the MIR JSON facade.

## MIR Lower

- `src/self_hosted/mir_lower/main.pgy` -- entrypoint only.
- `src/self_hosted/mir_lower/decl_lower.pgy` -- declaration reconstruction,
  including MIR-carried generic parameter constraints and default types; it
  never guesses a missing declaration default.
- `src/self_hosted/mir_lower/declaration_zone_authority_projection_owner.pgy`
  -- fail-closed `zone_authorities` schema validation and canonical AST-row
  projection from the MIR declaration owner, including exact subject-slot
  membership. Action contracts are not an authority fallback.
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
- `src/self_hosted/mir_lower/expression_graph_indexed_instruction_policy_owner.pgy`
  -- the same slot policy over borrowed program-index expression facts. It
  decodes text only for the rare policies that require actual contents.
- `src/self_hosted/mir_lower/expression_graph_parser_bridge_owner.pgy` --
  bounded parser-owned reconstruction of producer-only collection receiver
  roots during MIR graph reconsumption; it is not a codegen fallback authority.
- `src/self_hosted/mir_lower/structured_expression_emission_order_owner.pgy` --
  stable MIR instruction/lane/derived-ordinal occurrence order captured at the
  structured AST emission boundary; repeated CFG visits remain distinct rows.
- `src/self_hosted/mir_lower/expression_graph_semantic_occurrence_owner.pgy` --
  exact emitted MIR instruction occurrence to canonical semantic NodeId/root
  mapping after call-target and place analysis; producer IDs never alias the
  reconstructed AST identity epoch.
- `src/self_hosted/mir_lower/structured_condition_emission_owner.pgy` --
  CFG-owned branch conditions, `for` value/auxiliary lane identity, and derived
  match-binding occurrence order recorded at their actual AST emission point.
- `src/self_hosted/mir_lower/expression_graph_occurrence_owner.pgy` -- exact
  occurrence-key to MIR graph-slot selection; it rejects positional and textual
  lookup and appends directly into the one final sequence arena.
- `src/self_hosted/mir_lower/generic_specialization_fact_owner.pgy` --
  fail-closed MIR direct/member generic row decoder and final codegen-view
  projection; lane, call ordinal, callable, actuals, and symbol remain exact.
  Semantic rows are verifier evidence, not emitted-symbol input.
- `src/self_hosted/mir_lower/generic_specialization_identity_epoch_owner.pgy`
  -- one sealed producer/canonical owner map derived from the distinct
  within-epoch source preorder; raw numeric equality and offset inference are
  forbidden.
- `src/self_hosted/mir_lower/expression_graph_sequence_owner.pgy` -- bounded
  ordered graph-sequence construction over exact persisted graph captures.
  Match/destructure extensions preserve the admitted identity prefix and append
  only their new identity rows; they may not rebuild whole-program Unknown rows.
- `src/self_hosted/mir_lower/expression_graph_kind_code_owner.pgy` -- the
  persisted-shape vocabulary: exact JSON spans for expression-graph node,
  call-target, and binding kinds mapped allocation-free to their wire codes.
  The vocabulary mapping is not sequencing logic, and the sequence consumer
  must not materialize or remap those transient enum Strings.
- `src/self_hosted/mir_lower/expression_graph_persisted_read_owner.pgy` --
  one-pass persisted graph-HEADER capture, plus the shared index-bounds helper.
  It validates the exact schema while forbidding field-by-field JSON object
  rescans.
- `src/self_hosted/mir_lower/expression_graph_persisted_node_read_owner.pgy` --
  the same one-pass discipline for the persisted graph-NODE record. Two records
  with two shapes, so two owners.
- `src/self_hosted/mir_lower/expression_graph_persisted_shape_owner.pgy` --
  exact legacy/sealed persisted graph object shape and canonical digest-presence
  validation without consumer-side graph re-hashing.
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
- `src/self_hosted/mir_lower/phi_predecessor_binding_fact_owner.pgy` -- exact
  predecessor-to-incoming ValueId binding from typed CFG, SSA definition, use,
  phi, and dominance owners. Incoming array position is never predecessor
  identity; every slot is consumed once, while equal ValueIds may occupy
  distinct predecessor slots only when they remain the latest local value.
- `src/self_hosted/mir_lower/routine_definition_dominance_fact_owner.pgy` --
  routine block dominance plus strict definition ordering shared by phi and
  ordinary latest-local consumers.
- `src/self_hosted/mir_lower/latest_local_value_fact_owner.pgy` -- canonical
  latest dominating ValueId at an ordinary instruction use point. A merely
  dominating but shadowed SSA version is rejected without relying on numeric
  version order or source text.
- `src/self_hosted/mir_lower/ssa_identity_owner.pgy` -- consumer-side
  canonical `<source-local>.<version>` validation shared by phi and direct
  backend admission without importing producer version assignment internals.
- `src/self_hosted/mir_lower/fixture_manifest_owner.pgy` -- MIR parity
  source fixture manifest rows.
- `src/self_hosted/mir_lower/json_fact_read.pgy` -- bounded MIR JSON fact reads.
- `src/self_hosted/mir_lower/intent_execution_json_decode_owner.pgy` --
  exact-field-order object/scalar/string-array decoding for the native
  `intent_execution` wire format; it owns wire shape only, not semantic
  readiness.
- `src/self_hosted/mir_lower/intent_execution_json_rows_owner.pgy` -- exact
  step, ordered-compensation, and terminal wire-row capture, including the
  `has_predecessor` boolean-to-identity seal.
- `src/self_hosted/mir_lower/intent_execution_identity_index_owner.pgy` --
  one document identity index for routines, callable declarations, enum
  variants, and uniquely named `tobject` payload declarations.
- `src/self_hosted/mir_lower/intent_execution_plan_fact_owner.pgy` -- unchecked
  exact-wire plan carriage plus stable routine/declaration/instruction
  semantic joins. Full plan readiness belongs only to machine admission.
  Its focused executable evidence is
  `tests/self_hosted/parity/fixture/intent_execution_plan_json_admission_probe.pgy`;
  `tests/self_hosted/parity/intent_execution_plan_json_admission_owner.sh`
  delegates present/multi/interleaved/mutation evidence to
  `tests/self_hosted/parity/intent_execution_protocol_mutation_owner.sh`.
- `src/self_hosted/dir/domain_graph_fact_owner.pgy` -- bounded DIR census,
  graph-anchor identity, topology-producer orchestration, and exact zone-state
  row carriage.
- `src/self_hosted/dir/zone_state_row_fact_owner.pgy` -- exact zone-state
  identity, effect/relation layer-slot join, and participant-slot join. The
  parser-owned payload is decoded once at the DIR fact boundary; inventory and
  rendering may not reopen AST provenance or replace a state edge with a count.
- `src/self_hosted/dir/domain_graph_inventory_owner.pgy` -- exact program DIR
  node/edge inventory projected once from admitted declaration, intent,
  authority, and topology rows. Producer-local source syntax IDs may differ
  across native and self-host artifacts; DIR-local row and resolved-node
  identities may not. Aggregate census or graph-anchor values are not an
  inventory substitute. Zone-state edges consume the typed state row and keep
  the owning zone as their exact DIR-local resolved node.
- `src/self_hosted/dir/domain_graph_row_owner.pgy` -- canonical DIR graph row
  carrier, DIR-local node identity assignment, and exact node/edge append and
  lookup operations. It does not decide which semantic facts enter the graph;
  the inventory owner remains the only admitted-fact orchestration owner.
- `src/self_hosted/dir/intent_fact_owner.pgy` -- exact intent declaration,
  participant/value range, ordered-step range, and intent-edge census owner;
  it validates typed identities and never rescans source text.
- `src/self_hosted/dir/intent_child_policy_owner.pgy` -- exact typed child-kind
  admission policy for rows directly owned by an intent declaration.
- `src/self_hosted/dir/intent_mode_fact_owner.pgy` -- exact singleton direct
  mode-child identity and `exclusive`/`concurrent` spelling. The parser-owned
  default is materialized before DIR, so missing mode is never inferred here.
- `src/self_hosted/dir/intent_priority_fact_owner.pgy` -- optional intent
  priority node and parser-owned expression-root admission; it requires the
  admitted expression to resolve to `Int` and never invents a default row.
- `src/self_hosted/dir/intent_row_owner.pgy` -- compact read-only row projection
  for one admitted intent declaration, its exact participant slice, and exact
  guard/post/expect/ordered-compensation typed-AST identities.
- `src/self_hosted/dir/intent_step_clause_fact_owner.pgy` -- one-pass exact
  typed-AST child census and lossless intent-step clause collection; singleton
  clauses reject duplicates while compensation retains source order.
- `src/self_hosted/parser/intent_default_clause_owner.pgy` -- intent-level
  `who`/`where` parser clauses and their exact per-step application provenance.
  It emits the native AST contract wording once; DIR consumes the typed row.
- `src/self_hosted/dir/intent_step_carriage_contract_owner.pgy` -- structural
  range, node-kind, and child-census validation for intent-step row carriage.
- `src/self_hosted/dir/intent_step_provenance_fact_owner.pgy` -- typed
  explicit-versus-derived provenance flags for admitted intent steps. The
  renderer consumes these rows and may not re-infer action defaults from the
  final resolved names.
- `src/self_hosted/dir/intent_step_target_contract_owner.pgy` -- exact
  discriminated action-or-intent target identity, return, and arity validation
  over one carried DIR step.
- `src/self_hosted/dir/intent_step_fact_owner.pgy` -- one intent-step
  resolution owner for `on` receiver/action binding, semantic action-contract
  defaults, transfer endpoints, zone/using/who/requires/causes/authorized
  identities, and ordered predecessor edges. It consumes the clause and header
  owners without source rescans and carries guard/post/expect plus ordered
  compensation identities into DIR.
- `src/self_hosted/dir/intent_outcome_contract_owner.pgy` -- exact step-to-intent
  membership, participant receiver, subject-action signature, outcome node,
  name, and return-type validation over already-owned DIR rows.
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
- `src/self_hosted/mir_lower/mir_json_input_owner.pgy` -- MIR JSON input
  boundary and the file-backed input-buffer lifetime owner. Borrowed text APIs
  retain caller ownership; file-backed compilation retires the one raw JSON
  buffer only after every JSON-indexed consumer has projected typed facts.
- `src/self_hosted/mir_lower/abi_layout_admission_fact_owner.pgy` -- bounded
  required-layout row admission from instruction-owned spans. It parses the
  exact row once and returns the capture consumed by the Option match plan.
- `src/self_hosted/mir_lower/routine_instruction_match_fact_owner.pgy` --
  routine-local typed match index. It opens each instruction span once and
  owns pattern, variant, binding, and binding-type captures consumed by AIR;
  certificate and backend owners may not reopen those JSON spans.
- `src/self_hosted/mir_lower/mir_cfg_graph_owner.pgy` -- pure CFG distance,
  blocked-reachability, structural-merge, and dominator-edge queries used by
  the routine fact index. Its allocation-free proofs return zero backedges for
  a validated strictly-forward CFG and the canonical false-start merge for a
  terminal true arm with exactly one incoming edge; every non-proven graph
  continues through the exact query, never a numeric-backedge guess.
- `src/self_hosted/mir_lower/machine_layer_fact_owner.pgy` -- checked
  machine-contact projection validation for MIR JSON rows; it is the sole full
  readiness and semantic cross-seal boundary for the intent execution plan.
  Its admitted carrier preserves the already-built document index; downstream
  consumers must not revalidate plan/digest/graph facts.
- `src/self_hosted/mir_lower/parallel_capture_fact_owner.pgy` -- sealed parallel
  capture boundary/kind/writer fact validation for MIR JSON input.
- `src/self_hosted/mir_lower/program_declaration_index_owner.pgy` -- one
  document-order declaration identity/span inventory shared across canonical
  declaration-family projection phases. It consumes the document-owned
  declaration table carrier and preserves its row spans while composing field
  identity and enum variant indexes without reopening the JSON root. The
  production caller owns same-document provenance; this index does not claim an
  independently sealed document digest.
- `src/self_hosted/mir_lower/program_enum_variant_index_owner.pgy` -- one
  program-owned enum variant/name/ordinal/payload identity index built during
  declaration admission; condition and expression-graph consumers query it
  and never reconstruct the declaration graph from the JSON root.
- `src/self_hosted/mir_lower/program_declaration_field_identity_index_owner.pgy`
  -- one flattened owner/name/source-ID/field-kind identity index built from
  program declaration spans; topology consumers must use its exact join and
  fail closed on missing, non-positive, or duplicate field identities.
- `src/self_hosted/lib/json_bounded_fact_read.pgy` -- exact-bound JSON object
  and string-array fact reads that consume structure-owner spans without
  rediscovering the full document length. The string-array scanner owns comma,
  exact-end, and string decoding validity once for count and indexed reads.
- `src/self_hosted/mir_lower/assignment_binding_mode_fact_owner.pgy` --
  fail-closed comparison of carried MIR assignment modes with semantic
  assignment type facts; the named C-oracle bridge is excluded.
- `src/self_hosted/mir_lower/program_top_level_routine_order_owner.pgy` --
  typed source-epoch cursor that merges function and intent producer phases;
  row position carries only a monotonic producer-phase sequence, while source
  IDs own cross-phase interleaving.
- `src/self_hosted/mir_lower/program_lower.pgy` -- canonical declaration and
  source-preorder top-level routine assembly through that cursor.
- `src/self_hosted/mir_lower/intent_lower_owner.pgy` -- exact intent MIR
  stable import surface for the split intent routine-tree projection.
- `src/self_hosted/mir_lower/intent_routine_tree_projection_owner.pgy` -- exact
  admitted legacy/typed intent routine AST reconstruction orchestration;
  malformed cross-carrier identity fails before code generation.
- `src/self_hosted/mir_lower/intent_routine_carrier_projection_owner.pgy` --
  one bounded block-zero projection of legacy semantic/resource carriers and
  temporary executable mirrors; typed transition facts are not inferred here.
- `src/self_hosted/mir_lower/intent_routine_step_projection_owner.pgy` --
  per-step participant/action/outcome contract validation and legacy/typed AST
  step rows over already-admitted routine carriers and transition identities.
- `src/self_hosted/mir_lower/intent_step_placement_contract_owner.pgy` -- exact
  optional placement-carrier contract for legacy nested-intent calls. An
  admitted direct nested target may own zero zone/alias/invalidation/read rows;
  every other step must own the complete non-empty placement set.
- `src/self_hosted/mir_lower/intent_execution_structure_owner.pgy` -- typed
  intent legacy-spine and admitted-plan block ownership; unknown extra block
  IDs fail closed while legacy Bool retains its four-block contract.
- `src/self_hosted/mir_lower/intent_execution_tree_projection_owner.pgy` --
  predecessor-owned typed step/terminal tree rows and exact plan instruction
  occurrence order, without source or row-order topology recovery.
- `src/self_hosted/mir_lower/intent_execution_carrier_projection_owner.pgy` --
  plan-ordered step/outcome/compensation semantic carriers selected by exact
  block and instruction identities.
- `src/self_hosted/mir_lower/intent_execution_graph_mirror_owner.pgy` -- exact
  routine-local `{graph root,digest}` multiset seal between temporary legacy
  statement graphs and plan-owned on/compensation graphs, including duplicate
  identical expressions.
- `src/self_hosted/mir_lower/intent_execution_graph_target_owner.pgy` -- exact
  `action_syntax_id`/compensation target identity projection from source member
  spelling to the canonical declared member name consumed by semantic graphs.
- `src/self_hosted/mir_lower/intent_action_contract_owner.pgy` -- exact intent
  `on` expression-graph identity, subject-action declaration join, return type,
  stable action syntax ID, and declared authority-presence contract consumed by
  intent admission. An omitted action authority remains explicit absence; a
  required authority cannot disappear from semantic/resource carriers.
- `src/self_hosted/mir_lower/intent_carrier_projection_owner.pgy` -- canonical
  participant/value binding and ordered-step projection from admitted intent
  carriers, including exact companion-row cardinality and tree text rows.
- `src/self_hosted/mir_lower/intent_expression_carrier_contract_owner.pgy` --
  shared exact graph-presence contract for MIR-lowered intent expression rows.
- `src/self_hosted/mir_lower/intent_mode_projection_owner.pgy` -- exact
  singleton `IntentMode` carrier projection into the reconstructed intent tree;
  missing, malformed, and duplicate carriers fail closed.
- `src/self_hosted/mir_lower/intent_priority_projection_owner.pgy` -- exact
  zero-or-one priority-carrier projection into the reconstructed intent tree
  and expression occurrence order; malformed or duplicate carriers fail closed.
- `src/self_hosted/mir_lower/intent_phase_projection_owner.pgy` -- one admitted
  phase plan with exact step attachment, singleton cardinality, on-only result
  shape, graph presence, and source-ordered compensate ranges. It stages
  member arrays as distinct locals and materializes the projection once on
  success, so a rejected carrier never publishes partial phase state and the
  Pergyra-built subset does not depend on direct member-array inout.
- `src/self_hosted/mir_lower/intent_phase_tree_owner.pgy` -- compact direct
  `Intent` or action `On` plus Compensate/Guard/Post/Expect rows and matching
  graph occurrence order from the admitted phase plan; it owns no MIR or
  source rediscovery path.
- `src/self_hosted/mir_lower/intent_cleanup_contract_owner.pgy` -- bounded
  rollback/abort and invalidation/detach block validation for one intent routine.
- `src/self_hosted/mir_lower/routine_cfg_projection_owner.pgy` -- routine-local
  successor, block identity, loop-header, and loop-exit projection queries over
  the admitted routine fact index.
- `src/self_hosted/mir_lower/routine_instruction_view_owner.pgy` -- typed
  routine/block/instruction coordinate view over the program instruction
  identity; consumers cannot reopen kind/source/machine fields.
- `src/self_hosted/mir_lower/program_routine_index_owner.pgy` -- one admitted
  document-order routine/block/instruction structure view, including
  short instruction routing facts, borrowed expression/expression-graph spans,
  and raw machine spans. It consumes and preserves the already-admitted
  declaration index from the same source document; it must not rebuild that
  index or retain materialized program-wide render expressions.
- `src/self_hosted/mir_lower/program_routine_block_fact_owner.pgy` -- exact
  one-pass block-row schema capture for block identity, reachability,
  instruction bounds, and successor facts consumed by the program index.
- `src/self_hosted/mir_lower/program_instruction_expression_index_owner.pgy` --
  program-lifetime aligned borrowed routing, expression, and graph spans. Long
  text and optional routing values are never materialized into program-global
  string arrays.
- `src/self_hosted/mir_lower/program_instruction_routing_span_owner.pgy` --
  allocation-free literal comparison over borrowed `name`/`arg0` spans,
  with bounded decoding only for an actually escaped routing value.
- `src/self_hosted/mir_lower/program_routine_receiver_identity_owner.pgy` --
  exact routine source-ID uniqueness, declaration-owner join, and receiver
  carriage admission shared by routine index construction and validation.
- `src/self_hosted/mir_lower/routine_instruction_fact_bundle_owner.pgy` -- one
  routine-local aligned view that decodes the result/render scalars and raw ABI
  value bounds needed by reconstruction exactly once for that routine.
- `src/self_hosted/mir_lower/routine_instruction_use_fact_owner.pgy` -- one
  routine-local flattened view of admitted instruction `uses` arrays. It keeps
  use identity shared across backend consumers and rejects missing arrays or
  empty use identities instead of allowing backend-local raw JSON reads.
- `src/self_hosted/mir_lower/routine_result_definition_fact_owner.pgy` --
  unique routine-local SSA result definition identity and its global/block/
  instruction coordinates, derived from the admitted instruction bundle.
- `src/self_hosted/mir_lower/routine_instruction_scalar_capture_owner.pgy` --
  one bounded instruction-object walk that captures structural identity,
  machine spans, instruction name/render strings, ABI value spans, and
  expression-graph object spans; ABI syntax and semantic validation remain
  with `abi_layout_fact_owner.pgy`.
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
  ABI layout rows and stable-identity validation. Integer token materialization
  delegates to the exact-bounded JSON integer owner and never remeasures the
  complete MIR document.
- `src/self_hosted/mir_lower/routine_inventory_owner.pgy` -- routine inventory facts.
- `src/self_hosted/mir_lower/routine_lower.pgy` -- routine CFG/body reconstruction.
- `src/self_hosted/mir_lower/stmt_render.pgy` -- instruction fact to AST text rendering.

## Codegen

- `src/self_hosted/codegen/input/intent_execution_codegen_view_owner.pgy` --
  bounded codegen view of one admitted typed intent plan, canonical routine
  identity epoch, and its exact semantic expression roots.
- `src/self_hosted/codegen/input/intent_policy_codegen_view_owner.pgy` --
  canonical-routine keyed admitted intent mode and priority receipt consumed by
  C emission without reopening AST children or reconstructing graph roots.

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
  prototypes, function environments, and member calls; erased role rows also
  bind the exact concrete target type and its distinct value-or-identity
  carriage. MIR consumers exact-join admitted source ID/owner/name rows while
  source entrypoints derive the same fact once from verified semantic nominal,
  enum, role, and compiler ABI owners.
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
  it consumes the admitted concrete target carriage and only projects the C
  value-copy or pointer-identity prologue. It does not reopen role declarations
  or reconstruct the decision from type-environment nominal-kind strings.
- `src/self_hosted/codegen/run/codegen_run_owner.pgy` -- codegen CLI run boundary.
- `src/self_hosted/codegen/text/text_owner.pgy` -- codegen expression scanning and unsupported-surface policy.
- `src/self_hosted/codegen/text/owned_string_join_owner.pgy` -- consuming join and compiler-internal prefixed statement-line materialization for codegen-owned text fragments; exact emitter call sites are gated, borrowed fragments are forbidden, and only the final joined/materialized result survives.
- `src/self_hosted/codegen/text/enum_literal_owner.pgy` -- payload-free enum literal projection facts.
- `src/self_hosted/codegen/text/expr_scan.pgy` -- expression text scanning.
- `src/self_hosted/codegen/text/expr_sequence_owner.pgy` -- top-level comma-separated expression sequence facts.
- `src/self_hosted/codegen/text/struct_literal_call_owner.pgy` -- struct literal call-envelope facts.
- `src/self_hosted/codegen/text/struct_literal_field_owner.pgy` -- struct literal field-name/value entry facts.
- `src/self_hosted/codegen/text/struct_field_access_owner.pgy` -- dotted member-access field spelling projection facts.
- `src/self_hosted/codegen/type_facts/type_env.pgy` -- type environment facts,
  including declaration/role preseal epochs that keep the original global
  index, the ordered program-global delta, and function-local rows as distinct
  lookup layers. The program delta is sealed once into its own immutable index
  after the last role/operator row; it is never copied into each function-local
  environment or reparsed with the original global serialization. A local-row
  delta is copied once ahead of the retained newest-first prefix; suffix and
  intermediate-prefix reconstructions are forbidden. CSV field and parameter-
  mode scans compare the allocation-free character-code delimiter fact; they
  never allocate a one-character String for each scanned byte.
- `src/self_hosted/codegen/type_facts/type_env_state_lifetime_owner.pgy` --
  function-local type-row ownership, borrowed statement views, and retirement
  only after the expression emitter's last consumer has returned. It also
  consumes already-materialized `CodegenFunctionValueBindingFact.env_rows`,
  copies them into the function-local environment, and retires that temporary
  backing after the copy; statement emitters must not rebuild the same typed
  value-binding rows.
- `src/self_hosted/codegen/type_facts/type_env_local_row_materialization_owner.pgy`
  -- the function-local binding-row schema and its single-allocation text
  materialization. Borrowed names/types are appended into one owned result;
  nested `Concat` intermediates are forbidden.
- `src/self_hosted/codegen/type_facts/type_env_global_index_owner.pgy` --
  immutable same-epoch global type-row offset index; it owns no copied row
  strings and never scans the sealed global serialization during lookup.
- `src/self_hosted/codegen/type_facts/type_env_local_row_scan_owner.pgy` --
  dynamic local-scope row-boundary identity and first-row/newest-first lookup;
  value and presence consumers share the same row-start fact, and sealed
  global rows are forbidden here.
- `src/self_hosted/codegen/abi_layout/enum_abi_value_fact_owner.pgy` -- one
  semantic-enum-to-C-value/default ABI fact for payload-free and tagged enum
  consumers.
- `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy` -- self-host C ABI type spelling facts, including nominal struct type and empty parameter-list spelling.
- `src/self_hosted/codegen/runtime_abi/collection_runtime_owner.pgy` -- self-host C collection runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/checked_division_runtime_owner.pgy`
  -- checked integer division and modulo in emitted C. The raw operator let
  `x / 0` run to completion while the native pipeline panicked on the same
  source; signed division overflow is checked with it, while
  `INT64_MIN % -1` returns the language-defined zero remainder. Float division
  keeps the plain operator.
- `src/self_hosted/codegen/runtime_abi/checked_builtin_runtime_owner.pgy`
  -- the CheckedAdd and CheckedMul lowering. Neither had a lowering at all, so
  the declared fail-closed arithmetic surface did not compile through the
  default path. The helpers narrow to the Int32-ranged declared surface before
  checking, matching the native `((int32_t)(a), (int32_t)(b))` call rather than
  reporting an overflow the native oracle does not report. Distinct from the
  checked *cast* helpers owned by `checked_arithmetic_runtime_owner.pgy`.
- `src/self_hosted/codegen/runtime_abi/collection_bounds_owner.pgy` -- the
  bounds-guarded shape of an emitted element accessor. The accessors indexed
  raw, so an out-of-bounds write ran to completion in a program built by the
  default path while the native pipeline panicked on the same source. The guard
  reports through the panic contract the emitted prelude already carries.
- `src/self_hosted/codegen/runtime_abi/list_runtime_owner.pgy` -- canonical `List<T>` runtime ABI fact, supported element ABI, specialization macro, and constructor/operation-symbol projection.
- `src/self_hosted/codegen/runtime_abi/queue_runtime_owner.pgy` -- canonical
  `Queue<T>` runtime ABI fact, supported element ABI, and constructor/operation
  symbol projection.
- `src/self_hosted/codegen/runtime_abi/set_runtime_owner.pgy` -- canonical
  `Set<T>` runtime ABI fact, supported element ABI, and constructor/operation
  symbol projection.
- `src/self_hosted/codegen/runtime_abi/checked_arithmetic_runtime_owner.pgy` --
  fail-closed numeric conversion runtime symbol facts and the canonical
  target-library symbols for checked Long division and remainder.
- `src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy` -- self-host C host file/argv/process entrypoint runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/math_runtime_owner.pgy` -- self-host C math/random runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy` -- self-host C Option/Result runtime symbol facts.
- `src/self_hosted/codegen/runtime_abi/result_runtime_owner.pgy` -- explicit
  `Result<T, E>` runtime ABI facts and specialized helper symbol ownership.
- `src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy` -- the emitted C string/text runtime blocks.
- `src/self_hosted/codegen/runtime_abi/string_runtime_symbol_owner.pgy` -- what those runtime entry points are called: C symbol and format-string names. Naming is an ABI fact; emitting is code generation.
- `src/self_hosted/codegen/runtime_abi/intent_runtime_symbol_owner.pgy` --
  stable target-runtime spellings for intent admission/exit, observation, and
  MIR cleanup shared by self-host C and direct LLVM consumers.
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
- `src/self_hosted/codegen/runtime_abi/zone_runtime_owner.pgy` -- isolated C include projection for the canonical native/self-host zone lock and generation ABI.
- `src/self_hosted/codegen/emission/expr_rewrite.pgy` -- expression rewrite/lowering.
- `src/self_hosted/codegen/emission/expr_semantic_graph_emit_owner.pgy` --
  recursive expression emission from semantic node handles and child edges;
  codegen does not split migrated payloads to rediscover precedence. It is the
  semantic-check cluster root for the mutually recursive call and composite
  literal projection owners below.
- `src/self_hosted/codegen/emission/expression_c_text_materialization_owner.pgy`
  -- single-allocation materialization of common emitted C call, binary, and
  parenthesized expression shapes, plus the bounded lifetime epoch that retires
  recursive owned child fragments after the root has been selected; semantic
  routing and borrowed graph text remain with its callers.
- `src/self_hosted/codegen/emission/expr_semantic_leaf_place_contract_owner.pgy` --
  executable binding/value leaf projection contract: admitted place kind wins
  over flat enum/function spellings, and missing binding identity fails closed.
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
- `src/self_hosted/codegen/emission/expr_semantic_struct_call_emit_owner.pgy` --
  admitted struct-constructor field binding and single-builder C initializer
  emission; call dispatch does not reconstruct its field-row policy.
- `src/self_hosted/codegen/emission/expr_semantic_machine_call_emit_owner.pgy`
  -- typed DeviceSlot machine-call ABI selection and argument emission.
- `src/self_hosted/codegen/emission/expr_semantic_domain_query_emit_owner.pgy`
  -- admitted projection-readiness query emission, including exact nested-zone
  sync before observation.
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
- `src/self_hosted/codegen/emission/function_emit.pgy` -- function definition
  emission and function-local environment lifetime.
- `src/self_hosted/codegen/emission/function_prototype_block_owner.pgy` --
  program-scale prototype traversal and streaming emission into one builder.
  It consumes symbol and ABI owners directly; recursive parameter-prefix
  concatenation and construction of unused function binding-environment rows
  are forbidden.
- `src/self_hosted/codegen/emission/extern_prototype_block_owner.pgy` --
  host-ABI prototypes for extern "C" members: bare non-static declarations
  under the declared name, definition left to the linker. Distinct from the
  function prototype block because those symbols the unit defines and these
  it imports. Rows are recognized as signature-only with no owner name;
  ability members stay excluded by their ability owner.
- `src/self_hosted/codegen/emission/function_global_env_owner.pgy` -- one-pass
  serialization of admitted builtin, runtime, source, specialization, and
  callable-receiver and intent rows into the immutable global codegen
  environment.
- `src/self_hosted/codegen/emission/program_function_definition_block_owner.pgy`
  -- program-scale function-definition traversal and bounded materialization;
  each completed function string is released after transfer to the block
  builder instead of being retained in a parallel definitions array.
- `src/self_hosted/codegen/emission/intent_emit_owner.pgy` -- distinct intent
  prototype/environment/definition emission for admitted participant bindings,
  zone rebinding, action execution, projection synchronization, and caller
  value-result writeback; it does not reclassify intent as `func`.
- `src/self_hosted/codegen/emission/intent_action_step_emit_owner.pgy` -- one
  admitted zone-bound action step's materialization, call/outcome, sync, and
  caller writeback emission. It returns the cleanup expressions to the intent
  definition owner and does not own nested-intent execution.
- `src/self_hosted/codegen/emission/intent_nested_call_emit_owner.pgy` -- one
  placement-free direct nested-intent expression graph, Bool failure
  propagation, and step observability emission. The called intent remains the
  owner of its zone materialization and synchronization.
- `src/self_hosted/codegen/emission/intent_step_binding_owner.pgy` -- one
  actor/using/authority parameter admission and exact-alias-or-unique-type zone
  slot projection, including by-value versus inout zone C address spelling.
- `src/self_hosted/codegen/emission/intent_step_binding_contract_owner.pgy` --
  executable positive/negative contract for where/using identity, declared
  authority, exact-or-unique subject-slot selection, and zone address mode.
- `src/self_hosted/codegen/emission/intent_zone_subject_slot_owner.pgy` --
  exact-alias-first, otherwise unique-type subject-slot resolution shared by
  actor and authority binding; non-subject fields never satisfy the join.
- `src/self_hosted/codegen/emission/intent_signature_emit_owner.pgy` -- intent
  callable environment, parameter ABI, prototype, and local-binding C facts;
  intent remains a distinct Bool orchestration boundary.
- `src/self_hosted/codegen/emission/intent_control_flow_emit_owner.pgy` --
  legacy action-completion flags, Bool predicate failure edges, and full-policy
  reverse-step/reverse-expression compensation C emission. Typed variant
  success-only completion remains outside this owner.
- `src/self_hosted/codegen/emission/intent_mode_emit_owner.pgy` -- exact
  singleton reconstructed intent-mode lookup and C runtime concurrency
  projection; missing, duplicated, or unknown modes fail closed.
- `src/self_hosted/codegen/emission/intent_observability_emit_owner.pgy` --
  opt-in legacy intent enter/step/bind/materialize/fail/ok/exit C projection
  from the admitted mode, signature, subject, priority expression, and exact
  zone-slot facts.
- `src/self_hosted/codegen/emission/intent_priority_emit_owner.pgy` -- exact
  zero-or-one reconstructed priority-node lookup and admitted semantic-graph C
  projection. Absence owns the language default; duplicates and non-Int graphs
  fail closed without source or name-table fallback.
- `src/self_hosted/codegen/emission/intent_outcome_emit_owner.pgy` -- exact
  typed immutable C binding for one intent action result and step-local Bool
  `expect` dispatch through the shared intent control-flow owner.
- `src/self_hosted/codegen/emission/intent_execution_plan_emit_owner.pgy` --
  top-level typed intent definition assembly and exact semantic-signature
  consumption, with no legacy Bool fallback.
- `src/self_hosted/codegen/emission/intent_execution_plan_index_owner.pgy` --
  routine-local numeric transition/terminal lookup and admitted semantic graph
  expression/name projection for typed intent C emission.
- `src/self_hosted/codegen/emission/intent_execution_plan_local_emit_owner.pgy`
  -- typed intent local environment and C declaration materialization.
- `src/self_hosted/codegen/emission/intent_execution_plan_control_emit_owner.pgy`
  -- success-only completion, predecessor-only reverse compensation, numeric
  transition dispatch, and typed terminal result returns.
- `src/self_hosted/codegen/emission/role_dispatch_emit_owner.pgy` -- ability
  vtable types and instances, role method ABI declarations, party bind
  boundaries, and value-to-receiver operator adapters from semantic role facts.
- `src/self_hosted/codegen/emission/nominal_struct_emit_owner.pgy` -- nominal C
  struct layout and environment rows, including dynamic party role-slot
  storage and its dispatch-vtable field identity.
- `src/self_hosted/codegen/emission/program_statement_shape_owner.pgy` --
  recursive statement/block shape admission before program-level C assembly;
  semantic kind rows remain authoritative for each accepted statement.
- `src/self_hosted/codegen/emission/program_admitted_semantic_owner.pgy` --
  one admitted semantic-to-codegen adapter that materializes body views once
  and enters the reconstruction-free program emission core.
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
- `src/self_hosted/compiler/direct_mir_intent_plan_projection_owner.pgy` --
  exact producer routine/instruction identity join to canonical semantic
  occurrence roots; it performs no graph or plan revalidation.
- `src/self_hosted/compiler/domain_runtime_c_codegen_bridge_owner.pgy` --
  one-way renderer from the admitted target-neutral runtime plan to exact C
  role binding and projection-sync prologues; it owns no source, JSON, ordinal,
  or same-name recovery path.
- `src/self_hosted/compiler/intent_policy_c_codegen_bridge_owner.pgy` --
  exact semantic-DIR or machine-admitted-MIR intent policy join into the one C
  codegen receipt; the MIR route binds canonical routine identity, carrier row,
  and semantic expression occurrence without an AST fallback.

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
  six-block loop-break, or four-block identity-match inventory
  once, binds MIR, CFG, predecessor-resolved phi, nested-spine, and loop-spine
  facts, and permits only strict zero-fallback/zero-drift evidence with a
  fixed-size identity guard.
- `src/self_hosted/air/mir_cfg_certificate_readiness_owner.pgy` -- fixed-size
  post-issuance readiness for the common certificate. It consumes only carried
  identities and never reopens MIR, JSON, AST, or an expression graph.
- `src/self_hosted/air/mir_cfg_certificate_value_owner.pgy` -- immutable
  replacement constructors shared by issuance and repaired-digest negatives.
- `src/self_hosted/air/mir_cfg_certificate_mutation_owner.pgy` -- repaired-
  digest negative owner for outer, nested, while, range, loop-break, and
  identity-match certificate facts.
- `src/self_hosted/air/mir_cfg_certificate_fact_owner.pgy` -- fixed-size v7
  certificate identity shared by issuance and the target-neutral plan; it
  carries nested, while, range, loop-break, and identity-match digests without
  reopening MIR.
- `src/self_hosted/air/mir_identity_match_cfg_certificate_fact_owner.pgy` --
  optional common-CFG child binding the exact four-block topology, five
  instruction roles, one SSA definition, and the two exact SSA uses. It owns
  no enum declaration, ordinal, ABI, or target syntax.
- `src/self_hosted/air/mir_identity_match_cfg_certificate_mutation_owner.pgy`
  -- repaired-digest negative for the carried identity-match instruction rows.
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
  Its installed-artifact entrypoint returns the original bytes only after the
  same declaration is admitted; it is not a second physical serializer.
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
- `src/self_hosted/compiler/capability_manifest_owner.pgy` -- installed
  source capability-manifest orchestration and stable JSON projection from
  admitted semantic capability facts; it owns no builtin-name inference.
- `src/self_hosted/compiler/dir_text_artifact_owner.pgy` -- verified
  source-to-DIR debug artifact renderer over the exact admitted program
  inventory. It compares an independently owned domain census when present,
  never rebuilds rows from that count, and delegates admitted intent detail
  without reopening parser provenance.
- `src/self_hosted/compiler/dir_intent_text_artifact_owner.pgy` -- final DIR
  intent participant and ordered-step renderer. It consumes exact declaration,
  range, resolved-node, and typed provenance rows; it owns no action-default
  inference.
- `src/self_hosted/compiler/dir_text_row_format_owner.pgy` -- exact scalar
  index, padding, unresolved-node, and empty-value formatting policy shared by
  the DIR program and intent text renderers.
- `src/self_hosted/compiler/driver_bootstrap_main.pgy` -- installed compiler
  composition root used by producer parity and the integrated seed/oracle
  fixed point. It admits argv once through `driver_rung2_cli_request_owner.pgy`
  and delegates effects to `driver_rung2_installed_cli_owner.pgy`; it owns no
  mode or positional interpretation. The explicit
  `--emit-c-artifact-verified source output` request owns the public C artifact for
  `pgy --emit-c` and the admitted
  `--backend=c` compile/run envelope after the native selector admits it. The
  removed `source output` form is not a compatibility surface: it is rejected
  before publication so an option can never become a flag-named artifact. The
  native `c_runner` may consume that artifact only for host compile/link and
  optional execution; it cannot re-enter parser, semantic, MIR, AIR, or native
  codegen as a fallback. Its verified source-to-MIR and direct-MIR LLVM modes
  also own the admitted public `--backend=llvm` artifact pair;
  the native launcher may only select that pair, invoke it once per stage, and
  pass the resulting textual IR to clang. The host linker obtains the
  canonical external runtime object from `compiler_runtime_cache.c`; it does
  not infer linkage by scanning that IR. Pipeline ownership remains in
  `driver_rung2_owner.pgy`; test fixture manifests are excluded from this
  import graph. This target-specific substitution covers the currently
  admitted default-runtime rows, including host I/O; intent observability,
  composite-intent runtime programs, and package paths remain open.
- `src/compiler/driver_self_host_selection_owner.c` -- native launcher's one
  policy owner for deciding whether a plain C/LLVM binary request is inside an
  installed self-host artifact envelope and whether exact public `--tokens`,
  `--ast`, `--capability-manifest`, or no-source `--machine-manifest-json` is
  an admitted installed request. Unsupported options fail closed and cannot
  fall through to the native semantic/backend pipeline.
- `src/compiler/self_host_machine_manifest_artifact_owner.c` -- installed
  machine-declaration companion delivery boundary. It resolves the companion
  beside the selected Pergyra-built driver and invokes the exact verified
  read request once; it owns no physical literals or JSON serialization and
  cannot retry through the native pipeline.
- `src/compiler/compiler_transient_artifact_workspace.c` -- private transient
  artifact directory lifetime owner shared by the C and LLVM installed-driver
  runners. It owns path allocation and cleanup, not semantic or backend facts.
- `src/compiler/self_host_mir_artifact_owner.c` -- installed verified
  source-to-MIR artifact publication owner shared by package verification and
  LLVM materialization. It owns one sibling-driver invocation, output
  existence, and fail-closed diagnostics; it cannot re-enter native semantics.
- `src/compiler/self_host_llvm_driver.c` -- installed self-host LLVM materializer
  boundary. It invokes exactly one verified source-to-MIR producer and one
  direct-MIR LLVM projector. It does not inspect LLVM text to infer runtime
  policy or attach a runtime object. The final compiler boundary consumes the
  canonical default-runtime object independently of artifact spelling, while
  unsupported runtime profiles still fail closed.
- `src/compiler/compiler_self_host_artifact.c` -- final host compiler boundary
  for admitted self-host C and textual LLVM artifacts. It accepts no AST, MIR,
  AIR, libLLVM, or artifact-derived runtime policy. The LLVM leg consumes the
  canonical external runtime object owned by `compiler_runtime_cache.c` and
  cannot claim projection identity.
- `src/self_hosted/compiler/driver_rung2_execution_protocol_owner.pgy` --
  stable request, target, receipt, rejection, and artifact-failure protocol
  shared by the direct-backend and general MIR-to-C execution actions. It owns
  no compilation, artifact write, or world composition.
- `src/self_hosted/compiler/driver_rung2_execution_owner.pgy` -- reachable
  identity-bearing action boundary for admitted-MIR artifact publication. Its
  direct-backend and general MIR-to-C actions share one target acceptance and
  atomic commit transition inside the existing one-subject direct-MIR authority
  zone, while the typed MIR and backend owners remain unchanged.
- `src/self_hosted/compiler/driver_mir_c_protocol_owner.pgy` -- detached
  MIR-to-C payload admission receipt, rejection, readiness, and diagnostics.
  It keeps pressure observation independent from default versus explicitly
  verified machine-declaration identity, seals manifest ID/fingerprint, and
  carries the canonical target fact plus original compiler artifact. It owns no
  compilation, publication, or semantic/emission fact.
- `src/self_hosted/compiler/driver_mir_c_payload_execution_owner.pgy` -- sole
  MIR-to-C payload producer shared by the direct-MIR subject's read-only stdout
  and write-authorized artifact actions. It validates request identity and the
  canonical CPU-C target, dispatches the existing observed/unobserved compiler
  exactly once, and issues one typed admission without reconstructing backend
  facts.
- `src/self_hosted/compiler/driver_mir_c_stdout_execution_owner.pgy` -- checked
  read-only last consumer for installed MIR-to-C stdout. It enters the existing
  direct-MIR compiler-world zone, accepts only a ready typed admission, and logs
  the preserved compiler artifact payload without a commit or fallback.
- `src/self_hosted/compiler/driver_source_mir_protocol_owner.pgy` -- source-to-
  MIR request, identity/schema, detached payload/artifact receipt, rejection,
  outcome validation, and diagnostic protocol. It owns no compilation,
  artifact write, or semantic fact.
- `src/self_hosted/compiler/driver_source_mir_execution_owner.pgy` -- public
  source-to-MIR substitution subject/action/zone boundary. The exact public
  `pgy --mir-json <source>` selector reaches this owner through the installed
  sibling and cannot retry the native pipeline. It consumes the protocol owner,
  admits verified versus pressure-observed execution and subject/topology
  identity, then calls exactly one existing source-to-MIR payload owner. Its
  `io_read` payload action and `io_read, io_write` artifact action share that
  admission; only the latter owns one atomic commit. It owns no lexer, parser,
  semantic, DIR, or MIR fact.
- `src/self_hosted/compiler/driver_source_c_protocol_owner.pgy` -- detached
  source-to-C payload/artifact receipts, rejections, outcome readiness, and
  diagnostics. The typed request carries an admitted machine declaration; its
  payload receipt seals the declaration manifest ID/fingerprint and preserves
  the existing `CompilerEmissionArtifact` rather than copying payload, target,
  or capability facts. It owns no compilation, publication, or
  semantic/emission fact.
- `src/self_hosted/compiler/driver_source_c_execution_owner.pgy` -- production
  source-to-C admission and execution subject, exact existing compiler
  consumption, artifact transaction transition, and the sole
  `DriverSourceCZone` declaration. Its read-only payload action and
  write-authorized artifact action consume one admission owner. It owns no
  parser, semantic, MIR, target, or C-emission fact.
- `src/self_hosted/compiler/driver_source_c_stdout_execution_owner.pgy` --
  read-only last consumer for installed source-C stdout requests. It enters the
  existing compiler world, accepts only a ready typed payload admission, and
  logs the preserved compiler artifact payload without a commit or fallback.
- `src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy` -- active-slice
  composition owner for the single `PgyCompilerWorld`. It constructs the
  ordered direct-MIR, source-to-MIR, and source-to-C zones once through
  `PgyCompilerWorldMaterializeExecutableZones`; the public wrappers only
  delegate through world methods. This is not a physical no-copy claim. It owns
  no target, source, MIR, backend, or artifact fact and may not declare another
  world.
- `src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy` --
  backend-neutral hard-substitution boundary that receives one admitted MIR
  graph, claims enum-marked input before nominal/erasure/shape dispatch,
  selects the bounded scalar or verified-CFG path, and creates one C or LLVM
  artifact without rebuilding AST/semantic artifacts or creating backend-
  specific MIR readers.
- `src/self_hosted/compiler/direct_mir_pressure_observation_owner.pgy` --
  opt-in direct-MIR stage receipt vocabulary. It reuses the existing driver
  pressure prefix, emits only named owner boundaries, and is disabled by every
  default compiler path; it owns no semantic fact or fallback.
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
- `src/self_hosted/compiler/direct_mir_compile_time_declaration_erasure_owner.pgy`
  -- exact compile-time declaration admission and zero-runtime-materialization
  receipt. Only declaration kinds explicitly owned by the compile-time
  contract policy are claimed; enum and runtime nominal declarations cannot
  enter or retry this route. A shape-only declaration claim prevents malformed
  input from retrying another backend path, while semantic kind/name checks
  remain exclusively in the fact consumed by the literal-Log plan.
- `src/self_hosted/compiler/direct_mir_literal_log_plan_owner.pgy` -- one
  target-neutral plan for a terminal one-instruction `Log` whose semantic
  value is a canonical integer or safe string graph literal. It combines the
  declaration-erasure receipt, routine/block/use identity and structured
  formatted-print ABI without reparsing `expr0`; scalar `Int` keeps the
  canonical 32-bit format while `Long` remains a distinct ABI.
- `src/self_hosted/compiler/direct_mir_literal_log_emission_owner.pgy` -- C and
  LLVM text emission from the verified literal-Log plan only. It cannot read
  MIR declarations or create runtime nominal storage.
- `src/self_hosted/compiler/direct_mir_cfg_plan_owner.pgy` -- target-neutral
  plan issuer. It derives the admitted bounded shape or enum identity-match
  child from typed owners and issues one verified plan; no full outer
  certificate survives issuance.
- `src/self_hosted/compiler/direct_mir_cfg_plan_value_owner.pgy` -- immutable
  fixed-plan replacement constructors used by the issuer and negative owner.
- `src/self_hosted/compiler/direct_mir_cfg_plan_mutation_owner.pgy` -- repaired-
  digest negative owner for target fingerprint, phi, nested, while, range, and
  enum plan bindings.
- `src/self_hosted/compiler/direct_mir_cfg_plan_fact_owner.pgy` -- fixed-size
  v8 plan identity/readiness contract binding AIR/MIR/CFG/phi/nested/while/
  range/enum digests, target capability, topology, and normalized shape before
  emission. Break CFG execution is owned by the general scalar CFG plan.
- `src/self_hosted/compiler/direct_mir_enum_value_match_route_owner.pgy` --
  single-shot declaration-family route. Either captured declaration axis may
  claim enum input, but exact admission remains downstream and claimed failure
  cannot retry nominal, erasure, scalar, Option, or legacy CFG paths.
- `src/self_hosted/compiler/direct_mir_payload_free_enum_abi_owner.pgy` --
  target-neutral scalar-ordinal ABI fact with one materialization and zero
  payload, aggregate layout, payload storage, or runtime helper facts.
- `src/self_hosted/compiler/direct_mir_enum_value_match_plan_fact_owner.pgy`
  -- optional enum child carried by the one common CFG plan. It seals route,
  declaration, selected ordinal, independent case literal, arm literals, SSA
  identity, identity-match certificate, and ABI digests.
- `src/self_hosted/compiler/direct_mir_enum_value_match_plan_owner.pgy` --
  exact enum declaration/value/match admission from persisted owners. It does
  not consume display `expr0`, fixture names, or target syntax.
- `src/self_hosted/compiler/direct_mir_enum_value_match_plan_mutation_owner.pgy`
  -- repaired-digest negative binding selected ordinal to the enum ABI.
- `src/self_hosted/air/mir_option_match_graph_fact_owner.pgy` -- exact persisted
  graph shapes for the Option constructor, `IsSome`, `UnwrapOption`, match
  subject, and both Log lanes.
- `src/self_hosted/air/mir_option_match_cfg_certificate_fact_owner.pgy` --
  target-neutral seven-block Option match certificate. It binds exact CFG
  successors, typed pattern/binding capture, SSA-use cardinality, graph shapes,
  and the admitted ABI layout identity to one MIR/CFG digest.
- `src/self_hosted/compiler/direct_mir_option_match_abi_fact_owner.pgy` --
  compact verified Option<Int> ABI receipt carrying every physical field needed
  to reconstruct and recheck the canonical layout identity; it owns no target
  spelling.
- `src/self_hosted/compiler/direct_mir_option_match_abi_capture_owner.pgy` --
  one-way projection from the admitted canonical ABI row to the compact ABI
  receipt. Unsupported physical shapes fail before plan issuance.
- `src/self_hosted/compiler/direct_mir_option_match_abi_projection_owner.pgy`
  -- target-bound C or LLVM storage, aggregate, extension, and field-index
  projection from the verified ABI receipt. The unselected backend mapping is
  absent and cannot consume the receipt.
- `src/self_hosted/compiler/direct_mir_option_match_cfg_plan_fact_owner.pgy`
  -- fixed target-neutral Option match plan identity binding the AIR certificate
  and ABI receipt.
- `src/self_hosted/compiler/direct_mir_option_match_cfg_plan_owner.pgy` -- one
  admitted-plan issuer over the typed routine/use/match/ABI owners; it contains
  no backend emission or general compiler fallback.
- `src/self_hosted/compiler/direct_mir_option_match_cfg_plan_mutation_owner.pgy`
  -- repaired-digest negatives for certificate, canonical ABI identity, target
  capability binding, and enclosing plan identity.
- `src/self_hosted/compiler/direct_mir_option_match_cfg_emission_owner.pgy` --
  last C/LLVM text consumers for the fixed Option match plan. Neither emitter
  reads MIR JSON, reconstructs AIR, nor links Pergyra runtime symbols.
- `src/self_hosted/compiler/direct_mir_array_int_graph_fact_owner.pgy` -- exact
  typed expression-graph owner for the bounded local `Array<Int>` literal,
  reassignment target, length call, and index/add topology. It consumes the
  program-lifetime expression index and never reopens the MIR document root.
- `src/self_hosted/compiler/direct_mir_array_int_plan_owner.pgy` -- one target-
  neutral runtime-free aggregate plan over typed routine identity, latest SSA
  uses, graph facts, and the admitted canonical `Array<Int>` ABI row. Repaired-
  digest target, length, index, and layout mutations fail during issuance.
- `src/self_hosted/compiler/direct_mir_array_int_abi_projection_owner.pgy` --
  selected C or LLVM mapping for the verified array plan. The unselected
  backend spelling is absent, and physical field indices derive from the
  admitted offsets.
- `src/self_hosted/compiler/direct_mir_array_int_emission_owner.pgy` -- final C
  and textual LLVM consumers of the same fixed array plan. The bounded slice
  materializes stack-backed storage and links no Pergyra runtime symbol; it is
  not authority for general or runtime-bearing arrays.
- `src/self_hosted/compiler/direct_mir_array_return_graph_fact_owner.pgy` --
  typed direct-call and single-index expression facts for the bounded
  two-routine `Array<Int>` return graph; source text and routine row order are
  not call-target authority.
- `src/self_hosted/compiler/direct_mir_array_int_abi_fact_owner.pgy` -- one
  canonical captured `Array<Int>` ABI row predicate shared by local-value and
  returned-value direct-MIR plans, including every field offset, size, and
  alignment before target projection.
- `src/self_hosted/compiler/direct_mir_array_int_producer_fact_owner.pgy` --
  target-neutral identity, literal elements, and canonical ABI receipt for one
  pure no-parameter `Array<Int>` producer. Direct-call consumers bind to this
  receipt instead of reparsing the call or reopening the producer routine.
- `src/self_hosted/compiler/direct_mir_array_return_program_identity_owner.pgy`
  -- exact-one `Main`/producer identity and strict return-signature join. It
  binds the typed direct call target to stable routine syntax IDs without the
  legacy missing-return-to-`Void` default.
- `src/self_hosted/compiler/direct_mir_array_return_plan_owner.pgy` -- one
  target-neutral call/return/use/ABI/lifetime plan. The caller owns fixed
  backing storage, the producer cannot return its dead frame, and repaired-
  digest use/layout/target mutations fail before emission.
- `src/self_hosted/compiler/direct_mir_array_return_emission_owner.pgy` --
  final C/LLVM consumers for the same two-routine plan. Both materialize a real
  producer call with caller-owned storage and no Pergyra runtime symbol.
- `src/self_hosted/compiler/direct_mir_returned_array_program_route_owner.pgy`
  -- shared shallow `Array<Int>` return claim used by both returned-Array
  routes; routine and block counts alone cannot claim collection semantics.
- `src/self_hosted/compiler/direct_mir_returned_array_foreach_program_owner.pgy`
  -- exclusive multi-routine identity joining one `Main` scalar CFG to one
  admitted `Array<Int>` producer independently of routine row order.
- `src/self_hosted/compiler/direct_mir_returned_array_foreach_projection_owner.pgy`
  -- selected-target composition boundary that gives the producer receipt and
  exact `Main` row to the existing scalar-CFG plan and C/LLVM consumers.
- `src/self_hosted/compiler/direct_mir_routine_param_fact_owner.pgy` -- exact
  formal-parameter admission, including value/resource/pass carriage and the
  complete carried ABI row; a required row cannot be reconstructed by a
  backend.
- `src/self_hosted/compiler/direct_mir_routine_signature_fact_owner.pgy` --
  strict unique routine identity/signature facts shared by bounded
  multi-routine projections. Missing return facts and duplicate fields fail
  instead of defaulting. Ordered parameter rows are admitted once by
  `direct_mir_routine_parameter_set_admission_owner.pgy` and sealed as typed
  parallel identity arrays by `direct_mir_routine_parameter_set_fact_owner.pgy`;
  the latter deliberately owns no unsupported growable struct-array ABI.
- `src/self_hosted/compiler/direct_mir_inferred_generic_member_host_kind_fact_owner.pgy`
  -- passive generic-member host identity and the exact class/value or
  vessel/mutable-identity receiver-carriage join.
- `src/self_hosted/compiler/direct_mir_member_receiver_target_projection_owner.pgy`
  -- selected C/LLVM formal, local-storage, and call pass shape derived from
  the sealed host/carriage pair; emitters may render but not reclassify it.
- `src/self_hosted/compiler/direct_mir_array_argument_graph_fact_owner.pgy` --
  exact typed nested-call and Array-literal argument graph; direct call targets
  come from expression facts rather than routine row order or source text.
- `src/self_hosted/compiler/direct_mir_array_argument_program_identity_owner.pgy`
  -- exact three-routine envelope joining `Main`, the scalar callee, and the
  Array consumer to their strict signatures independent of row order.
- `src/self_hosted/compiler/direct_mir_array_argument_plan_owner.pgy` -- one
  target-neutral call/parameter/use/ABI/lifetime plan. Main owns fixed storage
  and passes the admitted Array aggregate by value.
- `src/self_hosted/compiler/direct_mir_array_argument_emission_owner.pgy` --
  final C/LLVM consumers of that plan; both preserve the two real calls and
  reopen neither MIR nor a Pergyra runtime path.
- `src/self_hosted/compiler/direct_mir_array_argument_legacy_route_owner.pgy`
  -- shrink-only one-block claim for the older Array-argument envelope; looped
  collection programs cannot be classified through this route.
- `src/self_hosted/compiler/direct_mir_collection_local_context_fact_owner.pgy`
  -- routine-qualified local/formal/call-result collection origins plus
  dynamic scalar input identities embedded in the shared `CollectionPlan`.
- `src/self_hosted/compiler/direct_mir_collection_program_route_fact_owner.pgy`
  -- coarse type-role claim for collection producer, consumer, and `Main`;
  malformed ABI or call edges remain claimed for fail-closed admission.
- `src/self_hosted/compiler/direct_mir_collection_program_identity_owner.pgy`
  -- exact row-order-neutral function/signature identity for the three semantic
  roles, including the formal Array ABI receipt.
- `src/self_hosted/compiler/direct_mir_collection_program_graph_fact_owner.pgy`
  -- persisted graph facts for empty collection, loop bounds, dynamic input,
  direct call, reduction call, and length observation shapes.
- `src/self_hosted/compiler/direct_mir_collection_program_routine_fact_owner.pgy`
  -- target-neutral producer, entrypoint, and consumer scalar/collection
  semantics and routine-qualified ValueId construction.
- `src/self_hosted/compiler/direct_mir_collection_program_instruction_abi_owner.pgy`
  -- exact Array instruction ABI admission shared by producer and entrypoint.
- `src/self_hosted/compiler/direct_mir_collection_program_producer_admission_owner.pgy`
  -- producer CFG, SSA, dynamic push, return edge, and reallocating Array ABI
  admission.
- `src/self_hosted/compiler/direct_mir_collection_program_consumer_admission_owner.pgy`
  -- formal-parameter length/Get reduction CFG and SSA admission.
- `src/self_hosted/compiler/direct_mir_collection_program_entrypoint_admission_owner.pgy`
  -- producer call result, consumer argument, ordered logs, and caller ABI
  admission.
- `src/self_hosted/compiler/direct_mir_collection_program_local_plan_owner.pgy`
  -- three routine-local projections of the shared `CollectionPlan`, with one
  canonical storage root and no cross-routine raw ValueId lookup.
- `src/self_hosted/compiler/direct_mir_collection_program_local_join_owner.pgy`
  -- exact join from the three role facts to origin, qualified ValueId,
  operation input, global-row, storage-root, and ABI rows in those local plans.
- `src/self_hosted/compiler/direct_mir_collection_program_edge_fact_owner.pgy`
  -- explicit return-to-call-result and argument-to-formal endpoint graph.
- `src/self_hosted/compiler/direct_mir_collection_program_plan_owner.pgy` --
  sealed program receipt joining the three local plans, edge graph, ABI,
  reallocating carriage, target capability, and mutation falsifier.
- `src/self_hosted/compiler/direct_mir_collection_program_c_emission_owner.pgy`
  -- MIR-blind C consumer with checked dynamic growth and single-owner cleanup.
- `src/self_hosted/compiler/direct_mir_collection_program_llvm_emission_owner.pgy`
  -- MIR-blind LLVM consumer of the same growth, carriage, reduction, and
  cleanup facts.
- `src/self_hosted/compiler/direct_mir_collection_program_projection_owner.pgy`
  -- selected-target composition boundary that issues one sealed plan.
- `src/self_hosted/compiler/direct_mir_composite_intent_program_route_fact_owner.pgy`
  -- exclusive structural claim for the canonical twelve-routine composite
  intent family; a claimed malformed family cannot fall through to the scalar
  program route.
- `src/self_hosted/compiler/direct_mir_composite_intent_program_graph_fact_owner.pgy`
  -- admitted declaration, zone-slot, method-mutation, and Main-expression DAG
  receipt for that family, without source-text or AST reconstruction.
- `src/self_hosted/compiler/direct_mir_composite_intent_program_plan_owner.pgy`
  -- sealed target-neutral subject, zone, method, intent-step, compensation,
  and observability plan derived only from admitted carrier and DAG facts.
- `src/self_hosted/compiler/direct_mir_composite_intent_program_llvm_emission_owner.pgy`
  -- MIR-blind LLVM consumer preserving three-zone state, leaf and aggregate
  intent execution, failure compensation, history, step, and trace output.
- `src/self_hosted/compiler/direct_mir_composite_intent_program_projection_owner.pgy`
  -- LLVM-only exclusive composition boundary issuing and consuming the
  composite intent plan before scalar admission.
- `src/self_hosted/compiler/direct_mir_nested_intent_program_route_fact_owner.pgy`
  -- exclusive structural claim for the one-function, one-method, two-intent
  nested-priority family before scalar-only admission.
- `src/self_hosted/compiler/direct_mir_nested_intent_program_graph_fact_owner.pgy`
  -- admitted declaration, routine, field, call, and observation DAG receipt
  for `Main -> Outer -> Inner -> method`, without source or AST reconstruction.
- `src/self_hosted/compiler/direct_mir_nested_intent_program_plan_owner.pgy`
  -- sealed target-neutral callable headers, implicit receiver, ordered intent
  bindings, literal/dynamic priority, step, expectation, and cleanup receipt.
- `src/self_hosted/compiler/direct_mir_nested_intent_program_c_emission_owner.pgy`
  -- MIR-blind C consumer preserving the same zone copy/sync, nested runtime
  priority, active-intent observations, and Bool result owned by the plan.
- `src/self_hosted/compiler/direct_mir_nested_intent_program_llvm_emission_owner.pgy`
  -- MIR-blind LLVM consumer preserving zone copy/sync, method mutation,
  nested runtime priority, active-intent observations, and Bool result output.
- `src/self_hosted/compiler/direct_mir_nested_intent_program_projection_owner.pgy`
  -- target-pair exclusive composition boundary issuing the nested plan once
  after composite intent and before scalar admission, then selecting C/LLVM.
- `src/self_hosted/compiler/driver_rung2_nested_intent_c_substitution_owner.pgy`
  -- exact source/MIR-to-C substitution boundary; claimed nested programs emit
  from the shared plan before the general MIR-to-AST reconstruction consumer.
- `src/self_hosted/compiler/direct_mir_legacy_intent_program_route_fact_owner.pgy`
  -- exclusive structural claim for the first executable one-function,
  one-subject-action, one-intent, one-subject/zone program family; a claimed
  malformed family cannot fall through to generic three-routine inference.
- `src/self_hosted/compiler/direct_mir_legacy_intent_program_graph_fact_owner.pgy`
  -- admitted Main construction/call and action-assignment graph receipt for
  that family, with no source-text or AST reconstruction.
- `src/self_hosted/compiler/direct_mir_legacy_intent_program_plan_owner.pgy`
  -- sealed target-neutral declaration, carrier-policy, placement, action,
  expectation, and cleanup receipt; mode spelling is erased to one concurrent
  bit before target materialization.
- `src/self_hosted/compiler/direct_mir_legacy_intent_program_llvm_emission_owner.pgy`
  -- MIR-blind LLVM consumer preserving subject mutation, zone copy/sync,
  runtime admission mode/priority, expectation, cleanup, and Bool observation.
- `src/self_hosted/compiler/direct_mir_legacy_intent_program_projection_owner.pgy`
  -- LLVM-only composition boundary issuing and consuming the sealed legacy
  intent receipt before scalar and generic three-routine routes.
- `src/self_hosted/compiler/direct_mir_multi_routine_terminal_projection_owner.pgy`
  -- late multi-routine decision that gives a claimed collection program one
  fail-closed path before the legacy three-routine classifier.
- `src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy` --
  target-neutral aggregate value-flow authority shared after family admission.
  It seals wrapper representation, element/Array identity, actual ABI evidence
  provenance, caller-owned single storage, index, construction/identity,
  allocator/lifetime/carriage, and the closed-module call receipt. It reads no
  MIR/JSON/classification or target spelling.
- `src/self_hosted/compiler/direct_mir_aggregate_value_flow_target_projection_owner.pgy`
  -- selected C/LLVM emission view joining the sealed flow with the public
  four-field Array storage projection. It owns internal linkage and allocator
  spelling for the selected target; family emitters retain element-specific
  input/output and nominal ABI projection but may not re-own flow constants.
- `src/self_hosted/compiler/direct_mir_role_operator_declaration_fact_owner.pgy`
  -- exact role/ability/impl/method declaration admission from the carried MIR
  rows, including method identity, target type, signature, and role range
  cross-seals. It does not infer a target from names or row order.
- `src/self_hosted/compiler/direct_mir_role_operator_plan_owner.pgy` -- one
  target-neutral role-call plan joining the producer-carried operator target,
  exact-one `self` signature, receiver/rhs/result types, method routine,
  literal result, caller use, CFG, and target capability. Claimed failure cannot
  retry primitive arithmetic or another multi-routine family.
- `src/self_hosted/compiler/direct_mir_role_operator_abi_projection_owner.pgy`
  -- selected-target call ABI view derived from the target-neutral plan and the
  canonical `Int` ABI: C `long long` or LLVM `i64`, alignment 8, direct receiver
  pointer, and direct scalar argument/return. The unselected target mapping is
  absent.
- `src/self_hosted/compiler/direct_mir_role_operator_emission_owner.pgy` --
  role-family projection boundary and final C/textual LLVM consumers. It issues
  the fixed role-call plan, selects one ABI view, and then gives only those
  facts to the chosen emitter. The LLVM method is internal; neither emitter
  reads MIR, expression text, or role vocabulary.
- `src/self_hosted/compiler/direct_mir_constructed_record_array_member_array_abi_absence_owner.pgy`
  -- sealed `Array<Point>` result evidence derived from the admitted MIR ABI
  receipt. It binds the typed result capture to physical-layout absence and
  supplies that exact digest to the shared aggregate value-flow fact.
- `src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy` --
  exclusive multi-routine direct-MIR projection boundary; rejection cannot
  retry hello, scalar, single-routine Array, Option, or CFG dispatch. Its
  observed adapter reports the existing scalar-route boundary without
  rebuilding the route fact.
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
- `src/self_hosted/compiler/direct_mir_llvm_text_format_owner.pgy` -- shared
  LLVM line-format byte encoding used by scalar and CFG text emitters.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy` --
  immutable target-neutral scalar CFG graph facts carrying local storage,
  ValueIds/LocalRefs, blocks, conditions, operations, predecessor-bound phi
  rows, typed local/value receipts, optional range/foreach receipts, one joint
  collection-pop receipt, and the selected String-concat runtime ABI identity.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_type_family_owner.pgy` --
  the bounded scalar-CFG type-family policy for `Int`, `Long`, `Bool`, `String`,
  and their currently admitted iteration pairs. It also owns the narrow
  `Option<Unknown>` source-local compatibility rule: only an already resolved
  canonical `Option<Int>`, `Option<String>`, or `Option<Bool>` may match. The
  first Long rung owns only the canonical literal and zero-parameter return
  representation; arithmetic, casts, and zero-argument calls remain
  fail-closed. Route classification consumes this policy; it does not infer a
  type from source-local arrays or block counts.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_value_type_owner.pgy` --
  derives one target-neutral result-ValueId type table from canonical local,
  phi, and operation facts. It must observe every definition of one local and
  reject missing or conflicting concrete types; source-local inventory consumes
  this completed type plan rather than selecting a first definition.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_local_inventory_owner.pgy` --
  exact source-local multiset admission after LocalRef normalization and value
  type consensus. An inferred Option source row may consume only the same local's
  resolved canonical Option type; names, routine identity, and source spelling
  are not type fallbacks.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_expression_owner.pgy`
  -- exact persisted-graph admission for String literals, direct `Concat`, and
  typed String `Log`; display `expr0` and call spelling are not fallback facts.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_typed_readiness_owner.pgy` --
  post-issue consistency for local/result types, String operation operands,
  foreach element receipts, and the selected concat ABI identity.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_graph_identity_owner.pgy` --
  stable digest construction and immutable digest replacement for that plan.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_operand_shape_owner.pgy` --
  the flat GraphPlan row operand-cardinality invariant. Readiness consumers use
  this owner instead of recreating value/local/literal exclusivity checks.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_graph_readiness_owner.pgy` --
  sealed plan cardinality, operand exclusivity, target identity, phi, and range
  topology readiness plus the repaired-digest negative.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_expression_base_owner.pgy` --
  expression sequence, call-target absence, canonical Int, and exact use-row
  joins shared by the three scalar expression admission operations.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_expression_owner.pgy` --
  typed persisted-expression-graph projection for the admitted Int/Bool
  literal, copy, comparison, addition, and Log subset; display `expr0` is not
  semantic input.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_leaf_operand_owner.pgy` --
  ValueId-first leaf resolution. Wire-required ranges use exact expression-node
  LocalRefs only for the wire-owned `expr0` lane; later expression lanes never
  reinterpret an equal node ordinal as an `expr0` LocalRef. The byte-stable
  single-range wireless shape admits only one
  iteration local with the requested spelling. A later expression lane may
  reuse one unique ValueId already consumed by the same instruction receiver
  or earlier lane; ambiguous prior matches fail closed. Spelling never
  overrides an explicit SSA use or selects a first/last candidate.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_direct_local_operand_owner.pgy`
  -- exact `(instruction,node)` LocalRef-to-iteration-slot resolution after
  ValueId admission; duplicate or missing identities fail closed.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_wire_local_ref_owner.pgy` --
  exact conditional LocalRef wire decoding and shape admission.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_wire_range_scope_admission_owner.pgy`
  -- receipt-keyed direct-use completeness plus CFG-dominance validation of the
  innermost active same-spelling range binder. With no range receipt it owns no
  foreign foreach ref; those refs remain mandatory inputs to the foreach and
  shared direct-local owners.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_local_ref_plan_owner.pgy` --
  target-neutral LocalRef-to-plan-slot normalization. Source-local inventory is
  admitted only after the value-type owner has resolved every local definition.
  Local/value/block identities are checked inside the
  routine partition that owns them; equal SSA spellings in different routines
  are not global collisions, and physical JSON row order is not storage
  identity. An admitted indexed-assignment result is retained as another SSA
  value of the exact parameter LocalRef so a later common `PhiValue` can join
  it; it is never reclassified as a source local. `inout_param` spelling alone
  is not a LocalRef or parameter-identity fallback.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_local_ref_identity_owner.pgy`
  -- scalar-CFG local-slot kinds and exact explicit-identity lookup.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_range_local_ref_identity_owner.pgy`
  -- canonical range LocalRef projection through `SelfMirLocalRef` plus exact
  receipt-set lookup; it neither rebuilds nor reparses the wire grammar.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_iteration_local_owner.pgy` --
  canonical range-receipt-to-local-slot bijection and bound receipt publication.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_assignment_target_owner.pgy`
  -- exact one-leaf assignment-place graph receipt joined to the instruction's
  carried source-local identity; display target text is not a fallback.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy`
  -- target-neutral plan issuer over typed MIR indices, use/dominance facts,
  exact phi bindings, range receipts, and the optional scalar-program dialect.
  It is the only block/operation/phi assembly loop; claimed invalid graphs
  cannot retry a legacy topology path.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_phi_operation_admission_owner.pgy`
  -- shared predecessor-order, local identity, type, and operation-row admission
  for every scalar GraphPlan phi. Int and String keep their existing operation
  identities; Bool, Long, Option<Int>, admitted collections, and logical-record
  joins use one target-neutral value-phi identity. Dialects do not reconstruct
  phi arrays or infer a record layout.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_graph_input_owner.pgy` --
  one decoded scalar-CFG input bundle shared by graph admission. It owns the
  admitted document/index/use/wire setup, not a second plan or route.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_graph_plan_seal_owner.pgy`
  -- the single immutable GraphPlan construction boundary. Final digest,
  readiness, and repaired-digest rejection are delegated once to
  `direct_mir_scalar_cfg_graph_plan_verification_owner.pgy`; admission owners
  contribute facts and do not issue parallel plans.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_routine_partition_fact_owner.pgy`
  -- canonical routine identity and contiguous local/value/block/operation/phi
  ranges inside that one flat GraphPlan. The paired mutation owner proves a
  shifted range cannot survive plan readiness.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_expression_owner.pgy`
  -- type-directed String/Int collection expression routing inside that one
  GraphPlan. It selects an already admitted receipt and never reopens a graph.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_instruction_graph_owner.pgy`,
  `direct_mir_scalar_cfg_instruction_position_owner.pgy`,
  `direct_mir_scalar_cfg_operation_position_owner.pgy`,
  `direct_mir_scalar_cfg_while_induction_owner.pgy`, and
  `direct_mir_scalar_cfg_array_guard_dominance_owner.pgy` -- element-neutral
  graph access, source/operation positions, zero-plus-one while induction, and
  unique true-edge dominance. String and Int collection lanes consume these
  owners instead of retaining type-named copies.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_graph_route_owner.pgy` --
  topology-independent per-routine classification by the supported
  operation/type envelope, never fixture names or exact block counts. A
  validated multi-routine composition may select its exact `Main` row.
- `src/self_hosted/compiler/direct_mir_scalar_program_route_fact_owner.pgy` and
  `src/self_hosted/compiler/direct_mir_scalar_program_route_admission_owner.pgy`
  -- immutable typed scalar-program route receipt plus its sole one-pass
  admission owner. The route carries exact admitted routine rows in canonical
  Main-first order, the optional admitted two-Int nominal representation, and
  the exact value-result `Array<Int>` parameter ABI. A declined route carries a
  stable owner/stage/routine/parameter receipt to the terminal dispatcher;
  neither owner claims or diagnoses by fixture name, display spelling, block
  count, routine-array order, or an exact routine count. The terminal consumer
  must not rescan MIR or replace that receipt with a coarse unsupported string.
- `src/self_hosted/compiler/direct_mir_scalar_program_expression_fact_owner.pgy`
  -- normalized typed expression-DAG arena shared by every program routine,
  including String concat/equality/inequality and routine-qualified calls.
- `src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_owner.pgy`
  -- graph/use/LocalRef normalization for literals, logical operators, Int
  arithmetic, String comparison, registry builtins, and zero-or-more-argument
  direct calls; formal
  parameters bind only through persisted syntax IDs/ordinals, never display
  text. Rejected def/Log/branch/return rows use the shared expression
  diagnostic owner rather than a silent `None` at the routine boundary.
- `src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_failure_owner.pgy`
  -- immutable first-failure receipt for expression normalization. It carries
  the exact rejecting admission stage and source-graph node to the terminal
  routine diagnostic without reopening the MIR graph or entering GraphPlan.
- `src/self_hosted/compiler/direct_mir_scalar_program_call_expression_admission_owner.pgy`
  -- typed ordered-argument direct-call admission. User routines join the
  canonical routine partition by persisted call-target syntax ID and ordered
  signature types, including contextual `None` arguments whose Option type is
  owned by the exact target parameter rather than the enclosing expression;
  builtin identity remains owned by the builtin registry.
- `src/self_hosted/compiler/direct_mir_scalar_program_expression_mutation_owner.pgy`
  -- small persistent append primitives for expression rows. It owns no
  semantic selection policy and prevents callers from aliasing growable arena
  arrays through a stale enclosing struct value.
- `src/self_hosted/compiler/direct_mir_scalar_program_expression_readiness_owner.pgy`
  -- exact node arity, type, endpoint, and literal invariants for that arena;
  raw modulo projection is admitted only for nonzero, non-minus-one literals.
- `src/self_hosted/compiler/direct_mir_scalar_program_intent_observability_readiness_owner.pgy`
  -- exact zero-, one-, or two-Int argument shape for an intent-observability
  call whose stable ABI ID was joined from the generated registry row.
- `src/self_hosted/compiler/direct_mir_scalar_program_numeric_cast_expression_kind_owner.pgy`
  and
  `direct_mir_scalar_program_numeric_cast_expression_readiness_owner.pgy`
  -- append-only `TypeName(Long)` and `Cast(Int, Long)` identities plus their
  exact source/target/type-name shape. Other numeric casts remain fail-closed.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_short_circuit_owner.pgy`
  -- LLVM branch/phi projection for logical right subtrees whose calls, indexed
  reads, or other operations may not be evaluated. It consumes the normalized
  expression topology and keeps ordinary non-trapping Bool trees on the linear
  `and`/`or` path; it never infers callable purity from spelling.
- `src/self_hosted/compiler/direct_mir_scalar_program_option_int_expression_kind_owner.pgy`,
  `direct_mir_scalar_program_option_int_builtin_signature_owner.pgy`, and
  `direct_mir_scalar_program_option_int_expression_readiness_owner.pgy`
  -- the reached Some/None/IsSome/UnwrapOption identities, canonical builtin
  registry projection, and exact typed node shapes for the first non-scalar
  value family in the shared program expression arena.
- `src/self_hosted/compiler/direct_mir_scalar_program_option_int_abi_owner.pgy`
  -- the one program-wide Option<Int> representation receipt captured from the
  persisted required MIR ABI row. Expression and complete by-value parameter
  rows consume the same receipt. It rejects missing or disagreeing layouts;
  consumers cannot infer tag order, field offsets, or backend type spellings.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_option_int_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_llvm_option_int_owner.pgy`
  -- target projections and expression materialization for that admitted ABI.
  Both targets consume the shared tag/value/print mapping and do not recreate
  an Option layout from the surface type name.
- `src/self_hosted/compiler/direct_mir_scalar_program_option_int_try_admission_owner.pgy`
  -- the reached try-let control-flow admission. It strips only the persisted
  unary try root, normalizes its operand through the shared expression owner,
  joins the existing OptionInt receipt, and requires an enclosing OptionInt
  callable without value-result copy-out in this first rung.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_option_int_try_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_llvm_option_int_try_owner.pgy`
  -- target control-flow consumers of that admitted operation. Both propagate
  None by returning the existing OptionInt absence value and write the Some
  payload to the declared Int local; neither rereads MIR or invents tags.
- `src/self_hosted/compiler/direct_mir_option_string_abi_capture_owner.pgy`,
  `direct_mir_scalar_program_option_string_abi_owner.pgy`,
  `direct_mir_scalar_program_option_string_expression_kind_owner.pgy`,
  `direct_mir_scalar_program_option_string_builtin_signature_owner.pgy`, and
  `direct_mir_scalar_program_option_string_expression_readiness_owner.pgy`
  -- the persisted Option<String> physical receipt and its contextual
  Some/None/IsSome/UnwrapOption expression family. The same receipt owner
  consumes complete by-value callable parameter ABI rows and cross-seals them
  with instruction-owned receipts; a parameter does not invent a second
  Option layout. Generic builtin identity is joined once with actual/expected
  types; Option<Int> is not a fallback.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_option_string_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_llvm_option_string_owner.pgy`
  -- C and LLVM materialization of the admitted tag-plus-pointer layout. Both
  targets consume the same layout identity and reject field-offset mutation.
- `src/self_hosted/compiler/direct_mir_option_bool_abi_capture_owner.pgy`,
  `direct_mir_scalar_program_option_bool_abi_owner.pgy`,
  `direct_mir_scalar_program_option_bool_expression_kind_owner.pgy`,
  `direct_mir_scalar_program_option_bool_builtin_signature_owner.pgy`, and
  `direct_mir_scalar_program_option_bool_expression_readiness_owner.pgy`
  -- the persisted eight-byte Option<Bool> physical receipt and its contextual
  Some/IsSome/UnwrapOption expression family. The same owner family admits the
  generic persisted `None` leaf through
  `direct_mir_scalar_program_option_absence_expression_owner.pgy`; it does not
  invent an Option<Bool> layout from `Option<Unknown>`.
- `src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_id_owner.pgy`,
  `direct_mir_scalar_program_expression_kind_owner.pgy`, and
  `direct_mir_scalar_program_bool_readiness_owner.pgy` -- stable expression 72
  for Bool equality and its recursive non-trapping proof. An Option<Bool>
  unwrap beneath `logical_and` remains conditional unless both equality
  operands are independently proven non-trapping.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_option_bool_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_llvm_option_bool_owner.pgy`
  -- C and LLVM projections of the admitted tag-plus-bool layout. Both targets
  consume the same size, alignment, field offset, and discriminant receipt.
- `src/self_hosted/compiler/direct_mir_scalar_program_two_int_nominal_abi_fact_owner.pgy`
  -- the optional program-wide two-field Int nominal declaration and physical
  ABI cross-seal. It admits one declaration row, checks formal-parameter and
  reached instruction receipts against that owner, and rejects unused or
  disagreeing representation facts.
- `src/self_hosted/compiler/direct_mir_scalar_program_two_int_nominal_target_owner.pgy`,
  `direct_mir_scalar_program_c_two_int_nominal_owner.pgy`, and
  `direct_mir_scalar_program_llvm_two_int_nominal_owner.pgy` -- the target
  projection and C/LLVM type materialization for that fact. Neither backend
  derives `{i32,i32}` or a C struct merely from the source type spelling.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_fact_owner.pgy`,
  `direct_mir_scalar_program_logical_record_expression_owner.pgy`, and
  `direct_mir_scalar_program_logical_record_expression_readiness_owner.pgy`, and
  `direct_mir_scalar_program_logical_record_payload_free_enum_join_owner.pgy`
  -- the program inventory of callable-referenced ordered logical-record
  identities and their constructor/member graph rows. Each candidate consumes
  the declaration index's canonical
  `source_module_path` and requires the exact nine-field wire object before
  exposing fields or callable identities. Distinct declaration
  rows remain distinct even when their field types and spellings match. The
  fact consumes the admitted declaration field index, closes nested record
  dependencies in dependency-first order, and checks exact instruction
  ABI-absence receipts. Variable field counts are declaration facts; cycles,
  missing dependencies, unreferenced same-shape declarations, and inferred
  physical layouts are rejected. `Array<Int>`, `Array<Bool>`, and
  `Array<String>` are bounded terminal field identities, not nested record
  dependencies. The enum join owner admits a nominal field as scalar-ordinal
  only when the payload-free enum inventory owns that exact type; neither the
  record owner nor a target backend reclassifies it from spelling.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_option_expression_kind_owner.pgy`,
  `direct_mir_scalar_program_logical_record_option_builtin_signature_owner.pgy`,
  `direct_mir_scalar_program_logical_record_option_expression_readiness_owner.pgy`,
  `direct_mir_scalar_program_c_logical_record_option_owner.pgy`, and
  `direct_mir_scalar_program_llvm_logical_record_option_owner.pgy` -- the
  ABI-free `Option<logical-record>` expression family. The wrapper shape comes
  only from `OptionPayloadTypeOpt`, and the payload identity must resolve to the
  declaration-keyed logical-record fact before target projection. C and LLVM
  derive an internal tag-plus-record carrier for this GraphPlan slice; it is
  not an interoperability ABI receipt. Record/routine spelling allowlists,
  `Option<Unknown>` substitution outside `None`, physical record-layout
  invention, and backend MIR rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_payload_free_enum_fact_owner.pgy`
  -- the declaration-keyed scalar-ordinal representation of callable-referenced
  payload-free enum value parameters. It consumes the admitted declaration and
  enum-variant inventories, requires a nonempty contiguous ordinal sequence,
  and joins exact `value` carriage with absent physical ABI rows. Enum spelling,
  routine-name branches, general Int relabeling, payload-bearing variants, and
  backend MIR rereads are forbidden. C and LLVM consume its canonical Int ABI
  type only after this fact is admitted.
- `src/self_hosted/compiler/direct_mir_scalar_program_payload_free_enum_expression_owner.pgy`,
  `direct_mir_scalar_program_payload_free_enum_expression_readiness_owner.pgy`,
  `direct_mir_scalar_program_payload_free_enum_match_condition_owner.pgy`, and
  `direct_mir_scalar_program_payload_free_enum_exhaustive_match_owner.pgy`
  -- declaration-owned payload-free enum variant constants and the exact
  value/variant equality join, including MIR match-pattern normalization and
  the exact proof for an empty fallthrough after a stable-scrutinee exhaustive
  match. The existing enum fact owns type,
  variant name, ordinal, and scalar-ordinal representation; expression
  admission stores only that admitted ordinal and both targets consume the
  sealed `i64` carrier. LLVM direct-call result and argument ABI projection
  also consumes the exact enum type fact, including an enum value returned by
  another callable; it never infers the ABI from a variant-literal node shape.
  Enum/routine spelling branches, ordinal inference, generic member fallback,
  treating arbitrary missing returns as unreachable, and backend MIR rereads
  are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_collection_abi_owner.pgy`
  -- the only join from collection-bearing logical fields to the already
  admitted Array ABI receipts. It does not copy offsets or authorize a record
  from field spelling alone; a required collection layout must be present.
- `src/self_hosted/compiler/direct_mir_array_bool_abi_fact_owner.pgy`,
  `direct_mir_scalar_program_array_bool_abi_owner.pgy`,
  `direct_mir_scalar_program_array_bool_abi_capture_owner.pgy`,
  `direct_mir_scalar_program_c_array_bool_materialization_owner.pgy`, and
  `direct_mir_scalar_program_llvm_array_bool_materialization_owner.pgy` -- the
  exact program-wide `Array<Bool>` storage receipt, its selected-routine
  admission capture, and target projections. The fact is the sole physical
  layout authority and also owns exact value-result routine/parameter/digest
  rows plus the exact owned-return presence receipt; capture may not rescan the
  whole program instruction table. Logical records, copyout emitters, and the
  ArrayBool return emitter consume the same receipt. Neither declaration
  spelling, ArrayInt identity, nor another Array layout may substitute.
- `src/self_hosted/compiler/direct_mir_scalar_program_array_bool_populated_literal_admission_owner.pgy`,
  `direct_mir_scalar_program_array_bool_nested_literal_owner.pgy`,
  `direct_mir_scalar_program_array_bool_populated_literal_readiness_owner.pgy`,
  `direct_mir_scalar_program_c_array_bool_populated_literal_owner.pgy`, and
  `direct_mir_scalar_program_llvm_array_bool_populated_literal_owner.pgy` --
  the exact nonempty `Array<Bool>` literal projection. The persisted expression
  graph owns its array spine and ordered canonical `true`/`false` leaves; the
  nested owner joins that spine to already-normalized Bool operands when a
  logical-record constructor owns the outer expression. The existing program-
  wide ArrayBool ABI owns storage, and C and LLVM consume only the normalized
  Bool operands. Source-text parsing, nonliteral elements, nonempty use rows,
  ArrayInt substitution, and backend MIR rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_target_owner.pgy`,
  `direct_mir_scalar_program_c_logical_record_owner.pgy`, and
  `direct_mir_scalar_program_llvm_logical_record_owner.pgy` -- the bounded
  target representation for that logical value carrier. Field ordinals come
  only from the ordered declaration fact, while offsets remain intentionally
  absent because the MIR owns no interoperability layout receipt.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_readonly_ref_owner.pgy`
  and
  `direct_mir_scalar_program_llvm_logical_record_readonly_ref_owner.pgy` --
  backend projections of signature-owned logical-record carriage. A
  `readonly-ref` is a C const pointer or LLVM pointer plus bounded aggregate
  load; an already-admitted `owner-handle` is the direct aggregate value in
  both targets. Neither backend may invent ownership, coerce readonly carriage
  to by-value, or admit an unaddressable call argument.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_value_result_policy_owner.pgy`,
  `direct_mir_scalar_program_logical_record_value_result_target_owner.pgy`,
  `direct_mir_scalar_program_c_logical_record_value_result_owner.pgy`, and
  `direct_mir_scalar_program_llvm_logical_record_value_result_owner.pgy` --
  the declaration-keyed logical-record `value-result` signature,
  target-neutral parameter identity, and C/LLVM copy lifecycle. One routine
  may carry one or more such copyouts, zero or more declaration-keyed readonly
  record inputs, direct scalar value parameters, and a scalar result.
  Readonly records remain indirect while every copyout remains direct;
  Direct-call identity joins the same target and source
  carriage facts; it cannot substitute the ArrayString copyout inventory.
  Every explicit return copies the complete aggregate back after loading the
  initial caller value. Name/arity allowlists, field-prefix copies, and
  backend MIR rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_member_rebind_owner.pgy`,
  `direct_mir_scalar_program_c_logical_record_member_rebind_owner.pgy`, and
  `direct_mir_scalar_program_llvm_logical_record_member_rebind_owner.pgy` --
  exact member-rebind facts for value-result parameters, by-value parameter
  copies, and ordinary logical-record locals. A target joins the exact
  signature carriage, carried LocalRef, result/predecessor
  ValueIds, canonical latest-dominating local fact when a same-local prefix is
  present, declaration member ordinal, member type, and the target-use prefix.
  Branch-exclusive inout writes and by-value copy writes consume the existing
  memory-local carrier and do not invent a predecessor from physical
  instruction order; ordinary local writes still require their predecessor.
  A by-value write never authorizes caller copyout. Non-record assignments are
  outside this owner. Target/source spelling, generic assignment-type guessing, a
  second record compiler, and backend MIR rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_mixed_collection_value_result_policy_owner.pgy`
  -- the exact declaration-keyed logical-record return plus mixed public
  collection copyout signatures. Both admitted families have four
  `Array<Int>` value-results at ordinals 0/2/3/4, two `Array<String>`
  value-results at ordinals 1/5; the ten-parameter family then carries ordered
  Int/String/Int/Int values, while the seven-parameter family carries one
  String value. It
  consumes the persisted Array ABI identities and existing C/LLVM copy
  lifecycle rather than reconstructing layout or emission facts. Routine-name
  branches, count-only matching, copyout reordering, first-array ABI
  substitution, general multi-array widening, and backend MIR rereads are
  forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_bool_mixed_collection_value_result_policy_owner.pgy`,
  `direct_mir_scalar_program_c_array_bool_value_result_owner.pgy`, and
  `direct_mir_scalar_program_llvm_array_bool_value_result_owner.pgy` -- the
  exact Bool-returning 11-parameter family with five ordered `Array<Int>`, one
  `Array<Bool>`, and two `Array<String>` value-results followed by
  Bool/Bool/String values. The policy consumes each persisted ABI identity;
  the backend owners copy the ArrayBool carrier in and out on every explicit
  return. LLVM materializes a mutable parameter-local carrier only when the
  shared operation inventory targets that parameter, then copies the latest
  aggregate back. Routine-name branches, count-only matching, ArrayInt
  substitution, copied layouts, partial return handling, and backend MIR
  rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_void_logical_record_array_int_value_result_policy_owner.pgy`
  -- the exact three-parameter Void signature joining one persisted public
  `Array<Int>` value-result, one declaration-keyed logical-record value, and
  one direct String value. The policy owns only complete signature admission;
  the existing ArrayInt and logical-record owners remain the physical and
  backend copy authorities. Routine-name branches, arbitrary Void widening,
  record-shape guessing, count-only matching, copied layouts, and backend MIR
  rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_inputs_value_result_policy_owner.pgy`
  -- the exact Bool-returning three-parameter signature joining three distinct
  declaration-keyed logical-record identities. Ordinals 0/1 are direct values;
  ordinal 2 alone is a direct value-result and consumes the existing record
  copyout route. The owner adds no physical ABI fact and does not authorize the
  caller-side three-record `AST_CALL` boundary. Routine-name branches,
  record-shape guessing, arbitrary value-result widening, count-only matching,
  and backend MIR rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_readonly_logical_record_array_string_value_result_policy_owner.pgy`
  -- the exact Bool-returning five-parameter signature joining one indirect
  readonly-ref declaration-keyed logical record, two direct String values, and
  two direct `Array<String>` value-results. The copyouts must carry positive
  equal persisted ABI layout identities. This policy creates no record or
  Array carrier authority; existing readonly-record and ArrayString C/LLVM
  owners remain the last consumers. Routine-name branches, broad signature
  widening, count-only matching, copied layouts, and backend MIR rereads are
  forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_bool_two_array_string_two_array_int_value_result_policy_owner.pgy`
  -- the exact Bool-returning eight-parameter signature with two direct
  `Array<String>` value-results, two direct `Array<Int>` value-results, and
  four direct String values. Each collection pair must carry one positive
  equal persisted layout identity, while the two collection families must
  remain distinct. This policy creates no carrier fact; existing ArrayString
  and ArrayInt C/LLVM owners remain the last consumers. Routine-name branches,
  broad mixed-copyout widening, count-only matching, copied or cross-family
  layouts, and backend MIR rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_readonly_logical_record_array_bool_return_policy_owner.pgy`
  -- the exact three-parameter owned `Array<Bool>` return signature joining one
  declaration-keyed indirect readonly-ref logical record with direct Int and
  Bool values. The signature policy creates no layout fact; the program-wide
  ArrayBool ABI owner must prove every return instruction carries its sealed
  layout before C/LLVM emit the aggregate value. Routine/record-name branches,
  general ArrayBool-return widening, guessed record shapes, copied layouts,
  and backend MIR rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_owned_logical_record_return_policy_owner.pgy`
  -- the exact single-parameter signature transferring one declaration-keyed
  logical record by `owner-handle` and returning that same record identity.
  It adds no destructor or physical layout: the source/MIR ownership carriage
  remains authoritative. The carried call-target identity and callable
  inventory admit the same owner handle at direct calls; existing logical-
  record C/LLVM value emitters remain the last consumers. Treating the handle
  as value, readonly-ref, or
  value-result, accepting a different return record, general owner-handle
  widening, routine/record-name branches, or backend MIR rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_readonly_logical_record_string_array_string_value_result_policy_owner.pgy`
  -- the exact Bool-returning signature joining one declaration-keyed indirect
  readonly-ref logical record, one direct String value, and one direct
  `Array<String>` value-result. The record inventory and persisted ArrayString
  ABI identity remain the fact owners; existing C/LLVM readonly-record and
  collection copy-in/out emitters are the last consumers. Routine/record-name
  branches, a broad one-copyout rule, record-as-value coercion, copied or
  missing ArrayString layouts, and backend MIR rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_return_array_string_value_result_policy_owner.pgy`
  -- the exact same-record return signature joining one direct ArrayString
  value-result, one direct declaration-keyed logical-record value, and one
  direct String value. The return must preserve the record parameter identity,
  while the persisted ArrayString ABI remains the sole collection-layout fact.
  Existing C/LLVM collection copyout, record-value, and record-return emitters
  are the last consumers. Name branches, broad record-return/copyout widening,
  identity mismatch, copied layouts, and backend MIR rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_void_logical_record_array_string_value_result_policy_owner.pgy`
  -- the exact Void signature family joining one direct ArrayString value-result
  with one direct declaration-keyed logical-record value and exactly zero,
  three, or four ordered direct String values. The admitted tail-cardinality
  set is the union of reached production shapes, not an arbitrary scalar tail.
  The logical-record inventory and persisted ArrayString ABI remain the fact
  owners; existing C/LLVM collection copyout, record-value, and String-value
  emitters are the last consumers. The retired three-String policy may not
  reappear. Routine/record-name branches, other tail counts, carriage coercion,
  missing or copied layouts, and backend MIR rereads are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_value_result_policy_owner.pgy`,
  `direct_mir_scalar_program_logical_record_array_value_result_target_owner.pgy`,
  `direct_mir_scalar_program_logical_record_array_target_owner.pgy`,
  `direct_mir_scalar_program_logical_record_array_empty_literal_admission_owner.pgy`,
  `direct_mir_scalar_program_c_logical_record_array_empty_literal_expression_owner.pgy`,
  `direct_mir_scalar_program_llvm_logical_record_array_empty_literal_expression_owner.pgy`,
  `direct_mir_scalar_program_c_logical_record_array_value_result_owner.pgy`,
  and
  `direct_mir_scalar_program_llvm_logical_record_array_value_result_owner.pgy`
  -- the declaration-keyed compiler-owned three-field `Array<Record>`
  `value-result` parameter identity, exact mixed-Void signature policies, and
  C/LLVM copy lifecycle. The logical-record
  inventory derives an array element declaration from canonical Array shape;
  the nominal-array layout owner supplies `data,len,cap`, C type spelling, and
  the LLVM aggregate. The exact Void shapes carry one such record array plus
  either one-or-more persisted public `Array<Int>` copyouts, one distinct
  direct logical record and two direct Int values, or one direct value of the
  array's own element declaration. The latter two declaration relationships
  are explicit identity joins, not spelling guesses. The same declaration and
  nominal-array owners also admit an exact routine-local `Array<Record>`
  carrier and its empty literal; C and LLVM materialize only the existing
  three-field target shape. Parameter admission is local to the parameter's
  type, carriage, resource, pass shape, and no-physical-ABI receipt so the
  unique callable role plan may compose it with a supported non-Void return;
  it does not require one of the exact mixed-Void whole-signature policies.
  The public four-field Array layout, record-name
  allowlists, arbitrary `Array<T>` widening, copied layout facts,
  one-copyout-only emission, and backend MIR rereads are forbidden. By-value
  record-array parameters, indexed reads, and record-array operations are
  owned by the next row rather than inferred from this copyout/local contract.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_value_parameter_policy_owner.pgy`,
  `direct_mir_scalar_program_logical_record_array_index_expression_owner.pgy`,
  `direct_mir_scalar_program_c_logical_value_expression_owner.pgy`, and
  `direct_mir_scalar_program_llvm_logical_value_expression_owner.pgy` -- the
  exact by-value compiler-owned `Array<Record>` parameter policy, stable
  indexed-read expression identity, and target-bound C/LLVM indexed load.
  The signature returns public `Array<Int>`, `Bool`, or the declaration-owned
  logical record, carries exactly one record Array plus direct scalar values,
  and joins its element through the declaration inventory. C and LLVM consume
  the existing three-field nominal-array target projection before reading
  `data`; the resulting record value then uses the existing ordered member
  identity. Record-name allowlists, public four-field Array substitution,
  return-type guessing, backend MIR rereads, and admitting local collection
  mutation through this read-only expression owner are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_array_int_value_result_fact_owner.pgy`
  -- the program-wide complete persisted four-field `Array<Int>` ABI row plus
  the exact subset of formal parameters that own value-result carriage.
  Instruction-local collection definitions, direct return rows, and admitted
  ArrayInt formals must agree on one layout; only value-result formals create
  copy-in/copy-out identity rows. Direct returns reuse the storage receipt and
  remain ordinary value carriers.
- `src/self_hosted/compiler/direct_mir_scalar_program_array_int_value_result_target_owner.pgy`,
  `direct_mir_scalar_program_c_array_int_value_result_owner.pgy`, and
  `direct_mir_scalar_program_llvm_array_int_value_result_owner.pgy` -- the one
  target-bound representation plus C/LLVM copy-in/copy-out boundary. Actual
  calls remain fail-closed until a caller-side value-result owner is admitted;
  they may not reuse the existing by-value direct-call path.
- `src/self_hosted/compiler/direct_mir_scalar_program_parameter_fact_projection_owner.pgy`
  -- the ordered parameter-row projection shared by typed scalar-program ABI
  owners. It consumes admitted routine bounds and never guesses a nonzero
  formal's ABI from the first-parameter shortcut.
- `src/self_hosted/compiler/direct_mir_scalar_program_array_string_abi_owner.pgy`,
  `direct_mir_scalar_program_c_array_string_value_result_owner.pgy`, and
  `direct_mir_scalar_program_llvm_array_string_value_result_owner.pgy` -- the
  program-wide join between the complete `Array<String>` layout, exact
  value-result routine/parameter identities, owned-return presence, and the
  target copy lifecycle.
  The bounded callable shapes are exactly Void with one such formal and Bool
  with `String, Int, Int` values followed by four such formals. Every explicit
  or fallthrough return copies all carried rows out, while callers admit only
  addressable locals or already value-result formals. The separate owned-return
  shape accepts a nonempty composable role plan, including admitted readonly
  logical records and direct scalar values, and may transfer its array only
  when every return-row ABI capture matches this same layout. The ABI owner
  consumes the callable's composable signature fact; it does not recreate the
  role plan. Unadmitted collections and spelling-only return admission remain
  forbidden.
  Same-mistake rule: join signature-wide value-result admission to the
  individual parameter policy, and inspect existing last consumers before
  adding a target-local owner. A singular helper that returns one parameter
  ordinal cannot select mutations after multiple copyout identities are
  admitted; operation targeting remains closed until it has an exact parameter
  fact.
- `src/self_hosted/compiler/direct_mir_scalar_program_string_index_expression_kind_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_string_search_expression_readiness_owner.pgy`
  -- stable StringIndexOf expression identity plus its exact typed two-argument
  readiness. They own no target syntax or search evaluation.
- `src/self_hosted/compiler/direct_mir_scalar_program_string_trim_expression_kind_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_string_transform_expression_readiness_owner.pgy`
  -- stable StringTrim expression identity plus exact typed unary-transform
  readiness. They own no target syntax or trim evaluation.
- `src/self_hosted/compiler/direct_mir_scalar_program_string_window_builtin_signature_owner.pgy`
  -- canonical bounded String window/search signature projection from the
  semantic builtin registry; consumers cannot own copied spelling/arity/type
  tables. `SubEqualsWithLen` is stable expression 71 and joins the existing
  five-argument Bool signature rather than a JSON-routine spelling exception.
- `src/self_hosted/compiler/direct_mir_scalar_program_string_runtime_requirement_owner.pgy`,
  `direct_mir_scalar_program_runtime_abi_fact_owner.pgy`, and
  `direct_mir_scalar_program_runtime_abi_projection_owner.pgy`
  -- the sealed runtime-call identity needed by normalized String expressions.
  The SubEqualsWithLen row consumes canonical operation `sub-equals-with-len`,
  symbol `pgy_subequals_with_len`, and call shape
  `string_int_string_int_to_bool`; target emitters cannot reconstruct it. The
  same sealed fact now projects the existing `host-io/args` registry row as
  `process_args_id`; `Args()` is an ordinary zero-argument collection
  expression and does not impersonate an instruction-local runtime-value row.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_process_args_materialization_owner.pgy`
  and
  `direct_mir_scalar_program_llvm_process_args_materialization_owner.pgy`
  -- target materialization of the sealed `host-io/args` receipt. They capture
  the entrypoint argc/argv carrier and duplicate every published argument
  String into the returned `Array<String>` backing. They do not own the Args
  spelling, signature, runtime symbol, or call-target identity.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_dir_walk_materialization_owner.pgy`
  and
  `direct_mir_scalar_program_llvm_dir_walk_materialization_owner.pgy`
  -- target adapters from the sealed `host-io/dir-walk` row to the native
  `pgy_dir_walk` runtime owner. The runtime remains the sole directory-walk,
  capability, ordering, and owned-path authority; these adapters only carry
  its ABI-compatible `Array<String>` result into GraphPlan target types.
- `src/self_hosted/compiler/direct_mir_scalar_program_host_io_runtime_requirement_owner.pgy`
  -- the single GraphPlan join from normalized host-I/O expression kinds to
  existing runtime-call ABI rows. It currently seals process Args, directory
  walk, file-existence, and file-read IDs; String runtime admission only carries these
  IDs and cannot recreate host-I/O symbols or call shapes.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_file_exists_materialization_owner.pgy`
  and
  `direct_mir_scalar_program_llvm_file_exists_materialization_owner.pgy`
  -- target adapters for the sealed `host-io/file-exists` row. Native
  `pgy_file_exists` remains the capability and filesystem fact owner.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_read_file_materialization_owner.pgy`
  and
  `direct_mir_scalar_program_llvm_read_file_materialization_owner.pgy`
  -- target adapters for the sealed `host-io/read-file` row. Native
  `pgy_read_file` remains the capability, bounded-I/O, failure, and owned
  String authority; the adapters do not reconstruct file-reading policy.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_string_window_expression_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_window_expression_owner.pgy`
  -- target consumers for ordered String-window operands. C preserves language
  `&&` evaluation and LLVM places a potentially trapping right-side runtime
  call under the existing short-circuit branch/phi owner.
- `src/self_hosted/compiler/direct_mir_scalar_program_string_transform_builtin_signature_owner.pgy`
  -- canonical bounded unary String-transform signature and actual-argument
  projection from the same semantic builtin registry.
- `src/self_hosted/compiler/direct_mir_scalar_program_callable_admission_owner.pgy`
  -- admission of the strict supported callable signature. The optional
  callable receipt and canonical-empty invariant live in
  `direct_mir_scalar_program_callable_fact_owner.pgy`, while ordered scalar
  signature support lives in `direct_mir_scalar_program_callable_signature_owner.pgy`;
  block and return admission remain owned by the shared per-routine GraphPlan
  builder.
- `src/self_hosted/compiler/direct_mir_scalar_program_callable_inventory_owner.pgy`
  -- the one typed callable catalog for every non-Main row in the admitted
  program representation. Direct-call nodes join its unique persisted syntax ID
  to ordered parameter and return types; consumers do not scan routine names or
  rebuild one optional callable per instruction.
- `src/self_hosted/compiler/direct_mir_scalar_program_runtime_abi_owner.pgy`
  -- the sole expression-kind-to-runtime-ABI requirement mapper. Its sealed
  case/math subfact owns StringReplace/Abs/Min/Max call identities, and its
  sealed StringIndexOf subfact owns search identity and the `-1`-or-byte-offset
  result contract. The registry-owned StringJoin ID is projected from the same
  ABI row; backends consume projections and do not infer calls.
- `src/self_hosted/compiler/direct_mir_scalar_program_builtin_signature_projection_owner.pgy`
  -- the bounded semantic-builtin signature join for GraphPlan expressions.
  It owns the canonical runtime-call ABI ID alongside arity and type facts;
  call admission only cross-seals the MIR-carried ID and may not requery a
  source-name registry.
  Its `ToString` specialization keeps String input as an identity value while
  Int input continues to require the registry-owned formatting ABI. Its
  `Print` specialization admits exactly `String -> Void`; expression readiness
  and C/LLVM String emitters consume the canonical `string|print|pgy_print`
  runtime row without rewriting Print as newline-producing Log.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_join_materialization_owner.pgy`
  -- the LLVM body for the sealed `Array<String>, String -> String` join ABI.
  It consumes the runtime projection and owns no builtin name or argument-type
  decision; the C path consumes the canonical String runtime block.
- `src/self_hosted/compiler/direct_mir_scalar_program_string_index_runtime_owner.pgy`
  -- canonical StringIndexOf ABI identity, result sentinel/range/unit, and
  signed-headroom contract consumed by both target materializers.
- `src/self_hosted/compiler/direct_mir_scalar_program_string_trim_runtime_owner.pgy`
  -- canonical StringTrim ABI identity, ASCII boundary set, null behavior, and
  owned-result contract consumed by both target materializers.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_extension_fact_owner.pgy`
  -- optional callable identity, typed expression-row links, per-block return
  rows, and nominal/String/closed-module ABI IDs carried by the existing CFG
  GraphPlan; it owns no duplicate CFG, SSA, phi, local, or operation arrays.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_extension_readiness_owner.pgy`
  -- range-aware cross-links from extension rows to the sole GraphPlan. It
  fail-closes cross-routine CFG edges, local/parameter/call identity drift,
  wrong return types, unsafe eager logical RHS, and unused expressions.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_extension_mutation_owner.pgy`
  -- executable negative proving an inactive extension cannot hide expression
  or closed-module ABI payload outside its zero digest.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_admission_owner.pgy`
  -- the small admitted-program composition boundary. It seals the optional
  callable and expression-derived runtime ABI IDs; it owns no storage loop,
  backend decision, or sibling plan.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_graph_storage_owner.pgy`,
  `direct_mir_scalar_cfg_program_graph_storage_mutation_owner.pgy`, and
  `direct_mir_scalar_cfg_program_value_storage_owner.pgy` -- layered persistent
  value, block, operation, and routine-count storage. Mutations return rebuilt
  owner values so growable Array reallocation cannot leave stale lengths in a
  copied aggregate.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_instruction_expression_owner.pgy`
  -- one instruction-to-expression-row admission boundary shared by Main and
  the callable for definition, Log, branch condition, and return positions.
- `src/self_hosted/compiler/direct_mir_scalar_program_builtin_argument_chain_owner.pgy`
  -- canonical call-marker and ordered argument-spine recovery. A nonzero
  call-target `SyntaxNodeId` is the direct-call identity carried to the
  callable inventory; namespace-local short source spelling is not compared
  with the canonical target name. Syntax-id-zero builtin calls retain their
  exact spelling check, and name-only direct-call fallback is forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_call_callee_identity_owner.pgy`
  -- structural direct/namespace callee topology joined to the persisted
  semantic call-target kind and `SyntaxNodeId`; namespace receiver/member
  spelling and local function-name scans are not alternate authorities.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_statement_admission_owner.pgy`
  -- one statement-operation admission boundary. It joins Log, Exit, and
  collection mutation targets to their expression and operation rows. A bare
  direct call may discard its non-Void result without rewriting the callable's
  admitted return type; non-call String/Int expressions cannot use that discard
  boundary. The routine owner retains only ordered traversal and state commit.
- `src/self_hosted/compiler/direct_mir_scalar_program_int_literal_expression_row_owner.pgy`
  -- the canonical raw-Int-literal constructor for the normalized expression
  arena. It owns row append only; instruction field admission remains with the
  instruction-expression owner.
- `src/self_hosted/compiler/direct_mir_array_string_literal_fact_owner.pgy` and
  `src/self_hosted/compiler/direct_mir_bounded_literal_index_owner.pgy` --
  shared target-neutral typed
  String-literal-array payload and fixed-cardinality index proof. The typed
  program and older local indexed collection route consume these owners rather
  than decoding parallel literal graphs.
- `src/self_hosted/compiler/direct_mir_scalar_program_nested_array_literal_seed_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_program_array_string_nested_literal_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_admission_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_operand_admission_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_readiness_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy`,
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_array_string_expression_kind_owner.pgy`
  --
  Array<String> literal admission into the existing typed ExpressionSet, one
  exact value/owner-formal or local-SSA String element identity, or an ordered
  literal spine containing already-normalized String expressions with exact
  instruction-use and LocalRef receipts, plus semantic readiness and the
  stable expression kind identity.
  The cleanup policy derives borrowed versus owned element storage for each
  local from the admitted expression and operation facts; the obsolete
  one-literal program boundary is not an alternate cleanup authority.
  The common seed owner is shared with nested `Array<Int>` admission and owns
  the one-seed/one-spine identity. Mixed literals retain source element order
  and derive `Array<String>` only from normalized String operands; they do not
  reuse the enclosing record expression's expected type. The constructor owner
  still seals the resulting array type to the exact declaration field type.
  They may carry non-parameter dynamic String rows; parameter operands remain
  single-element so owner-handle transfer cannot be silently downgraded to a
  borrow. The same owner family
  admits the exact semantic `ArrayDropOwnedStrings` signature only for an
  addressable local and projects the canonical owned-string drop symbol in
  both targets. It also admits `ArrayPushOwnedString` only for an addressable
  local or an exact value-result `Array<String>` formal, and both targets
  consume the canonical owned-push symbol that duplicates the String before
  the matching deep drop. Mapping that call to ordinary `ArrayPush` is
  forbidden. The persisted array spine owns element
  order; the routine parameter set and producer LocalRef own formal identity
  and type; the instruction-use/value-type owners additionally own local SSA
  identity and one-use consumption. Source spelling guesses, untyped dynamic
  elements, mixed parameter ownership, and a second expression parser remain
  forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_array_int_empty_literal_admission_owner.pgy`,
  `direct_mir_scalar_program_array_int_empty_literal_readiness_owner.pgy`,
  `direct_mir_scalar_program_array_int_expression_kind_owner.pgy`, and their
  C/LLVM expression owners -- the exact empty `Array<Int>` value reached by
  collection-index constructors. This rung does not authorize populated local
  storage or reuse the older shape-specific Array plan.
- `src/self_hosted/compiler/direct_mir_scalar_program_array_int_populated_literal_admission_owner.pgy`,
  `direct_mir_scalar_program_array_int_populated_literal_operand_admission_owner.pgy`,
  `direct_mir_scalar_program_array_int_populated_literal_readiness_owner.pgy`, and
  their C/LLVM expression/materialization owners -- canonical populated
  `Array<Int>` literals whose one-or-more elements are ordered canonical Int
  literals, zero-parameter direct calls returning Int, exact value-parameter
  leaves, or exact local SSA Int leaves. The local route consumes the existing
  instruction-use, LocalRef, and value-type plans and advances the caller's
  ordered use cursor; missing or foreign use identity fails closed. The same
  owner recognizes an exact one-or-more-element array spine
  when it is nested as a logical-record constructor field and the common
  expression owner has already normalized every element to Int. Literal, call,
  parameter, local, and admitted Int-expression rows therefore share the
  existing populated-array kind without reading the outer record type. The
  persisted array spine owns order; SyntaxNodeIds and the callable
  inventory own call target identity; the routine parameter set plus producer
  LocalRef own parameter ordinal/type; the existing ArrayInt ABI receipt owns
  storage. No same-type parameter guess, second literal decoder, tag-name table,
  empty-literal fallback, or backend MIR reread is permitted.
- `src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_policy_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_program_callable_route_envelope_owner.pgy`,
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_empty_owner.pgy`
  -- one
  callable value/carriage/ABI policy, broad typed claimant envelope with its
  exact failure assessment, and the canonical empty signature projection.
  Zero-parameter non-entrypoint callables are admitted only for bounded returns
  and C emits an exact `(void)` prototype. Standalone zero-argument call
  expressions stay closed; the populated ArrayInt literal owner admits only
  its exact internal admitted Int operands. Individual ArrayString
  value-result parameters are admitted from their local type, carriage,
  resource, pass-shape, and ABI identity rows; the program ArrayString ABI
  owner later cross-seals every captured layout. By-value ArrayInt, ArrayString,
  ArrayBool, and SetString parameters and by-value OptionString parameters
  consume their existing physical ABI receipts; no collection- or
  Option-specific layout is recreated here. Exact signature policies may constrain their specialized
  routes but are not a prerequisite for the composable callable lane. Target
  emitters do not reclassify return type, copyout count, or parameter position.
  Final signature readiness remains with
  `direct_mir_scalar_program_callable_signature_owner.pgy`.
- `src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_role_plan_owner.pgy`
  -- the unique declaration-independent plan that classifies every formal as
  exactly one direct-scalar value, declaration-keyed logical-record input or
  value-result, admitted ABI value (including the central SetString runtime
  value receipt), payload-free enum value, or admitted `Array<Int>`,
  `Array<String>`, or declaration-keyed `Array<Record>` value-result ABI role. Array value-result
  admission is parameter-local; the program ABI fact remains the owner that
  cross-seals captured layout identity across routines. The final signature
  may compose those roles
  with Void, scalar, owned `Array<String>`, or nonempty-parameter
  `Option<Int>`, `Option<String>`, and `Option<Bool>` returns. Each collection
  or Option physical receipt remains owned by its
  existing ABI owner; the role plan gains no return-layout authority. Exact
  routine names, arity/ordinals, overlapping role claims, generic Array
  widening, scalar-only Option parameter scans, backend MIR rereads, and
  return-specific parameter classifiers are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_set_string_value_result_policy_owner.pgy`,
  `direct_mir_scalar_program_set_string_value_result_target_owner.pgy`,
  `direct_mir_scalar_program_c_set_string_value_result_owner.pgy`, and
  `direct_mir_scalar_program_llvm_set_string_value_result_owner.pgy` -- exact
  `Set<String>` value-result signature identity plus C/LLVM copy-in/copy-out.
  The canonical Set runtime fact remains the storage authority; routine-name,
  ordinal-first, by-value, and backend MIR reread fallbacks are forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_array_string_readonly_ref_policy_owner.pgy`,
  `direct_mir_scalar_program_array_string_readonly_ref_target_owner.pgy`,
  `direct_mir_scalar_program_c_array_string_readonly_ref_owner.pgy`, and
  `direct_mir_scalar_program_llvm_array_string_readonly_ref_owner.pgy` -- exact
  `Array<String>` readonly-ref signature identity and indirect target
  projection. The persisted ArrayString ABI remains the physical authority;
  local callers pass an address and forwarding callers preserve the admitted
  pointer. Copy-in/out, by-value coercion, and backend MIR rereads are
  forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_program_process_exit_owner.pgy`
  -- the target-neutral process-exit operation inventory consumed by statement
  admission and both block emitters. The canonical symbol and
  `int_to_noreturn` call shape remain owned by
  `runtime_call_abi_structured_fact_owner.pgy` and its runtime registry row.
- `src/self_hosted/compiler/direct_mir_scalar_program_array_string_callable_abi_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_fact_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_admission_owner.pgy`,
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_plan_readiness_owner.pgy`
  -- exact join between canonical Array<String> ABI, caller-frame literal,
  by-value parameter, bounded callee index, borrowed result, and the sealed
  GraphPlan columns.
- `src/self_hosted/compiler/direct_mir_scalar_program_extension_abi_seal_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_cfg_program_extension_fact_readiness_owner.pgy`
  -- small
  extension ABI/boundary sealing and final extension-fact verification owners;
  they keep construction separate from mutation/readiness policy.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_control_transfer_admission_owner.pgy`
  -- the exact break/continue edge owner shared by the single-routine and
  program GraphPlan consumers. Break targets must not dominate their source;
  continue targets must dominate it. Both require one true successor, no false
  successor, terminator position, and no value uses.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_routine_admission_owner.pgy`
  -- the sole per-routine local/value/block/operation/phi append loop. Both Main
  and each callable use it with explicit offsets and consume the shared
  control-transfer fact instead of reconstructing break/continue topology.
  It also asks the canonical OptionString ABI owner to consume each routine's
  admitted parameter receipts before expression attachment; it does not scan
  parameter JSON or manufacture a layout locally.
  Collection mutation consumes the producer-owned primary `LocalRef` as its
  receiver identity. `ArraySet` then admits `expr1` index before `expr0` value
  from one ordered use cursor; a value-result parameter receiver consumes no
  expression-use row. Indexed assignment consumes the same stable mutation
  operation after the target graph proves its exact formal ordinal and the
  routine fact proves the predecessor SSA version.
  An explicit entrypoint `AST_RETURN_VOID` owns the same cleanup boundary as
  fallthrough, then projects target status zero; silently rejecting the source
  return or emitting a raw void return is forbidden.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_definition_admission_owner.pgy`
  -- the cohesive definition-operation selector shared by routine admission.
  It distinguishes the persisted unary try root from ordinary definitions and
  records stable operation 38 without adding a routine-name or JSON fallback.
- `src/self_hosted/compiler/direct_mir_scalar_program_array_mutation_target_owner.pgy`,
  `direct_mir_scalar_cfg_program_array_mutation_storage_owner.pgy`,
  `direct_mir_scalar_program_array_mutation_readiness_owner.pgy`, and
  `direct_mir_scalar_cfg_program_operation_shape_owner.pgy` -- one typed
  `Array<Int>`/`Array<Bool>`/`Array<String>` Push/Set/Pop fact. Stable ArrayBool
  operation identities 39/42 and ArrayString Set identity 34 are target-neutral:
  local receivers persist in `operation_left_locals`, while an admitted value-
  result parameter persists by exact ordinal in `operation_right_locals` and
  must join the existing program element ABI receipt. Push/Set values and the optional Set index
  persist in the primary and secondary expression rows, while Pop carries
  neither. MIR-use receiver recovery, unique-parameter inference, target-
  specific duplicate opcodes, and legacy collection-plan routes are forbidden
  fallbacks.
- `src/self_hosted/compiler/direct_mir_scalar_program_indexed_assignment_fact_owner.pgy`
  -- routine-local join from an `AST_ASSIGNMENT` target graph to one exact
  `Array<Int>` or `Array<String>` value-result parameter. It owns canonical
  target/index identity, element type, and the per-parameter SSA predecessor
  chain. A dynamic target index is admitted from the persisted target graph's
  right root before the RHS consumes the remaining ordered use rows. Routine,
  name, arity, first-parameter, and source-text inference are forbidden. The
  exact Array ABI type gates this owner before target parsing, so a logical-
  record member rebind remains owned by its separate member-target fact. The
  last consumers are stable array mutation operations 37 and 34, shared
  unchanged by C and LLVM; this route does not create a target-specific opcode
  or backend fork.
- `src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_indexed_assignment_owner.pgy`
  -- declaration-keyed typed fact for an `AST_ASSIGNMENT` whose target is one
  local or formal value-result logical record, one-or-more persisted member
  ordinals, and a terminal `Array<Int>` or `Array<String>` dynamic index. The
  source spelling is never split into a field path; the expression graph,
  LocalRef/value-type plan, formal carriage/ordinal, and logical-record field
  inventory own the join. Stable operation 44 preserves the complete target
  graph as its secondary expression row, the RHS as its primary expression
  row, and the resolved element type as its typed receipt.
- `src/self_hosted/mir_lower/latest_local_value_fact_owner.pgy` and
  `direct_mir_scalar_program_logical_record_member_rebind_owner.pgy` -- the
  canonical latest-dominating LocalRef value row and its operation-use prefix
  consumer. A value-result record's first mutation may consume the parameter
  entry without a use row, but once an explicit SSA value dominates the
  assignment that exact value must be the first persisted use. Missing,
  foreign, or stale predecessors cannot fall back to the parameter entry.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_definition_route_owner.pgy`
  and
  `direct_mir_scalar_program_logical_record_assignment_readiness_owner.pgy`
  -- the definition-route and typed-readiness consumers shared by direct
  logical-record member rebind and nested indexed assignment. They do not
  rescan MIR or infer a member path from `expr1` text.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_assignment_owner.pgy`
  and
  `direct_mir_scalar_program_llvm_logical_record_assignment_owner.pgy` -- last
  consumers of operations 40 and 44. Operation 44 roots the stored member
  ordinals at the admitted operation-result LocalRef, projects the address,
  and selects the existing bounds-checked `pgy_ai_set` or `pgy_as_set` from
  the typed receipt. C/LLVM do not own record identity, field spelling,
  predecessor selection, element typing, or target reconstruction.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_routine_partition_owner.pgy`
  -- canonical Main-first contiguous range construction over the flat GraphPlan
  storage, independent of admitted routine-array order. Routine signature and
  role arrays are assembled by the bounded
  `direct_mir_scalar_cfg_program_routine_identity_owner.pgy`.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_graph_admission_owner.pgy`
  -- loops over every canonical route row through one routine-admission call,
  constructs one callable inventory, partition, and extension, and calls the
  sole GraphPlan seal once.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_expression_owner.pgy`
  -- scalar/string expression admission and the one target-neutral PhiValue
  carrier classification. Bool, Long, Option<Int>, Option<String>, logical
  records/options, and admitted collection values share operation 29; target emitters do not
  reclassify it.
- `src/self_hosted/compiler/direct_mir_scalar_program_array_int_runtime_requirement_owner.pgy`
  -- target-neutral generated-runtime requirements. ArrayInt runtime bodies are
  selected from sealed operation and expression identities, never from ABI
  presence alone; C, LLVM, and foreign declarations consume the same decision.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_expression_owner.pgy`
  -- MIR-blind C expression rendering from the sealed typed arena and String
  runtime ABI receipts. Responsibility-specific direct-call, case/math, and
  generated-runtime owners render ordered calls and standalone runtime bodies.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_external_runtime_expression_owner.pgy`
  -- final C consumption of generated external runtime rows. Intent
  observability and runtime-value calls share dispatch but not ABI authority.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_numeric_cast_expression_owner.pgy`
  -- the explicit `long long` target projection for the admitted Int-to-Long
  identity; it never infers a target type from source spelling.
- `src/self_hosted/compiler/direct_mir_scalar_program_string_literal_fact_owner.pgy`
  -- exact decoding of persisted Pergyra String literal spelling into the
  sealed expression payload. It accepts only the owned ASCII escape vocabulary
  and consumes the bounded String decoder with an exact-end receipt.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_string_search_expression_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_c_string_index_materialization_owner.pgy`
  -- MIR-blind C StringIndexOf call rendering and runtime-body materialization
  from the sealed search ABI fact.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_string_transform_expression_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_c_string_trim_materialization_owner.pgy`
  -- MIR-blind C StringTrim call rendering and runtime-body materialization
  from the sealed transform fact.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_string_special_expression_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_c_string_scalar_materialization_owner.pgy`
  -- small C composition boundaries for the independent String search and
  transform expression/body owners; they own no semantic dispatch.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_literal_expression_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_cleanup_owner.pgy`
  -- MIR-blind C
  block-lifetime literal materialization and boundary-selected cleanup. The
  cleanup owner skips only the sealed borrowed-static local.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_set_string_expression_owner.pgy`
  -- MIR-blind C projection of admitted `Set<String>` construction, mutation,
  and lookup through the canonical Set runtime symbol fact. It does not infer
  a carrier layout or builtin signature from source spelling.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_emission_owner.pgy`
  -- MIR-blind range-driven C program rendering. One routine renderer serves
  entrypoint and callable; it never reopens admitted MIR.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_operation_owner.pgy`
  and `direct_mir_scalar_program_c_array_mutation_owner.pgy` -- MIR-blind C
  per-operation dispatch and local/value-result ArrayInt/ArrayString storage
  mutation from the sealed GraphPlan receiver and ordered expression rows.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_expression_owner.pgy`
  -- MIR-blind LLVM SSA expression rendering from the same typed arena and ABI
  receipts. Ordered signatures/direct calls and case/math runtime bodies live
  in target-specific owners rather than a second semantic dispatcher.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_external_runtime_expression_owner.pgy`
  -- final LLVM call/declaration consumption of generated external runtime
  rows, including intent-observability ABI IDs and target symbol spellings.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_numeric_cast_expression_owner.pgy`
  -- the representation-preserving Int-to-Long projection for GraphPlan's
  shared `i64` scalar ABI. The semantic source/target identity remains in the
  sealed expression facts rather than a backend width guess.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_search_expression_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_index_materialization_owner.pgy`,
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_llvm_substring_materialization_owner.pgy`
  -- MIR-blind LLVM StringIndexOf call/runtime materialization and checked
  Substring runtime materialization. The window owner composes these blocks and
  does not own their bodies.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_transform_expression_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_trim_materialization_owner.pgy`,
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_special_expression_owner.pgy`
  -- MIR-blind LLVM StringTrim call/body ownership and the small search/
  transform expression composition boundary.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_literal_expression_owner.pgy`
  and
  `src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_cleanup_owner.pgy`
  -- MIR-blind
  LLVM caller-frame literal materialization and boundary-selected cleanup from
  the same ownership fact as C.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_set_string_expression_owner.pgy`
  -- MIR-blind LLVM projection of the same admitted `Set<String>` operations;
  raw runtime declarations come from the canonical Set ABI owner and a
  by-value receiver is materialized only at the call boundary that requires an
  address.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_foreign_declaration_owner.pgy`
  -- one plan-derived declaration set for generated LLVM runtime bodies. It
  is shared by scalar-program and legacy scalar-CFG projections and owns
  declaration cardinality only; semantic bodies remain with their runtime
  owners. Bounds declarations follow the target-neutral helper requirement or
  an actual checked set/index operation, never ArrayInt ABI presence alone.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_global_owner.pgy`
  -- literal String global materialization, separated from expression policy.
- `src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_literal_owner.pgy`
  -- LLVM byte escaping for decoded scalar-program String literals. Quotes,
  slashes, and control escapes are encoded here rather than concatenated into
  IR by the global owner.
- `src/self_hosted/compiler/direct_mir_scalar_program_comparison_expression_kind_owner.pgy`
  and `direct_mir_scalar_program_comparison_expression_readiness_owner.pgy`
  -- one normalized typed comparison identity/readiness family. Int comparison
  identities and exact Long greater/equality/inequality/less identities remain
  distinct; exact Bool equality/inequality share this same owner while C and LLVM
  consume the same signed comparison projection; targets do not reclassify
  source graph kinds or operand types.
- `src/self_hosted/compiler/direct_mir_scalar_program_case_math_expression_readiness_owner.pgy`,
  `direct_mir_scalar_program_case_math_runtime_requirement_owner.pgy`,
  `direct_mir_scalar_program_case_math_runtime_projection_owner.pgy`,
  `direct_mir_scalar_program_c_case_math_expression_owner.pgy`, and
  `direct_mir_scalar_program_llvm_case_math_expression_owner.pgy` -- one
  target-neutral String/math expression family and its sealed runtime-call ABI
  projection. Dynamic Long division/remainder join exact Long operands to the
  checked arithmetic rows; neither target emits raw `/`, `%`, `sdiv`, or
  `srem`, nor recreates divisor safety policy.
- `src/self_hosted/compiler/runtime_value_call_abi_identity_owner.pgy`,
  `runtime_value_call_abi_owner.pgy`,
  `runtime_value_representation_owner.pgy`,
  `direct_mir_scalar_program_runtime_value_expression_readiness_owner.pgy`,
  and `direct_mir_scalar_program_runtime_value_lifecycle_owner.pgy` -- the
  target-neutral join from one allocation-free Allocator/TextBuilder stable
  identity receipt through the canonical ABI-layout registry to one
  runtime-value representation and its GraphPlan-local last-consumer proof.
  Final MIR validation consumes the identity receipt and never reconstructs
  the serialized runtime-call registry per instruction. This is compiler
  storage lifetime, not
  an entity or world lifecycle model: every admitted owner value is created by
  its registry call, used only while live, and reaches its exact terminal call
  on every exit without reopening source spelling or rescanning other routines.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_runtime_value_expression_owner.pgy`
  and `direct_mir_scalar_program_llvm_runtime_value_expression_owner.pgy` --
  MIR-blind target projection of the same runtime-value call identity. They
  materialize the canonical aggregate storage and symbols but do not infer
  ownership, cleanup, or ABI shape independently.
- `src/self_hosted/compiler/direct_mir_scalar_program_c_expression_owner.pgy`,
  `direct_mir_scalar_program_llvm_expression_owner.pgy`, and
  `direct_mir_scalar_program_llvm_int_math_materialization_owner.pgy` -- typed
  Int/Long addition, subtraction, and multiplication, Int negation, and `Abs`
  lowering consume the language's defined two's-complement wrap semantics.
  LLVM uses plain `add`/`sub`/`mul` without `nsw`; emitted C is compiled under
  the canonical
  `-fwrapv` contract. No CFG-, magnitude-, or String-window-specific overflow
  proof may narrow these source semantics.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_emission_owner.pgy`
  -- MIR-blind range-driven LLVM program rendering from the sealed GraphPlan.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_operation_owner.pgy`
  and `direct_mir_scalar_program_llvm_array_mutation_owner.pgy` -- MIR-blind
  LLVM per-operation dispatch and local/value-result ArrayInt/ArrayString
  storage mutation from the same sealed receiver and expression rows.
- `src/self_hosted/compiler/direct_mir_scalar_program_projection_owner.pgy` --
  selected-target boundary that issues one sealed GraphPlan and never retries a
  returned-Array, Option, terminal-graph, or native backend path. Opt-in
  receipts bracket the one GraphPlan build and the one target emission.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_operation_plan_owner.pgy` --
  operation-row assembly plus latest-dominating ValueId joins.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_loop_flow_admission_owner.pgy`
  -- scalar execution receipt over the existing loop-summary/CFG projection
  owner. The current pure-Int slice accepts while or a sealed range receipt,
  neutral effects, stable flags, and empty resource-state spans without
  reconstructing a loop from a fixture topology.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_range_iteration_owner.pgy`
  -- admitted-MIR construction of canonical range receipts over typed
  iteration and loop-flow owners; multi-range selection consumes the one
  decoded wire and exact `loop_syntax_id` LocalRefs, while the byte-stable
  single-range shape has one possible typed receipt. Its bound is issued by
  `direct_mir_scalar_cfg_range_bound_owner.pgy`, which admits exactly one
  canonical integer literal or one persisted `ArrayLength(ValueId)` graph.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_admission_owner.pgy`
  -- joins a collection loop candidate, typed iteration row, collection
  ValueId, resolved local-literal or direct-call collection source, binding
  identity, element payload receipt, and CFG edges once. `Array<Int>` and
  `Array<String>` use the same loop receipt; a mistyped collection loop fails
  here instead of being retried as an integer range.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_element_owner.pgy`
  -- loop-syntax-keyed element type and String-pool companion receipt. It keeps
  target-specific storage out of the primitive foreach fact set.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_definition_owner.pgy`
  -- element-neutral definition-instruction identity predicate shared by
  foreach and indexed local collection owners.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_string_collection_owner.pgy`
  -- foreach adapter over the target-neutral local `Array<String>` collection
  fact; it does not decode a second literal graph or ABI row.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_array_string_collection_owner.pgy`
  -- exact local `Array<String>` literal graph, canonical ABI, storage
  identity, and element-pool admission shared by foreach and indexed reads.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_array_length_fact_owner.pgy`
  -- one persisted-graph `ArrayLength(collection)` subtree identity shared by
  range and while conditions. It owns no CFG or backend policy.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_array_length_log_graph_owner.pgy`
  -- element-neutral exact `Log(ToString(ArrayLength(collection)))` graph. The
  String-named compatibility view delegates here and owns no second shape.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_collection_owner.pgy`,
  `direct_mir_scalar_cfg_array_int_collection_admission_owner.pgy`,
  `direct_mir_scalar_cfg_array_int_program_fact_owner.pgy`, and
  the static/read-only/reverse/pop fact owners -- one
  public-ABI `Array<Int>` identity and one program receipt with mutually
  exclusive dynamic-push, initialized-static-set, initialized-read-only,
  fresh-reverse, and foreach-backed-pop modes. Initial elements, current
  length, capacity, and selected child receipt are facts; a backend cannot
  reinterpret one mode as another. The pop mode references the existing
  foreach storage identity and owns effects only, so no second `xs` storage is
  materialized.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_graph_shape_owner.pgy`,
  `direct_mir_scalar_cfg_array_int_program_admission_owner.pgy`,
  `direct_mir_scalar_cfg_array_int_static_graph_owner.pgy`, and
  the static/read-only admission owners -- admit
  either the bounded producer/push/consumer topology or the initialized
  current-length while sum followed by one in-bounds set, indexed observation,
  and final length observation, or one read-only range maximum with two indexed
  reads, a greater branch, exact phi joins, and final Log. The read-only source
  and phi/topology owners consume the existing range receipt rather than
  duplicating its block facts. `loop_flow_summary.effect_delta` is not mutation
  authority, and no mode is tried as a compatibility fallback.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_binding_owner.pgy`,
  `direct_mir_scalar_cfg_array_int_operation_binding_owner.pgy`,
  `direct_mir_scalar_cfg_array_int_program_identity_owner.pgy`,
  `direct_mir_scalar_cfg_array_int_static_mutation_identity_owner.pgy`,
  the read-only binding/identity/expression owners, and the three mode
  readiness owners -- bind exact ValueId/LocalRef/operation rows,
  include the single receipt and explicit mode in `GraphPlan` identity, and
  prove loop induction, accumulator recurrence, mutation ordering, derived
  storage state, range bounds, greater-condition projection, predecessor/value
  phi joins, and target operation shape. The C/LLVM storage, initialization,
  program-operation, and read-only emission owners consume that receipt without
  reopening MIR or calling a native/runtime collection path.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_fact_owner.pgy`,
  `direct_mir_scalar_cfg_string_array_plan_lookup_owner.pgy`,
  `direct_mir_scalar_cfg_string_array_plan_append_owner.pgy`, and
  `direct_mir_scalar_cfg_string_array_plan_identity_owner.pgy` -- immutable
  primitive column sets for local `Array<String>` collections, length guards,
  and indexed concat/Log/static-set/push/pop accesses. Row views preserve one
  collection storage identity, an operation-ordered current-length timeline,
  and one global-instruction-to-operation mapping without a custom-struct
  array ABI. Capacity remains the initial-plus-push bound and is never reduced
  by pop.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_pop_program_fact_owner.pgy`,
  `direct_mir_scalar_cfg_collection_pop_program_readiness_owner.pgy`, and
  `direct_mir_scalar_cfg_collection_pop_typed_readiness_owner.pgy` -- one joint
  program receipt joining the foreach-owned `Array<Int>` source, the
  String-plan-owned source, and the globally ordered `4->3`, `3->2`, `3->2`
  pop effects. Partial Int-only or String-only admission is invalid.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_fact_owner.pgy`
  and `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_identity_owner.pgy`
  -- the target-neutral collection SSA-value, ordered `Initialize`/`Get`/`Set`,
  and observation columns carried by `GraphPlan`. Storage identity and
  predecessor rows stay distinct from source names, and one digest seals the
  complete value/operation/observation view.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_array_index_assignment_route_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_cfg_index_assignment_graph_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_index_assignment_source_owner.pgy`,
  and `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_index_assignment_admission_owner.pgy`
  -- coarse claim and exact admission of the currently bounded Int/String
  indexed-assignment graph into the general collection plan. The claimant owns
  rejection routing; only exact typed graph/use/ABI/length admission can
  publish the plan.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_selection_owner.pgy`
  -- the exclusive selection boundary between the embedded general collection
  plan and the older String/ArrayInt plans. Once the indexed-assignment route
  is claimed, invalid admission cannot retry either older plan.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_binding_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_expression_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_value_readiness_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_operation_readiness_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_observation_readiness_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_readiness_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_typed_readiness_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_array_int_absence_owner.pgy`,
  and `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_operation_shape_owner.pgy`
  -- graph-row binding and final plan validation. Every collection version,
  operation, observation, target operation row, ABI layout, and inactive legacy
  Array-operation handoff must be complete and exclusive before sealing.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_c_storage_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_c_operation_owner.pgy`,
  `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_llvm_storage_owner.pgy`,
  and `src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_llvm_operation_owner.pgy`
  -- selected-target storage materialization and ordered operation projection
  from the sealed collection plan only. They cannot read MIR JSON, call the
  private three-field mutation helpers, or precompute final observations.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_foreach_owner.pgy`
  and the ArrayInt pop admission/binding/identity/lookup/readiness owners --
  bind the two Int effects to the pre-existing foreach collection identity.
  C and LLVM mutate the same live length field later consumed by the loop and
  final length observation; the private value-returning three-field helper is
  not a legal projection.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_pop_c_operation_owner.pgy`
  and `direct_mir_scalar_cfg_string_array_pop_llvm_operation_owner.pgy` --
  decrement only the live String-array length in the canonical four-field
  object. Data, capacity, allocator, and the popped source tail remain intact.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_readiness_owner.pgy`
  -- source and bound-row shape, static literal bounds, duplicate collection /
  global / operation claim rejection, and explicit empty-String value presence.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_source_fact_owner.pgy`,
  `direct_mir_scalar_cfg_string_array_statement_source_owner.pgy`, and
  `direct_mir_scalar_cfg_string_array_length_log_graph_owner.pgy` -- exact
  persisted graph/use facts for one collection statement, including literal
  `ArrayPush` and `Log(ToString(ArrayLength(collection)))`. Display text is not
  an authority and missing graph facts fail closed.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_capacity_owner.pgy`,
  `direct_mir_scalar_cfg_string_array_index_readiness_owner.pgy`, and
  `direct_mir_scalar_cfg_string_array_push_graph_readiness_owner.pgy` --
  operation-ordered length version, bounded final capacity, absent-index
  sentinel, and post-plan push/length-log operation binding. A future push
  count cannot be observed as the current length.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_push_dominance_owner.pgy`
  and `direct_mir_scalar_cfg_string_array_entry_execution_owner.pgy` -- admit
  only once-executed entry-block push prefixes after the collection definition
  and before every length/read/set consumer. The entry owner rejects every
  predecessor edge into block zero; loop, branch, late, reordered,
  pre-definition, or re-entered pushes fail before target projection.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_graph_shape_owner.pgy`
  and `direct_mir_scalar_cfg_string_array_plan_admission_owner.pgy` -- exact
  graph/use admission for range or while `ArrayLength`, indexed concat/Log,
  and bounded static `ArraySet`. Display text, fixture names, block counts, and
  capacity never own these decisions.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_dominance_owner.pgy`
  and `direct_mir_scalar_cfg_string_array_index_safety_owner.pgy` -- collection
  definition dominance, unique-predecessor true-edge guard dominance, and the
  zero/start-plus-one nonnegative proof required before C `size_t` or LLVM
  unsigned/inbounds projection.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_binding_owner.pgy`,
  `direct_mir_scalar_cfg_string_array_expression_owner.pgy`, and
  `direct_mir_scalar_cfg_string_array_graph_readiness_owner.pgy` -- bind source
  selector identities to exact ValueId/LocalRef and operation rows, then reject
  stale row bounds or operation-kind drift before either backend publishes.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_collection_admission_owner.pgy`
  -- the one type-directed join over local Int, local String, and admitted
  returned-Int collection definitions. It exposes one collection receipt and
  one companion element receipt to the planner.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_collection_owner.pgy`
  -- exact collection-definition join. Local literals consume their own graph
  and ABI; hoisted calls consume the admitted producer receipt and reject call
  target, ABI, result-name, or hidden LocalRef drift.
- `src/self_hosted/compiler/direct_mir_array_captured_abi_fact_owner.pgy` and
  `direct_mir_array_literal_spine_owner.pgy` -- element-neutral captured-array
  ABI and persisted array-literal spine predicates shared by the Int and String
  collection owners.
- `src/self_hosted/compiler/direct_mir_array_string_abi_fact_owner.pgy` and
  `direct_mir_array_string_abi_projection_owner.pgy` -- canonical
  `Array<String>` ABI admission and selected-target C/LLVM spelling. They
  consume the persisted layout row; neither backend guesses offsets or runtime
  symbols. The direct public C projection is the four-field
  `PgyArray_String`; `CompilerAbiLayoutArrayStringCValueType()` and its
  three-field `pgy_as` spelling remain private to self-codegen/runtime carriage
  and cannot own this direct projection.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_fact_owner.pgy`,
  `direct_mir_scalar_cfg_foreach_set_owner.pgy`, and
  `direct_mir_scalar_cfg_foreach_append_owner.pgy` -- immutable target-neutral
  collection-iteration receipt, canonical primitive set storage, identity
  digest, and exact lookup. A storage identity deduplicates repeated calls to
  the same pure producer while each loop retains its own cursor. The receipt
  owns data/length roles and elements; neither backend reconstructs the hidden
  cursor protocol.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_local_owner.pgy` --
  binds each foreach source binder to one scalar local slot while its
  `Array<Int>` collection remains owned by the collection receipt.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_graph_readiness_owner.pgy`
  -- verifies receipt-to-CFG topology, condition identity, latch ownership, and
  non-overlapping collection/cursor/binder identities after plan issue.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_range_fact_owner.pgy` --
  target-neutral per-range receipt schema, digest, and bound/unbound readiness.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_range_set_owner.pgy` -- one
  canonical receipt-set owner for primitive storage, ordering, digest, exact
  lookup, and set readiness; array position is not semantic identity.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_range_graph_readiness_owner.pgy`
  -- receipt-by-receipt CFG and unique latch-effect validation after plan issue.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_range_transfer_admission_owner.pgy`
  -- pre-plan proof that every range backedge belongs to the innermost active
  receipt; an outer dominating header cannot steal an inner continue or normal
  fallthrough edge.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_range_emission_owner.pgy`
  -- target spelling for the plan-sealed range initialization and latch effect;
  it cannot reopen MIR, source, or topology.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_range_block_effect_owner.pgy`
  -- canonical receipt-set aggregation of per-block C/LLVM range effects.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_c_operand_owner.pgy` -- C
  spelling for sealed ValueId/LocalRef/literal operands.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_c_emission_owner.pgy` --
  final C label/goto projection from the immutable scalar CFG plan. Operation
  spelling is delegated to the type-directed operation owner; MIR is never
  reread.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_c_operation_emission_owner.pgy`
  and `direct_mir_scalar_cfg_string_c_materialization_owner.pgy` -- typed C
  operation spelling and the bounded concat runtime block selected solely by
  the sealed plan and runtime ABI receipt.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_c_collection_operation_emission_owner.pgy`
  and `direct_mir_scalar_cfg_array_int_c_emission_owner.pgy` -- collection
  operation dispatch and public four-field `PgyArray_Int` storage. A proven
  bound is capacity while current length starts at zero; each dynamic value is
  stored at `data[length]` before length advances. No runtime push or realloc
  path is available in this bounded lane.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_c_emission_owner.pgy`
  -- stable C consumer names delegated to
  `direct_mir_scalar_cfg_foreach_typed_c_emission_owner.pgy`, which owns typed
  Int/String storage, ABI-length cursor, binder load, latch increment, and
  condition projection from the sealed receipts.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_array_c_materialization_owner.pgy`,
  `direct_mir_scalar_cfg_string_array_c_storage_emission_owner.pgy`,
  `direct_mir_scalar_cfg_string_array_c_mutation_emission_owner.pgy`, and
  `direct_mir_scalar_cfg_string_array_c_emission_owner.pgy` -- canonical public
  four-field C String-array storage, initial length/capacity materialization,
  operation-time push store then length update, final length observation,
  indexed reads, and static in-bounds set. A signed dynamic index is converted
  to `size_t` only after the shared nonnegative plan proof.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_emission_owner.pgy` --
  final textual LLVM block projection from that same plan. Operation spelling
  is delegated to the type-directed operation owner; it owns no admission.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_operation_emission_owner.pgy`
  and `direct_mir_scalar_cfg_string_llvm_materialization_owner.pgy` -- typed
  LLVM operation spelling, String constants/arrays, and the selected concat
  helper body from sealed receipts.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_collection_operation_emission_owner.pgy`,
  `direct_mir_scalar_cfg_array_int_llvm_storage_owner.pgy`, and
  `direct_mir_scalar_cfg_array_int_llvm_emission_owner.pgy` -- the LLVM view of
  the same bounded Int receipt: one mutable four-field object, explicit
  i64-to-i32 push truncation, i32-to-i64 read extension, current-length guard,
  and store-before-length-update order. Backends consume no MIR JSON or graph.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_array_string_llvm_value_owner.pgy`
  remains the immutable foreach value materializer. The direct indexed lane is
  instead owned by `direct_mir_scalar_cfg_string_array_llvm_storage_emission_owner.pgy`,
  `direct_mir_scalar_cfg_string_array_llvm_mutation_emission_owner.pgy`, and
  `direct_mir_scalar_cfg_string_array_llvm_emission_owner.pgy`: one mutable
  four-field object owns data, current length, capacity, push stores, indexed
  reads, and final length observation. No stale immutable aggregate snapshot
  may own a post-push length. Unsigned `icmp ult` and inbounds GEP consume the
  same nonnegative and owned-length receipt as C.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_llvm_emission_owner.pgy`
  -- stable LLVM consumer names delegated to
  `direct_mir_scalar_cfg_foreach_typed_llvm_emission_owner.pgy`, which owns
  typed aggregate storage, ABI-indexed data/length access, binder load, cursor
  update, and condition projection from the same receipts.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_local_emission_owner.pgy` and
  `direct_mir_scalar_cfg_llvm_terminator_emission_owner.pgy` -- responsibility-
  named scalar local and LLVM terminator spelling owners extracted to preserve
  the existing emitter hard caps; they own no semantic admission policy.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_operand_owner.pgy` --
  LLVM spelling for sealed ValueId/LocalRef operands.
- `src/self_hosted/compiler/direct_mir_scalar_cfg_projection_owner.pgy` --
  selected-target boundary joining the one scalar CFG plan with the owned
  formatted-print ABI and C/LLVM text consumer.
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
  JSON inputs converge on one `AstTreeArtifact` verifier. For file-backed MIR,
  it snapshots the topology receipt and machine declaration, completes the
  typed codegen view, then retires the raw JSON input before C emission.
- `src/self_hosted/compiler/canonical_mir_execution_owner.pgy` -- canonical
  MIR execution owner. It reads one admitted MIR input, performs body
  verification once, consumes the resulting `DriverRung2VerifiedFacts`
  receipt for MIR projection, and rebinds the canonical identity epoch. It
  captures nominal-constructor identity facts before projection retires the
  artifact storage; rebind consumes that typed capture and may not reread the
  retired artifact or submit published call-return rows to a second body
  fixpoint.
- `src/self_hosted/compiler/driver_rung2_intent_consumer_owner.pgy` -- one
  admitted typed-intent plan tree projection and exact post-semantic expression
  occurrence remap boundary for the DRV-2 consumer.
- `src/self_hosted/compiler/canonical_mir_identity_epoch_owner.pgy` --
  canonical MIR tree/directive identity adapter and program-level atomic
  composition boundary; rebinds nominal owners and topology directives into
  one reconstructed `AstTreeArtifact` epoch, then consumes the pre-projection
  nominal-constructor capture and field identity epoch owner before publishing
  program facts.
- `src/self_hosted/compiler/canonical_mir_source_module_epoch_owner.pgy` --
  canonical MIR source-module provenance reconstruction; binds wire-owned
  paths to reconstructed top-level declarations and reseals the AST digest.
- `src/self_hosted/compiler/canonical_mir_field_identity_epoch_owner.pgy` --
  declaration-field identity epoch owner; rebinds declaration fields and
  topology field references by exact `(owner, name, field_kind)` joins.
  Numeric equality, offsets, declaration order, and name-only fallback are
  forbidden.
- `src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy` -- sole pure
  argv admission owner for the versioned DRV-2 CLI request family. It maps
  exact source-token, source-AST, internal verified source-DIR, installed
  machine-manifest, source-C,
  source-MIR, canonical, MIR-C, and direct-backend forms to a flat typed enum
  before I/O. Artifact requests
  require an explicit artifact mode or `-o`; optional positional-third guessing
  and implicit default source are forbidden.
- `src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy` --
  read-only executor for admitted token stdout, import-composed AST stdout,
  compiler stdout, machine-manifest, canonicalization, and missing-projection
  probe requests. Source-C stdout delegates to its checked typed last consumer;
  artifact variants fail before reads.
- `src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy` -- installed
  artifact-effect executor. It delegates read variants to the read owner and
  publishes only typed artifact variants through compiler-world actions. It
  owns no direct compiler call or atomic transaction transition.
- `src/compiler/driver_self_host_llvm_selection_owner.c` -- native public LLVM
  request admission adapter. It distinguishes plain binary and exact file-form
  IR requests but owns no source, MIR, or backend fact.
- `src/compiler/self_host_llvm_ir_artifact_owner.c` -- exact public LLVM IR file
  publication boundary. It materializes the installed source-MIR/projector pair
  in a private same-directory workspace and publishes opaque bytes only after
  success. Native semantics, libLLVM retry, and LLVM text inspection are
  forbidden.
- `src/compiler/self_host_llvm_ir_stdout_owner.c` -- exact public LLVM IR
  stdout delivery boundary. It completes the same installed artifact pair
  before output and streams opaque bytes with one fixed buffer. Whole-artifact
  strings, LLVM policy inference, and native fallback are forbidden.
- `src/self_hosted/compiler/driver_rung2_cli_owner.pgy` -- 11-line standalone
  wrapper that performs one argv admission and one read-only execution.
- `src/self_hosted/compiler/driver_rung2_fixture_manifest_cli_owner.pgy` and
  `driver_rung2_fixture_manifest_main.pgy` -- test-only fixture inventory CLI;
  the installed composition root cannot import this surface.
- `src/self_hosted/compiler/driver_rung2_main.pgy` -- DRV-2 runnable hard
  semantic test entrypoint and fixture-manifest boundary; production CLI
  request ownership remains in `driver_rung2_cli_request_owner.pgy` and
  compiler-stage ownership remains in `driver_rung2_owner.pgy`.
- `src/self_hosted/compiler/authority_owner.pgy` -- authority contracts
  (abilities + roles) for the sensitive compiler-world boundaries: semantic
  verdict, C emission, subprocess planning, and parity judgement.

## LSP

- `src/self_hosted/lsp/main.pgy` -- LSP-0 runnable artifact boundary.
- `src/self_hosted/lsp/completion_owner.pgy` -- registry-directed LSP completion
  projection over all 146 language-word identities; exposure remains owned by
  the language keyword registry flags.
- `src/self_hosted/lsp/diagnostics_owner.pgy` -- semantic diagnostic block to
  `publishDiagnostics` JSON payload projection.
- `src/self_hosted/lsp/document_store_owner.pgy` -- LSP-2f buffered
  didOpen/didChange multi-document state projection.
- `src/self_hosted/lsp/document_revision_owner.pgy` -- Insere-derived URI,
  monotonic version, exact text, and HostTask generation receipt used by the
  buffered store and its latest-only diagnostics publication admission.
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
