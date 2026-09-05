# Semantic indexed-mutation source review — 2026-09-05

Status: AUDIT COMPLETE; one concrete auxiliary-checker coverage gap remains.

Base: `f34355b37dbd9e86ef574399e895a78fd41dd0a3` plus the primary task's
uncommitted repair. This is a read-only review of compiler/test sources, not
semantic authority, executable parity, installed-driver evidence, or a new
self-host implementation rung.

## Scope and evidence boundary

The directive is
[`ci_semantic_integration_review_2026-09-05.md`](../agent_work_directives/ci_semantic_integration_review_2026-09-05.md).
Only this report was written. No compiler was built, no test suite or invalid
program was executed, no binary was installed, and no Git write was performed.
The primary task owns focused-probe execution and full semantic parity.

Read-only commands: `git rev-parse HEAD`, scoped `git status --short`, scoped
`git diff`, `rg`, `Get-Content`, and `Get-FileHash -Algorithm SHA256`.
Observed HEAD matches the base above. The body/operator files were modified;
the focused probe was untracked. Source hashes at review time:

| File under repository root | SHA-256 |
| --- | --- |
| `src/self_hosted/semantic/body_check_owner.pgy` | `4EC065A2F29918D65CA9DEF23C100634262E7EA81D7ABCE6D94C6FD17A4D9DDC` |
| `src/self_hosted/semantic/expression_operator_fact_owner.pgy` | `1BCA0F89AF7CA564B4AFABA6240E4F0CE7F7FADEC361599088D17F57978A78B0` |
| `src/self_hosted/semantic/collection_mutation_policy_owner.pgy` | `AD81A90CF2D2DD104C4EABBDC201655B655D7E9C7D0DBA090EDD900B825D6B3D` |
| `tests/self_hosted/parity/fixture/semantic_index_mutation_owner_probe.pgy` | `00F0CD559AD3895E4932CFB304BA933A253CBE10B8CE48992A85AD4B8B0052FB` |

## Ownership assessment

The reached bare-binding repair has the correct ownership direction:

- `body_check_owner.pgy:272` bounds one statement with existing
  `FindStatementEnd`, consumes `SemanticTopLevelOperatorFactsFromExpression`,
  then delegates to `SemanticCollectionMutationError` at line 279. It does not
  duplicate the parameter-mode policy or recognize fixture names.
- `collection_mutation_policy_owner.pgy:80` remains the policy owner.
  The `ArraySet` mapping agrees with the production typed-assignment consumer
  in `ast_assignment_type_fact_owner.pgy:235`; this review does not expand
  collection eligibility or change `ref`/`own` semantics.
- `expression_operator_fact_owner.pgy:104` consumes `==`, `!=`, `<=`, and `>=`
  as pairs before the new assignment branch at line 118. Existing parenthesis
  and bracket depth prevents an equals sign inside an index from being
  mistaken for a top-level write.
- The operator owner now delegates comment skipping to the existing text-scan
  owner at lines 66 and 70. Quoted-string skipping remains before comments.
  `FindStatementEnd` also skips strings/comments, so a comment semicolon is not
  a statement boundary for this consumer.
- Environment lookups run from the newest binding backwards
  (`env_owner.pgy:3`, `:23`). `CheckBody` marks locals at line 91 and restores
  names/types/modes after `if`/`while`, `else`, and `for` blocks at lines 172,
  188, and 225. The mutation hook consumes these same scoped modes.

These are source observations, not proof that all accepted source forms reach
the new hook.

## Finding: grouped receiver skips the new mutation consumer

Priority: P2; scope is the auxiliary text checker, not a demonstrated
production compiler admission failure.

Bounded falsifier, not run by this review:

```pergyra
func F(xs: Array<Int>) -> Void {
    (xs)[0] = 9;
}
```

Expected ownership result: the same `value_param_collection_mutation` verdict
as `xs[0] = 9`, with receiver `xs` and the existing `ArraySet` policy.

Source evidence:

1. `parser_expr.c:650` consumes the closing parenthesis and returns the enclosed
   expression unchanged at line 651. `parser_expr_postfix.c:196` then attaches
   the index. Grouping the receiver therefore does not invent a new mutation
   semantic in the native syntax owner.
2. `body_check_owner.pgy:235` first asks `SemanticReadIdent` for a leading
   identifier. At `(`, that reader returns an empty identifier
   (`text_scan_owner.pgy:61`). The nonempty-identifier guard at line 273 skips
   the new indexed-write consumer entirely.
3. The subsequent `CheckCall` cannot recover this verdict:
   `call_check_owner.pgy:202` returns an empty result when the bounded text does
   not end in `)`. This example ends in `9`.
4. The retained probe's `nested-index` case is `xs[(0)] = 9`, not
   `(xs)[0] = 9`. It exercises grouping inside the index, not grouping of the
   target binding.

Inference from those branches: the current auxiliary `CheckProgram` returns
an empty error for the grouped receiver although the mutation policy would
reject `xs` if reached. No executable result for this falsifier is claimed.
The finding was sent to the primary task before completion of this report.

Suggested integration: first add grouped-default rejection and grouped-`inout`
acceptance to the focused probe. Resolve the target through an existing owned
grouping/target fact where possible; do not add a fixture exception, a second
mode table, or a native retry. Recheck full semantic parity afterwards.

## Focused coverage and unrun boundaries

The retained probe contains ten verdict cases. Its source covers bare indexed
and builtin mutation, parentheses inside an index, read, equality, an equals
sign in a block comment, `inout`, a local binding, inner shadowing, and restored
parameter mode after shadowing. `RequireMutationVerdict` checks the mutation
code and receiver on rejections and an empty diagnostic on accepted controls.
The probe does not assert the complete `func`/`mode` diagnostic payload.

Additional bounded cases worth retaining after the grouped-target falsifier:

| Case | Expected claim | Review state |
| --- | --- | --- |
| `xs[0] != 9`, `<= 9`, `>= 9` | Comparisons do not become writes | Pair ordering inspected; unrun |
| A line comment containing `=`, `;`, or `]` before a comparison | Comment text does not create a write or terminate the statement | Both scanner owners inspected; unrun |
| `xs[0 /* ] = ; */] = 9` | Comment delimiters do not hide the actual indexed write | Source inference; unrun |
| `while`/`else`/`for` inner local followed by an outer `xs[0] = 9` | The original default-parameter mode is restored | Restore paths inspected; probe currently uses `if` only |
| `(xs[0] = 9)` or a mutation embedded in an initializer/index | Nested assignment must not be described as covered by the top-level hook | Separate pre-existing expression-coverage boundary; unrun |

The last row is not a request to open a general expression-checker rewrite.
`parser_parse_assignment` accepts assignment expressions recursively
(`parser_expr.c:137`), whereas the auxiliary checker dispatches `let`/`return`
before the statement hook and the operator fact deliberately reports only
top-level positions. Keep this limitation explicit unless executable evidence
and the active integration objective justify widening the reached repair.

## Handoff

The owner-consumption direction is sound for the bare indexed-statement slice.
Do not state that all indexed writes are covered until the grouped-receiver
falsifier is resolved or explicitly excluded. Primary execution results,
source-owner hash evidence, full semantic parity, and current-driver evidence
remain pending integration here; this report increments no closure metric.

## Follow-up: grouped-target repair and residual comment boundary

This section supersedes the original finding's current-state verdict while
preserving its discovery evidence. The primary task requested a second narrow
source review; compiler sources and shared tests remained read-only for this
reviewer.

### Primary-reported execution evidence

The primary task reported the following; this reviewer did not rerun or
independently observe these process results:

- Adding the original grouped-receiver falsifier produced `grouped-index` and
  exit 1 in focused session `78674` before the repair.
- After the repair, the first focused probe inside the new full parity run
  passed under `.tmp/self_hosted/semantic_index_mutation/run.HCT7HL`.
- Full C/LLVM semantic parity was still running at that handoff. Neither that
  focused result nor this report asserts full parity, remote CI green, or
  current installed-driver closure.

### Independently inspected source delta

`expression_operator_fact_owner.pgy:77` now owns the first top-level index
opening position in `index_open_index`, using the same string/comment/depth
scan as the other operator facts. `CheckBody` at line 273 admits a grouped
statement prefix, bounds the target before the top-level assignment at line
280, and uses existing `SemanticStripOuterParens` to normalize both target and
receiver. The identifier reader plus complete-consumption check at lines
289–291 prevents an arbitrary target prefix from becoming an invented binding.
The final decision still delegates to `SemanticCollectionMutationError` at
line 292; no parameter-mode table or native fallback was added.

This resolves the original `(xs)[0] = 9` source path and also reaches the
whole-target grouping `((xs)[0]) = 9`. The probe now has grouped receiver,
grouped target, grouped `inout`, a benign comment inside receiver parentheses,
`!=`/`<=`/`>=`, and a line comment containing `= ; ]`. Rejection assertions also
require `func: ArraySet` and `mode: default`; accepted controls still require
an empty error. These are independently inspected source facts; the PASS
above remains primary-reported execution evidence.

Follow-up snapshot hashes:

| File under repository root | SHA-256 |
| --- | --- |
| `src/self_hosted/semantic/body_check_owner.pgy` | `15FAC3AC791EB11F3CCF70F17E9523F71774C6E57B6F2566B335FE4792FF5005` |
| `src/self_hosted/semantic/expression_operator_fact_owner.pgy` | `BEB6270B654C8BB060B299371339EBC3598CEFAF15533624C71A3F5EA62373E8` |
| `src/self_hosted/semantic/expression_normalization_owner.pgy` | `2CA3B375048425D26AC69D384FAC77D62CBCCD2E510DADF8640474B12EA9C84A` |
| `tests/self_hosted/parity/fixture/semantic_index_mutation_owner_probe.pgy` | `D5C3F9B733D7D3AC88F03A45E0AF6EA0D05D8BFBF97093D2F3E46C5C10F9914D` |

### Residual P2: grouped receiver normalization is not comment-aware

The original grouped-receiver finding is repaired for the retained forms,
but the newly reached normalization dependency has this narrower residual.
Two bounded, unrun falsifiers:

```pergyra
func CommentClose(xs: Array<Int>) -> Void {
    (xs /* ) */)[0] = 9;
}
func CommentGap(xs: Array<Int>) -> Void {
    (xs) /* gap */ [0] = 9;
}
```

`SemanticStripOuterParens` (`expression_normalization_owner.pgy:39`) skips
quoted strings but not comments. Its depth scan can therefore select a `)`
inside the first example's comment as the wrapper close. Separately, its
`close == StringLength(text) - 1` requirement does not accept the second
example's comment after the receiver's closing parenthesis. In both cases the
receiver remains prefixed with `(`; the subsequent identifier read/complete
consumption check does not reach mutation policy. The operator scanner itself
correctly skips these comments, which is why the disagreement arises at the
normalization consumer rather than in `index_open_index`.

This is a source-derived auxiliary-checker false-negative prediction, not an
observed executable or production-admission failure. It was immediately sent
to primary with both falsifiers. Add them as focused defaults plus a matching
`inout` control before calling the grouped-comment boundary closed. Keep
normalization with its existing owner rather than adding a body-local comment
parser or mutation-policy exception.

Nested assignment expressions such as `(xs[0] = 9)` or assignments embedded
inside an initializer/index remain outside this top-level repair scope. The
new target grouping does not imply coverage of those forms.

## Final follow-up: comment-aware wrapper normalization

Current bounded source-review verdict: the grouped receiver and the two
reported comment residuals are resolved in the inspected source. No additional
blocking finding was identified in this top-level indexed-write repair slice.
This supersedes the preceding residual's current-state verdict, not its
historical discovery record or the untested wider expression boundary.

### Independently inspected correction

`SemanticStripOuterParens` remains the sole wrapper-normalization owner.
The current delta in `expression_normalization_owner.pgy:39`:

- Finds a possible outer `(` after existing whitespace/comment scanning, so a
  leading comment before an inner wrapper does not stop repeated stripping.
- Delegates strings, line comments, and block comments to `SkipQuotedString`,
  `SkipLineComment`, and `SkipBlockComment`. Parentheses inside those spans no
  longer change the wrapper depth.
- Requires a real close and requires `SkipWhitespaceAndComments` after that
  close to reach the end before removing a wrapper. Therefore `(xs) /* gap */`
  can normalize, but `(xs) /* gap */ + 2` remains a partial grouping rather
  than losing its parentheses.
- Extracts only the actual wrapper interior and repeats with the same owner.
  `CheckBody` continues to consume the result through the complete identifier
  check and existing collection-mutation policy; it gained no comment parser,
  parameter-mode policy, or fallback.

The focused probe now retains `comment-close`, `comment-gap`,
`comment-nested`, and a matching `comment-gap-inout` control. It also explicitly
checks preservation of partial grouping and of comment-like text inside a
quoted string, in addition to the existing normalization contract. These are
source observations; this reviewer did not build or execute the probe.

Inspected snapshot hashes:

| File under repository root | SHA-256 |
| --- | --- |
| `src/self_hosted/semantic/expression_normalization_owner.pgy` | `D6166BC781328B099BAE20B2767DD63965A2BFB3E5278C150E43D149DF40132A` |
| `tests/self_hosted/parity/fixture/semantic_index_mutation_owner_probe.pgy` | `DDCC56DFD4B30298EB014D9131F8793C408133DFA995978B2F144B7D4D8DECA5` |
| `src/self_hosted/semantic/body_check_owner.pgy` | `15FAC3AC791EB11F3CCF70F17E9523F71774C6E57B6F2566B335FE4792FF5005` |

### Primary-reported execution and remaining integration boundary

Primary reported observing both comment falsifiers return `Status: ok` with
exit 0 from the then-current C manifest tool's `--check` before this correction.
That reported executable evidence corroborates the earlier source prediction.
Primary then reported an observed focused PASS under
`.tmp/self_hosted/semantic_index_mutation/run.QZi11k` after the correction.
These are explicitly primary-reported results, not reviewer-run commands.

At this follow-up, full current-source C/LLVM semantic parity was separately
running in session `63651`; no final result is asserted here. Owner hash/gate
evidence and driver integration remain primary responsibilities. Nested
assignment expressions, including `(xs[0] = 9)` and assignment inside an
initializer/index, remain outside the top-level repair. No new expression
feature, mutation policy, or parallel implementation lane is proposed.
