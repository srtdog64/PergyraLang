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
first live binding for the array-literal initializer body:

- fact: `FInitializerArrayBody`;
- authority: `OSemanticLocalBindingFacts`;
- consumer: `CArrayLiteralEmitter`;
- read kind: `OwnedRead`.

It also proves three rejection cases:

- an owner read plus a codegen text fallback is not closed;
- two semantic producers are not closed; and
- a required fact with no producer is not closed.

The file also declares the 16 top-level compiler-spine fact families and a
total `spine_authority` mapping. `every_spine_fact_has_declared_authority` and
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
- the retired AST-text array-literal codegen owner must not exist; and
- the codegen view must not contain `StringTrim`, `CharAt`, or direct AST value
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
