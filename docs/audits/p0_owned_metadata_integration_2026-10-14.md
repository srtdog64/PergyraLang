# P0 owned metadata integration

Status: LOCAL CANDIDATE; full bootstrap, installed driver and published-SHA
compiler CI remain OPEN. Evidence below does not close V1 automatic cleanup
or the parent SoT family. Base: `af8ede6f0bf6a38f389b09f29d04b84b875c461d`.

## Executed repair and invariant

The fixed-input observation rejected borrowed metadata publication in
PartForLocal. The chosen repair returns an exact type row and publishes four
independent owned String columns; codegen publishes two independent name
columns. Existing handled-error and last-emission consumers retire these
internal columns. This manual internal retirement is a P0 bridge, not the
language's automatic cleanup implementation. Its removal condition remains
the admitted MIR ownership producer and shared per-type cleanup glue.

The next actual refusal was aggregate-release shared-demand uniqueness, not
a missing returned-field summary. The same origin was cleaned in two mutually
exclusive error paths; one cleanup occurred in a loop but immediately left
the routine. The existing plan producer already issued that exit fact. The
retirement owner now consumes it without relaxing deferred/repeated cleanup.
Invalid arena shapes and foreign callable frames are rejected. The exit Bool
is issued and consumed immediately on the same artifact; the helper is not a
standalone validator for arbitrary stale or forged facts.

HashMap Set/Remove now participate in the existing growable-storage boundary.
Checked constructor identity supplies freshness; corrupted mutation identity
cannot become a no-op permission. User/formal callable overrides are retained.

## Actual observations

| Boundary | Observed result | Receipt |
| --- | --- | --- |
| Native seed | Parser, semantic3191/0, diagnostics PASS | `.tmp/ci-native-main-root-20261014/` |
| Portable Map currentness | C/LLVM, five real Set/Remove corruptions and restoration PASS76.013s | `.tmp/A-ci-map-currentness-gate-20261014/` |
| Routine-exit source behavior | Original32 plus five new cases, C/LLVM PASS95.022s | `.tmp/B-retirement-behavior-v3-20261014/` |
| Behavior gate after SRP | All37 cases plus generation parity PASS99.629s | `.tmp/B-retirement-behavior-v4-20261014/` |
| Physical metadata retirement | C/LLVM normal6 columns/27 payloads/6 backings; handled error4/4/4; real frees and descriptor reset | `.tmp/C-owned-metadata-backing-20261014-v4/` |
| Output lifetime | Four original/instrumented outputs equal23640 bytes | Same runtime receipt |
| Final v6 structural inventory | PASS7.433s/60,2768 cap requests, generator drift and14 checker self-tests | `.tmp/ci-p0-recovery-v6-root-20261014-static/` |
| Final v6 formal-key behavior | C/LLVM PASS84.037s/300, currentness/aliases/malformed facts | `.tmp/ci-p0-recovery-v6-root-20261014-formal-key/` |
| Final v6 native storage behavior | PASS16.162s/300, original cases plus six consume/repeat/read controls | `.tmp/ci-p0-recovery-v6-root-20261014-native/` |
| Final v6 unchanged full bootstrap | FAIL196.108s/1800, nominal MIR root control node80071/aggregate_release_plan_unproved; no gen1/fixpoint/installed evidence | `.tmp/ci-p0-recovery-v6-root-20261014-bootstrap/` |
| Current syntax mapping | Parser rc0/guards0; actual82212-row compact AST maps80071 to verdict-owner278 cleanup in binding_move_use.found branch | `.tmp/A-ci-v6-node-map-20261014/` |
| Current first-guard observation |222.232s/300, status2/guards0/no emission; named-source collision of different fields sharing definition60232; routine-exit facts valid | `.tmp/B-ci-aggregate-unique-v6-first-v2-20261014/` |
| Completed-plan named-source census |229.499s/300,65 demands, COMPLETE for this assembled plan, original80071/status2/guards0 preserved | `.tmp/B-ci-aggregate-unique-v6-census-20261014/` |
| Behavior-only focus gate after path repair | PASS134.279s/300/guards0;18 positives,7 negatives,19 poison cases; target-C and pthread observables retained | `/home/c/pgy-focus-behavior-only-C-20261014-v2/run/` |
| Joined v7 structural gate | PASS6.283s/60, guards0, original caps | `.tmp/ci-p0-recovery-v7-root-20261014-static/` |
| Joined v7 formal-key behavior | C/LLVM PASS75.454s/300, guards0 | `.tmp/ci-p0-recovery-v7-root-20261014-formal-key/` |
| Joined v7 aggregate query | FAIL41.218s/300, guards0, C baseline premise refused;49 source cases and LLVM query not reached | `.tmp/ci-p0-recovery-v7-root-20261014-aggregate/` |
| Joined v7 focus behavior | PASS130.497s/300, guards0; actual recovery/negative/poison observables | `.tmp/ci-p0-recovery-v7-root-20261014-focus/` |
| Joined v8 structural gate | PASS5.807s/60, guards0, original caps | `.tmp/ci-p0-recovery-v8-root-20261014-static/` |
| Joined v8 aggregate behavior | C/LLVM53 and query13 PASS157.436s/300, guards0; all four built binaries guarded before use/EXIT | `.tmp/ci-p0-recovery-v8-root-20261014-aggregate/` |
| Joined v8 unchanged full bootstrap | FAIL178.818s/1800, guards0, peak1.8GiB; first indexed-formal entry refusal node67321 before alias finalization | `.tmp/ci-p0-recovery-v8-root-20261014-bootstrap/` |

Fresh parse-only mapping of that same control has82551 nodes and source/input
guards0. Node67321 calls FocusPartRepack; caller ref inferred_types formal67278
flows to target formal66927, then ScopedLocalBindingIdentityForGraphLeaf and
ForName. Actual selected restoration source lines are366 (call) and82 (callee),
not the separate shared WIP's line466/184. Artifact/receipt:
`.tmp/C-ci-v8-node-map-20261014/`. This new first failure occurs before the
earlier aggregate plan boundary; it does not prove that all later canonical
obligations are closed. Full canonical remaining-refusal count remains unknown.

The v7 positive fixture's actual C analyzer reports
`aggregate_release_incomplete` in `Retire`, at `names.left` extraction
(syntax11). This is not a timeout or a passed poison test. The probe retains
its full-body admission requirement while the fixture/consumer contract is
investigated. v7 has not established full-bootstrap or installed evidence.

The guarded setup comparison confirms kind3/3 and PairReady=true. Independent
seeds still fail the unsupported own extraction; existing inout writeback
admits the intended positive. Rechecking the twelve aligned new fixtures
finds one wrongly accepted post-capture alias (`names.right = names.left`
after an owned push). Its expected refusal is unchanged. Ten other negatives
retain their original diagnosis. C uses v7/B2 and LLVM uses B1, so this is
not complete same-generation parity. Unsafe supplied code was never emitted
or run. Receipt: `.tmp/C-empty-capture-retire-alignment-20261014/analysis/`.
Existing assignment facts already carry the necessary member sides; the
leaf-only join in the exclusivity consumer is the reached missing check.

The identified native executable is
`7cac83dda2f096f2f6528b12d4f29ec7f972e983f95726896d629e65c33bbb14`.
Runtime output SHA256 is
`796693c7f2f19859235311b6253b285770e492e7be0541f54bcbac90072b94f2`.
Inputs, source membership, native and relevant artifacts were guarded before
and after these runs. Imported/unreachable/const-qualifier/target-triple
warnings remain explicit; PASS never means warning-free.

## Responsibility migration

Focus schema/shape and current-generation admission are subordinate to the
existing collection-ownership family. The issuer retains construction;
schema owns the existing rows and retirement; admission owns use/copy/exit
validation and the focus receipt binding. The19 existing definitions moved
without a second implementation. Body receipt, carrier, consumption and
terminal-restoration no longer own duplicate focus policy. Mutation imports
the two actual owners instead of relying on the composition root's closure.

Original production caps remain: receipt90, carrier90, transition180,
terminal restoration30 and consumption440. Final counts are90/88/180/24/440.
Schema155/180 and admission545/600 are cohesive bounded subordinates, not
new top-level semantic families. Independent review found no import cycle or
missing owner path after the migration.

## Limits and next acceptance

The metadata ledger observes the two stated inputs. It does not prove OOM
cleanup, all handled-error paths, arbitrary reuse, ASan safety, general graph
reclaim, or automatic cleanup of every aggregate. In particular, its old
duplicate-backing instrumentation claim is narrower than arbitrary repeated
descriptor retirement; per-frame duplicate validation needs separate evidence.

Published9/97 refusal census rows were conditional after continued failures;
they are not the remaining count of this candidate. Required next evidence is
the unchanged full bootstrap/fixpoint, fixed DRV-2 input with zero refusals,
actual installed pair and public routes, followed by executed required CI jobs
on the exact pushed implementation SHA. Docs-filtered skipped green is not
compiler acceptance. Unrelated shared WIP and all earlier frozen generations
remain preserved.

The new v6 failure must be observed at its current first uniqueness/source
guard before another fact or permission change is chosen. Do not transplant
v2 syntax IDs or revive the withdrawn returned-field-summary explanation.
Gen0, parser and call-argument/status controls passed; they do not substitute
for admission of the unchanged whole control. A/B are investigating this same
P0 boundary; no new implementation track is activated by the failure.
The actual v6 artifact observer independently maps that call to
`ast_collection_ownership_verdict_owner.pgy:279:9`, under callable79917.
This validates source identity and location, not runtime execution of that
error branch. The completed observer then isolates the named-storage-source
guard: inputs636/637 have different fields60204/60205 but one seed definition
and source60232. Retirement-site checks and the same-generation exit fact
are valid. This is a newly reached source distinction, not failure of the
former exit-fact repair.

EmptyFocus initializes its four String columns from one `[]` descriptor. The
actual emitted C and runtime agree on zero capacity and NULL backing at that
initialization. A sound compiler decision still needs this fact at constructor
capture: mutation between the empty declaration and capture would invalidate
it. Neither different field IDs nor length0 alone proves independent backing.
The chain survey and full current-plan collision enumeration precede repair.

The focus gate's generated-layout word approvals were removed in a test-only
candidate; actual target observables and all18 positive/7 negative/19 poison
cases remain. Its first Linux run failed128.102s after both C/LLVM builds,
before any behavioral invocation: a relative work path was passed to an
absolute-root path operation. Guards stayed unchanged and it did not time
out. This is a test transport failure, not behavioral acceptance or a
production verdict. The second run only normalized that path to absolute on
every platform, retaining the existing Windows conversion and original budget,
and passed. This gate builds the Pergyra analyzer/emitter with C and LLVM and
compares emitted-C bytes; it executes the actual C targets. Native target-LLVM
and MIR recovery are outside its acceptance boundary. Pthread cleanup has two
unique targets, executed three times including the paired var case. The whole
memory peak was not retained after the transient unit unloaded; no measured
whole-compiler memory-performance claim follows from its configured limit.

## v9 same-value classification integration (OPEN)

Frozen source: `/home/c/pgy-ci-p0-recovery-v9-root-20261014/source`.
Manifest: `.tmp/ci-p0-recovery-v9-root-20261014/`, AF main base.
After the existing exact inferred/constructor type equality, the two
classification queries consume the equal constructor-owned field type.
Bounds, field/binding identity and admission checks remain; this is not a
new storage grant or a change to the formal-effect lattice.

Root observed structural PASS6.067s/60, focus PASS129.296s/300 (peak1.9GiB),
and aggregate C/LLVM53/query13 PASS157.490s/300 (peak1.6GiB), all guards0.
Both actual C/LLVM classifier modes output exactly
`PROBE|classifier-valid|6|bounds2`, stderr0, with six fact mutations, bounds
denial and restoration. Original focus modes remain passing. Aggregate source
targets are analyzed only, never emitted or run. Independent review confirmed
the two classification consumers do not retain their type-name argument;
this observation is limited to their current synchronous contracts.

The earlier no-emission candidate on the unchanged MIR control no longer
refuses node67321, but refuses fresh node78237 at
`unproved_exclusive_sink_entry`/nominal aggregate (227.456s/300, guards0).
That is a moved first failure, not canonical acceptance or a remaining count.
Fresh mapping and denial facts must establish whether the earlier negative
effect was eliminated or merely transferred to another formal. The full v9
bootstrap subsequently FAIL181.323s/1800 at node78237; guards0. Fixpoint,
installed driver and published compiler CI remain OPEN.

## Current Map dependency integration boundary (v14, OPEN)

Origin/main1d9e00053 is preserved as the selected source parent. v12 actual
full unchanged bootstrap FAIL162.115s/1800, guards0, at node78850 nominal MIR
control (`unproved_exclusive_sink_entry`/nominal aggregate), before gen1.
That current syntax ID is not interchangeable with the earlier v9 ID.

Log-only A2 observation finds the dead-holder control's exact Path inout
formal26 lacks a matching callee group while CountPaths passes. EV already
owns its typed Int-array dependency; the growth consumer previously selected
only the String-array origin marker. A7 connects that dependency to existing
MarkRoot, conservatively emitting wildcard growth. Complete focus Current,
copy/use/exit and whole-body alias checks still own permission. The existing
field-read target selector now owns original callee/binding/direct-kind/ABI/
bounded-spine checks for both forwarding and root use; old duplication is
deleted. Same-responsibility measured caps: growth247/field-read384, both
below600; no new owner, source annotations or execution-budget increase.

v13 structural PASS5.826s/60; focus FAIL6.607s/300 at a duplicated local name
before probe compilation, guards0. A8 only renames that validation-loop
binding, and actual native HIR passes11.787s, stderr0/source/tool guards true.
Frozen v14 structural PASS6.117s/60, guards0; its full focus gate is running.
Do not claim behavior, bootstrap, cleanup automation or installed-driver
acceptance from a running gate or these source/type checks. The additional
nominal bodyful-forwarding producer falsifier is still being prepared; C's
separate scalar forwarding-current19 is not a substitute for that claim.
