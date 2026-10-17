# Main-only takeover and P0 integration

Status: ACTIVE; integration and acceptance are not yet established.
Date: 2026-10-16 KST. Integration owner: GPT in the main checkout.

## Objective and boundary

- Objective: preserve all inherited work, integrate the published performance
  changes and Claude's P0 lane into main, then verify the actual installed
  compiler chain on that integrated source before publication.
- Priority: recoverability, stable semantic identity, current owner evidence,
  preserved refusal behavior, installed artifacts, measured memory, exact-SHA CI.
- Inputs: shared main `af8ede6f0` with 231 tracked changes and 317 untracked
  paths; published main `571ae8c3a`; Claude lane `3721a901c` (including the
  unverified memory change `3cc0ecc47`). The clean lane's fixed-point receipt
  belongs to `b8e66a387`, not to the combined source.
- Fact owner: existing semantic ownership, focus-recovery and body-bundle
  owners. MIR producers and admitted consumers retain their existing identities.
- Last consumers: admitted C/LLVM codegen, gen2/gen3 fixed point, installed
  driver CLI, dependent compiler shards and the published implementation CI.
- Forbidden: throwing away inherited WIP, choosing an entire merge side without
  semantic review, native fallback for public acceptance, weakening a negative
  oracle, reduced canonical inputs, increased memory/time budgets, or claiming
  old receipts validate the integrated source.

## Current active rung — fixed-input summary work (2026-10-17)

### Current element-retention integration boundary

#### Reached full-input boundary after named-String integration

- Objective/order: preserve typed String payload provenance through a field
  read such as `nodes[i].type_name`, without treating unrelated local
  Array<String> storage or a parameter mode as ownership evidence. Current
  full-main entrypoint is `src/self_hosted/codegen/main.pgy`; the first real
  refusal is `CodegenAstTextTypedArenaFromNodes`/185:44 at ArrayPush of that
  field. The former135:13 named-result use has only narrow proof so far;
  semantic success and canonical admission remain unestablished.
- Current chain: typed member/index selection and parameter publication ->
  existing flow input claims/backing dependencies -> indexed String source ->
  actual retaining-call consumer -> returned arena/caller cleanup. The
  indexed owner has no admitted aggregate-field source here and conservatively
  matches a local String-array candidate. Map the exact producer, carried path,
  mutation/rebind invalidation, all consumers and actual caller retirement
  before choosing the repair. A current field identity alone is not a storage
  or payload-lifetime certificate.
- No fallback: do not map a field/parameter root to source0 solely from its
  syntax, assume value mode owns its fields, restore the removed member-ID
  exclusive grant, or insert Clone/drop/annotations into the canonical input.
  Do not weaken the old String retention or negative gates.
- Unknowns to resolve before production edits: the existing typed place fact
  that can own nested member/index lineage; the lifetime/return dependency
  needed when the source is an aggregate parameter; and whether the actual
  caller's release is blocked while a returned payload remains live.
  A surveys existing typed/indexed publication, B the reached flow/caller
  lifetime chain, C bounded positive and analyze-only negative falsifiers in
  scratch. These are read-only investigations, not successor implementation
  tracks. Root alone builds, instruments and owns the shared integration gate.
- Acceptance/falsification: admit the actual unchanged compiler projection;
  reject a retained field borrowed from an owned String array when its owner
  is released, including aliases and mutation/rebinding. Preserve original
  phase/full strict, C/LLVM parity, ASan/UBSan, both frozen/current full inputs,
  then bootstrap/gen2==gen3/installed driver and exact published-SHA CI.
- Receipt: native01d64ac6, source61a+22 verified current code/test overlays:
  phase10995/0, original/full registered strict, safe C/LLVM66/66, MIR370/0,
  semantic3230/0, transpile1012/0, AIR156/0, Map24/58, all four IR flow gates
  PASS (HIR/RIR first needed their fresh-snapshot test binaries built).
  Named currency B0poXd separately invokes every requested old fault group.
  ASan/UBSan tmCfU7 passes10995/0 with a live UAF witness, leaks OFF.
  Full fixed/current jnk0G1 remains RED/MIR0,27.97/28.16s,
  1367584/1365684KiB. 18435 errors are cascading BORROW diagnostics, not an
  independent defect count. Diagnostic-only `azyrRX`
  retains original semantics and identifies the first refusal above; the two
  own-field retire errors occur after it. No bootstrap/CI closure or push.
- Bounded String source checkpoint is now
  `b53e9dd0568fe5a2ca7b6d7087231a1dfee6aea6`, local/unpublished. Corrected
  broad runner Df1vHx exits0 with the newly registered positive/negative
  fixtures. The ASan phase uses204840KiB with leaks OFF. Documentation follows
  in its own checkpoint; subsequent field-provenance investigation is not part
  of this bounded green proof.

#### Broad integration failures before canonical continuation

- Current integration native is
  `dc9746258aa7810211d3e4eeea6fcbd309715f08f402fafd409255c27f36824d`.
  Diagnostic-only ref-call identity and ownership permission are now separate:
  phase10942/0 and safe C/LLVM64/64 PASS. The full strict gate passes its
  diagnostic/admission/runtime groups but is RED at its original4097 independent
  demand control (30-second limit). A diagnostic-only run of the same362471-byte
  input also times out after45.86s/203028KiB; this is not an enlarged acceptance
  budget. Source and binary are frozen while the reached operation is sampled.
- Cost-blocker objective: make that unchanged control complete within its
  existing30-second gate. Priority: unchanged summary episode/work semantics,
  checked identity and currentness, negative oversized-closure control, then
  cost. Existing publication/host/summary owners remain authoritative; root
  owns measurement and integration, A surveys only the reached repeated work.
  Do not add a general cache, assume a missing fact, shrink4097/4096 inputs,
  raise limits, or start the named-String repair while this boundary is RED.
  A sampled stack is localization evidence, never compiler acceptance.
- Same-input diagnostic sampling3VSYze finds58/59 CPU samples in final
  Array preservation consumers,50 in array-preservation failures and35 in
  output-current replay. Each consumer rechecks all preceding calls although
  this synchronous finalizer has just checked every call against the same
  typed generation. Repair only this repeated owned operation: reuse the
  existing per-call verdicts during the finalizer's closed consumer loop.
  Pending recording is already disabled there; no user code or external edit
  executes in that loop. Clear the private scope before return. Public queries
  after finalization must still replay their current facts, including all
  existing AST/call/formal/frame tamper negatives. Owner: deferred preservation;
  last consumer: its call/drop loop; forbidden: persistent unchecked verdicts.
- Same-input result after that scoped reuse: native
  `c807569c165d9795470c5517ed9c1a15f8110ce564e406ead8fac85ea3efd912`,
  original30-second control rc0 in1.74s/203136KiB,4097 entries/evaluations,
  zero recursion hits, unchanged4096 episode budget. Phase10942/0 PASS,
  including post-finalization currentness tampering. Full strict/broad replay
  is in progress; the timing alone is not an integration PASS.
- Once this integration gate is green, the next already reached canonical
  dependency is named String result provenance. The same declared-result
  frame/control-root owner must prove complete returns and free-variable
  closure, then current formal RETURN dependencies determine actual backing.
  Non-dependent aggregate arguments do not confer backing; dependent aggregate
  arguments remain unproved. Existing indexed source publication carries only
  the exact initializer lineage; a final shallow-store obligation requires
  current source0. UNKNOWN, owned-String-result flags and member IDs do not
  grant independence or element cleanup rights. B owns existing flow store,
  internal declarations and frame/control-root factoring; root owns existing
  indexed/LET/deferred consumers. C supplies paired and currentness falsifiers.
- Integration checkpoint: `61a92442f684c4d3c2c23653461bf4cb665d2a45` on
  main, local/unpublished. Nativec807 broadA8cuX0 is fully green at this bounded
  boundary: original strict including4097/4096 controls, MIR370/0,
  semantic3230/0, transpile1012/0, Map24C/LLVM positives+58 negatives and4IR.
  ASan/UBSan0aE5mf:10942/0, intentional UAF witness live, leak detection OFF,
  202572KiB. These are not canonical/bootstrap/installed/CI acceptance.
- Next dependency's independent edit scopes: B existing flow store/internal/
  consumer-navigation frame proof and ready getter; A existing indexed source,
  typed publication/LET initializer receipt and final shallow-retention
  obligation; C existing behavioral/currentness fixtures. Root alone connects
  the final consumer at the existing program-success boundary and runs builds,
  Git and integration. Prefer carrying the obligation in its existing typed
  publication row over a second store; no Symbol pointer survives its scope.
  Assignment revokes initializer provenance monotonically. No ready proof means
  the original refusal, not source0. Shared gate: named independent String
  positive and retained owned-element negative, exact currentness tampering,
  original full strict/broad and unchanged full canonical, then real bootstrap
  and installed C/LLVM. Only one active rung, no new language or owner track.
- Reached public-fact last consumer: semantic parameter summaries publish only
  the existing mask, not the native internal descriptor/element-unproved bits.
  HIR/RIR/MIR carry that mask; AIR copies MIR rows and the planner consumes its
  certificate. Root confirmed the certificate hashes only the AIR row count,
  not the copied row contents. Objective: seal all five existing public row
  fields in current owner order and reject unknown mask bits at MIR validation,
  AIR copy admission and AIR validation. Existing owners/identities stay; no
  reanalysis, new fact family or assertion of AIR-owned native lifetime proof.
  Falsifiers: after successful verification mutate each copied row field and
  require certificate refusal; unknown-bit MIR/AIR input must refuse before use.
  Root alone owns these existing certificate/validator/test files, disjoint
  from A/B/C's source provenance chain. Run the original AIR unit and MIR/AIR
  flow gates, then include this same last-consumer delta in the shared gate.
- AIR pre-fix executable falsifier:147 pass/8 fail on the exact61a production
  sources with only the new tests. After sealing all five fields and adding
  mask guards:156/0 (including incomplete-storage guards), original MIR370/0,
  and AIR flow gate PASS. Native internal descriptor/element bits still do
  not cross that public mask boundary. The ready String-result consumer also
  rejects unknown mask bits; this does not add a new semantic flag.
- Final String-source obligation consumer is the success return of the existing
  `type_check_program`, after its other finalizers, graph/currentness checks,
  scope retirement and capability sealing but before semantic success/MIR.
  Do not insert its error-free ready query into the earlier Array finalizer:
  doing so would short-circuit other existing error collectors and couple
  unrelated private stores. Root replaces the old success return with this
  required final predicate; there is no second acceptance path.

- Frozen native01286244423dd67cbd85a36eed806d2328466cf34fe6f9b34593fc9119660e97:
  safe C/LLVM runtime64/64, phase10747/0 and paired348/unique349 decisions
  PASS. BroadW2x5sx is RED: MIR363/3, semantic3190/38, transpile997/11.
  The whole failure lists are retained in that lab; narrow greens do not
  discharge them. Source units sampled between D and Linux are byte-equal.
- Objective: restore the unchanged successful programs and owned diagnostic
  paths without granting any missing ownership fact. Priority: exact selected
  callable/receiver/formal identity, typed publication lifetime, explicit
  failure, unchanged negatives, then patch size. Existing host-index and
  publication owners remain authoritative; no per-program route or fallback.
- Shared host dependency: the selected nested-callable index covers class/enum
  but not every already-supported host/ability owner. A checks the complete
  producer set and carries its identity to the existing formal mapping. The
  observed world `Pick(4)` refusal is `Selected method argument has no current
  formal identity`, before MIR; fourteen MIR/C checks share host inputs.
  Missing identity must still refuse, and dynamic ability dispatch must not
  choose a convenient implementation to make a header appear complete.
- Defer dependency: the low-level statement checker also accepts non-String
  cleanup outside a routine. B preserves that body/resource-state contract;
  issuing a String future-read or release still requires its real routine
  identity. No fabricated function owner or lifetime receipt is permitted.
- Root checks the remaining transitive ref diagnostics as a single publication
  chain and the datetime/domain-kit return-backing failures. A previous error
  is not permission to emit MIR, infer no effects, or replace a missing fact
  with a default. Do not change the negative oracles to a weaker refusal.
- Isolated local-error publication stageXW3fWk/native4daddcf5 restores27 of
  those38 semantic checks unchanged (3217/11): an unrelated earlier error
  does not erase a subsequently checked expression's identity. A local error
  still prevents that expression's publication; the whole program remains
  rejected and no MIR is emitted. This is not a current integration receipt.
- Remaining datetime/domain-kit wrappers reach an implicit receiver carrying
  input backing. B connects a current, completed, non-mutating and
  return-independent self summary to that exact receiver, the existing
  declared-result edge and final backing replay. Receiver evaluation remains;
  dependent/mutating/opaque receivers remain refused. No argc+1 guess or
  no-store-as-independence is permitted. Reuse the current validation queue,
  not recursively restart the whole backing validator. Existing binding/node
  storage and bitset/epoch charging primitives move unchanged to the existing
  fact-storage owner; semantic policy stays with backing/currentness owners.
- Shared gate: exact full strict + phase mutation groups, all existing
  MIR/semantic/transpile units, safe C/LLVM and defer parity, then unchanged
  full canonical admission. Named-String result-source repair is deferred
  until these integration failures are classified and repaired.
- Integrated KGTpXy/nativefc58a51b builds within caps; Wffqbq observes
  semantic3230/0, MIR370/0 and transpile1012/0. Paired1I8ffm has351 rows,
  352 unique inputs, no admission decision regressions. Phase10805/14 is
  still RED: six new host tests used a pending query after finalization
  (test setup corrected), and eight invalid-program publication controls
  expose the root expression-local error frontier's overreach. Preserve the
  same-routine/invalid-declaration no-grant tests; repair the publication
  boundary, not the oracles. Strict needs the observed extra caller-error
  sites after that boundary is stable. No acceptance or known-red push.
- Publication decision: discard the broad expression-local publication
  experiment and restore the sticky-error no-grant gate unchanged. The
  existing PendingRefCall already issues a typed call/actual/formal tuple;
  carry its exact formal/site identity to its diagnostic-only last consumer.
  That consumer must replay the receipt rather than silently drop a diagnostic
  when the whole-call value was deliberately not published. Actual String
  return-loan/ownership admission retains the stronger current value/call,
  routine and completed summary requirements. A owns this single receipt
  migration. Missing diagnostic identity stays explicit; there is no fallback
  that supplies an ownership fact from a diagnostic snapshot.

#### Reached defer lifetime / final-consumer repair (2026-10-17)

- Objective: retain the existing routine-exit LIFO defer behavior while making
  Array/String lifetime admission and generated binding lifetime agree with it.
  Priority: existing execution contract, exact binding/source identity, explicit
  refusal, preserved safe readers/copies, then patch size. The July21 executable
  delta in `docs/self_hosted/16_hard_substitution_velocity_process.md` explicitly
  selected routine exit (`2,1`) over block exit (`1,2`); do not reverse that
  choice from a code comment or change those runtime oracles to hide a failure.
- Chain: checked defer body -> existing indexed deferred read/release ledger ->
  later assignment/transfer/named-call consumer -> final parameter publication;
  MIR defer registration -> C/LLVM routine-exit emission -> exact deferred local
  binding. The ledger must survive an inner lexical block until routine exit.
  Its rows contain stable AST/binding identities, not a dangling local Symbol.
  C must not lose an admitted inner local merely because the return block has
  a different transient SSA name map. Mutable bindings must observe their
  value at execution, not a registration-time value snapshot.
- Existing owners: indexed String borrow/defer scope and final Array call
  preservation; existing MIR local/SSA binding and defer emission owners. No
  new lexical-exit wire format, AST rescan, public syntax or extra semantic
  owner is justified by this repair. Missing/currentness failures stay closed.
- Falsifiers: inner defer read followed by outer drop must refuse with no MIR;
  inner local plus deferred drop and non-retaining named reader must execute
  correctly on C and LLVM; future deferred drop plus a retaining Array callee
  must refuse. Preserve LIFO/same-body/mutable-rebind negatives and the existing
  branch/loop `2,1` outputs. A verified ledger with no release reservation is
  different from unknown source identity in the presence of a reservation.
- Integration snapshot0NdW7o/nativea248ed4a: phase10653/0 PASS, strict RED
  (an existing invalid move now has an additional call-preservation diagnostic).
  Previous native690 runtime66 checks had62 successes and4 failures: two unsafe
  inner-read-after-drop executions exposed early ledger expiry, and two C
  compiles lost the deferred local name. Do not run the unsafe admissions again.
- Edit boundaries: B existing indexed/defer semantic scope; A C MIR binding and
  defer emission after surveying its owner/consumers; C fixtures/oracles only
  after observed diagnostics. Root alone Linux/build/process/Git and integration.
  Shared gate: currentness phase + full strict/admission census, safe C/LLVM
  runtime controls and existing defer parity, then broad/canonical/bootstrap/CI.
  These are dependencies of the active ownership rung, not new parallel tracks.
- Binding repair decision after surveying the actual MIR: do not add a second
  C mutable-binding store/live-reference cell at every write. The existing SSA
  owner must carry deferred exact-ID reads to routine exits and feed the normal
  backend SSA consumers. The reached prerequisite is HIR's infeasible edge for
  a literal Boolean: semantic already distinguishes that same literal, whereas
  HIR currently leaves both edges and makes an inner local fail to dominate its
  deferred exit use. Remove only the literal's impossible edge at the existing
  CFG producer; do not fold effectful or inferred expressions. Retain routine
  LIFO and dynamic-defer refusal. Carry/validate local identities using existing
  MIR graph/local-ref fields through native and installed consumers; no guessed
  spelling, registration-time value snapshot or additional wire format. Missing
  binding/use facts stay closed. A owns this one producer-to-consumer chain;
  root validates existing HIR/MIR/JSON/installed gates and backend runtime parity.
- Independent review reaches the same call/retirement chain at two additional
  last consumers: host-method OWN actuals and Array RETURN. Root connects the
  existing owned-element and generic ownership consumers to the host route,
  keeping actual ordinal separate from mapped formal ordinal through capture
  and final validation. The selected typed callee/mode is authoritative; do not
  resolve ownership again from a display name. Returning a descriptor with a
  live deferred read/release must consume the same existing lifetime guard as
  an OWN call. Required negatives include use after method OWN, method OWN
  after a reserved release, and an unused/public dangling-return function.
  Test the future-release -> shallow alias -> retaining-call variants too;
  an escaped flag must not erase an already issued release obligation.
- Fixed canonical36G7t5/native0b39 remains RED12512 diagnostics, MIR0,
  22.05s/1277232KiB; many are cascades after the first source failure. Isolated
  diagnostic observerVRtJic (not an acceptance binary) identifies that first
  failure at `SemanticConstructedTypeNameFactFrom`/135:13, an ArrayPush of a
  String returned by a normal canonicalization call. Its input/result backing
  relation is not yet connected to indexed source provenance. Do not mark an
  unknown String result independent or insert a manual source Clone to hide it.

- Objective: returned Array storage and its String payload lifetime reach
  every existing move, mutation, retained-store and final deep-drop consumer.
  Priority is current typed source identity, separate storage/element facts,
  explicit refusal, preserved independent-copy positives, then patch size.
- Existing owners: RETURN/birth/deferred storage receipts; native collection
  element premise; indexed String source publication; demanded parameter
  summary and its single two-facet indexed-root walk. No new semantic owner,
  source annotation, whole-AST pass, cache or safety exception is introduced.
- Reached sinks: named/opaque calls, constructors, shallow Array stores,
  field/inout String stores, tuple and Option/Result payloads, Slot Write and
  assignment sugar, callable-let String capture. A shallow store has no
  non-retaining callee: exact indexed backing cannot cross it without copying.
  List/Set/Queue/Map String insertion and String channels copy payloads at
  runtime and must not inherit a blanket shallow-retention refusal.
- An actual String copy (including checked Concat) is independent. A source
  name, primitive String type, descriptor-only preservation or a matching
  member identity is not independent payload evidence. Destructuring cannot
  erase a retained tuple element's lifetime: its shallow store is checked
  before the tuple is admitted, including intermediate and returned tuples.
- Full paired admission census eukyHA on native a8d01daa:274 inputs,2
  regressions, no unsafe execution. Both are genuine OwnedPush/inout positives
  (recursive Grow and reader-then-Append); the other272 decisions match.
  Scratch bd4MFa identifies raw CALL16 from INOUT as the cause of permanent
  element failure; raw visitor facet0 became2 solely at the coarse mask join.
  All four scratch source/object/binary paths restore byte-identically.
  The bounded repair observes actual payload projection in the same walk,
  retaining direct-formal/unsupported/unknown-call failure. It is not a
  general CALL_ESCAPE exemption. Current compile/runtime gates still pending.
- Independent falsifiers are analyze-only. C owns source/phase/behavioral
  fixtures, A the final deep-drop and Slot consumers, B the summary facets,
  root retained-store hooks/build/Git. No parallel WSL/build/process owners.
  Shared acceptance remains full phase/strict C+LLVM positives and exact
  BORROW/no-MIR negatives, broad/sanitizer gates, then unchanged canonical,
  actual bootstrap/install and required CI on the same published main SHA.
- Reached last-consumer dependency, not a new precision track: COzxKW on
  7cb7f6b2 passed17/20 independent-copy C/LLVM runs. Both backends lacked
  the intermediate tuple local `pair.1` type; LLVM also misidentified the
  tuple-return aggregate layout. The typed tuple issuer, source-local MIR
  consumer and exact C/LLVM destructure consumers are now independently
  verified in tpPcdM: native10fc7542, phase10291/0, all20 runtime runs and
  full strict gate PASS. The stage preserves the original input and budgets,
  and checks clone/missing/stale tuple receipts. It does NOT include the
  later method/defer/retirement integration, validate the current dirty tree,
  establish canonical admission, or replace bootstrap/install/CI evidence.
- Final same-lifetime sink map adds local String return, Slot/SecureSlot plain
  initialization, DeviceWrite, deferred read vs retirement ordering and host
  methods. Root owns return/init; A the existing DeviceWrite and typed method
  actual-to-formal correspondence owners; B existing Scope/defer/indexed
  lifetime and retirement consumers; C behavioral fixtures. Method formal
  indices include self while actual indices do not: one ordinal must not
  serve both. Known method readers require the same admitted correspondence,
  not an opaque escape exemption. Deferred future reads survive typing's
  resource snapshot and remain until the routine exit; LIFO order,
  same-body read/drop and later String rebinds must not erase the obligation.
- Fixed canonical V576EF on 7cb is RED20132 diagnostics (cascades included),
  MIR0,30.45s/1256272KiB. Scratch T7UlqT identifies the first new source
  refusal: ArraySet(values,slot,Substring(...)). Both runtime ABIs duplicate
  Substring; existing checked-local-constructor owner already admits that
  independence, but indexed String origin omitted it. Connect this known
  identity/arity contract, not a fresh heap-free right. After a prior error,
  missing value publication must not generate a second lifetime error for
  every unrelated live String array. Keep compiler failure and the primary
  diagnostic; re-enumerate on unchanged canonical after the focused chain.
- A9Y1Pz on the unchanged7cb binary confirms the remaining sink obligations:
  host-method retention, local String return, Slot initialization,
  DeviceWrite, late deferred read, reversed cleanup order, mutable String
  rebinding, and release followed by another release/use still emit MIR.
  These are unsafe admission observations, NEVER executable runtime cases.
  Safe reader/copy/defer controls are checked separately. SecureSlot Read
  precision and an unrelated destination cleanup currently obscure two
  copy controls; isolate those boundaries before adopting semantic oracles.
  Method actual/formal mapping must include the implicit receiver in the
  existing host-index owner. Deferred release is an affine reservation,
  not immediate physical retirement or a grant from an AST/name match.

### Current integration regression and selector-name review

- Current source `f4279a13b`/native `fb533c58c050e3a808279bf7fa8d08c8812498eab4c21286e04812a2b45baf94`
  is local/unpushed. c1a7 selector-name and de3 cast-error-boundary reviews
  are SCOPED APPROVE. f427 direct MEMBER_MOVE permission revocation passes
  OgzflW phase10060/0/full strict, Ftpl2Z broad and Dk2bjQ SAN(leak OFF),
  but its family is NOT closed: FwMWnp admits both `Extract(State)->rows`
  wrapper negatives through ordinary CALL UNKNOWN birth. Neither was executed.
- Current full census 3QVIvA is RED1761 over unchanged4273 routines/3188 keys,
  versus1047 on de3:797 new,83 resolved,291 changed causes. There are10
  direct Pass2 errors across five field cleanup routines (3+1+3+1+2), then
  1636 stale-return/125 stale-index failures. Restore is BYTE_IDENTICAL.
  Lower wall time is early failure, not a performance or admission receipt.
- Immediate bounded negative-boundary objective: reject an Array result's
  borrowed parameter backing even when its formal is DEFAULT/INOUT, using
  the existing typed function-param return-dependency owner and current
  program-point facts. No new AST walk, owner, syntax or fresh-storage claim.
  Last consumer is the existing final parameter-escape diagnostic. Forbidden
  fallback: RETURN_ESCAPE from a non-own formal silently ignored because
  only REF is checked, or no-store/no-dependency treated as uniqueness.
  Required falsifiers: the two frozen wrapper probes, forwarded/local aliases,
  mixed returns and descriptor growth; preserve fresh factory and owned-return
  positives. This rejects a known borrowed route; it does NOT discharge the
  full returned-storage/field ownership producer set below. Map all new
  refusals before extending this boundary or claiming the canonical restored.
- Return-storage implementation slice (same rung; no field permission):
  the existing deferred Array-storage owner records typed RETURN events and
  exact ordinary CALL births, then seals their ownership after publication.
  Source premises are current literal/verified independent builtin storage or
  an unescaped exclusive local/OWN formal, consumed at return. A result from
  another call remains provisional and depends on that call's certificate.
  Existing indexed/current program points supply the complete RETURN set;
  no separate body traversal or no-store-as-freshness shortcut. Defer,
  capture/worker, retained alias, unsupported source and recursive unproved
  cycles cannot issue a certificate. Unknown is not exclusive.
  Result-pending state has its own path-local flag, preserved by snapshots,
  joins and loops; it is not an input-INOUT preservation receipt. Own, drop,
  deep-drop, mutation and later calls record a final storage obligation.
  Builtin physical allocation, String-element lifetime, and field-root
  uniqueness remain distinct premises. No return certificate for a nominal
  member is implemented in this slice.
  Edit boundaries: A deferred owner/header + typed-return hook; B Symbol and
  path-state snapshot/merge/equality; root birth/assignment/last consumers;
  C tests only after interfaces freeze. Root alone builds and integrates.
  Shared gate: phase including genuine fresh/OWN-relay cleanup positives,
  strict C/LLVM positives and analyze-only aliases, before/after currentness
  faults, then unchanged canonical. Any remaining failure stays OPEN.
- Last-consumer extension, before further grants: bw6v84 built native
  `fe9d580decec76b2388978891cffef38d3189298eba4d5b781012546fb8ac30c`;
  phase10124/11 and strict are RED (test syntax, read-only unclosed births,
  duplicate OwnedPush diagnostics). Review additionally identifies moved
  result bindings, shallow element mutation after birth, and a CALL used
  directly as an own actual. A producer-only element certificate is not
  permission at those consumers. Capture the current local element premise
  and exact dependency CALL at the existing pending-drop/actual snapshot,
  before consumption; finalization checks that consumer identity and its
  result dependency. Deep-drop own-formal requirements carry both storage
  and element obligations. Move keeps the dependency, never converts UNKNOWN
  into ownership. Call/retention effects must revoke local element permission.
  The four unclosed Array-result phase probes must now refuse: a read also
  consumes storage liveness, and there is no admitted borrowed-return
  contract. Keep the birth rejection until read, iteration and implicit
  cleanup consumers are all connected; do not erase it merely to preserve an
  old unsafe positive oracle. No new owner/file,
  field grant, source annotation, body rescan, timeout or memory allowance.
  A owns deferred ledger/header and element-lifetime capture/currentness;
  B owns owned-call requirements and collection deep-drop/move consumers;
  root owns release/builtin hooks, call retention, integration and Git;
  C owns phase/source falsifiers and gate registration. No WSL/build/process
  or Git action is delegated. Shared gate is the same phase/strict matrix,
  with borrowed/moved/shallow-mutated/direct-call negatives (no emitted MIR)
  and actual owned cleanup positives, then canonical/bootstrap/install/CI.
- Observed follow-up boundary: 21cc/fYXjmM is phase10221/1, not green.
  The remaining positive is MapKeys -> DEFAULT read-only Inspect -> move ->
  deep-drop. Blanket withdrawal at every named DEFAULT/INOUT call is too
  coarse; descriptor preservation alone must not replace element evidence.
  Extend the existing demanded parameter entry and its indexed-root transfer
  to two monotone failure bits, descriptor and element, in one traversal.
  Missing/unsupported facts fail both; exact typed ArrayLength and owned
  String push preserve elements, shallow Push/Set/index assignment do not.
  Named calls consume the same callee entry; final payload return/call/channel
  retention remains a separate refusal. No extra root scan, entry family,
  cache, syntax, source annotation, or validation-budget increase.
  B owns existing summary/transfer/currentness/typed builtin facet files;
  A owns existing deferred row and element-consumer integration; root restores
  provisional local authority only for that recorded named-call obligation.
  Existing Inspect/move and recursive inout OwnedPush positives must remain;
  shallow mutation, retained payload and missing/currentness negatives must
  still refuse. C owns behavioral gates. Root alone runs integration.
  Root additionally observes local constructor retention PAzvoT as MIR-admitted
  and connects generic/nongeneric constructor arguments to the existing
  retention boundary. Local Log/StringLength/named-reader positive must pass.
- Published39a CI38860052641 is RED. Codegen-bootstrap executed and failed;
  seed publication and downstream self-host bootstrap/install were skipped.
  No actual bootstrap/install receipt on f427 and no known-red push.

The older checkpoint descriptions below are historical observations, not
pending agent assignments or the current acceptance result.

- Source follow-up `c1a7b43ed2e9cd62e5fab1f56adfdb5fa29bb017` is local,
  unpushed. The selector-name P1 below is repaired, not still assigned:
  QvwRpT before10032/10; uHvKV8 after10032/0+366 MIR/full strict runtime and
  original4096/4097 PASS; VYiF4K366/3228/1012+Map24/58+4IR PASS; xxX5We
  ASan/UBSan10032/0, live UAF oracle, leak OFF191376KiB. Native SHA05453f728b.
  Independent exact-c1a7 review requested; canonical/bootstrap/CI not approved.
- f3isNv oldc3 field-detach observation admits wrong-field, duplicate-local,
  partial-branch and live-root-copy MIR; opaqueHold already refuses. Current
  tTEPKF refuses all five, now locked in the strict source gate. The legitimate
  two-field detach ALSO refuses and remains an unimplemented positive, not a
  passing or expected-negative gate. No unsafe case was compiled or executed.

- Source checkpoint `4e34e2fe5da5466cc65d7416a81b6aa43ddb3167` is local,
  unpushed. APyEZo full4273/3188 is RED1047, versus616:501 new failures,
  70 resolved,448 prior-success mask changes; this is NOT a cost improvement.
  Source Pass2 has exactly3 errors: IndexedStringBorrowStateRetire293526
  own calls at42:55/43:61 after field-to-local extraction and exact field `[]`
  replacement; later MirIntentExecutionHashString has numeric-cast publication
  failure. First two can poison later publication; establish zero Pass2 before
  attributing the other647 stale-return/28 stale-index census failures.
- The complete new dependency is generic field detachment, not a call-site
  annotation or a permission to undo alias refusal. A/B survey the current
  field/local/root identity, replacement, alias and flow ownership; C has one
  source positive/four falsifiers. No detachment grant is approved yet: clearing
  a field alone is not sole ownership, inout root alone is not physical
  uniqueness, and String element facts cannot be silently widened to Int/Bool.
- A separate independent4e34 P1 is missing ordinary MEMBER_ACCESS name currency.
  Existing input claims snapshot/replay `syntax_name` omits member names; same
  receiver/id/Bool type with `value` changed to `other` escapes both the local
  getter and owner-wide replay. Fix the existing owned syntax snapshot, not a
  parallel publication owner: input_claims capture/current and navigation local
  currentness read the same stored member name. Existing String identity/name
  destruction already owns its lifetime. A edits only these2 production files;
  B adds a same-type name-withdrawal/restoration phase falsifier. C source files
  remain frozen. Root alone runs the old-before/new-after and full integration.
- Preserve8 unsafe alias refusals, all old negatives, exact ordinary member
  behavior, unit-enum receipt, D/O/loan and original budgets. Before/after phase,
  strict source/runtime and later canonical evidence are required. Cost gtYdUs
  still hits4096 at117 entries/evaluations, E117/G148/C2287/O1035/D509, all3831
  backing charges epoch1; it is telemetry only, not admission or cache authority.

#### Field-ownership closure plan — surveyed, not an approved permission

- Confirmed P1 in the retained String-only issuer, exact de3/native8f:
  ltTYSD admits both a live aggregate copy and sibling fields sharing one
  Array<String> descriptor (13375/14560 MIR bytes), without executing either.
  Independent review confirms the missing ownership premise. Current
  `array_storage_initialize` turns escaped=false solely from a LIVE
  MEMBER_MOVE row's root/field IDs, which are identity, not alias absence.
  Immediate bounded repair: revoke THIS implicit permission at the existing
  Array initializer; keep the element/move identity facts for their legitimate
  consumers, but they must not mint storage/own authority. No replacement
  grant until the full producer chain below exists. Objective/priority:
  refuse known unsafe transfers first, then restore genuine positive field
  moves using actual proof, preserving the fixed canonical and all positive
  oracles. Last consumer is own/drop; forbidden fallback is accepting a
  syntax-only String move while plain arrays are refused. A edits only the
  existing Array initializer, B adds phase-before/after falsifiers without
  weakening any positive oracle, C adds two analyze-only source negatives.
  Root enumerates all regressions on the fixed inputs and performs the same
  strict/broad/canonical gate. A new failing positive is OPEN work, not success.

- Separate bounded publication repair at parentc1a7: after an earlier error,
  `type_check_expression` intentionally stops value/call publication, but its
  CAST dispatch still tries to publish a checked cast using the unpublished
  operand and emits a misleading internal error. Objective: keep that SAME
  error boundary at the cast issuer, not synthesize missing operand facts.
  Owner: typed expression publication; last consumer: normal diagnostic and
  final program refusal. Forbidden: clearing has_error, publishing guessed
  operands, swallowing the original ownership error or numeric type errors.
  A changes the one guard in type_checker_expr.c; B phase fault cases; C one
  source negative/exact diagnostic gate. Root observes before/after, runs the
  shared strict gate, then rechecks fixed canonical Pass2 errors. This repair
  is diagnostic containment, NOT proof or permission for field detachment.

- Objective/priority: restore unchanged canonical admission without undoing
  alias refusals; semantic identity/actual ownership, fail-closed evidence,
  old-path deletion, negative behavior, then patch size. One executable rung,
  no independent cache/optimization or language-feature track.
- Existing identity owner: `SemanticNominalResultSnapshot/Field`, issued by
  the constructor owner and current-checked by constructor fields/publication.
  Reuse its field declaration IDs, field/argument mapping, types and original
  call identity. This is not storage ownership; do not reinterpret it as one.
- Current chain: ForLocals creates independent sources:Array<Int> and
  invalidated:Array<Bool> literals, pushes scalar elements, and returns one
  State constructor. The verdict caller stores one result, reads ok, and
  passes it either to Retire on failure or to ScanIntoVerdict inout then
  Retire. Reached Scan helpers perform Array element reads/writes, not Array
  descriptor returns/stores/replacement. Other Map carrier operations are
  reached too; a field-specific proof must not pretend the whole root is
  mutation-free. This is a source observation, not an issued permission.
- Missing producer set, in dependency order:
  1. Constructor field transfer from current physical birth/independent Clone,
     last-use or explicit-copy decision, and source retirement. Existing
     constructor handling only revokes the Array local's exclusivity.
  2. Field ownership through normal/handled-error returns to the actual caller
     local. Generic declared-result backing dependence is not uniqueness.
     Root copies, cross-field reuse and opaque retention must withdraw or
     refuse the relevant field authority, even if types and layout match.
  3. Inout field descriptor preservation/replacement/growth/retirement facts
     across the reached call graph and every continuing exit; element-only
     writes preserve storage, while a missing summary is not all-borrowed.
  4. Exact routine/root/field/local/type/current-generation extraction, fresh
     same-field replacement and ordered/all-path completion. Resource-flow
     snapshots/joins must not mint permission from one branch or a deferred
     future assignment. Keep any active view/loan or retained alias visible.
  5. Own/drop/cleanup last consumer checks and consumes the actual ownership
     evidence; MIR and C/LLVM must retain overwrite-before-single-retirement
     behavior. Existing String element-lifetime rows are not plain Array
     storage provenance, and instruction/wire facts are not optional fallback.
- Every producer and consumer above must use an existing authoritative seam
  or a deliberately registered successor, not a second AST query or parallel
  truth. Do not start editing a local reset rule while these issuance and
  lifetime obligations are unknown. Native nominal mapping alone, an inout
  annotation, normal UNKNOWN CALL admission, or manually observed absence of
  copies cannot discharge the list.
- Integration owner/root owns all executable checks; the three agents' last
  edit scopes were selector production, phase faults, and source negatives.
  Those candidates are frozen/committed in c1a7. A new implementation split
  requires the exact storage/result/flow owner interfaces and independent
  scopes above to be fixed first; no implementation grant is delegated now.
- Shared acceptance: preserve phase/currentness and all old positives and
  negatives; admit the legitimate two-field source on C/LLVM while refusing
  all five field negatives; add cross-field, deferred/partial/zero-loop and
  stale-generation cases as their actual producer is implemented. Then zero
  direct canonical Pass2 errors, unchanged full fixed-input census/admission,
  actual bootstrap/fixed point/install and required exact-published-SHA CI.
  Scratch census instrumentation remains observation, not acceptance.

### Blocking review follow-up — Array descriptor alias births

- Independent bbf3 review found tuple destructuring creates Array symbols with
  zero-initialized escaped=false and no source retirement; an own handoff to
  an unseeded callee accepts the shallow descriptor while its original remains.
  Reproduce with analyze-only MIR; never execute or link the unsafe input.
- Soundness takes precedence over the following cost slice. Its nIWyR5
  candidate08c2c37d passes9957 phase checks/366 MIR/full strict including the
  original4096/4097 controls, but that does not close this additional birth path.
- Before editing production, enumerate all variable/parameter/payload/loop/
  destructure/outcome creation and alias-assignment paths, their existing
  storage/fact issuance, and the final own/release consumers. A is doing this
  read-only; B maps phase evidence; C prepares minimal source falsifiers.
  Root alone runs WSL/build/process/Git. No new authority field or owner.
- The required repair must preserve legitimate literal/Clone/MapKeys/owned
  parameter transfers and existing positive/negative gates. A stored alias
  cannot become ownership merely because a calloc default says not escaped;
  a genuine move needs current source identity and retirement, not syntax.
  Complete producer/consumer edits and currentness tests must be fixed here
  after the survey and before implementation. Costs/CI are not yet green.
- Fixed bounded repair after survey: Array `symbol_create_variable` defaults
  escaped=true; the existing initializer explicitly reproduces the admitted
  literal/checked producer/linear move states, while member/index and other
  unproved extracted births remain escaped. Existing typed formal modes retain
  their current borrow/inout behavior; own handoff independently refuses a
  borrowed formal or host field. Tuple storage invalidates an identifier source
  via the existing storage owner; direct fresh tuple components use the same
  initializer. No syntax-only move, new bit/owner or element-ownership grant.
- Required production set: symbol_table.c, array_storage_release_owner.c,
  type_checker_func_decl.c, type_checker_ownership_destructure.c,
  type_checker_expr.c and collection_owned_element_requirement_owner.c.
  Preserve any already-current String member-move/linear-transfer fact rather
  than replacing it with a descriptor copy. A owns these edits and the pending
  one-line loan-query preservation; B phase only; C source/gate only.
- Explicit exclusion, not a safety proof: existing normal CALL-return Array
  UNKNOWN bootstrap admission is retained without widening its set. It has
  real own/deepdrop admission consequences; false escaped is NOT fresh/owned
  evidence. Its missing physical-result producer remains OPEN. The current
  result/header/dependency and owned-String receipts cannot supply that proof.
  Do not label this bounded alias repair whole Array or language memory safety.
- Before6BW27Z on immutablec3 admits7 unsafe MIR-only cases: tuple alias,
  Array/Slice element destructure, match payload, tuple-store original own,
  ordinary field/index extraction. For payload already refuses MOVE_FROM_RELEASED
  at11:21. Fresh literal tuple is only MIR-positive; its existing C Unknown
  initializer-lowering failure is recorded, not silenced or counted as runtime.
- Integration falsifiers: those7 exact old admissions must now refuse with no
  MIR; existing direct-let orders, chains, borrowed formals and assignment
  refusals persist. Positive normal literal/Clone/MapKeys/String move, named
  ref/default/inout/own and existing UNKNOWN factory behavior must remain.
  Run static, phase, strict, broad and SAN, then full fixed census/actual chain.
  Any additional source dependency revises this map before extending edits.
- Final before3AjLgM adds for-payload with an immediate break: it also admits
  unsafe MIR, while the original loop's second iteration had masked the alias
  with MOVE_FROM_RELEASED. Total newly falsified paths8; two-hop is already
  refused. UFRktc on c7279c refuses all10 negatives with exactly one BORROW and
  no MIR, retaining fresh tuple MIR7747. No unsafe negative was executed.
- ikIU88 on nativec7279c85409bc1a534c2b46935f307748a89fe9657f77e058cf33be5dd5b07fb
  passes10008 phase checks/0,366 MIR/0 and full strict source/runtime/unchanged
  4096-4097 controls. Initial Xm84vN's two new-positive failures were invalid
  test inputs: reserved Read spelling and existing REF Clone refusal, identical
  on oldc3 and candidate. Replacing only those test inputs with InspectLength
  and local-source Clone gives identical admitted MIR13649/5765 on both; no
  production safety condition or existing oracle was changed for that repair.
  uDmzYy broad repeats366 MIR/3228 semantic/1012 transpile, Map24 C/LLVM
  positives/58 analyze-only negatives and4IR flows. uDW0O1 separate-build SAN
  passes10008/0 with a live UAF oracle, leak detection OFF, peak191284KiB.
  Full fixed census and actual bootstrap are still pending here. No CI claim.

### Current selector C traversal boundary — bbf3 checkpoint

- Objective/priority: unblock the unchanged full canonical program within the
  existing4096 demand bound. Preserve typed identity, escape/output facets,
  fail-closed currentness and negative gates before reducing repeated work.
  This remains a native-admission prerequisite, not self-host substitution.
- Fixed source is `bbf3fbffe34eecbed10ba5de22fee9b1fa4f28af`, native
  `c3f75cf638ba967429194ebf0a1afe3ee7e4dbecd9db81f8559bac013238596c`.
  Full4NcZFU has4273 routines/3188 keys, zero direct Pass2 errors and616
  failures, all work4096. Actual S1F73p bootstrap remains RED18410/16 before
  gen0. No fixed point, installed-driver or CI acceptance is established.
- Reached diagnostic u675Fj on cold132263/input0: E115/G148/C2475/O894/D464,
  all3833 backing charges at epoch1; exact same-frame repeats0, cross-input
  occurrences2005. C includes185 MEMBER and38 ARRAY_ACCESS visits plus671
  Binary/Unary ancestors, not all necessarily removable. Savings and coverage
  are UNKNOWN until the same-input after run. The full scratch restore was
  BYTE_IDENTICAL. Do not optimize global-pass replay as this demand's cause.
- Owner/chain: current typed publication -> existing input claims and derived
  empty-C receipt -> existing backing C traversal -> required return/output
  finalizers -> unchanged admission -> bootstrap/install/CI. No new fact family,
  field, ledger, cache, ABI, annotation or copy is selected.
- Exact existing traversal: MEMBER visits only its object unless the current
  typed unit-enum selection says to skip it; ARRAY_ACCESS visits receiver then
  index. Neither wrapper emits a C store/call/return itself. Extend the existing
  derived receipt only when those exact child traversals are empty. CALL,
  CAST, unknown nodes and any effectful descendant remain nonempty. This is
  equivalence to old C traversal, not operator purity or backing-origin elision.
- Complete production edit set: consumer_navigation.c derive/currentness;
  backing_dependencies.c wrapper dispatch. Existing claims already capture
  child pointer/ID/ordinal/order and enum selection. Add a bounded current-row
  check there only if the getter contract requires it; do not reconstruct a
  program root or recursively replay the whole AST on each C query. Owner-wide
  claims currentness remains mandatory at its existing integration boundaries.
  D/O, LET/RETURN/CALL, loan traversal, one-origin guard, charging and limits
  remain unchanged. Delete no other owner and add no compatibility route.
- Before edits, resolve current getter versus owner-wide validation guarantees.
  A owns those existing production files only; B owns phase falsifiers only;
  C owns source fixtures and the existing summary smoke gate only. Root owns
  this card, all WSL/build/process/Git and the shared integration gate.
- Falsifiers: typed member/index over empty children omits actual C row work;
  effectful member receiver and array index retain their calls and refusals;
  child pointer/ID/order/type/value or enum-selection withdrawal fails closed
  at its owning validation boundary, restoration revalidates. Existing loans,
  D/O, ordinary calls/casts, releases and original4096/4097 controls stay intact.
- Acceptance: capture old-native before probes; run phase and strict C/LLVM
  positives plus analyze-only negatives, then same fixed cold trace/full census
  with no new failure/cause/success-mask regression. Measure actual charged
  work and wall/memory without changing input or allowances. If this does not
  unblock admission, report the next reached operation rather than claiming
  all616 failures fixed. Actual unchanged bootstrap/install/CI still required.

### Reached first-pass cost survey after cd5 (read-only, not a permission)

- Objective/priority: unblock unchanged canonical admission within the original
  4096 demand bound, preserving typed identity, all escape/output facets and
  refusal behavior before reducing work. Bootstrap/install/CI remain RED.
- Frozen source is cd5 plus navigation-only7bf; native is
  `e7f4111dabbb36d63771c457f19a4e5054f99ba6fdd66c048c907eb10a8f10f9`.
  LCaaSV's complete3188-key census leaves622 work failures:23 with evaluations
  above entries,578 equal and21 below. A worklist for repeated evaluation alone
  is therefore not the current whole-set plan.
- Diagnostic-only cold receipt8LktUI uses the same4273-routine full program and
  `SemanticAstEnumPayloadStateBeforeUseInNode`334504,input2. It reaches4096
  with90 entries/90 evaluations: E90/G280/C2459/O853/D414, major frames at
  epoch1. Full source/build/bin/modes/Git restore is BYTE_IDENTICAL; no MIR or
  successful admission is claimed.
- Reached chain: typed routine publication -> existing no-output/no-store graph
  and return-backing classification -> summary producer -> required C/D/O
  backing frame -> final currentness/output consumers -> canonical admission.
  `SemanticAstEnumPayloadApplyNodeToEnd`333843 pays128 backing visits per
  input while returning a two-field Bool/Int struct. Publication currently
  recognizes only primitive physical scalars. This is a candidate missing
  descriptor-free result fact, not proof that its whole frame is unnecessary.
- Unknowns before any edit: existing typed nominal field/enum/tuple owner and
  currentness; descriptor-free result versus input/side-effect/output contracts;
  exact no-store graph conditions; affected failed-key coverage; String,
  recursive/generic/handle and unresolved types must remain conservative.
- Parallel read-only scopes: A traces the existing type/publication owner and
  consumers; B maps falsifiers and dangerous predicate reuses; C classifies the
  full remaining census and observation coverage. Root alone owns WSL/build/
  process/Git. No production/test source edits are authorized by this survey.
- Forbidden: widening global `publication_physical_scalar`, treating COPY_ONLY
  as backing-free, rescanning program roots, erasing CALL/CHANNEL/inout/store
  effects, partial dependency replay, a new cache/ledger/SoT family, increased
  allowance, annotations/copies, or a green count as canonical acceptance.
- Integration boundary for a subsequently justified bounded change: before/
  after behavioral and stale-fact phase controls, unchanged budget falsifiers,
  strict summary/C+LLVM parity, full fixed census with no regressions, then
  actual unchanged bootstrap/install and exact-SHA CI. Root must first fix the
  complete owner/consumer edit set and removal obligations in this card.

### Array storage path-state repair — implementation boundary

- Objective: correct the existing flow owner's fallthrough storage authority,
  not expand plain-result permissions. Priority is sound path identity and
  complete join/replay before cost. The physical-scalar predicate is unchanged.
- Measured first failure: QbxT4O/vk4c05 isolate the same full334504,input2
  demand; the first failing no-store member is123657
  `SemanticCallSpineViewFromGraph`. Its first mutable site is123725 ArrayPush,
  before ArraySet. Receipt3UnqwI observes that exact call's local literal
  `reversed`: parameter0/host0/exclusive0/escaped1/pending0, current Array<Int>/
  Int, no prior error. All4096 charged events equal8LktUI byte-for-byte; each
  scratch run restores source/build/bin/modes/Git BYTE_IDENTICAL.
- Cause: a preceding terminating if-arm returns a constructor containing
  `reversed`; constructor correctly invalidates that arm's storage authority.
  Existing FlowFlags exclude terminating arms from fallthrough. Existing
  ResourceConsumeSnapshot omits Array exclusive/escaped/pending fields, so the
  Symbol mutation survives restoration. Array bindings already belong to its
  existing constructed-type inventory; no classifier change is needed.
  The published private-store issuer correctly refuses the now-poisoned state.
- Fact owner/lifetime: existing resource-flow snapshots and function-local
  declaration indices, from branch/loop entry through the actual reachable
  join. Add the three existing Array Symbol facts as one typed snapshot state;
  no new semantic owner/family, wire, AST rescan or private-store exception.
- Required production set: ResourceConsumeSnapshot declaration;
  allocation/grow/copy/failure/destroy; raw and stable-index
  restoration; alternative/parallel merge; equality and availability comparison.
  Exclusive is AND; escaped/pending are OR at joins. Conditions remain before
  the baseline; return is excluded only by existing FlowFlags. Break/continue,
  loops, defer, transaction/compensation and private whole-loop summary reuse
  must carry the same fields through their existing copy/restore/merge paths.
  The pending-call sequence owner is never erased or rewound. These transient
  storage fields confer no new backend cleanup or ArrayDrop authority. The
  original resource/loop inventory is unchanged: HIR/MIR/RIR validate names
  and unique indices, and RIR associates
  resource authority only with its existing Slot/Qubit/Future resource kinds.
  The three new transient bits are not exported, like existing Future lifecycle
  state; no schema/kind/filter is added. Verify mixed Array/Slot identities and
  all four IR projections. Keep existing Array formal identities and resource
  parameter ordinals unchanged. Missing new state cannot restore positive
  storage authority; its affected Array is conservatively nonexclusive/escaped.
- Last consumers: unchanged ArrayDrop/collection and private ArrayPush issuer,
  then claims/no-store/backing output currentness, canonical admission and the
  actual bootstrap/install/CI chain. Remove the old omission in every snapshot
  path; do not leave a separate Array-only branch pass or guessed fallback.
- Parallel edits: A owns only the production flow snapshot/merge/lifecycle/
  equality files; B owns phase behavioral snapshot and publication regressions;
  C owns source fixtures and the existing summary smoke gate. Root owns this
  plan, all WSL/build/process/Git, before/after evidence and integration.
- Falsifiers: early terminating arm with transfer followed by a local operation
  (positive); live fallthrough alias/condition escape/other reachable arm
  (negative); break/continue and loop-summary replay; pending inout must not be
  erased; snapshot copy/restore/merge/missing-column behavior; unchanged Hold
  before/after and cached private receipt withdrawal. Existing release, String,
  loan, C/D/O,4096/4097 and both-backend controls remain unchanged.
- Acceptance order: frozen before phase/source falsifiers, native phase and
  strict source C/LLVM/no-MIR negatives, flow/resource/MIR broad gates, full
  unchanged census and actual bootstrap. This repairs one producer; remaining
  ArraySet/String-store and return-layout obligations stay OPEN until measured.
  No claim that this slice alone removes622 failures or makes CI green.
- Before7wyxrn: e7 with E70721F4 phase yields9355 checks/1 failure, solely
  private_push. Full scratch restore BYTE_IDENTICAL. The outer runner's last
  phrase grep used a space instead of the emitted hyphen and returned1; the
  actual phase verdict and sole failure were read directly, not called PASS.
  Source-before vuuvQ5: public early-return ArrayDrop wrongly refuses BORROW at
  9:15/MIR0; nested private push compiles/runs C+LLVM4. Retained fallthrough and
  retaining-condition controls refuse exact BORROW at11:15 and10:15/MIR0;
  neither negative was executed.
- Candidate native0d7cf2a from OgoDSX compiles the5 frozen flow files. Public
  early-return drop now runs C/LLVM0,1 and nested private push C/LLVM4
  (9uGaI7); mixed Array parameter/local Array/Slot input runs C/LLVM6,6
  (eGGWRP). This is NOT slice acceptance:9uGaI7's retained-fallthrough
  negative is newly reachable and emits MIR. It was never executed.
- P1 blocking last consumer: dodcvn analyzes minimal cases with both original
  e7 and candidate0d7. Both admit foreign Void DEFAULT/ref Array calls and a
  local Void forwarder to that foreign call followed by ArrayDrop. A real
  read-only local Void helper remains admitted; own handoff then drop already
  refuses MOVE_FROM_RELEASED at5:15. Thus early-return poisoning had hidden
  an independent final release-authority gap: Void/scalar result shape is not
  no-retention evidence. Do not remove the negative or publish this candidate.
  Survey the existing array-storage call owner -> deferred preservation ->
  release/own consumers before extending the implementation. Any extension
  must preserve legitimate named read-only helpers and the original inout
  contract, fail closed on opaque calls, and never erase the pending ledger.

### P1 completion of the same release-authority chain

- Before choosing a broader permission, correct the survey: Array is already
  BORROW_TRACKED through the existing constructed-type classifier. The added
  non-parameter classification shortcut is redundant and must be removed.
  Preserve the original tracked binding/identity inventory; carry the missing
  three path bits for those existing Array rows, including already tracked
  formals. Do not introduce new parameter ordinals, types or backend fields.
- Actual security boundary: C by-value Array arguments copy a descriptor with
  an aliased data pointer, not independent storage. 0Yjw5y confirms both e7 and
  0d admit MapKeys -> foreign Void Keep -> ArrayDropOwnedStrings as well as the
  plain-release counterexamples. All probes are analyze-only. Existing own
  consumption still refuses later release. Named read-only helpers on Int and
  owned String arrays are required positives, not exceptions to remove.
- Owner decision: existing array-storage call admission preserves current
  authority only from an exact selected named body and its existing descriptor
  preservation facet, never from Void/scalar result shape. Extend the existing
  delayed call/sequence store for REF/DEFAULT only when preserving actual
  authority: current exclusive named binding, or a local exact LIVE
  MAP_KEYS_SNAPSHOT/OWNED_ELEMENTS fact with the same function/binding IDs.
  Borrowed/unknown facts grant nothing. Existing inout issuance and own
  consumption remain; resource-bearing result guards stay conservative.
- Unproved foreign/opaque/indirect calls revoke both exclusive storage and
  possible-retention permission (`escaped`), so plain release, owned-element
  release and later own forwarding all see the same loss. A proved/deferred
  named read-only call preserves its existing authority, not a fresh grant.
- Complete additional production edit set: array_storage_release_owner.c;
  array_storage_deferred_preservation_owner.c/.h (exact call/callee/argument/
  binding identities and current replay, same sequence lifetime); existing
  function_param_flow_preserves_array_storage query; collection owned-string
  release's pending-consumer hook. Share the existing pending release record
  for plain/deep cleanup with an explicit operation identity for diagnostics;
  no new ledger, family, fallback or generic policy wrapper. Keep successful
  MapKeys cleanup even though its plain `exclusive` flag is initially false.
- Internal solving may read its existing provisional preservation facet.
  Outside solving, the release consumer requires COMPLETE/current output,
  exact entry identity, supported analysis and proved preservation. A rejected
  demand with an old false `array_storage_unproved` bit is never permission.
  The existing4096/4097 boundary controls are not altered; the cap remains4096.
- Do not rewind/drop delayed rows to guess branch reachability. The existing
  ledger can conservatively reject some path-correlated cases; this is OPEN
  precision, not permission to erase an unproved call. Track source order and
  generation/currentness to the last plain/deep/own consumer.
- Acceptance/falsifiers: direct foreign DEFAULT/ref, a named forwarder to
  foreign, retaining condition and owned-String deep cleanup must refuse
  without MIR; source-before unsafe admissions are never executed. Preserve
  named read-only Void helpers (including forward declarations), independent
  owned-result cleanup, original inout/own positives/negatives, stale receipt
  revocation, full path-state phase controls, unchanged work-budget tests,
  broad native/IR/runtime gates, then full fixed canonical/bootstrap/install/CI.
- Delegation remains disjoint: A production in the named owners only; B phase
  protocol/fault tests; C source positives/negatives and existing smoke gate;
  root integration/build/process/Git/docs. No new optimization work starts
  while these discovered release-authority negatives remain open.
- vPNSRY phase9405/1 observes count10: Array formals were already tracked,
  confirming the classifier correction above. Its only remaining negative is
  a continue path. Existing loop owner merges continue into the backedge, but
  omits it from a dynamic while's condition-false exit and from finite for exits
  except the special one-iteration case. The new snapshot restoration exposes
  that loss of authority. Add `type_checker_flow_loops.c` to the same A scope:
  reachable continue states also contribute to non-static-true while exits and
  finite for exits, using the existing identity-checked merge; static-true
  while does not invent a normal exit. Use a real terminating falsifier (set a
  local condition false before continue), plus a finite for continuation; do
  not rely solely on a constant-true infinite body's unreachable later store.
  Keep break, condition effects, cached loop replay and original budgets.
- Frozen candidate c3d868cc: native phase kIiRSd passes9487/0 and the complete
  existing strict summary/source/runtime/4096-4097/caps gate. Its two previous
  failures in6ZdYoz were exactly output-ready withdrawal(fault3), on both plain
  and deep release. Array is not an anchored handle; outside the solver its
  authority consumer now requires the existing ready output seal as well as
  COMPLETE/current. General anchored-handle currentness and solver-internal
  provisional behavior are unchanged. Hold-before-push now correctly has no
  private candidate after the opaque call; Hold-after keeps its candidate.
  Both still require graph refusal and CALL_ESCAPE.
- bVgMT4 on the preceding d704 candidate: named forward/Void readers run C and
  LLVM1; foreign DEFAULT/ref, named foreign forward and owned-String retention
  all refuse BORROW with no MIR at6:15,6:15,10:15,8:27. Unsafe negatives were
  never executed. These are narrow receipts, not canonical/bootstrap/CI.
- Namespace probe q1cVYQ is not a new positive-regression claim: both old e7 and
  new d704 refuse the Array-reading Tools call as Undefined Tools before MIR.
  No selected-call producer flag or namespace route is changed in this slice.
- Last-consumer review is part of P1, not a new precision track. Analyze-only
  aQ1NDc on c3d reproduces two missing consumers: a named DEFAULT forwarder to
  foreign Keep followed by an own call with no drop seed emits13077 MIR bytes;
  MapKeys -> the same unproved forwarder -> owned binding move -> deep drop
  emits17384. A ref forwarder is already refused by the separate borrow gate,
  so it cannot falsify these boundaries. Neither unsafe program was executed.
- Complete the existing decision chain before acceptance: own-argument
  requirements must consume their captured pending result regardless of a
  callee's drop-seed bits; the authoritative owned-String binding-move producer
  must record a pending authority consumption before retiring its source fact.
  Reuse the existing ordered consumer store with an explicit move diagnostic
  operation; no alias ledger, transferred-call identity rewrite, new owner or
  blanket restriction on descriptor reads. Actual binding/current call IDs
  and sequence still identify the original evidence. This avoids dropping the
  obligation when the destination receives a new binding ID. Preserve a proved
  named reader followed by either own consumption or a legitimate owned move.
  Add collection_owned_element_requirement_owner.c to A's existing production
  scope; B keeps phase protocol/fault tests and C the matching source pairs.
  Re-run the entire frozen narrow/broad boundary after these two consumers.
- xYPejF on f5: the own-unseeded and moved-owned-String negatives now refuse
  BORROW at17:17 and14:17 with no MIR; paired C/LLVM readers preserve1,1 and1.
  FV646Y passes9529/0 and the complete strict gate. Earlier c3 broad EQqxYO
  passes MIR334/semantic3228/transpile1012,Map24/58 and4IR; c3 actual bootstrap
  mlWH3L still refuses before gen0 with18785 errors/16warnings, versus e7's
  18410. Do not count stricter rejection or narrow greens as canonical progress.
- Final f5 broad ADhxLu repeats the complete strict gate with both last-consumer
  exact-site checks, MIR334/semantic3228/transpile1012,Map24/58 and4IR. Native
  phase remains9529/0. The same source's sanitizer and full fixed canonical
  census still need their own observed receipts; no CI acceptance is claimed.
- Read-only full consumer map distinguishes unrelated policies: ArrayPush
  relocates backing, Pop only changes length, Set/Sort/Reverse write in place;
  they are not one invalidation operation. The privileged internal retire
  checks its caller registry/own signature and consumes storage, but does not
  directly consume the public exclusive/escaped/pending predicate. Its caller
  invariant and foreign-retention -> growth safety are OPEN source findings,
  not observed executable vulnerabilities or an approved new implementation
  track. Public release/own/move repair does not establish these contracts or
  whole-language memory safety. Preserve the current canonical finish line.

### P1 integration regression — descriptor preservation versus result shape

- Full unchanged uyyZgj on366/f5 is RED:3188 keys/4273 routines,1785 failed
  (1660 incomplete/stale return relation,125 stale program-point index),1302
  newly failed versus LCaaSV's622. Its9.33s is NOT a speed win. Earlier Pass2
  errors suppress later checked publications. No current admission/CI claim.
- Diagnostic-only SXid8g preserves the same verdict and restores source/build/
  binaries/modes/Git BYTE_IDENTICAL. Complete Pass2 error attribution starts at
  SemanticAstSignatureTypeExpressionAppend32965: recursive inout Array<String>
  followed by OwnedPush is marked escaped. Next FactContractReady35049 deep
  drops arrays after a source helper returns independent aggregate facts.
- Correct the producer decision, not the negative gate: descriptor nonretention,
  existing release authority and result backing are separate facts. Unknown/
  extern/indirect/constructor calls still revoke possible-retention authority.
  A current exact named descriptor proof preserves the prior escaped state;
  nonplain results still revoke plain-release exclusivity as before. Do not
  turn descriptor preservation into an element-ownership or public-drop grant.
- Complete required edit set behind existing owners: release call issuer accepts
  the existing String inout descriptor boundary as well as plain elements and
  carries an existing pending same-binding proof through consecutive named
  borrows; its result-shape guard remains separate. The exact stdlib descriptor
  transfer list admits only receiver ordinal0 of ArrayPushOwnedString in addition
  to its existing ArrayPush/Set/Pop/Reverse rules. OwnedString push consumes its
  pending receiver proof at the recorded sequence through the same ordered store
  with an explicit push operation. Public plain/deep/own/move consumers and
  their currentness remain intact. No new ledger/family/ABI/annotation/copy.
- A owns that production set (existing release/deferred owners, stdlib collection
  transfer and owned-String member-transfer owner); B phase currentness and actual
  last-consumer withdrawal; C source positive/negative pairs. Root alone owns
  scratch observation/build/process/Git and the shared integration boundary.
- Falsifiers: recursive String inout then owned push; independent nonplain named
  result then deep cleanup; retaining named inout then caller owned push without
  any drop (must still reject); current call/argument/binding/summary withdrawal,
  shadowed builtin, mixed/borrowed elements and all earlier release/own/move
  negatives. Require the original strict/broad/SAN/full fixed census gates and
  actual bootstrap. A scalar-only Count struct is not a nonplain-result falsifier.
- Unknown: the named aggregate-result callee may need an existing output seal
  before descriptor preservation is authoritative. Observe its real producer/
  state/ready first if it fails; do not bypass COMPLETE/current/ready or add a
  separate descriptor cache to force this positive. Restore all diagnostic edits.
- Consecutive pending named calls are not new authority. The later call itself
  must consume its earlier same-binding sequence prefix at finalization; its
  own preservation row cannot conceal a failed predecessor merely because no
  drop follows. Reuse those existing call rows and the same currentness replay,
  diagnose at the later argument, and keep unsupported first calls conservative.

### P1 loop recheck — acknowledge the same current receipt, never reissue authority

- Objective/order: restore unchanged canonical analysis after the path-state
  repair, without weakening any loop-state comparison or release check.
  Native2778c164/8n7XQ8 still has1776 failed cold keys. Scratch aOSI3A records
  the complete reached replay set:262 call-effect reissuances and55 binding-fact
  reissuances, all exact tuple matches; every binding fact is LIVE. Zero tuple
  drift or RETIRED binding matches were observed. Source/build/binaries/modes/
  Git restore BYTE_IDENTICAL. This is diagnostic evidence, not admission.
- Existing fact owner: collection_ownership_fact.c, reached from repeated loop
  body typing after a storage pending bit changes. Last consumers remain HIR/
  MIR collection-transition admission. Preserve their operation/binding identity.
  The raw AST setter stays write-once. Its semantic issuer may acknowledge the
  same kind/receiver/source only after current typing, binding and disposition
  checks. Binding append may acknowledge the same seven fields only when LIVE.
  A changed tuple, stale binding or retired binding must still fail; do not
  overwrite a receipt, resurrect a retired fact, or create a second authority.
- DROP keeps its existing issuance order: owned-local facts have already retired
  at effect publication, while the admitted empty-literal path remains LIVE.
  Do not manufacture one disposition rule that changes either contract.
- Edit scopes: A existing production owner; B phase replay/mismatch/retirement
  checks; C executable loop-recheck positive and second-iteration escape
  falsifiers. Root alone builds and runs the shared strict/broad/full-input gate.
  No new owner, cache, loop-bit exclusion, allowance increase or manual clone.
- Falsifiers: repeated identical LIVE binding and effect succeeds once; changed
  receiver/source/effect or a retired binding fails. Execute outer String-array
  and loop-local fresh-binding cases on C/LLVM, retain delayed escape rejection,
  and preserve independent4097 timeout/work boundaries. Then rerun all3188 cold
  keys/4273 routines and actual unchanged bootstrap; narrow green is not closure.
- Open: repeated typing also appends ordered deferred rows. No safety bypass is
  established, but a later prefix can conservatively include another analysis
  pass's rows. Classify remaining full-input refusals after this repair; do not
  rewind sequence numbers or delete rows speculatively.
- rQHbeR/native76ba5065 passes9649 phase checks and the original complete strict
  budget gate, including4097 within30s. Outer-loop recheck executes C/LLVM2.
  Loop-local fresh birth now reaches MIR but fails branch-join; its exact DEF
  lifetime is a reached last-consumer obligation, not a reason to hide the case.
- Complete tKT0b8 Pass2 set is now9 rows rather than261: seven escaped pushes,
  one escaped deep drop and one downstream numeric publication failure. The
  first three sites borrow an inout String-array formal through a named reader;
  another borrows a named factory result. Borrow eligibility still wrongly
  requires an owned release authority. Correct the same issuer to require a
  current typed named, unconsumed, non-host, nonescaped binding for descriptor
  proof; do not require or grant its exclusive/element release authority.
  The existing deferred ledger/currentness and last consumers remain mandatory.
  Unknown/foreign/indirect/constructor calls still revoke; invalid prior prefixes
  still fail. Parameter read-then-push and factory-read-then-cleanup are the
  paired positives; retaining reader variants must refuse without MIR.

### P1 reached MIR lifetime — an empty local is born at its exact DEF

- Objective/priority: carry the same lexical birth/transition evidence through
  the actual last MIR consumer, preserving use-after-drop and forged-receipt
  refusal. No new syntax, wire row, semantic owner or per-program route.
- Owner/chain: semantic empty-literal fact -> existing LET DEF population
  (source/binding ID, expr0 initializer, declared-local marker) -> existing MIR
  source-local type -> mir_branch_collection_transitions.c. It currently starts
  every empty-literal binding at routine entry and ignores its DEF. An exact
  body-local birth/drop therefore joins fictitious EMPTY with RETIRED at the
  loop header. OEhmI7 is the unchanged falsifier; do not remove or shrink it.
- Required replacement: keep possible inactive/EMPTY/BORROWED/OWNED/RETIRED
  states at CFG joins; begin inactive, and activate only an exact current empty
  literal DEF whose binding/source/origin IDs and Array<String> local type join.
  Push/drop must be legal for every incoming possibility. A fresh DEF may not
  reset a live prior generation. Remove the unconditional routine-entry EMPTY
  authority and immediate mixed-state rejection, not safety checking itself.
- Reuse the existing canonical-birth shape predicate if its invariant fits;
  do not add a sibling state owner. Preserve MapKeys/Clone/move admission and
  terminal rules. Keep unchanged size caps; cohesive replacement can remove
  the old enum join branches rather than append another validation path.
- A production: only the reached MIR owner after freezing the release issuer;
  B existing MIR tests for birth deletion/move, live-overwrite and after-drop
  use; C the frozen loop-local C/LLVM source oracle. Root alone runs the exact
  MIR artifact, roundtrip, strict/broad and unchanged full bootstrap boundary.
  Capture the pre-fix case before edits. Unknown: imported MIR preserves these
  DEF facts; test that boundary explicitly before claiming closure.

### Current integration receipt and remaining independent-review boundary

- Native b410bc0b / V1keGC passes9649 phase checks,366 MIR tests and strict
  summary/runtime/original4096-4097 budgets. Before upTlNL had337 pass/14 fail
  with only the new MIR falsifiers added. AvT1YG executes the unchanged local
  loop on C/LLVM1/1 and outer loop2; its delayed escape stays two BORROW/no MIR.
  Final CEAB812B gate is included in broad FCfekx: MIR366, semantic3228,
  transpile1012, Map24 C/LLVM positives/58 analysis negatives and4IR PASS.
  SAN cMOlU2 passes9649/0 with a live UAF oracle, leak detection explicitly OFF.
- Full unchanged Ylpvi5 on452201b9 before only the MIR fix: zero direct Pass2
  errors, all4273 signatures/3188 cold keys;622->616 failures, all unchanged4096
  work cap, zero new failures and zero prior-success mask changes.3:37.15,
  peak1252780KiB, full scratch restoration BYTE_IDENTICAL. No timing win claim.
  Actual b410 bootstrap8puFIY is RED before gen0:18410 errors/16 warnings,
  first recursive-summary4096 failure followed by conservative escape cascade.
  This is not admission, installed-driver evidence or CI closure.
- Serialized-MIR scope is OPEN, not a demonstrated vulnerability: its existing
  reader checks local_ref/DEF/source kind/type/empty graph, while native checks
  current AST pointer/origin relationships too. The wire has no expression
  stable-ID or AST-pointer field. Do not claim the native proof is byte-for-byte
  reproduced by imported MIR or start a new wire track without a reached rung.
- Independent366 review is still OPEN. 5PzMlH reproduces alias before/after Leak
  crossing unseeded own with MIR14415/14435, never executed. Capture/validate the
  own-consuming call/formal identity next, preserve the existing extern/static-
  method/generic call classes, then fix alias versus proved linear transfer.

### P1 independent review closure — own-call currency and alias transfer

- Objective/order: close the two reproduced366 review gaps without granting
  ownership from a shallow alias, weakening retention rules, or adding source
  annotations/copies. Parent315e4d7bf/nativeb410 is the verified checkpoint;
  its whole bootstrap remains RED. Root alone owns builds/process/Git.
- Current ownership/consumer: helpers_late records each own Array actual in
  collection_owned_element_requirement_owner.c before call consumption. Its
  finalizer runs after typed publication but snapshots only the actual/target
  index, so an unchanged actual can hide a changed consuming call or formal.
- Complete fix in that existing owner: capture call pointer/ID, selected decl
  pointer/ID, ordinal, exact formal pointer/ID/type-node identity and issued
  actual Type. At finalization check bounds, current call/selected declaration/
  actual identity, current typed argument/value, target function/formal join,
  OWN mode and current formal type-node identity before any seed/pending test.
  Keep extern and static-method selected declarations admitted as before;
  checked_argument owns the call target/argument/selection replay. Do not use
  IDENTIFIER-only direct_callee or body-summary currency as a signature proxy.
  Generic instantiated actual Type is compared to its current actual value,
  never directly to the raw Array<T> formal. No new query API or host-index scan.
- Alias producer fix in array_storage_release_owner.c and ownership_let: run
  storage initialization after the existing collection transfer issuer, so a
  proved String-element move has consumed its source. A bare identifier that
  has not actually been consumed remains a possible descriptor alias; mark
  that destination escaped rather than silently clear its prior obligations.
  Preserve prior source escape even for a proved move. The ordered pending
  move use is still recorded before source retirement. No pending row rewind.
- Unknown/limit: no native plain-Array last-use move issuer was found. Do not
  invent one from syntax position, silently clone or tell users to annotate.
  Measure any real canonical rejection after this conservative authority fix;
  investigate the responsible producer before extending the contract.
- Edit scopes: A those three existing production files; B existing phase tests
  for consuming call ID/argument pointer/selected decl/formal ID, plus mode,
  type and binding currency (prior reader/actual unchanged for call faults);
  C source alias-before/after-Leak negatives and existing proved String move/
  direct-own positives. Raw function-variable calls are already unsupported;
  do not broaden that unrelated contract. Root runs original strict/broad/SAN,
  the full4273/3188 census and actual bootstrap before a published-green claim.
  A correct foreign owner is not released by a name/ID-only compatibility read.
- Candidate c3f75cf6 passes heJNoe9723/0 and strict original4096-4097 gate,
  MIR366/0; broad Sa1fpc repeats366/3228/1012, Map24/58 and4IR. SAN DsAdB1
  passes9723/0, live UAF oracle, leak detection OFF. kUsBTc on oldb410 admits
  nine of the new faults; its same-context diagnostic dedup is not a semantic
  failure. Final tests isolate each of ten faults, retain failure plus a new
  owned diagnostic and verify restored success in the same store.
  pwBO0s refuses both official alias orders at exact BORROW21:17/no MIR;
  QBzXVs preserves extern analysis and class/generic C/LLVM1. Unsafe probes0.
- Full4NcZFU on c3: all4273/3188, zero Pass2 errors;616 failures, no newly
  failed/resolved/changed-cause or prior-success mask changes versus Ylpvi5.
  Wall3:22.45/1252752KiB; full scratch restoration BYTE_IDENTICAL. Actual
  bootstrapS1F73p remains RED before gen0,18410errors/16warnings, original
  first4096 failure. Required CI/installed acceptance is still OPEN.
- Diagnostic u675Fj is regenerated from current owners, retaining the stronger
  COMPLETE/output/currentness query (old cost32 overlays must not be reused).
  Cold132263/0 charges E115/G148/C2475/O894/D464 at original4096;115 entries
  and evaluations, all3833 backing visits epoch1, exact same-frame repeats0,
  cross-input occurrences2005. C types: CALL509, LET470, RETURN468, BINARY437,
  UNARY234, MEMBER185, ARRAY_ACCESS38. This rules out local/global replay as
  the reached first cause for this cold key, not for every other key. Transparent
  selector C traversal is being mapped read-only; no cache or skip is approved.

### Current checkpoint and reached zero-argument result premise

P1 safety priority before the remaining cost work: independent c143 review
found required callee frame deletion bypassing backing_rows_current. Before
receipt `.tmp/p0-required-frame-before.3q9mSz/` has8651 checks/4 failures:
the caller remains current/precise after either its required source row is
NULL or the entire callee vector is removed and count reset to0. Direct
output_dependency already refuses both; the cached caller is the missing last
consumer. Fix the existing validator, not the oracle or solver construction.
The first guard candidate e64eb1a9 is REJECTED: WVVTnK has47 existing phase
failures because unrequested callback/unsupported ordinals can legitimately
have no frame. Typed requiredness alone does not claim an issued proof.
Outside the solver, replay the existing summary entries that actually issued
OUTPUT_BACKING_FRAME: exact decl/ID plus ordinal requires the same index's
vector and row to exist. An issued producer cannot become unmaterialized by
deleting its vector. Unissued NULL rows remain legal. Use existing store
entries/identity, no new fact family, flag, schema or owner. Preserve the
non-NULL incomplete/pending/epoch checks and all recursive construction. Root runs
phase/strict summary/caps and broad gates, then sends the exact fix for review.
Fix commit870b28e94c68d498f26dda9b5032045a8c1c3e9a, native85c5d569:
4dyy4J passes8651/0 plus strict summary/paired-runtime/negative/caps;
GxGwiQ passes MIR334/semantic3228/transpile1012,Map24/58 and4IR. Full fixed
census jCNA6p completed721 failures/0 new, with BYTE_IDENTICAL restoration.
Exact870 review found the additional withdrawal gap below; it is not approved.

The870 review found a further P1 premise: a caller must remember the exact
ready witness it used, not infer its past existence by scanning current
OUTPUT_BACKING_FRAME markers. Marker/state/output-ready/unproved withdrawal
must revoke caller permission even if the frame remains. Replace that global
presence inference with the existing exact call/callee/ordinal relation
vocabulary: physical D reuse records its earlier required ordinal in the
caller frame's relation list; pending witnesses reuse their existing tuple,
ready witnesses add the same sealed relation without setting pending.
Existing final relation replay then validates COMPLETE/ready/producer/frame
and all current identities; include unsupported withdrawal. No new semantic
family, wire field, parent/child cache architecture or blanket typed-required
scan. Preserve solver pending construction and old String/raw/callback paths.
Scopes: A existing dependencies/store owners only; B adds the4 source-entry
withdrawal faults to the existing witnessed fixture and preserves row/vector
and recursive tuple faults; C's private Int fixtures remain a separate edit
scope on this same rung. Root runs before/after phase, strict/broad/full-input
and actual bootstrap; commit870's approval is still OPEN. The speculative
marker-based guard is retired when the persistent relation takes ownership.

Integrated39dc3ecb passes sed77P phase8834/0, strict summary,9FKcAT CLLVM
`1\n4\n` and broad6YdHva, but full9Mm3ap is RED:793 failures (695work,
98 recursive-relation),80 new/8 resolved vs721. All7 old required-return
failures disappear (6 resolved,1 now work-limited); that does not excuse the
new relation regression. No commit/push of this candidate.
All98 failed relation keys were re-demanded cold on the same full4273-routine
input in sKAFxn;98/98 report pending=1,count2,sealed1 (and analogous partial
seals),0 without the mismatch. Source/build/bin/Git/modes restore byte-identically.
Minimal mixed Select-ready -> Recur-pending phase6ENDAc has8836checks/1failure.
The missing transition is existing callee_unfinished setting pending without
reopening the earlier ready seal. Set sealed_pending_relation_count=0 whenever
that producer introduces a pending relation, including an existing tuple.
Do not weaken the validator to accept partial seals. The existing completion
owner must replay all relations and atomically reseal the full count. Then
repeat the minimal case, old withdrawal faults, broad/full/actual bootstrap.

Final producer-reset candidate native385c2e3e passes D9iQew phase8849/0,
strict summary including C/LLVM `1\n4\n`, exact Keep refusals, old controls
and caps. Full QHnR78 checks all3188 keys/4273 unchanged routines:713 failures,
all work4096, incomplete0;8 resolved/0 new vs jCNA6p721. The98 mixed-relation
regressions disappear. Wall2:50.75, peak1249168KiB, full BYTE_IDENTICAL restore.
This is narrow safety/regression evidence, not canonical admission. Broad
FxGJu6 passes MIR334/semantic3228/transpile1012,Map24/58,4IR and strict/runtime
summary. Actual bootstrap1TyubV still has18410errors/16warnings before gen0.
Local checkpoint247b2c54944f27166f771667beb138ac6b578935 commits the3 production
owners plus phase/gate/source controls (8files); exact247 review was requested.
Exact247 independent static review completed without a blocking finding and
judged the earlier P1 closed; supplied Linux receipts were not rerun and this
does not approve bootstrap/install/CI or whole-chain closure.
No budget, canonical Pergyra source or owner identity changed; no safety
validator was weakened.

Warm AEWNVY on this exact385c source restores BYTE_IDENTICAL. First failure is
still request254 ResultCallReturnType127572,input3:4096=E69/G74/B3953,
C3037/D443/O473, physical D queries0. ReadTypeFact103229 inputs0 and3 each
charge533 (E1 plus266 in each of two epochs). Next observation is the exact
binding growth after a read that demands those epochs; do not choose a
matrix/cache/skip or remove a charge before resolving that Unknown. The
existing dependencies/store owners and currentness consumers retain authority.

### Reached physical Array element destination — bounded closure card

- Objective: remove a false storage alias for a typed physical element read,
  not its eager reads/effects, to unblock the same canonical bootstrap demand.
- Evidence: exact247/native385c, unchanged4273-routine input. Warm nj3cQJ
  preserves all4096 charges and restores BYTE_IDENTICAL. In both reached
  ReadTypeFact inputs0/3, D on `i = cursor_out[0]` (node103291,binding103237)
  grows destination0->8 after an earlier read, demanding a second266-node
  epoch. The value is Int, not a retained array descriptor. Origin tracking
  does not grow at that site; do not disable the general growth rule.
- Priority: current typed identity and one owner, eager effect preservation,
  currentness/negative behavior, then measured cost. Existing physical-value
  publication and input claims own the type; the existing backing destination
  traversal consumes it. Final output/currentness/loan consumers keep authority.
- Proposed boundary, to verify against source before implementation: a physical
  Array element with a current simple identifier receiver and literal/identifier
  physical index has no backing destination. Require matching current Array
  element/result types. No calls or composite receiver/index can be hidden by
  this rule. All other projections keep the existing general dependency path.
  C still visits both children; all node/child IDs, type and physical claims
  must revoke cached permission if withdrawn. Slot, descriptor, nominal and
  nested-array results are not physical values.
- Forbidden: blanket physical subtree/call skipping, dropping eager C visits,
  a new fact family/cache/schema, cross-origin mask reuse, changing convergence
  or work charging, annotation/copy/manual-drop workarounds, cap/input changes.
- Complete change set: A verifies and edits only existing backing_dependencies;
  B adds direct typed/currentness and preserved descriptor cases to the existing
  phase unit; C adds minimal loop positive plus effectful index/receiver negatives
  to the existing source gate. Root owns navigation/builds/integration/Git.
  All lanes share one objective, not independent implementation rungs.
- Gates: old baseline demonstrates the false D bits/second epoch; narrow phase
  and strict source/runtime gate, old alias/backward/recursive/4096-4097 controls,
  broad Map/IR/native suites, same3188-key census (no new failure), same warm
  trace, actual unchanged bootstrap, then original fixed-point/install/CI chain.
  Physical-flag/type/child identity withdrawal, descriptor or Slot payload,
  and hidden call effects are the falsifiers. If a typed premise cannot be
  validated with existing publication/claims, stop this proposed edit and
  record the missing owner fact rather than infer it from syntax.

Before-source sTxN2w on247/native385c observes the loop positive admitted and
C/LLVM output `7\n`; both effectful projection variants reject with exactly2
BORROW/no-MIR diagnostics. The first phase attempt lFOLjn is NOT the intended
falsifier:2 failures are typed prerequisites (reserved function name Read and
an invalid borrowed nested-descriptor binding). Those fixtures were corrected
without changing production or weakening safety. hQ9CrQ on the same385c now
has8948checks/2failures, both the physical scalar local's false nonzero D bits;
all other new premises/descriptor refusal/eager child/currentness checks pass.
Production B4F95665 is26 added lines/587 noncomment LOC in the existing owner.
Candidate narrow verification was started; no acceptance claim from the before run.
The first build AgqJpb found a missing direct declaration include and is not
a pass. Adding existing type_checker_internal.h fixes that compile dependency
without changing the predicate. Final source855E24D8 is588/599 noncomment LOC;
phase8A24BE82 and source gate84EE8C17 are frozen. Candidate3OYt3n nativeb92168a5
passes phase8948/0, strict paired C/LLVM7, descriptor/effect negatives,
old String/opaque/recursive/4096-4097 controls and caps. Broad BjuKNW also passes
MIR334/semantic3228/transpile1012,Map24/58 and4IR. Same-input full census
1egAQH completes all3188/4273:713 failures,0 new/0 resolved and0 mask changes
among previously nonfailed keys vs247; incomplete0. Wall2:54.20,1249056KiB,
BYTE_IDENTICAL restore. Warm GwNFEg proves the intended532-unit saving:
ReadTypeFact inputs0/3 each533->267 and one epoch instead of two. The same
request254 nevertheless remains work4096, now at ExprType129563,input2;
E76/G74/B3946, C3069/D435/O442. Diagnostic source/build/bin restore exactly.
Actual bootstrap C38avJ still fails before gen0 with18410errors/16warnings.
No claim of canonical admission, installed driver or CI relief follows.
Local checkpoint98549707b41d5c05612d27c46940fa2f00c1f480(parent247) commits
the6 bounded production/test paths. Only the2 navigation files remain dirty.
Exact985 independent static review completed without a blocking finding. It
did not rerun supplied Linux receipts or approve bootstrap/install/CI. No
known-red push.

Next observation only (not permission for a producer/consumer edit): current
nativeb921's first warm demand still spends3069 charges in C. Classify the
charged binary/unary nodes by current result/operand type, leaf shape,
operator identity and loan-seed participation. A possible bounded certificate
is an ordinary built-in primitive operation over only current literal or
identifier leaves, with no executable child, write or backing transfer. Its
actual count and the existing typed owner's guarantee are Unknown. Prove
those premises and measured savings before changing C; a physical result
alone is insufficient. Calls, projections, Slot/nominal/function values,
overloaded/state-changing operators and loan-seed paths cannot be inferred
pure. Keep per-origin verdicts and general eager traversal; do not revive
the rejected joint/matrix scheduling or delete charges for work still done.
Root owns the one shared scratch build; A prepares observation only, B checks
the reached typed operator contract read-only, C checks falsifiers read-only.

Observation vAp6HM (no source admission): all26369 charged events are byte
identical to GwNFEg, and the first request still fails at4096. Of817 binary/
unary C visits,192 have physical leaf shapes; only1 has an existing built-in-
only contract. The other191 require selected-operator evidence because the
type checker can select overloads for canonical primitive types. This tiny
elision is REJECTED; do not add a new operator contract for a one-unit claim.
There are947 charged C control visits over450 unique routine/node pairs, not
947 guaranteed savings. Existing input claims seal current syntax and typed
facts but do not issue C execution order, root-internal stopping or loan
context. Three read-only reviews reject treating claims DFS as that frontier
or simply exempting repeated visits from work. No frontier implementation.

Next observation remains behind the existing no-store/D owners: can an actual
current no_store_graph with a no-output header make D destination aliases
irrelevant in a formal backing frame while preserving C/O and eager effects?
Do not use evaluate.graph_closed alone: its graph_required short circuit can
coexist with inout parameters. Explicit current graph/header, private stores,
local replacement, return/wrapper origins, pending relations and cache
withdrawal must be resolved before choosing any production edit. Loan and
declared-result D paths are not permission to generalize this proposal.

The three no-store/D reviews found a concrete last consumer: LET/ASSIGN D
validates zero-argument declared results and records result_callee_id/type for
backing_rows_current. O/C alone do not replace that validation. Blanket D
elision is REJECTED. No production edit was made for it.

### Current implementation card — owner-produced C control navigation

- Goal/order: remove the measured repeated control-wrapper preparation behind
  the existing program-point owner, preserving semantic identity, source
  currency, per-input admission and eager data effects before cost savings.
- Evidence/input:985/nativeb921, unchanged4273 routines/3188 keys; first warm
  demand254 hits4096 with947 C control charges for450 routine/node identities.
  This count is an upper bound, not promised savings or admission.
- Owner: the existing input-claims snapshot/program-point index produces an
  ordered navigation projection from its current control edges. Claims keep
  syntax/typed/reachability authority. No new semantic family or general cache.
- Complete change set: one immutable heap-owned sequence with original root
  ranges, IDs and readiness in the existing index; producer/currentness and
  destruction; normal formal and declared-result root consumers. Non-control
  expressions/statements remain under the existing C/D/O evaluators. Loan
  analysis retains its distinct seed/consumer-stack traversal, not a fallback.
- Meaning: expand only BLOCK, IF(condition/then/else), WHILE(condition/body),
  FOR(range-start/range-end/iterable/body), WITH(body), DEFER(body); BREAK and
  CONTINUE add no data operation. A non-control node is emitted unchanged and
  its subtree is not flattened. An explicitly unreachable control ancestor
  emits no descendant. Missing reachability is never guessed false.
- Preserve original top-level root boundaries: complete=false stops the next
  root, not siblings within a root; analysis_unsupported stops immediately.
  Keep data-node epoch/identity dedup, origins, outputs, pending witnesses,
  call-retention and result-currentness unchanged. No requested ordinal may
  activate an unrequested ordinal or inherit another ordinal's permission.
- Production must piggyback the already required snapshot, not rescan the
  whole AST/body or replay every claim per origin. Count actual removed node
  allocation/lookup/preparation; never leave the same C wrapper visit in place
  and merely exempt its charge. A shared AST DAG, completed raw snapshot or
  index-array relocation must not lose/reorder data nodes or grow storage
  quadratically. A missing/stale projection fails closed; no old/new fallback.
- Scopes: A owns existing production index/claims/header/destructor and the
  two normal root consumers; B existing phase unit; C existing source gate
  and bounded fixtures. Root alone owns builds, integration and Git.
- Falsifiers: unreachable ancestor currency withdrawal; root-internal failure
  versus next-root stop; unsupported callback tail; shared-node DAG and cycle;
  nested loops/break/continue with binding growth; reverse input demand; loan
  seed/escape site; declared-result return_seen; missing projection identity,
  pointer/count/range/readiness. Existing D result edges must remain.
- Gates/order: focused before/after phase, strict source C/LLVM runtime and
  negatives, unchanged4096/4097, broad native/Map/IR, full3188-key census with
  no newly failed key, same warm observer and actual bootstrap. If projection
  construction has new unbounded work, changes verdict/effects or cannot be
  validated in the existing owner, reject it rather than expand scope or caps.
  No canonical/bootstrap/install/CI closure or known-red push is authorized by
  a faster narrow receipt.

First executable receipts: JVn7iw before has8956 checks/1 failure for7 actual
control-only backing rows; all other new effects/output/epoch/currentness
assertions pass. Harness's final descriptive grep does not match CHECK text,
so its rc1 is not a separate product failure; raw phase exit1/that one CHECK
is the observed falsifier. Complete source/build/bin/modes/Git restore is
BYTE_IDENTICAL. Source-before1dwWNi on nativeb921 observes C/LLVM6 and4 for the
two positives and exactly2 BORROW sites(21,19),(26,15), no MIR for the negative.
First cut TCMhya native6f81944a passes phase9095/0, the strict/runtime/source
gate, old4096/4097 and size caps. This is narrow evidence only. Production is
6 existing files; final distribution claims599/index595/deps587/store584/
fact265, without a new owner file or arbitrary line compression. Root's API
review then found NULL snapshot input could write root_offsets[1] on a
zero-root allocation. The producer now rejects NULL/mismatched body identity
(claims460B980E); B extends missing-input and root/edge faults. Rebuild/retest
that guarded source before the full canonical/warm/bootstrap boundary.

Guarded rebuild fTIvda native31128358 compiles and stays inside caps. Phase9132
has1 failure: the added body-wrapper test assumes a PublishedNode execution
row exists after source typecheck, but that producer does not issue one.
This is an invalid new test prerequisite, not proof of a production regression.
B replaces it with the existing pending raw-owner fixture, which can legally
publish body=false and statement-root=true before finalize. No silent test
skip, reopened finalized store or production safety weakening is permitted.

Final phase9DE20BF2 uses that legal pending owner fixture. ryiHsw guarded
native31128358 passes9164/0 and the complete strict summary/runtime/negative/
4096-4097/caps gate; broad0suNfM passes MIR334/semantic3228/transpile1012,
Map24/58 and4IR. Full qpB5oL completes4273 routines/3188 keys:645 failures,
68 resolved/0 newly failed/0 previously nonfailed mask changes against1egAQH.
Every remaining refusal is work4096; incomplete0. Wall3:02.16,
peak1249008KiB, source/build/bin/modes/Git restore BYTE_IDENTICAL. This is
partial fixed-input cost relief, not canonical admission. Warm HVINn7 still
fails request254 at4096: E104/G78/B3914, C2786/D581/O547. It reaches
ScalarUtilityReturnType128691,input4,epoch2 rather than the old ExprType first
pass; ReadTypeFact0/3 each267->211. Complete scratch restoration is
BYTE_IDENTICAL. Actual u5q5uG bootstrap remains RED before gen0 with unchanged
18410errors/16warnings. Separate-build ASan/UBSan VgzmuE passes9164/0 and the
intentional UAF witness proves the detector is live; leak detection is OFF.
Native SHA256 is
`311283587cee1a8df22b58d9b55c3713604c31c50865782ad6b7c04bea3432d9`.
Local checkpoint80bab8e5bc384134d01d82b2bfaa5493b97809dc preserves the13 scoped
paths. Exact80bab independent static review found no blocking defect; it did
not rerun Linux receipts or approve canonical/install/CI. No known-red push.
Self-host substitution is BLOCKED at the native admission boundary: the
existing function-param backing owner still reaches work4096 on the fixed
canonical input; its last consumers are cleanup admission and bootstrap gen0.
No new SoT row or replacement count is claimed from this C prerequisite.

Next measurement only: the reached frames now repeat in a second global
pending pass. Existing pending relations retain call/callee/ordinal identity,
not all actually consumed masks/output relations, and omit some ready edges.
Record actual D/O/C read facets, output relation results and solver states
without a new query or charge; compare the previous read with the next-pass
snapshot. A frame-end snapshot cannot stand in for the earlier read value.
Do not treat UNSEEN/COMPUTING as a current completed target or seal a pending
equation from equal numeric masks. The existing stationary-group completion
owner remains the last authority; declared-result frames have a separate
completion path and are outside this possible computation-reuse boundary.
Observer jh1jIL preserves all22463 warm charge events byte-for-byte and restores
the full scratch BYTE_IDENTICAL. ScalarUtility input4's second-pass PRE finds
ExprType targets still COMPUTING, with producer0->2/output shape0->5 and
pending/unproved transitions. Equal consumed mask bits are insufficient;
the pending computation shortcut is REJECTED. No new dependency ledger.

Next observation only: the same failed demand still pays1033 C binary and23
C unary wrapper charges. Existing C does not query their selected operator;
it visits children. An owner-produced empty C frontier is a traversal-
equivalence candidate, not a purity or overload-safety claim. Preserve every
effectful data child at its original C invocation point, all D/O and LET/CALL
interleaving, and the separate loan traversal. Before any implementation,
measure the actual empty-frontier upper bound and fix the complete typed/
identity/currentness contract, DAG/semantic-promotion boundaries and negative
gates. No flat body rewrite, cross-frame permission or budget change.

### Active bounded C-empty slice — measured, one shared acceptance chain

- Objective/order: remove repeated per-input C-only Binary/Unary wrapper rows
  and their leaf walks; preserve current identity, exact old-C traversal and
  all transfer/effect/currentness behavior before cost. This does not prove
  operators pure or fix any pre-existing overload-analysis obligation.
- Owner/chain: sealed typed publication -> existing claims snapshot -> child
  completion/ordinal evidence -> empty-C judgment -> ordinary formal and
  declared-result C consumers -> existing final claims/backing currentness.
  D/O, LET and CALL interleaving, result edges and pending completion stay put.
- Scope: Number/String/Bool and current identifier leaves; Binary/Unary only
  when all required children are present and empty. Every other shape is
  nonempty, including CALL/CAST/member/array/control nodes. No special
  unreachable shortcut. Raw syntax is not a semantic receipt; promotion must
  finish children before derivation. Consumer requires normal non-loan mode
  and the original one-origin frame width. A missing required current fact
  cannot become an empty permission.
- Linear storage/replay: preserve snapshot's existing child_at ordinals rather
  than searching every child during currentness or duplicating frontiers per
  subtree. Replay each ordinal's pointer/ID/completion and the same derivation.
  Shared DAGs reuse completed children; active cycles reject. Heap-owned claims
  survive index relocation. Measure the extra edge storage on the fixed input.
- Placement if measurement justifies it: consolidate the existing control
  navigation append/executor/currentness and the new derived judgment into one
  responsibility-named compilation unit under the same logical claims/index
  owner. Move existing functions, not owner identity or authority; add no SoT
  family. This avoids both >600-line files and fragmented tiny owner files.
  Claims retain snapshot/promotion, backing_dependencies the C last consumer,
  fact_snapshot storage retirement, and Makefile the single unit registration.
- Three edit scopes after root authorizes implementation: A production/header/
  Makefile only; B existing direct phase unit only; C existing source gate and
  two positive/negative fixtures only. Root alone builds, reviews, commits and
  publishes. One shared gate: before falsifier -> strict phase/source C/LLVM/
  negative/4096-4097/caps -> broad -> unchanged full3188-key census -> warm and
  actual bootstrap -> original fixed-point/install/CI acceptance.
- Falsifiers: actual C-only row removal rather than waived work; nested CALL
  remains visited; width/raw/promotion and identity/type/meta/binding/token
  withdrawal; child ordinal/pointer/count/completion and derived-flag faults;
  missing child, DAG/cycle/relocation, reverse inputs, loan/unsupported/root
  boundaries and all existing D/result/pending controls. No canonical source
  edits, annotation/copy/manual drops, larger allowance or source text scans.
- Measurement YUo54d preserves every22463 charge byte-for-byte and restores
  source/build/bin/modes/Git BYTE_IDENTICAL. Warm254 has2786 actual C charges;
  among1056 Binary/Unary wrappers,700 have the bounded empty-frontier shape
  (699 Binary/1 Unary),258 distinct routine/node identities,0 unknown/cycle.
  This is an upper bound, not700 promised saved charges or source admission.
  Existing claims must issue and replay the semantic receipt before use.
- Implementation is authorized within the above three scopes. Shared interface:
  child claim ordinals and consumer_empty remain in the existing claim carrier;
  use one classifier for snapshot issuance and currentness replay. A normal
  C shortcut has to preserve the original one-origin width refusal, and loans
  never consume this receipt. The extra edge storage's actual memory/cost is
  still Unknown until the same full-input candidate run, not an optimization
  success claim. No pending-equation replay or unrelated shape extension.

Before UkUjxx on unchanged311 observes9171 phase checks/1 failure: the new
old-API oracle finds exactly1 Binary and1 Unary per-input C-only row; effects,
output and currentness assertions pass. Full scratch restoration is
BYTE_IDENTICAL. The runner's trailing label used lowercase `c-empty` while
the observed label is `C-empty`, so its rc1 is not an additional product
failure; the raw phase exit1 and that one CHECK are the falsifier. Source-
before yghSn5 observes the positive MIR admission and C/LLVM `7\n`, plus exact
two BORROW/no-MIR errors at(9,57),(15,15) for the negative. The negative was
never executed. Candidate j0wNqc builds/caps PASS, nativee7f4111d; phase9339/6
finds a new test interface error: it expects the Binary/Unary-only bounded
getter to succeed on a leaf/CALL/CAST. Production is unchanged. Corrected
phase5C0FC28F checks every derived carrier and explicitly refuses the getter's
nonwrapper inputs. Final07NHAw passes9350/0 and the complete strict source/
runtime/negative/4096-4097/caps gate on the same native. Broad9HzQAP passes
MIR334/semantic3228/transpile1012,Map24/58 and4IR. Local checkpoint
cd5efe4d291ea6a5527bfc1a4159cfc0ef0215b6 preserves all12 scoped paths.
Full LCaaSV completes all3188 keys/4273 unchanged routines:645->622 failures,
23 resolved/0 newly failed/0 previously nonfailed mask changes,incomplete0.
All remaining refusals are work4096. Wall3:22.96, peak1249824KiB, full
source/build/bin/modes/Git restoration BYTE_IDENTICAL. Prior qpB5oL was
3:02.16/1249008KiB: do not claim a wall-time improvement.

Warm mhYGZh on exact e7 remains request254 ResultCallReturnType127572,input3
at4096, but reaches OptionCallReturnType129252,input0,epoch2. Charges
E133/G78/C2534/D690/O661, entries492/evaluations581. ReadTypeFact0/3 now each
execute210 in each of two global passes,422 total. This is newly reached
pending replay, not the earlier physical-element local-growth bug returning.
Full scratch restoration is BYTE_IDENTICAL. Actual bootstrap86juHM remains
RED before gen0:18410errors/16warnings, unchanged. Separate-build ASan/UBSan
NZlqlb passes9350/0 with a live UAF witness and leak detection OFF. Exact cd5
independent static review found no blocking defect; it did not execute the
Linux receipts or approve canonical/install/CI/whole-language memory safety.
No known-red push. Native e7 is still a C prerequisite, not Pergyra substitution.

Whole remaining-set guard before the next edit: of622 failed cold keys,
23 have evaluations>entries,578 equal and21 fewer. Local backing epochs are
UNMEASURED, so this is not proof every equal-count key is single-pass, but
it rejects a whole-scope plan based solely on ReadTypeFact's second global
pass. Read-only worklist review also finds missing reader identities, zero
reads, O-cache ancestry, callee/output/ready/result-edge facets and persistent
consumer ordering. Existing epoch stamps/pending tuples cannot justify
selective replay. No production worklist/ledger/cache/matrix is authorized.
Observe the actual E/G/D/O/C and local epochs of an equal-count failed cold
demand on the same full semantic program before choosing a new producer
contract. Preserve all original charges, order,4096 and fail-closed outcomes.

D main `c1435d62664144ddd60573bc05d51a35f1e97781` (parent67aee05af)
commits the physical-actual repair below; it is local, not published. Production
Linux overlay binary is dff3f869049933053ee794900b0f2ae7e40f8b84b20a45a77964c3479e6b4d2f;
Linux Git remains39a and must not be reported as the D revision. Only navigation
documents are dirty. c143 independent review is pending;67's namespace delta
was approved without rerunning supplied receipts.

Acceptance observed: m68ymx phase8639/0 and strict summary/paired runtime/
negative controls; dozwQg native MIR334/semantic3228/transpile1012, Map24/58
and4IR PASS. Full unchanged4273/3188 census oqClaW has721 failures (714work4096,
7incomplete_required_return),41 resolved/0 new vs5532. Bootstrap NmTeXN still
refuses before gen0 with18410 errors/16 warnings. Warm ditGpm removes every
physical D query but request254 still reaches4096; this is not closure.
All scratch overlays restore source/build/bin/modes/Git BYTE_IDENTICAL.

The next falsifier is the common first completion loss for all7 incomplete
keys, recorded by XSpgzN: MirExpressionGraphSequenceError id448163,input0,
local initializer MirExpressionGraphSequenceEmpty(), call448169, D lane.
MirExpressionGraphSequenceAppend id448557,input0 propagates that failure to
the7 requested keys. Empty has zero arguments and a nominal result. No body
loss appears before the outer D failure; the failing declared-result premise
is not yet known. The source program is frozen; do not choose a copy, manual
drop, annotation, solver exception or work allowance from this observation.

- Objective/order: identify the exact missing producer/currentness premise;
  preserve selected identity, complete body/eager effects and negative gates;
  repair that owner and its existing consumers, then retry the full chain.
- Owner/last consumer: existing typed declared-result publication ->
  declared_result_closed -> backing destination/origin -> final backing_rows_current
  and complete_return_relations -> canonical/bootstrap/installed-driver.
- Current change set: scratch-only stage diagnostics in those reached owners;
  phase/source lanes map the minimal behavioral falsifier. Production changes
  are not decided until that observation. Root owns the only shared build,
  WSL processes and Git. No parallel implementation track is opened.
- Forbidden: name-only factory freshness, zero-argument automatic approval,
  weakening return/currentness checks, treating dependency facts as unique
  storage/drop authority, or enlarging4096/reducing the canonical input.
- Gates: before/after minimal source+phase falsifiers, strict summary,
  broad native/IR checks, unchanged3188-key census and real bootstrap; then
  installed C/LLVM/gen2==gen3 and required same-SHA CI, all still OPEN.

### Bounded private physical-array store premise

After the P1 checkpoint, the same7 incomplete keys reach the exact closure
Empty448147 -> ArenaEmpty92478 -> IdentityRowsUnknown91729. lStm9v confirms
the first2 members are current/nonmutating and the third has mutable_output=1.
Its5 local Array<Int> pushes have typed literal origins and no publication
under the current String-only private-store predicate. No-store failure is
observed before declared-result body evaluation, not a uniqueness verdict.
Before dzPyo5 rejects the minimal nested Int factory and both Keep-before/
Keep-after variants with BORROW/no HIR; no unsafe artifact was run.

- Objective/order: issue the missing local-only store premise, preserve all
  actual effects/currentness/retention refusals, then replay the full canonical
  and real bootstrap. No new memory policy, owner family or layout.
- Producer and query: existing function_param_publication_owner.c, same
  private_array_push fact. Admit the existing String case unchanged or an
  exact physical-scalar element through publication_physical_scalar. Join
  Array element, actual payload, receiver, literal initializer and resolved
  annotation types. Require the same exclusive, unescaped, nonpending local
  storage and syntax/currentness evidence. A missing primitive-array premise
  fails closed; do not infer exclusivity from a nominal result or type alone.
  String-specific string_array_type premises/negative controls remain required
  for String; generic typed value_type owns the new physical-array type.
- Consumers: existing index claim replay -> private no-store graph -> backing
  store/origin/consumer propagation -> declared-result closure -> final rows /
  summary -> canonical/bootstrap. Preserve eager payload evaluation; a scalar
  result does not excuse a nested retaining call. Do not approve Array<Slot>,
  Array<nominal>, Array<Array<T>> or a type mismatch as physical. This premise
  grants neither unique result fields nor physical storage birth/drop authority.
- Retire only the reached String-only restriction for physical element types;
  no compatibility read, fallback, new fact bit, descriptor field or raw pointer.
- Scope A: that existing production owner only; stop/revisit this map if a new
  producer or ownership dependency is required. B: existing phase unit, nested
  Int factory plus stale type/ID/annotation/exclusivity/pending/escape faults.
  C:3 source fixtures in the existing summary gate; root observes runtime
  outputs before fixing the oracle. Root alone builds, executes and integrates.
- Acceptance/falsifiers: minimal before/after, old String fault matrix,
  new Int typed/private proof and final currentness; foreign Keep before/after
  push and nested scalar retention stay analyze-only refusals. Then caps,
  strict summary/paired CLLVM, broad native/IR, unchanged3188-key census and
  actual bootstrap.4096/4097 and the full input are unchanged. Shared closure
  evidence, not file/fixture counts, decides whether this slice is complete.

### Result-relation checkpoint and next measured blocker

Source `72b21cad6adfdf1326bed2b57492472483fcea85` implements the complete
result-relation sub-slice below. Binary32cd6c26 passed narrow phase8060/0,
strict summary, the new multi-exit Option factory on C+LLVM and existing
Pool/opaque/4096/4097 controls. Independent-review direct edge faults brought
phase to8086/0 (`.tmp/p0-result-review-faults.0TtftO/`) without changing native
binary. Broad `.tmp/p0-typed-retention-broad.eCC7j0/` passes native
MIR327/semantic3228/transpile1009, Map24/58 and HIR/RIR/MIR/AIR propagation.
Full frozen4273/3188 census `.tmp/p0-identifier-origin-census.BGdqW8/` resolves
all5 required-backing keys:538 ->533,0 new failures, no MIR, BYTE_IDENTICAL
restore. Actual bootstrap `.tmp/p0-return-bootstrap.6gDheu/` still has18410
errors/16 warnings before gen0. No installed-driver or required green CI.

The failed cold keys are now all4096 work-limit failures, but this diagnostic
does not enumerate all semantic refusals:913 nonfailed keys have mask ALL and
916 reach unsupported summaries. Do not relabel533 as the full DRV-2 error set
or the only remaining safety obligation.

Bounded measurement objective: identify the actual repeated owned operation
blocking the next full canonical admission, keeping existing identity,
generation, negative gates and4096 limit. Baseline is exact72b21 source /
32cd6c26 executable and complete frozen `.tmp/p0-fixed-canonical.nbPIex/` input.
The first isolated failing key is
`SemanticExpressionGraphResolvedCallConcreteScalarTypeName`, id132263,param0,
103 entries/evaluations, work4096,0.094 seconds in BGdqW8. Instrument only the
entry-evaluation, no-store-member and backing D/O/C charge sites for one cold
demand; record routine/formal/epoch/node identity and actual counter. Root owns
scratch overlay/build/byte-identical restoration. Before/after charge counts
must balance; no amount is waived. The last consumers remain final summary /
loan admission and actual full bootstrap. No implementation choice is made
until this measurement identifies a repeated operation. In particular,
PendingBodyCall alone has no ordinal/backing dependency and cannot authorize
ownership admission; unrelated prewarm, generic cache architecture, a bigger
budget and the previously rejected shared-matrix trials are forbidden.

Measurement completed without changing production. Frozen observer receipt
`.tmp/p0-cost32.UIYFCx/` balances all4096 charges:103 entries,139 graph members,
3854 backing visits (C2951/O525/D378), plus exactly one denied charge. Repeated
same-frame/epoch/node charges are0;2264 cross-formal repeats do not prove that
their different origins can share a computed answer. Source/build/bin/modes
and Git status/diff were restored byte-identically.

The causal receipt `.tmp/p0-cost32-causal.9kQN4C/` contains176 nested
SUMMARY/OUTPUT observations; interval-union additional work is1, with one new
entry. All138 exact-call-member/already-admitted observations add0 work. The
stricter private-closed/member/admitted guard matches0 events. Therefore do not
implement the proposed consumer summary-query shortcut: it cannot repair this
measured blocker. The next bounded survey enumerates the existing current
typed publication CALL rows marked unmodelled and joins the actual backing
frame's unsupported_relation call identity. This distinguishes missing result
premises from repeated traversal before any contract choice. It does not
rescan program roots, grant result freshness from input borrowing, or modify
admission. Root alone runs the scratch observer and restores the full overlay.

### Reached typed String result premises — whole-chain slice

Diagnostic `.tmp/p0-publication-unmodelled.oTqzs2/` inspected all47434 current
typed CALL rows on the same4273-routine frozen input and restored everything
byte-identically.1241 rows are marked unmodelled;118 already have exact local
MapNew premises. Remaining families are ToString717, Substring323,
Slice<String>52, Split23/StringSplit1, StringReplace4, TextBuilderFinish2 and
ArraySort1. This is a publication inventory, not reachability or refusal count.
The isolated failing demand has60 unsupported frames whose direct leaves join
exactly to2 Substring,2 ToString and1 StringReplace call identities. Do not
replace that evidence with a single first-failure name.

- Objective/order: close those exact native input-to-result dependency gaps,
  preserve currentness, nested effects and negatives, then measure the complete
  fixed input and actual bootstrap. It is not a new ownership algorithm.
- Owner: existing typed publication checked_local_owner_constructor. Current
  call/result/argument snapshots own identity, kind, stdlib selection, types,
  arity and binding. Existing checked input-retention rows own only synchronous
  borrowing for Substring/StringReplace. For ToString only the existing physical
  scalar premise is admitted: the String overload returns its input unchanged.
  LLVM Bool conversion can return static text, so input-independent backing is
  expressly not fresh heap storage, copy escape, successful allocation or drop.
- Consumers/change set: publish exact Substring(String,Int,Int),
  StringReplace(String,String,String) and ToString(physical scalar)->String
  premises; move the unmodelled-result consumer behind that existing predicate;
  keep backing origin/destination and no-store visitors consuming it, traversing
  every actual eagerly. Existing input claims snapshot/replay this predicate at
  the final summary/result edge. No indexed-string whitelist, origin_ready or
  copy_escape expansion, registry identity change, new owner or wire field.
  Within the same publication owner, reuse publication_result_current instead
  of duplicating current call/argument checks in the unmodelled consumer; move
  the unchanged checked_call/checked_argument APIs to its existing callable
  predicate section. This preserves identity and the599 noncomment TU caps
  without another owner file or unrelated structural cleanup.
  Current typed value_type is mandatory authority. Native stdlib calls do not
  necessarily issue AST semantic_value_type_name; if that derived projection
  is present it must agree, but its absence is not an absent typed result.
  The first trial040c302b incorrectly required that projection and retained
  all3 refusals (phase8088/1); the integrated predicate repairs the owner join,
  not the test expectation or the required typed value.
- Last consumer: final backing_rows_current -> complete_return_relations ->
  summary/loan admission -> full canonical/bootstrap. Physical birth, exact
  exit-drop, allocator/Pool authority and installed-runtime failure parity are
  separate OPEN obligations, not inferred from this dependency relation.
- ABI evidence: native C/LLVM Substring/Replace copy non-NULL results; scalar
  ToString has no input backing, String ToString aliases. Installed helpers have
  distinct allocation-failure/overflow behavior; this slice makes no claim of
  physical allocation success or complete native/installed ABI safety parity.
- Forbidden: all-String/name-only freshness, treating all ToString inputs as
  physical, clearing forwarded/nested retaining calls, widening indexed-string
  copy permission, Pool/Finish approval, larger4096 or smaller canonical input.
- Dependency order/scopes: A edits only existing publication owner/routines;
  B owns source fixtures and focused summary runtime gate; C owns phase-unit
  receipt/currentness faults. Root first runs new positive probes on frozen
 32cd, then integrates, runs shared caps/phase/strict summary, fixed full census,
  broad native/IR gates and actual bootstrap. No concurrent build/Git lane.
- Falsifiers: String ToString and user-shadowed/indirect calls remain dependent;
  nested retaining actuals remain refused; missing or changed generation,
  result/argument type, target/kind/std/decl/binding/arity/argument ID revoke the
  premise and the final cached receipt. Existing Pool/opaque and4096/4097
  controls remain. Slice/Split/Finish/ArraySort are not silently granted by
  this contract; revisit only as reached canonical blockers.

Before evidence: identified native32cd rejected all3 new positive fixtures at
the expected borrow/required-backing boundary in `.tmp/p0-string-result-before.
hQZWBm/`; no MIR was emitted. The earlier ToString fixture without the zero-
argument result-frame consumer already passed and was strengthened rather than
misreported as a regression fix. The3 new negative controls are also refused
with PGY_SEM_BORROW_ESCAPE at the forwarded or nested retaining source site.

Candidate6927563d3/native0699399a narrow `.tmp/p0-string-result.nrv2CK/`
passes phase8486/0, strict summary, exact new3 C/LLVM outputs,3 exact-site
negative refusals, existing Pool/opaque and4096/4097 controls,599/599 TU caps.
It is NOT integrated closure: full fixed census `.tmp/p0-identifier-origin-
census.onWYW4/` regresses533 ->762 (0 resolved,229 newly failing), all4096,
3188/3188,incomplete0,121.110s internal,2:10.05 wall,1248088KiB peak; full
source/build/bin restoration BYTE_IDENTICAL. Actual bootstrap `.tmp/p0-return-
bootstrap.xXWg8f/` remains18410 errors/16 warnings before gen0. Keep the source
checkpoint local, no push. Correct dependency facts expose work beyond earlier
unsupported cutoffs; that is not permission to claim canonical improvement.

Next measured-operation question, not a new algorithm: backing_consumers always
calls backing_origin(actual) before discovering that the exact current input is
synchronously non-retaining and no origin bit is consumed by that call edge.
The C visitor still must eagerly evaluate the actual and preserve nested effects,
claims, pending/unsupported relations and declared-result checks. Instrument the
existing visit to measure actual work/entry/node allocation for the local-true
subset on0699/full fixed input before changing the operation. No shortcut has
been approved; merely dropping charges or weakening unsupported/currentness is
forbidden. The prior no-store summary-query shortcut remains rejected by its
zero-benefit measurement.

Origin diagnostic `.tmp/p0-cost32-origin.BxvaDo/` restores byte-identically and
retains the exact4096/101-entry failure.1492 actual-origin invocations include
708 ordinary-local cases:76 first-charge nodes,0 new callee entries and0
observed flow-state changes. Those nodes can be charged instead by the required
later C visit (shared work_epoch);76 is not a saving. More importantly, declared
result O performs destination/result-frame admission that C alone does not.
Do not implement this blanket skip. Native broad `.tmp/p0-typed-retention-
broad.41el1X/` passes MIR327/semantic3228/transpile1009, Map24/58, phase8486/0
and HIR/RIR/MIR/AIR; full bootstrap is still RED.

Next diagnostic compares only the ordering of the existing registered work.
The actual semantic_consume_param_publication already demands every registered
ref formal in declaration order before its final consumers. Its validated
body_calls already link exact caller/callee routine indexes for body-summary
publication. Use those edges only to order that same request set callee-first,
with cyclic edges still handled by the unchanged recursive flow owner. It is
not another semantic authority or extra/unrelated prewarm; every existing
routine/formal request remains and4096/currentness/negatives stay unchanged.
Unknown edges cannot grant anything: the existing demanded solver still owns
closure and failure. A scratch-only postorder experiment must check indexes,
overflow/allocation failures, emit the request multiset and stop before MIR.
Production ordering is not approved until measured on the unchanged full
canonical input and reviewed for missing edges/cycles. The762 cold-key failures
remain separately visible even if production ordering changes its result.

Ordering experiment finished and is REJECTED, not an implementation plan:
`.tmp/p0-ref-order.QdOiMH/` checks identical request multisets (3188 on the
full input),4273 routines,23181 recorded edges and201 cyclic edges. Both
orders fail with18410 errors/work4096; postorder adds one entry/evaluation
(557/601 ->558/602). Independent4097 requests still pass and the single
4096-closure negative still refuses. Production source/build/bin/modes/Git
state restore BYTE_IDENTICAL to5532. The call-edge registry is selective,
not an authorization to infer complete callee effects; cached COMPLETE rows
still replay current output premises. Do not land scheduling, a generic
cache, a higher cap or an unrelated prewarm. Next measurement must name the
actual first warm-demand key and charged producer behind the existing owner.

### Reached physical-actual destination relation (2026-10-17)

Warm diagnostic `.tmp/p0-warm-cost.pIfYKe/` uses production declaration-order
requests on the unchanged full4273-routine input, stopping before MIR at the
first failed request:254th request `ResultCallReturnType` id127572,ordinal3.
Its4096 charges are60 entry evaluations,69 no-store graph members and3967
backing visits. ReadTypeFact id103229 spends533 each on input0:String,
input1:Int,input2:Int,input3:inout Array<Int>. Additional observer YRPkB2
finds160 physical-actual D summary queries whose nested interval union adds
1646 work units. This is observed cost, not guaranteed savings. Both scratch
runs restore source/build/bin/modes/Git BYTE_IDENTICAL to5532.

- Objective/priority: remove only proven-empty physical actual -> call-result
  backing joins, then retry full canonical/bootstrap; preserve identity,
  currentness, eager effects, recursive pending relations and4096/4097.
- Owner: current typed publication physical-scalar value classification
  (Int/Long/Float/Double/Bool/Duration only), consumed by existing
  backing_destination_value's non-declared-result AST_CALL argument join.
  Its declared-result branch already uses the same premise. A physical
  *returned value* is not an address/descriptor borrowed from that argument.
- Change set/last consumers: after checked_argument, a current physical
  actual contributes no destination bits, but its callee ordinal query may
  also be the last whole-body validator. Therefore blanket declared-actual
  skipping is NOT approved. An earlier nonphysical argument of the SAME
  exact call must retain a required BACKING_FRAME relation (ready, or linked
  by the existing pending relation until that full frame closes). Only then
  may a later physical actual avoid its duplicate D query. With no such
  witness, retain the original query, including scalar-only opaque factories.
  Source audits approved this narrower witness/recursion condition.
  Before phase bx2VOn confirms a reached last-consumer gap on67: cached parent
  receipts accept an incomplete/pending required callee frame. Existing
  backing_rows_current must reject non-unsupported unfinished REQUIRED parameter
  frames outside the solver. Optional raw scalar-return draft frames are not
  that admitted witness; their existing shape/claim/edge checks remain. Active
  solver construction/pending sealing stays valid. A first overly broad guard
  broke28 old raw-producer unit assertions; narrow it, not those test oracles.
  Do not weaken the fault test or invent another witness owner to bypass this.
  Record/replay its existing shared claim; keep O and C visitors intact,
  including every eagerly evaluated actual, output stores, unsupported-call
  preflight, zero-argument result closure and final backing_rows_current.
  Keep the skip INSIDE the existing exact declared-callee branch; opaque/
  builtin missing relations must still refuse. The reached scalar predicate
  must join its issued boolean with publication_physical_scalar(value_type),
  not trust a forged scalar bit on a String/Slot value. This is the same
  publication owner and no additional snapshot or schema.
  No formal-frame shortcut, new producer/cache/family or wire is needed.
- Forbidden: global physical-place skipping, inferring physical from
  return_backing_required=false, classifying String/Slot/nominal values as
  scalar, removing nested consumers, granting output authority, shrinking
  the canonical input or increasing a limit. Do not reopen the rejected
  joint-matrix, ordering, origin-skip or general-cache proposals.
- Scopes: A owns function_param_flow_backing_dependencies.c, the existing
  scalar query in function_param_publication_routines.c and the reached final
  currency check in function_param_flow_store_destroy.c, not a new track.
  B owns existing readonly_map_query_fact_phase_unit.c's behavioral currentness
  faults; C owns bounded source fixtures and their existing summary gate
  registration. Root owns before probes, integrated build, caps, strict
  summary/phase, full fixed census, broad native/IR, actual bootstrap,
  review and Git. No other lane builds or touches shared WSL.
- Falsifiers: physical call actual with separate inout output; nested
  RetainThenLength/Keep remains a borrow refusal despite scalar return;
  known opaque allocator-birth cases still refuse and pending full-body witnesses
  must seal before any caller receipt becomes complete;
  descriptor actual still propagates backing; scalar classification/type,
  argument ID/binding/arity and selected callee drift revoke the cached
  receipt. Keep recursive and high-ordinal outputs plus4096/4097 controls.
  Record the full unchanged census outcome and actual bootstrap, not just
  the removed query count. Installed-driver and same-SHA CI remain OPEN.

Before probes gtQAzv confirm C/LLVM positive7 and3 exact-site BORROW refusals
with no HIR; negatives are never executed. The initial proposed scalar-only
MakeText(Int)->ForeignText wrapper is admitted by5532 (XfE2QK); do not invent
a refusal oracle or call this new regression. It is not a freshness proof.
The retained negative uses the existing unproved allocator-birth boundary.
Old phase7SbuBN has8639 checks/9 expected failures:3 scalar type/bit faults,
4 incomplete/pending required-callee currency faults and2 recursive-edge faults.
Candidate7151433 fixes all9 but has the28 raw-producer failures above; it is
not acceptance. Its CLI summary trial Iu3wV2 passes the current positive/
negative,4096/4097 and C/LLVM controls; rebuild the narrowed guard and rerun.

Integrated dff3f869 narrowed-guard receipt m68ymx passes8639/0 phase checks,
strict summary including all4 new cases,599/header caps and4096/4097. Broad
dozwQg passes native MIR334/semantic3228/transpile1012,Map24/58,phase8639
and HIR/RIR/MIR/AIR. Full frozen census oqClaW:3188/3188,721 failed vs762,
41 resolved,0 newly failed,incomplete0;714 work4096 and7 now reach required
return-relation incompleteness instead of the earlier work limit. Internal
138.558s,wall2:29.60,peak1247988KiB; BYTE_IDENTICAL restoration. Do not call
these failures resolved or equate a nonfailed ALL mask with admission.
Actual bootstrap NmTeXN is still RED before gen0,18410errors/16warnings.
No current gen2/gen3/installed/CI evidence. Remote main39a and CI38860052641
were checked live and remain unchanged/RED. This bounded repair is safe to
checkpoint locally; do not publish it as green. Next inspect actual warm
failure after the repair and the7 reached incomplete producer/consumer rows.

### Blocking red-team finding — selected source callable identity

STOP the ordering experiment until this safety boundary is repaired. Independent
review of6927563d3 found a source-level identity bypass, reproduced by root on
0699 in `.tmp/p1-callee-collision.Xum6V2/`: intent ToString (Bool), event ToString
(Void), intent StringReplace (Bool), event Substring (Void) all admitted a
String binding and emitted MIR. The4 artifacts were never executed. This is a
real typed-call selection bug, not just a new result-predicate condition.

- Objective/order: selected source identity and type -> current publication ->
  identical C/LLVM target; fail closed before cost or bootstrap. No fake stdlib
  input/result contract may be issued for an event/intent or other source symbol.
- Owner: type_check_call's admitted lexical symbol and exact declaration ID.
  Existing constructor/value/callable checkers own kind-specific typing; names
  must not reopen stdlib after a source symbol has been selected. Preserve the
  current docs/205 reservation policy; do not prohibit legitimate events/intents
  or add reservation families to hide the bug. Existing language constructor
  forms keep their explicitly documented precedence.
- Whole chain: program/import symbol collection -> bare-call selection ->
  existing symbol-call/constructor typing and decl ID -> declared_callable /
  stdlib / builtin / value-binding facts -> typed publication snapshot/current
  replay -> MIR purity and call type inference -> C/LLVM event/function/intent
  emission. Event dispatch and Void inference must consume exact selected
  declaration identity, not whichever same-name event a backend happens to find.
- Required changes: source-callable selection includes legitimate Intent/Event;
  a non-callable selected source symbol errors instead of retrying stdlib. Join
  any declaration lookup to the selected symbol ID. Snapshot/replay existing
  declared_callable in PublishedNode (no new AST/MIR wire). Keep constructor
  routes and local values distinct. Reached backend declared-call dispatch and
  type inference must distinguish Event before ordinary function/Intent.
  Reached existing backend consumers include C user-call host-method selection
  and both event emit owners as well as dispatch/type inference; those must
  compare selected source/MIR method/event syntax ID, not a same-name match.
- Last consumers: final backing currency and both native backend emitted call
  targets/outputs. Missing or drifted IDs/flags reject; no name-only fallback.
- Scopes: A owns semantic call selection and publication producer/currentness;
  B owns reached existing C/LLVM dispatch/type-inference consumers; C owns phase
  flag faults, source collision negatives and legitimate domain-call positives.
  Root owns source-plan reconciliation, before evidence, build/test/commit and
  independent review. Do not modify scratch postorder/source while fixing P1.
  Reached identity obligations extend these same scopes, not new work tracks:
  A's existing host-declaration index must own exact nested callable IDs and
  generated class/enum aliases (enum currently omits the method ID). Build the
  bounded nested callable index once, retire it with the context, and delete
  the trial per-call program/extern scan. Top-level lookup must still join ID
  and kind; generated alias spelling must join its real owner/method. B's
  existing constructor emitters and two type-inference consumers must join
  selected nominal/enum binding IDs before choosing an ABI. Do not require the
  function/event flag on a constructor or let a builtin name retype it.
  Preserve language Option/Result forms, current MIR-only projections and
  legitimate generated aliases; new missing producers return to this map,
  not exemptions. C prepares constructor/alias controls; root observes their
  actual outputs before registering exact oracles.
  Constructor/enum producers must publish their selected ID in the existing
  CALL target field while keeping declared_callable=false. Root's existing MIR
  graph materializer consumes that flag when issuing a declared-callable leaf;
  a constructor target ID alone must not select the function ABI. The existing
  persisted graph schema already permits this distinction; no wire field or
  new binding kind is introduced. Snapshot/currentness and full native MIR
  controls must check the changed projection, not only direct-call output.
  Observed integration45f3fb0e: phase8522/0, strict native summary and scoped
  R7 C/LLVM pass; MIR334/0 and semantic3228/0. Constructor outputs7/9 and
  class alias31 match both backends. Two reached obligations block approval:
  enum alias emits C19 but LLVM declares Route_Value without its body; repair
  the existing nominal body consumer and align simple-enum self=i32 with its
  registered prototype (payload-enum struct carriage unchanged). Existing
  expression-emitter units also invent undeclared Left/Right/Add calls; the
  new guard rejects them and the old test dereferences NULL at core_a_1:195.
  The isolated ASAN/UBSan receipt Bn4RPF confirms that exact first UB without
  changing production native45f. Enumerate the complete unit failure set in
  a labelled scratch harness, then repair test inputs to use admitted identity;
  do not weaken production admission, skip assertions or conceal NULL results.
  The scratch census then stops at a real pre-existing sanitizer defect:
  string_window_extent_finalize sorts absent zero-row arrays with qsort(NULL,
  0,...). Root repairs only that reached owner to sort when count>1, preserving
  the same rows/order/lookup/last consumers and all extent refusals. The same
  ASAN/UBSan input must proceed past this boundary before the census can be
  called complete; this is not permission to disable the sanitizer or assert
  a NULL result valid.
  Second sanitizer observation KL8IAK reaches bsearch(NULL,0,...) in the same
  owner. The complete reached family is5 sorts and3 lookups over optional
  arrays. Finalization now rejects nonzero-count/missing-storage inconsistencies;
  zero-row lookups return not-found without entering libc, and only multi-row
  tables are sorted. Preserve positive extent facts and all denied mutations.
  Recheck the same census under both sanitizers, not a non-sanitized workaround.
  Complete census9O8WBc reaches the end under ASAN/UBSan with explicit rc86:
  18 failed assertions plus1 NULL-comparison diagnostic, not a PASS.3 positive
  expression cases invent Left/Right/Add without declarations.7 addressability
  negatives depend on a failed builtin falling through to unselected user-call
  text. Standalone boundary aGwwS2 confirms real ListNew->ListPush returns NULL
  and the original addressability error. Repair positives through the existing
  strict source/MIR projection, replace fake Make* operands with real constructor
  shapes, and strengthen negatives to no C output plus their exact owned error.
  Root's C dispatcher stops on an emitter's owned error instead of trying a
  different lane. Do not recreate the old fallback merely to preserve a
  non-NULL invalid fragment. These are backend component checks, not a claim
  that every negative source was admitted by the language front end.
  Latest integrated native92d2bf67, narrow uFZjyp: phase8522/0, strict summary
  and all constructor/alias identity controls PASS. Final original test suite
  qk0pof passes native1012/0 and ASAN/UBSan1012/0 (leak detection explicitly
  off); no diagnostic interposers. Actual source declarations require the
  existing pgy_u_ emitted names, and Cancel now uses a semantically admitted
  async Worker/spawn/Future/await source fixture. Backend oQ58gm passes all5
  C/LLVM constructor/alias outputs7/9/31/19/23 and scoped R7 shadow controls,
  not installed evidence. Actual prior827 bootstrap VRmRof remains18421
  errors/16 warnings before gen0. Keep scoped native green and canonical RED
  separate. Broad rCeOTO is now PASS: MIR334/semantic3228/transpile1012,
  Map24/58, phase8522/0 and HIR/RIR/MIR/AIR. Save this coherent local source
  checkpoint and obtain exact-SHA independent review before resuming the
  fixed-input admission blocker. These are native prerequisite receipts.
  Source checkpoint f3015c448a99af4dcb6962f4771312f3c4eb00a5 is local;
  exact-source independent review is pending. Actual92 bootstrap tie9sL is
  still18421 errors/16 warnings. The new fixed cold census pYZH2w regresses
  762->1777 failed keys (1194 newly failed/179 no longer failed):1652 required
  return-relation failures and125 stale indexes;4273 signatures/3188 keys and
  BYTE_IDENTICAL restoration verified. The179 are not claimed progress:
  earlier unsupported paths prevent meaningful traversal.
  The first real pre-summary failures are10 qualified namespace call identity
  errors plus1 dependent numeric-cast publication error. Minimal ucTgLQ probe
  Tools.Render is accepted by0699/HIR1388 bytes and rejected by92 at12:15,
  PGY_SEM_TYPE_MISMATCH/semantic:call:not_callable, no HIR. Old/new nominal
  and enum-index reduction probes do not isolate this regression. Diagnostic
  o4mZqa confirms missing current CALL publications in the reached real routine,
  not a demonstrated new backing algorithm bug. Do not run ordering trials or
  weaken currentness before repairing the original identity producer.
  Follow-up objective: preserve the selected canonical Symbol name+ID from
  module normalization through static namespace calls, publication, MIR and
  both backend consumers. The normalizer owns Tools_Render; Tools.Render is
  diagnostic spelling only. Reuse the same host index, not a second scan or
  name fallback. A maps canonical symbol/flag publication; B maps static-member
  MIR/backend consumers; C owns reduced namespace positive/negative inputs.
  Root verifies old/new behavior and one shared namespace C/LLVM+strict-summary
  gate, then unchanged full census and actual bootstrap. Freeze production
  while the f301 independent review runs; the necessary flag/flattening
  consumer changes must be known before editing, not discovered as exemptions.
  Whole-chain survey shows native MEMBER remains MEMBER through the reached
  graph and is dispatched before identifier/constructor routes. Do not add a
  flag or projection change on speculation. Isolated one-line lookup fix
  8qVmgr/native5532f2f2 passes Tools.Render C/LLVM7/7, existing namespace direct
  return8/8, flattened-name collision refusal, and strict summary4096/4097.
  Full fixed census lbpznn returns762 work4096, new failures0/resolved0 vs0699,
  4273 signatures/3188 keys unchanged,124.415s internal. Both nested and outer
  source/build/bin restores are BYTE_IDENTICAL to92. No production D source
  was edited. Save this evidence for the bounded canonical-name follow-up;
  the remaining cost blocker is still open, and static-member last-consumer
  identity drift requires a concrete negative rather than an assumed flag fix.
  Independent f301 review completed with that namespace regression as its one
  confirmed change request; enum ABI and empty-table repair had no additional
  finding. The one-line canonical-name fix/comment is now in D WIP. Actual
  namespace fixture is registered in the existing strict summary, with exact
  host-index canonical/ID and wrong-name/zero/crossed-ID phase controls.
  Integrated5532 narrow atjS78 passes phase8532/0, strict summary including
  namespace C/LLVM7/7, imported namespace8/8 and flattened collision refusal.
  Original test_transpile yDu6O9 passes native1012/0 and ASAN/UBSan1012/0,
  leak detection off, no diagnostic interposers. Broad Njn5fa is PASS:
  MIR334/semantic3228/transpile1012, Map24/58, phase8532 and HIR/RIR/MIR/AIR.
  Actual5532 bootstrap eUeXQO is back to18410 errors/16 warnings before gen0;
  the11 namespace/cast regressions are gone. Save the bounded namespace fix,
  then run the previously planned scratch-only request-order experiment with
  exact request-multiset and independent/oversized work-boundary controls.
  No cost/source algorithm change has been approved or implemented.
- Falsifiers: the4 String-binding collisions now refuse semantically/no MIR;
  valid nonreserved ToString Intent/Event retain Bool/Void semantics and C/LLVM
  agreement, including statement-position effects. Ordinary function/host/value
  shadow and constructors retain existing meaning. A true stdlib String-result
  call with declared_callable flipped must revoke its predicate and cached
  backing proof. Preserve typed String positives, nested retaining/Pool/opaque,
  CFG, recursion,4096/4097, all native suites and propagation gates. No unsafe
  pre-fix binary is executed. Installed-driver/bootstrap/CI remain unverified.

Root also reproduced the single-flag proof forgery on unchanged0699:
`.tmp/p1-declared-flag.qgdBq3/` runs a scratch phase unit with only
declared_callable=true added as fault14 for4 real stdlib calls.8502 checks,
8 expected failures: all4 predicates and4 final cached proofs stayed current.
Native0699/source/build/test/binary restoration is BYTE_IDENTICAL. This is
before evidence, not a passing acceptance gate. The same assertions must pass
after producer/consumer repair. Reservation-only declarations remain under
the existing policy; do not add a new rule merely to make collision tests red.

### Integration checkpoint and remaining result relation

Source checkpoint `e6010d946` consumes the exact SubStartsWithLen String input
contract, widens its existing self-host selection/recording chain to Long,
repairs MapNew's non-materializing type check, and seals the exact Concat
constructor premise in the existing input claims. No physical cleanup right is
issued. Linux binary `dbd2e4c5...` passed phase7970/0, strict4096/4097 summary,
three C/LLVM factory controls, native MIR327/semantic3228/transpile1009,
Map24/58 and HIR/RIR/MIR/AIR propagation. Full fixed input receipt
`.tmp/p0-identifier-origin-census.w2O0jC/` is538=533work+5required, versus541
before this slice:3 resolved,0 new failures,4273 signatures/3188 keys unchanged,
incomplete0/no MIR; observer restored source/build/bin byte-identically.
Map key-copy positive and value-retention refusals were separately observed at
their exact sites in `.tmp/p0-prefix-probes.nbYP8g/`.

Actual current full bootstrap remains RED before gen0 (18410 errors/16 warnings,
`.tmp/p0-return-bootstrap.Tpohpg/`). Actual collection parity gate stops in its
prerequisite map-written-root program (15321 errors, `.tmp/self_hosted/
map-written-root.i8qo5X/`); new high-bit mutations133-141 are UNEXECUTED.
No gen2/gen3, installed-driver or required green CI evidence exists. Do not
publish known red or count these native prerequisites as self-host substitution.

The next implementation unit is one result contract across the complete
reached factory chain, not a series of per-function exceptions:

- Objective/order: resolve the5 required-backing rows without weakening caller
  dependency or lifetime checks; preserve current generation and failure
  behavior before work cost and bootstrap. Physical transfer/drop stays OPEN.
- Chain: domain exposure -> builtin signature factory -> protocol names and
  nominal protocol -> Option signature helper -> UnwrapOption -> private rows;
  plus intent-observability RowAt and String concatenation. Current result
  frames admit only zero-parameter functions, omit typed wrapper/String+
  composition, and cannot coexist with per-formal frames at their final check.
- Existing owners: CFG body flow produces normal-exit closure; routine
  publication carries that receipt with current body identity. Existing
  declared-result frame proves closure modulo its typed formals, represented
  by zero-origin/zero-destination formal placeholders. The existing per-formal
  returned-backing summaries, not those zero placeholders, own actual
  formal-to-result dependency. Missing, recursive or pending summaries grant
  nothing. Some/Unwrap use their exact typed backing relation; String+ uses
  its existing checked two-String-operand receipt, never generic origin_ready.
- Full change set: pass existing CFG result through param-summary/routine
  publication; require it in result currentness; generalize result frame and
  seed exact formal identities; compose every actual's admitted dependency;
  seal call-edge arity/dependency bits; replay those bits and current callee
  facts at backing_rows_current; validate each frame with its own width and
  allow result-only formal vectors; release new edge storage in the existing
  snapshot destructor. Keep eager evaluation of all nested actuals, stores,
  retaining effects, private ArrayPush payload and no-store graph unchanged.
- Last consumer: backing_rows_current -> complete_return_relations -> final
  summary/loan admission. No new HIR/MIR schema or cleanup permission.
- Forbidden: arity-only relaxation, treating unready mask0 or placeholder0 as
  independence, input-to-inout outputs as result dependency, name/type-wide
  freshness, Pool/Finish grants, skipped argument evaluation, increased4096,
  new owner files or a parallel algorithm.
- Scopes: A owns backing visitor, result frame/edge, internal types, final
  validator and snapshot destructor; B owns routine/CFG publication and the
  existing checked constructor predicate; C owns focused source/runtime and
  currentness/failure tests. Root owns integration/builds/Git and one shared
  strict-summary/phase gate, then fixed full census and actual bootstrap.
- Falsifiers: forwarded formal/String/Option/nominal/ArrayPush dependencies
  remain; fresh String+ can cut result dependency but never nested retaining
  effects; missing returns and unsealed recursion fail closed; changed body,
  flow receipt, call target/type/actual/summary bit revoke the edge. Observe
  focused positives fail on dbd before integrating. Preserve allocator/Pool
  negatives. Unknown until phase evidence: the frame and per-formal summaries
  must refer to the same current typed body/generation, including pending SCCs.

### Executable sub-slice — typed factory dependency independence

- Objective/priority: repair the reached zero-input factory backing-dependency
  refusal using its already-published typed constructor premise. Preserve
  current identity and missing-fact refusal before canonical admission and cost.
  This is an execution unit of the full ownership/bootstrap rung above.
- Fact owner: `semantic_param_publication_checked_local_owner_constructor`
  consumes the current `empty_map_declaration`/binding receipt. The receipt
  joins the checked call, exact LET/initializer/HashMap Type, stdlib target,
  zero actuals and current syntax. It proves no input/output backing relation,
  never unique storage, implicit copy, source retirement or a drop right.
- Last consumers: `function_param_flow_mutable_output_required`,
  `backing_origin` and `backing_destination_value`, then exact backing-row
  currentness, final summary admission and returned-loan checking. No new
  HIR/MIR physical cleanup permission is projected from this predicate.
- Forbidden: call-name/zero-arity grant, setting `origin_ready`/`copy_escape`,
  using nominal type mapping as freshness, treating an issued stale receipt
  as unsupported, relaxing 4096, or claiming physical aggregate cleanup closed.
- Independent edit scopes: A changes the existing typed publication predicate;
  B supplies a safe C/LLVM runtime factory control; C updates the existing
  phase currentness/fault controls and summary fixture oracle. Root owns the
  shared plan, build, unchanged census, integration and Git. No parallel
  ownership algorithm or new fact family is opened.
- Acceptance/falsifiers: current typed empty-map factory positive; wrong
  LET/call/Type/stdlib/arity/generation receipt refusals; retaining ref input
  and resource-birth negatives; unchanged strict 4096/4097 controls. Then the
  fixed 4273-signature/3188-key census must introduce zero success-to-failure
  regressions before actual full canonical/bootstrap/installed-driver checks.
  Internal duplicated collection storage remains a separate physical safety
  falsifier, not a counterexample to caller-input dependency independence.

The next reached dependency premise is a private local `Array<String>` literal
store. Its current publication incorrectly reads the String-element ownership
transition as evidence for the outer Array storage. Replace that dependency
read in the existing `private_array_push` publication with a call-time receipt:
exact checked stdlib `ArrayPush`/two actuals/Void, same local non-alias typed
literal birth, current caller/binding/initializer identities, exclusive outer
storage with no prior escape or pending handoff, and a syntax snapshot of the
literal declaration plus call. Literal length and String-element provenance
do not decide this outer-storage premise. Initial and inserted element backing
still pass through `backing_origin` and `backing_store`; borrowed or retained
inputs must continue to escape in the summary. The three last consumers are
the no-store selector, returned-backing transfer and `slot_analyzer_escape`.
Remove the old element-effect read and unused private effect columns together;
do not leave old/new authority or change element cleanup carriers. A owns this
existing publication/header change, B the safe runtime control, C the phase
mutation and retaining-input controls. Root builds each fixed source snapshot.
Falsifiers include alias/field/branch escape before push, reassignment, ref
inputs in initial or pushed elements, nested retaining calls, shadowed targets,
and missing/stale literal children/Type/caller/call receipts. The same unchanged
census and canonical/bootstrap chain remain the acceptance boundary.

The `faf77f78e` candidate (binary `50d0a2f0...`) passed phase7757, strict
summary, native MIR327/semantic3228/transpile1009, Map24/58 and both factory
C/LLVM runtime controls. Fixed4273/3188 census receipt WSL
`.tmp/p0-identifier-origin-census.m9cb5s/` is541=536work+5required, with zero
new failures; the Array literal premise itself removes no additional key.
The observer restored source/build/bin byte-identically. Actual full bootstrap
still refuses gen0 (18410 cascades); installed-driver/CI are not validated.

Next dependency within this same factory chain: the no-store closure reaches
`SemanticExpressionGraphCollectionCallProtocolFromName` through
`SemanticBuiltinSignatureRows`. Diagnostic-only receipt WSL
`.tmp/p0-factory-dependency.hYxWqn/` demands the reached domain-exposure
ordinal5 on the unchanged full canonical source: the signature factory now has
`mutable=0 private=1`, but the protocol callee has `mutable=1`. Its first
unmodelled String read is checked stdlib `SubStartsWithLen` (arity4); current
retention registry has no row for String actuals0/3. Runtime and type-check
owners establish synchronous read-only inputs and a scalar Bool result.
Objective/order: publish that existing runtime input contract, preserve exact
call identity and currentness, then remeasure the complete fixed census.
Fact owner is `builtin_argument_retention_registry.def` via
`region_retention_summary.c` and its generated Pergyra projection; last consumers are typed argument publication,
the no-store graph, backing-origin/call escape, then final summary/loan
admission and HIR/MIR projection. It grants no output backing/drop permission.
A edits the two exact stdlib/arity/ordinal rows and regenerates their projection;
B migrates the existing self-host selection/call-recording consumers and adds
a safe C/LLVM factory control; C supplies direct currentness/identity/arity/
ordinal faults and retaining-user/opaque-call negatives. Root alone builds/runs/Git. Missing,
shadowed, changed-arity or stale calls must not inherit the row. No ANY
ordinal/name-only fallback, result grant, budget increase or unrelated builtin
registry expansion. Shared gate: existing strict summary and phase controls,
C/LLVM output equality, unchanged4273/3188 census without new failures, actual
bootstrap and required installed/CI evidence. This is a measured prerequisite
of the existing rung, not a new precision or performance track.

Consumer survey found a required representation repair before those rows can
land: selection currently uses a signed31-bit `Int` and rejects registry rows
above31; adding String actuals0/3 produces33 rows. Its call recorder also
mistakes two selected argument rows of one builtin identity for competing
call identities (already latent for `Write`). Migrate the selection, bit and
mask return/argument through the existing producer, expression-identity caller,
call recorder and probe consumers to `Long`. Bound the count by its actual
63 positive value bits, never merely change31 to33 on Int arithmetic. Record
one canonical first registry row per selected builtin identity, accepting
multiple exact argument rows of that same identity and still refusing an
identity/name conflict. Keep one call fact and the existing borrowed-ordinal
lookup; no new owner or wire schema. This is a representation/identity join,
not a raised analysis budget or memory allowance. Test high bits31/32,
both newly borrowed ordinals, both existing Write ordinals, malformed selection,
shadowing/arity, missing call facts and sealed-fact currentness on both backends.
Before any Pergyra source edit, root froze the complete unchanged self-host
input at WSL `.tmp/p0-fixed-canonical.nbPIex/` with per-file and tar hashes.
Run the original4273/3188 diagnostic census on that full frozen input and also
the full current canonical/bootstrap source; changed self-host source is not
permitted to masquerade as a smaller or identical fixed benchmark input.

Independent faf77 review found a fail-closed currentness hole at the same
empty-map premise: `semantic_param_publication_empty_map_binding` still uses
the re-materializing metadata lookup. Repair that existing consumer with the
non-materializing resolved-type lookup; a missing annotation receipt must not
be reconstructed during validation. A owns this one deferred-owner read;
C adds NULL/TYPE_INT metadata and mismatched map key/value Type faults, requiring
constructor/no-store/backing rejection and recovery after restoring the facts.
Add analyze-only caller-derived MapSet key/value return controls. Respect the
existing copied-String-key contract instead of labelling that safe copy an
escape; use an actually shallow retaining aggregate value for the negative.
Do not
turn this into a reassignment kill-precision track: monotone conservative
overwrite refusal is known, not the current safety/CI blocker. Existing
typed-map positives and the fixed full census remain mandatory.

Candidate10cb now proves SubStartsWithLen String actuals0/3 are borrowed reads,
and both Probe/factory no-store graphs close, but the declared-result frame
still fails at the factory's local Concat calls (diagnostic WSL
`.tmp/p0-factory-dependency.2PyCkQ/`, mutable0/result.complete0, current typed
Concat rows with independent String results). The destination consumer ignores
the generic origin_ready flag while checking a declared result; keeping that
distinction prevents allocator-lifetime grants. Repair the same existing
checked-local-constructor predicate with only exact checked stdlib
BUILTIN_CONCAT(String,String)->String. Its runtime allocates/copies or returns
an independent empty result, with no requested allocator domain or retained
input. Reuse current call/target/actual Type facts, preserve argument evaluation
through the no-store and eager backing consumers, and issue no drop/copy
authority. Do not grant every String result or reuse name/zero-arity evidence.
A owns this predicate delta, C owns its currentness and nested-retaining-actual
negative, root reruns the original prefix positive and unchanged full census.
Its first fault run exposed a missing last-consumer premise: Concat's issued
constructor predicate becomes false after corrupting captured argument0 Type,
but cached no-store/backing claims remain true (phase7970/2, fault9). Seal that
predicate's true/false result in the existing `FunctionParamFlowIndexClaim`
AST_CALL capture and replay; no additional owner or external IR payload. The
same index owner must reject either direction of change rather than replay a
weaker input-read fact after its constructor premise is withdrawn. A owns the
existing internal claim struct and input-claims producer/currentness; C's
unchanged fault9 is the executable falsifier. Then replay all existing pending,
unsupported, allocator, Map and strict4096/4097 controls before integration.

### Whole-chain implementation map for the active aggregate edge

- Objective and priority: keep D1 independent values and one final cleanup
  owner while admitting proved fresh results; semantic identity and sound
  transfer precede the 13 factory refusals, 532 work-budget refusals, and
  installed-driver/CI evidence. No gate may substitute for another.
- Source premises: typed publication records call/binding identity and current
  generation. The existing program-point/returned-backing owner proves
  parameter/result backing dependencies and effects, **not** a physical move,
  source retirement, or cleanup placement. The post-DCE MIR ownership-clean
  owner specified by `docs/semantics/27_ownership_clean.md` §5 must decide
  ordered constructor field transfer, post-transfer non-use, copy and every
  exit drop using current typed premises. The nominal result snapshot owns
  field/type mapping only. `array_storage_release_owner.c` currently
  revokes `ArrayDrop` exclusivity on escape; that is not a move or a cleanup
  receipt. The `Array<String>` collection-ownership row tracks element
  provenance and cannot be reused as physical-storage proof by name alone.
- Coverage constraint: the nominal snapshot exists only for complete mapped
  non-generic constructors. Generic records, tuple/enum packs and nested
  expression temporaries need the same final MIR operand/pack analysis, not
  a nominal-snapshot fallback. Publication resets a value row on expression
  recheck and may record the checked call twice, so provisional premises must
  be sealed in the last publication generation.
- Reached consumers: constructor admission and identifier/return checks;
  semantic -> HIR -> MIR fact projection; MIR validation and JSON output;
  installed self-host MIR input and C/LLVM artifact routes; native C and LLVM
  aggregate emission and cleanup. `mir_cleanup.c` currently creates
  resource/pin/rollback edges, not ordinary Array/HashMap/aggregate retirement.
  AIR's present MIR cleanup evidence counts topology markers, not per-value
  exactly-once drops; it cannot certify this new physical cleanup receipt.
  The exact ownership-clean payload and codegen exit placement remain to be
  established before production edits; absent proof must fail closed.
- Disjoint implementation lanes after interface agreement: (A) native semantic
  birth/field-transfer/currentness and post-move admission in `src/semantic`;
  (B) typed HIR/MIR carrier, validation, exact cleanup and native C/LLVM
  consumers in `src/compiler`, `src/codegen`, `src/runtime`; (C) installed
  self-host MIR projection/consumption in `src/self_hosted` and behavioral
  C/LLVM fixtures. Root alone owns shared interfaces, builds, fixed-input
  census, bootstrap, Git and integration. Agents first report any contract
  conflict; no lane may silently add a parallel authority or shape route.
- Shared integration gate: same semantic source input and compiler hash;
  duplicate/nonempty, alias, push/pop and post-field-push/return refusals;
  distinct births, repeated proved-empty and explicit-Clone positives on C
  and LLVM; a runtime/free observer proving each backing retired exactly once
  on direct and early/branch exits (output parity alone is not cleanup proof);
  forged/missing/stale MIR receipt refusal; unchanged 3188-key
  census (including 4096/4097); full canonical admission; gen2 == gen3;
  installed C/LLVM parity; required same-published-SHA CI. Narrow phase or
  source-only greens cannot close this rung. An unsafe admitted probe is never
  executed merely to see corruption.
- Dependency order: resolve the physical cleanup producer and receipt shape;
  seal semantic transfer and last use; carry/validate one fact through MIR;
  emit exact cleanup in both backends; migrate installed self-host consumers;
  then measure the two factory paths and the work budget on the fixed input.
  If a reached consumer changes this map, revise it before extending code.

Read-only interface audit after this map: document 27 §5.1–5.5 already fixes
the final ownership-clean owner **after** MIR DCE/normalization; semantic
publication can provide current typed constructor premises but cannot decide
final move/copy/drop. The existing final-generation expression graph preserves
ordered argument/field node positions, so `(routine, block, instruction, lane,
local node index)` can identify an operand without a new global source-ID
column. This position is not yet a sealed typed reaching-definition/storage
receipt, and the current digest does not establish it. Collection/String
operation ABI, deep glue and final exit-edge
settlement are also not implemented. Adding a semantic-only consumed bit, an
unused HIR/MIR field or a subset-only mandatory drop would not close this
chain. The generic/tuple/enum pack coverage and direct-constructor path are
part of the same owner decision.

The new behavioral gate `tests/aggregate_storage_transfer_smoke.sh` was run
with unchanged Linux compiler SHA256 `d539d7eca05beb938cae038e08ed13783928b67362fe4670b2e34446d2460b86`.
Five positive fixtures passed on both native C and LLVM (10/10), including a
local `Pair` assembled without illegal source reuse; six unsafe negative
fixtures were all compiled on both (12/12 RED). The unsafe programs were
**not executed**. The gate is not wired into required CI while RED. Receipt
after strengthening the post-field-return use and the diagnostic oracle: D
`.tmp/aggregate-storage-transfer.eGbZQD/`, with before/after binary and fixture
SHA-256 manifest. Its negative path requires an ownership diagnostic at the
intended operation, not a parser error or a missing-fact internal refusal.
The positive output does not check that each backing was retired once; that
requires the separate runtime/free observer above. Existing ordered C
constructor temporaries were separately
replayed on both backends, so no duplicate positional-order patch was made.

### Aggregate storage-result closure card (PLAN; no source edit)

- Objective/order: preserve D1 independent values and fail closed on duplicated
  live storage before admitting the two canonical fresh-result factories.
  Proven birth, per-binding generation/effects, field-edge transfer, all-path
  return, then installed C/LLVM consume the same authority. Work4096 remains a
  separate blocker on this same unchanged canonical input.
- Fact ownership (intended, not current implementation): typed publication
  must establish `[]` zero-storage birth, distinct `MapNew` allocated births
  and array literal births by stable identity. Today's publication has typed
  call/binding currentness but no complete physical-birth receipt. The
  program-point/return-flow owner supplies only current dependency/effect
  premises; the post-DCE MIR ownership-clean owner must decide storage at each
  ordered constructor field edge, source retirement and final release from
  those premises. The nominal field snapshot supplies field/type identity
  only. Unknown control flow or effect is not `ZeroStorageDescriptor`.
- Consumer split: nominal field type admission and returned-backing dependency
  checks consume semantic premises. The final physical receipt is consumed by
  direct-MIR validation, both C/LLVM emitters and installed self-host routes.
  Migrate these consumers together; do not retain a source-only admission bypass.
  The direct zero-parameter `Make()->Pair` negative must be rejected at
  constructor admission even when the REF-triggered flow-summary path is not
  demanded; a declared-result-only guard cannot cover it.
  C positional compound initialization does not establish sibling side-effect
  order, so the first positive scope is pure identifier arguments; impure
  siblings require ordered lowering or fail closed.
- Forbidden fallback: `MapNew` name grant, nominal snapshot as uniqueness,
  `length == 0` after push/pop as no storage, an implicit nonempty Clone, or
  source edits replacing compiler inference with manual copies. A zero-capacity
  empty descriptor may be repeated only with current no-growth/no-escape
  evidence. Each allocated birth has exactly one consuming field edge.
- Falsifiers/gates: reject nonempty `Pair(rows, rows)`, push/pop-to-length-zero
  duplication, `let alias=rows; Pair(rows,alias)` for Array and Map,
  branch-only growth, alias/inout/unknown-call growth, reassigned old
  generation, same `MapNew` local twice, forged/stale receipts, and impure
  sibling argument order. Admit canonical repeated empty `ints`/`names`, four
  distinct `MapNew` fields, explicit Clone, and separate post-copy growth;
  verify C and LLVM plus installed MIR-input path. Only then remeasure the 13
  required-backing refusals, unchanged 3188-key census and full bootstrap.

The first source-only pairwise guard was **rejected**, not an admitted
implementation. On identical 4273 signatures/3188 keys it changed 545
failures to 1785 (1660 required-return backing, 125 unavailable/stale
program-point indexes; incomplete 0). Its first new canonical error was a
valid constructor whose Array fields came from separately proved results,
not direct literal births. More importantly, a distinct-birth `Pair(a, b)`
still allowed `ArrayPush(a, ...)` or `return a` after placing `a` in a field;
the C emitter copied the descriptor into the result before the original was
used again. This is a source-versus-result double-owner counterexample, so
sibling-duplication refusal alone cannot establish D1 or cleanup authority.
The candidate's narrow C/LLVM probes passed but did not override either
whole-input falsifier. Keep the field-edge transfer, post-transfer use
rejection, cleanup ownership, typed publication/currentness and installed
MIR consumer together in the next whole-chain design; do not revive a
symbol-only birth heuristic or a blanket unknown-birth refusal.
Root independently replayed the field-transfer counterexample on the frozen
d539 native binary: C emission succeeds with 0 errors and is byte-identical
to the candidate's output. In that output `pair.first` receives the `first`
descriptor, then `ArrayPush(first, 3)` mutates its original copy; the routine
also has no ordinary Array/Pair drop on return. The selected automatic
cleanup contract in document 27 is still explicitly implementation OPEN.
The native MIR cleanup machinery observed here handles cleanup edges but has
not supplied an ordinary aggregate field-transfer/drop receipt for this
case. The precise production implementation, required input facts and MIR
payload are **Unknown** and must be resolved against actual source before the
next production edit; the intended post-DCE MIR decision owner is fixed above.

Executable substitution status: **BLOCKED on a missing compiler fact**, not
closed. The missing fact is a current per-field `Array`/`HashMap` storage
transfer receipt that retires the source at the field edge, distinguishes a
zero-backing copy from an allocated move/explicit Clone, and determines the
single final cleanup owner on every exit. Typed publication and program-point
facts provide current premises, while the post-DCE MIR ownership-clean pass is
the sole final decision owner; no symbol flag or emitter heuristic may recreate
it. Last consumers are MIR
validation, C and LLVM aggregate emission/cleanup, and the installed MIR
driver. Falsifiers are the `Pair(first, second)` post-construction push and
return probes, nonempty/alias duplicate probes, and the fixed 3188-key
canonical census. Resume by resolving the MIR owner's input and carried
identity, then migrate every reached consumer in one plan.

### Demand-only joint backing frame falsified (`0dc970187` source)

Status: REJECTED, not landed. The attempted whole-chain migration was removed;
production source, Makefile and phase-unit test are byte-equal to HEAD. Only
this directive records the trial. The older eager joint candidate's 545 -> 695
regression was not an argument to waive the unchanged 4096 work budget.

- Objective/owner/consumer: share a charged backing node visit only among
  formal origins reached in one demand episode. The program-point index would
  own one formal frame, while summary projection, recursive pending sealing,
  returned-loan/currentness and final admission remain the last consumers.
  This trial migrated those consumers and destruction together, kept the loan
  and declared-result frames separate, and removed the per-ordinal read path.
- Narrow gates: the trial's phase unit reached 7662 checks, zero failures,
  including reverse demand, unproved-origin independence, new-episode snapshot
  clearing and missing active-mask refusal. Strict summary negatives, including
  4096/4097, and effective source-size gate passed. These do not establish
  canonical admission.
- Fixed-input comparison: isolated HEAD and trial builds used the same
  4273-signature/3188-key manifest and canonical `main.pgy` SHA-256
  `466ba13e6f1392a35e9b2152d8d84afd6debdb825e0bee09de49970007676805`.
  HEAD: 545 failures (532 at work 4096, 13 other), internal 128.771 s,
  wall 3:37.51, peak RSS 1,258,228 KiB. Trial: 581 failures (524 at work
  4096, 57 other), internal 164.151 s, wall 4:32.45, peak RSS 1,258,288 KiB.
  Both completed before timeout; neither reached MIR or acceptance.
- Regression: 46 keys newly failed and 10 formerly failing keys passed.
  New failures comprised 24 recursive backing stale/incomplete, 20 required
  return backing stale/incomplete, and two work-budget refusals. At key 200,
  `SemanticAstIntentActionCallFromText` origin 1 had sealed evidence at
  epoch 2; a later joint visit activated origins 0/1/3 and unmodelled-call
  handling marked all three unsupported. Origin 1 became
  `ready=0, complete=0, unsupported=1` despite current row storage, causing
  the required-return refusal. This violates origin-independent verdicts.
  Key 2312 changed from success at work 2227 to recursive stale/incomplete
  refusal at work 2421; its deeper transition was not established.
- Decision: do not revive this candidate by weakening admission, increasing
  budget/time, or accepting the aggregate eight work-budget wins. Before any
  alternative, measure the first 4096-work demand by charged operation and
  distinguish repeat visits of one `(routine, ordinal)` from duplication
  across ordinals. Preserve the existing per-ordinal fact owner until a
  narrower owner-local change has a falsifier and a no-regression census.

Read-only follow-up on the existing first-demand trace
`WSL .tmp/p0-current-work-trace.3rGPw2/run.log` (not a new compiler run):
at demand 608/work 4096 the charges are entry 139, graph 324 and backing 3633.
Backing has 3625 formal charges (3623 complete, two already incomplete) and
eight declared-result charges. Every formal `(routine, ordinal, node)` is
unique: repeat charges of the same formal origin across epochs are zero.
The 3625 formal charges map to 2305 `(routine, node)` pairs, leaving 1320
cross-ordinal extra charges on 792 shared nodes. By phase those extras are
destination 95, origin 220 and eager consumer 1005; CALL 331 and BINARY 206
are the largest individual syntax shapes. This is an observed prefix bound,
not a removable-work count or proof that 1320 saved units would admit the
full demand. Epoch-only optimization is therefore the wrong target.

Next producer-only hypothesis, not implementation permission: the existing
program-point/publication owner could certify a closed subtree whose current
typed facts prove no formal backing, no retaining effect, no output write and
no relevant control-flow edge. Only then could the existing per-ordinal
consumer omit the corresponding actual visit and its charge. Keep separate
per-ordinal verdict/currentness and do not reuse a global active-origin
permission. The safely skippable count is Unknown; reuse of observed
cross-ordinal work alone is bounded by 1320 units in this prefix. Falsifiers:
an unmodelled callback under a physical result, `Hold(Echo(input))`,
different-origin unsupported/retained outcomes, late binding growth,
shared-DAG diamond expansion, changed publication claims, and 4096/4097.
If the producer cannot prove subtree closure without walking or reproducing
the same work per ordinal, reject this hypothesis rather than hiding charges.

Read-only follow-up on that trace found at most 130 certifiable visits for
typed empty `[]` literals (74 first visits and 56 cross-ordinal repeats), only
3.17% of the 4096-work prefix. Five other array literals contain a child
identifier and are not closed. The 561 BINARY and 218 UNARY charges cannot be
treated as skippable without child/effect/currentness proof. `loan_seed` and
semantic promotion were not captured by the trace, so 130 is an upper bound,
not a measured saving. This does not establish relief of the 532 work-limit
failures. No producer patch was made: a syntax-shape fast path without a
current, effect-closed subtree fact would evade the owner rather than close it.
The 83 same-routine ordinal-lane pairs in this trace have zero overlapping
charged-event intervals. A shared immutable topology alone would not reduce
the per-ordinal backing work; a truthful bitset wavefront would need a change
to demand scheduling while preserving each lane's independent verdict and
pending/currentness. The already-rejected eager joint frame shows why
unrequested lanes cannot simply be activated. Measure already-demanded batch
overlap before attempting that rewrite.


### Joint-origin backing falsifier (checkpoint `c34ee0f6`)

- Objective/priority: remove the measured repeated per-formal backing traversal
  on the unchanged canonical input, preserving typed identity, per-origin
  refusal and currentness before reducing work. This is the existing backing
  owner, not a new summary or cache authority.
- Current owner/last consumer: `backing_frames[ordinal]` is built by
  `function_param_flow_returned_backing_effects`; summary projection,
  recursion sealing, returned-loan/currentness and final ownership admission
  consume it. Intended owner is one index-owned joint formal frame with an
  origin bit per formal; the declared-result and local loan frames stay
  separate. Remove every old per-ordinal frame read and destructor path.
- Proposed migration order: seed diagonal formal bits; calculate expression
  snapshots and effects/output rows together; preserve origin-specific
  unproved/unsupported/pending verdicts and edge-origin witnesses; restrict
  `owner_store` and recursive-self exemptions to the matching origin bit;
  register a same-frame cross-ordinal re-entry as a solver revisit; migrate
  currentness, pending seal, output/unsupported projection, and destruction
  atomically. A result is ready only for the ordinal whose entire evidence
  is current; joint completion does not complete its separate SummaryEntry.
- Forbidden fallback: no skipped charge for work still performed, global
  exemption of an unrelated origin, global clearing of another origin's
  pending edge, stale binding pointer as an expression snapshot, old/new
  dual frame read, reduced canonical input or increased 4096 budget.
- Falsifiers/gates: two inputs with only one retained/unsupported/pending,
  owner-store/self mixed-origin payloads, same-frame cross-ordinal demand,
  late binding growth, 65/129-bit origin and output boundaries, stale claims,
  shared DAG/cycle and original 4096/4097. Run phase and strict summary first,
  then exact fixed-input census; only an admitted unchanged canonical input
  permits bootstrap, installed drivers and exact-SHA CI claims.
- Separate blocker: the 13 required-backing factory refusals need fresh
  aggregate birth/update/return proof. A typed `MapNew` name grant alone
  resolved zero fixed-input keys and may miss duplicate-field ownership.

The eager joint-frame candidate was implemented temporarily, tested in an
isolated build, and removed from production source/test files. Its focused phase unit
passed 7640 checks and the strict summary gate passed, but the unchanged
4273-signature/3188-key cold census regressed from 545 to **695** failures:
620 work, 50 required backing, 25 recursive backing (receipt WSL
`.tmp/p0-joint-census.VW3Vpo/`, full 3188/3188, incomplete 0). The real
canonical route happened to stop at required backing instead of work4096;
that one route did not represent the cold key set. Eagerly seeding every
formal forced unrelated callee summaries into a demand closure. Do not land
that candidate or treat its faster first route as a closure. The next
algorithmic unknown is whether only already-demanded origins can share a
frame without replaying real semantic work or granting uncomputed rows. The
measured counterexample must be included before another joint design edit.

### Fresh-result proof boundary after the rejected candidate

- The 13 baseline required-backing refusals pass through zero-argument factory
  results, including `SemanticAstCollectionFocusRecoveryFactsEmpty` and
  `SemanticBuiltinSignatureRows`. The former builds four distinct `MapNew()`
  results and repeats empty `Array<Int>`/`Array<String>` descriptors across
  nominal fields; the latter returns a nonempty `Array<String>` built from a
  literal and `ArrayPush`. `MapNew` allocates buckets immediately, whereas
  `pgy_array_new_Int(0)` has null data. A paired empty-array probe that pushes
  only `pair.first` prints lengths `1`, `0` on both native backends. This
  establishes a bounded empty-descriptor distinction, not a general result
  ownership proof. These are distinct birth/transfer premises, not a single
  constructor-name permission.
- A falsifier compiled with the unchanged native binary d539 on C and LLVM:
  `let rows: Array<Int> = [1]; return Pair(rows, rows);` followed by
  `ArraySet(pair.first, 0, 2)` and reads of both fields. Both backends admit
  and print `2`, `2`. The C emitter puts the same array descriptor in both
  fields, with no clone. This is an observable ordinary-value alias (an
  independent-value result would print `2`, `1`), not evidence that a general
  nominal constructor owns a fresh independent result. The isolated source
  is `.tmp/p0_nonempty_duplicate_aggregate_probe.pgy`; the C receipt is WSL
  `.tmp/p0_nonempty_duplicate_aggregate_probe.c`.
- Do not use `semantic_param_publication_nominal_result_complete` as a
  uniqueness proof: its snapshot validates field/type identity, not duplicate
  backing, copy synthesis, or cleanup rights. A sound factory-result proof
  must account separately for empty descriptor copies, unique `MapNew` births,
  nonempty array copy/move, intervening growth, and every return path. Its
  negative gate must reject or independently copy the nonempty duplicate on
  both backends while preserving the canonical empty factory; no blanket
  nominal/MapNew grant is admissible.

- Objective and order: admit the unchanged full canonical compiler program
  without weakening ownership, then run the actual bootstrap, installed C/LLVM
  driver and same-published-SHA CI. Stable typed/publication identity and
  fail-closed outputs precede work reduction; speed is not a separate goal.
- Fixed evidence: local source `9b3eb9f1e` plus handoff commit `d6ef32898`,
  tested binary SHA256 `7f04442a32e66ffc8292bad578975902040afa6b7c33c365f34be38f9ea7ba5c`;
  unchanged `src/self_hosted/codegen/main.pgy`. The actual bootstrap stops
  before gen0 at work4096. The isolated 4273-signature/3188-key census has
  698 work failures and 9 required-backing failures, incomplete 0. The first
  failing key reaches 77 summary evaluations and work4096. This does not
  establish which charge is redundant.
- Chain and authority: the native self-host entrypoint seals typed inputs in
  semantic publication; program-point indexes/claims carry current routine,
  call and node facts. `function_param_flow_summary_demand` owns the
  per-demand 4096 budget and fixed-point entries. The no-store graph and
  returned-backing frame charge reached graph members and node/epoch work;
  recursive calls reuse the same demand. Output/currentness and returned-loan
  consumers are the last legitimate users before ownership admission and MIR.
- Forbidden fallback: no larger cap or timeout, smaller input, uncharged
  real work, `TOP`/unproved as permission, missing/stale claim acceptance,
  aggregate copy/annotation workaround, cross-demand cached permission, or
  native-only admission substituting for installed self-host evidence.
- Investigation and edit order: decompose the first failing demand into body,
  graph and backing charges on a scratch observer while preserving production
  source/build/bin byte-identically. Classify duplicate versus distinct work
  and its fact owner; inspect all reached callers and final consumers. Only
  then choose one bounded owner operation, its exact currentness/identity
  premise, signatures/consumers to migrate and old work path to remove.
  Unknown now: the removable operation and whether one fix covers all 698.
- Falsifiers and integration: retain original 4096/4097, stale/missing claims,
  late SCC growth and output/loan refusal negatives. Demand a same-input
  before/after charge trace, phase/strict gates, native/Map/Slot/C+LLVM
  positives and negatives, then actual unchanged bootstrap/gen2==gen3,
  installed drivers and required CI. A lower failure count alone is not done.

Scratch measurement on the actual first warm bootstrap failure, binary7f,
is now observed at `WSL .tmp/p0-current-work-trace.BXglu6/`: the 4096 units
are 117 entry evaluations, 309 no-store graph members and 3670 backing
node/epoch visits (3660 complete, 10 already incomplete). All 3670 charged
backing rows have distinct `(routine, input ordinal, node ID)` in this demand;
the fixed-point-wide repeat hypothesis does not explain this frontier.
First-charge backing phases are origin 1309, eager consumer 2089 and
destination 272. The origin phase includes 846 identifier leaves; 686 have
current typed `origin_ready=false`, 139 `origin_ready=true`, 21 meta. Three
observer builds stopped at the same demand608/work4096 and restored production
source/build/bin/dependencies byte-identically. This is an ownership-cost
measurement, not an admission receipt.

Next bounded candidate for proof, not yet an implementation permission:
an identifier is a leaf. Its origin may be read directly from the current
binding row rather than allocating a separate expression node, provided the
same stable ID, shared claim, current typed value/binding, slot-token/meta
rule and `origin_read_epoch` are retained. Consumer-only identifiers already
use such a leaf path; destination and all non-identifier origin/eager work
remain unchanged. Binding-origin growth after an earlier read must still
trigger another epoch, and cached output/loan validation must reject a
mutated ID/binding/publication claim. The binding bitset pointer lifetime and
cached expression semantics are under read-only audit before source editing.

Identifier-origin result, 2026-10-17: the direct leaf read now snapshots its
one-bit origin instead of borrowing a growable binding row. The checked Slot
token (binding zero) is handled before the ordinary binding requirement; the
first version reversed this order and falsely rejected the existing
`secure_implicit_token_safe` program. The corrected binary is
`746dfa4d750770d23f7d892d3da9e65684c04d28ae1f56b0c30d4117ff971815`.
Focused phase7616/0, original 4096/4097 guards, 119 frozen analyze-only
outcomes, native MIR327/semantic3228/transpile1009, Map24/58, Slot C+LLVM,
and five C+LLVM runtime pairs pass. A direct SecureSlot positive/negative pair
is now in the summary smoke gate. The unchanged real bootstrap remains RED
before gen0, first work4096 at line48:6. The complete fixed-input census is
still 4273 signatures/3188 keys and 567 failures (554 work, 13 required
backing), incomplete0, with no changed mask or new failure versus the first
identifier candidate. This is measured progress, not DRV-2 or CI closure.

Next exact cost seam: after identifier-origin leaves are removed, the first
warm demand608 charges E135/G324/B3635; B has 388 destination, 559 origin,
2688 eager-consumer rows. All 3635 B rows are unique per routine/input/node,
but only 2377 unique routine/node pairs across input frames. Trace receipt is
WSL `.tmp/p0-current-work-trace.rQQMQv/`, source/build/bin restored
byte-identically. The existing body claims own syntax topology, publication
and currentness; backing frame nodes own per-frame effects, visit stamps and
work charges. Last consumers remain summary outputs, output dependencies and
returned-loan admission.

The six-kind passive-wrapper trial is **rejected and reverted**, not the next
implementation. It skipped backing rows and work charges for BLOCK/IF/BINARY/
UNARY/BREAK/CONTINUE. Although the same-input census fell to 387 failures
(364 work, 23 required backing) and focused/broad gates passed on binary
`f0334f2315ce9fafa65a14c2586a7c8345a762e225483af6f0365dc2e930b6f3`,
claim currency allows completed shared DAG nodes. A diamond of AST_BINARY
wrappers can revisit an O(depth) graph exponentially with no charged wrapper
work. The unchanged bootstrap also remained red before gen0. Independent
source review found this unbounded-work counterexample; the accepted source
and executable are restored to binary `746dfa4d750770d23f7d892d3da9e65684c04d28ae1f56b0c30d4117ff971815`.
Do not use the trial's 180 resolved keys as progress or raise the 4096 budget.

The next ownership decision must account for repeated per-input backing work
without turning a bounded DAG into uncharged traversal. Investigate whether
the existing backing fact's origin bitset can jointly represent multiple
formals under one current frame, preserving per-input effects, output
dependencies, pending relations, and loan validation. A bounded alternative
must name its owner, all consumers, and a shared-DAG falsifier before editing.
The 13 required-backing failures on the accepted binary are separate; the
root census key is not necessarily the failing callee entry. Instrument the
actual final return-relation consumer on representative keys before treating
them as one cause.

The bounded dead-node charge trial is also **rejected and reverted**. It
checked the existing execution publication and current claim, then returned
before creating/charging a per-frame row for a non-loan-seed unreachable
node. Binary `ba57e9754c43e9a2f97bdea2971d1b9d66d1c05edb4c6520b4167f8932645521`
passed the narrow phase/summary checks. On the exact 4273-signature/3188-key
canonical census, however, failures remained 567 (554 work, 13 required
backing), with zero resolved/new failures and zero mask changes. Receipt:
WSL `.tmp/p0-identifier-origin-census.ZXVvb2/`, with source/build/bin
restored byte-identically after the diagnostic run. It did not unblock this
rung, so no dead-node exception or test was retained.

The identifier destination leaf now reads the current claim, typed publication
and binding's destination bits/read epoch without a duplicate per-frame
expression row. It does not apply to a loan seed or to a call/member/array
expression. Last consumers remain frame outputs and output-current/loan
admission. Candidate binary
`d539d7eca05beb938cae038e08ed13783928b67362fe4670b2e34446d2460b86`
passed phase 7638/0 (including stale identifier ID, binding and publication
faults), LLVM 140/0, strict summary/4096 controls and 119-case frozen replay.
The exact 4273-signature/3188-key census has 545 failures (532 work, 13
required backing): 22 prior work failures resolved, no new failures, and two
resolved keys changed from TOP to concrete masks. Receipt WSL
`.tmp/p0-identifier-origin-census.WneTsQ/`, diagnostic source/build/bin restored
byte-identically. Broad native MIR/semantic/transpile, chain/Slot and five
C+LLVM runtime pairs also passed on this binary. The unchanged real bootstrap
was run and remains RED before gen0, with 18410 diagnostic errors; it reached
neither gen2==gen3 nor installed-driver/required CI. This is measured progress,
not canonical admission. The next operation must remove the measured repeated
per-input backing traversal behind this same owner, preserving per-origin
effects/output facts and the 4096/shared-DAG negative controls.

Re-measured on exact binary d539 and unchanged canonical input: first demand
608 stops at 4096, with 139 summary evaluations, 324 no-store graph members,
and 3633 backing charges (307 destination, 574 origin, 2752 eager consumers).
Those events cover 4052 distinct `(routine, input ordinal, node)` occurrences
but only 2617 distinct `(routine, node)` identities across 116 frames. Receipt
WSL `.tmp/p0-current-work-trace.3rGPw2/`; observer-only source/build/bin was
restored byte-identically. This establishes repeated per-input owned work;
it is not a license to charge fewer units while still doing the same scans.

Whole-chain closure map for the next implementation decision:

1. The fact owner is the existing program-point index and backing frame, not a
   new query cache. `function_param_flow_returned_backing_effects` currently
   seeds and traverses one frame per formal; `backing_origin`,
   `backing_destination_value` and `backing_consumers` produce origins,
   destination outputs, effects and pending/unsupported relations.
2. A joint frame would seed one origin bit per formal and traverse each owned
   node once per fixed-point epoch. It must retain per-origin effects,
   destination-output rows and **separate** pending, unsupported and unproved
   verdicts. `owner_store` and recursive self-call exemptions must mask only
   the matching origin bit; the other input bits still escape or await proof.
   Snapshot reads must remain stable across later binding growth.
3. Consumers to migrate together: frame/currentness and exact identity checks
   in `function_param_flow_backing_rows_current`; output projections in
   `function_param_flow_output_current` and summary evaluation; recursive
   pending sealing; unsupported/unproved queries; returned-loan and final
   required-backing admission; frame destruction. Do not leave per-input
   frames as a second semantic authority beside a joint owner.
4. Admission falsifiers: two formals with only one retained, two aliases to
   the same output, owner-slot write that exempts only its own origin, a
   recursive self-call that exempts only its matching formal, late binding
   growth after a read, shared AST diamond/cycle, 65/129-formal bit edges,
   stale/forged claim or typed publication, 4096/4097 limits, and the same
   4273-signature/3188-key fixed canonical census. The last consumer is the
   installed-driver ownership admission, not a local unit-test count.

Unknown before editing: whether one pending call edge must retain an origin
subset or can be reconstructed from the joint node bits on replay; whether
unsupported-call classification is uniformly per-origin; and which current
summary state queries require a fully complete joint frame versus a ready
projected formal. Resolve these from source and falsifying fixtures before
changing the frame layout. A partial joint/per-input dual read is forbidden.

The accepted binary's 13 required-backing failures are exactly the current
input frames of two routines: `SemanticAstCollectionFocusRecoveryFromResolvedFacts`
(8 inputs) and `SemanticAstOwnedStringDomainExposureFactsFromResolvedFacts`
(5 inputs). Each frame is unproved, not pending or unsupported. Scratch
consumer-to-producer trace pins the first incomplete call to the respective
zero-argument factories `SemanticAstCollectionFocusRecoveryFactsEmpty()` and
`SemanticBuiltinSignatureRows()`. Both have a declared-result receipt; its
frame is created but has zero traversed nodes when it refuses, placing the
failure at the no-store graph prerequisite before body proof. The two source
factories have different local mutation shapes (MapNew/struct assembly versus
ArrayPush on a nonempty literal), so do not waive that prerequisite merely
because both have zero formals. Receipts: WSL
`.tmp/p0-identifier-origin-census.apdYQb/` and
`.tmp/p0-identifier-origin-census.Ave3rG/`; production diagnostic edits
were removed and binary746 restored. Exact failed no-store predicate and
fresh-ownership proof remain OPEN.

Scratch no-store probes on the current destination binary refine this split.
Both factories have `mutable_output_required=true`; the first failing call in
`SemanticAstCollectionFocusRecoveryFactsEmpty` is its typed, zero-argument
`MapNew()`. `SemanticBuiltinSignatureRows` has no equivalent MapNew proof and
still needs a separate retained-array transition analysis. Receipts WSL
`.tmp/p0-no-store-trace.vCQTQn/` and `.tmp/p0-no-store-trace.ZeR7of/`;
source/build/bin were restored byte-identically and diagnostic code removed.
Scoped objective for the map case: consume the *existing current* empty-map
binding publication as a fresh local result fact at the no-store graph owner,
while still recording the declaration claim. The last consumer is declared-
result closure and its caller's return relation. A name-only `MapNew` test,
arbitrary zero-arg call grant, or stale/forged publication is forbidden.
Falsifiers: an exact map-factory result through a borrowed caller; changed
typed binding/initializer receipt; shadowed or otherwise unmodelled `MapNew`;
the 4096/4097 limits and same fixed canonical census. If this proof still
does not unblock a key, revert rather than adding another constructor route.
That local connection was tested and **reverted**: binary
`c7fe7205f26754632099ac3fea63ac134d13ca75023ff88c10162cea551d92ff`
kept the exact fixed-input 545 failures, with zero resolved/new keys or mask
changes (WSL `.tmp/p0-identifier-origin-census.8YgceK/`). The small fresh-map
aggregate probe also remained required-backing unready. A current empty-map
binding proves fresh `MapNew` construction but is not by itself a complete
proof for the returned aggregate's carried map; the next failed predicate
after `MapNew` was not measured. The compiler source and binary were restored
to destination candidate d539. The probe is now an explicit no-MIR negative
under `declared_empty_map_scope_unready.pgy`, not an unsound positive grant.

## Active return-provenance completion policy

The local-alias RETURN repair is a safety prerequisite, not a new precision
track. Its initial native regressions (storage, datetime, layered imports and
an enum callback) have passed unchanged on d61; canonical admission remains
blocked. Separate unsupported analysis from missing or corrupt authoritative inputs.
An existing typed operation whose backing relation is not modelled may produce
the existing conservative ALL effect lattice, never a ready empty relation.
The producer must distinguish that case explicitly; do not reinterpret every
`complete=false` or failed currentness check as unsupported. Missing/stale typed
inputs, identity/shape/claims faults, OOM, overflow and work4096 remain failures.
ALL must grow during evaluation and propagate through the active fixed point,
not be substituted only after callers consumed provisional zero. Unproved
relations cannot grant output-dependency, return-loan or storage-preservation
permission. Current typed facts, exact pending edges and batch SCC completion
remain mandatory for precise results; no selector fallback is allowed.

Scopes of this same chain: A owns backing/summary/SCC/internal representation;
B repairs only the existing formal snapshot's legitimate absent implicit-type
syntax while preserving actual Type identity and explicit-node currency; C
adds conservative-effect versus permission behavioral controls in the existing
phase unit. Root owns integration and all execution. Native semantic success
cases and strict borrowed/corrupt/work-limit negatives are unchanged. First
run build/phase/summary, then complete native tests, Map/IR and frozen review,
then the unchanged canonical and original bootstrap/install/memory/CI chain.
This policy is a plan until its producers, consumers and tests have run.

Root's analyze-only instrumented storage probe (`p0-nominal-trace.wJUTfr`)
finds StorageSave's class constructor has current call/argument rows but no
nominal-result receipt. The existing nominal producer intentionally covers
structs only. Distinguish that supported typed class target from a missing
ordinary routine publication at the checked-call producer; retain exact
declaration identity, not a name whitelist or an owned scope Symbol. This is
an unsupported-analysis witness, never a class freshness or retention grant.
An issued nominal/result receipt that becomes stale is not unsupported.
The diagnostic clone source/build/bin were restored to binary1e77 exactly.

At integration the backing TU reaches600 executable LOC against the unchanged
599 cap. Move the complete unchanged index-roots-current predicate into the
existing program-point-index owner (its actual responsibility); shared identity,
signature and every caller stay unchanged. Do not compress lines, add a new
owner/file or raise the cap. Rebuild and replay the same currentness falsifiers.

Full native replay on binaryf2c5106 passes MIR327/0 and semantic3228/0, but
the unchanged transpiler's alias/MapGet<Player> program still fails before
emission (one source case, five output assertions). Its non-Int MapGet has
no typed-map relation: the existing owner covers Int MapGet and MapKeys only.
Extend the same unsupported-effect discriminator for current typed external
stdlib results, not a new MapGet precision rule. Read issued-row presence from
the existing typed-map owner so an issued-but-stale MapKeys/IntMapGet relation
cannot become unsupported. Presence is not a permission. Preserve current
typed inputs, Void/scalar/independent-result paths, every issued-row corruption
negative and the original transpile fixture. B owns this small existing-owner
query, C its behavioral fault controls; no new owner/file or metadata fallback.

The subsequent unchanged Map integration exposes one exact diagnostic
regression: a harmless lexical defer before a borrowed field return reaches the
backing consumer's unsupported default and emits a generic error before the
required BORROW diagnostic. The existing shared input claims already own the
defer body/identity and typed flow checks it as same-routine cleanup; it is not
an escaping lambda/worker capture. A connects that already-checked body to the
same backing consumer, including nested retention and read-before-later-write
epochs; C preserves independent-positive and retaining-defer falsifiers.
The original Map negative oracle remains strict. Snapshot DpaZ7z stays frozen;
the defer delta needs a new identified build and final review evidence.

Final d61 source passes phase6423/0, strict summary/work controls, complete
native MIR327/0/semantic3228/0/transpile1009/0, Map24 C/LLVM positives and58
ownership-only negatives, HIR/RIR/MIR/AIR and all reached caps/component checks.
The actual full bootstrap still fails before gen0 (20.91s/831588KiB): first
required relation failure is JsonObjectFactIndex id57662, Option<Int> result,
not work4096. The18410 later diagnostics are cascades. Before any further
contract/edit, enumerate the complete current-owner failures on the same
sealed signature/key input. Scratch only, terminal before consumer/MIR/codegen,
original budgets and current typed/unsupported/SCC receipts preserved. The old
meta census files are diagnostic references, never a replacement source tree.
Root's first-owner trace restored source/build/bin and original hashes exactly.

The current-owner census is now complete on the identical4273 typed signatures
and3188-key manifest:452 independent failures (work218, recursive relation120,
required relation114), incomplete0. Against ce3,76 requests resolve and238 newly
fail. Its diagnostic exit86 precedes shared Type-writing consumers/MIR and is
not admission. The2736 nonfailed queries include260 reaching explicit TOP and
276 reaching unproved relations; those are not permission receipts. Root's
actual raw snapshot restored source/build/bin/dependencies/modes/status/diff
byte-identically to d61. Receipt: `.tmp/p0-return-current-census.XYzHog/run.log`
in the Linux clone; the D copy is `.tmp/p0-return-current-census-run-20261016.log`.
The prepared manifest's `production-raw/` entries were source-hash labels, not
stored backup files; they do not replace root's actual restoration snapshot.
The first coarse hooks identify identifier-origin failures and a binary-consumer
failure, not their exact producer cause. Before implementation, A prepares
scratch-only setter-level observations; B maps existing typed origin receipts;
C designs bounded success/retaining/fault controls in the existing test owner.
Do not exempt Option<Int> as a physical scalar, treat stale/missing evidence as
unsupported, weaken String backing tracking, or optimize work218 before the
next actual blocker is identified.

### Current destination transfer repair — selected after the full census

Current integration slice (2026-10-16, after binarye8a): the unchanged input
still refuses before gen0. A fresh observation on the exact4273 signatures /
3188 keys has975 failed requests:769 work,185 required backing,21 recursive
backing;60 old failures resolved and583 newly reached. This is an isolated
diagnostic census, not an admission count. Source/build/bin/metadata were
restored byte-identically. Receipt: `.tmp/p0-e8a-census.Zo8keT/run.log` in WSL;
the D copy is `.tmp/p0-e8a-current-census-run-20261016.log`.

The next dependency set is bounded to the reached typed constructor/read
chain, not all anchored inputs or a new foreign ABI policy:

- Objective/priority: restore missing typed MapNew output and Read payload
  provenance while preserving current identity, exact storage responsibility,
  eager retention and all original positive/negative gates before cost.
- Owners: successful typed LET owns MapNew's contextual HashMap type and exact
  empty-input construction; successful typed Write owns its checked receiver
  and payload. Existing publication/claims seal and replay those premises.
  Existing program-point/backing owners select and consume Read payload facts.
- Last consumers: cached output/loan permissions and final REF diagnostics,
  followed by unchanged full canonical/bootstrap/installed-driver/CI gates.
- A edits existing flow/index/backing components only: select an anchored
  backing frame from an actual current payload-dependent Read claim, including
  Void/scalar-return consumers. Consume typed MapNew and Write receipts. A
  same-owner Write-back requires the exact current original Slot binding;
  other Slot/output/Channel/opaque retention must still escape. No Slot
  identity RETURN from Read and no generic nonretaining Write exemption.
- B edits existing typed LET/Slot/publication components only: seal exact
  unshadowed zero-actual MapNew plus annotated HashMap type/LET/initializer
  syntax and identities; seal successful Write's actual receiver/payload
  types/identities/arity. Reuse existing owned syntax/currentness/retirement
  contracts; absence or mutation grants no permission. Coordinate API before
  edits. Do not infer fresh output from a call name, COPY_ONLY or argc0.
- C edits existing behavior/phase tests only: retain the currently failing
  Read payload-to-opaque Keep negative, add paired direct/field/alias and
  scalar/Void cases, same-binding Write-back versus another Slot, and exact
  receipt mutation controls. Preserve scheduler/Read-to-Log and all existing
  oracles. Root alone builds/runs/commits.
- Verification: phase and strict summary first; old full native, C/LLVM/Map/IR
  and caps next; frozen independent review and unchanged canonical after that.
  Work4096/4097 and all validation budgets are unchanged. No safety weakening,
  annotation/copy workaround, new owner file or performance track.

The first actual canonical failure is MapNew in
SemanticCallableNameTableFromNames (call node68652, source line230): its LET
annotation succeeds but the initializer value publication is absent. General
zero-argument declared functions (for example
SemanticAstEnumPayloadVariantUnproven) are a separate reached remaining
obligation: current direct-callee headers alone do not prove no-input result
provenance. Do not substitute an empty actual loop or a fake parameter ordinal;
fix their result owner only after this integration slice and falsifiers run.
The769 work refusals are not permission to increase caps or optimize before
the next actual semantic blocker is established.

Integration falsifier on binary7ad5: phase6690/1; the new opaque Read retention
negative now refuses, but exact same-Slot Write-back does not typecheck. Root's
scratch owner inspection confirms MapNew current=true and Write current=true;
TextLedger constructor current=true but checked backing relation=false and no
nominal snapshot. The frame stops incomplete before Write-back. Receipt:
`.tmp/p0_slot_write_inspect_20261016.{c,mk,sh}` (D), executed against7ad5.
The actual scheduler also reconstructs a subject, so changing tests to struct
would conceal the missing consumer. Extend only the existing exact full-field
nominal-result mapping to an outer constructed subject. Keep field payload
restrictions (including nested identity/resource/authority), no generic/default/
partial/destructure/channel fields, exact current flavor/raw flag/Types/IDs,
and all argument dependencies. This receipt describes result backing; it must
not make a subject COPY_ONLY, fresh, or safe to return/retain. B owns the shared
producer/currentness predicate, C exact same-owner success versus returned or
other-owner retention/fault controls; A's current backing consumer is unchanged.

The1283 slice passes phase6760/0, the prior strict summary, all native327/3228/
1009 tests, Map24 C/LLVM positives/58 negatives, IR/caps/component/generator.
Actual canonical is still RED before gen0 at source418:6. The completed1283
exact-input census has912 failed requests (795 work,103 required,14 recursive),
65 resolved/2 new work failures against e8a; its exit86 is not admission.
Source/build/bin/status/diff were restored byte-identically. Do not call this
baseline closed. Added safe Main/runtime output on the same-write fixture
exposed missing LLVM metadata at `Log(Read(slot).text)`. After the bounded
consumer repair below, root observed e011 phase6760/0, LLVM metadata140/0,
strict summary PASS and actual C+LLVM output `2\nkept\n`. Receipt:
`.tmp/p0-destination-candidate.0aFOKk/` in the clone. The immutable ownership
review1283/dRW4Zn precedes that runtime extension and LLVM repair.

Revised reached LLVM boundary: successful Read publishes actual payload type;
the admitted MIR expression-type owner consumes that fact. LLVM's
llvm_expr_custom_type_name CALL arm instead repeats callee/class/function
inference and misses Read before member lookup. Objective: make the active
MIR CALL consumer use that existing result type and registered LLVM nominal
identity, not the callee spelling. B edits that existing consumer only, after
tracing its callers. Within an active MIR routine, missing/unregistered type
fails explicitly without retrying the old CALL inference; preserve the
separate non-MIR API boundary, not a fact-dependent fallback. Scalar/String
results grant no nominal type, and borrowed scratch names do not escape.
C owns the exact unchanged runtime falsifier and missing/wrong typed-fact
controls. Root verifies C/LLVM output, existing nominal/Option/member/LLVM
gates, phase/summary and caps before broader integration. No AST root scan,
Read-name case, temporary-let workaround or new emitter/owner is permitted.

Independent1283 review also reached ref ReadView<String> -> Read -> opaque
Keep, including a concrete ViewRead(slot) callsite. It emits MIR with summary
READ|CALL_ESCAPE=17, so the effect producer already knows the retention.
The missing last consumers are final parameter validation and the generic
boundary validator: both skip COPY_ONLY non-String values. The view descriptor's
copyability is not permission to retain its payload. Do not reclassify every
View globally or introduce another borrow model. A may connect only the
exact current REF ReadView contract and existing sealed escape mask to those
final consumers, including deferred ref-view calls whose bodies are not typed
yet at initial admission. No new Read-presence/alias analysis or marker is
needed: ordinary view copies and direct view escape already use the existing
effect owner. Missing evidence fails closed. A
responsibility-named payload validator may project to the same diagnostic
owner, but no mode-label string or guessed Boolean may stand in for evidence.
View copy/binding, pin/await/spawn/parallel policy and no-Read/physical-read
positive behavior stay unchanged. C adds bounded Keep/return/output versus
Log/scalar view controls; all dangerous negatives are analyze-only. Root
validates these and existing view/pin gates before claiming even this scoped
connection fixed. General View authority or whole-language safety is not
closed by this payload repair. The no-input result track remains unstarted.

### Reached canonical prerequisite — declared-call result provenance

The one executable rung remains unchanged full canonical admission before
bootstrap/install/CI. Read payload repair and its scoped independent review
passed; do not extend that slice for hypothetical precision or ABI cases.

- Objective/priority: prove the reached declared call's actual return backing,
  preserving semantic identity, current typed evidence and fail-closed behavior
  before cost or patch size. Zero formals are not a freshness proof.
- Reached chain: codegen_bootstrap.sh native gen0 admission -> typed publication
  -> demanded parameter/backing index -> destination value of
  SemanticAstEnumValueScopeEmpty() -> final required backing permission. The
  existing summary/publication/index owners retain their identities/lifetimes;
  no result fact is owned by a work directive or by a fake parameter ordinal.
- Root completed the binary91 census:4273 signatures/3188 keys,912 failures
  (795 work,103 required backing,14 recursive backing), incomplete0. Signatures,
  manifest, masks and failure set match1283 exactly. Exit86 is observation only;
  production source/build/bin/modes/status/diff were restored byte-identically.
  Receipt: clone `.tmp/p0-91-census.lbfZCS/`,94.87s/1163912KiB.
- Chosen contract: an explicit routine-result facet in the existing program-
  point index, not a formal summary row. Initially it proves zero-formal direct
  synchronous bodies. Require the original current call/header/body/result Type,
  a current closed no-external-store graph AND all reachable return values closed
  by the existing sparse binding/alias/aggregate evaluator. Untracked origins0,
  no-store alone, and COPY_ONLY alone are not proofs. Missing/global/unmodelled
  sources, borrowed producers, unsupported nonzero-formal result calls and
  unfinished result cycles remain unready. This grants destination independence
  only; it grants no cleanup, copy-escape, origin_ready or lifetime authority.
- Dependency/currency: a consuming sparse node records the exact original
  result callee ID and Type. Existing current-validation queues visit the
  result facet, its claims, closed-store graph and transitive result nodes.
  A changed call, body, header, result Type, claim or missing facet revokes the
  caller's cached permission. Heap-owned frames survive recursive index growth;
  retire them and their rows with that compilation's existing index storage.
  Integration's literal-Type fault7 exposed that shared claims sealed only
  value readiness/origin flags, not the actual typed value identity. Extend the
  existing SemanticPublishedValue projection and claim snapshot/replay with its
  compilation-owned Type pointer; no separate Type/name authority or new query.
- Edit order/scopes: root first moves the complete final loan-admission cohort
  (source checks plus shared sparse loan query/wrappers) into its existing last
  consumer, type_checker_ownership_param_summary.c, and declares shared schema.
  A owns publication identity APIs, result-frame currentness and retirement;
  B owns result production and destination consumption in the existing sparse
  evaluator; C owns paired behavior/currentness fixtures in existing gates.
  No new production file/owner. Root alone builds/runs/Git; all lanes share
  phase -> strict summary/work controls -> unchanged canonical integration.
- Forbidden: argc0/empty actual loops as permission, guessed callee/result IDs,
  absent or stale facts treated as no dependency, new borrow engine/owner file,
  user annotations/copies, increased budgets, or another parallel code track.
- Falsifiers: closed empty-scope and forwarding positives; retaining local
  storage, opaque/cached/indirect/cyclic result sources and changed callee/body/
  return Type/header/currentness negatives. Required gates retain all existing
  Read, mutable-output,4096/4097 and strict diagnostic controls. Then run the
  unchanged canonical/bootstrap, not a smaller input. This prerequisite alone
  is neither self-host substitution nor V1 closure;795 work failures are still
  separately observed, not repaired by this contract.

Root integration receipt: binary71b88fdbccfa41771ea87632e992b82148062b5741f1e5225689d09d98ca89c2,
clone `.tmp/p0-destination-candidate.T89zem/`. Phase7057/0, LLVM140/0,
strict summary and4096/4097, full native MIR327/semantic3228/transpile1009,
Map24 C+LLVM positives/58 negatives, IR/caps/component/generator and Slot
goldens pass. Both new closed-result programs run as9 on C and LLVM; old91
refused both before MIR (`.tmp/p0-result-before.M4nndZ/`). Runtime receipt:
`.tmp/p0-result-runtime.dFAuMO/`; integration logs `p0-declared-result-*`.
Actual unchanged bootstrap remains BLOCKED before gen0: first refusal moved
from418:6 required backing to66:6 recursive backing,22.83s/1164432KiB.
The18410 later diagnostics remain cascades, not independent obligations.
Fresh same4273-signature/3188-key census:893 failed (803 work,76 required,
14 recursive),19 resolved/0 newly failed/16 changed masks versus91. Eight
former backing failures now reach the unchanged work guard; this is not a
performance win or full admission. Nonfailed2295 include641 TOP/644 unproved,
not permissions. Exit86,94.59s/1163860KiB, incomplete0; source/build/bin/modes/
status/diff restored byte-identically. Receipt `.tmp/p0-result-census.lxKq0V/`.
Next exact BLOCKED obligation: the existing summary/backing owners must connect
stationary recursive relations to the last output_current/return-loan consumer
without an unproved/TOP relation becoming an empty permission. All14 independent
recursive failures remain. First census falsifiers are
CodegenGenericSpecializationFactsFromSemantic input2 and
CodegenGenericSpecializationInstantiateCalls input0. Survey their complete
reached SCC before choosing an implementation; do not revive a new owner,
annotation, copy or budget increase. Published main/CI remain39a8fc1db/RED.

The first actual warm-stage SCC trace resolves that Unknown without changing
any guard: active576..587 stops at577, GenericCallFactCaptureFromResolvedTarget
input0, frame incomplete/nonpending/nonunsupported. Its first transition is
destination evaluation of GenericCallNotApplicable() at source176. The result
body's nominal constant fields include unary -1; declared-result evaluation
disabled the ordinary independent-origin shortcut and had no physical-scalar
premise. ScopeEmpty's arrays passed, but this equally current no-backing scalar
was unready. Repair the producer's whole physical-value family through the
existing typed physical-scalar owner, not a unary/-1 syntax exception. Keep
the complete no-external-store graph AND eager argument/statement traversal:
physical return storage is not permission for opaque/retaining side effects.
B owns this result producer premise; C owns scalar-field success, still-opaque/
retaining failures and current Type/physical-fact corruption/restoration.
Identity/currentness/last consumers/work limits stay unchanged. Root runs the
same phase/strict/canonical gates; never skip SCC completion. Traces are
observation-only exit86; both source/build trees restored byte-identically.
Receipts `.tmp/p0-scc-trace.xv9E5T/`, `.tmp/p0-scc-transfer.2TBOyi/`.

Physical-premise integration: binary6f6679d510dedf1b95ae825994668a5c2ae0ce7f47119a91757e910e06201bfc,
phase7082/0, LLVM140/0 and unchanged strict/work/cap gates pass, receipt
`.tmp/p0-destination-candidate.q1xcRw/`. Actual unchanged bootstrap is still
RED before gen0 (24.40s/1164388KiB), not fixed by those bounded controls.
Fresh4273/3188 census now866 failures:814 work,44 required and8 recursive;
27 resolved/0 newly failed/15 changed masks against71, incomplete0. Nonfailed
2322 includes653 TOP/656 unproved, not permission. Receipt
`.tmp/p0-physical-census.lsUcWQ/`,94.23s/1163852KiB;
production restoration is byte-identical. The warm SCC advanced from entry577
to590: SemanticError2 input0 -> SemanticErrorWithFacts input0 ->
SemanticDiagnosticCodeKnown input0 -> local SemanticDiagnosticCodes(). The
first two results are now explicit current unsupported negatives; no SCC guard
was skipped. Exact nonpending unfinished edges are recorded in
`.tmp/p0-scc-physical.HRPsEO/` and `.tmp/p0-scc-edge.FFzmMi/`.

Next bounded producer repair: SemanticDiagnosticCodes returns a current typed
nonempty Array literal of static strings. The existing no-store graph admits
only empty literals, so its caller cannot get a closed result. Keep one owner:
in program-point-index mutable-output analysis, require the original current
typed literal and recursively inspect EVERY element for actual effects/stores.
The existing sparse result evaluator separately checks every returned element's
backing. The shared claim owner already snapshots exact child topology and
actual Type; replay the same typed-array premise for all literal widths.
No literal-count/name whitelist, no erased child calls, no no-store-as-freshness,
no change to normal RETURN/CALL/CHANNEL or last loan/output consumers. B owns
that index/claim producer; C owns constant-filled success, retained/borrowed/
opaque element negatives and changed child/Type/currentness controls. Root runs
the same phase/strict and unchanged canonical gate; this is one result chain,
not an independent literal language feature or another implementation track.

The first root narrow run found an existing malformed-AST falsifier before
integration: checked_empty_array_no_output sets count=1 on empty NULL storage.
The new traversal reaches ast_array_literal_element, whose old accessor
dereferences that NULL storage; phase exits139. Extend only that existing AST
child-access owner to return NULL for absent element storage. The no-store
selector already refuses a NULL child; keep that refusal and the original
count/topology negative. Do not skip traversal or currentness. Failed receipt
`.tmp/p0-destination-candidate.RnXal6/`, binaryf01f7fd1; ASan preload locates
the accessor/selector/test stack. It is not a passed phase or candidate.

Root integrated result physical/filled-array candidatef25e6fdd7c6dfa1c8641ef0c5d2d1536358c8c54606c99c1f57d29ec5142ef4a:
phase7105/0, LLVM140/0, strict summary/4096/4097/caps PASS. Whole native
MIR327/semantic3228/transpile1009 PASS (27.76s/266600KiB), Map24 C+LLVM
positives/58 negatives and reached IR/caps/component/generator PASS (40.96s/
98320KiB), Slot goldens PASS. New declared EmptyScope/ForwardScope/static
array programs execute9/8/11 on BOTH C and LLVM. Receipts
`.tmp/p0-destination-candidate.yOfV35/`, `.tmp/p0-result-runtime.o8RpdF/`,
`/root/pgy-main-20261016.p0-declared-array-{native,chain,slot}.log`.
Actual unchanged bootstrap is still RED before gen0 (31.67s/1164040KiB),
but the first failure changes from recursive backing to work-budget at45:6;
18410 subsequent diagnostics are cascades, not independent refusals. Log
`/root/pgy-main-20261016.p0-declared-array-bootstrap.log`.
The same4273-signature/3188-key isolated census is858 failed:814 work,
36 required backing,8 recursive;8 resolved/0 newly failed/0 mask changes
against6f, incomplete0. Nonfailed2330 includes661 TOP/664 unproved, not
permission. Receipt `.tmp/p0-array-census.mQwFfg/`,107.22s/1163872KiB.
Its source/build/bin/dependencies/modes/status/diff restore byte-identically.

Before any cost repair, measure this exact binary/unchanged canonical input
at its existing demanded summary owner: separate body evaluation, no-store
graph membership visits and sparse backing node/epoch work. Preserve the
4096 limit and its4097 falsifier. Find the repeated owned operation; do not
raise the budget, change the input, weaken SCC/currentness or open general
cache/query/parallel optimisation work. Root alone builds the scratch observer
and restores production byte-identically. This remains the same blocked
canonical rung, not substitution progress.

Measured blocking operation (source50f93a7dc/binaryf25, unchanged canonical):
first warm failure demand490, active1002..1075, routine
SemanticExpressionGraphIdentifierEvidenceWithSymbolicNodes. Of4096 charges,
74 are body evaluations,162 no-store membership visits,3860 backing node
work; backing has1530 distinct routine/node pairs but3860 frame/node pairs.
Second observation distinguishes1054 charges AFTER the frame already has an
explicit current unsupported witness (ALL/unproved, no output permission),
2806 ordinary backing charges,0 merely incomplete-frame charges. Receipts
`.tmp/p0-work-trace.mAULnC/` and `.tmp/p0-work-tail.dXGSIA/`,20.60/21.05s,
1163912/1163988KiB, diagnostic exit86/no MIR; production restoration is exact.

Bounded repair objective: stop only that already-negative traversal in the
existing sparse backing owner. Priority is unchanged fail-closed semantics,
current unsupported-call witness/identity, final output/loan refusal, then
saved work. Owner: backing_dependencies; legitimate last consumers remain
unproved_current/output_current and returned-loan admission. A current
analysis_unsupported frame is a refusal, NEVER freshness or empty outputs.
Do not short-circuit merely pending/incomplete/building frames, omit full
index claim currency, alter work charging or budget, share formal identities,
cache permission, or subtract an effect. Add entry guards to destination,
origin and eager-consumer traversal only for analysis_unsupported. The summary
must remain ALL/unproved with outputs unready and the sealed refusal witness
current; changed witness/argument/child facts must still invalidate it.
Use one implementation lane sequentially, not parallel optimisation tracks.
Acceptance: strict existing4096/4097, currentness and runtime/negative gates;
new tail-work falsifier shows exact same refusal without post-refusal backing
nodes. Root reruns unchanged canonical and complete fixed-input census before
claiming any improvement; resolved budget failures do not imply admission.

Root before-falsifier on unchangedf25: phase7139/1, only tail-node absence
fails, receipt `.tmp/p0-negative-tail-before.vJ18ZQ/`. After the three explicit
negative guards, binarybd673e807aa0f755bfe90439a97dd19c9e433aa6f0fd6caf343d2c224a364b09
passes phase7145/0, LLVM140/0, strict summary/4096/4097/caps. Fault controls
cover first witness identity/argument AND skipped-tail ID/value/Type; incomplete,
pending and building nonunsupported controls still traverse and charge work.
Whole native327/3228/1009 PASS (28.25s/266624KiB), Map24 backend pairs/58
negatives/reached IR/caps/component/generator PASS (41.16s/98292KiB), Slot
goldens and three C+LLVM output pairs PASS. Receipts
`.tmp/p0-destination-candidate.j2MWIz/`, `.tmp/p0-result-runtime.XP1JWS/`,
adjacent `p0-negative-tail-{native,chain,slot}.log`. Actual unchanged bootstrap
still fails before gen0 (29.22s/1164380KiB), first failure now REQUIRED backing
at39:6 rather than WORK at45:6;18410 cascade diagnostics remain. No V1/CI
closure. Complete4273-signature/3188-key same-input census is748 failed:
702 work,36 required backing,10 recursive;110 resolved/0 newly failed/0 mask
changes againstf25, incomplete0. All110 become current conservative TOP/
unproved, NOT new admitted programs. Nonfailed2440 includes771 TOP and774
unproved. Receipt `.tmp/p0-negative-tail-census.9fzH4u/`,108.80s/1164068KiB,
exact production restoration. Earlier912/893/866/858 counts are query failures,
not the number of compiler ownership errors or whole canonical admissions.

Next BLOCKED fact/owner/last-consumer/falsifier: actual warm demand531 fails
required backing for CodegenGenericSpecializationFactsFromSemantic, with470
work (not4096). Root first-transition trace identifies CIdentifier input0,
local129 AllocatorResult(): current typed Allocator, no origin premise, no
argument. The destination consumer only knows declared zero-formal result
proof and cannot prove this builtin resource birth. Trace
`.tmp/p0-backing-first.qTNXtA/`,19.64s/1164040KiB, exit86/no MIR, exact
restore; first-owner trace `.tmp/p0-negative-tail-first.JeMMoE/` is16.95s.
Survey the ENTIRE existing allocator/builder birth -> mutation -> Finish String
result -> AllocatorDestroy -> generic-name aggregate publication chain before
choosing a repair. Do not infer builtin freshness from argc0 or make resource
handles physical/copy-escapable. Existing typed owner/let birth and publication
seams must own the premise, exact operation/Type/argument/consumption currency
must remain fail-closed. Missing contract is Unknown, not permission.

#### Resource-birth chain — bounded contract before editing

- Objective/priority: connect the already checked local Result allocator and
  TextBuilder constructor contracts to canonical String-name production,
  preserving ownership identity and refusal first, then admission. This is a
  prerequisite of the same canonical rung, not a new resource language feature.
- Source86767e3fe/binarybd; root's before controls are
  `.tmp/p0-builder-before.ccF5qz/` (Result positive, Pool and opaque negatives
  all currently unready/no MIR). The earlier QNR temporary-argument probe is
  superseded, not a valid isolated constructor falsifier.
- Whole chain: CodegenGenericSpecializationFactsFromSemantic -> qualified and
  specialized C symbol -> CIdentifier; Result allocator birth + builder birth
  -> Append -> Finish -> Destroy -> String result -> local names arrays ->
  returned aggregate. Existing function_param_publication owns checked call,
  actual Type/kind/arity/IDs/current generation. Native builtin checking owns
  successful signatures, builder local ownership and consume; no Symbol may
  survive into the query. Final consumers remain sparse output/loan and REF
  admission, then unchanged C/LLVM/bootstrap.
- Runtime evidence: Result starts poolNULL, Finish detaches builder data on
  that branch, Destroy frees only pool storage. Pool-backed Finish/Destroy is
  explicitly OPEN in04_ownership_abi. Allocator is rebindable; an original
  Result initializer is NOT a persistent Result-kind lifetime witness.
- Selected query: exact CURRENT direct checked AllocatorResult -> TYPE_ALLOCATOR
  with0 arguments, or TextBuilderNew -> TYPE_TEXT_BUILDER with1 current
  Int/Long capacity. Result must be nonmeta; no declaration/value target or
  unknown kind/Type/arity may qualify. Its limited intrinsic contract says
  the constructor result has no caller backing and the constructor does not
  retain caller storage. Argument evaluation is a separate mandatory effect
  traversal. No new stored field/top-level fact/owner is needed.
- Consumers: mutable-output analysis consumes only that no-retention half and
  recursively checks EVERY argument; sparse destination/origin consume only
  result backing independence. Keep exact full index claims/call/value/child
  currency. Do not derive either premise from the other. Do NOT change
  origin_ready/copy_escape/physical, generalise other Allocator constructors,
  propagate Result-kind to identifiers, bypass a later assignment, or grant
  Finish lifetime/cleanup/transfer permission. Pool and opaque RHS remain
  unready, even after Result -> Pool rebinding. No name/argc-only fallback.
- Required changes/dependency order: A implements the query in existing
  publication_routines plus owner header; B consumes it in existing
  program-point-index/backing owners; C adds the paired fixture/currentness
  controls in existing phase/strict gate. Independent edit scopes share the
  exact query signature agreed below. Freeze together before root builds.
  No source annotations, copies, manual restoration or new production files.
- Acceptance/falsifiers: Result builder copies a real borrowed String and
  returns independent output (C+LLVM runtime9); direct Pool, Result->Pool,
  opaque allocator, handle copy/return and nested retaining capacity refuse
  with no MIR. Kind/target/declaration/value ID/arity/actual Type/meta/presence
  corruption rejects the direct query AND cached output, restoration restores
  only the original permission. Existing borrowed/opaque negatives,7145 phase,
  strict4096/4097, native/Map/IR/Slot and unchanged full canonical are root's
  shared integration boundary. Whole fixed-input census remains mandatory;
  no partial green becomes V1/CI or C-path substitution.

Shared query signature:
`bool semantic_param_publication_checked_local_owner_constructor(const SemanticContext *ctx, const ASTNode *call);`

Resource candidateec49bf0c3dbfa942e91d3639653ca67358ccc1b0d2c088f5ca0d8540000b0878
is SOUNDNESS_BLOCKED, not publishable. Frozen independent review sm2qHi
(report4fcb5df78005dbc4f89ce88ca39a919423c1b537181a2fd067a47363132bc343)
finds new nonzero-formal Pool/opaque helper and Result->Pool/branch compositions
admitted through TextBuilderNew and the old unconditional Finish independence
grant. The previous bd binary refuses those complete programs. No unsafe
program was executed. The narrow/broad receipts below do not excuse this
missing lifetime premise. Do not start the subsequent enum/meta gap.

The same full4273-signature/3188-key ec census completed with708 failures,
incomplete0, diagnostic exit86/no MIR,117.21s/1163996KiB; receipt
`.tmp/p0-owner-birth-census.WLH5M0/`, exact production restoration confirmed.
This diagnostic count is NOT acceptance progress while soundness is blocked.

##### Finish lifetime repair — one bounded producer chain

- Objective/priority: remove the unconditional Finish lifetime grant before
  admitting resource births. Identity/current lifetime, fail-closed behavior,
  then canonical admission; no source-copy/annotation workaround.
- Owner: existing typed parameter publication stores the checked local
  AllocatorResult birth and monotonic invalidation on replacement or an
  unmodelled allocator argument use. This is a premise of the existing value
  publication, not a new top-level family or allocator kind inference engine.
  Native local `let` immutability is NOT sufficient evidence by itself.
- The bounded witness is an exact typed local binding -> current direct
  AllocatorResult call. Any assignment invalidates it, even Result->Result;
  any call argument exposure except exact checked Finish/Destroy invalidates
  it. This conservative premise does not follow helper results, branch joins,
  opaque returns or parameter origins. A Result-only wrapper is Unknown until
  the existing owner can prove its current returned allocator kind; it must
  not silently become Pool permission. No fallback to the original LET or
  an owned-String summary bit is allowed.
- Finish consumes that receipt through indexed-string classification,
  origin_ready/copy_escape publication and replay, unmodelled-call refusal,
  and collection-owned-String summaries. Missing lifetime evidence cannot
  authorize independent escape. The last consumers remain sparse
  origin/destination, stored/returned String admission and REF loans.
- Required edits: A owns publication header/routines/owner implementation;
  B owns indexed-string header/implementation and collection result summary;
  C owns currentness units, strict gate and allocator-wrapper fixtures. Root
  wires typed local initialization/replacement callbacks and maintains the
  shared integration/replay manifest. No shared production file edit overlap.
  All source freezes before root builds; only one implementation rung exists.
- Queries: `semantic_param_publication_record_result_allocator_binding(ctx,
  declaration, type)`; `semantic_param_publication_invalidate_allocator_binding
  (ctx, value)`; `semantic_param_publication_checked_finish_independent(ctx,
  call)`. Indexed-string independence now takes the same semantic context.
  Recheck exact binding/initializer/callee/Type/argument/current generation;
  never keep a scope Symbol pointer or grant allocator copy/cleanup rights.
- Falsifiers: direct/helper/branch/rebound Pool, opaque/unknown origin, same
  constructor spelling with wrong identity, inout or unknown call exposure,
  and stale birth/actual/binding facts must not publish independent Finish
  results or cached output permission. Safe direct Result still executes9 on
  both backends. Existing strict negatives/budgets are unchanged; all98 frozen
  review probes are replayed before broader native/Map/Slot/bootstrap/census.
  Pool-local nonescaping use and Result-only wrappers are not promoted to a
  new supported contract by this bounded repair. Record any precision loss;
  do not label V1 or whole-language memory safety CLOSED.

Root independently replayed all98 ec frozen exit/MIR outcomes and checked
source/probe manifests: `.tmp/p0-review-resource-birth-replay.JIM7KX/`.
The review soundness blocker is reproduced, not merely reported.

The bounded repair binary5d8f0e9b39a5776ad6358067fb1010ff588c53e2bc29244b1d35d962d8441a59
passes phase7318/0, LLVM140/0, frozen strict/caps (KQxADu), full native,
Map/IR/component/Slot and four C+LLVM runtime positives (xXKxWe).
Root's98-probe replay tWRykY changes exactly7 permissions to refusal:
5 Pool/opaque/branch safety cases and2 genuinely safe Result-only allocator
helpers, explicitly unproved by this bounded witness. No old rejection
becomes admission. The previous scratch replay QHpJoi expected the safe
zero-formal allocator wrapper to remain admitted; that expectation was
incorrect for this documented conservative contract, not a safety failure.
No code/oracle was relaxed to recover those2 helper permissions.

One reached consumer obligation was discovered before committing: the
collection-owned-String body bit can upgrade ordinary ArrayPush to an owned
push, but its consumers only replay the producer ID. Normal source typing
creates the bit after the entire function and sees all monotonic invalidation;
a later raw receipt/AST mutation can nevertheless leave that bit stale.
Revisit this same chain, not the next enum gap. Existing collection result
analysis will record its already-visited returned String source calls once,
plus an owned syntax snapshot of that producer body, in the existing typed
publication storage. Querying a result will replay those source witnesses
and transitive owned-result dependencies rather than rescanning the AST body
or trusting the bit. Unknown/missing/stale evidence fails closed with no
bit-only fallback. Publication's owned push consumer must replay that same
result evidence. A owns the typed carrier/destruction and push replay;
B owns existing collection result production/consumption; C owns the
zero-argument producer stale-witness falsifier. The same narrow/full gates
are required after the new shared freeze. No new production owner/file,
generic cache, allocator kind propagation or extra language surface is added.

Root reproduced the stale body-bit and private-push problem in YENU8z:
phase7331/2, exactly the two expected missing revocations. Final same-chain
repair binary604bc1702f723fea3f1078edb932af771581f400f295ed7fb65b91d84bb1362e
passes phase7450/0 and LLVM140/0; narrow receipt WmQycK contains the unchanged
strict/budget/cap gates. Root full native MIR327/semantic3228/transpile1009,
Map24 C+LLVM positives/58 negatives plus IR/component/generator and Slot
goldens all PASS; runtime J2gUpo executes9/8/11/9 on C and LLVM. Native30.01s/
266828KiB; reached chain44.24s/98204KiB; Slot8.81s/166756KiB. Root replay
zrxYkM rechecks all98 frozen ec probes with the same7 intentional refusals
and no rejection->admission. No unsafe program was executed.

Actual unchanged bootstrap remains RED BEFORE gen0:27.24s/1245668KiB,
primary32:6 required backing and18410 cascaded diagnostics. Exact full census
OxbwCQ (4273 signatures/3188 keys) is708 failures:698 work/10 required,
incomplete0,105.20s/1245328KiB. Versus committedbd:40 resolved/0 newly failed/
0 mask changes; versus unsafeec:0 changes. Nonfailed2480 includes811 TOP/
814 unproved; these diagnostic counts do not grant permission. Observer
exit86/no MIR, source/build/bin/dependencies/modes/status/diff restored
byte-identically. Installed-driver/gen2==gen3 are not reached or passed.

The frozen RXLKQ8/binary604 independent review, source.diff
61223eadb1171fe849faa60b626818b98c6e5e36034c63f361ff0ab48326f6f5,
found no new false admission in its bounded Finish repair; 119 analyze-only
cases were 31 MIR/88 refusals, and root replayed their outcomes. Its separate
Pool/Destroy read case is an existing OPEN: both prior sound binbd and bin604
admit the same29532-byte MIR, with no unsafe execution. This review is not a
whole-language safety approval. Published39a CI run38860052641 was refreshed:
completed/failure. No V1/SoT closure or published-green claim.

##### Next canonical prerequisite — exact unit-enum qualifier edge

- Objective/order: unblock the unchanged canonical admission at
  SemanticAstNativePipelineOnlyStatementRefusal:32:6 without turning a type
  namespace into a fabricated runtime value. Semantic identity/currentness,
  fail-closed behavior, then bootstrap/install/CI. Source checkpoint78e19dd80,
  tested binary604; no other implementation track is open.
- Whole chain: enum registration owns the enum declaration ID and Type;
  its unit variant Symbol shares both. Existing static-member resolution
  selects a symbol but publishes only the parent meta Type. Sparse eager
  traversal subsequently demands a runtime value for the untyped qualifier.
  Typed member resolution must preserve the successful exact relation;
  sparse no-store/origin/destination/eager and claim replay consume it.
- Bounded fact: a simple namespace identifier resolves to an actual enum
  type Symbol. Source inspection corrected the initial route assumption:
  enum registration declares the bare variant Symbol, not a synthesized
  `Enum_Variant` alias, so a valid `Enum.Variant` selects that bare Symbol.
  The existing selected Symbol (whether the flat lookup found it or the
  established bare lookup did) must be the exact named unit variant of the
  SAME enum declaration/Type as the qualifier, and the registered enum AST
  must contain its exact ordinal/name with payload count0. Bare spelling
  alone never proves that relation; `First.Two` selecting a `Second.Two`
  Symbol remains unproved. The existing
  host-declaration index supplies the declaration; no AST/program root scan
  or new variant ID is introduced. Keep scope Symbols transient.
- Carrier/currentness: optional parent-owned receipt in existing publication
  holds qualifier/parent IDs, exact enum declaration/Type, selected ordinal,
  names and payload premise. Recheck parent/member spelling, qualifier
  topology/name/binding, declaration ID/name, Type, variant count/name/payload
  and current generation. A missing/changed receipt cannot keep cached
  permission. The qualifier still has a syntax edge and snapshot; only its
  execution edge is absent. Existing semantic claims seal/replay this fact.
- Forbidden: uppercase heuristic or bare-member spelling alone as authority,
  class/global variable/runtime enum receiver, payload constructor, generic
  all-meta skip, all-enum physical classification, fake qualifier typing,
  lost input evaluation or weakened budget/loan/negative policy.
- Edit scopes: A owns existing typed member producer/query, publication
  carrier/value replay/destruction; B owns sparse/index/claim consumers and
  the existing flow claim carrier; C owns minimal enum fixtures, strict gate
  and currentness units. No shared edited production files. Root alone
  builds/tests/WSL/Git, fixes any new dependency map before expanding a patch.
- Shared query:
  `semantic_param_publication_checked_unit_enum_selection(ctx, member)`.
  It proves the one selected parent/qualifier relation, not enum value
  ownership in general. Binary still evaluates both operands.
- Acceptance/falsifiers: actual minimal unit-enum comparison with a retained
  nonphysical result must be admitted; runtime receiver/cross-enum fallback/
  payload/global-storage uses must not inherit that permission. Raw identity,
  member/ordinal/Type/declaration/payload or missing receipt faults revoke
  direct and cached proof; restore recovers only original permission. Run
  narrow phase/strict/budgets first, then full native/Map/Slot/C+LLVM runtime,
  actual unchanged bootstrap, complete fixed-input census if it still fails,
  installed C+LLVM/gen2==gen3 and exact-published-SHA CI when reachable.
  No syntax/type-invalid probe counts as an ownership negative.

Enum integration receipt (local candidate7f04442a, not published): the valid
`Purpose.InspectOnly` source probe was refused by prior bin604 at its
nonphysical `Probe` return backing, then admitted by the new binary. The
producer seals the actual qualified syntax and same-declaration unit variant;
the qualifier remains a syntax child but contributes no execution edge.
Phase7606/0, LLVM metadata140/0, strict summary including unchanged4096/4097,
semantic/header caps, native MIR327/semantic3228/transpile1009, Map24 C+LLVM
positives/58 negatives, reached component/IR/Slot gates, and C+LLVM runtime
output9 for the new fixture all passed. The previous four runtime controls
also passed on both backends. The frozen RXLKQ8 119 analyze-only cases had
identical exit/MIR outcomes on candidate7f. Current frozen enum review is
7FAhzE, bin7f04442a, source.diff982da2c6; independent verdict pending.
The unchanged real bootstrap still fails before gen0 at the 4096 recursive
summary work guard (30.13s/1256780KiB, 18410 cascading diagnostics). The
isolated full-input census completed with exactly4273 signatures/3188 keys,
failed707=698 work+9 required backing, incomplete0, no MIR, and byte-identical
production restoration. Compared with the prior bin604 census on the same
input, one enum backing key resolved, zero new failures and one expected mask
change. This is one admitted prerequisite, not canonical/DRV-2 or CI closure.
Next work remains the reached existing summary work owner; map its complete
dependency/cost chain before editing, without raising the budget.

The ec candidate
passes phase7246/0 and LLVM140/0. First narrow run h3L46x exposed a NEW test
classification error, not MIR admission: Pool/opaque/rebind were already
required-backing-UNREADY before the repair, not completed borrow-only refusals.
Those3 new cases now require their exact existing owner failure, structured
semantic errors and no stdout/MIR, permitting only existing BORROW extras.
Old exact-BORROW paired oracles are unchanged; retaining capacity stays paired.
Root invalidated K3GUKb because root copied the tightened test during execution
(shell continuation offset changed). That run is not semantic evidence.
Final fully frozen W61m0w narrow run passes phase7246/0, LLVM140/0, complete
strict summary including UNREADY/capacity and4096/4097, plus owner caps;
source manifest matches. Actual canonical and broad integration remain pending.

The current scoped red-team request is frozen at lNEApm/binarybd673/source
diff694f1e5320aac5899918cb981eccf527af2d67ed44a1676f1a3bbabdbfb6225a,
not the live overlay. Read-only/analyze-only review is complete:68 cases,
18 with MIR and50 refused, previous46 unchanged, no newly found false
admission in scope. Report SHA256
e92f841fd0f044687989df53f63bb627f75e063bf9491743f8f0dd931f3fe280.
Root checked full source/probe manifests and independently replayed68/68
exit/MIR outcomes, receipt `.tmp/p0-review-negative-tail-replay.f8zWZa/`.
In-memory stale-fact units were not independently executed by that no-build
review; root's phase covers them separately. Canonical/CI are not approved.
No review model/settings/build/process authority was changed or granted.

Frozen declared-result independent review only covers committed binary71,
snapshotGKFUTd/report2e99dfc5. Root rechecked its full manifest and replayed
all46 analyze-only cases with matching exit/MIR results, receipt
`.tmp/p0-review-result-replay.Bjyrpt/`. No new false admission was found in
that scope. Ordinary nonzero-actual fresh local-result admission existed on
both91 and71; the nonzero-formal unready rule describes the new declared-
result facet ONLY, not every ordinary backing path. This is neither current
f25 independent review nor canonical/CI approval.

### Reached local Read payload consumer — scoped checks/review pass, canonical open

Independent fd17/kGgLNi review confirms the REF repair but reaches a stronger
local falsifier: Main owns Slot<String>, creates ViewRead, reads its payload,
stores it via a bodyful inout Save, releases the Slot and reads the output.
MIR/C/LLVM emit all succeed. Local return and Keep/wrapper/pin variants also
emit. No unsafe program was executed; physical UAF is not established. The
whole active boundary must include local and default-formal Read results,
not just another formal REF exception. No-input result implementation is held.

Root's exact fd17 preflight admitted all seven new paired inputs, with rc0,
no diagnostics and actual MIR markers: four escape falsifiers and three safe
read controls. Receipt: `.tmp/p0-local-read-before.oDjN34/` in the WSL clone.
A/B/C subsequently froze the one-chain implementation and tests for root's
actual build/phase/strict gates. No unsafe probe was linked or executed.

Root-observed candidate91cfd12ba490463e41a40807e896be8f48965a58aecc7ef3210454e375978d9f
passes phase7009/0, LLVM140/0, unchanged strict summary/4096-4097, native
MIR327/semantic3228/transpile1009, Map24 C+LLVM positives/58 negatives,
reached IR/caps/component/generator and Slot C+LLVM goldens. Receipt: clone
`.tmp/p0-destination-candidate.loYLzi/` and `p0-local-read-{native,chain,slot}.log`.
Actual unchanged bootstrap remains RED before gen0, same418:6 required backing
first refusal,18410 cascades/16 warnings,24.08s/1164676KiB. This scoped native
repair is not self-host substitution or V1 closure. Immutable DO97fm review
is complete (review.md SHA2568f0c3ef412906292fa39e98fdccb56b157083deeb307e7a3d2d2bef610da0047).
Root verified the frozen manifest and replayed all101 analyze-only inputs;
exit/MIR outcomes match101/101 in `.tmp/p0-review-replay.g04KwZ/` in WSL.
The prior P1s refuse and no new dynamic unauthorized payload escape was found
in that scope. Dead Read over-refusal, safe generic/defer backend output gaps,
no-Read descriptor/FFI lifetime and malformed-birth diagnostic quality remain
explicit limits, not proof of whole-language or runtime safety. These are not
new parallel precision/emitter tracks. No-input result implementation still
requires its whole-chain contract before edits.
Source recovery checkpoint:6f54b4b53ce4a2ad19fd5d875aff4becf0262d19 on main,
not pushed. Only coordination documents follow that source checkpoint.

- Objective: connect issued checked Read payloads to final eager escape
  validation using existing loan and publication owners. Priority: exact
  current source identity, preserved storage responsibility and nonretaining
  positives, fail-closed missing facts, then measured cost.
- Producer: existing successful typed Read publication, with explicit issued
  local payload-loan registration/lifetime in that same owner. Read container
  identity and payload-backing facets remain distinct. No new fact owner.
- Consumer: existing 1x1 sparse loan evaluation, with the actual Read CALL as
  seed rather than a fake callee/formal ordinal. Reuse indexed dependency
  roots/claims and origin/eager/SCC/work behavior. Final publication consumes
  every issued loan, including Main with zero formals, before MIR/codegen.
- Contract: the proposed Read entry certifies only absence of eager
  RETURN/CALL/CHANNEL/output retention. It cannot use the ordinary REF-return
  loan's static-source exception, or grant complete lifetime, last-use or
  source cleanup permission. Current physical results are excluded through
  their owner, not a name/shape guess. Same-owner Write-back requires the
  exact current owned-Slot receiver identity; a View does not grant that
  exemption and existing live-view Write guards stay intact.
- Forbidden: local initializer/alias/Read-presence rescans, global View
  reclassification, Read in PendingRefCall using a fake ordinal/callee,
  skipping a missing/stale issued row, generic zero-argument freshness,
  annotation/copy workarounds, another owner/file, weaker negatives/caps or
  an assertion that occupancy-only Release proves physical cleanup safety.
- Required independent edit scopes: A existing backing/loan evaluator and
  final diagnostic consumer; B existing typed publication registration,
  current queries/view and retirement; C existing phase/strict harness and
  bounded paired cases. Shared header API and any complete responsibility
  relocation must be fixed before parallel edits. Root alone integrates and
  runs every gate; do not start a parallel semantic implementation track.
- Falsifiers: local plain Slot and scoped/copy View Read -> inout Save,
  RETURN, opaque Keep and nested wrapper; current row/caller/receiver/type
  corruption; preserve Log/Length/physical reads, local copies, ref helpers,
  secure/pin behavior and exact same-owner subject Write-back. Unsafe cases
  are analyze-only with exact BORROW and no MIR, never a syntax/ABI failure
  counted as safety. Existing phase/summary/4096-4097/native/Map/IR/Slot/pin
  gates precede unchanged canonical/bootstrap/install/memory/published CI.
- Root corrected the callerNULL hypothesis before edits: top-level String/Int
  Read positives emit MIR/C/LLVM onfd17 (`.tmp/p0-top-level-read.C3zEQw/`),
  and the parser already synthesises Main before semantic checking. The typed
  row inspector confirms actual FUNC/Main caller, Readid19/callerid2, current
  backing1/depends1/retains0. Publication records the actual callable owner,
  not ctx.current_function_decl. No new PROGRAM model is justified; preserve
  the positive and distinguish lambda/intent/deferred execution owners.
- Newly reached prerequisite before edits: the successful owned-Slot claim LET
  bypasses generic value publication (root observes ClaimSlot value.ok0).
  The shared full-body loan visitor cannot skip that missing input. Existing
  successful resource/Slot claim ownership must issue one exact birth receipt
  for annotated/generic LET and destructure forms, sealed by shared syntax
  including generic-call arguments. This is current allocation input-backings
  evidence, not generic argc0 freshness or post-release permission. It must
  retire with existing publication/claims; never retain scope Symbols.
  Existing source lifetime after Release/lease end and
  nested callable ownership remain separate unproved obligations. Native
  REF/default View ABI emission gaps are explicit, not a new emitter track.

Selected implementation boundary after that survey: actual FUNC owners,
including parser-synthesised Main, reuse the sparse evaluator. Preserve the
actual owner shape for lambda/intent/deferred/fragment cases; unsupported
execution proof fails explicitly and grants no loan permission. Do not add
a PROGRAM model without an actual admitted execution falsifier. Typed
physical Read results are not payload-loan obligations. No whole-lifetime
permission is inferred from an eager no-escape result.

Shared API/edit agreement for this one chain:

- Reached producer prerequisites are part of this same slice: successful
  ViewRead must publish its receiver-dependent descriptor result, separately
  from Read payload identity; implicit secure token binding0 needs the exact
  successful paired-token operand witness, not a fabricated binding or a
  general TYPE_TOKEN exception. Reuse existing publication syntax/currency
  and keep capability/cleanup permission separate from backing independence.
  Root moves the complete compilation-owned fact allocation/retirement cohort
  from store_destroy.c into existing fact_snapshot.c, with identities and
  lifetime unchanged. This makes room for A's complete ordinary REF-return
  source-admission cohort in the existing backing-state component. No new
  semantic owner/file, cap increase or compressed code is introduced.

- B owns PendingReadPayloadLoan in the existing publication header/store/view:
  issued actual call/caller identities and shape, current result Type/kind and
  owned diagnostic module lifetime. Current Read result/receiver/auxiliary
  arguments and both relation projections are revalidated. Every issued row
  is consumed or fails, never dropped because currentness became unavailable.
  `semantic_param_publication_read_payload_loan_current` returns only current
  input facts and an exact owned-identifier Slot binding, or0 for a View; it
  does not prove no escape. Never fabricate an ordinary callee or ordinal.
- B's single `record_owned_slot_binding` / `owned_slot_binding` API covers
  AST_LET_DECL and AST_LET_DESTRUCTURE. SemanticPublishedSlotBirth carries
  exact slot/token binding IDs and Type pointers. Ordinary implicit secure
  token ID0 is distinct from no token and cannot become a fabricated ID.
  Successful existing canonical claim checks own issuance. Seal annotation
  or generic-call syntax, actual binding topology, owned Slot access/security,
  inner Type, zero actuals and builtin identity. Generic CALL arguments use
  the existing shared syntax capture/current vocabulary. Query failure cannot
  turn into generic no-input independence. Birth claims join cached currency.
- A owns the common sparse evaluator and
  `function_param_flow_read_payload_no_escape`, existing final consumer and
  diagnostics, and any shared-flow-header field needed by B. Read entries
  accept only proven no-retention effects, never the REF-return static-source
  exception. Exact source Write-back and successful birth binding initialisers
  consume B's current facts. Token arguments with no stable binding must use
  an existing exact typed non-backing witness, not an invented identity.
- B edits publication components/headers, successful claim LET/destructure
  hooks and shared input-claims syntax/current only. A edits backing/index,
  flow internal header and final diagnostic/parameter consumers only. Root
  owns shared destruction integration; neither agent races on store_destroy.
  Any cap-driven move is one complete responsibility into an existing owner,
  reported before editing; no new production file or line compression.
- C owns existing phase unit/strict harness and bounded local/inout/copy/
  wrapper/default-view versus nonretaining/same-owner/physical/pin controls.
  Existing fixtures and strict error oracles stay unchanged. First establish
  that a new negative is an admitted typed path onfd17; never run it. Safe
  actual Main/top-level pairs may run on C+LLVM after semantic admission.

This authorises parallel edits only within that shared producer-to-consumer
chain. Root replays the fixed existing gates before widening integration and
revisits the map if another reached owner changes this contract. It does not
authorise no-input result inference, a new borrow model or an ABI/codegen track.

Base: local source checkpoint `c64719068995a7ccf59504d56da87ed05facf80d`,
matching native binaryd61 and the unchanged canonical input. Root's detailed
observation `.tmp/p0-return-detail-census.FDnGDU/run.log` in WSL completed all
3188 keys,45.44s/831316KiB, exit86/incomplete0; every key's mask, failure and
work match the prior census. Original raw source/build/bin/status/diff were
restored exactly. This is diagnosis, not canonical admission.

- Objective: preserve checked RHS destination aliases through returned values
  and recognise whole-value replacement of default value parameters, without
  confusing either fact with absence of backing provenance or retention.
- Priority: current identity/typed evidence, correct origin and destination
  transfer, preserved eager effects/ref/inout/resource boundaries, then cost.
- Owners: the existing typed assignment/publication owner produces replacement
  eligibility; the existing program-point index replays it against the actual
  routine/formal identity; the backing frame owns input-origin and destination
  transfer. The checked result/nominal and sealed callee RETURN owners retain
  their existing identities. No new semantic owner or source annotation.
- Last consumers: backing_store and output matrices, stationary SCC completion,
  returned-loan admission and final borrowed-escape diagnostics; then the same
  canonical/bootstrap/installed-driver/CI chain.
- Forbidden: treating a CALL base as a writable place, interpreting relation
  query failure as no dependency, guessing a field ordinal, dropping eager
  effects, clearing input origins because destinations are empty, or making
  ref/inout/host/using/resource places into local replacements. No lookup/type
  publication rewrite: the earlier receipt-erasure hypothesis is unconfirmed.

Detailed first false setters on failed keys:267 store_destination (including
default `start`, `value_type`, `local_bindings` parameters),34 place_binding
(call-result member/index aliases),151 work_budget. These are observations
of the first reached setter, not promises that301 independent queries will
resolve after the repair. In JsonObjectFactIndex the actual first setter is
UnwrapOption(key_read).value at line169; the older identifier-origin marker
was downstream contamination. The separate minimal Find/Some/None positive
already emits MIR on d61 and its eager retaining counterpart refuses with
exact BORROW_ESCAPE; Option itself is not the missing language feature.

Independent edit scopes on this same chain:

- A: existing backing_dependencies and backing/SCC/internal components only.
  Transfer destination sets through current checked dependent call arguments
  and projections, conservatively union multiple dependent inputs; ordinary
  calls also need their sealed RETURN relation/pending evidence. Unknown or
  stale relations grant no empty destination permission. Preserve all origin,
  eager consumer, read/growth and actual write-place checks. For an admitted
  replacement, merge local binding origins without claiming a storage write;
  never mark the entire formal or its member/index writes local. If physical
  caps block this, move one complete existing responsibility within existing
  components after reporting its exact boundary; no new owner/file or cap raise.
- B: type_checker_assignment and the existing program-point replacement fact
  only (publication components/header only if the existing API is insufficient).
  Admit whole-binding replacement of current PARAM_MODE_DEFAULT formals,
  excluding ref/inout/host/resource/using aliases; verify stable formal IDs and
  current typed routine/mode. Do not edit A's backing components or name lookup.
- C: existing phase unit, summary gate and a bounded pair of behavioral
  fixtures only. Establish the old failure on an actual checked call-result
  projection, preserve a retaining/output-alias counterexample, and cover
  default parameter rebind versus ref/inout/member/resource writes. Reuse
  existing currentness hooks, adding only new premises; no generic-error or
  unchanged negative-oracle relaxation.

Root alone builds/runs and integrates. Run narrow falsifiers/phase/strict summary
first, then unchanged native/Map/IR/caps, frozen independent review and the full
canonical. Re-enumerate only if a newly reached owner changes the decision.
Work limits remain4096/4097. If cost is the next actual canonical blocker,
measure the repeated owned operation before selecting a bounded optimisation.
Bootstrap/gen2==gen3/install/memory and exact published-SHA CI remain required;
this repair is not SoT substitution or V1 completion by itself.

Reached dependency during integration: direct-call destination transfer also
consumes class construction. The existing unmodelled-class receipt does not
prove complete sources: C/LLVM admit omitted/default fields and destructure
initialisation. Do not infer an empty alias set merely because actual inputs
have no queried origin. B therefore extends the existing nominal result mapping
only to nongeneric plain classes with the complete unique actual-field mapping,
no defaults/destructure/channel, current argument/result Types and declaration
identity. Snapshot flavor seals struct/class identity; old struct-flip faults
must remain failures. Struct arity semantics are unchanged; partial/default/
generic class cases stay unmodelled. The existing ordinal and new complete
query share one result-current predicate. Complete zero-field nominal input
mapping permits an empty destination set, not freshness or purity; ordinary
zero-argument descriptor calls without proof remain unavailable. C preserves
these distinctions, rather than widening a generic-error oracle or changing
native positive inputs. Root's work-accounting review also requires new
destination visits to use the existing epoch/work owner and MapKeys to consume
the existing typed independent-result fact, never an absence fallback.

Reached dependencies in the first integration build: binarye9 phase6551/24
failures became binary7328 phase6557/10 after restoring raw/semantic nominal
flavor agreement and correcting two new class fixture declarations. Existing
oracles are unchanged; no broad gate or canonical acceptance is claimed.

- The exact typed Channel LET succeeds at the existing channel-binding owner;
  it is not an ordinary typed CALL. Destination analysis must consume a
  current receipt from that immutable LET/Channel<T>/one Int capacity boundary,
  not invent generic call freshness. B owns producer and receipt/currentness;
  A consumes it. The receipt must seal declaration, initializer, argument and
  type identities and become unavailable if any premise is stale. Existing
  channel send effects and missing-fact refusal are the integration falsifiers.
- Required return-backing frames own the RETURN facet, including SCC pending
  iterations. Do not publish coarse named RETURN first and then permanently
  OR it into later precise results. A repairs that existing summary join:
  actual frame origins remain monotone; CALL/CHANNEL/eager effects remain
  accumulated; pending or incomplete frames grant no output/loan permission.
  ReadRecord's Option<Int> projection is the nonretaining falsifier and
  StoreRecord/inout plus original retaining alias/SCC negatives preserve the
  opposite boundary. This is not descriptor-as-scalar classification.
- C owns only tests for these newly reached premises, reusing current phase
  and strict summary gates. Root retains the single build/integration lease.
  No new owner family, work-limit increase or annotation/copy workaround.

Receipt reuse is part of this same obligation: A extends the existing input
claim's LET snapshot/replay to seal the channel-binding query result in both
directions. A cached frame must reject a changed receipt even when syntax is
unchanged. B reuses the existing owned syntax-claim snapshot for the Channel
LET, because type-resolution metadata alone accepts same-ID syntax drift.
Its existing retirement path destroys that snapshot. C mutates and restores
these exact premises; this does not open an independent claims/cache track.

Observed remaining falsifier on binary6affa: phase6604/2, receipt fault7
(`GenericParam.name` String -> Int at unchanged pointer/ID) leaves current=true
and CHANNEL32. Receipt `.tmp/p0-destination-labels.3RDNqB/phase.log` confirms
the exact case. Existing syntax claims seal generic count and constraint/default
children but omit the argument names themselves. A therefore completes that
same AST_TYPE syntax snapshot/current/destroy responsibility: owned ordinal
names, explicit malformed/allocation failure, replay and retirement. The
Channel LET query and cached summary remain the last consumers; no name-based
typing substitute or new owner. Other source scopes stay frozen until the
unchanged phase/summary gates are rerun by root.

Next integration boundary, binarydbceb: phase6604/0 and the strict summary
gate pass, MIR327/0; semantic3227/1 finds the unchanged copied-object snapshot
positive at test_semantic_qubit_part_c_2.cases.h321. Holder(Packet(packet.hp))
reaches an issued OBJECT mapping, but the new replay incorrectly equated raw
is_struct with semantic STRUCT flavor. Root verified ast_create_object and
ast_create_tobject both inherit raw is_struct=true from ast_create_struct,
then replace nominal_kind; the prior issuer already admitted them through its
rawstruct route. Root's e09 probe confirms current inner Type/argument/snapshot
identity but snapshot_current=false. B therefore seals the actual raw bool
separately from semantic flavor, replays both, and preserves the original
rawstruct-or-plainclass issuer/current set (STRUCT/OBJECT/TOBJECT/CLASS).
The explicit OBJECT issuer addition was not the cause and is redundant. C
verifies scalar-field reconstruction versus retaining the borrowed object and
flavor/raw-flag corruption. No COPY_ONLY=fresh, unknown-call origin-zero
permission or input edit; A's transfer remains unchanged.

Next actual integration falsifier, binaryf835: phase6632/0, strict summary,
MIR327/0 and semantic3228/0 pass. test-transpile reports four failed assertions
from one unchanged scheduler example whose lowering first fails in
BudgetSnapshot's returned-backing frame. The minimal Read(Slot<BudgetLedger>)
probe reproduces it; `.tmp/p0-destination-existing.izGtjw/` also records current
TOBJECT reconstruction admission. Typed Read's Slot/view/lifecycle/security
owner validates the operation, but only publishes an input nonretention fact.
Its C/LLVM/runtime ABI shallow-copies the payload; it does not prove freshness.

- B joins the successful typed Read boundary to the existing checked-result
  publication: result depends on receiver0, external retention=false, exact
  current source/result Type and call identities. Preserve secure/view/arity
  validation and fail closed on missing/stale receipts. Other actuals cannot
  acquire a result dependency merely by position or syntax spelling.
- A consumes the same relation through existing origin/destination/eager and
  cached summary checks; never translate BORROWED_FOR_CALL into independent
  output. Return/frame and Slot loan obligations remain distinct and required.
- C reuses existing Slot read phase and retention/currency scaffolds; the
  original scheduler input and four source-order assertions are unchanged.
- Root reruns narrow/strict/native/chain and the whole example before the
  unchanged canonical/bootstrap. Frozen review QUBf9g is f835 only, not approval
  of this later repair. No new owner, annotation/copy or larger work budget.

The first Read candidate cb27 exposes a facet collision, not a reason to drop
payload provenance: phase6383/12, including unchanged Read(Slot<Int>) return
and Read(Slot<String>)->Log positives. Root reproduces both. The existing
coarse slot-escape consumer interprets result_depends as carrying the input
Slot identity and emits RETURN even for a read used only by Log. Read returns
the payload value, not its container identity; shallow descriptor backing can
still depend on that container. B therefore publishes two projections of the
same current typed result receipt behind one shared validator: the existing
identity/result-value query reports no carried container identity for Read;
the backing relation reports its receiver dependency. A migrates the three
backing consumers and seals both facets in existing shared claims. Other
consumers needing only nonretention retain their behavior. No query failure
becomes independence and no old/new fallback exists. C keeps all old identity
oracles and tests the new backing facet explicitly, with receipt corruption
rejecting both projections. This completes the reached consumer map rather
than exempting Read by syntax spelling in the escape collector.

## Complete execution order

1. Record inherited shared WIP as an explicitly unverified local checkpoint.
   Merge the committed Claude lane into main and reconcile overlapping schemas,
   consumers, generators and probes. Do not create another implementation branch.
   Retain recoverable Git objects before deleting merged branch names. Preserve
   worktree files and ignored artifacts; they are not acceptance evidence.
2. Read the actual remaining-job logs under `/root/pgy-ci` before reusing any
   result. Check input revision, dirty probe delta, executable and completion.
3. Survey the memory change's complete chain: per-event verdict constructors ->
   final focus producer -> body-bundle `collection_focus_recovery` -> readiness
   and receipt digest -> codegen view and all probes. Missing/stale evidence
   must still refuse; do not retain a parallel verdict-owned carrier fallback.
4. Validate the merged source narrowly first: native build, component checks,
   formal-key, field-read, carrier, round-trip and focus-recovery behavior.
   The previously red non-CI focus gate remains an explicit obligation, not an
   exemption. Distinguish C/LLVM-built probes from emitted target execution.
5. Measure full DRV-2 consumer peak and replay both source admissions, unchanged
   codegen bootstrap, installed-driver build and gen2/gen3 fixed point. Use the
   repository's limits; the scratch oversized ReadFile build is diagnostic only.
6. Run the remaining five self-host bootstrap targets and policy corpus, then
   Linux core/self-host and backend-compare shards on the integrated revision.
   Reconcile any newly reached owner with this chain before changing semantics.
7. Inspect the final diff, commit coherent repairs on main, publish main and
   verify required GitHub jobs at the exact published SHA. Skips are not passes.
   Update the active handoff with source/executable/receipt identity and any
   genuinely remaining obligations. Do not reopen unrelated SoT/emitter tracks.

## Falsifiers and budgets

Preserve corrupt/stale focus carriers, forwarded opaque calls, growth through
aliases, wrong-field restoration, old-holder writeback, missing MIR ownership
states and closed-arm/match round trips. Check stable admitted output as well
as actual refusal locations; an earlier censored refusal is not attack coverage.
Static owner checks: 60 seconds; focused parity: 300 seconds; integration shard:
1800 seconds. One heavy bootstrap/consumer run at a time in shared WSL.

The main merge is complete. The resumed three-implementer workflow now has
three non-overlapping scopes on the same main checkout and the same P0 chain:

- A: inherited native parameter-publication/flow owners and their behavioral
  controls. Determine the complete canonical refusal causes; preserve the 4096
  cap and setter/escape negatives. No source annotations or copies to bypass it.
  The measured selector also reaches the existing builtin-retention registry:
  The reached Option tag observation `IsSome` has no retained payload, but
  lacks a row. Extend this same owner, regenerate its Pergyra projection, and check
  exact typed stdlib identity/arity, forged identity, user shadowing and
  retained `Some` payload negatives. C and LLVM both inspect only the tag;
  neither the constructor nor payload extraction receives a no-retain grant.
  This is a reached dependency, not a parallel retention policy or name bypass.
  `IsNone` remains native-only and is not part of this correction.
- B: structural reconciliation, existing JSON row-owner placement and static
  gate machinery. Preserve real behavioral tests and remove inherited forbidden
  source/doc phrase assertions. No semantic admission edits or blanket cap raises.
- C: the existing focus-recovery behavioral gate and its reached producer/
  consumer obligations, including the known nominal-forward red. Preserve
  corruption/restoration controls; distinguish optional focus from a permission.

Root owns integration, main commits/publication, the full source/binary-bound
canonical/bootstrap/install/memory/CI gate and the active handoff. No agent
creates a branch/worktree, commits, pushes, kills a process, or runs a heavy
shared WSL bootstrap. Request the root's compile lease before a large probe.
The common acceptance remains the complete integrated compiler chain, not
three independent green reports. Unexpected cross-scope dependencies return
to root and this map before edits.

## Current bounded native closure decision

The query-local DAG repair at `3dc597701` restores a real full-input verdict
within budget (10.10 s, exit1, peak597032 KiB); it does not admit the program.
The next complete failed demand is `SemanticAstBodyFlowOfBlock` parameter1:
132 summary evaluations +3496 backing nodes +468 graph nodes =4096. The
Block/Statement/If/Match/Loop SCC repeatedly analyzes local `flow` and `child`
whole-value replacements as if they could write an external output.

The existing typed-assignment/publication owner distinguishes an original
ordinary local binding replacement from a storage write. Produce the fact only
at successful `type_check_assignment` completion, after Slot/view sugar and
subject/borrow/assignability guards, with an exact local identifier binding.
Snapshot assignment/target/RHS identity, shape, binding and the existing typed
binding name; do not retain scope Symbols. The selector joins this fact to the
same original LET with `is_alias=false`. A bare identifier alone is insufficient:
`using` aliases and pin views can write through; Slot assignment has write sugar.
Keep formal/member/index writes conservative and traverse the full RHS. Seal
and replay the LET shape/alias flag and typed replacement evidence; missing or
stale facts cannot grant permission. Reuse the existing selector and index
claims, including their former scalar-local path, not a second owner.
The old heap-rebinding experiment was removed; this narrower decision requires
the measured demand above and behavioral alias/Slot/stale/RHS falsifiers.

The independent frozen review also identifies a reached caller-consumer gap:
`ref String` can bypass already-produced CALL/CHANNEL facts via broad COPY_ONLY
branches. B repairs the existing caller/summary/diagnostic consumer chain;
A owns typed publication/selector/claims. Preserve String value classification,
result loans and pending-to-finalized publication. Fix consumption, not source
annotations or blanket freshness. Pass the existing typed callable declaration
and parameter mode from the sole call-check consumer into the String boundary;
name lookup must not downgrade an indirect ref mode to default. Temporary
String arguments still register their exact call/site identity (a diagnostic
label is not ownership evidence); unknown/bodyless ref callees fail closed.
Existing literal-to-Slot/inout C/LLVM positives distinguish static String
storage from a dynamic borrow. Keep String CALL/CHANNEL summary/MAY facts and
apply them at the actual call boundary, not as an unconditional String formal
rejection. Only the current typed literal row (String, AST_STRING, copy-safe)
has the static-backing permission for a known body. Allocating String calls can
also have `copy_escape/origin_ready`; those bits alone do not transfer a ref's
cleanup responsibility. Preserve all previous literal runtime positives and
dynamic/record-field/Concat-temporary retaining negatives. Do not expand this
into general String provenance inference. A prior semantic error also prevents
constructor publication; retain its typed diagnostic rather than adding a
misleading missing-fact cascade to an already-refused program.
Scalar backing exclusion does not prove a
callee has no receiver/capture: exact local calls still enter the closed-body
graph, sealed builtin/stdlib identity is distinct, and opaque/indirect/member
targets remain conservative. Add direct owner-level negatives; CLI admission
alone is not evidence of an escape. Both repairs feed the same unchanged
canonical and original positive/negative/work-budget integration gate.

### Complete SCC census after `b9beae91e`

Scratch-only exhaustive traversal observes 55 exact reached routines and 2090
nodes, with 15 false premises and no incomplete row: ToInt input5, StringTrim
input1, three ArrayPush calls' receiver/value6, and empty literal3. Every
call-target/header and local-assignment premise passes. The original full
verdict remains work4096/exit1. Receipt: `a-scc-census.log`; source and binary
were restored byte-for-byte. Do not infer further causes from the error cascade.

Selected first slice, behind the existing owners, not separate implementation
tracks: exact ToInt/StringTrim input retention rows (not result freshness) and
current typed empty-literal purity. Measure the unchanged canonical with just
these facts before deciding whether private local array precision is necessary.
Empty literals alone contain no evaluated element or retained backing; keep
nonempty construction conservative, with changed-count/identity negatives.
ArrayPush retains its value and MUST NOT get a blanket no-retain row. Before
any private-array repair, resolve whether the current storage/exclusivity owner proves
the receiver has not escaped or acquired an alias. An EMPTY_LITERAL origin or
an effect receipt emitted after a diagnostic does not by itself prove this.
Preserve element-to-return/output origins and all CALL/CHANNEL/drop negatives.
Scopes: B existing retention registry/projection and conversion fixtures; C
selector/claim empty-literal branch and existing phase unit; A read-only local
storage/effect producer-to-last-consumer proof survey until its unknown is
resolved. Root integrates and runs the shared original canonical/work guards,
query C/LLVM and bootstrap/install gates. No larger work/time/memory budgets.
Private ArrayPush precision is explicitly DEFERRED until that measurement:
CALL suppression without carrying the inserted value's RETURN/output origins
is unsound, and current exclusivity flags alone do not prove nonretention.

The B/C-only measurement is now observed, not hypothetical: phase4105/0,
canonical exit1 in10.93s/621612KiB. A's exhaustive repeat has55 routines,
2090 nodes, exactly6 false premises (three ordinary ArrayPush calls, receiver
and value), incomplete0. Work4096 is58 evaluations+2404 backing+1634 graph;
ToInt/StringTrim/empty literals now pass. Receipts: `bc-canonical`,
`a-bc-demand`, `a-bc-census` in `/root/pgy-main-20261016.*.log`.
Select the private-local mutation slice only across the complete existing
publication -> selector/closed-call graph -> slot escape -> backing origins ->
returned/output relation -> evaluator chain. It must distinguish a checked
fresh local receiver from an alias/formal/escaped receiver, with exact current
call/binding/storage facts and the entire no-external-output graph. Do not
suppress CALL in isolation: inserted values returned through an array or a
nominal wrapper, stored in an output, or sent on a channel keep those effects.
Missing/stale evidence stays conservative. A owns this existing native chain;
no new fact family, broader builtin grant or source workaround is selected.

Independent b9 review also falsifies a separate last-consumer claim on this
same ownership path: RETURN-only ref String loans can reach caller-owned Slot
Write or inout String storage from a retiring local record. Ordinary read/log
loans and static literals must remain valid. Root and B first map an existing
owner repair: carry the exact returned loan's actual backing through locals
and wrappers, then discharge the final storage obligation after publication
and summaries are sealed. An empty formal-origin bitset is not proof of an
independent local lifetime. Avoid an all-local dense origin matrix on every
routine; settle the demand and lifetime representation before editing. A and
B may not concurrently edit shared publication/backing owners. Four frozen
analyze-only falsifiers and the valid controls are in
`.tmp/redteam-b9beae91e-20261016/`; no unsafe runtime execution is allowed.

B/C narrow acceptance: phase4105/0; summary behavior including both C/LLVM
reader/Trim-return runtime cases and original4096/4097 controls PASS10.18s;
retention registry/region behavior and component/generator checks PASS11.86s.
Trim-return is a valid independent-result path, not a fabricated negative.
The old region probe now distinguishes borrowed Write receiver from retained
payload and an unknown builtin. Self-host identity parity and the full
canonical/bootstrap/installed chain remain OPEN; these receipts do not close P0.

### Measured next blocking operation, not a new precision track

The private-local candidate's first full run (clone47d+12 dirty paths,
native SHA256 `df956fdaa406efd25ab9df6b8e003a29d73c0e7337b620b8bcf8b3e0756681e4`)
fails10.44s/719984KiB. Its corrected phase unit passes4161/0; the original
BodyFlow closure now has55 routines/2090 nodes/false0, all3 private calls
issued/current. The new first failed demand is
`SemanticAstCollectionMemberCaptureBinding` param1 of type
`SemanticAstFunctionSignatureFacts`, not the previous BodyFlow demand.
Work4096 =120 evaluations+1696 backing+2280 graph. One top-level demand
rechecks the same three failed no-store closures40 times each:
Binding2 nodes, Provenance34, Subject21; unique graph routines35.
Receipts: `private-canonical`, `private-unit-second`, A's private census logs.

Selected cost repair: behind the existing no-store selector, remember only a
failed selection for the exact routine within the same immutable top-level
summary demand. This is not a new permission or a cross-query cache. A failed
selection still requires the existing conservative backing analysis; do not
skip current evidence or suppress any effect. A new demand must retry, and
stale/missing/escape/work-budget negatives remain unchanged. Do not broaden
ArraySet/Pop, nonempty-literal or indirect-call permissions to avoid measuring
this repetition. B's queued single-seed String-loan consumer is sequentially
integrated into the same existing backing owner; source windows do not overlap.

The integrated A/B candidate builds without warnings after root's const-owner
and diagnostic-site repair. Root observed phase4218/0, summary runtime/negative
and unchanged demand controls PASS8.24s, and readonly-map24/58 PASS.
Four returned-String Slot/inout falsifiers now refuse before MIR; literal/read
controls and C/LLVM private-local read remain valid. First full input remains
RED (11.43s/719788KiB, first diagnostic line218/work4096). Its independent
failed-demand inventory is the next investigation, not more local exemptions.
`production-c-size-test-smoke` ran its host counter/cap controls but stops at
the unavailable installed self-host driver; it is OPEN, not passed or replaced
with a native fallback. Local stable Rocq prefix is unavailable too; the actual
published proof job must execute before any full-CI claim.

Sealed-input isolated inventory is complete:3188 unique routine/ordinal keys
from4273 routines/8851 pending rows;692 cold failures across151 routines,
all work4096, incomplete0, elapsed27.865s. Scratch exits86 before MIR.
Receipt `a-inventory.log`; this is not the warm production verdict. Further
telemetry disproves a40-round oscillation: MemberCapture is8 parameters times
5 normal convergence rounds. False-selection reuse admits that demand.

The current warm BodyFlow failure is now measured:85 evaluations+280 backing
+3731 graph=4096. Same55-member closed set is reconstructed65 times
(3575 visits), distinct membership sets1; `a-positive.log`. Selected next
repair is an admitted closed-set proof within one top-level summary demand,
behind the existing index/graph owner. Reuse must recheck every proof member's
current header, roots/claims, false mutable-output selector and exact typed
outgoing call facts; missing/stale members refuse. Do not merely stop charging
the existing repeated traversal. Construct/expand the graph once, then consume
its validated proof without rebuilding its queue. No positive reuse outside
the solver or across demands, no weaker work/time budget, no new cache owner,
and no ArraySet/Pop or indirect-call permission. Preserve same-query leaf
corruption/refusal, next-query restoration, original unique-work limits and all
behavioral escape gates before retrying the unchanged canonical.

Root integrated that proof and the independent2800 review's opaque-inout
falsifier. A local's original literal is not static permission after an unknown
call can replace its binding. The existing publication owner now joins exact
current String value evidence to a registered synchronous const/copy input;
nonretention alone is not a no-mutation fact. Missing/bodyless/indirect/member
mutation facts refuse, while known user callees still consume current formal
mode and WRITE summaries. Four permanent analyze-only controls cover opaque
Slot/inout sinks and direct/bodyful mutation; a new read-only static-local
positive executes identically on C and LLVM. No dangerous negative is run.

Root observations on clone27d plus14 frozen candidate paths: native build
warning-free; phase4245/0; summary including original4096/4097 and recursive
controls PASS8.68s; readonly-map24/58 and HIR/RIR/MIR/AIR propagation PASS19.38s;
component/generator and retention/region controls PASS10.50s. Receipts:
`proof-build`, `proof-unit`, `proof-summary`, `proof-chain`, `proof-component`.
Same full canonical is still RED11.02s/720000KiB/18410 errors/16 warnings;
its primary moved from line218 to line71, work4096. This is not V1 closure.
A has the sole WSL lease for one scratch warm-demand measurement, then must
restore all source/objects/native bytes. Root holds publication/bootstrap;
the next action is the reached failure's owner/cost, not another permission.

The next warm primary is `EmitStmtList` param5 at
`CodegenSemanticTaggedEnumVariantTagFromGraph`:4096=183 evaluations+3475 backing
+438 graph, with respective unique counts183/2566/275 (3024 total).
`RuntimeCallCName` repeats679 backing nodes in epochs1/2. Closed-set proof
reuse is observed22 times, current replay85, refusal0, union147; this is the
old cold key2669, not evidence for another builtin exemption. The next bounded
repair separates final output/expression-cache bit growth from changes to
the actual iterative inputs, local binding origins/destinations. Keep OR
merges, binding-triggered convergence, recursive pending/building and pass
currency unchanged. Final effects/outputs are not inputs to expression
evaluation. Falsifiers must include late binding growth/loops, output-only
growth, recursive pending, all escapes and the original demand controls.

Adjacent alias investigation also reaches the same String mutation owner:
the `dd3d42418` native admits `using held as alias; Fill(alias)` followed by
storing `Echo(held)`. C's alias emitter substitutes the original place; a new
alias declaration ID is not a new mutation authority. Repair by resolving
only admitted/current LET `local_alias` claims to their original storage root,
on both source and mutation comparisons. Ordinary value-copy declarations
stop the walk and stay separate. Missing/stale/cyclic alias facts fail closed;
do not infer pin/view-call authority or introduce a name lookup/new owner.
Root analyze-only receipt `/root/pgy-alias-dd3d42418-20261016.glo2BE`: using
alias rc0/MIR30239 (falsifier), ordinary copy rc0/MIR30237 (positive), field
alias rc1/MIR0 (existing conservative boundary). B edits this facet first;
A's convergence edit waits for B's source freeze. C owns paired fixtures;
root owns stale-fact/whole-chain execution. Unsafe extern inputs are never run.

Root repeated the combined alias/convergence implementation: phase4285/0,
including alias identity/flag/initializer corruption and restored-query checks;
output-only, loop-backward origins and a legal nested using-output loop preserve
output/CALL relations. The proposed Array loop was rejected by the existing
tracked-shallow-assignment guard and was replaced with the legal alias case,
not made admissible. Summary/recursive/4096/4097 controls PASS8.60s;
readonly-map24/58 plus HIR/RIR/MIR/AIR PASS21.76s; component/registry PASS9.74s.
Receipts: `binding-unit-final`, `binding-summary-final`, `binding-chain`,
`binding-component`. Full canonical still RED11.11s/719860KiB/18410 errors/16
warnings, primary now line9 rather than71. Receipt `binding-canonical`.
These are coherent repairs, not bootstrap acceptance. Next is the same3188-key
cold inventory delta plus one warm measurement to classify the remaining set;
do not extrapolate completion from the first diagnostic's movement.

Updated isolated census on4f19 uses exactly the same3188
(routine name,declaration ID,ordinal) keys:692->515 failures,151->121 routines,
177 resolved, no new failures, incomplete0. All remaining failures are work4096.
Body45.531s/process54.62s, diagnostic exit86. Warm root remains EmitStmtList
param5 at RewriteSemanticHashMapCall:219 evaluations+3427 backing+450 graph;
category-unique3736, but unvisited dependencies remain unknown. RuntimeCallCName
now uses679 nodes in one epoch. Source/object/binary/dirty snapshot restored.
This is failure-set inventory, not admission or hard substitution progress.

Independent frozen4f19 review found no new bypass in its narrow scope. Preserve
its two-level alias mutation/source/assignment falsifiers as permanent
analyze-only negatives, not runtime tests. Root repeated the enlarged summary,
recursive and original demand controls: PASS12.23s, phase4285/0.
Receipt `alias-review-controls`; reviewer raw evidence remains in
`.tmp/redteam-4f19d8760-20261016/`. Neither result approves the full canonical.

The next unknown is the scope of the reached backing operation: its API asks
for one ordinal's effects/output row, but initializes all formal origins and
demands callees for any origin. The last consumer copies only the requested
row into the existing summary entry. Measure actual work initiated by other
origin bits before selecting a representation change. Do not simply delete
seeds or publish an uncomputed row as ready. Deferred activation would need
explicit per-row currency, recursive activation/pending and output-axis
preservation. A diagnostic observation may classify this cost; it must not
change the algorithm, work cap or admission. No implementation is selected.

The observation falsifies a narrower assumption: the first EmitStmtList
episode hits4096 inside callees before its own backing CALL traversal, so
there are0 direct root-fact requests to attribute. Within reached nested
facts, requests without their own requested-origin bit consume650 work
(305 calls), versus62 with it (330 calls). The observed712 is only the
instrumented callee-request span sum, not the whole4096 or a saving forecast;
inclusive/exclusive totals agree and nested duplicate contribution is0.
One absent-bit request, RewriteSemanticMemberCall input1 ->
RewriteSemanticArraySliceMemberCall argument4, consumes583. Observation only,
11.78s/exit89 before MIR; source/objects/native/dirty10 restored exactly.
Receipt `a-formal-attribution`; the root and nested scopes must not be conflated.

Candidate closure objective: align the existing returned-backing producer's
query identity with its existing summary entry `(function ID,input ordinal)`.
Priority: current typed identity, computed-row readiness, all output destinations,
recursive pending and lifetime, unchanged negative/work gates, then cost.
Current chain: evaluate one entry -> eager all-input backing matrix -> select
one effects/output row -> entry.output_dependencies -> call output query and
sealed summaries. The existing summary entry is the last row consumer.
Investigate an ordinal-owned backing frame inside the existing index, with
one input origin and the complete ref/inout destination axis, not a general
query engine or new semantic owner. Map every creation/current/read/destroy
site and direct unit consumer before implementation. Unknown: exact frame
lifetime and recursive cross-input currency. An uncreated/uncomputed frame
is not a ready empty result; no old matrix fallback or cap increase is allowed.
Preserve >64 output columns, zero-formal loan queries, cross-input recursion,
private-store mode, loop propagation, alias/Slot/channel/return escapes and
the original4096/4097 controls. A must return the complete map/plan first;
root has not authorized production edits or another diagnostic run.

The whole-chain review is now complete and root authorized this bounded
implementation. Current -> intended ownership: one eager function matrix ->
ordinal-owned lazy frames in the same program-point index; published identity
stays `(function ID,input ordinal)` at the existing SummaryEntry. Each frame
has origin_count/words1 and the full destination_count/wordsN/ceil(N/64), exact
input ordinal/ID, and independent building/complete/pending/pass/mode/epoch.
All formal bindings exist; only the requested one has an origin bit. Keep
every ref/inout destination, including columns64+; do not compute other input
rows or interpret their absence as zero. Matrix extraction is removed, not
retained as a compatibility read.

Independent edit scopes of this one chain: A owns internal.h, index producer,
single-input evaluation/B loan1x1 and summary producer/cache/output consumers;
B owns structural current-validation and frame/vector destruction in existing
store_destroy; C owns high-output65 literal/dynamic behavioral fixtures; root
owns direct phase-unit migration/faults and the single integration gate.
An internal output_producer receipt distinguishes BACKING_FRAME from the
existing CLOSED_SELECTOR proof. UNPROVED is not a ready empty row. Frame-ready
consumption requires its exact current input identity and completed nonpending
frame; selector-ready consumption independently replays its admitted no-output
evidence. Structural currency alone must not require completed states during
SCC reentry. No missing-frame/old-matrix fallback. The existing entry owns the
projected result; all frame allocations remain context-owned until destruction,
then shared claims are freed once by the index. No new fact owner or syntax.

All three implementers froze their assigned files without building. Root is
integrating the existing68-parameter cross-word unit, input-order isolation,
uncreated/wrong-input frame and producer faults, loop/alias/recursive/private
controls, and new output65 literal/dynamic analyze-only pair.
First gate is native build+phase; then summary/unchanged4096/4097, full unchanged
canonical and measured cost, IR/Map/component/registry, independent frozen
review, and the original bootstrap/install/fixed-point/memory/CI chain. The
actual effect on execution cost, total refusals and memory is still unknown.

Root integrated this candidate on clone3f428ce86 with nine source/test/directive
overlays. Native build PASS3.48s; phase4342/0 and summary (including unchanged
independent4097/oversized4096 controls) PASS12.73s; Map24 runtime positives/58
analyze-only negatives and HIR/RIR/MIR/AIR PASS20.80s; component/registry
PASS10.54s. Full unchanged canonical is still RED8.01s, peak719980KiB, exit1,
18410 cascading errors/16 warnings, first work4096 at line110. These results
do not establish canonical admission, bootstrap, driver, memory or CI green.
Receipts: `input-frame-build`, `input-frame-summary-scoped-final`,
`input-frame-chain`, `input-frame-component`, `input-frame-canonical`.

The first integration run caught completed-output validation accumulating
unrelated finished demands' graph work. The existing graph owner now charges
a fresh synchronous outside-solver validation to a local counter with the
same4096 bound; inside the solver the original demand counter is unchanged.
Two-member/one-unit outside-validation refusal and restored-budget controls
pass, alongside the original4097 independent and4096 single-closure tests.
No budget increase, uncharged graph walk or ready-result fallback was added.
The output65 external pair is MIR-only: the native C emitter's pre-existing
64-argument ceiling prevents runtime execution. Existing68-parameter API
coverage and all previously registered C/LLVM runtime controls are preserved.

Next diagnostic scope, not a selected production change: the named escape
consumer in `slot_analyzer_summary.c` asks for one slot_name, while
`slot_escape_mask_in_program` builds the all-name escape map and selects that
row afterwards. Its AST_CALL path demands every eligible argument-root callee.
The all-name consumer in `slot_analyzer.c` must retain full collection; named
access in `slot_analyzer_access.c` already filters roots. Measure this base
escape request work on the same input before choosing a repair. Classify
exact binding, different binding, same-name shadow, alias candidate and missing
identity, preserve typed argument checks/nested traversal, and retain the
existing formal/backing/return/channel chain. Source repetition alone is not
proof of savings. Repeat the3188-key census and warm attribution; require a
coherent map and falsifiers before another production edit.

Checkpoint55fe918a4 falsifies a performance assumption: the same3188-key cold
set has839 failures (18 resolved,342 new,497 retained; key difference0), all
at the unchanged work cap. Root independently ran the comparison. The new
warm first failure is SemanticExpressionGraphCallArgumentScalarTypeName input0:
38 evaluation+4006 backing+52 graph charges=4096. Its backing walk revisits
237 AST nodes across four input frames (3978 charges); this is not evidence
that input frames alone reduce cost. Of973 recorded request spans, base named
escape requests with different bindings account for3011 exclusive charges;
147 exact-binding requests account for0. The total span sum3017 is only the
instrumented demand span, not all4096 or a forecast of savings. Signatures
4273/4273 are byte-equal. Logs: `a-input-frame-census-warm` (46.41s/exit89),
`a-input-frame-warm-signature` (7.28s/exit89); both stop before MIR. Source,
the four preserved objects/dependencies and native binary were restored to
55fe/958df32b; three additionally rebuilt unchanged-source objects have no
prior byte baseline, so the next normal build must relink and rerun controls.
The independent55fe soundness review found no new bypass in its stated narrow
scope but explicitly withholds whole-candidate approval. This regression is
OPEN, not erased by the focused green gates or the recoverable checkpoint.

Selected bounded repair objective: push the existing named escape result
projection to the expensive callee-demand boundary. Identity remains the
existing FunctionIdParamIndex summary, and the projection continues using its
existing name key; this does not introduce name identity or change alias
selection meaning. Current -> intended chain: named summary -> full escape
map/all-root demands -> select one name becomes named summary -> same typed
argument/root validation -> demand only a root whose recorded name is selected
-> same name row. Every argument expression is still traversed, including
outer calls with no root. Full-function Slot analysis explicitly selects all
names and retains all demands. Both modes use the same existing collector.
Owner: slot_analyzer_escape.c; last consumers: parameter summary and function
exit Slot warnings. Unrelated summary closure failure is no longer a named
query dependency; malformed/stale typed call/argument/root facts still fail
closed. Direct self-reborrow, private-store, own, RETURN and CHANNEL behavior
remain unchanged. No formal/alias reclassification, new graph/cache/owner,
skipped subtree, body reopen, budget increase or input reduction is allowed.

Complete edit scopes: A changes only the collector, its internal signature
and two existing caller modes; B adds direct named/full behavior, cross-name
isolation and nonselected typed-fact corruption/restoration units; C adds a
nested-call retaining/literal pair to the existing summary gate. Root owns
integration and the common gate: normal native build+phase, unchanged summary
and4096/4097 controls, full canonical, same3188-key census, Map/IR/component,
then independent frozen review and the existing bootstrap/install/memory/CI
chain. Keep existing using-alias, ordinary copy, RETURN/CALL/CHANNEL and stale
publication negatives. Acceptance requires removing the newly introduced342
failures rather than accepting a favorable aggregate or first diagnostic.

Named candidate integration is coherent but still RED. Normal native rebuild
PASS2.99s; phase4360/0, summary (including nested positive C/LLVM output and
exact negative call site), component/registry PASS22.19s combined; Map24/58
and HIR/RIR/MIR/AIR PASS18.42s. Full canonical RED14.56s/peak720024KiB,
18410 cascading errors/16 warnings, first12:6 work4096. The same3188-key
census yields287 failures,552 resolved/0 new vs839; against original515 it
resolves260 but adds32 (18 routines). It therefore fixes310 of342 regressions,
not all of them. Type signatures4273/4273 and key sets are identical; root
independently repeated comparisons. `a-named-census`:24.47s/peak720184KiB,
exit86 before MIR; inventory17.423s. All remaining failures are work4096.
Source, complete build tree, native binary and dirty state restored exactly.
Production binary is e9e0a20e953e878bb01adefa373da94dbf60133e3e8b9ef6c0b55ff15c4d0756.
Next observation only: actual warm first demand and one remaining regression,
with cross-input AST, same-input epoch, dependency and receipt work separated.
Do not select another representation, relax caps or ignore the32 regressions
from aggregate improvement. Bootstrap/install/memory/CI remain OPEN.

The next observations separate two causes rather than selecting a blanket
pass removal. At3b44/e9e0a20b, actual warm root is
SemanticAstCollectionOwnershipVerdictFromResolvedFactsWithFormalEffects input14:
E8+B4068+G20=4096, receipt0. CallEffectFacts inputs6/8 each visit904 AST nodes
twice in one solver pass (pending0; changed1 then0), costing3616. Their demand
arguments both really originate from formal_effects; named filtering is not
dropping a legitimate dependency. A representative remaining regression,
ArithmeticResultType input3, costs E36+B4024+G36, receipt0: its single-input
frames revisit during3-4 solver passes with pending1/changed0, a separate
readiness/settlement problem. Logs `a-named-warm` (14.47s/exit89) and
`a-named-single-arithmetic` (7.29s/exit89), signatures4273 equal; both diagnostic.

The unchanged warm sequence was then instrumented at all three binding read
sites without changing its scheduling. All4096 FRAME_WORK events and4273
signatures match byte-for-byte. Of373 merge records,12 grow, all destination
first fills (7 LET sites,5 assignment sites), none read earlier in that epoch.
No origin grows, and epoch2 has no growth. Root independently inspected all12.
Receipt `a-binding-read`,14.18s/exit89/peak719972KiB, exact source/build/bin/
status restore. This establishes the reached redundant traversal, not a claim
that every loop or SCC is redundant.

Selected closure-blocking repair: schedule binding-growth replay only when a
changed binding kind was already read in the current frame epoch. Existing
owner and result identity do not move. Priority: identical monotone origins,
destinations/effects/outputs; every zero-bit read; loop/backward and expression
memo dependencies; current typed facts; then avoided whole-body replay.
Chain: backing_origin identifier reads origins; backing_store reads target
destinations; backing_destination_alias reads source destinations. These three
reads stamp separate frame-local origin/destination read epochs before value
inspection. LET origin, local-store origin and alias-destination OR merges
still retain every bit, but set fact.changed only if their corresponding
read stamp equals the current epoch. An unread forward value is consumed by
its later first read; a growth after an earlier read replays that reader and
any cached expression on the next epoch. Seed initialization precedes reads.
The last consumer is the existing do/while(fact.changed && fact.complete).
No pending/readiness/private-store behavior changes. No read may be inferred
from nonzero bits: reading an empty value is a dependency too. Missing current
typed facts still fail closed. No unconditional replay compatibility path,
cap change, effect erasure, annotation or shared cross-frame stamp is allowed.

A owns only index producer/read/merge sites; B owns binding fields in existing
internal.h, structural read-epoch<=frame-epoch validation and direct unit
falsifiers; C owns backward-loop dynamic/static fixtures in the existing
summary gate. No allocation owner is added: fields are zero-initialized and
freed with their existing binding. Root integrates and reruns the same build,
summary/4096/4097, full canonical,3188-key census, Map/IR/component and frozen
review. Direct semantics, not an exact iteration count, are the test oracle.
Preserve backward origin, alias-of-empty-destination, nested/memo and branch
growth cases as well as forward positives. The SCC pending repeats and all32
remaining regressions stay explicit whole-chain obligations; investigate
their exact callee/version dependencies after this actual warm blocker, not
by treating pending as completed or extending precision beyond the canonical.

Read-growth candidate root verification: normal build PASS3.50s; phase4409/0
and summary/runtime/unchanged4096/4097 PASS10.55s; Map24/58 and HIR/RIR/MIR/AIR
PASS20.34s; component/registry PASS11.12s. Full canonical remains RED13.25s,
peak720096KiB, first15:6 work4096,18410 cascading errors/16 warnings. Binary
cc91699c8f898307f4564dfc96aa454a0559191f81e49329b97c0cb7fcf27ec0;
receipts `read-growth-build`, `read-growth-summary`, `read-growth-chain`,
`read-growth-component`, `read-growth-canonical`. The current full cold failure
set and actual new warm dependency are not yet measured. Do not carry287
forward as a result for this candidate.

Representation decision checkpoint, observation only: multiple genuinely
related inputs may lose body-sharing in per-input frames. Before adding more
producer state, compare the current frame candidate with a scratch all-input
matrix using the same existing index/summary owners and fixed input. A raw
4f19 matrix is not a controlled safety comparison: it predates read-growth,
output-producer/current receipts and outside-solver local graph accounting.
Any diagnostic matrix must retain those contracts, all computed-row readiness,
typed/seed/width/overflow/current validation, full destruction and the original
work cap; otherwise its result is only confounded historical cost evidence,
never admission. The matrix must have no optional fallback in production.
No representation change is selected. First enumerate its complete creation/
current/row-consumer/loan/destruction map and review the controlled scratch
delta, then permit at most two fixed-input diagnostic processes (current and
matrix), with complete source/build/bin restoration. No model result can
substitute for the original positive/negative or full compiler acceptance.

## Active safety blocker — nested execution children (before matrix comparison)

Atde1335b2/cc91699c, root and the independent reviewer reproduced a real
ref-to-own contract bypass, not a work-cap refusal. Direct
`TakeSlot(own slot)->Bool` from `Probe(ref slot)` refuses with the exact
BORROW_ESCAPE and no MIR; wrapping that same call in binary if/return/Log/LET
or unary if emits MIR with Probe mask0. The safe REF callee counterpart passes.
No unsafe output was executed. Evidence:
`.tmp/redteam-de1335b2-p0-20261016/review.md` and root's
`.tmp/p0-direct-slot-20261016.pgy` / nested-condition pair. The first malformed
Release/ClaimSlot draft was corrected before this comparison and is not proof.
This predates read-growth; the narrow earlier review is not overall approval.
Matrix preparation is preserved but execution/selection is deferred until this
same parameter-flow safety seam is repaired.

Whole chain: AST identifier-ref selection -> roots_by_param -> current child
claims -> named READ/WRITE/RELEASE and RETURN/CALL/CHANNEL collectors -> existing
FunctionIdParamIndex summary -> publication borrow checks and MIR refusal.
The root selector sees BINARY/UNARY, but escape collection silently succeeds
without their children. Anchored handles correctly skip returned-backing
analysis, so that separate facet cannot repair a missing direct CALL effect.
Named/full access also miss cast/type-test/aggregate/destructure children;
root selection misses Set and parallel join inputs. Child sealing must cover
the newly reached topology. Do not remove anchored semantics or grant ALL
from missing identity as a supposed fix.

Objective: every supported eager evaluation child reaches its existing effect
owner, independent of expression wrapping. Priority: preserve all six effect
facets and typed identity, seal new child identities, explicit failure for an
unmodelled live form, then unchanged positive behavior and bounded execution.
Owners stay the existing AST-ref selector, index claims, access and escape
collectors; last consumers are parameter publication and full Slot/parallel
analysis. Forbidden: default-success for an unvisited executable child, a
new generic visitor/cache owner, treating lexical pattern binders as calls,
skipping worker effects merely because its body was visited, broad source
annotations or reducing the canonical/safety input.

Before edits, complete the supported-child map against parser/schema, semantic
match facts and actual C/LLVM evaluation. Eager expressions include binary,
unary, cast, type-test, member/index, array/tuple/set/map key-value and local
destructure initialization. Match runtime patterns cannot all be skipped:
general/multi patterns execute, whereas validated tag/binding patterns do not.
Walk subject/guard/body plus actual runtime pattern expressions, or refuse
when the existing typed evidence cannot distinguish them. Unsupported
deferred/capture/worker forms must not silently supply an empty effect proof;
consume existing owned evidence where available, otherwise fail explicitly at
that affected boundary. Pure literals/types/non-evaluating leaves are explicit.
Keep current typed-call/argument/root checks and all subtree recursion in both
named and full modes. Seal pointer/ID/count changes for supported children.

Approved implementation decision: use the current typed match subject family,
never variant spelling. General patterns are evaluated; Option/Result/enum
tag/binder patterns are syntax-only. Initial index construction seals the whole
selection-input topology once, including children after the no-output selector's
first true, rather than rediscovering it at every query. Syntax-only rows cannot
grant semantic permission; existing semantic claim recording promotes their
checked facts. Cycles and unknown child topology fail explicitly. Child identity,
count, shape, match family and decision targets must replay on current queries.
Bounded parallel task effects and its eager join inputs remain visible without
replacing the existing type checker's capture/race authority. Unproved detached
capture forms cannot be silently treated as empty effects.

Approved independent scopes of this one safety chain: A escape collector and
AST-ref root selection; B access named/full collectors and index child claims;
C direct semantic unit and executable per-wrapper paired falsifiers. Root alone builds,
integrates, commits/publishes and owns the shared WSL lease. Verification:
independent valid wrapper cases with exact BORROW_ESCAPE/MIR0, REF positives,
all six effect bits at direct APIs, child/current corruption and restoration,
existing alias/loop/64+/recursive/4096/4097 controls, Map/IR/component, unchanged
full native/canonical, then the original bootstrap/install/memory/CI chain.
The read-growth census (same3188/signatures4273) is202 failures:85 resolved/0 new
vs287; pre-frame515 leaves179 existing+23 new (14 routines). Its warm first
demand is CodegenIntentPriorityExpression input0, RuntimeCallCName atwork4096:
E84+B3730+G282, no same-frame AST repeat, receipt graph111. Log
`a-readguard-census-warm`30.76s/exit89/peak719960KiB is diagnostic before MIR.
All source/build/bin were exactly restored. Those work failures remain OPEN;
the P0 cannot be traded for a better aggregate performance count.

Root's fixed de1335 wrapper triage admitted20 REF controls;14 corresponding
OWN variants incorrectly emitted MIR. Three other shapes (Set, general runtime
match emission, OR destructuring) were not valid artifact controls and do not
count as safety evidence. The first integrated executed-child repair refuses
all20 OWN cases with BORROW_ESCAPE/MIR0 and preserves all20 REF admissions.
Its blanket deferred-boundary refusal is not acceptable:11 of12 existing worker
positive programs regressed. Preserve this observation, not the blanket policy.

One coherent extension closes that reached dependency: the existing typed
publication store owns actual capture scan success, final boundary check, exact
source binding/type/mode and syntax currentness; existing lambda/async/spawn/
event producers issue those facts, and both collector modes consume them.
Lambda bodies are not eager caller executions. Physical copy-only captures
carry no backing; String descriptor captures retain CALL provenance. The final
lambda scan occurs after body typing so match payload binders join by exact
binding ID, not a pre-typing spelling guess. Missing facts still refuse.

The independent frozen review also confirmed CAST/MAP/TYPE_TEST hiding a
borrowed Slot capture and a `let closure -> return closure` escape. Source and
analyze-only evidence: `.tmp/redteam-capture-de1335b2-20261016/review.md`.
The existing callable capability equation owner must retain original compacted
argument ordinals and resolve all alias/result/return candidates before allowing
a non-copy environment across a retaining/return/subscription boundary. Direct
local invocation, copy-only Int environments and no-capture functions stay valid.
Aggregate-valued environment carriers are an explicitly unverified dependency,
not a permission or a claim of whole-language closure.

The frozen review subsequently reached `Holder(f)` -> return -> `Relay` argument
-> return -> caller local, with a locally owned String-capturing lambda. It
emits MIR; field extraction/invocation fails for both unsafe and copy controls,
so this is aggregate admission evidence, not an observed runtime UAF. Repair
the existing callable capability owner at typed FUNCTION leaf stores: nominal
constructor fields, literals, Option payloads, collection writes, aggregate
field/index writes, and FUNCTION actuals crossing non-FUNCTION generic formals.
Record exact typed value/site identity and consume existing environment facts
at sealing; do not add an aggregate AST rescanner or another value graph.
Direct local invocation, named functions and physical Int-copy environments
are the required controls.

The schema recheck found transaction compensations outside `body`. They execute
on fail in reverse order, so they are evaluation children, but current flow
typing never publishes them. Existing snapshots do not prove every fail-exit
resource state. Do not bless conditional ownership cleanup from entry/end
snapshot equality or typecheck handlers as unconditional normal execution.
The body/handler scope and typed fail-state producer remain an implementation
decision before transaction permission; no green/closed claim covers them.

The implementation decision is now bounded: the existing flow owner collects
the actual reachable fail states projected to the transaction's outer scope,
joins them with its existing conservative resource MAY join, and checks the
reverse handler chain exactly once. This deliberately loses path correlation;
it does not grant permission missing from any failure alternative. A distinct
fail exit is propagated through branches/loops/select and consumed only by its
transaction, not mistaken for a function return. Incomplete loop summaries are
not reused while that outer failure frontier is active. Normal and compensated
poststates join at the real continuation. Callable/worker/defer bodies cannot
target their defining caller's transaction. The duplicate statement entrypoint
must delegate to the same flow owner.

With no reachable failure, handlers still need typed identities for existing
backends. Check them once in the outer entry state as unreachable code and
restore the actual normal resource continuation. Existing conservative MAY
effect/capability/call obligations remain; this is not a new type-only rollback
API or a claim of dead-code precision. Snapshot fields do not prove universal
transactional rollback of all compiler metadata. Local cleanup on a fail edge
must match runtime lowering; no semantic scope pop may invent a release.

Current independent scopes remain one safety chain: A publication/capture
producers and escape/root selection; B access/claims plus existing callable
equations' last retaining consumers; C behavioral tests and transaction flow
producer investigation. Root owns all builds, integration and publication.
The initial new phase run was4748 checks/9 failures:8 were an invalid channel
helper control, now replaced by a lexical owned-send control, and1 was the
observed deferred blanket refusal. Subsequent edits require a fresh run.
Source-size measurements also found required599-line gate violations in the
reached publication/claims/access/summary components. Do not raise caps or
compress code; coherent physical components must preserve the same store and
fact identities, with no new semantic owner or fallback.

Local WIP preservation checkpoint (parent `de1335b2e`): root repeated the
native phase unit after the complete function-type/transaction topology
correction and deferred-unit oracle repairs:4901 checks,1 failure,0.41s,
peak52468KiB. The single remaining failure is untyped compensation; receipt
`/root/pgy-main-20261016.p0-checkpoint-phase.log`. It is intentionally recorded
RED and is not pushed as accepted work. The fixed canonical/bootstrap/driver/
memory/CI chain still needs current evidence; older counts do not authorize it.

P0 integration after that preservation checkpoint: typed aggregate FUNCTION
leaf stores feed the existing callable seal; original nonretaining function
formals stay separate. Actual failure resources join conservatively before one
reverse handler check. The missing compensation ID producer was repaired in
`ast_identity.c`, not bypassed at publication. Root observed5069/0 phase checks,
full summary/worker/Holder/4096/4097 gate PASS10.06s, Map24/58+IR PASS17.73s,
component PASS11.66s, semantic599/header600 caps and build inventory PASS. Runtime
header composition changes pass four-key inline/raw15.24s and linkage/region/
ABI drift7.23s. Physical components preserve logical ownership; no registry row
is promoted CLOSED by these receipts. The one full-program Slot capture probe
requires the exact first BORROW code/cause/site and MIR absence; its later native
recovery diagnostics are recorded. All pre-existing strict negative oracles
remain unchanged. Unsupported Set typing is not borrow-safety evidence.

Candidate binary238a938d7d230bebf14d5b1d3dda6a9e0e636794318b1a7a6e5a42d7e4de4257:
unchanged canonical remains RED19.82s/peak769804KiB, first15:6 work4096,
18410 cascading errors/16 warnings (`p0-candidate-canonical` receipt). Next
decision is the already-planned complete current census / controlled shared-
body geometry measurement, not another language feature or owner family.
The old matrix files would revert current P0 claims/output currency; only the
reviewed geometry delta may be prepared in scratch. Full bootstrap/driver/
memory/published CI remain OPEN.

Published prerequisite checkpoint:795f71423, independently frozen-reviewed
without a newly found unsafe MIR admission in that bounded P0 slice. Exact-SHA
CI38855687075 is RED at the same Linux bootstrap work4096 primary; dependent
seed/install jobs are not passes. Root's current diagnostic reconfirms identical
3188 keys/4273 typed signatures and202 cold failures, incomplete0. Its original
warm first-ref stage charges E86+B3728+G282, dropped0; later Type-writing
consumers do not execute. Source/build/bin/.d restoration is byte-identical.
The second diagnostic completed and is rejected:406 failures,28 resolved/232
introduced/174 retained, identical4273 signatures/3188 keys,144.37s. Both runs
restored all source/build/bin/.d bytes. Retain the per-input geometry; no matrix
source or scratch test adaptation is integrated. Detailed bounded observations:
[`geometry comparison`](../audits/param_flow_geometry_comparison_2026-10-16.md).

Reached independent CI fixture repair objective: preserve original effect/
body-summary/contract/branch-warning assertions on supported named spawn calls.
The16 failing macOS assertions (3206 pass) share scalar-spawn test inputs,
which both real emitters already reject. Chain: named routine/call typing ->
checked deferred publication -> final effect/capability seal -> existing effect
assertions. C owns only the two existing effect fixture parts; root integrates
and verifies. Include a real typed top-level pure worker in each directly built
program/host-index/pending-routine boundary, and use that call rather than a
scalar as the spawn target. Keep all original assertions, await/join behavior,
effect negatives and lambda/method isolation expectations. Forbidden: allowing
an unchecked scalar/value target, bypassing publication, relaxing expectations,
or accepting arbitrary recovery diagnostics as success. First gate is the
complete native semantic suite and fixture-isolation target; then unchanged
P0 phase/summary/worker controls. If a corrected input exposes a separate
missing fact, map that producer/consumer before changing production code.

Root repeated the original795 native semantic failure3206/16, then only the
two-fixture correction:3228/0 and concurrent fixture-isolation PASS. The next
actual gate, test-include699, finds five inherited overcaps in AIR/MIR/parser
fixtures, not the changed effect parts. B owns only coherent contiguous test
placement/include wiring; original order/tokens/assertions/caps stay fixed.
Root verifies that gate and affected semantic execution before publication.

Follow-up actual gates:all size categories now pass, including the final
MIR local-type641->575 move into the existing AST binding-fact component298.
The moved functions and full residuals are exact; borrowed AST type-name
lifetime and the existing legacy consumer are unchanged. No new owner is
introduced. Test-transpile separately exposed a bare-name spawn fixture:
only that unit now supplies a real checked call and strict MIR, retaining its
original five output assertions. Root observed1009/0, not synthetic success.

Next bounded cost repair objective: remove redundant backing-node allocation/
linear lookup/stamps for the exact pure leaves STRING/NUMBER/BOOLEAN only.
Measured blocker remains the current795 canonical work4096 demand, whose
3728 B charges are distinct row/node work, not origin/consumer double charges.
Literal contribution and savings are Unknown until the actual gate reruns.
Existing complete shared claims already own their identity, shape and payload;
ordinary frames and the independent loan both have exactly one origin word.
Exact registered loan seeds are CALL nodes, never literals. A owns only
backing_dependencies.c: preserve claim recording and all outer current checks,
require one origin/count and node != loan seed, and return immutable zero bits
without a per-literal row. Consumers make the same checked leaf a no-op. All
nonliteral child traversal, output columns65+, read-growth, incomplete/pending,
callable/capture and transaction checks remain unchanged. No general scalar/
zero-origin subtree skipping, missing-proof permission, new owner/cache, query
warming/reordering or charge-only deletion is allowed. C owns focused direct
semantic falsifiers in the existing phase unit; root alone builds/executes.
Falsifiers: literal ID/shape/payload/flags and typed/current evidence corrupt/
restore through cached summary/output and the independent loan, false literal
loan seed, wrong origin width, and an eager retaining call replacing a leaf.
Acceptance: phase unit, original4096/4097 and P0 summary/worker controls, Map/
IR/component, unchanged full canonical. A smaller first failure is not closure.

Root's exact leaf repair result:phase5248/0, summary/P0/worker and unchanged
4096/4097 controls PASS10.69s. Full canonical remains RED17.90s,
peak769560KiB, same15:6 work4096/18410 cascading errors. This removed a real
per-literal operation but did not unblock admission; no cold-census reduction
or driver-memory result is claimed. Final physical-component build SHA256
21a4b6e14750aea167fa99cdd51299019060789b101fb600a75d7a9eaaff93b6:
all size gates,phase5248/MIR327/semantic3228 PASS; Map24/58+IR19.73s,
MIR ownership projection0.63s, C/LLVM Slot4-positive/7-negative9.86s,
build inventory16.41s PASS. Bootstrap/install/full DRV-2 memory remain OPEN.

The next original size gate also reaches four inherited production699 overcaps.
B's bounded placement plan keeps all logical owners and their facts/stores:
MIR ABI-layout output moves to its existing runtime-ABI component; the complete
collection transition state/validator becomes a physical component of the same
branch-fact owner, with its two shared queries defined once in the original
core and declared privately; Slot member Read/Write/Release emission moves as
one responsibility with exact handled/error/fallthrough and receiver/value
ordering; LLVM argument materialization moves to the existing call-args owner
with explicit success/output and saved return-context restoration. Only two
new production translation units and required private declarations/build wiring
are allowed. No new semantic family/registry/cache, public contract, fallback,
budget or line-limit change. Root verifies real native/LLVM build, size gates,
MIR facts/transitions and C/LLVM Slot behavior after reviewing the full delta.

Candidate next join (scratch preparation first, not admission): consume the
existing current closed no-output graph together with the current Void/physical
primitive return header, excluding private stores, anchored sources, own/inout
formals and mutable outputs. The two premises together can remove only RETURN
backing; graph alone or scalar result alone cannot. Keep named eager access/
escape collection, CALL/CHANNEL and array-storage facts unchanged. Use an
explicit internal output-producer reason for this join, not a naked empty row:
cached summary/output queries must require its ready row, current scalar header,
complete topology and the same closed graph. Missing/changed producer or ready
fact fails closed, including the early no-output-ready path. The ordinary
BACKING_FRAME and String/aggregate/unknown/capture/mutation paths remain.
A prepares only summary/internal-header scratch delta; C prepares phase-unit
falsifiers in scratch, while root verifies/checkpoints the current repair.
No production application before root reviews both complete deltas. Required
counterexample: retained `Hold(Echo(input))` followed by a physical return;
plus current return Type/AST/producer/readiness corruption and restoration,
SCC late effects, private stores, missing/opaque facts, and original4096/4097.
Once issued, the new scalar reason must survive later SCC passes only while
the full live conjunction still holds; missing premises explicitly refuse,
not silently downgrade to the weaker ordinary selector. The typed scalar
classification remains owned by the current publication header. Scratch
summary measures604/599, so its existing entry-reservation lifecycle function
moves verbatim to the existing store allocation/retirement component before
application. No new semantic owner/file or cap/format compression is allowed.

Root applied the complete scalar producer/falsifier delta, preserving the
allocator body. Phase5440/0, original summary/worker/P0/4096/4097 PASS9.67s,
Map24/58+IR PASS19s. Full canonical still RED19.26s/769624KiB, same primary
4096/cascade18410; frozen external scalar review found no new unsafe admission
within its bounded cases. One new current-only census, not a matrix run,
preserved identical4273 signatures/3188 keys and restored all bytes. Failures
202->173 (29 resolved/0 new), admitted mask differences0, incomplete0,
46.75s/769732KiB. First warm E86+B3728+G282 remains4096. Receipt and limits:
geometry audit follow-up; Linux `.tmp/p0-current-scalar-census.ICWGLF/`.
Consumer-only identifier visits are a next hypothesis, not yet an edit or
permission:1165 charged identifier shapes include real origin reads too.
Measure/remove only a redundant per-leaf row/work operation while keeping
current shared claims/typed binding and every actual origin/read-growth path;
no zero-origin subtree skip or query-order/budget change is allowed.

Next bounded objective: delete only consumer-only IDENTIFIER leaf row allocation,
linear lookup and visit/work stamps, not origin reads. Priority is current
identity/binding/claims, eager effects and read-growth before cost. Existing
backing/claims owners produce the same relation; last consumers are summary
outputs and the independent returned-loan admission. Current a8713e9 candidate,
fixed full canonical, work4096 and diagnostic first-demand1165 identifier charges
bound this hypothesis; actual removable overlap and savings remain Unknown.
An exact non-loan AST_IDENTIFIER consumer has no children or transfer operation.
Keep claim recording/nonzero identity and current typed binding validation,
including rejection of binding0. backing_origin identifiers retain all origin
merges and zero-to-growing read stamps. Calls, casts, members, indexes, captures,
branches, expression effects and all origin/destination reads stay unchanged.
The loan seed is a checked CALL and cannot be waived as a leaf. Do not broaden
the exact leaf predicate or delete budget charges while retaining their work.
A edits only existing backing_dependencies.c; C adds currentness/eager replacement
and late-growth falsifiers in the existing phase unit; root builds/tests under
one lease. First phase and original4096/4097/P0 controls, then unchanged full
canonical; only if it admits proceed immediately to bootstrap/install/memory.
No new owner, cache, annotation/copy, test-text scan or cap increase is allowed.

The identifier-only candidate passes phase5503/0 and original summary/P0/
worker/4096/4097 PASS10.95s. Canonical RED22.42s/769740KiB reaches a different
frontier:8173 cascading errors, no failed-closed/work-budget diagnostic, first
SemanticAstSignatureTypeExpressionFormalIndexInRow:310 ref facts gets ALL.
This is not zero refusal or a measured173->N census. Root's standalone unused
ref + inout + typed None example fails on both frozen scalar and current
binaries (`.tmp/p0-none-meta.VgDoYN/` in Linux), so the meta seam predates the
consumer-only change; the earlier global failure concealed it.

Revisited complete meta chain: type_checker_expr's typed record_value(meta=true)
is the authority, publication preserves that current fact, backing_origin
already treats it as carrying no input. Its backing node and the consumer-only
leaf reach two last consumers that nevertheless require a variable binding.
Existing shared claims seal readiness/physical classification but not meta.
Repair exactly four existing files: internal claim bool value_meta; semantic
claim capture/replay of current(ok&&meta); consumer-only identifier and final
backing-row identifier both require current value.ok&&(meta||binding!=0).
No origin, name whitelist, owner/API, allocation, geometry or cap change.
Unpromoted/dead claims retain their existing semantic boundary. Missing typed
facts and real variable binding0 must still refuse; meta true->false and
variable false->true must invalidate cached summary/output/loan, then recover
only after exact restoration. C adds normal typed None to the existing inout+
static Echo loan control and tests those three consumers; root independently
rebuilds, runs phase/summary and unchanged full canonical before integration.

Root meta integration0014b8a build/size PASS, new combined None+loan phase
baseline still RED (one typed assertion), with the original5440 checks intact.
The reached alias consumer is part of this same chain: LET invokes
backing_destination_alias on an identifier initializer before origin handling;
it calls backing_place and rejects a legitimate typed meta binding0. Revisit
the map before extending: current(ok&&meta) means no alias destination, while
ordinary/missing values keep the original binding/alias path. Shared meta
capture/replay remains the currentness authority. Do not change origin/read
growth or store/retaining behavior to make the positive pass. The standalone
probe also still refuses (CALL now rather than RETURN/CHANNEL); no closure.

Soundness blocker supersedes cost work. Independent frozen review
`.tmp/redteam-meta-39a8fc1db-20261016/review.md` reproduces ref String -> local
alias -> return -> borrowed record caller with published RETURN0/MIR admission.
The no-meta pair also passes frozen a871, so this predates the meta candidate.
Direct-return control refuses. A stronger Make() lets the result outlive its
local dynamic record. C/LLVM source emits pointer copies, not independent
Clone/retain; no unsafe program was run and runtime UAF is not claimed.
RETURN0 skips the actual return_loan_admitted consumer, making this a concrete
proof-obligation bypass. Candidate remains UNPUBLISHED and soundness-BLOCKED;
do not use the bounded meta controls as whole-candidate approval.

Closure objective: current typed routine header owns result classification;
the existing per-input backing frame, not a no-store graph or named zero mask,
owns whether local/field/branch/loop/call result origins reach descriptor RETURN.
Connect that relation independent of an already-present named RETURN. Last
consumers are summary_for_call -> final RETURN fact -> return_loan_admitted,
plus output_dependency_for_call. Keep CALL/CHANNEL/access/storage and SCC
pending/settling; incomplete relations fail closed. Old CLOSED_SELECTOR must
not substitute for descriptor-return provenance. No one-LET/name exemption,
manual Clone/annotation, new owner/cache, skipped safety or increased cap.
Source survey resolved the classification boundary: ordinary COPY_ONLY includes
String and is not backing independence. Existing publication owns current
formal/result Type identities. Its narrow return-backing-required query will
separate this type premise from no-output/closed-scalar permission, preserving
async scalar/Void and primitive-input capture cases. Anchored handles get no
descriptor-return exemption; existing Slot/borrow boundaries remain in force.
At SCC completion, a required backing relation must be complete/current, never
an unresolved zero or a CLOSED_SELECTOR substitute. Preserve pending iterations
without prematurely OR-ing a permanent conservative bit into a live SCC.
Independent edit scopes on this one repair: B publication routines/header typed
premise; A summary evaluate/current/final completion consumers; C existing
phase/summary behavioral falsifiers. Root alone builds and integrates, first
observing the old binary fail new tests, then checking the same full controls.

First integration exposes the remaining reached producers, not new work tracks.
All109 paired MIR cases were checked once: only independent StringTrim regressed;
the12 original worker/capture positives still pass. The phase unit additionally
finds local whole-value replacement losing RETURN to a destination-only CALL,
inferred headers published before final return inference, and RenderMoney's
builtin String+ lacking a checked independent-origin receipt. The repair must
separate local replacement from mutable contents through the existing exact
replacement/local-definition owner; never erase member/index/using stores.
Publish inferred routine headers only after their return Type is final. Extend
the existing independent String producer for checked allocating StringTrim,
and issue builtin-concat evidence only after overload/unknown/coalesce paths
are excluded by the typed operator owner. Publication/claim replay must seal
that receipt and origin_ready; no syntax-only binary-zero or COPY_ONLY=fresh
shortcut. Preserve region lifetime obligations separately from input-backing
independence. A owns replacement/header timing, B typed String-result receipts,
C their positive/negative/currentness controls; root retains the build lease.

Strict paired diagnostics reveal a final reached SCC obligation: mutual
descriptor-return routines keep complete-but-pending backing equations after
their masks/outputs stop growing. The old settling flag turns such dependencies
into permanent incomplete facts. Replace that transition in the existing
relation owner: each frame records only the exact call/ordinal dependencies
that actually caused pending; at a stationary demand boundary validate current
frames, equations and every external-current or same-active-group dependency,
then issue readiness atomically. Missing/opaque/stale/building/incomplete edges
cannot be promoted. Retire dependency storage with its frame. Final existing
current validation remains mandatory; no generic-error oracle relaxation or
unconditional pending=false. This is one fact family's completion evidence,
not a new owner/cache/demand lane. Separately retain the original structured
capture denial when later relation work fails as its consequence; failed typed
input must never become an empty permission.

The meta candidate's one current census completed before this new safety repair:
same4273 signatures/3188 keys,173->290 budget failures (10 resolved/127 new),
223 admitted masks differ. Diagnostic73.44s/769484KiB; restore byte-identical.
First root ExclusiveValueFactsFrom input1 -> ConsumedAt: E9+B4075+G12=4096;
Classify uses3314 charges across4 input rows/922 distinct AST nodes. These are
not the alias repair's results or an ownership-refusal completion count.
Receipt Linux `.tmp/p0-current-meta-census.eTjnOy/`. No more cost changes until
the returned-origin safety producer and its falsifying controls are repaired.

## Observations

- The inherited main worktree and Claude lane use different formal-effect and
  focus carrier layouts. They must not be combined by file timestamp.
- WSL was restarted before takeover: the handed-off remaining-target process
  is no longer running. Its log, not a running-job assumption, owns the result.
- Publication and CI-green claims remain OPEN.
- The bounded `IsSome` correction passes the native publication phase unit
  (3708 checks, zero failures, repeated independently by root) and generator drift. The full
  canonical still refuses: its first owned error moves from the 4096 demand
  cap to `parameter program-point index allocation failed`; the unchanged
  18408 total includes cascading publication/borrow failures. Diagnose actual
  index currency/allocation before choosing the next repair. The local heap
  rebinding experiment did not unblock the chain and was removed completely.
- Index failure classification now identifies a current, prepared index whose
  claims reject an unreachable constructor call after `Die`. The selector
  visited that dead statement even though the existing CFG/execution owner
  supplied no live call publication. The intended repair consumes the existing
  execution fact before descending a dead statement and snapshots its presence
  and reachability for current replay. Stale/missing execution evidence must not
  create a no-output permission; reachable opaque calls remain conservative.
  Paired live/dead calls and changed-deadness controls falsify this repair.
- The next fixed-demand census (same input/cap) is `EmitStmtList` parameter 5,
  `ref CodegenSemanticBodyTypeFacts`: 159 summary evaluations + 3776 backing
  nodes + 161 graph nodes = 4096. The reached pure type lookup pays for opaque
  scalar arguments and zero-argument local constant calls. Repair this facet
  coherently: use current physical-scalar argument evidence only to exclude
  carried backing (still visit the argument expression), and let the original
  publication identify an exact call-level local callee for the existing
  closed-body graph. No ordinal is invented for a zero-argument call. Typed
  identity, arity, binding and header currency remain mandatory. Preserve
  actual inout/heap-alias walks and separate RETURN/CALL/CHANNEL/result-origin
  facts. Falsifiers: retained heap setters, opaque/extern/indirect zero-argument
  targets, changed callee/body/type facts, and the unchanged work-budget cases.
- Root's clean Linux `b791c0b7a` observations: component contract PASS 11.45 s
  within 60 s; native summary behavior and unchanged 4096/4097 demand controls
  PASS 10.01 s, including the 3708-check publication unit. Receipts:
  `/root/pgy-main-20261016.component-joined.log` and
  `/root/pgy-main-20261016.native-summary-controls.log`. Focus recovery remains
  blocked before probe execution: `c-probe-compile`, not a restoration result;
  `/root/pgy-main-20261016.focus-main.log`. No integrated bootstrap, installed
  driver, memory or current GitHub CI acceptance yet.
- Native candidate checkpoint `59af37fb6` implements the sealed dead-statement,
  physical-value and original call-level graph corrections, without new owners
  or language syntax. Root independently repeated its publication unit: 3761
  checks, zero failures (0.02 s). An instrumented full-input run outlived the
  300-second budget and was terminated on its verified own PID at 306 s. That
  is INCOMPLETE_BUDGET, not successful admission. Remove instrumentation and
  measure the identical input with `timeout 300` before deciding whether any
  reached repeated operation is actually closure-blocking.
- Closure-blocking cost record: the uninstrumented `59af37fb6` implementation
  also ended INCOMPLETE_BUDGET (`timeout 300` child exit 124; wall 308.81 s,
  user 308.60 s, maximum RSS 596896 KiB, no compiler verdict). All seven native
  implementation/test blobs in the Linux build match the frozen commit; native
  binary SHA-256 is `27a98b788157828ef32186b9cf7089028263a8e8438864bf1c2bd448a9746225`.
  Input remains the full import-composed `src/self_hosted/codegen/main.pgy` at
  this tree, not a subset. Receipt: `/root/pgy-main-20261016.a-final-empty-canonical.log`.
  The blocked gate is full native admission within 300 s. The reached candidate
  operation is transitive backing/claim/publication current-validation; its hot
  attribution is not yet measured. Instrument only those existing owners in a
  scratch source, then change only the proven repeated operation. No global
  cache, relaxed safety, reduced input, new owner or increased budget. Recheck
  stale facts, shared diamonds/cycles, all escape controls, original demand
  limits and this same full-input gate before returning to bootstrap/install.
- Scratch attribution now confirms repeated current-validation of a shared DAG,
  not repeated visits required by a cycle: one root validates 722 routine calls
  for 20 unique routines (36.1x, cycle count zero, depth five). The early sample
  has 49152 backing validations over 258 boundaries, 50232 claim validations
  and 1836842 publication value reads; a common leaf is replayed 20560 times.
  Keep the repair local to one synchronous current-validation boundary: enqueue
  each exact routine once and still validate every reached fact/call/header.
  Discard the visited set on return, so a later query must detect changed facts.
  This is not a cross-query cache or an unchecked generation assumption.
- The later scratch sample reaches 9808 calls for 19 unique routines in one
  boundary (no semantic result was claimed; the verified own profile PID was
  stopped after 78.57 s). A implemented the local queue in the existing
  validation owner, removed the old recursion flag, and preserved all fact,
  header, call and argument checks. Root independently repeated its 3797-check
  unit, including shared-diamond/cycle leaf corruption and restored next-query
  controls: zero failures. A reports the uninstrumented full-input gate now
  returns a real refusal in 10.10 s (peak597032 KiB, exit1), rather than timing
  out; the next primary is the unchanged 4096-work guard at source line343.
  This removes the measured cost blocker, not the semantic refusal. Return to
  the original demand's complete dependency set; bootstrap/driver/CI stay OPEN.
- Structural reconciliation: routine generic-constraint JSON projection moves
  unchanged from the aggregate serializer to the existing routine-parameter
  JSON owner (already imported by both consumers). Function identity and
  emitted bytes stay stable; there is no second projection or fallback.
  This keeps the aggregate owner below its unchanged 600-line ceiling. Small
  per-responsibility cap deltas below 600 cover the lane's actual owned-text
  copies and final bundle field, not compressed code or permission changes.
- Shared WIP checkpoint: `78cc0ba23`. Merge resolution keeps the lane's coherent
  copy-edge/last-use API and final focus schema instead of the incompatible
  earlier typed-edge API; the earlier source remains recoverable in this
  checkpoint. The inherited native publication and typed Map query checks,
  indexed import membership, and both independent inout fixture sets are kept.
  The remaining non-overlapping WIP is included and must be validated too.
- A Windows mapped-file replacement refusal occurred while resolving one
  owner. Mechanical resolution now uses atomic replacement with previous
  conflict contents retained under `.tmp/p0-merge-recovery-20261016/`.
