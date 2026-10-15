# Ownership refusal explanation proof receipt

Date: 2026-10-12 KST. Base HEAD:
`23d1e65fd1709104a0a8c4f47e315a964b356632` (`main`, same as `origin/main`).
Status: model/checker, negative mutation and corpus integration gates PASS.
No commit, push or installed-driver
replacement belongs to this work.

## Scope and observed evidence

The user's request was to write and prove the proposed Rocq explanation model.
The new model, extraction, independent proposition consumers and executable
controls implement the bounded scope in
[`ownership_refusal_explanation.md`](../semantics/ownership_refusal_explanation.md).
The objective and full planned chain are recorded in
[`the work directive`](../agent_work_directives/ownership_refusal_explanation_2026-10-12.md).

The active DRV-2 parser/EV/runtime work was already dirty and continued changing
in the shared checkout (including the flow owner and map query fixtures).
This task edits no production source. `gmon.out` and existing work are preserved.
Verification binds the proof/checker input generation by hash; it does not
freeze or verify the concurrent production candidate.

| Gate | Observed result | Receipt |
| --- | --- | --- |
| `ownership_refusal_explanation_smoke.sh` | PASS: 4 modules kernel-checked, no axioms/admits/unsafe kernel features; 41 extracted executable controls | `.tmp/ownership-refusal-explanation/run.Hp8qwu/` |
| `ownership_refusal_explanation_selftest.sh` | PASS: 3 planted regressions rejected in isolated copies | `.tmp/ownership-refusal-explanation/selftest.dtp2Tb/` |
| `documentation_quality_smoke.sh` | PASS: UTF, index, async wording, executable example surface | Observed local command output |
| `formal_semantics_smoke.sh` | PASS: 75 proof owners; 86 modules kernel-verified plus the approval export/binding consumer; existing 2 approved abstractions unchanged, no admits or unsafe kernel features | `.tmp/ownership-refusal-explanation/formal-integration.log` |
| Scoped document links, strict UTF-8, shell syntax and whitespace | PASS | Observed local command output |

The focused receipt includes `source.sha256`, `source-after.log`, `kernel.log`,
`controls.log`, `artifacts.sha256`, and the extracted `.ml`, `.mli` and Linux
reference executable. The latest focused run removed the OCaml unused-record
warning observed in the preceding development run `run.flgI8N`.
Proof source SHA-256:
`9ec0e0a636fe253ae4c70c884ccc38144d6ac22a5919f63b69bd6adf85257d7f`.

The extracted controls include 8 adverse-summary assignments across 2
boundaries and all 27 comparable summary pairs across those boundaries.
They reject wrong rules, parents, summary references, boundaries, sites,
targets, missing-fact identities and hazard traces. They also cover a seedless
cycle, longer-than-minimal valid derivations, read-before-growth, no relocation,
invalid allocation and a stale read of an unrelated register.

The planted checker regressions remove edge membership and target-reader
binding; the model's proofs reject both. The third weakens
`remote_break_attributed` to `True`: the model still compiles, but the independent
consumer fails on the lost proposition. This checks theorem meaning rather
than a theorem-name inventory.

Toolchain: Linux `Ubuntu-E-WSL`, explicit
`OPAMROOT=/home/c/.local/share/pergyra-rocq/opam`, project wrapper, Rocq 9.3.0 /
Stdlib 9.2.0. Existing core nested-inductive registration warnings and the
extraction default-directory warning are visible in the focused log. Extraction
runs in a new temporary directory. The kernel reports the existing default
indices theory explicitly; it is not silently counted as an added axiom.
The full corpus also reports pre-existing absolute-name masking, deprecated
notations, nested-inductive registration and approval load-path warnings.
They did not invalidate the observed kernel/assumption-budget result.

## Changed files and handoff

- New proof: `docs/semantics/proofs/OwnershipRefusalExplanation.v`.
- Independent consumers: `tests/coq/OwnershipRefusalExplanationAudit.v` and
  `tests/coq/OwnershipRefusalExplanationExtraction.v`.
- Extracted checker controls: `tests/ocaml/ownership_refusal_driver.ml`.
- Gates: `tests/ownership_refusal_explanation_smoke.sh` and
  `tests/ownership_refusal_explanation_selftest.sh`.
- Registration: `tests/coq_kernel_check.sh`, `tests/formal_semantics_smoke.sh`
  and `.github/workflows/formal_proofs.yml`.
- Scope/evidence/navigation: the semantics note and README, this receipt,
  the work directive and a bounded completion note in the current handoff.

All are uncommitted at the base HEAD above. Existing production dirty state
remains separately owned. The active executable rung is still DRV-2; this
proof request does not advance it. Last green proof gate is the full formal
smoke above. CI execution is not claimed; the new gates are registered for it.

## Remaining boundary

Production EV provenance export, admitted source/MIR-to-graph binding, feasible
source execution paths, summary generation identity, diagnostic rendering,
first-parent storage cost, and an all-refusals DRV-2 witness census are OPEN.
The trace checker proves execution of its finite descriptor model. It does not
prove that a supplied model trace occurs in a particular Pergyra program.
No collection SoT row, ownership-clean implementation or self-host substitution
rung is closed by these results.

Next falsifying production fixture: bind a refusal to its admitted generation,
then substitute a same-shaped fact from another function/field/generation. The
production bridge must reject it before the reference checker is used to claim
source-level explanation coverage. The current model gate already rejects
unregistered graph facts and a witness for another requested boundary.
