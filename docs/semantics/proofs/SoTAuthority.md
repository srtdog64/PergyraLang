# SoT Authority Model

Status: mechanized bounded-rung model  
Date: 2026-07-12

`SoTAuthority.v` formalizes the negative property that was missing from the
compiler migration discipline. A named owner is not enough. A closed hard
substitution rung requires:

1. every required semantic fact has an authority producer;
2. every producer of that fact is the authority owner;
3. every required consumer reads the authority owner; and
4. no semantic read is a fallback read.

The model proves the generic consequences of `RungClosed` and instantiates the
first live bounded bindings for semantic-to-codegen facts:

- fact: `FInitializerExpressionGraph`;
- fact: `FInitializerTryOperand`;
- fact: `FCollectionMutationParts`;
- fact: `FEnumDeclarationRows`;
- fact: `FNominalDeclarationRows`;
- fact: `FRoleDeclarationRows`;
- facts: `FExpressionRuntimeUsageSurface`, `FTypeRuntimeUsageSurface`, and
  `FNodeKindSurface`;
- fact: `FEntrypointSelection`;
- fact: `FFunctionDeclarationRows`;
- facts: `FLocalBindingStatementRouting`, `FAssignmentStatementRouting`, and
  `FStatementKindRouting`;
- authorities: `OParserExpressionGraph`, `OSemanticStatementFacts`;
- authority: `OSemanticEnumFacts`;
- authorities: `OSemanticNominalConstructorFacts`, `OSemanticRoleFacts`,
  `OSemanticExpressionSurfaceFacts`, `OSemanticTypeSurfaceFacts`, and
  `OSemanticKindSurfaceFacts`;
- authority: `OSemanticSignatureFacts`;
- authorities: `OSemanticLocalBindingFacts`, `OSemanticAssignmentFacts`, and
  `OSemanticStatementFacts` for their distinct statement-routing rows;
- consumers: `CArrayLiteralEmitter`, `CTryLetEmitter`,
  `CCollectionMutationEmitter`, `CEnumEmitter`, and
  `CDeclarationRoutingEmitter`;
- read kind: `OwnedRead`.

It also proves three rejection cases:

- an owner read plus a codegen text fallback is not closed;
- two semantic producers are not closed; and
- a required fact with no producer is not closed.

The file also declares the architectural compiler-spine fact families, including
the MIR-owned ABI layout rows and their runtime-call ABI row projection, plus
the bounded self-host closure facts and a total `spine_authority` mapping.
`every_spine_fact_has_declared_authority` and
`declared_spine_authority_unique` prove that this architectural mapping is
total and functional. `declared_owner_does_not_imply_rung_closed` keeps the
critical distinction explicit: assigning an owner does not prove that live
fallback consumers are gone.

## Implementation Binding

`tests/sot_authority_adequacy_smoke.sh` binds the model to the current source:

- `src/self_hosted/parser/expression_graph_owner.pgy` produces the array
  literal root and ordered element edges;
- `src/self_hosted/codegen/input/semantic_expression_codegen_view_owner.pgy`
  carries that graph and `src/self_hosted/codegen/emission/stmt_emit.pgy`
  consumes every element through its graph handle. Local-binding array-body
  strings, the retired dedicated view, and sequence splitting in the hard
  consumer are forbidden;
- `src/self_hosted/parser/expr_postfix_owner.pgy` produces postfix try as an
  `AstExpressionNodeTry` plus its operand edge;
- `src/self_hosted/codegen/input/semantic_expression_codegen_view_owner.pgy`
  and `src/self_hosted/codegen/emission/try_let_emit_owner.pgy` consume that
  graph fail-closed. A local-binding try string or codegen text recovery is not
  an alternate authority;
- `src/self_hosted/semantic/ast_statement_fact_owner.pgy` owns collection
  mutation payload rows consumed by the semantic statement codegen view;
- `src/self_hosted/semantic/ast_enum_fact_owner.pgy` owns enum declaration rows
  consumed by the semantic enum codegen view and `CollectEnums`; its nested
  range contract preserves multi-parameter variant arity;
- `src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy` owns nominal
  names and ordered field rows consumed by `CollectStructs`;
- `src/self_hosted/semantic/ast_role_fact_owner.pgy` owns role names, target
  types, and method `NodeId` rows consumed by operator binding and receiver ABI;
- `src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy` owns
  expression atom/value/auxiliary rows and call/token queries consumed by
  runtime usage projection; its condition subset also owns stable normalized
  expression handles and child edges consumed by `if`/`while` codegen, while
  compact parser payload to graph production remains explicitly `BRIDGE`;
- `src/self_hosted/semantic/ast_type_surface_fact_owner.pgy` owns canonical
  type-name rows consumed by runtime usage projection;
- `src/self_hosted/semantic/ast_kind_surface_fact_owner.pgy` owns canonical
  node-kind rows consumed by runtime usage projection; direct arena kind scans
  and backend-local kind tags are forbidden;
- `src/self_hosted/semantic/ast_signature_fact_owner.pgy` owns ordered function
  node/name rows used for entrypoint cardinality and codegen selection; semantic
  verdict and codegen may not rescan arena function names;
- function signature, nominal, role, and enum facts also own top-level
  declaration identity consumed by `program_emit.pgy`; the four codegen arena
  declaration predicates are forbidden;
- `FNodeKindSurface` owns ability/event declaration classification as well as
  runtime kind usage; consumer-specific kind aliases are forbidden;
- local-binding, assignment, and statement semantic owners separately own
  statement dispatch identity; emitters may retain syntax structure traversal
  but cannot recover statement kinds from the arena;
- the retired AST-text array, try-let, collection, enum, and mixed declaration
  codegen owners must
  not exist; and
- the codegen views must not contain string-shape recovery or direct AST value
  reads.

The smoke mutates temporary owner and consumer copies to prove that missing
owner accessors and reintroduced fallback reads are rejected by the binding
gate.

## Claim Limit

The Coq file proves the authority rule for its model. It does not inspect C or
Pergyra source and does not prove whole-compiler SoT closure. The adequacy gate
is source-consistency evidence for the named live slice. Each later expression,
MIR, ABI, or AIR consumer needs another explicit model binding before it may
claim the same closure.

The machine-readable implementation/status binding is
`docs/semantics/sot_owner_spine_registry.md`. It is the canonical registry;
self-hosted owner projections must eventually consume or be generated from it,
not copy its rows into another handwritten authority list.
