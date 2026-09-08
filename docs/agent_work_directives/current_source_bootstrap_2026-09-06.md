# Current-source integrated bootstrap

Status: CALL-SPINE CANDIDATE BUILT / FOCUSED INTEGRATION — updated 2026-09-07.
Current seed D56862B2 / manifest 657A0A7A and native driver BC77A185 build.
Projection/readonly gates and bounded bootstrap pass (`0b063c`, `457def`).
Session 1299, the old public LLVM query leg, is terminal RED at 300 seconds,
exit 124, child code 143, no executable (`510f1a`, `b91ac6`). No compiler job
remains; source freeze is released. Read the last evidence card before edits.
The next paragraph is the pre-projection baseline: the C7920411 /
FFFBF13F seed/native pair builds, readonly and summary-budget controls pass,
and bounded source/MIR/common-consumer bootstrap passes (`38b74c`). The old
73945 refusal remains RED history. Owned inventory regeneration, keyword/
protocol and 2,240 source hashes pass (`91be97`). Full phase session `16470`
is terminal RED, exit 1 (`ab0e85`), at its 1,800-second boundary. The Pergyra
producer completed its full MIR; the native oracle stopped in generic analysis
without an artifact or stderr diagnostic. No producer comparison or gen2/gen3
result exists. No compiler job remains; source freeze is released. Preserve
the completed MIR and failed logs; do not restart the old directory.
Base: `5b97f2e10ffa7ecf9cfe932829a83ffffaa3ba12`, unpublished local changes,
zero staged paths. Primary alone; no delegated edit lease.

## Objective card

- Objective: the current Pergyra-built compiler must produce its complete
  driver source MIR, match the independent native-built driver's MIR, and
  consume that one Pergyra-produced artifact through gen2/gen3 into identical C.
  A receipt must bind that equality to the current graph, seed and binary.
- Priority: real self-host execution, single Pergyra-owned MIR input, exact
  identity/receipt, missing-fact refusal, then reuse of already verified builds.
- Production entrypoint: `driver_bootstrap_main.pgy`, named by the existing
  nine-row codegen-bootstrap manifest. Seed C681F4BF and native oracle 2EEA2BA6
  were both built from the current source graph; no semantic source changed
  after their builds. Source-input manifest is 20854311 (2,240 matching inputs).
- Fact owner: the existing verified MIR producer owns the complete driver MIR.
  Native output is comparison evidence only. The existing artifact comparator
  and fixed-point receipt owner own their verification formats.
- Last consumer: gen3's MIR-to-C path consumes the same Pergyra-produced MIR
  that gen2 used; it must not regenerate or substitute native MIR.
- Forbidden fallback: native MIR as production input, stale-graph receipt,
  different/partial driver source, skipping producer equality, weakening the
  original comparator, claiming a bounded example as the full fixed point,
  or source changes while the run is live.
- Verification: execute the original full phase of `driver_bootstrap.sh`,
  including `--pressure-owned-full-fixpoint`, producer comparison, generated
  gen2 bounded preflight, gen2/gen3 C equality, then the existing receipt writer
  and validator. Reuse the two observed driver builds and prior bounded sample
  artifacts; call this reused-build/resumed evidence, not an uninterrupted
  invocation of the full script. Use the existing test C profile, not a release
  performance claim.
- Falsifying case: the complete manifest-selected driver source; missing facts,
  MIR disagreement, gen2 runtime rejection, unequal C or a stale receipt each
  prevents closure. Preserve any failed stage and continue only from verified
  evidence, never by changing expected output or rerunning an expired observer.
- Budget: one 1,800-second integration shard, static checks 60 seconds. Do not
  increase the timeout, memory allowance, worker count or artifact copies to
  hide repeated work. The assignment enum-readiness stack is bounded evidence,
  not authorization for a concurrent optimization lane.

## Scope and handoff

Primary alone may prepare the isolated runner/evidence and update navigation
documents. Keep compiler source frozen throughout the run. No shared `bin/`
installation, staging, commit, push, remote CI dispatch or unrelated cleanup.
The prior physical-record directive's original C/LLVM assignment gate passed
as session 92648 (`2f66c7`), with five outputs and C seven/LLVM six refusals.
This opens the bootstrap verification rung, not a new semantic implementation
track or an increase to SoT/substitution percentages.
Isolated runner: `.tmp/self_hosted/intent_participant_dir_carriage_20260906/full_fixed_point.sh`.
Its log is `full-fixed-point.log` in the same directory. Full-stage artifacts
are under `.tmp/self_hosted/driver/intent_participant_bootstrap_20260906/`.
The original full MIR phase is reached after binary/receipt/source-hash checks
and the owned manifest (`d6ab1e`). Session 73945 then completes semantic body
analysis and refuses a readonly Array<Int> -> default/value boundary
(`4c51f8`, `7117cf`). No full MIR, native producer comparison, gen2/gen3 output
or fixed-point receipt exists. The source freeze is released; no bootstrap
process remains live. All failed stage logs remain preserved.

## Reached whole-driver read-boundary diagnosis

- Objective: identify the exact consumer that turns a readonly Array<Int>
  projection into a value input in the complete driver source. Keep the existing
  semantic boundary rule and the original full-source falsifier.
- Existing fact owner: the named-value boundary verdict joins admitted
  parameter mode/type and expression-place facts. Its diagnostic names the type
  and boundary but does not expose the callee, so do not guess the consumer.
- Allowed next evidence: one explicit native-front MIR dump of the same
  complete driver source, then read-only call/parameter/member-fact inspection.
  Native MIR is diagnostic evidence only; never feed it into bootstrap as the
  Pergyra-produced full MIR. Preserve stable IDs and label incomplete lookup.
- No producer rule relaxation, by-value compatibility fallback, source rewrite
  before consumer inspection, new ResourceCarriage abstraction, shared install
  or another full-bootstrap run. Scope any fix to the reached owner and its
  actual read/write/transfer responsibility after a reduced executable case.
- Primary alone, no parallel lease. Native diagnostic budget 300 seconds;
  static inspection 60 seconds and reduced parity 300 seconds. The original
  assignment C/LLVM PASS remains valid for C681F4BF.

## Reached read-query carriage objective

Native-front diagnosis completed with zero errors/four existing warnings
(`96e02b`). The 1,350,806,230-byte diagnostic MIR has SHA-256
`7B170C5994A857BE884FCADDB16A5D3AB3E92D93131B67403B9044DCF20AAF70`.
The streaming inspector was checked against the six previously known Intent
edges before reading this file one routine at a time. It finds 136 readonly
sequence-to-value edges (62 Int, 74 String) into 37 functions in 25 files, all
joined by exact source syntax ID (`90f756`). This is bounded direct-call/formal/
member evidence, not full alias analysis; 5,156 other edges remain unresolved
by this diagnostic. The dump is not a self-host bootstrap input.

The three-line generic call-return-owner import reduction reproduces the
public `borrow_boundary_escape`, default Array<Int>, with no C artifact. Its
public launcher log records exit 1; the expired observer's terminal result is
not recovered. Explicit native emission succeeds with one existing unreachable
statement warning (`82880d`, `311483`). This reduces source admission, not query
runtime behavior.

- Objective: preserve the readonly sequence boundary through the witnessed
  production read-query family, including its actual transitive read callees.
  The full current-source MIR producer remains the sole integration target.
- Priority: honest read/ownership modes, existing semantic identity, unchanged
  validation and results, executable controls, then patch size.
- Fact owners: existing generic-call, enum/local identity, declaration, DIR,
  CFG partition/local, callable, runtime materialization and emission owners.
  No new family, registry authority or query engine is introduced.
- Last consumers: scalar lookup/validation or fresh result construction.
  The callable receiver join validates routine columns but returns its own
  semantic-produced rows; it does not retain the borrowed routine columns.
  Array-string boundary and leaf-identity constructors return scalar facts.
  CFG distances owns its newly allocated output, not the borrowed edges.
- Edit scope: only the inspected read-only array parameters in these owners
  and their transitive read-query functions, plus a focused executable fixture,
  its source-residue gate, owner documentation and this handoff. Mutation,
  construction and transfer functions keep their owned/inout modes. Inspect
  every transitive sequence use before including it.
- Forbidden fallback: copied input arrays to satisfy value formals, weakening
  readonly admission, by-value overloads, identity reconstruction, changed
  expected semantics, rebuilding a whole driver after each isolated query.
- Verification: execute real generic/CFG/String read queries on owned and
  readonly-projected inputs, check missing/duplicate/range controls and input
  preservation across native/public C/LLVM. Retain the existing mutation and
  escape-refusal gate. Then rebuild in an isolated directory, recheck source
  hashes and perform the original full-source bootstrap phase. The original
  C681F4BF assignment PASS is historical evidence after source changes, not a
  pass for the new candidate.
- Falsifier: the existing full-driver default Array<Int> refusal and any newly
  exposed transitive value boundary, mutation escape, differing result or lost
  identity. A passing small query does not close the complete driver rung.
- Primary alone; static 60 seconds, focused 300 seconds, integration 1,800
  seconds. No shared installation, staged/committed changes or remote writes.

## Reached native summary accounting boundary

The read-query migration changes only 93 parameter modes across 51 inspected
functions (27 files); preimages verify that bodies and other source text are
unchanged (`7aaba1`). The isolated Pergyra-seed build passes (`c189f0`), driver
SHA `C7920411E2BEAE230C65A47D179CF5A84F8FA7EB84B758394399FC132FBC1CB7`,
source-input manifest `435A49A715E57274A1C8838B9F3B60688719CA83C99381EF2737846F7ACA8D0A`.
Its readonly regression passes eleven values and all 40 source/MIR refusals
(`e26813`). The new query fixture has 23 matching results in native C/LLVM and
public C on C681F4BF; public LLVM ended without an artifact at the aggregate
300-second boundary, code 143 (`779212`, `59a3d7`). This is not a complete query
matrix or an independent LLVM semantic refusal.

The native full-source comparison build is RED (`07ec5d`). Its first failure is
the existing 4,096-unit function-parameter summary work budget, followed by 246
secondary diagnostics. A reduced nonrecursive control proves the accounting
scope problem: 4,096 independent demands pass, while 4,097 fail with zero
recursion hits and only 4,096 body evaluations (`bee6b4`, `5918bd`). Initial
`--check` invocations were invalid CLI uses, not semantic evidence; the observed
reduction uses the existing native `--hir` entrypoint.

- Objective: charge one independent demand closure to its own bounded solver
  episode; previously completed unrelated demands must not consume that
  episode's recursive work allowance.
- Priority: preserve summary identity and completed-entry reuse, bounded
  convergence and fail-closed behavior, then compiler-scale acceptance.
- Owner: `src/semantic/function_param_flow_summary.c`, with the existing
  store and active-start/solving state. Access/escape consumers still use the
  same demanded summary; snapshot ownership is unchanged.
- Last consumer: the existing resource/escape checks and summary snapshot.
- Edit scope: the work-counter episode boundary in that owner and its focused
  smoke gate. No new query engine, cache, SCC implementation, larger constant,
  discarded failure, wider timeout, skipped native comparison or semantic
  fallback. Body-evaluation telemetry stays cumulative.
- Falsifiers: unrelated 4,097 pure demands must pass; a single oversized demand
  closure must still fail explicitly at the same work cap; recursive propagation
  and real escape negatives must retain their existing results. Then the same
  complete driver source must pass the independent native build.
- Primary alone. Preserve old A0F2FCDB and all failed logs; rebuild only to an
  isolated native binary. Static 60 seconds, focused 300 seconds; full-source
  bootstrap remains open, not resumed while its native prerequisite is RED.

The native prerequisite subsequently passes as FFFBF13F (`3df9a8`). New-pair
bounded bootstrap also passes. Full session 16470 produces the complete
286,073,435-byte MIR, SHA-256
`6C7993EA0DBD03BE924603D6DB097932214E9C940B0DAE691E68857582A80CE3`.
It terminates before the independent producer completes (`ab0e85`, `b07a5c`).
A read-only stack identifies named-boundary's call-spine suffix scan (`96894e`)
and source confirms one arena suffix scan per reached call. This is repeated
work evidence, not a measured runtime share. The traced full size rerun passes
unchanged (`979273`); the earlier unexplained exit 1 remains preserved.

## Reached call-spine work objective

- Objective: the named-value boundary must recover each call's exact argument
  spine without scanning the rest of the complete expression arena per call.
  Full current-source bootstrap remains the integration falsifier.
- Priority: preserve node identity, argument ordering and fail-closed verdicts;
  remove repeated owned work; then measure the same input. No timeout increase.
- Fact owner: parser/HIR owns immutable topology; the existing semantic
  `ast_expression_graph_call_view_owner.pgy` owns call-spine projection.
  A temporary node-indexed root view is derived once for the admitted graph
  during named-boundary checking, never serialized or retained as authority.
- Structural allowance: the same call-view owner gains 39 lines for that
  projection (189 total). Its old 160-line inventory pin is updated to 200,
  not the shared semantic-owner policy. A cosmetic extra owner is not warranted;
  preserve the original ordered-view and readonly-boundary gates as well as
  the new scan-residue gate. The first new-marker-only check missed this pin;
  the complete affected blocks expose it (`b11645`) before the explicit update.
- Last consumer: `ast_named_value_boundary_verdict_owner.pgy`, including its
  fresh-constructor check. Existing `SemanticCallSpineViewFromGraph` still owns
  argument and generic-actual ordering. Other analysis consumers are out of scope.
- Edit scope: these two existing owners, one bounded projection regression,
  its source-residue gate/manual entrypoint and navigation evidence. Primary
  alone; no parallel implementation, shared install, commit/push or remote write.
- Forbidden fallback: a new query engine/cache, spelling-based reconstruction,
  copying input graphs, missing-index fallback to an arena scan, changed
  argument order, skipping readonly/escape checks or an inferred fixed point.
- Verification: compare projected call roots and every view field against the
  old scan on nested, generic, zero-argument and branching-spine graphs; retain
  malformed projection refusal. Count/measure bounded increasing inputs, run
  native/public C/LLVM, then current readonly refusals and source-matched builds.
- Falsifier: any changed call ID, callee ID, argument order or generic actual;
  accepting a missing required projection; quadratic work in this consumer;
  or full MIR disagreement. Static 60 seconds, focused 300 seconds, integration
  1,800 seconds. Preserve the pre-change candidate and full MIR as controls.

## Reached logical-record query — next evidence, not an implemented repair

The call-spine change builds as D56862B2 / manifest 657A0A7A; independent native
BC77A185, current native/public C/LLVM views, readonly eleven values/40 refusals
and bounded bootstrap pass. All 2,240 source hashes and current size/keyword/
protocol gates pass. No full current-source fixed point or speedup is claimed.

The formerly missing public LLVM query still exceeds 300 seconds. Its useful
read-only stack (`1b29e1`, explicitly detached) reaches
LogicalRecordFactReady -> LogicalRecordMemberMarkerReady -> AppendExpression
inside GraphPlanFromAdmitted. The first attach sampled only the debugger's
break-in thread; neither attach measures a runtime share or a clean benchmark.

- Objective: isolate the repeated whole-record validation reached by that
  exact query without weakening record identity, digest, ABI or malformed-fact
  refusal. The complete bootstrap remains the eventual integration gate.
- Owners: `direct_mir_scalar_program_logical_record_fact_owner.pgy` owns fact
  validation; `direct_mir_scalar_program_logical_record_expression_owner.pgy`
  owns candidate classification; the graph-plan expression append is the last
  reached consumer. No new fact family or cache authority.
- Observation: both member-marker/member queries call full FactReady before
  checking node kind. FactReady hashes all columns and repeats declaration/
  field duplicate and dependency scans. The digest and these checks remain
  necessary at their admission boundary; a stack does not prove they may be
  deleted or replaced by `valid` alone.
- Next falsifier: a bounded real-owner probe distinguishes definite nonmembers
  from actual record members and malformed matching facts. First determine
  whether safe node-shape guards can reject noncandidates before table work;
  preserve bounds/column safety and exact negative results. If an admission
  receipt is needed, identify its existing owner and lifetime before designing
  any changed carriage. No production edit has begun for this record query.
- Primary alone. Do not repeat the public LLVM leg/full bootstrap, grow inputs,
  raise timeouts or memory, or skip validation before that reduction. Static
  60 seconds, focused 300 seconds; no shared install, commit/push or remote write.

### Record-query shape-guard implementation card

- Previous goal turn: progress, with an executable call-spine replacement,
  source-matched builds, regression results and a newly reached LLVM frontier.
  Current source hashes are rechecked (`078e26`); no prior job is live.
- Objective: avoid whole-table validation for graph nodes that cannot be a
  record member or its marker. Preserve exact query outcomes and all actual
  record identity/digest/ABI checks; then retry the same LLVM workload.
- Edit owner: only `direct_mir_scalar_program_logical_record_expression_owner.pgy`
  for the two witnessed member queries. The fact/digest/ABI owners stay unchanged.
  The last consumer remains the admitted graph-plan expression append.
- Allowed change: bounds-checked node-kind guards, and a marker's next-node
  kind test, may run before full record validation. Keep right-child and field
  reads after the existing validity boundary; do not create unchecked graph
  reads while reordering. Do not infer a record from a spelling or `valid` bit.
- Falsifier: record-member IDs/ordinals/types and invalid/missing/empty cases
  must retain exact results. A matching shape with a stale digest, duplicate
  identity or invalid ABI absence must still fail. A bounded real-owner probe
  runs before/after, with fixed field counts and query counts; its timings are
  not a whole-compiler benchmark. Existing public/direct record controls and
  the same 23-result LLVM leg remain required after a source-matched build.
- Primary alone, separate hash-preserved preimage and test outputs. No cache,
  admission deletion, extra owner, shared install or publication. Budgets stay
  static 60 seconds, focused 300 seconds, integration 1,800 seconds.

### Record-query execution checkpoint and next falsifier

The shape-only edit is implemented in the two named queries. Before/after
native C retains all seventeen results; the actual preimage is rejected by the
source-order ratchet. Original expression-owner checks and new queued caps
pass (`ac3ab8`). Existing direct logical-record C/LLVM ten-output/ten-refusal
gate also passes (`e8abf7`); its original commands run in a fresh exact evidence
directory without deleting any earlier results.

Current isolated driver is 723F5707 / source manifest 6C95B095, independent
native 234AAB7E. Both builds, all 2,240 source hashes, keyword/protocol and
bounded bootstrap pass. New owner-query native C/LLVM and public C retain the
seventeen results. Its public LLVM leg reaches the aggregate 300-second gate
boundary, exit 124 / child 143 (`da97d6`), not a semantic rejection or four-way
PASS. Keep `.tmp/self_hosted/record-shape-query.jaMNpM/` and its logs. The
current full size run separately reaches 60 seconds (`aa8837`); do not claim
the previous source's complete size PASS as this run's result.

Read-only stack `3543a2`, exact driver identity checked and explicitly detached,
reaches PayloadFreeEnumFactReady -> VariantFromGraph -> MarkerReady ->
AppendExpression. Source confirms the existing marker scans all later graph
nodes for each leaf, and VariantFromGraph validates the whole enum fact before
checking the scanned node's kind. This is a repeated-operation observation,
not its runtime share and not permission to skip enum admission. The enum
owners are unedited. Do not expand a general cache/index/query-engine lane.

The original twenty-three-result public LLVM leg on the changed driver also
ends at its same standalone 300-second boundary, exit 124 / child 143, with no
executable (`70964f`, `bd2db1`). Full-source bootstrap remains open. Source
freeze is released, all jobs terminal; no repeated run, shared install,
commit/push or SoT percentage change.

### Constructor field query — next active evidence card, no implementation yet

- Objective: isolate repeated record-table validation in the original
  twenty-three-result query's actual constructor path. The record shape guard
  has bounded evidence, but did not close this original LLVM workload.
- Observation: identity-checked, explicitly detached stack `ae97e2` reaches
  FactDigest -> FactReady -> Row -> FieldType -> ConstructorFromGraph. Source
  shows the constructor already obtains a validated record row, then each
  argument's FieldType query repeats whole-table validation/name lookup.
  This is not a measured runtime share or permission to remove admission.
- Owners: existing logical-record fact/row validation and expression-owner
  constructor query. Last consumer is the admitted graph-plan expression
  append. Preserve exact record row, field order/type, source/callee identity,
  partial/complete constructor meaning and ABI checks before performance.
- Next falsifier: establish how long the already validated row is legitimate
  within that one query; compare bounded constructor arities against the old
  query, including wrong types/counts, missing record, stale digest, duplicate
  identity and malformed ABI. Any derived view must use that same owner and
  end at the existing last consumer, never become parallel semantic authority.
- Forbidden fallback: `valid` alone, skipped digest checks, spelling-inferred
  fields/ABI, a generic cache/query engine, larger budgets or silent acceptance.
  Keep missing-fact refusal; no constructor repair is implemented or verified.
- Primary alone. The enum suffix-scan observation is secondary, not an active
  parallel queue. Do not repeat the failed tests or full bootstrap before a
  demonstrated reduction. Static 60 seconds, focused 300 seconds, integration
  1,800 seconds; no shared install, publication or cleanup.
