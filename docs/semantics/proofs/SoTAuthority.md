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

- fact: `FInitializerArrayBody`;
- fact: `FInitializerTryOperand`;
- fact: `FCollectionMutationParts`;
- fact: `FEnumDeclarationRows`;
- fact: `FNominalDeclarationRows`;
- fact: `FEnumDeclarationRows`;
- authorities: `OSemanticLocalBindingFacts`, `OSemanticStatementFacts`;
- authority: `OSemanticEnumFacts`;
- consumers: `CArrayLiteralEmitter`, `CTryLetEmitter`,
  `CCollectionMutationEmitter`, `CEnumEmitter`;
- read kind: `OwnedRead`.

It also proves three rejection cases:

- an owner read plus a codegen text fallback is not closed;
- two semantic producers are not closed; and
- a required fact with no producer is not closed.

The file also declares 15 architectural compiler-spine fact families plus seven
bounded self-host closure facts and a total `spine_authority` mapping.
`every_spine_fact_has_declared_authority` and
`declared_spine_authority_unique` prove that this architectural mapping is
total and functional. `declared_owner_does_not_imply_rung_closed` keeps the
critical distinction explicit: assigning an owner does not prove that live
fallback consumers are gone.

## Implementation Binding

`tests/sot_authority_adequacy_smoke.sh` binds the model to the current source:

- `src/self_hosted/semantic/ast_local_binding_fact_owner.pgy` produces and
  exposes the array-literal body row;
- `src/self_hosted/codegen/input/semantic_array_literal_codegen_view_owner.pgy`
  consumes that accessor;
- `src/self_hosted/codegen/input/semantic_try_let_codegen_view_owner.pgy`
  consumes the semantic try-operand accessor;
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
  runtime usage projection;
- `src/self_hosted/semantic/ast_type_surface_fact_owner.pgy` owns canonical
  type-name rows consumed by runtime usage projection;
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
