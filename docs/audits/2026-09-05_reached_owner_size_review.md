# Reached CI owner-size policy review — 2026-09-05

Status: READ-ONLY DIAGNOSIS COMPLETE. Implementation and its gates belong to
primary; this report does not claim that the repaired CI is green.

The follow-up below records the final eight-owner candidate and independent
focused verification. The original five-file diagnosis is preserved as the
initial checkpoint, not the final migration inventory.

Reviewed revision: `918a7c98e541771fab542713262b643d375d51c0`. Primary supplied
Platform full run `33966692630`, macOS job `101307966639`, and the five reported
violations. This reviewer inspected local source and Git history, not the remote
job. Concurrent primary/other-agent changes are preserved. This report is the
only repository source/document file edited by this review lane; later focused
gate execution creates its bounded synthetic fixtures under `.tmp/`.

## Objective and verdict

- Objective: identify the authority behind the five reached line-cap failures,
  without raising limits to fit the current source or splitting coherent owners.
- Priority: one responsibility-specific cap owner, unchanged source ownership,
  explicit failure and negative checks, then cross-platform integration.
- Existing policy decisions: component gate responsibility-specific caps.
  Compiler semantic ownership remains with the named Pergyra owners and
  `src/self_hosted/OWNERS.md`; a size table is not semantic authority.
- Last consumers: component line-cap checks and the generic production/semantic
  scans in `tests/test_inc_size_smoke.sh`.
- Forbidden repairs: duplicate exception tables, blanket cap increases, excluding
  these production files from coverage, guessed missing-row defaults, and
  mechanical source splits without a distinct responsibility.
- Falsifier: a cap-plus-one file must still fail; an ordinary unregistered file
  must not inherit another owner's exception.

The immediate failure is **pre-existing conflicting size policy**, not a
macOS/BSD line-count discrepancy or source growth introduced by `918a7c98`.
There is real historical LOC growth; this finding does not certify that every
large file is an ideal final design. It does show that a mechanical split is
not justified by the reported failure alone.

## Observed counts and existing decisions

PowerShell byte counts found exactly these LF counts and zero CR bytes in all
five sources. A narrow GNU `wc -l` plus the existing `awk` threshold projection
reported the same five violations as primary's CI excerpt.

| Source under `src/self_hosted/` | LF count | Existing component cap | Existing cap location |
| --- | ---: | ---: | --- |
| `codegen/emission/stmt_emit.pgy` | 800 | 800 | `require_stage_owner_line_cap`, lines 469–473 |
| `semantic/ast_enum_payload_variant_provenance_verdict_owner.pgy` | 1438 | 1450 | Same function, lines 480–484 |
| `compiler/direct_mir_composite_intent_program_llvm_emission_owner.pgy` | 853 | 860 | Composite owner table, line 7921 |
| `compiler/direct_mir_role_override_program_identity_owner.pgy` | 800 | 800 | Role override table, lines 24744–24753 |
| `compiler/direct_mir_composite_intent_program_plan_owner.pgy` | 875 | 900 | Composite owner table, line 7920 |

Component anchors are in
[self_hosted_component_contract_smoke.sh](../../tests/self_hosted_component_contract_smoke.sh).
Line numbers reflect the inspected working source; concurrent integration may
move them. The named functions, paths and tables identify the decisions.

The component comments give explicit ownership reasons for two exceptions:

- Statement emission retains lexical cleanup across normal, return and loop
  exits; splitting that CFG state could create another statement-control owner.
- Enum provenance keeps bounded `Known/Unproven` transfer, structured joins and
  final payload admission together. The source owner contract at
  [OWNERS.md](../../src/self_hosted/OWNERS.md), lines 667–671, forbids rendered
  source recovery, MIR feedback, unresolved-receiver success and unchecked loop
  generations.

These are existing responsibility decisions, not new reasons invented to pass
the generic gate. The composite plan carries admitted execution topology; its
LLVM owner materializes a sealed plan. Role override identity retains its exact
declaration/routine/expression-graph identity boundary. This review did not
attempt to redesign those compiler responsibilities.

## Conflicting consumers

[test_inc_size_smoke.sh](../../tests/test_inc_size_smoke.sh) independently owns:

1. A default `SELF_HOSTED_OWNER_MAX_LINES` limit of 699 at line 7. Its production
   Pergyra scan at lines 221–238 excludes fixtures, expected outputs and tools,
   but does not consume any of the five component exceptions.
2. A second semantic scan at lines 254–269 with a literal 599 limit. It includes
   top-level Pergyra semantic owners and excludes only `diagnostic_owner.pgy`.
   Therefore an exception added only to the 699 scan leaves enum provenance
   failing at the 599 scan.

The Makefile invokes this script directly; no relevant override was found in
the inspected call path. Both these generic rules and the five scoped caps
already exist in `918a7c98^`. The current publication did not introduce this
conflict.

Adjacent, not part of the five-file finding: the diagnostic vocabulary owner has
a 650 default in the generic gate and a 660 component cap. Do not silently turn
this observation into a wider policy migration. Native C/H, helper, tool,
type-system and fixture rules retain their separate scopes.

## Historical provenance

For every source in the table, `git rev-parse 918a7c98:<path>` and
`git rev-parse 918a7c98^:<path>` yielded identical blobs. The restricted source
diff across the publication is empty.

| Most recent source-changing commit inspected | Observed source delta | Relevant cap history |
| --- | --- | --- |
| `56ecb3359d00509541bef3840091d450fc3614ed` (2026-09-03), evaluate match scrutinees once | Statement emission 794 → 800 lines | No change to component or generic cap gate in that commit |
| `95148fdc490cf8e859f4341e54a442d565de97dd` (2026-09-05), prove active enum variants | New enum provenance owner, 1438 lines | Same commit adds component cap 1450 with the single-authority rationale; generic gate unchanged |
| `d7b785757a6acc7f2e08f54c31731384388294d6` (2026-08-29), close intent observability ABI bridge | Composite plan 788 → 875; LLVM emission 853 → 853 | Plan cap 800 → 900 in the same diff; LLVM cap 860 unchanged; generic gate unchanged |
| `506c25272637203ee6bb8a0f6b9462d2a6f226fd` (2026-09-05), preserve role override target identity | Role identity 793 → 800 | No change to component or generic cap gate in that commit |

These observations establish prior growth and policy divergence. They do not
establish the original introduction commit of every older exception.

## Minimal correction proposed to primary

1. Move the five existing decisions, **unchanged** at 800/1450/860/800/900, into
   one small responsibility-named test cap owner keyed by exact repository-
   relative source path. Preserve the ownership explanations. Do not derive caps
   from the current file size or select the largest of conflicting local values.
2. Make both the generic 699 scan and semantic 599 scan consume those same
   registered path decisions; retain their defaults for unregistered sources.
3. Make the existing component checks consume the same owner and remove their
   five local numeric decisions. Do not leave a fallback copy of the old table.
   Keep unrelated component caps and the role override family aggregate of
   **1250** lines (currently lines 24770–24777).
4. Fail explicitly for a missing/unreadable cap owner, missing required row,
   duplicate or malformed row, invalid path or missing/unreadable required source.
   A required registered owner must not silently fall back to the generic cap.
5. Preserve the meaning of explicit caller limits such as a stricter
   `SELF_HOSTED_OWNER_MAX_LINES`; do not make a deliberately narrower bound
   silently ineffective. State its interaction with registered responsibility
   caps in the focused test contract.

There is an existing scoped precedent:
[scalar_program_owner_caps.tsv](../../tests/self_hosted/parity/scalar_program_owner_caps.tsv)
is the shared scalar-program cap SoT, and
[expression_graph_identity_carriage_owner.sh](../../tests/self_hosted/parity/expression_graph_identity_carriage_owner.sh)
rejects missing/duplicate/invalid required rows without a local numeric fallback.
None of the five reached owners belongs to that scalar table. Reuse its bounded
reader pattern if useful; do not silently broaden its scalar responsibility into
a generic compiler policy bucket.

This proposal is a test-policy ownership repair, not a compiler implementation
track, source refactor, SoT closure increment or self-host substitution increment.

## Required focused falsifiers

- Exercise the actual shared selection/consumer path, not a test-local copy:
  exact cap accepts, cap plus one rejects. Cover both generic and semantic
  consumers so the enum exception cannot be accepted by one and rejected by the
  other.
- An unregistered production Pergyra source at 700 lines still fails the 699
  default; an ordinary semantic owner at 600 lines still fails the 599 default.
- Missing required rows/table/source, duplicate rows, malformed/noncanonical
  numeric values and input read errors fail explicitly. They must not become
  zero-line sources or fallback success.
- Same basename, suffix, lookalike path and an unregistered neighbor do not
  inherit a registered source's cap. Exact source identity owns the exception.
- A stricter explicit caller limit remains observable. No new environment-based
  relaxation or backend skip is introduced.
- Retain role-family aggregate 1250, single-issuer/typed-projection checks and
  the existing semantic negatives. A shared individual cap must not bypass a
  family constraint or a behavioral falsifier.
- Ratchet the old duplicate numeric decisions out of consumers. Preserve the
  production scan's existing fixture/tool exclusions and covered source kinds.
- Keep shell mechanics compatible with macOS Bash 3.2 and BSD utilities; avoid
  introducing Bash 4-only maps or a GNU-only count protocol. Count semantics
  should be explicit: the generic gate uses newline counts, while the existing
  scalar reader uses `awk NR`; this review's five LF sources agree under the
  observed `wc`/byte census and do not justify a global counting-policy change.

## Execution evidence and remaining work

- Direct byte LF/CR census: observed counts above, zero CR bytes for all five.
- Narrow five-file GNU `wc -l` plus the existing 699 `awk` projection: printed
  all five reported violations. The filter process exited 0; it was a census,
  not execution of the complete gate's failure branch.
- Full `timeout 60s bash tests/test_inc_size_smoke.sh`: session `15175`, exit
  **124**, without output before the budget expired. No full-gate PASS or complete
  local reproduction is claimed, nor is owner-stage reachability inferred.
- Bounded Git source history/blob/diff inspection completed. An additional
  bounded blame attempt timed out at 55 seconds with no usable result; the
  conclusions above rely on the successful direct commit inspections instead.
- No compiler build, full MIR production, remote job query, source/test edit,
  artifact deletion or Git write was performed by this lane.

Primary still owns the implementation, observed focused negative gate and
exact-revision CI reconciliation. This audit alone is not CI-green evidence.

## Follow-up — final shared-cap integration review

Status: scoped source review ACCEPTED; independent focused checks PASS.
The complete production size gate result below is primary-reported, not a second
full execution by this reviewer. No source or test file was edited by this lane.

Reviewed the complete new
[responsibility table](../../tests/fixtures/self_hosted_responsibility_caps.tsv),
[size-policy owner](../../tests/self_hosted_owner_size_policy.awk) and
[focused checker](../../tests/self_hosted_owner_size_policy_smoke.sh), plus the
component and production-size consumer diffs. The original five numeric caps
are unchanged. Three later-reached conflicts extend the migration to exactly
eight paths:

| Additional source under `src/self_hosted/semantic/` | Observed LF / CR / final byte | Existing component cap moved unchanged |
| --- | --- | ---: |
| `ast_assignment_type_fact_owner.pgy` | 600 / 0 / 10 | 600 |
| `ast_statement_fact_owner.pgy` | 600 / 0 / 10 | 600 |
| `diagnostic_owner.pgy` | 654 / 0 / 10 | 660 |

This reviewer independently counted these bytes. Each additional source also has
an identical blob at `918a7c98` and `918a7c98^`; the explicit 600 decisions were
verified in the parent component gate, and its diagnostic 660 branch was already
recorded in the initial review. Primary reported that the full gate reached the
two semantic 599 conflicts and then the diagnostic 650 conflict after the first
five failures were repaired. Thus the previously adjacent diagnostic divergence
became a reached failure, not an unrelated cleanup track.

An interim hypothesis that the two 600-line sources differed between newline and
record counts was retracted before implementation acceptance. Both have 600 LF
bytes and a final LF. **Counting differences did not cause those two failures.**
The migration does reconcile which existing cap applies to these exact paths;
it is not a claim that the old conflicting generic gate accepted the same set
of source sizes.

### Consumer and failure-path findings

- The table now owns all eight responsibility limits. Component consumers call
  `require_responsibility_owner_max_lines`, which obtains the exact required row
  through the AWK owner and fails if the authority is missing or invalid.
  Both prior assignment-owner 600 literals were replaced, including the second
  occurrence formerly near line 17127. The stage default no longer independently
  decides limits for the registered semantic/codegen sources.
- The production scan consumes batch `wc -l` rows under `set -euo pipefail`.
  Unregistered sources retain the previous newline-count defaults, 699 for
  production and 599 for top-level semantic sources. Registered sources are
  read by the policy owner and retain the component's record-count contract.
  No source newline was added to manufacture acceptance.
- Missing/unreadable cap inputs or registered sources, malformed/duplicate rows,
  invalid defaults, malformed count records and duplicate scan paths have
  explicit failure paths. An unregistered source's read failure is owned by
  `wc`; the caller's pipeline must retain its nonzero status. This is not a claim
  that arbitrary fabricated `wc` records independently prove source existence.
- The initial candidate accepted a successful partial inventory: one projection
  path with an explicit 799 ceiling omitted all five then-registered larger
  owners. This reviewer observed exit 0 and reported it. The final `END` guard
  requires every registered path to appear; that counterexample now exits 1.
  This proves registered-owner coverage, not a standalone proof that an arbitrary
  supplied inventory enumerates every unregistered repository source. The
  production `find` pipeline continues to own that enumeration.
- Explicit narrower `SELF_HOSTED_OWNER_MAX_LINES` is applied after selecting a
  registered cap. The separate diagnostic limit derives its default from the
  same shared lookup and still applies an explicitly supplied diagnostic limit
  in its existing check. It cannot loosen the shared cap enforced earlier by
  the production scan.
- The role-family aggregate remains 1250, with the surrounding single-issuer,
  typed-projection and semantic negative checks retained. Native semantic C/H,
  tool, helper, fixture exclusions and unrelated component caps were not folded
  into the new responsibility table.

### Final observed checks

- Independently ran `tests/self_hosted_owner_size_policy_smoke.sh`: PASS. The
  enclosing syntax/focused/counterexample command completed in about 5.96 seconds.
  `bash -n` also accepted the focused checker and both changed shell consumers.
- The focused gate uses the actual eight-row table for cap/cap-plus-one cases;
  it covers 699/599 defaults, basename lookalikes, explicit tighter limits,
  newline-versus-record behavior, missing inputs, malformed/duplicate caps,
  partial inventory and the actual component lookup consumer with changed and
  missing cap rows.
- Independently replayed the partial-inventory counterexample against current
  source: `registered source missing from scan`, exit 1.
- Independently supplied all eight valid owner counts and then a producer exit
  7: the pipeline preserved exit 7 even though the policy scan could otherwise
  accept the records. Producer failure was not converted to green.
- Extracted and executed the actual diagnostic settings/check blocks from the
  production gate without executing its unrelated sections. Shared default 660
  accepted the real 654-line owner; explicit diagnostic ceiling 653 rejected it
  with `654 > 653`, exit 1. This is focused consumer evidence, not a full-gate run.
- Primary reported full `tests/test_inc_size_smoke.sh` PASS, session `47247`,
  exit 0 within the 60-second budget. This supersedes the initial timeout for
  primary's repaired candidate only; it does not retroactively make the initial
  execution successful or establish all CI/platform jobs as green.

No further blocker was found in this bounded size-policy review. Native macOS
execution and exact-pushed-revision CI remain primary's integration evidence;
the independent checks here ran with the local MSYS tools. The audit and its
synthetic fixture count do not advance SoT closure or self-host substitution.
