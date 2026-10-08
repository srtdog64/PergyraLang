# IR dependency levels and witness-interface coverage

Status: fixed-graph formal result and source-consistency audit; **not whole
architecture minimality**. Corrected after the 2026-10-08 red-team finding C4/C7.
The source owner is the production driver and its admitted lowering request,
not this diagram or the formal model.

## Source-bound dependency graph

`src/compiler/driver_app.c` constructs DIR with
`dir_lower_with_hir_facts(ast, hir, ...)` and gives DIR to MIR through
`mir_lower_request_bind_dir(&mir_request, dir)`. Those reads are dependencies,
even though the backend itself does not directly query DIR or AIR.

```text
        HIR
        / \
       DIR RIR
        \ /
        MIR -> C / LLVM

HIR / DIR / RIR -> AIR verification
```

The full modeled edges are RIR <- HIR, DIR <- HIR and MIR <- HIR/RIR/DIR.
HIR, DIR and RIR may all initially consume AST facts; that does not remove
their subsequent admitted reads. AIR is not a directly emitted backend IR.
**DIR -> MIR is on the production lowering chain.**

## Exactly three dependency levels for this fixed graph

[`IRMinimality.v`](IRMinimality.v) defines a valid layering by strict ordering
for each declared read. `L3` places HIR at 0, DIR and RIR at 1, and MIR at 2.

- `codegen_chain` and `domain_chain` establish independent HIR -> RIR -> MIR
  and HIR -> DIR -> MIR chains.
- `three_layers_suffice` admits all five edges, and
  `dir_colocates_with_rir` puts DIR beside RIR, not beside HIR.
- `codegen_minimum_is_three` gives three dependency levels for this fixed
  fact graph. It does not prove that three distinct IR representations are
  the smallest possible compiler, nor that another fact factoring is worse.
- `deferred_still_needs_three` retains HIR -> DIR -> MIR when only the
  RIR <- HIR edge is deferred. `two_layers_refused_when_only_rir_deferred`
  rejects the old two-level proposal. There is no single RIR pivot.

## RIR design decision retained, justification corrected

The 2026-06-20 decision to keep a separate flow-sensitive RIR is unchanged.
Its invariant is: **flow-sensitive resource checking happens at the resource (RIR) layer**,
before MIR fusion. `rir_enrich_scope_with_hir_flow` consumes
the HIR CFG and `rir_validation.c` merges resource states before `mir_lower`.
That is a responsibility/localization and extensibility choice, not a proof
that every other compiler structure is impossible. Moving this checking to
MIR would relocate responsibility; it would not by itself remove the DIR
dependency or establish a two-level pipeline.

## AIR / HKT scope

The AIR witness portion is **interface coverage, not architectural minimality**.
`AdequateEvidence S` means every listed requirement holds; `AIRWitness` has
a constructor for each listed requirement. Therefore
`air_is_minimal_witness_set` is an extensional vocabulary contract.
`functor_hkt_not_adequate` refutes only the deliberately restricted
`FunctorHKTWitness` interface in this file. It is not a theorem about every
HKT/Functor encoding or the inability of a library to carry domain evidence.
The decision not to add HKT remains a product/DX decision, not a consequence
of this definitional lemma.

Live overfit test: when a backend or domain primitive grows, inspect the
responsibility and actual reads introduced. A changed graph requires a new
source-bound theorem/gate result; it cannot reuse this count by convention.

## Verification

`tests/ir_minimality_adequacy_smoke.sh` binds the declared edges to current
driver/lowering source and checks the narrow Rocq module when configured.
`tests/coq/ProofRedteamMainRegression.v` consumes the real five-edge graph
and the negative two-level result. The whole formal corpus is compiled from
a fresh snapshot and rechecked by `rocqchk` through the existing kernel owner.
Neither a source grep nor a proof compile establishes production execution
of every represented path.
