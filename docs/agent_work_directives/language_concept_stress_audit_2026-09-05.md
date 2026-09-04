# Language concept stress audit — 2026-09-05

**Status:** AUDIT COMPLETE
**Base revision:** `01f280a3566fb581d0f65d7783e678eee6c987d9`
**Authority:** temporary coordination only; all findings are observations or
proposals, never language-semantics or progress authority.

## Shared objective card

- **Objective:** attack Pergyra's concepts with deletion, semantic-compression,
  and local-reasoning tests across realistic combinations, then identify which
  concepts carry an irreducible static guarantee and which remain decorative,
  redundant, or too costly.
- **Priority order:** falsifying deletion case; distinct compiler guarantee;
  three unrelated workloads; conceptual cost; prose elegance.
- **Fact owner:** current source, semantic owner documents, and executable gates.
  Audit documents own no compiler fact.
- **Last legitimate consumer:** a future, explicitly opened language-design
  decision; this audit cannot open an implementation rung by itself.
- **Forbidden fallback:** keyword counts, preference-only verdicts, invented
  guarantees, one showcase example, or treating similar spelling in another
  language as semantic equivalence.
- **Verification gate:** every strong verdict names repository anchors, gives at
  least one deletion counterexample, exercises at least three materially
  different workloads or explains why that bar is unmet, and survives
  `git diff --check`.

## Independent scopes

- **Deletion matrix:** `Slot`, `Zone`, `Capability`, `Scope`, `Intent`, and only
  the adjacent concepts required to test substitutions. Write only
  `docs/audits/2026-09-05_concept_deletion_stress_matrix.md`.
- **Compression/local-reasoning matrix:** combine the same concepts in concrete
  programs and compare their full obligations with C#, Rust, C++, Go, Kotlin,
  or another justified baseline. Write only
  `docs/audits/2026-09-05_semantic_compression_local_reasoning_matrix.md`.
- Do not edit source, tests, registries, progress documents, handoff state, or
  the active enum/MIR implementation files. Do not edit the other agent's file.

## Commands and budget

- Read-only `rg`, `git show`, `git diff`, and focused source/document inspection
  are allowed.
- Bounded compiler probes may use `.tmp/` only and must be reported as observed
  runs, not semantic truth.
- Spend at most 30 minutes per audit. Do not run the full CI matrix.

## Integration

- **Integration owner:** primary task `/root`.
- **Integration gate:** primary owner cross-checks both reports against current
  repository anchors and runs `git diff --check` before handoff.
- Reports remain read-only design evidence. Any implementation candidate needs
  a later objective card and executable falsifier.

## Integration result

- The deletion report is
  `docs/audits/2026-09-05_concept_deletion_stress_matrix.md`; the independent
  compression/local-reasoning report is
  `docs/audits/2026-09-05_semantic_compression_local_reasoning_matrix.md`.
- Primary integration resolved every cited local path: 43 distinct anchors in
  the deletion report and 21 in the compression report exist at this working
  revision. `git diff --check` is clean for both reports and this directive.
- The reports agree on the separating result: `Slot`, `Zone`, and `Capability`
  retain irreducible facts; containment remains a fact without justifying a
  generic `scope` keyword; `Intent` remains conditional outside its bounded
  typed-transition/authority/trace subset.
- The deletion audit's Intent executable probe stopped before its assertions
  at the separately owned `nominal_constructor_argument_type` failure. It is
  recorded as inconclusive, not silently promoted to a pass or counterexample.
