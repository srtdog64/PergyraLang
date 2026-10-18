# Indexed String: checked ReadFile result independence

Status: post-gap V10 capture allocation-bounds candidate APPROVED for its
bounded scope by independent frozen review. Current results
live in [gates](../truth/state/gates.md), [blocker](../truth/state/blocker.md),
and the [V10 review verdict](../truth/evidence/2026-10-18_v10_independent_review.md).
V1–V9 exports remain immutable HOLD. Bounded native checkpoint
`5be40a9c3` is local and not published; canonical and CI remain RED.
Base: main `8404061d7`; native
`47ecd2f7f733aae04fb3ca98f602074d68b16dcc4bd47ebad05ea0c232081f4e`.
Navigation only. Publication, typed flow claims and immutable collection source
evidence retain their existing authority; no new semantic family or language word.

## V10 reached capture allocation bounds (implemented candidate)

- Objective/priority: reject malformed certified capture counts before any
  inventory traversal or plan-output change. Preserve the existing semantic
  identity and allocation owner, then exact restoration and all normal plans.
  This repairs a native prerequisite, not self-host substitution or D1 copy.
- Whole reached chain: semantic context grows boundary/row arrays; ownership
  moves to SemanticResult, whose boundary capacity is currently lost; MIR
  imports them into calloc arrays without retaining allocation sizes; the AIR
  binding fingerprint and final C/LLVM/parallel plans consume their active
  counts. Existing MIR accessors/validator and both owners' teardown also use
  active counts. The only capture allocation/resize owners are semantic
  parallel_capture_facts.c and MIR mir_parallel_capture_facts.c. No separate
  copy/grow owner or wire-format change is needed.
- Intended owner/lifetime: retain actual allocation capacities, never infer
  them from a possibly changed count. SemanticResult receives the context's
  boundary capacity with the existing move. Semantic realloc initializes its
  new tail; MIR calloc records both capacities after successful allocation.
  Release traverses initialized allocation slots, independent of active counts,
  including partial import failure. Producer reset/add/seal, accessors, semantic
  import validation, MIR validation and AIR hashing reject count > capacity or
  missing active storage before dereference. Capacities are private allocation
  metadata, not added semantic facts, new grants, or protection against arbitrary
  simultaneous fabrication of native pointers and their allocation metadata.
- Last consumers: existing current-binding query and the two final planners.
  Their existing guard order stays before plan mutation/allocation. Hash v2
  keeps the same valid semantic payload; storage metadata gates traversal, not
  another census, AST walk, fallback, certificate reissue or serialized field.
- Dependency/falsifiers: root first compiles two separate ASan+UBSan native
  probes against unchanged V9 source, each with the same genuine AIR/MIR/source
  snapshot. Raise boundary count or row count above its real allocation, query
  current-binding first, retain the independent sanitizer failures. Original
  and exact-restore controls must pass. No Pergyra program executes. Only then
  release production edits and add both upward mutations to the existing14.
- Independent scopes: A owns semantic.h/semantic.c/parallel_capture_facts.c/.h,
  type_checker_context_helpers.c and MIR program/types/capture-facts storage,
  import, accessor, validator and release. B owns only air_evidence_mir.c's
  pre-traversal capacity checks. C owns the existing AIR post-certificate tests
  and new MIR semantic-import/storage lifecycle negative controls. No file
  overlap; root owns docs, scripts, all commands/Git and integration. These
  are parts of one reached rung, not independent implementation tracks.
- Shared acceptance: static60; phase/MIR/AIR/RIR/DIR; same two prefix probes
  now reject, preserve source/AIR and outputs, and restore all three plans;
  separate SAN with leak detection OFF; complete strict C/LLVM, existing
  semantic/transpile and portable replay. Preserve original7 semantic and7
  missing binding/storage faults. Export immutable V10, independent review,
  bounded local commit, then reopen D1's full copy/cleanup chain. Full unchanged
  canonical, real bootstrap/gen2==gen3, installed C/LLVM and published-SHA CI
  remain the endpoint; no known-red push and no SoT CLOSED claim.

Implementation and current observed results are recorded in the immutable
[post-gap V10 execution receipt](../truth/evidence/2026-10-18_v10_capture_allocation_bounds_post_gap.md).
This directive remains the objective and chain map; current acceptance state is
owned by [truth/state](../truth/state/blocker.md).

## Previous V8 executable receipts

- mch2KB: static/build, phase13422/0, MIR681/0; PADcM4: AIR160/0, RIR44/0,
  DIR15/0. h3t1ov: genuine source-less projection probe PASS.
- LcUc9a: complete strict C/LLVM PASS; the rejected slot-wrapper fixture's
  preexisting secondary diagnostic is retained, not presented as a clean log.
- dwtv6t: semantic3230/0, transpile1014/0, AIR160/0, currency6, Map24 positive
  and58 analysis-only negative controls, HIR/RIR/MIR/AIR flow PASS. Separate
  ASan+UBSan phase13422/MIR681 PASS; 81XOu5 AIR160/RIR44/DIR15 PASS, leaks OFF.
- 9G4H0w: independent fresh relative-root replay of all5 units,20 runtime inputs,
  both missing-input refusals and exact restoration. Later transpile-only edits
  do not change those5 binaries. tJysoX: all8 original callable positives;
  XNWlJG: empty and parser-normalized implicit Main positives.
- x7Yw6m: unchanged fixed/current canonical still RED18435 errors+16 warnings,
  MIR0. Fixed31.87s/1729900KiB, current31.35s/1728268KiB. ObserverzjmlFq first
  prior=0 at diagnostic_owner.pgy573:39 ArrayPush(codes,found[j]),37.82s and
  1729560KiB. Concurrent validation is not a performance comparison; refusal
  occurs before HIR allocation, not a compiler-scale census cost measurement.
- Final raw source/doc verification and immutable V8 review export are next.
  No bootstrap, installed-driver, required CI or whole-language closure claim.

## V9 reached certified MIR inventory binding

- Objective/priority: the two final MIR-taking plans must consume the inventory
  that AIR actually admitted, not merely a MIR with the same source AST. Keep
  existing identity and one binding owner, fail-closed before output/allocation,
  then preserve genuine source-less partial tests without a native fallback.
- Whole reached chain: AIR MIR collector computes the existing binding token,
  copies evidence and seals that token into its certificate. The two planners
  currently check source identity only. Their later MIR reads are the recorded
  surface-usage flags and the parallel capture boundary/row inventory, including
  source ID, sealed/task count, row name/kind/writer task and validator storage.
  Existing routine/block/instruction/flow token fields remain part of the seal.
- Existing owner: air_evidence_mir.c computes one versioned token and exposes a
  pure current-binding query (source current, started/bound, nonzero recomputed
  token equal to the issued token). Missing/count-without-storage facts return0;
  no AST scan, reissuance, second census/cache, whole-MIR safety claim or public
  language change. Hashes are mutation guards, not cryptographic authority.
- Last consumers: C/LLVM observability projection and parallel capture planner.
  Both query before consuming that MIR. Source-less synthetic tests must collect
  their actual partial MIR before issuing a certificate; empty stack AIR is not
  proof of a different MIR. Keep standalone accessor mocks explicitly partial.
- Edit scopes: A owns air_evidence_mir.c/air.h; B owns the two final planner
  files; C owns post-certificate AIR test mutations and registration only.
  Root owns test producer integration/mocks/docs and all execution/Git. Production
  edits wait for the unchanged60f prefix to reproduce all four reported cases.
- Falsifiers after certification, same AIR/source/snapshot: already-tokenized
  instruction identity; observability flag; capture count withdrawal; valid
  capture payload mutation. Both planners must refuse, preserve inputs and accept
  exact restoration. Also incomplete storage/shape and source-less partial
  positives, all V8 foreign-source negatives and backend-negative tests.
- Shared integration gate: static60, identified phase/MIR/AIR/RIR/DIR, full
  strict C/LLVM and expanded tests, separate SAN and fresh portable replay, then
  a new immutable V9 review export. Never overwrite V8 or call this CI green.
  D1 production remains unopened while this reached prerequisite is HOLD.

GGkxq8 on unchanged60f reproduces all7 valid post-certificate mutations:
original and exact restores pass, C/LLVM/parallel all accept each mutation,
source/AIR unchanged; expected AIR160/1. V9 cH3bsb builds native71f5662c and
passes phase13422/MIR681. YevXkb's complete AIR set is144/17: the new14 binding
faults all deny with unchanged outputs/inputs and exact restore; old manual MIR
fixtures omitted capacity fields and now stop at the new shape guard. All manual
MIR allocation sites are bounded to test_air.c and cleanup_c_1/_c_2,
observability_pin_g_2, mir_terminator_h headers. Set their real array capacities,
including negative controls so those still reach the intended failing invariant;
do not add a production cap0 exemption. The deliberately missing routine/block
storage cases now assert preflight refusal before collector-state mutation,
instead of the previous already-started state. A owns test_air.c, B the two
cleanup headers and pin header, C the terminator header; prior C binding tests
are frozen. Root owns partial producer/mocks. Same integration gate, no new rung.

V9 final rs340M passes static/build, phase13422/MIR681; EQEqJn passes
AIR161/RIR44/DIR15, including every old negative and the14 binding faults.
LxVon5 strict C/LLVM and Abb9Yf semantic3230/transpile1014/AIR161, currency6,
Map24+58 and all4 flow gates pass. Separate SAN phase13422/MIR681 (Abb9Yf)
and AIR161/RIR44/DIR15 (sCo1lo) pass, leaks OFF. 9Bra9H rebuilds/replays all5
relative-root units with20 inputs and both missing-input refusals/restores.
EhWqnU's standalone empty-MIR row/ABI probe is a test double, not native proof.
Native source-less transpile fragments collect their actual MIR before issuing
plans. V9 fixed/current canonical YSfo5x is still18435 errors+16 warnings/MIR0,
33.64s/1730224KiB and31.75s/1728480KiB. Observert9AMcV first prior0 is the same
573:39 indexed String push,35.84s/1729804KiB. Final source/docs/export/review are
pending; no bootstrap/driver/CI or whole-language closure claim.

## V8 original DIR program identity

- Objective/priority: prevent a foreign DIR with the same numeric root ID from
  joining native HIR/RIR/MIR. Preserve original identity, one issuer, fail-closed
  admission and complete consumers before patch size. V7 remains immutable HOLD.
- Observed prefix: hhxuI0 on unchanged nativec45 records valid A/B baselines.
  Main/empty and empty/empty cross-DIR inputs pass synthesis, DAG collection and
  verification with unchanged metadata; exact restores pass. The foreign-intent
  control already refuses. AIR158/1 is the expected new negative-test failure.
  Independent MIR-entry prefix kt4oNI on that same unchangedc45 accepts all three
  foreign DIR pairs (including intent), with both original baselines/restores
  passing. MIR642/3 is the expected new negative-test failure.
- Whole chain: dir_lower_internal borrows the original AST_PROGRAM but only
  publishes its numeric stable ID. That ID restarts at1 for each parse. The
  graph anchor hashes the ID and row counts, so neither is source authority.
  HIR->DIR lowering, DIR validation, RIR/DIR validation, AIR synthesis/census
  and native MIR lowering currently admit or compare only that numeric identity.
  Internal DIR row ID checks remain consistency checks after source admission.
- Fact owner/lifetime: existing DIR issuer in dir.c seals one private origin
  witness {exact DIR object, original AST root pointer, nonzero original ID} at
  successful publication. A borrowed public program_root projection is checked
  against that immutable origin; queries do not issue, replace or scan anything.
  DIR owns only the witness, releases it once, and never frees the AST. Existing
  driver teardown keeps the AST alive beyond DIR/AIR/MIR's last source consumer.
- Required consumers: HIR-assisted DIR lowering checks the existing HIR census
  with that exact AST; DIR validates its own seal; RIR/DIR checks RIR.program_root;
  AIR joins native HIR/RIR to the sealed original before projection and retains
  the callable/IO bijections; MIR joins SemanticResult.annotated_ast to DIR after
  its existing semantic/HIR census admission. Preserve source-less manual AIR
  fragments as their existing partial contract, never as native admission.
- Forbidden: numeric/hash-only origin, mutable-pointer-only authority, current-
  time reissuance, AST rescans, new owner/TU/census, blanket HIR/MIR provenance
  expansion, per-input exceptions, relaxed safety or source ceremony.
- Independent edit scopes: B owns dir.h/dir.c/dir_validate.c; A owns air.c,
  air_evidence_rir.c and rir_validation_dir.c; C owns AIR origin falsifiers and
  their registration only. Root owns mir.c, MIR/DIR entry falsifiers, navigation,
  all commands/builds/WSL/process/Git and integration. No overlap or new track.
- Falsifiers: origin NULL, same-ID foreign root on the same DIR, whole foreign
  zero-intent DIR, same-shape foreign intent root. Check AIR and native MIR
  separately, no query regeneration, exact restoration. Also missing/borrowed
  origin, foreign HIR at DIR issuance and RIR/DIR crossing; valid empty/ordinary/
  hosted/intent inputs and all existing41/14/24 controls must remain green.
- Shared gate: same identified candidate, static60 then phase/MIR/AIR, strict
  C/LLVM, adjacent negatives, separate ASan/UBSan, portable replay and immutable
  export for independent review. No production edit preceded the hhxuI0 prefix.
  Canonical18435+16/MIR0, bootstrap, installed drivers and exact-SHA CI are OPEN.

### Reached RIR scope binding (same source-tuple boundary)

Review found another consumer of that source tuple before V8 integration.
hovwaF on unchangedc45 uses two separately parsed, same-name Main programs.
Both full native MIR baselines pass. Whole foreign RIR, missing root, root
relabel, root+scope-root relabel, a foreign scope under original RIR and scope
withdrawal all still pass MIR; exact restores pass. This is not a new rung.

- Existing RIR issuer publishes program_root and each scope's original AST,
  source ID, kind/name/owner/parameter shape. rir_flow's enrichment matcher uses
  numeric ID only, while MIR separately joins by name and silently skips facts
  when no scope matches. The latter is the forbidden path to delete.
- Repair the existing RIR binding owner, not another census: expose one pure
  rir_scope_for_hir_routine query from rir_flow.c/rir.h. Native binding requires
  exact scope AST/ID and program root, kind/name/owner/parameter shape and a
  unique match. HIR census binds RIR enrichment's root before mutation. Preserve
  the genuinely source-less partial contract, not a native fallback.
- MIR entry binds non-NULL RIR.program_root to SemanticResult.annotated_ast;
  its routine loop consumes that RIR query and fails on missing native scope.
  Delete mir_find_matching_rir_scope's name-only implementation/declaration.
  No new AST scan, RIR census, owner family or generalized provenance track.
- Added scope: A owns rir.h/rir_flow.c and deletion from mir_base_helpers.c/.h;
  root retains mir.c and MIR tests; B owns RIR tests only after A's API is fixed.
  C's AIR origin tests stay frozen. Root alone executes commands/builds.
- Gates/falsifiers: hovwaF's six mutations plus duplicate scope, cross-program
  enrichment, original restoration, missing/foreign scope identity and valid
  source-less/empty/implicit-Main/ordinary/hosted/intent positives. Use the same
  V8 integration gate above. This addition was mapped and prefixed before edit.

### Reached AIR late evidence and certificate boundary

AwA2jr on unchangedc45 confirms that AIR_A accepts+verifies Semantic_B, MIR_B,
MIR_B with its source relabeled to A, missing MIR snapshot/source and missing
SemanticResult. Both original native MIR baselines pass. A rejected input must
not consume the native collector's state: old same-object restoration fails
after the wrong evidence was already consumed (NULL-sem is a no-op and restores).
The executed probe prints literal newline escapes; values and hashes are kept.
The metadata getters themselves are NULL-safe (semantic.c506-544); the review's
suggested pre-guard NULL dereference is not an observed bug.

- Complete reached chain: checked DIR origin -> AIR synthesis -> later semantic
  DAG/capability/lifecycle import and MIR evidence import -> AIR verification ->
  existing certificate -> verified projection/parallel plan joining that MIR.
  AIR currently retains no whole-program origin, and hashes do not bind roots.
- Existing AIR issuer retains its own small private origin seal (AIR object,
  original borrowed AST root/nonzero ID), published once on successful synthesis.
  Public source-ID projection is consistency only; no AST scan or fact census.
  DIR dies before AIR, so do not borrow DIR or its witness. Free the AIR seal once;
  the original AST follows the already-existing AIR boundary AST borrow lifetime.
- DAG consumes AIR's origin and SemanticResult's existing immutable collection
  source snapshot before writing any AIR facts. MIR import consumes AIR's origin
  and MIR's existing original source snapshot before the one-shot started flag.
  Missing native inputs fail closed and leave the exact original AIR reusable.
  Real source-less partial fixtures and imported wire artifacts remain separate,
  not native proof. NULL metadata access is not used to invent source authority.
- One AIR/MIR source-current query in existing air_evidence_mir.c owns native
  pairing and is reused by import and MIR-taking final plan consumers. The
  certificate fingerprint validates the AIR origin and includes its source ID;
  missing/replaced origin cannot retain an already-issued native certificate.
  Do not hash process addresses into portable output or introduce another family.
- Scopes: A owns air.h/air.c and air_evidence_certificate.c for origin issuance,
  pure query/destruction/certificate. C owns air_evidence_dag.c and
  air_evidence_mir.c plus its existing internal declarations for source joins.
  B owns AIR late-evidence tests/registration; its RIR tests remain frozen. Root
  owns the two reached MIR-taking plan consumer joins and all integration.
- Falsifiers: foreign semantics/MIR, relabeled source, missing snapshot/source,
  missing semantic, missing/foreign AIR seal and detached AIR owner; deny before
  mutation, exact same-object restoration, existing one-shot refusal unchanged.
  Valid native/empty/hosted/intent and true source-less partial controls remain.
  All use the same V8 integration gate; no new implementation track is opened.

Last-consumer prefix r3IRKb on unchangedc45 confirms the original AIR_A+MIR_A
certificate and both original projection/parallel plans pass. The same native
certificate with same-shape MIR_B also passes both plans. Root adds the shared
AIR/MIR source-current query after certificate admission in those two planners,
and the existing AIR validator checks its origin before accepting native facts.
Origin issuance is before the synthesis-time air_verify, not after a certificate
could already have been issued. No runtime program was executed by these probes.

Integration receipts: jtoUVY first build stopped at MIR's incomplete SemanticResult
forward declaration; root added the existing semantic.h contract include.
RFvs6L then passes static/build, phase13422/0 and MIR681/0 on native60f22ecc.
PpCpQl exposes a test-link omission, not a runtime result: the AIR test's new
last-consumer assertions need the two actual planner units and target contract.
Root adds exactly those three objects to that existing test target. The separate
minimal verified_projection_plan_probe keeps its explicitly source-less local
accessor contract; it cannot issue or prove native origin. Native origin uses the
real issuer/query/planners in AIR integration tests, not those local accessors.

### Reached backend-test evidence producer (not a production extension)

Expanded mnlZxm passes semantic3230/0, then reports the complete transpile set
839/52. Those tests share issue_test_c_projection_plan's minimal stack AIR and
one equivalent LLVM block, paired with native MIR. The new last-consumer guard
correctly refuses that fabricated certificate. Do not weaken it for tests.

- Owner chain: each test's original semantic/HIR/DIR/RIR lowering -> native MIR
  -> actual AIR synth/DAG(original semantic)/MIR collect/verify -> actual C/LLVM,
  parallel and spawn plans -> test context/backend -> disposal before MIR/AST.
  The factories still own the original semantic inputs at the issuing point.
- Fix a test-only explicit TestProjectionEvidence bundle, with exact MIR/source
  identity, ready flag and the issued plans. Return it alongside MIR; consumers
  read the admitted values, never re-scan/retype the AST or recreate AIR late.
  Native missing/foreign evidence is an explicit failure. Keep genuine synthetic
  source-less tests separate; never fall back to partial AIR after native failure.
- Preserve backend-negative diagnostics: issue plans before deliberate MIR
  mutations and retain those plans while the backend checks specialization,
  source-order/SSA/resource facts. Do not turn those tests into source refusals.
  Keep existing semantic Type lifetime through backend completion; this is not a
  broad harness leak repair or permission to destroy the semantic result early.
- Scopes: A owns test_transpile.c and helpers_1/_2 for type, real producer,
  signatures/consumer admission/disposal. B owns all test_transpile_mir* cases
  and generic_method_specialization cases, including direct MIR lowerers and
  LLVM consumers. C owns the remaining core/domain/program/stdlib/parallel test
  callers. Root integrates and executes. No production source change, registry,
  cache, destroy macro, source annotation or unrelated fixture rewrite.
- All factories append an explicit TestProjectionEvidence* output (NULL only
  where no plan consumer is reached). Backend wrappers accept the matching
  explicit bundle. Callers dispose dynamic parallel/spawn rows before MIR/AST;
  source-less callers retain their explicitly bounded old row contract.
- Acceptance: entire original transpile suite, existing specialization and SSA
  negative diagnostics, all preceding focused/strict guards on native60f22ecc,
  expanded/sanitized/portable and immutable review. The 52 failures are retained
  as a test-boundary regression receipt; they are not 52 source-language fixes.

The first explicit-bundle integration UJj9ch reports999/15, all15 in the direct
intent-observability test. The real backend entry derives its context disposition
from the verified plan; the test binder had omitted that same derived assignment.
It now uses c_plan->disposition, not a forced true value. The bind-then-false
negative remains. The complete original suite then passes1014/0 in dwtv6t.

## V7 bounded issuer repair after V6 positive census

- Objective: restore valid enum method projection without relaxing original
  owner identity or the 14 deletion controls. Priority remains original fact
  identity, producer completeness, fail-closed consumers, then patch size.
- Chain: original enum AST/name -> hir_node_name -> HIRTopLevelItem.name ->
  HIRDecl.name -> hidden HIRRoutine(decl_id/declaration_ast/owner_name) ->
  original RIR METHOD tuple -> AIR owner prepass and exact IO correspondence.
  The last consumer is the existing AIR matcher. The selected callable alias
  is a Symbol with the original method ID, not a second top-level AST function.
- Observed full affected input set: scratchckimz3 on unchanged706 checks all8
  registered source_callable positives. Ordinary/payload enum aliases fail;
  six class/namespace/intent/event/conversion controls admit. Both failing
  methods retain owner_current=1 but their parent HIRDecl.name is NULL.
  hir_node_name omits AST_ENUM_DECL; all other hosted parent kinds are named.
- Required edit: only add the enum name to that existing HIR issuer. Keep
  hidden parent declaration identity, private witness, RIR method identity,
  and AIR requirements unchanged. No alias promotion, name-based exemption,
  current-time reissuance, source annotation or extra owner/TU is permitted.
- Tests: extend the existing hosted AIR fixture to enum ordinary/payload;
  parent-name/parent-identity/routine-owner/RIR-tuple mutations must refuse and
  restore. Record the new test against unchanged706 before the issuer edit.
  Re-run all8 original positives, narrow/static, phase/MIR/AIR and all14 old
  masks before strict C/LLVM, adjacent negatives/sanitizers/portable replay.
  Full fixed/current canonical remains a diagnostic, not local closure.
- Independent read-only review confirmed the same producer omission. V6 is
  immutable HOLD; a new identified V7 must carry the complete positive and
  negative receipts before approval or a bounded commit.
- Integration caught a test-contract mistake before the producer edit:
  AT0Tkd used a new hosted fixture that incorrectly demanded an AIR IO boundary
  for a hidden method. This version intentionally retains that IO only in RIR.
  Its subject case also failed, so that run is a test-defect receipt, not an
  enum-only producer falsifier. Keep the RIR tuple and explicit AIR exclusion;
  do not add a hosted AIR projection to satisfy the test. ckimz3 and strict
  KyDB4w already establish the real enum issuer failure independently.
- Corrected baselinevnACU5 on unchanged706 uses test SHA256C1E26BB7 and
  observes subject admitted with all8 tuple denials/restores and missing-layout
  no-regeneration, while ordinary/payload enum controls remain unadmitted.
  AIR156/1 is the expected regression baseline. The next edit adds only
  AST_ENUM_DECL -> ast_enum_name to hir_node_name; no AIR policy changes.

## Original callable set: complete projection obligation

The enum issuer repair on native834 passed ujycpJ phase13422/MIR636,
vZC7YP AIR157, KyCkMW all8 positives, strictwzyysD, expandedLUD37D and
portable5qWfBx. It is still HOLD, not a frozen or approved candidate:
VOqXeZ on unchanged834 reproduces all three whole-inventory withdrawals.
Main, intent and hidden method each have a passing original baseline, then
synth/collect/verify still pass after their derived rows vanish; exact restores
pass too. Original HIR items/decls remain. V6's prepass only sees surviving rows.

- Objective/priority: prove that the complete originally issued callable set
  survives projection, before proving each surviving row's identity. Preserve
  original identity, missing-fact refusal, complete consumers, then patch size.
- Existing owner: the HIR CFG execution witness issuer. Its per-routine seal
  is correct for surviving rows but cannot observe deletion of the whole row.
  The callgraph's source index is temporary, not an immutable census.
- One chain: typed AST -> HIR items/decls -> issued top-level function/intent,
  hidden method and synthetic routine rows -> HIR-owned original flat census
  -> HIR validation and native MIR entry -> exact DIR/RIR callable projection
  -> AIR collection/verification. Program census lives independently of mutable
  routine rows. Capture from already-issued HIR inventories once at successful
  HIR publication; never re-scan the AST or rebuild it in a current query.
- Census identity/lifetime: bind the exact HIRProgram and original program AST,
  source ID, original item/decl/function/intent/routine identities and original
  per-routine execution witness. Own only flat census storage; AST and routine
  witnesses follow existing HIR borrows. Publish after all fallible allocations;
  failed lowering frees partial storage and normal HIR destruction frees once.
- Consumers: HIR validation and the existing semantic-snapshot HIR->MIR boundary
  check that same census. AIR's native-RIR boundary checks it before any mutable
  projection, requires one original scope per source callable, and requires the
  exact DIR node/info for each original intent. Reverse/cardinality checks reject
  duplicate, foreign or extra callable rows. uPEMa3 verifies empty source and
  top-level statements on unchanged834. The latter is normalized by the parser
  into Main before HIR (RIR JSON records Main), not HIR's legacy synthetic
  executable. Existing rir_enrich already requires one source-ID scope per
  published source routine. Do not invent a synthetic-name exemption; raw
  unnormalized HIR is not an admitted native-RIR path.
- MIR's selected ReadFile caller completeness already has an owner: the retained
  semantic snapshot caller union. HIR adapter347+ and full MIR evidence311+
  demand exact routine matches for every original selected caller. Add deletion
  negatives to demonstrate it; do not add a second general MIR census without a
  falsifier. Scoped emission checks only its selected routine; program-wide
  admission remains the full MIR gate. Imported JSON is not this native proof.
- Forbidden: AIR-only guessed expected counts, mutable-row-derived valid-empty,
  AST root re-scans, per-routine-only program anchors, demand/reissue, alias
  promotion, safety relaxations, extra owner/TU or source ceremonies.
- Finite falsifier set: top function's function/routine/scope withdrawal masks;
  intent's HIR-intent/DIR-node/DIR-info/routine/scope masks; hidden method's
  routine/scope masks; missing/cross-program census; exact restore and valid
  empty/synthetic controls. Preserve all old14 masks, 24 hosted tuple controls,
  occurrence/placement negatives and strict runtime parity.
- Shared gate: same identified native and unchanged inputs, static60, focused
  phase/MIR/AIR, strict C/LLVM and adjacent sanitizer/portable controls; independent
  frozen review before bounded commit. Canonical, bootstrap, installed driver
  and exact-SHA CI remain separate OPEN endpoint obligations.

## Objective card and fixed boundary

- Objective: admit the unchanged canonical
  `SemanticDiagnosticCallCodesFromContent(ReadFile(path))` at
  diagnostic_owner.pgy570:79. The path is at88. Do not change input-retention
  rules to repair a result-source failure.
- Priority: selected identity and typed result evidence, single original owner,
  missing-fact refusal, last actual consumer, then patch size.
- Fact owner: existing checked-call publication owns the typed builtin contract;
  existing flow claims seal its selected independent-result facet; existing
  immutable collection snapshot owns those original claims after semantic teardown.
- Last consumer: actual HIR/MIR root/value/use/definition/return visitors and
  emission validation. No live SemanticContext, scope Symbol or Type authority
  may be needed after MOVE.
- Forbidden: UNKNOWN=0, source-name exemptions, manual Clone/drop/annotations,
  borrowing classification as result independence, deep cleanup grants,
  current-time demand/reissue, a second AST walker, parallel proof ownership,
  known-red push, or claiming native repair is self-host substitution.

## Verified whole chain

1. ReadFile typing selects BUILTIN_READ_FILE with one String path and String
   result (type_checker_builtins_nominal.c). Its effect/capability identity is
   already registered. Both runtime implementations resolve/copy the path,
   allocate a separate successful result buffer, and return an independent
   empty String on failure. Neither returns the input bytes. This is not an
   exact-copy, allocation-success, non-retention or cleanup certificate.
2. type_checker_expr.c publishes typed value before checked-call metadata.
   Nominal special issuance can seal the call first and publish its value later.
   Channel/slot/map special paths otherwise preserve value-then-call ordering.
   checked-call needs typed arguments; do not reverse this sequence.
3. indexed_string_borrow_owner.c currently misses ReadFile. Its first unready
   CALL publication emits UNKNOWN; its checked-call issuer republishes later.
   In defer the first issued read source is immutable, so UNKNOWN then0 would
   conflict. Delay String source/deferred-read issuance until the call tuple is
   ready, leaving typed value metadata intact. Unready/missing calls cannot
   answer a current String source.
4. publication_checked_intrinsic already verifies selected target/builtin,
   arity, result type and original argument rows during pending and ready
   phases. One ReadFile-result query adds exact name, String argument and
   result constraints. indexed String source and local-owner-constructor
   backing analysis consume that same query. Finish's allocator-premise query
   remains separate and unchanged; it must not become an ordinary constructor.
5. Existing flow snapshots capture local_owner_constructor/value origin, but
   syntax-only current replay skips typed fields. Ordinary direct
   Consume(ReadFile(...)) also has no definition recipe and is absent from the
   current snapshot caller union. Therefore a publication-only0 repair is
   incomplete. Seal a selected ReadFile-result facet in the existing claim;
   carry its original caller claims in the existing union, not a new recipe.
6. The immutable facet owns copied primitive/type-projection facts and original
   child/target/argument/caller identities, not a live Type pointer. Revalidate
   selected builtin/declaration/value-binding/stdlib identity, optional type
   projections, arity, original child ordinals and exact use roles after teardown.
   Source independence grants no owned elements or destruction permission.

Baseline ezkPLF (identified native47, analysis only) admits ReadFile(paths[0])
and a result without an unrelated live Array risk, but refuses
Consume(ReadFile(literalPath)) with that risk at6:17/MIR0. Wrong arity refuses;
a top-level ReadFile declaration is reserved and is not a new identity falsifier.
Full canonical wvdpM2 remains18435 errors/16 warnings/MIR0. These cascades are
not distinct defect counts. The prior literal candidate was independently
approved and committed unchanged; its full canonical deferred proof is still open.

## Complete change set and dependency order

1. Add one checked ReadFile-result query behind existing publication. Its
   pending and ready uses require the same original current typed call.
   Gate initial indexed String source/defer issuance on call readiness;
   preserve the original value/call issuance order and all other evidence.
   Live current replay withdraws source0 if any required tuple/type is missing.
2. Extend existing FunctionParamFlowIndexClaim with a subordinate selected
   intrinsic String-result facet. Capture only after exact typed issuance,
   replay the original tuple without demand, and use copied facts after ctx
   teardown. Reuse original children/ordinals rather than duplicate them or
   invent independent source IDs. Preserve ordinary Finish lifetime checks.
3. At the existing snapshot boundary enumerate the admitted publication rows,
   check every selected result against its original claim, deduplicate caller
   claims in the same union and MOVE once after all fallible work. Seal the
   selected witness total; deleting witnesses or caller membership cannot
   become valid-empty. No new result recipe, registry or independent owner.
   Prepare the existing flow store and caller index at this constructor boundary
   if no earlier consumer demanded them; do not require an unrelated named
   result to have created them. Current/source queries never prepare. First
   candidate40c8 reproduces this dependency: direct/borrowed/defer positives
   fail at capture add_caller with no flow store, while named-return passes.
   ScratchHTik8O records the same fail-closed boundary without altering production.
4. Activate existing source visitors when that count is nonzero. Join the
   selected call to its real caller, enclosing root and actual sink/ordinal.
   Check actual DEF/RETURN/value and call uses; an equal source number or AST
   syntax membership alone is not permission. Repeated legitimate reads of
   the same independent result remain reusable, not affine consumption.
   Measured t64YE9 exposed a missing static-producer condition: collection,
   full MIR and emission all accepted a duplicated exact ReadFile DEF. Extend
   the existing string_write_count/write_seen ordinal mechanism to original
   direct ReadFile LET/ASSIGN writes, deriving each ordinal from its admitted
   claim child edge. Seal percaller write totals in the same snapshot; current
   checks the original total and actual source_definition yields that first
   stable ordinal. Static duplicate writes refuse; ordinary repeated reads
   and repeated dynamic execution of one loop instruction remain valid. This
   is producer correspondence, not an affine String result or a new IO policy.
5. Run typed withdrawal, postctx actual-role mutation and source fixtures;
   integrate only after narrow gates. Then strict C/LLVM, adjacent negatives,
   sanitizers and unchanged fixed/current canonical. Survey any new reached
   dependency before extending this contract.

## Independent implementation scopes on this one chain

- A semantic_transfer: function_param_publication_routines.c/.h API, existing
  publication owner call/current/constructor consumers, and indexed String
  source publication/independent dispatch. No claim/snapshot/test edits.
  Preserve the size cap through existing cohesive owner boundaries, not
  one-use helper buckets or compressed policy. Supply exact API first.
- B primitive_operator_contract: existing flow claim representation, capture
  and current replay; collection_ownership_source_snapshot.c actual-role
  queries. No A issuance, root capture/MOVE or tests. Supply the immutable
  facet/query interface and current/witness count logic to root/C first.
- C selfhost_gate: existing phase/MIR tests, minimal authoring fixtures and
  function-param-flow registration. No production edits; expected diagnostics
  are provisional until root runs them. Preserve old Finish and no-deep-drop
  gates, and the original phase fault loop.
- Root: collection snapshot storage/capture/MOVE/count/view, HIR/MIR visitor
  activation, interface integration, all commands/build/WSL/process/Git and
  independent frozen review. One shared integration gate; no worktree or
  parallel implementation tracks. Preserve docs/01_intent_first_design.md.

Shared field name: independent_string_result_count (and sealed counterpart)
in existing snapshot/view; each counted claim belongs to exactly one admitted
caller and original checked ReadFile result. B/root agree exact query prototypes
before the callers are edited. No new authority is inferred from the count alone.

The current indexed-String borrow gate exits without a current function.
Snapshot selection covers every checked ReadFile result in its admitted
AST_FUNC_DECL callers; it does not invent a function index for a top-level or
intent boundary call. The existing intent boundary owner still checks those
calls. Candidateb131 exposed an overbroad selector: the pre-existing parsed
intent IO AIR test failed because it was required to supply a function index.
qivgl9 reproduces that exact transfer refusal. Restrict only this function-fact
transport to its original consuming gate, preserving the same typed ReadFile
result contract and existing intent owner. Foreign reuse of a selected function
witness must still refuse. Keep percaller
sealed witness totals so unrelated routine validation does not replay whole
bodies. Full original claims are checked once at their required owner boundary.

The size gate requires two cohesive physical moves under unchanged owners:
typed value/source replay into function_param_publication_values.c and the
original claim-capture implementation plus its selected facet into
function_param_flow_claim_publication.c. Original definitions are deleted, not
duplicated. These are translation units, not new SoT identities or progress rows.

## Reached deferred IO boundary dependency

Candidate7eb74b admits direct/borrowed/named results and their C/LLVM output
matches. The defer input now reaches AIR strict validation, which refuses at8:21:
AIR sees the original inner ReadFile, but RIR has no matching IO operation.
This is a real last-consumer gap, not a reason to weaken AIR or move the call.

The RIR statement walker currently omits AST_DEFER_STMT while AIR's boundary
walker visits its body. RIR_OP_IO has no resource-state or flow transition;
it is a may-effect boundary fact, not an execution-time authority. Actual
deferred execution belongs to the existing MIR part/exit/cleanup population.
MIR's entry IO observability carrier does not itself execute the concrete
builtin; trace placement must not be represented as execution timing.

Extend that existing RIR walker only at the reached defer body to produce IO
boundary facts through its existing selected IO-call rule. Reuse child traversal
and the existing IO selector; do not duplicate builtin-name policy or rescan
program roots. Preserve original CALL identity, enclosing scope and the defer
statement's source-inventory identity. Do not emit Release/Move/Read/Write or
other resource transitions while observing a deferred body. This is not general
RIR deferred resource semantics and does not claim to close them.

The bounded implementation may project the body once through the existing
walker into a fresh temporary scope, then publish only its IO ops into the
original scope and discard the temporary storage. No temporary resource op
may be normalized, published or applied to original resource state. Do not
shallow-copy original owned scope storage. Verify the walker has no external
mutation beyond its supplied scope. Nested projection keeps the outer defer
registration identity and the original concrete IO CALL identity; it must
not execute IO during registration or duplicate the concrete runtime call.

A owns this bounded RIR producer extension; C owns existing RIR/AIR tests plus
the deferred source fixture; root integrates and runs strict AIR, C/LLVM runtime,
withdrawn/wrong IO receipt and deferred-resource non-early-application controls.
Full body traversal that applies cleanup resources at registration, silent
unmatched-operation suppression and AIR strict-evidence weakening are forbidden.

## Acceptance and falsifying cases

- Direct literal path plus unrelated live owned String Array; borrowed path
  input; result through a named String return; and defer call. Test successful
  file read and independent failure result on C/LLVM, exact output/stderr parity.
- Missing/unready checked-call/source-present row, wrong selected builtin,
  target, stdlib/decl/value binding, arity, argument identity/binding/type or
  result type withdraws proof; restoration passes without new rows/demands.
- Preserve borrowed user identity result and late deferred path-use negatives.
  Do not execute an unsafe negative merely because an old compiler admits it.
- After teardown mutate original builtin/type projection or actual nested
  ReadFile value, caller, argument slot, definition or RETURN; all relevant
  collection/full/emission validators refuse, restoration passes. Coherent
  AST plus MIR mutations still disagree with the immutable original witness.
- Missing witness/caller union membership/sealed total fails closed. Repeated
  valid uses pass. Keep origin/source0 separate from deep release permission.

Budgets remain static60s, focused5min, integration shard30min. No increased
timeout/memory allowance or reduced canonical input. Endpoint is still zero
canonical refusals, real bootstrap/gen2==gen3, installed C/LLVM drivers and
same published-SHA required CI with the actual jobs executed.

## Focused candidate receipts

Native97231d1673d0d0921fa83bb05c3370d3ba96ae0be0333912f1ee6d0e5003386a
in the identified non-Git validation copy passes jVbHlv static/build,
phase13025/0 and MIR543/0. AIR5x9t4f passes157/0, including the existing
parsed intent IO test and the new non-vacuous deferred-resource/IO witness
controls. Repeated pure result reads remain allowed; original static ReadFile
DEF duplication is now refused in evidence/full/emission validators.

Earlier40c8/EfLcRq and7eb74b/S6OqDR were not passing narrow receipts. The first
phase setup incorrectly assumed an existing flow store; the second required a
current UNKNOWN source query to fail outright, instead of requiring it not to
grant source0. Production was not weakened to match those test assumptions.
The real duplicated-DEF acceptance in t64YE9 was fixed in production using
the original static write ordinal/seen mechanism, not by relaxing its oracle.

Source5mgNxK onb131 admits direct/borrowed/named/defer and preserves both
borrowed identity and late deferred-path refusals (MIR0). Runtime0IPatb on7eb
has three C/LLVM pairs andgP66b8 onb131 verifies original deferred stdout
`heap\n9\n` and late-read stdout `heap\n2\n`, stderr0/byte-equal, the latter
in a fresh directory per backend. The strict final972 gate is the authority
for those registered fixtures on the final candidate; do not relabel old
runtime logs as new-binary execution. No unsafe negative executes.

Diagnostic-only full kG8OxI on7eb still has18435 errors/16 warnings/MIR0;
NL8I3n first moves from570:79 to573:39, ArrayPush(codes,found[j]). This is not
final972 canonical admission and opens no next production scope before this
candidate is integrated. Bootstrap, installed drivers and required CI remain
OPEN/RED. Source files/tests stay frozen for independent bounded review.

## Reopened original execution-role obligations

Independent review of immutable v1 found concrete source-to-execution
seams. Do not start the next canonical element-lifetime repair or land v1.

1. Original RETURN evidence cannot be erased into a NULL parent. Replace the
   last HIR source statement and matching MIR source-inventory entry with the
   same original ReadFile CALL, leaving the return value intact. HIR, MIR
   evidence/full/emission must all refuse and restoration must pass. Repair
   the existing snapshot return-role query, not individual consumer exemptions.
2. A checked ReadFile result is reusable, but its static execution occurrence
   is not duplicable. Exact-copy a MIR STMT executing
   Consume(ReadFile(path)), preserving original AST/root/source index. All
   three MIR validators must refuse. Separate source CALL nodes, repeated
   identifier reads and runtime loop iterations remain legal.
3. AIR IO evidence must name the exact original AST_CALL, not any descendant
   of that call. Mutating the RIR IO AST to its argument or callee must refuse.
   The original AIR boundary walk also carries the outer deferred registration
   ID as an IO-boundary-local fact; AIR matching checks that original ID
   against the RIR transport. Missing/wrong registration IDs refuse for
   deferred IO. Ordinary non-deferred IO keeps its existing identity contract.
   Reuse the existing boundary walk context: no separate ancestor scan or
   builtin selector, and no new user-facing choice or semantic owner family.
4. Moving Main's exact IO op into Outside's RIR scope while withdrawing the
   original cannot prove Main's boundary. The boundary walk carries its
   original routine/intent declaration pointer and stable ID; IO matching
   joins those to the RIR scope's original owner, not just spelling. Its
   evidence provider must agree with that original owner and, for routine IO,
   the HIR routine provider. An intent's RIR owner is the intent declaration,
   whereas HIR evidence names a step; preserve that explicit identity mapping
   rather than requiring unrelated display names to be equal. Cover the
   foreign-scope move and provider mutation with behavior, not text scans.

The owner remains the original function claim facet and immutable collection
snapshot. Assign a stable occurrence ordinal from those admitted ReadFile
CALL claims, not from another AST inventory. Existing actual-expression
traversal consumes that ordinal once per static executable occurrence;
non-executing RIR resource summaries consume none. A multi-part defer uses
the original part it executes, not a fresh traversal of its whole body per
part. Do not make current proof queries affine, weaken original role joins,
or use source0 as a cleanup grant. Preserve existing direct-definition write
ordinals while closing nested actual/return/standalone execution reuse.

Dependency/edit scopes: C first adds whole-chain behavioral falsifiers and
restoration controls in the existing MIR test TU; root runs them against
unchanged candidate972 before production changes. B owns the original claim
occurrence ordinal query/internal declaration. A owns the immutable snapshot
occurrence join and RETURN role guard/public declaration. Root owns HIR
expression-visitor/MIR static occurrence consumption, AIR exact IO matching
and its boundary-local registration fact, and integration. These
are disjoint edits on one existing chain, not parallel new implementation
tracks. All builds, WSL, process control and Git remain root-only.

Acceptance is both new negatives plus the four preceding actual-slot,
detached-root, foreign-caller and duplicated direct-DEF controls, repeated
valid read/call/loop and deferred positive controls, strict C/LLVM, phase/MIR/
AIR, broader regression and separate sanitizer gates. Re-freeze changed
sources and get independent review again. Canonical, bootstrap, installed
drivers and published-SHA CI remain separate OPEN obligations.

V1 evidence export: source167 manifest
12f552273c7d8eae82ba49aa6af068a3a0729b37835910bff9bf40d60cb896cf;
evidence241 manifest
4b9426da1a93c9894bd3cea9c769e6e286fd49e8964a8f8fbc154d6e63bb6689.
StrictpJSbMW, broad6cA9Xg (semantic3230/transpile1012/AIR157, Map24+58,
four IR flow gates, ASan/UBSan phase13025/MIR543 with leaks OFF), and
portable8fRtrf pass. Portable phase/MIR/AIR have20 hashed runtime inputs;
missing input controls each return1, restoration passes. Final972 canonical
wrVPA2 remains18435 errors+16 warnings/MIR0; WD4xSo first refuses573:39.

Scratch nested deferred ReadFile e56X6L refuses malformed MIR defer parts.
The SSA-bearing no-IO control kSoyK6 refuses identically on previous47 and
current972; routing supports CALL/ASSIGN, not a nested DEFER part. No-SSA
controls bypassed this existing check and were inconclusive. This is not a
new IO regression or claimed nested-defer support. Do not open that separate
MIR registration/SSA/JSON/C+LLVM contract while this executable rung is open.

## V1 falsification and repaired candidate

Original native972 with new strict behavioral tests utKt68/Sy8L7a accepts
RETURN-parent erasure at HIR/evidence/full/emission and duplicated nested
ReadFile execution at all three MIR validators: seven failing assertions,
548 passed. AIR additionally accepts both descendant AST substitutions,
missing/wrong defer registration, the foreign-scope move and foreign provider.
Restoration and the preceding four identity/DEF controls pass. These are
observed production gaps, not weakened test oracles.

Native2d6584b754b020ba96c511d666a7db72701c5835d5035d7830fbe11d9f413089
repairs those gaps through the original claims/snapshot and actual MIR
expression visitor. Query APIs remain pure/reusable; only an invocation-local
MIR static occurrence ledger consumes ordinals. A deferred row visits its own
original body part. RIR summaries have no executable roots. No new semantic
family or second AST walker was introduced.

The existing AIR boundary walk carries the original routine/intent pointer
and ID, plus the outer defer registration ID for IO. Exact CALL/scope joins
and the provider validator consume those facts; the JSON projection makes
both IDs inspectable. Intent evidence names its declaration at RIR and its
step at HIR, rather than silently requiring two different names to be equal.

Narrow0Tfona passes static/build, phase13025/0 and MIR555/0. AIRLcEPwh passes
157/0, including all new faults and restoration. The precedingb3GqXk AIR
155/2 was two incomplete manually constructed positive fixtures, not a
production relaxation: their owner was a line-only stack node with no issued
ID, and RIR had no owner AST/ID. Both now use the existing intent declaration
factory and stable-ID issuer, sharing that exact owner through DIR/RIR.
The original nested/loop positive assertions remain unchanged.

StrictrSTGp9 on the byte-identical native2d passes C/LLVM, including the new
distinct-call/repeated-value/three-iteration and multipart-defer controls.
Final source-pairs22gJ35 confirms155 raw D/Linux source/test/Makefile pairs.
The final strict rerun and expanded/sanitizer/portable/canonical receipts are
pending. No actual bootstrap, installed driver or published-SHA CI success is
inferred from these native receipts.

## V2 completeness repair: original executable roles, not just seen calls

The final v2 receipts are strictBPSmFE, broad8ItGSo (including separate
ASan/UBSan with leak detection OFF), portablevOpzfn and documentationFUtHoB.
They pass their stated scopes. Source179 manifest
b1148ce770ec3dbf576bff630678b37817fc9dbfc16821cd583ff9bdb900c7b7 and
evidence120 manifest
be5ef56549f33a3aedb458d20e848db2c93cf984668f38088241b6e0e6a74f34
are frozen at `E:\PergyraValidation\readfile-result-20261018-v2`.
Full canonicalh2puQQ remains18435 errors/16 warnings/MIR0 on each unchanged
fixed/current input; observer tjZeBj first refuses573:39. No next production
scope is open. V2 is not approved.

Erasure baseline1NqJ20 on unchanged native2d reports MIR560 passed/8 failed:
HIR-only deletion, MIR-only deletion and coherent HIR+MIR deletion are all
accepted at their affected validation boundaries. Restoration passes. New
falsifiers also isolate HIR duplication, LET/DEF role erasure, and promotion
of the inner ReadFile from `Log(Consume(ReadFile(...)))` into an executable
root while retaining one ReadFile occurrence. Their results must be observed
before recording them as confirmed faults.

- Objective/invariant: every originally admitted, potentially executable
  selected call and its direct definition remains exactly once in its
  original executable role. Reusable values and runtime loop iterations are
  not affine execution. Priority remains original identity/role, missing-fact
  refusal, complete actual consumption, then patch size.
- Owner and lifetime: the original flow claim contains execution/reachability,
  child ordinals and projected control-step/root identity. Validate that
  projection against its existing owner before MOVE; retain its immutable
  claim-side role, not a fresh AST inventory after teardown. The selected
  facet and caller totals seal the expected may-execute subset. Missing
  execution evidence fails closed; explicitly unreachable is not required.
  In particular dead transaction compensation can be typed but unreachable.
- Consumers: the existing HIR source visitor gains one invocation-local
  ledger over actual reachable CFG evaluations. Its existing pure source
  replay remains unchanged. Control-container inventory is not execution;
  body blocks own their statements, and RETURN is counted once rather than
  again through its terminator value. MIR uses actual instruction roots,
  validated DEF/ASSIGN/RETURN parent roles, and each deferred part. Original
  control-step ancestry binds nested CALLs to the actual outer execution root.
  Range/foreach header fact carriers must be distinguished from evaluations
  using their existing lowering/emission contract, not duplicate exemptions.
  Move the unchanged CFG-owned-control classification from its MIR-only
  translation unit to the existing HIR CFG lowering owner, migrate all MIR
  readers and delete that former unit/header/build rows. HIR and MIR must not
  own competing copies of this lowering decision. The existing HIR CFG owner
  also replays reachability from actual edges before a ledger trusts its bit;
  corresponding MIR blocks must retain the original HIR reachability.
- Completion: compare selected expected occurrence and direct-write subsets
  with the shared ledger at both HIR and MIR boundaries. MIR routine-only
  validation checks only that caller. Do not require every old shared write
  recipe ordinal to appear: this repair covers the independent String write
  slice, whose original count/offset already participates in write_seen.
- Forbidden: a second syntax walk/semantic owner, counting source inventory
  as executed, treating absent execution as unreachable, blanket all-selected
  coverage, accepting a promoted child to fill its erased parent's slot,
  or weakening a prior role/source/negative check.
- Dependency and scopes: C freezes the behavioral erasure/duplicate/role
  mutators, root reproduces them on2d. B extends the original selected claim
  facet and pure ordinal/root/required queries. A carries and seals caller
  expected totals and snapshot role/completion queries. Root integrates HIR/
  MIR actual-role consumption and the constructor projection validation.
  These are disjoint scopes in this one chain, not independent tracks.
- Acceptance: all erasure/duplication/role faults refuse and restoration
  passes, alongside original standalone/direct/return/deferred positives,
  distinct calls, repeated reads and runtime iterations. Then phase/MIR/AIR,
  strict C/LLVM, adjacent regressions, sanitizer/portable replay and unchanged
  canonical diagnostics, source/evidence freeze and independent v3 review.
  Bootstrap, installed drivers and published-SHA CI remain OPEN until run.

Reachability baselines784lt on unchanged2d reports MIR574/24: moving the
original statement into a naturally unreachable join or withdrawing its
reachable bit exposes five further accepted validation outcomes. Inverse
unreachable-source promotion has no admitted positive baseline and is not
claimed as a tested negative. Nativea5da361e (narrowc9k9Ia) now passes static,
phase13025/0 and MIR598/0, including all24 formerly failing assertions.
Original guard/for-start/nested-if/short-circuit inputs remain admitted;
for-end now admits because LOOP_INIT's retained end graph is not a second
evaluation. Dead compensation remains a pre-existing semantic transfer refusal.
Combined runtimeLenJdc confirms exact C/LLVM output for these five positions,
with stderr0; its one combined fixture is registered for final strict replay.

## Reached AIR reverse-completeness obligation

RIR9aIBIC on native2d and UcGQ20 on nativea5 (unchanged v2 AIR owner) confirm
that duplicating an exact original RIR IO row is accepted by synthesis,
collection and verify. The same detailed UcGQ20 test independently confirms
missing RIR IO and duplicated AIR IO boundary refusal and restoration. This
is one additional gap, not three confirmed faults.

The IO matcher currently accepts its first match and boundary/provider
evidence deduplication loses the original multiplicity. The last simultaneous
consumer is RIR evidence collection. A owns the exact tuple matcher and this
collector, its internal API and the air.c call. Root updates two direct test
call sites and runs all verification. C owns the behavioral faults, B owns
post-MOVE original execution-facet withdrawal tests; all are on this chain.

Pass the original HIR function and DIR intent-declaration inventories to that
collector, not a new source scan or an inferred scope-name/kind filter. Within
that explicitly projected owner set, each RIR IO row and AIR IO boundary must
join exactly once on original owner AST/stable ID, exact CALL AST/stable ID,
builtin subject and (for defer) outer registration ID. The existing tuple
matcher remains the single matching policy. Missing or duplicate joins refuse;
non-IO behavior and RIR resource state stay unchanged. No retained RIR pointer
becomes a new AIR-only authority after collection.

RIR also projects hosted-method bodies that AIR's current HIR function/intent
boundary walker does not cover. They remain outside this correspondence claim
because the original projection inventory excludes them, not because a missing
AIR IO boundary silently disables checking. Do not expand hosted-method AIR
projection or claim it closed in this repair. AIR-only input without RIR keeps
its existing contract. Re-run original hand-built and parsed intent/routine
positives, all original nine tuple faults, foreign scope/provider, both reverse
controls and restoration. Then re-freeze v3 and request independent review.

The original HIRDecl FUNCTION/EXECUTABLE and DIRNode INTENT rows check one-sided
withdrawal of their function/intent projection inventories. V3 does not prove
scope-kind identity or dual withdrawal: its method-disguise test also duplicates
IO, so duplicate denial masks the separate kind defect. Zero matches in both
inventories must not imply a valid hosted-method exclusion.

## Current v3 integration receipts

Native3a34e750dd3522f2894007790f0532cc140f8792707e6ae1f0dde766c22808c0
passes qfn5l8 static/build, phase13422/0 and MIR598/0. This includes the new
post-MOVE, ctx-destroyed execution flag/count/root withdrawal and restoration
controls, not just original syntax membership. AIRmFkj8R passes157/0: all IO
tuple/reverse/scope/provider/inventory faults and their restoration pass.
Source-pairsTJrHOq confirms166 changed/context input pairs are raw byte-equal
on D and the identified Linux validation copy.

Earlier on this identical native, AJ9FmH also passes phase13422/MIR598 and
strict5ry9P6 passes C/LLVM, but AIRzfFUsa is155/2. Those two old hand-built
positives had logical node1 at vector index0 and a detached step, so the
stable-ID issuer never reached their inner IO CALL. Final fixtures use the
real index0 tuple and intent-owned step graph before one stable-ID issuance;
duplicate destruction of that owned step is removed. Existing positive
assertions and production refusal rules are unchanged. Keep the failed
receipt; it is not a production-rule relaxation or a passing AIR result.

Final strictPVqxAR, expandedVfmxEF and portableIRYld3 pass on the same frozen
native. Separate ASan/UBSan phase13422/MIR598 pass with leak detection OFF.
Portable missing-input/ABI-artifact controls refuse and restoration passes.
The unchanged fixed/current canonical pair eUKAaw remains18435 errors plus16
warnings/MIR0; JZnaNW first refuses diagnostic_owner.pgy573:39,
ArrayPush(codes,found[j]). Freeze v3 source191/evidence194 is complete under
E:/PergyraValidation/readfile-result-20261018-v3; do not overwrite it.
No next production scope is open and no bootstrap/installed-driver/CI success
is inferred from these scoped receipts.

## V4 scope-identity closure card

- Objective: preserve exact original IO owner identity even when both of its
  HIR/DIR inventory rows are withdrawn. Prove that a hosted-method exclusion
  is positively owned, rather than inferred from absence.
- Priority: original identity and one existing owner, fail-closed completeness,
  genuine hosted/top-level-action positives, then patch size.
- Owners and chain: HIR routine issuance already records decl_id, owner name,
  owner AST type, hosted/action status and stable routine identity. RIR scope
  issuance records routine kind/name/owner/AST/ID. Existing AIR RIR matching
  owns their join; evidence collection is the last simultaneous consumer.
  DIR intents require their original node and intent projection regardless of
  whether either inventory currently yields a match. Do not rescan AST roots.
- Complete required changes: isolate the kind-only Main fault; HIR declaration
  plus function projection withdrawal; symmetric DIR node plus intent
  projection withdrawal; independent name/host mutations. Validate required
  projection and scope identity against the existing admitted routine/host
  relation. Only a positively validated genuine hosted method may remain
  outside AIR IO projection. Preserve AIR-only/non-IO behavior.
- Forbidden: zero-match hosted exemption; kind-only/name-only exemption;
  another semantic owner or AST inventory; expanding hosted AIR support;
  weakening genuine existing positives; treating missing rows as unreachable.
- Dependency/scopes: C changes only behavioral tests, then root reproduces on
  unchanged3a with retained baseline binaries. A surveys and repairs the same
  matcher/collector/internal API; B audits existing HIR/DIR issuance/host facts
  read-only. Root owns integration, all execution and Git. No next String-copy
  production track is opened while this chain is under review.
- Gates: pre-fix acceptance plus exact restoration; post-fix denial/restoration
  for each independent mutation, top-level function/action/intent and hosted
  positives; narrow phase/MIR/AIR, strict C/LLVM, adjacent regressions and
  sanitizer/portable replay, fixed unchanged canonical diagnostics. Re-freeze
  source/evidence as v4 and obtain independent review before bounded commit.
  Actual bootstrap/installed-driver/published-SHA CI remain OPEN.

## V6 original-owner prepass before IO cardinality

Pre-fix dXufNx on unchangedcce executes all seven deletion masks for each
fixed Main and NestedIO fixture. Bits1/2 withdraw HIRDecl/functions (or
DIRNode/intent projection); bit4 withdraws RIR IO. Masks1-5 already refuse;
masks6 and7 are accepted in both families. Every exact restoration passes.
This is the complete14-case census on those fixed original-owner inputs,
not two isolated first failures. The executed source/binary are retained.

- Missing check: remaining original owner evidence is validated only after
  discovering current IO. Zero derived AIR and RIR rows do not prove that
  their retained original owner may lose its projection membership.
- Existing owner/last consumer: AIR's original owner matcher already owns
  exact HIR declaration/function and DIR node/intent membership. The RIR
  evidence collector must invoke it in an independent original-owner prepass,
  before IO cardinality/multiplicity. No new witness family is required.
- Required changes: validate each retained issued HIR routine's private owner
  witness and projection membership; validate retained source-backed callable
  RIR scope identity and membership regardless of IO count. A genuine hosted
  exclusion still requires the original private owner witness and parent join.
  Native partial/stale scope tuples fail closed. Existing RIR-less AIR-only
  input and non-source-backed manual non-IO fragments keep their established
  contract; they cannot replace a retained native original-owner receipt.
- Premise/boundary: these countercases retain the original source AST,
  HIR routine/private witness and/or original RIR callable scope. This is not
  a proof about erasing every original source/owner input and then presenting
  an unrelated empty fragment. No AST root rescan or new generic program
  completeness model/registry is opened.
- Dependency/scopes: C freezes the14 behavioral cases, root reproduces on
  unchangedcce; A changes only the existing AIR collector with the existing
  matcher; B audits source/fragment boundary read-only. Root runs all gates
  and integration. No next String-copy implementation scope is open.
- Acceptance: all14 masks refuse with exact restoration, all original
  standalone/deferred/action/hosted/non-IO/AIR-only controls preserved;
  same-native narrow/strict/expanded/sanitizer/portable, unchanged canonical
  diagnostics, immutable v6 export and independent review before commit.
  Bootstrap, installed drivers and published-SHA required CI remain OPEN.

V6 native7063605bbcf7cb80ed8dabc401a780ebf35b16c142520a8eec1d3063ba88f32f
passes CZT0dF static/build, phase13422/0 and MIR636/0; F61cju AIR157/0
preserves every existing positive and refuses/restores all14 deletion masks.
Only the existing AIR collector changed production code since v5. It checks
retained issued HIR routine owner witnesses/projection membership and original
source-backed RIR callable tuples before testing IO cardinality. AIR-only,
non-source-backed manual non-IO, genuine hosted and native non-callable scopes
keep their existing contract. No new witness producer, owner family, AST walk
or semantic fallback was added. Final strict/expanded/sanitizer/portable and
unchanged canonical diagnostics precede v6 freeze/review and bounded commit.

The v4 AIR pre-fix receipt gKPGRo retains native3a and both old/new AIR unit
binaries. DIR dual withdrawal, HIR dual withdrawal, Main kind-only and
host-name-only faults each verify successfully before repair; restoration
passes. Name-only already refuses. Fresh-factory top-level action-shaped and
parsed genuine hosted-method positives both pass. These are independent
faults, not duplicate-IO disguises.

### Reached execution-placement completion of the same chain

- New falsifiers: swap ReadFile/non-IO statements between two reachable
  then/else blocks; swap only branch true/false successors; MIR-only and
  coherent HIR+MIR versions; source statement order where represented.
  Reachability and occurrence multiplicity do not prove any of these roles.
- Missing fact: original ordered CFG layout. Current HIR blocks and derived
  reachability are mutable; flow claims own source/control ancestry but no
  original lowered block/edge identity. Do not reconstruct a second CFG or
  walk AST roots at each consumer.
- Chosen producer: the existing HIR CFG finishing owner seals an opaque,
  routine-owned execution-layout witness before publishing the routine.
  It records original routine/body identity, entry, block order, ordered source
  statement tuples, terminator condition/value/kind and directed successor
  roles. One initial issuance only; current queries cannot seal or repair it.
  This subordinate HIR artifact is not a new semantic fact family or a
  semantic-preservation proof for arbitrary CFG optimizations. Current HIR
  analysis adds derived facts, not an authorized execution-layout rewrite.
- Lifetime: the witness borrows the same source AST as HIR and is freed on
  routine failure/destruction. MIR already borrows its original HIR routine
  through native emission; imported MIR is not granted this source witness.
  Original layout validation is required only by the selected independent
  ReadFile execution slice. Ordinary unrelated manual HIR fixtures keep their
  existing contract. Record memory/time on the unchanged canonical pair.
- Last consumer: the existing MIR HIR-fact transfer validates the original
  HIR witness, exact source block inventory and ordered statement-role/index
  correspondence, condition/value carrier and directed successors. Its
  existing full/routine-only/emission validation callers consume this same
  join; no backend recovers source placement independently. Synthetic cleanup
  metadata is not another source evaluation and cannot replace a missing join.
- Scopes: B implements only the existing HIR CFG finish/query/destruction
  lifetime after C's unchanged3a baseline; root implements HIR/MIR consumers;
  A remains on exact AIR owner/scope identity; C owns the behavioral tests.
  One shared phase/MIR/AIR plus strict/expanded/portable integration gate.

V4 CFG pre-fix ES8UT7 on unchanged3a reports607/22: statement relocation and
edge-polarity changes, each HIR-only/MIR-only/coherent, are all accepted at
the reached boundaries; exact restoration passes. Initial lnQ10W did not run
because the new test used nonexistent MIRBlock instead of MIRBasicBlock;
retain that compile-failure receipt, not as a production failure or falsifier.

Nativec2d758da9244d8a57a0aa8393f519d150e2c2809649eb33b0c5dd21730156dca
is now frozen. H1OVSH passes static/build, phase13422/0 and MIR636/0;
FTha3s passes AIR157/0. All six placement variants refuse and restore; the
new missing-layout control refuses at HIR/MIR/emission, leaves the witness
NULL during queries and re-admits after exact restoration. AIR kind-only,
host-name-only and both dual-withdrawal faults refuse independently, while
top-level action-shaped/factory and parsed genuine hosted positives pass.
The witness is opaque, issued once at successful existing HIR CFG finish,
and freed at routine discard/destruction. No new translation unit/family or
AST root walk was introduced for it. Native MIR joins the full original block
inventory, ordered statement role/index, terminator expression carriers and
directed edges; source0 and all cleanup rights remain unchanged. Final
strict/expanded/portable and unchanged canonical diagnostics precede immutable
v4 export and independent review. No next production scope is open.

## V5 original routine-owner issuance completion

Independent review found that v4 still trusts mutually consistent mutable
HIR routine host flags and a foreign host declaration. Pre-fix93jFBM on
unchangedc2 confirms coherent Main impersonation: synth/collect/verify1,
denied0, restoration1. The original Main AST/name/parameters/one IO are
unchanged; its HIR declaration/functions entries are withdrawn and remaining
routine/RIR metadata impersonates an existing Hosted declaration. Preserve
the executed baseline binary/source. This extends the same original-owner
chain, not hosted AIR support or another production track.

- Missing original fact: positively issued routine-to-parent-declaration
  identity and host/action/name/parameter tuple. The existing HIR private
  witness already seals decl_id but AIR did not consume that issuance.
- Producer/lifetime: existing HIR routine constructors carry the exact parent
  declaration AST (self for top-level; host for hidden methods) alongside
  decl_id. The same private finish-time witness seals that pointer/stable ID
  and original kind, hosted/action flags, owner AST type, parameter count and
  owned name copies. Names are copied to detect in-place mutation too. The
  existing witness release handles all new allocations, including failure.
- Last consumer: AIR's existing matcher must consume the original owner
  witness before a hosted exclusion and join the current HIRDecl to the sealed
  parent AST. A name/decl_id coincidence is not membership. The owner-only
  current query is independent of CFG/inventory size and does not re-walk
  bodies or issue evidence; full CFG current reuses it before layout replay.
- Scopes: B modifies the same five HIR issuance/query/lifetime files; A only
  consumes the query in the existing AIR matcher; C owns the coherent baseline
  and withdrawal/restoration controls. Root alone runs all gates/Git.
- Forbidden: zero-match exemption, mutable host flags as original authority,
  independent method-dispatch/AST root replay, another owner/family, manual
  source ceremony or weakening normal/genuine-hosted positives.
- Gates: unchangedc2 pre-fix acceptance, original private-owner withdrawal and
  coherent reclassification denial/restoration, all existing original-owner,
  execution-placement and IO negatives; narrow/strict/expanded/sanitizer and
  portable replay; unchanged full canonical diagnostics. Freeze new v5 source
  and evidence, request independent review, then bounded commit. Actual
  bootstrap/installed-driver/published-SHA CI remain OPEN.

V5 nativecce1523ba73ca325136312e1edc160cba62b2a219204b4f4cea5aa3978ad5668
passes static/build, phase13422/0 and MIR636/0 inViSkW3, AIR157/0 in3JLUoO.
The final coherent-host test also changes the new declaration_ast field; that
post-issuance facet control did not exist in the c2 baseline93jFBM and is not
retroactively claimed there. The genuine hosted ReadFile positive is fresh
DIR/HIR/RIR factory output; withdrawing only its execution_layout denies AIR,
does not regenerate it and exact restoration re-admits. All prior assertions
remain. Original names are copied into the same opaque witness and freed on
every new allocation-failure and normal/failed-routine release path. Final
strict/expanded/sanitizer/portable and unchanged canonical diagnostics are
pending; no next production scope or whole-closure claim is opened.
