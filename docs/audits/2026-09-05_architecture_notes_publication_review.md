# Architecture notes publication review — 2026-09-05

Status: AUDIT COMPLETE; primary's publication qualifications accepted by
readback in the integration follow-up below. No source implementation change
or new architecture work is proposed.

Base: `b4d22cf2c6e68fcbd42a1ce4a44444de0e7899fa` plus existing dirty work.
The user's integration authorization reopened the former other-session
exclusion for `docs/compiler_architectures/`. The directive is
[`integrated_publication_review_2026-09-05.md`](../agent_work_directives/integrated_publication_review_2026-09-05.md).
Only this report was edited by this lane; shared notes, compiler source,
tests, navigation, registry state, and Git state were not changed.

## Review performed

All eight Markdown files in `docs/compiler_architectures/` were read in full:
`README.md`, `pergyra_adoption_map.md`, `llvm_clang.md`, `rustc.md`,
`swift.md`, `ghc.md`, `mlir.md`, and `zig.md`.

Read-only commands included `git rev-parse HEAD`, scoped `git status --short`,
`rg`, `Get-Content`, strict UTF-8 decoding, `Test-Path`, registry row counting,
and `Get-FileHash`. No compiler/proof build, test matrix, artifact cleanup,
package installation, Git write, external publication, or new agent was run.

The local checks found:

- All eight files decode as strict UTF-8 without replacement characters.
- All seven README Markdown links resolve to the intended local note files.
- All ten occurrences of repository `docs/...md` references in the adoption
  map exist. They name eight distinct existing owner/navigation documents.
  These are code-formatted path references, not ten additional clickable links.
- There are 27 external HTTP(S) link occurrences. Their recorded targets use
  the LLVM/Clang/MLIR, rustc guide, GHC documentation, Swift/Zig repository,
  and Zig documentation namespaces shown in the notes. This review did not
  fetch those URLs, verify HTTP status, or confirm their substantive claims.

## Publication corrections requested

### P2: label the adoption baseline as a historical snapshot

At `pergyra_adoption_map.md:9`, the heading says current Pergyra baseline,
followed at line 11 by an explicit 2026-08-26 date. The historical date is
present, but the current-state heading and numerical checklist can be read as
the status of this publication. In particular, lines 13–16 record
`CLOSED=49`, `BRIDGE=36`, `ACTIVE=1`, and bootstrap/CI evidence `4/4`.

The current registry was independently counted during this review: 88 rows,
55 `CLOSED`, 32 `BRIDGE`, and 1 `ACTIVE`. Current `docs/00_progress.md` also
distinguishes published regular CI from missing exact-commit Platform full
evidence and from unpublished dirty-tree validation. This report did not query
remote CI and does not replace that evidence owner.

Requested correction: rename this section to a writing-time historical
snapshot, explicitly state that its numbers and `4/4` are not current
publication/CI evidence, and direct current status to the existing progress
and registry documents. Preserve the original dated values instead of creating
another live progress table in these reference notes. The historical values
were not independently reconstructed at a pinned 2026-08-26 revision, so this
review does not claim they were incorrect on that date.

### P2: separate original upstream-check claims from this integration review

`README.md:69` states that upstream rolling documentation/source was checked
on 2026-08-26. `zig.md:7` uses current-state wording, and lines 12–14 say its
source comments/declarations were checked on that date. These are existing
author claims. The checked-in note text does not attach pinned upstream
revisions or a reproducible verification receipt for that earlier inspection.

Requested correction: keep the historical provenance, but say explicitly that
the date describes the original note author's recorded inspection, that this
2026-09-05 integration did not revalidate upstream contents, and that rolling
`main`/`master` or generated development-documentation links are not current
implementation guarantees. Zig's opening can be scoped to the recorded
snapshot. This is not a finding that the original author failed to read the
sources or that the external descriptions are false; those facts are unknown
to this lane. No general upstream research should be opened for this edit.

### Clarifier: concept mapping is not an executable adoption inventory

`pergyra_adoption_map.md:44` labels a column as current Pergyra owner/seam,
but its entries are conceptual families, such as compiler-world request/
action/intent orchestration, rather than exact producer/consumer/gate paths.
The table's falsifiers are requirements, not recorded execution results.

`README.md:64` already says adoption does not create a work queue, and the
adoption map's final section requires the reached active rung and a green
focused gate before actual implementation adoption is recorded. Preserve that
good distinction. Add a brief qualification immediately above the table that
these are principle/candidate mappings, not evidence that every listed
interface, cycle diagnostic, invalidation contract, or gate is implemented.
Do not create a broad implementation task merely to fill the table's anchors.

All three publication points were sent to primary before this report was
completed. Primary owns any accepted edits to the eight notes.

## Owner and semantic-boundary consistency

The notes consistently state that external architectures are references, not
Pergyra compiler fact owners. Their stated priorities agree with the current
one-SoT, fail-closed, no-root-rescan, and no-unmeasured-framework expansion
rules. Nothing in these notes alone closes a registry row or advances the
active executable rung.

The strongest internal distinctions are preserved:

- LLVM/Clang and MLIR notes identify Pergyra AIR as verification-only, not a
  codegen stage. Zig's AIR is explicitly not equated with Pergyra AIR.
  This agrees with `docs/104_air_compiler_architecture.md:5` and the separate
  codegen/verification paths in
  `docs/semantics/09_abstraction_loss_contracts.md`.
- GHC/Swift lessons are framed as information-lifetime and verifier contracts,
  not a request to add matching numbers of IR layers.
- The rustc/Swift sections reject making a central evaluator/cache a second
  semantic authority. Swift's request API is explicitly not the definition of
  Pergyra Intent.
- `docs/180_compiler_logical_spine_handles_gates.md` remains an architecture
  frame with partial/absent targets, not evidence of whole-pipeline completion.
  The adoption notes point to this owner rather than redefining it.
- The current dogfood contract already records bounded source-C compiler-
  purpose Intent takeover. This review does not infer that such a path is
  wholly absent, nor does it promote the adoption table's generic cycle/request
  proposals into current production guarantees.
- The language word registry was independently counted at 146 declaration
  rows. The notes' separate selector/no-`channel` history remains attributed to
  `docs/199_language_word_and_dogfood_grammar.md`; this lane did not rerun its
  generated inventory gate or count selectors as substitution progress.

## Inspected source snapshot

All eight notes were untracked under their existing directory at inspection.
These hashes identify the pre-correction source review, not a future published
revision or a verified upstream snapshot.

| File under `docs/compiler_architectures/` | SHA-256 |
| --- | --- |
| `README.md` | `6F0D32CBBFDBD6D94482D5D55B1558A933A114F9D9CFAABA9968BF037F1AEEE9` |
| `pergyra_adoption_map.md` | `5D785FAE3AEC1ED23269B225F227A8DBF10DD5F6F31FD6D6B4DDD98223BE0514` |
| `llvm_clang.md` | `17867B0D783E5875EAFD8E486B3C1C01D0B73E61A345F01B9FDDBE5044FBB3EE` |
| `rustc.md` | `6AF910346C2E77BF659734825E2A0D13D34FA06E16804978E1A86C15BAC84212` |
| `swift.md` | `DF248938FEDC5F9F0506B6A5EFBD40CA3BD5771E774775EECA4A619A271E1322` |
| `ghc.md` | `AA66503B282CFF7DBF9B700BB14BC2F1C8056F21133090B91173DAAE08C215E5` |
| `mlir.md` | `87AEF74B2389D69B548A08F9FE4D6DB6AA260EA16F18B6C9E4C19AD361901741` |
| `zig.md` | `F85EDDF0F5DA4B0EFF84EE8A4E8F53F31A8E212FD06A36D66662F4AD097914A1` |

## Handoff

Local path integrity and current owner-document alignment are supported by
read-only checks. The dated current-state/provenance wording should be
qualified before publication. External implementation descriptions remain
historical referenced claims, not fresh verification by this review. No new
research, compiler mechanism, self-host closure, or remote CI success is
claimed.

## Integration follow-up — publication wording accepted

Primary integrated the recommendations into the architecture README, Zig note,
and adoption map. This lane independently re-read the changed wording and
checked only the new relative owner links; no upstream re-research, new gate,
source/test change, or other-note edit was performed.

- `pergyra_adoption_map.md:9` now labels the baseline as a writing-time
  historical snapshot. Lines 11–12 explicitly distinguish that snapshot from
  current state and say its original execution evidence was not revalidated
  during this integration. Lines 23–26 delegate latest status to the existing
  owners and forbid citing the old numbers as current completion percentages.
  The original values are preserved rather than converted into another live
  progress authority.
- `README.md:69` now attributes the 2026-08-26 upstream inspection to the
  original writer's recorded investigation and explicitly states that the
  2026-09-05 integration did not revalidate upstream contents. `zig.md:12–15`
  carries the same adjacent qualification for its source snapshot. These
  statements resolve the provenance ambiguity without asserting either that
  the original source descriptions were false or that this review verified
  them externally.
- `pergyra_adoption_map.md:46–47` now states that the table maps principles and
  candidate seams, not completed implementations or cycle gates, and requires
  concrete owners and executable gates for actual adoption. The prior
  no-independent-work-queue qualification remains intact.

The three new links were independently resolved from the adoption map's actual
directory and each target exists as a file:

| Relative link | Resolved repository target |
| --- | --- |
| `../00_progress.md` | `docs/00_progress.md` |
| `../semantics/sot_owner_spine_registry.md` | `docs/semantics/sot_owner_spine_registry.md` |
| `../self_hosted/17_pergyra_native_dogfood_contract.md` | `docs/self_hosted/17_pergyra_native_dogfood_contract.md` |

Readback snapshot hashes:

| File under `docs/compiler_architectures/` | SHA-256 |
| --- | --- |
| `README.md` | `A6F1C93445C0283FE0D3313458DE7A34FE9CBAC7FF496929006427CD5828A043` |
| `zig.md` | `55AF2F042D34E582664EDEC867CF5167B53B216D8C0C8CFB07E18FE2C3E759D0` |
| `pergyra_adoption_map.md` | `0749F41B1714CB56F86F1DCDE534B396412BE4C9E4A48820EE341199B8372026` |

All requested publication qualifications are accepted for this bounded notes
review. The 27 external references remain unfetched by this lane. This
acceptance is neither upstream implementation verification nor compiler/
bootstrap/CI completion evidence; primary retains publication responsibility.
