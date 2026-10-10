# Reached collection member capture integration

Status: FOCUSED CANDIDATE VERIFIED; P1/DRV-2 and SoT closure OPEN.
Observed base: `main @ 85fff5aa339ae136ac0797c4a6a95a5285ada70e`, dirty
shared checkout. No official executable, Git publication or exact-candidate
Actions run is included. This audit is evidence/navigation, not a semantic
owner or a successor implementation rung.

## Objective and affected chain

Restore the reached member gate's unchanged source-size boundary and refuse
readonly nominal storage capture without rejecting actual owned values,
default/ref calls or fresh factories. Priority is stable identity, one
owner-directed decision, preserved first error and partial carrier lifetime,
then bounded code size. Do not replace missing inference with user copies,
additional user annotations, compatibility reads or weaker expectations.

Production chain: admitted body/resolved expression facts -> collection
verdict -> ordered member root/node traversal -> capture obligation and
terminal publication -> verdict readiness before `found` -> ordered
statement/call checks -> terminal restoration -> aggregate
exclusivity/release/element-borrow consumers. MIR/backend and installed-driver
acceptance are later obligations of this same P1 rung, not established here.

| Responsibility | Authoritative source in `src/self_hosted/semantic/` | Last reached consumer |
| --- | --- | --- |
| Ordered transition | `ast_collection_ownership_member_transition_owner.pgy` | Collection verdict |
| Result schema, closures and scratch retirement | `ast_collection_ownership_member_result_owner.pgy` | Verdict and three schema-only aggregate consumers |
| Restoration disposition | `ast_collection_ownership_member_restoration_owner.pgy` | Scan finalization after statement/call checks |
| Exact capture occurrence | `ast_collection_member_capture_site_owner.pgy` | Capture obligation |
| Place, stable binding and readonly origin | `ast_collection_member_capture_source_owner.pgy` | Capture obligation |
| Readonly propagation and first scalar witness | `ast_collection_member_capture_obligation_owner.pgy` | Result publication and ordered verdict |

Array-literal element and struct-literal field projections remain owned by
their existing topology owners. Their materializing readers consume the same
allocation-free edges; separate child interpretation is removed. Builtin
stores require canonical identity and the existing collection protocol's
store policy. ArraySet's stored value is Auxiliary, not its Value index lane.
SetHas and SetRemove are not stores. Exact call modes remain inout=1, own=2,
default=0 and ref=3; selected required joins/ranges fail closed.

Capture/source selection occurs during the existing traversal; it does not
add a second AST/program scan. Readonly provenance is separate from ordinary
read hazards, so an actual owned local move is not refused by blanket alias
propagation. Arbitrary call results are not presumed aliases of arguments.
Sequence/tuple source capture is recognized only at a destructure boundary.

Publication owns sixteen internal terminal transfers: five returned
carriers, ten scratch arrays and the nested-blocked map. It preserves the
three early partial publications, then checks capture readiness, generation,
read closure, escape closure and nested closure in that order. Ten scratch
arrays are retired at the existing terminal boundary. The added internal
`own` modes describe this real publication/retirement responsibility, not a
manual-cleanup or annotation obligation imposed on user source. No new
backing-copy policy is introduced; allocation and peak-memory improvement
were not measured for this split.

## Executed evidence

Native builder: `.tmp/ownership-cutover-ci-2026-10-09/native-rebuilt/pgy.exe`,
SHA256 `22eb1794d89dd426281f67abe0cb1001ea9cf558e8310bff1022550e7e3f9c5a`.
It builds importing observers from the current Pergyra sources, not an
installed self-host driver.

Initial shared gate (before the scalar/retirement follow-up):
`tests/self_hosted/parity/member_indexed_read_permission.sh`, exit 0,
260 seconds, `.tmp/self_hosted/member-indexed-read.xz7oo0/`.

- Sixteen original source observations per C/LLVM, including indexed-read,
  move, restoration and first-error controls. Main independently compares
  fourteen preserved complete outputs per backend with
  `.tmp/self_hosted/member-result-baseline.YUktYR/`; all are byte-identical.
  The other two baseline cases were unsafe acceptances. Native C/LLVM
  confirmed readonly capture refusal; their original negative expectations
  are retained and now pass.
- Thirty-five added source controls: twenty-one refusals and fourteen
  positives per backend. Refusals cover direct/transitive/indexed local
  capture, constructor stores and temporary/returned constructors, nominal
  clone, unused/length/return paths, array push/set/literals, own calls,
  struct literals, destructure and MapSet. Native negatives must fail at the
  semantic borrowed-ref boundary, not merely at a later unsupported ABI.
  Owned values, explicit field clone, default/ref calls, inout and fresh
  factories/literals remain positive controls.
- Nine complete publication sentinels per backend pin scalar error IDs,
  restoration arrays/sites and all carrier entries through early refusal,
  unready generation and partially completed closures.
- Three native compile-time terminal-transfer refusals per backend cover
  duplicate scratch transfer and post-transfer array/map reuse.
- Two selected formal-fact guards per backend start from a real admitted own
  call. Removing its selected formal row or setting mode 4 makes capture-site
  readiness false and both existing signature/artifact matches false. These
  are bounded corruptions, not a claim that capture admission independently
  validates every possible forged graph.

Main rechecks all eight manifests: native, runner, original sources, capture
sources, publication-refusal sources, transitive Pergyra imports, binaries and
observations. The gate pairs full normalized C/LLVM observations. Supplied
source cases are analyzed; native controls compile an empty Main. Neither is
evidence that their application payload was executed.

Pre-follow-up focused companion receipts also pass:

- `.tmp/self_hosted/aggregate-release-source.ARFKoF/`: six positives,
  sixteen falsifiers and generation/coverage guards per backend.
- `.tmp/self_hosted/collection-formal-key.xMpaoM/`: pinned effects/carriers/
  verdicts and two source-placement mutation refusals.
- `.tmp/self_hosted/collection-binding-move.R6FKZN/`: seventeen full
  observations and four source-placement mutation refusals.

The existing 180 counted-line transition cap is retained; the new bounded
owners have their own responsibility caps. Comments remain excluded by the
lexical counter. Schema-only consumers no longer import the transition for
its result type. Structural ratchets forbid duplicate schema/publication,
transition-local closure/retirement/restoration and reverse owner imports.
Behavior belongs to the executable gate, not those source ratchets.

The member Make target is a prerequisite of the inout-effect target; its
former manual-inventory entry is removed. Fresh reachability reports 939
scripts, 871 reachable and 68 manual, with zero undeclared, stale or dual
entries. Full structural inventory is **not green**: the independent WSL
edit-loop run exits 124 at 60 seconds. Fourteen checker units pass, but
`.tmp/member-result-survey-2026-10-09/component-final-wsl.log` has no full
inventory end receipt. Do not raise the budget merely to label it green.

## Scalar relevance and ordered retirement follow-up

The original complete 430-input C/LLVM census identifies seven discrepancies,
not an unbounded sequence of isolated first errors. Two readonly user String
functions and one readonly aggregate observation are already admitted by the
existing declared-formal effect owner; their source cases become positives,
with four explicit retaining counterparts. A TextBuilder formal's earlier
resource refusal is pinned instead of pretending its String consumer ran.
No builtin-name fallback is introduced.

For implicit scalar fields, capture-source relevance now consumes the body
environment issuer's same-graph leaf type before requiring nominal backing
identity. Two real Int returns are noncaptures; missing/Unknown and a nominal
value without backing fail closed. The selected-site executable imports this
unit rather than building an identical composition twice. The consolidated
member receipt `member-indexed-read.zpopkH` exits 0 in 252 s with its eight
manifests rechecked. The earlier expanded run `XiXfxd` times out at 300 s and
has no end receipt; it is not a PASS. The body issuer is a required premise:
this projection does not independently validate arbitrary forged scalar types.

Exact retirement moves from a terminal second full scanner to pending event/
definition facts consumed by the scan's existing occurrence/current stream.
Reads see only earlier admitted consumption. A generic argument refusal
returns before publication; initializer movement occurs after RHS reads and
before destination activation. The old scanner and its second current array
are deleted, with structural absence ratchets. The transfer owner remains
within its 180-line cap; the scan retains its 400-line cap. The scan-local
HashMap cannot escape, but its physical heap reclamation is not established.

The complete frozen current C-only census runs 434 unique inputs with zero
execution/stderr failures and no accept/refuse flips. Eight old borrow
expectations become exact move diagnoses, all independently justified:
repeated Metadata, same-generation loop Retire/read, an unconditional Retire
after a conditional exit, transferred child drop, branch own then Copy/read,
and own-formal drop then Copy/read. The two original move expectations remain
unchanged. Receipt: `.tmp/ordered-retirement-census-astra-20261010/`.

New units 125–128 cover pending read-only issuance, actual generic refusal
without publication, missing current, a valid other local's current definition
and zero syntax. The admitted control pins the exact retirement site. The
same-event post-publication query is API evidence, not a replacement for the
later-event source cases. Formal storage units now use existing EnvironmentSeed/
EnvLookup under storage readiness instead of all-Unknown placeholders. Unit
arrays are process-lived because the current native builder cannot prove
their explicit deep/scratch release; no cleanup or RSS claim follows.

The order ratchet pins source/site writes and unconditional guard/commit
placement, rejecting seventeen planted variants, including wrong site,
conditional return, unreachable commit, spaced rejected writes and issued-fact
overwrites. Independent review found four false passes in its twelve-case
predecessor; those are now permanent falsifiers. It is a lexical shrink-only
guard, not semantic
correctness evidence. Fresh paired execution and whole integration are pending;
all pre-retirement receipts above are scoped to their own input/import hashes.

## Exact remaining boundary

### Reached LLVM type projection repair

The importing identity observer's LLVM37 originally crashes in StringEquals:
MIR source-local lookup drops the sealed binding ID and selects the first
same-spelled `modes:Array<String>` row for the actual `Array<Int>` buffer.
The repaired native builder is
`14c03d48f450feb7fd67b98cca300a544f4de367c7c26255067eac81a7bc5c66`.
It joins exact local/formal/host-field IDs, rejects duplicate/empty/zero local
type rows, reuses the canonical local-ID API in collection facts and compares
available MIR/active-array element types before materialization. The old
private local-ID scan and the reached first-name type read are deleted.

Observed receipts: 224/224 MIR units; 4/4 native C/LLVM runtime comparisons at
`.tmp/pgy_backend_compare.QUeWEC/`; 23 identity units per backend at
`.tmp/retirement-consumer-unit-20261010/paired-new-owner/`, including LLVM37/50
and 125–128. The pair checks its source/import/binary manifests at exit; the
subsequent formal admission guard requires a new importing replay. Observer
compilation has 0 errors and 9 existing warnings per backend. Static lexical
identity and backend fail-closed gates pass. No new user modes, copies, fixture
variable rename or manual cleanup fixes the production crash.

Trust boundary: ordinary uses are sealed by the semantic checker's active
scope lookup. MIR's `(name, ID)` inventory does not independently authenticate
an arbitrarily rewritten same-name ID after semantic analysis. An initially
overstrong synthetic scope-crosswire test exposed this limitation and is
recorded in the work directive, rather than claimed as refused. Exact type
projection is not scope membership; type agreement is not same-type identity
authentication. The existing differently named crosswire refusal remains.
LLVM direct lambda parameter/local array indexing is already missing-fact
refusal; independent frozen-builder probes retain that OPEN route, while
existing typed lambda member access remains C/LLVM positive.

The new collection-source duplicate-formal negative uses an owned Holder's
field extraction. Borrowed-parameter rebinding is intentionally refused, and
an Array formal alias has no source row, so neither is a valid falsifier of
this exact source-type consumer. The selected fixture reaches MEMBER_MOVE's
formal-ID query and distinguishes one admitted formal from duplicate IDs.
This unit is metadata validation, not heap-release evidence.

The broader inout gate's subsequent run `collection-inout-effect.qhCTKx`
exits127 at C mutation9 after the source admissions and constructor units:
the malformed parameter-ID vector reaches the String borrow consumer after
the inventory has rejected it. A complete 0–15 census also observes mutation15
false in both backends. The real cross-surface ArrayElement edge is rejected
by the root-interval owner, but the invalid context still produces opaque
effect rows. The same formal-use owner now returns its existing empty refusal
before either invalid inventory/context is consumed. Mutations9/15 require
empty effect, owned-push and text-borrow maps plus invalid inventory/context;
test expectations are strengthened, not waived. The existing 437 counted-line
cap remains unchanged. The fresh complete identity census is now green:
`.tmp/formal-admission-20261010/` executes all 127 identity units selected by
the permanent gate per backend, 254/254 total, with no changed input mapping.
This deliberately excludes modes79/80, which the permanent gate does not
select. Mutations9/15 are normal rc0/true with empty refused carriers. Both
observer compilations have 0 errors and 9 existing warnings. Every input,
Pergyra import, observer source, native executable and emitted executable is
bound by endpoint manifests; main observes the complete rc0 receipt and
independently checks both 127-row result sets and executable hashes. Native
builder identity is unchanged. Formal owner SHA256 is
`5a4ed37d601ad7892598195125af040fb2aa0d150c29f65900a26c703a68bec2`.
Fresh carrier replay also passes at
`.tmp/self_hosted/collection-formal-key.Hz0R0q/`: 30 complete C/LLVM carriers,
pinned effects/verdicts, four malformed-fact refusals and the two structural
hoist negatives. Unready/truncated order produces flags false:false:false,
zero generation/extents and all six map sizes0. Valid controls and admitted
opaque effects remain unchanged. Full inout and member replay remain pending;
these receipts are not physical-cleanup, full-source-admission or
installed-driver claims.

The separate 2531-file full-source census remains cost-blocked at its
unchanged 3 GiB cap. The last diagnostic allocation location is not proof of
a dominant operation. This member split is not a third allocation
optimization and supplies no whole-input speedup, allocation or RSS claim.
Sparse readonly membership remains linear; its compiler-scale cost is
unmeasured. Generic held types and callback/opaque-result provenance retain
their upstream admission obligations; this is not complete view-liveness or
nominal lifetime inference.

Exact base-HEAD CI run `37883531407` remains completed/failure:
`aggregate_release_plan_unproved`, Array<String>, syntax 214862 in the
installer's source admission. Its failing job is `113673946389`; five
dependent jobs are skipped, not green. Parser-only mapping identifies
`SemanticAstExpressionFunctionTableFactsRelease(function_tables)` in
`DriverRung2MirProjectionRelease`, but does not identify the failed plan
predicate or enumerate all demands. Installed-driver replacement, removal of
the real C bypass, complete P1, registry closure and candidate CI remain OPEN.

Unrelated shared graph/proof/document work is preserved. `gmon.out` remains
outside commit inputs. No AGENTS, branch, official binary, commit or push was
changed in this slice.
