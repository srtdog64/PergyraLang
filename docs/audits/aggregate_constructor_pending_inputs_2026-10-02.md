# Aggregate constructor pending-input review

Base: 2c4891cc1dc765364e47b17a95e713523d25cb4c.
Status: read-only audit complete; actual aggregate Release remains BLOCKED.
This file is navigation evidence, not semantic or closure authority.

Root alone edited, built and integrated. Three read-only reviewers examined
producer/current-definition selection, MIR/negative boundaries, and filtering
with existing U/R/N/E effects. They inspected retained receipts but did not
independently rerun tests.

Initial carrier review found coherent stale definitions, missing/duplicated
rows, cross-root tuples, and base-callee identity could evade row-local joins.
The final owner replaces that weak check with admitted surface intervals,
cursor coverage and exact producer replay. Whole deletion is refused when
eligible constructor edges exist; a genuine zero-input family remains valid.
Old/future definition pairs and named-to-unproved downgrades are refused too.

The candidate filter excludes known non-ArrayString inputs, including scalar
literals and exact known non-array locals. It conservatively retains all
formal modes, unknown leaves and value-producing expressions. Existing own
formals and ArrayString locals bypass the exclusion. Call return types are not
yet owned at this stage and are not used to infer a scalar default. No measured
speedup is claimed. The source comment now names known non-array inputs rather
than implying that the filter excludes only scalars.

Root reproduced an intent-context regression in constructor-intent-regression.
jfSaND: baseline C/LLVM admitted the source, while the initial candidate
returned ast_artifact_invalid / collection_call_effect_facts. The final carrier
preserves both function and intent IDs, requires exactly one valid context,
and compares both against the admitted surface. Orphan, mixed, coherent forged
function and wrong-intent mutations refuse. Two reviewers examined the
correction and retained results without another blocker. This validates intent
source admission, not intent execution or dogfood substitution.

Observed root gates: constructor-field-input.t6bbK6 (80), existing collection
effect integration kBSbUX (724), and actual refusal recheck zcTDyE (8). C/LLVM
outputs and frozen receipts match; the two new probe builds had zero errors
and nine existing unreachable warnings each. These are analyzer executions,
not executions of the supplied programs or actual Release admission. Actual
empty/owned/borrowed Release still first refuses names syntax30; FromArtifact
still first refuses JsonOwnedFragmentWriteFile syntax8026. The earlier cold
collection-inout-effect.lgLPiW attempt timed out at the five-minute edit-loop
budget; the final integration shard compiled six distinct targets once each.
Scoped static owner/cap/Bash/diff checks pass, but the full component inventory
timed out at its existing match-pattern checkpoint after 60 seconds. No full
bootstrap, installed-driver or performance matrix was rerun.

Still missing: event-time Live/exclusive/releasable source authority;
constructor reserve/consume/seal and alias exclusion; exact aggregate current
value/return/nested carriage; actual caller/formal discharge; versioned MIR
entry, drop and exact retired field/outer-descriptor writeback. MemberMove
Unknown-only and EmptyLiteral-only receipt validators are unchanged. No field
grant consumer was added. SoT is still 70 CLOSED / 23 BRIDGE / 2 ACTIVE.

Exact-base CI 37014228319 completed failure in Linux fixed-point/breadth step4;
public log access was HTTP403 with no artifacts and only exit2 annotations.
The first compiler diagnostic is unavailable, not inferred from names30 or
Json8026. Windows metadata was completed/success. No CI repair is claimed.
