# Function-scoped String copy landing receipt

Observed 2026-10-23 KST. This receipt covers an uncommitted production slice
based on `d72620340c68b7831b43cc7053694d5846caed9e`. The unchanged canonical
program is still rejected before MIR, so this slice is not V1 closure and is
not eligible to push as a known-red revision.

## Objective card

- Objective: collect every exact String copy landing identity reached after
  the first copy refusal in the same routine, so the fixed three-round landing
  budget advances by routines rather than by individual copies.
- Priority: preserve the refusal set and routine boundary, then reduce landing
  rounds. Do not enlarge the round budget or continue general analysis after
  an error.
- Fact owner: `loan_copy_landing_owner`, keyed by diagnostic emission revision
  and the current function's stable syntax identity.
- Last consumer: the driver landing plan that inserts exact `Clone` operations
  before the next semantic analysis.
- Forbidden fallback: a program-global error tail, cross-routine collection,
  diagnostic suppression, rule weakening, or a per-program admission route.
- Falsifiers: an unrelated diagnostic closes collection immediately, and two
  routines with the same two-copy shape require two landing rounds rather than
  one global round.

## Implemented boundary

The diagnostic owner now advances a saturating emission revision for every
diagnostic attempt, including an attempt deduplicated from the published
diagnostic array. After an exact String copy identity is selected and its own
borrow refusal is emitted, the landing owner seals a tail using that revision
and the current routine identity. Only String copy discovery may continue
through that tail. A later diagnostic, revision saturation, missing routine,
or routine change closes it fail-closed. Non-String analysis still stops on
the existing `has_error` boundary.

## Exact candidate and focused evidence

The six changed semantic files in
`/root/cc-selection-tail-v3-probe-20261023-v1` were byte-identical to this
working tree. Its product binary SHA-256 was
`ca25ba6ef5409c285de744b33c55c4b2de1ff76a3e4fed0e4a87ae00b467be68`.

| Check | Observed result |
|---|---|
| Same-routine two-copy fixture | PASS in one landing round; emitted C and LLVM both printed `LR` |
| Two-routine boundary fixture | PASS in two landing rounds; emitted C and LLVM both printed `ABCD` |
| Rejected global-tail candidate | Falsified: it completed the two-routine fixture in one round |
| Existing focused positive/negative cases | PASS: exact kept-String cases admitted; ownership and window negatives refused |
| Full function-parameter-flow gate | PASS with the two new fixtures registered |
| Component contract | PASS: 14 unit checks, source caps and generator drift |
| Emitted C sanitizers | PASS for both new fixtures under ASan and UBSan, leak detection disabled |

## Unchanged refusal census

The final census binary was an analysis-only copy with ownership escape errors
downgraded after recording. It ran the fixed canonical input for three landing
rounds. Normalizing diagnostic sequence numbers and scratch-root paths gave an
exact multiset match against the previous product census:

| Event family | Previous product | Candidate | Removed | Added |
|---|---:|---:|---:|---:|
| Borrow-refusal events | 134,385 | 134,385 | 0 | 0 |
| Hard diagnostic events | 224 | 224 | 0 | 0 |
| Unique hard diagnostics | 59 | 59 | 0 | 0 |

The analysis-only run ended with 56 hard errors and 33,614 downgraded warnings.
Those counts include known downstream cascades and are not a DRV-2 completion
count. They establish only that this execution change neither removed nor
added a refusal event on the fixed input.

The ordinary product candidate still rejects the canonical program with
33,647 errors and 21 warnings and emits no MIR. Its first barrier advanced
from the opaque String crossing at source line 236 to
`expression_graph_owner.pgy:369`, where `ArrayPushOwnedString` refuses escaped
storage in `texts`. Bootstrap, `gen2 == gen3`, installed-driver C/LLVM checks,
and exact-SHA CI were not reached.
