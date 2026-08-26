# Semantic-Hop Parallel Architecture Audit Directive

Status: `AUDIT COMPLETE`; this document is coordination, not semantic authority.

Base revision: `9ca4a69517142a4c87eb47862afcd55a9a9f2011`.

The attached 2026-08-26 architecture review is a proposal to test, not a
license to start a `FactStore`, query engine, cache, folder migration, or new
plan hierarchy. The existing World/Zone, SoT, fail-closed admission, sealed
plan, and backend-projection contracts remain authoritative.

## Shared objective card

- Objective: measure whether physical owner proliferation is causing repeated
  semantic decisions, excessive navigation hops, or repeated compiler-scale
  work, then name one evidence-backed consolidation candidate reached by a
  production executable path.
- Priority: semantic identity and one SoT; owner-directed facts; repeated
  decision removal; backend-neutral plan reuse; measurable hop/work reduction;
  navigation; then file count and folder aesthetics.
- Fact owners: the current SoT spine rows, typed MIR/admission indexes, sealed
  plans, protocol/ABI registries, and their executable gates. Audit reports do
  not own compiler facts.
- Last legitimate consumer: the production entrypoint and backend projection
  named by each measured route.
- Forbidden fallback: source/AST/JSON rescan, string-name identity, `new ? old`
  dual reads, family/fixture dispatch, speculative cache, a folder treated as
  authority, or a line-cap-only split/merge.
- Integration gate: every proposed consolidation must name one current
  production bypass or duplicated decision, its existing owner, all consumers,
  one old-path negative, and one executable falsifier. No implementation track
  opens from file count or naming alone.

## Global lease and safety boundary

- Lease F (`pgy --mir` installed diagnostic substitution) remains owned by the
  primary task until its code checkpoint is committed. Do not edit any Lease F
  source, Make, test, registry, handoff, progress, dogfood, collaboration, or
  completion-log file.
- Each auditor may edit only its assigned report below. Do not stage, commit,
  push, build DRV-2, run a full component scan, or inspect the user-owned
  `pgy-80135c2c/` directory.
- Use read-only `rg`, file reads, import/invocation census, and existing logs.
  Stop a static command at 60 seconds. Do not use generated file count, owner
  count, or token volume as progress evidence.
- Findings must distinguish observed fact, inference, and proposal. A proposed
  spine is not `READY`, `CLOSED`, or substitution progress.

## Track A — semantic-hop census

Assigned report:
`docs/audits/2026-08-26_semantic_hop_census.md`.

Measure two exact routes:

1. public installed `pgy --mir SOURCE` from launcher selection through the
   admitted MIR diagnostic stdout consumer;
2. the exact nested priority/observability source/direct C and LLVM family from
   route claim through its sealed plan to target emission.

For each route record the ordered hop list, owner responsibility, input/output
identity, whether the hop makes a semantic decision or only transports a typed
fact, and the executable gate that can falsify it. Count semantic-decision hops
separately from transport/navigation hops. Flag only repeated decisions or
whole-program operations; do not recommend merging files merely to lower the
count.

Required final section: one smallest candidate that removes at least one
duplicated decision or repeated owned operation without changing owner
identity, plus the old-path negative and executable falsifier. `No candidate`
is valid when the measured routes are already one-plan paths.

## Track B — direct-MIR semantic-dimension map

Assigned report:
`docs/audits/2026-08-26_direct_mir_dimension_map.md`.

Inventory the live `direct_mir_*` admission, route, plan, C emission, and LLVM
emission owners. Group responsibility by semantic dimension rather than file
prefix or fixture: callable identity, call edge, type representation,
carriage/ownership, intent policy, authority, cleanup, ABI, and target-only
projection.

Record which facts are already shared, which decisions are repeated across C
and LLVM, and which family-specific plans merely bound unsupported production
frontiers. Choose at most one consolidation candidate, and only if the same
semantic decision is demonstrably reconstructed in two live consumers. Do not
design a universal `ProgramPlan`, create a new protocol species, or infer a
query API from similar filenames.

Required final section: candidate owner identity, consumers to migrate,
forbidden old read, negative ratchet, executable fixture, and why the work is
or is not the next production rung.

## Track C — compiler navigation projection

Assigned report:
`docs/audits/2026-08-26_compiler_navigation_projection.md`.

Measure the flat `src/self_hosted/compiler/` namespace as a human-navigation
problem. Produce a responsibility grouping and importer/cycle-risk summary for
`world`, `facts`, `plans`, `targets`, `driver`, and compatibility-like files.
Folders are navigation projections only; existing typed owners and registry
identities must stay stable.

Propose no more than three bounded move clusters. Each cluster must list exact
files, importer count, path-update blast radius, generated/registry/doc
references, cycle risk, and an executable or structural gate. Reject any move
that combines semantic migration with path churn or that would obscure the
active executable rung. No files are moved in this audit.

Required final section: whether a navigation-only move is currently lower risk
than the next executable substitution. `Defer` is an acceptable conclusion.

## Primary integration decision

The primary task will compare the three reports after Lease F is checkpointed.
Only one follow-up objective card may be opened. It must be tied to the first
observed production bypass or repeated owned operation; general FactStore,
query/cache architecture, and mass folder movement remain out of scope unless
that exact falsifier requires them.

## Completion receipt

- Track A found no repeated semantic decision in the installed MIR diagnostic
  or nested one-plan route. It recorded one smaller source-C wrapper precheck
  candidate, but not as the active rung.
- Track B found that most direct-MIR families already share sealed plans. Its
  sole measured consolidation candidate is parameter target-indirection
  recomputation across scalar GraphPlan C/LLVM signature consumers.
- Track C measured an acyclic but very large import graph and concluded
  `DEFER`: even the smallest navigation-only compatibility move touches 84
  path references and removes no executable bypass.
- Primary decision: no architecture implementation opens while Lease F awaits
  remote Linux/full-self-host validation. The next rung must still begin from a
  freshly observed production bypass and objective card.
