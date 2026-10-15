# Ownership refusal explanation proof

Status: IMPLEMENTATION COMPLETE (bounded formal model and checker;
production bridge OPEN).
Base: `23d1e65fd1709104a0a8c4f47e315a964b356632`, with the existing DRV-2
collection/parser changes already dirty. The user's explicit request to write
and prove Rocq authorizes this proof scope, including the new core file.

## Objective card and chain

- Objective: checked, attributable refusal explanations for a finite admitted
  sharing graph and a small descriptor execution model.
- Priority: true statements and explicit premises; one fact owner; executable
  witness checking; negative evidence; extraction; production integration later.
- Fact owners: `27_ownership_clean.md` owns the language direction;
  `ast_collection_exclusive_value_owner.pgy` owns production EV classification;
  this proof owns only its explicitly defined mathematical model.
- Chain: admitted syntax/call/place facts -> sharing seeds and guarded transfer
  edges -> least sharing fixed point -> boundary refusal -> derivation and
  concrete trace or named missing obligation -> independent checker.
- Last consumer: the extracted reference checker. Actual EV provenance export,
  branch feasibility, graph completeness and DRV-2 integration remain OPEN.
- Forbidden fallback: treating a may-share fact as a feasible stale execution;
  calling a missing-proof witness evidence of unsafe code; inferring production
  coverage from model fixtures; assuming identical iteration schedules.
- Gate: `tests/ownership_refusal_explanation_smoke.sh`, pinned Linux Rocq 9.3.0
  and Stdlib 9.2.0, zero added assumptions, extracted executable controls.
- Falsifiers: forged rule or summary reference, unrelated boundary, wrong parent,
  unavailable effect, no relocation, read before retirement, seedless cycle,
  changed referenced summary and unchanged irrelevant summary.

## Fixed closure plan

1. Reuse the existing allocation identity vocabulary; model shallow descriptor
   copies and actual relocation separately from may-relocate summaries.
2. Define finite, guarded seed/transfer rules and inductive sharing. Prove
   synchronous rounds match bounded derivations, stabilization, and minimal
   derivation height at first discovery. Production updates in place and also
   grows copy/dependency facts: that schedule/refinement is a separate obligation.
3. Define a finite boundary decision and witnesses. Unsafe witnesses require
   checked concrete execution; conservative sharing without such execution is
   named inability to prove safety. Bind every witness to its requested boundary.
4. Prove checker soundness/completeness, refusal witness existence, local support
   sufficiency, verdict-change attribution, and monotonicity under an explicit
   order on adverse summary facts with the graph and boundaries held fixed.
5. Extract the checker, consume theorem propositions independently, execute
   positive/negative controls, register the module, run focused then corpus gates.

Seed categories track constructor overlap, unconsumed alias, ref/index origin,
callee shared result and explicit unavailable analysis facts. Transfer categories
track assignment, argument-to-result and field access. These are an abstract
interface to the reached production owner, not a proved 1:1 source translation.
Let/assignment/call/if/loop/return lowering, consumed-copy invalidation and field
path identity must be validated at that future interface; async is outside it.

## Edit scopes, commands and integration

Single integration owner: this chat. Edit only the new proof, independent audit
and extraction consumer, OCaml observer, focused gate, proof inventory and scoped
documentation/handoff links. Do not edit the active parser/EV/runtime candidate.
No parallel implementation track, commit, push or installed-driver replacement.

Use `OPAMROOT=/home/c/.local/share/pergyra-rocq/opam` with
`scripts/run_rocq_toolchain.sh` in `Ubuntu-E-WSL`. Static checks: 60 seconds;
focused gate: 300 seconds; corpus integration: 1800 seconds. The existing
`tests/coq_kernel_check.sh` owns kernel and assumption-budget checks. Preserve
the approved `tests/coq/assumption_budget` contract; do not enlarge its budget.

Outputs are a proved model and executable reference checker, plus observed
receipts. They are not C-path substitution, production explanation coverage,
package API stability, a source-language safety theorem, or performance evidence.

## Completion receipt

The requested bounded model and extracted reference checker are implemented.
Pinned Linux focused gate: 4 modules, zero assumptions, 41 executable controls.
Three planted regressions were rejected. Full formal integration: 75 proof
owners, 86 kernel-checked modules plus the approval export/binding consumer;
the existing two approved abstractions are unchanged. See
[`the observed receipt`](../audits/ownership_refusal_explanation_2026-10-12.md).
The next production falsifier and all remaining source/export obligations are
recorded there; no production owner or active DRV-2 rung was changed by this work.
