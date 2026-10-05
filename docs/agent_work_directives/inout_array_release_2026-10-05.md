# Caller storage authority after synchronous inout

Status: ACTIVE IMPLEMENTATION. Base: `9fd9ac06fa9521339ceef5c5f38572247a6b6cc2`.
This card coordinates implementation; it is not a semantic owner or closure receipt.

## Objective card

- Objective: a caller's exclusively owned plain-element array remains releasable
  after a synchronous `inout` call that neither retains nor replaces its descriptor.
- Priority: existing ownership, exact callable/formal identity, fail-closed missing
  evidence, negative release gates, native/self-host and installed parity.
- Fact owners: native demanded function-parameter flow owner (a distinct storage
  preservation facet, not a reinterpretation of Slot escape bits); self-host
  storage-release owner over admitted signature, binding and expression facts.
- Last consumers: native `semantic_array_storage_call_argument` and self-host
  `SemanticArrayStorageCallArgumentEscapes`, then public `ArrayDrop` admission.
- Forbidden fallback: allow-all `inout`, GUI/callee-name exemptions, treating an
  unknown Slot summary as preservation, granting previously absent ownership,
  permitting borrowed-formal release, or reopening callee bodies in consumers.
- Falsifiers: caller release after a descriptor rebind, alias store, retained or
  returned descriptor, deferred/worker handoff, unknown target or owned forwarding.
- Integration gate: `tests/self_hosted/parity/public_array_drop.sh`, including
  valid inout mutation execution and existing diagnostic-specific refusals.

## Scope and coordination

Root owns this contract implementation, its fixtures, verification and Git.
Preserve the existing dirty expression-graph owners and component checker changes;
they belong to the separate bootstrap investigation. Do not open another worktree
or send new work to the Main chat while its existing verification finishes.
Alrescha is a read-only consumer, not a compiler dependency or whitelist.

Static gates: 60 seconds. Focused parity: 5 minutes. Integration shard: 30 minutes.
Compile-only negative cases never execute a rejected/unsafe program.
The source proof consumes one validated occurrence-to-surface partition from
the expression-graph owner. Match and foreach owners append isolated binding
nodes after ordinary expression occurrences. The proof accepts only their
carried handles with neutral binding/call identities; unknown uncovered nodes,
cross-root edges and duplicate roots fail closed.
Results remain implementation candidates until execution and exact published-CI
evidence exist. Native PASS alone is not installed-driver or GUI readiness.

## Reached installation prerequisite

The official codegen seed currently rejects the fresh loop-local environment's
`own` cleanup (`ast_initializer_type_fact_owner.pgy`, node 50372 in receipt
`run.GryAGK`). This is a prerequisite of P0, not a parallel SoT cleanup track.

- Objective: connect a fresh lexical loop-local definition to its synchronous
  own-transfer event, including an error branch that unconditionally returns.
- Fact owners: existing definition/storage authorities, retirement-order owner,
  and ordered own-argument use owner. No spelling-based cleanup exemption.
- Last consumer: own-argument live-entry admission, then ordered post-transfer
  use validation in the full MIR-root bootstrap.
- Forbidden fallback: globally promoting sentinel zero to live, restoring a
  consumed definition, forgetting a nonterminal branch's transfer, admitting a
  nested repeated/deferred/worker use, or reopening a callee body.
- Falsifiers: same-iteration duplicate transfer, reuse after a nonterminal
  conditional cleanup, use in the terminal return expression, outer generation
  consumed by an inner loop, aliases, reassignment and deferred cleanup.
- Root owns the reached admission and transfer changes only after Main's current
  exact-root receipt is terminal. Do not change a running verification's inputs.
  The narrowed proof remains an implementation candidate until focused C/LLVM
  analysis and the unchanged official bootstrap gate observe its result.

## Reached readonly-field prerequisite

The next official receipt `run.PnUGqq` passed the cleanup entry but rejected
node 50408: forwarding `function_tables` array fields to a proved synchronous
indexed reader. This remains an installation dependency of P0.

- Objective: borrow a current direct field for a proved indexed read, without
  granting descriptor transfer, mutation or deep-element release authority.
- Fact owner: the existing ordered member-transition pass, exact carried
  formal/member identities and admitted call/formal modes.
- Last consumer: effect-3 call-argument admission, then unchanged full-root
  official bootstrap.
- Forbidden fallback: member spelling/type-only grants, callee-name exemptions,
  accepting a moved field, an opaque/own/inout root handoff, a whole-root alias,
  deferred/worker use, or reopening callee bodies at the read consumer.
- Falsifiers: extracted field reuse, whole-root copy or consuming handoff,
  non-read callee effects, deferred read and missing exact identities.
- Root alone edits this reached seam. No general aggregate-lifetime migration
  or parallel implementation rung is authorized by this prerequisite.

## Reached local-read prerequisite

Receipt `run.xuCJ4z` passed formal-field forwarding but rejected a local indexed
read at the same call. The exact current-source observer identifies
`member_root_names`: a fresh empty descriptor populated by a proved shallow
inout call before a read-only loop. Source admission currently treats the
completed descriptor update as if the same local were a stale sibling alias.

- Objective: read through that exact current local descriptor after a known
  shallow update, without granting copy-out, owning transfer or deep drop.
- Fact owners: current definition/storage authority, completed borrowed
  transition and existing ordered transfer/unknown-effect owners.
- Last consumer: effect-3 local read entry, then the unchanged full MIR root.
- Forbidden fallback: all shallow calls are safe, mode-only grants, sentinel
  zero promotion, admitting a sibling/borrowed alias, unknown/storage escape,
  detached reads or use after consume.
- Falsifiers: sibling alias reads, deferred/unknown effects, same-binding
  repeated cleanup, consumption before read and forged/noncurrent definitions.
- Root alone owns this reached dependency. General SoT work remains inactive.

## Reached repeated shallow-call prerequisite

Receipt `run.lG9AZH` passed the indexed-read boundary and rejects node 51162:
`SemanticAstGenericSpecializationAppend` updates the same caller-owned array
inside the generic-call loop. Shallow copyout invalidates sibling descriptors,
but is not a consumption of the updated caller descriptor.

- Objective: distinguish exact descriptor consumption from sibling-invalidating
  shallow calls in the existing definition-bound negative effect inventory.
- Priority: exact current definition and storage authority, explicit negative
  consumption facts, no detached use, then executable installation parity.
- Fact owners: argument permission effects and their existing definition-effect
  closure; no callee-body reopen or second source scan.
- Last consumer: shallow-call/current-local-read admission, then official seed.
- Forbidden fallback: accepting all zero bounds, aliases or borrowed formals;
  dropping a consuming, unknown, rebind or storage-escape obligation; allowing
  release-and-grow loops or deferred/worker capture.
- Falsifiers: repeated caller updates succeed; repeated release, own transfer,
  sibling use, alias mutation, unknown calls and detached reads remain refused.
- Root owns this dependency; freeze all gate inputs before each verification.

## Terminal retention integration boundary

Main is interrupted and idle as observed on 2026-10-05. The user selected
single-writer completion here; Root owns source edits, integration and Git.

- Objective: a terminating branch must not poison its untaken continuation,
  but skipping continuation effects must not admit further array use inside
  the same return expression.
- Priority: exact callable/formal and expression identity, fail-closed tail
  transfer, unchanged continuing retention refusal, then official installation.
- Fact owners: the admitted expression surface root and the formal descriptor
  retention summary. The reached retention-entry owner makes the decision.
- Last consumer: call-argument admission before the terminal call is executed.
- Forbidden fallback: treating every terminal expression as a completed
  transfer, ignoring later nested evaluation, or reopening a callee body.
- Falsifying case: `return Pair(ErrorFacts(names), Matches(names))`; only the
  terminal root call may claim completed-return retention. Compound transfers
  without a separate proof remain refused.
- Gate: the borrowed-descriptor C/LLVM analyzer shard, including this negative,
  then the unchanged official seed and installed-driver public ArrayDrop gates.
- Integration refinement: continuing retention is recorded only after an
  admitted call, keyed by its exact current storage definition. Neither an
  initial Unknown state nor a conservative future effect proves that a prior
  retention occurred. Ordered own-transfer validation remains the owner of
  consuming-call expression order; do not replace it with the tail restriction.

## Reached terminal aggregate reservation prerequisite

Official seed v9 reaches `aggregate_release_source_not_live` at the return of
`SemanticAstGenericParameterFactRowsFromOwnerNode`. Its exact constructor input
is already admitted, but terminal effects are deliberately absent from the
continuing escape bound. That absence must not be confused with a missing
constructor identity or with permission to consume borrowed storage.

- Objective: reserve the exact current owned input at terminal constructor
  completion without poisoning the untaken continuation.
- Priority: constructor input/definition identity, current storage and element
  authority, prior escape/retirement refusal, then official seed execution.
- Fact owners: the existing constructor-input inventory and ordered aggregate
  release transition; continuing escape bounds retain their current meaning.
- Last consumer: aggregate reservation at the physical constructor completion.
- Forbidden fallback: guessing an escape, permitting borrowed/unproved input,
  dropping element provenance, or accepting an earlier retention/consumption.
- Falsifiers: owned fresh return succeeds; borrowed return, duplicate storage
  and same-expression reuse after an inner transfer stay refused.
- Gate: the focused C/LLVM observer shard, then the unchanged official seed.

## Measured closure-blocking retention lookup

- Revision: `d0fa49ea95928c007a6670f2e2fbfb7b7479441b` plus the current
  uncommitted P0 source; no semantic registry promotion.
- Fixed input: `callable_table_from_artifact_release_probe.pgy`, SHA-256
  `3f0c6cb8dfd2c3450f5541adbdba40083850840e78c0327bd4bd77949bc15ce9`.
- Diagnostic executable: `.tmp/inout-array-release/body-pressure-probe-v2.exe`,
  SHA-256 `0df99055be65e65ba35d6e9c62813e18eb4ad44984016d137b72ce70b8146192`.
- Existing blocked gate: collection-inout-effect actual producer unit, 60
  seconds. The diagnostic body took 135781 ms; 26 direct indexed-element
  retention queries took 82141 ms in the global lookup. The existing context
  lookup produced equal Option identities for all 26; individual timer readings
  were zero milliseconds, not a claim of zero computational cost.
- Repeated operation: `SemanticExpressionCallIdentityForNode` searches every
  preceding root and recomputes each root start. The formal-use pass already
  owns the validated current root start, syntax owner and lane.
- Fact owner: the existing sealed builtin argument-retention call facts.
- Last consumer: the formal-use inventory before the formal-effect closure.
- Change only that consumer's lookup to the existing context-bound owner;
  preserve seal, owner, lane, ordinal, builtin identity and missing-fact checks.
- Falsifiers: forged owner/lane/ordinal, missing seal and crossed call targets;
  no unchecked identity reuse or global-lookup fallback.
- Verification: re-run the same 60-second actual producer unit with a fresh
  executable and the existing collection identity/negative integration gate.
  Then return immediately to official seed/installed-driver P0 completion.

## Reached owned-mutation terminal read

Seed v11 passed aggregate reservation and reached the generic-parameter
contract's `return Populate(names, defaults) && ... names[0] ...` expression.
The tail-only guard is needed for omitted retaining/shallow effects, but a
proved owned-content mutation may be followed by a read of its updated current
descriptor when that descriptor is unshared.

- Objective: admit that exact unshared owned-content mutation/read sequence.
- Fact owner: current definition/storage authority and constructor-input facts;
  the terminal storage-effect entry owner owns the bounded admission proof.
- Last consumer: call-argument admission in the existing ordered event replay.
- Forbidden fallback: effect-kind alone, borrowed storage, a sibling descriptor
  or a descriptor captured by a constructor in the same terminal expression.
- Falsifiers: owned mutation/current read succeeds; alias and constructor
  capture followed by the mutation stay refused. Retaining and shallow calls
  still need the existing exact-tail restriction.
- Gate: the focused C/LLVM analyzer shard, then official seed/installed driver.

Seed v12 subsequently passed the fresh-local case and reached direct inout
forwarding in `SemanticAstIntentExpressionSeedEnvironment`. Terminal chained
mutation of that same formal uses its complete formal-use summary, exact mode
and absent descriptor-retention summary. Descriptor copies/views and opaque
uses invalidate that summary; neither a formal mode alone nor an unproved
owned formal supplies this exception. Retaining and sibling-view negatives
remain unchanged. The next official seed must verify the full MIR root.

## Measured closure-blocking LLVM object emission

- Revision: `d0fa49ea95928c007a6670f2e2fbfb7b7479441b` plus the current P0
  source after focused gate18; no registry promotion or unrelated optimization.
- Fixed input: `nominal_constructor_source_arity_probe.pgy`, SHA-256
  `22008b8dbc0c7dceded9fc2a8e628c449da57b865a00e578efd9e15455498dc7`.
- Native executable: `.tmp/inout-array-release/bin-v2/pgy.exe`, SHA-256
  `fe5b4f3444a982d48b56b4d31481412d8671bc1fc8002285bb000b3cf41ce7ed`.
- Blocked gate: collection-inout-effect LLVM source observer compilation,
  unchanged `--opt=dev` and 120-second acceptance bound. Integration9 timed
  out before any LLVM observer execution.
- Timestamped diagnostic: 144.82 seconds total, approximately 7 seconds IR
  emission, 34 seconds LLVM IR optimization, 95 seconds target object emission.
  Receipt `.tmp/inout-array-release-source-context-llvm-stamped.log`; seconds
  have integer timestamp resolution. An earlier diagnostic took 228.07
  seconds; neither diagnostic is an integration PASS.
- Reached operation: `LLVMTargetMachineEmitToFile` reruns aggressive machine
  optimization for every admitted routine, even under the fixed dev profile.
- Objective: let the existing dev profile select unoptimized machine emission
  at its existing target-machine owner, without changing admitted IR, runtime
  safety attributes, the O2 IR pipeline, or release's aggressive machine mode.
- Priority: semantic identity and mandatory verification, explicit profile
  consumption, unchanged negative gates and input, then bounded compile cost.
- Fact owner: the compiler's existing optimization profile; target-machine
  lifecycle is its only machine-code consumer here.
- Last consumer: object emission after verified MIR, AIR/projection admission
  and LLVM module verification. No semantic check or retention fact is skipped.
- Forbidden fallback: a new environment switch, relaxed timeout, smaller
  correctness input, skipped LLVM leg or a different user-supplied profile.
- Falsifiers/gate: rerun the same full collection integration on both backends
  with a freshly identified native executable; public ArrayDrop positives,
  diagnostic-specific negatives and installed-driver parity remain required.
  Return immediately to P0 seed and driver completion after this budget blocker.

## Reached conditional use of a fresh repeated generation

Seed v13 passed direct-formal terminal forwarding and reached the environment
assignment in `SemanticAstGenericSpecializationFactsFromAdmittedBody`:
node `51211`, `unproved_inout_copy_entry`. Its arrays are freshly declared in
the outer loop body and used in a conditional child scope before their owned
cleanup. The call-argument consumer was asking the narrower direct-sibling
retirement proof rather than the existing conditional-generation owner.

- Objective: consume the existing exact generation proof at call admission.
- Priority: definition identity, lexical generation and no intervening repeat
  or detached use, then official seed and installed-driver evidence.
- Fact owner: `SemanticAstCollectionFreshLocalGenerationAtUse`; no new scan,
  shadow generation policy, or duplicate definition validation.
- Last consumer: call-argument mutation admission when the conservative
  prepass reports a repeated retirement bound. Element/storage owners remain
  mandatory, and an observed prior consumption is still refused.
- Forbidden fallback: accepting every loop use, guessing fresh storage, or
  bypassing nested-loop, deferred or worker boundaries.
- Gate/falsifiers: fresh loop-local conditional mutation succeeds; nested
  repeated own transfer and deferred same-generation access remain refused in
  the focused C/LLVM shard, then rerun the unchanged official seed.

## Reached generation-bound retirement projection

Seed v14 passed conditional mutation and reached generic-call capture at
node `51242`, `unproved_indexed_read_entry`. The fresh outer-loop environment
is read inside its work loop and handed to owned cleanup afterwards. A global
repeat sentinel for that later cleanup erased the current generation's order.

- Objective: project known consumption at its exact generation-relative
  syntax site when the existing fresh-generation owner proves the consumption
  event belongs to that generation. A read consumer must not reconstruct or
  ignore an erased future event.
- Priority: exact current definition/storage and event identity, preserve
  unknown/escape/sibling invalidation, reject prior or nested consumption,
  then official seed and installed-driver evidence.
- Fact owner: call-effect projection consuming the existing
  `SemanticAstCollectionFreshLocalGenerationAtUse` fact; generation scope
  policy stays in that owner.
- Last consumer: existing definition-bound retirement/read admission.
- Forbidden fallback: rewriting every zero bound, weakening opaque effects,
  granting alias authority, admitting an inner loop's repeated consuming event,
  or weakening deep-element cleanup requirements.
- Falsifiers/gate: fresh outer-generation repeated reads before later cleanup
  succeed; outer storage consumed inside an inner loop, post-transfer reuse,
  alias consumption and deferred cleanup stay refused. Run focused C/LLVM,
  unchanged official seed and full collection integration before acceptance.
- The implementation candidate consumes that fact in the effect producer,
  not in a read exception. Its cohesive projection owner grows from 176 to
  189 counted lines; the private inventory cap is explicitly 200 rather than
  minifying or introducing a policy-hiding wrapper. The general 600-line gate
  and the other outstanding structural cap failures are unchanged.
- Gate20 rejected the first candidate because an early-return branch's owned
  cleanup still poisoned the untaken normal path. It is not accepted evidence.
  The revised candidate consumes the existing terminating-retirement scope
  from the same owner as ordered visibility. Such events stay with ordered
  same-scope use checking, while later normal events remain in continuing
  bounds. Nested reads prove generation membership only; consuming admission
  continues to require a unique event (no intervening loop). A new terminal-
  hides-later-consumption negative must falsify lost normal events.
- Gate21 rejected terminal own-entry admission when a later continuing bound
  was present. The call-entry owner now distinguishes a future bound from a
  prior one using the same exact terminal/current-generation proof. It does
  not erase prior consumption, and ordered same-scope use remains mandatory.
  Gate22 passed 15 positives and 26 compile-only ownership refusals per C/LLVM
  analyzer; receipt `.tmp/inout-array-release-borrowed-read-gate22.log`, evidence
  `.tmp/self_hosted/collection-borrowed-descriptor-read.X2bzgH`. This is not
  seed, fixed-point, input-program execution or installed-driver proof.

## Reached nested-loop cleanup with unconditional function exit

Seed v15 still refused node 51242 in the generic work loop. Its error branches
consume the outer generation and immediately return from the function. Their
consumption cannot repeat, unlike cleanup followed by an inner-loop break or
continue. A small nested-terminal-cleanup fixture is independently refused by
both gate22 analyzers, before any new generation-owner change.

- Objective: let the current generation owner prove a unique consuming event
  through intervening loops only when its direct terminating scope exits the
  function unconditionally. Do not add a read-consumer exception.
- Priority: exact generation, owned storage and exit identity; ordered
  same-scope use; fail-closed detached/unknown/loop-local exits; then seed.
- Fact owner: the existing generation/retirement-scope owner. Return/Exit and
  loop-local Break/Continue must remain distinguishable in that fact.
- Last consumers: effect projection, own-entry and ordered use checking.
- Forbidden fallback: treating any terminal statement as a function exit,
  accepting repeated transfer, ignoring an earlier consumption, or granting
  cleanup permission from parameter mode alone.
- Falsifiers/gate: nested cleanup followed by function return succeeds; inner
  break/continue cleanup and same-scope post-transfer reuse stay refused. Run
  focused C/LLVM before the unchanged official seed and installed-driver gate.
- Gate24 passed 16 analyzer positives and 29 compile-only refusals per C/LLVM;
  evidence `.tmp/self_hosted/collection-borrowed-descriptor-read.n32M5a`.
  The added terminal reuse reaches `owned_argument_use_after_move`, while
  inner Break/Continue refusal stays at `owned_argument_storage_not_live`.
  These distinct boundaries are pinned in the focused gate. A parse-only
  candidate gate23 failed before compilation and is not semantic evidence.

## Reached accumulator element ownership

Seed v16 passed the names/types read at node 51242 and now refuses that call's
`actual_type_names` inout argument at `unproved_inout_copy_entry`. The preceding
`SemanticAstGenericSpecializationAppend` uses shallow `ArrayPush` even though
this accumulator later needs owned String elements. `Concat` is not the
collection owner's deep-copy certificate; the builtin transition explicitly
keeps shallow elements Borrowed.

- Objective: make the reached compiler accumulator use the existing owned
  insertion API, not widen permission for mixed borrowed/owned elements.
- Fact owner: builtin/formal element transition. Append is its last producer;
  the later generic-capture inout call is the falsifying consumer.
- Forbidden fallback: treating a String-returning expression as an owned
  container element, dropping the clean-element requirement, or a callee name
  exception. Use `ArrayPushOwnedString` on the original borrowed actual.
- Gate: repeated owned accumulator mutation succeeds; shallow insertion then
  owned mutation stays refused. Re-run focused C/LLVM and official seed.
- Gate25 passed 17 analyzer positives and 30 compile-only refusals per C/LLVM;
  evidence `.tmp/self_hosted/collection-borrowed-descriptor-read.Kj2gOq`.
  This validates the reached producer choice, not seed/installed execution.

## Reached borrowed scalar input to generic symbol encoding

Seed v17 passed the accumulator boundary and refused node 51578 at
`unproved_formal_element_use_entry`. Exact parser mapping names
SelfMirGenericSpecializationRowsFromSemantic's symbol encoder call. Inside
CompilerSymbolCGenericSpecializationName an indexed borrowed String goes to
CompilerSymbolCGenericActualName, a user call without an admitted borrowed-
scalar argument contract. The collection formal-use owner correctly refuses
that missing fact; array mode or a String result is not an escape certificate.

- Objective: materialize an independent String at that scalar input boundary
  through the existing Concat copy primitive, preserving symbol bytes.
- Fact owner: builtin argument retention/copy semantics. The naming function
  consumes the copied scalar; no array-descriptor or element grant is added.
- Forbidden fallback: blessing every user String call, array mode-only
  permission, copying the whole array, or bypassing formal element-use refusal.
- Gate: copied scalar input to naming succeeds; returning a raw borrowed indexed
  scalar through an unproved user call stays refused. Focused C/LLVM, then the
  unchanged official root and symbol parity own acceptance.
- Before changing production naming, both gate25 analyzers independently
  admitted the copied scalar input and refused the raw indexed scalar return
  at `unproved_formal_element_use_entry`. Caller-side String result lifetime
  remains conservative: the positive uses the result before dropping its
  source array; copying an input is not a new returned-value certificate.
- Gate26 passed 18 analyzer positives and 31 compile-only refusals per C/LLVM;
  evidence `.tmp/self_hosted/collection-borrowed-descriptor-read.3gwTzQ`.
  The production naming change still needs native value and official root
  execution evidence; no seed/installed completion is inferred.
- Native C/LLVM each executed four exact symbol-name assertions successfully:
  empty actuals, primitive/reserved base, nested array and nested constructed
  types. `.tmp/inout-array-release-generic-symbol-copy.sha256` binds the probe,
  production naming source and both executables. This is bounded native value
  evidence, not installed-driver or self-host closure.

## Reached runtime-value type lookup

Seed v18 passed symbol encoding and refused node 52506 in
CompilerRuntimeValueCLocalPreamble, which delegates to
CompilerRuntimeValueTypesPresent. That owner passes indexed local/parameter
type-name Strings to the user-defined runtime-value lookup. It has the same
missing borrowed-scalar call contract as the prior naming boundary.

- Objective: apply the already validated scalar copy boundary at those two
  lookup inputs. Keep ABI lookup and representation meaning with their owners.
- Last consumer: TypesPresent's Boolean projection, then C/LLVM local preamble.
- Forbidden fallback: blessing arbitrary String calls, reconstructing ABI
  layout from type text, or adding array/returned-resource permission.
- Gate: native C/LLVM exact presence/preamble assertions, existing copied/raw
  scalar admission falsifiers, then unchanged official root. Layout identity
  and source generation remain unchanged; seed/install status remains OPEN.
- Native C/LLVM each executed five exact presence/preamble assertions; both
  exited 0 and emitted the independently checked PASS marker. Manifest
  `.tmp/inout-array-release-runtime-value-scalar-copy.sha256` records the
  exact probe, production owner and executables. This does not establish
  self-host admission or ordinary installed-driver behavior; official seed
  v19 is the next falsifier on the unchanged semantic input.

## Reached participant alias result

Seed v19 passed runtime-value lookup and refused node 54508 at
SelfDirIntentUniqueParticipantForType. This value query assigns a borrowed
indexed alias to its local result and returns it beyond the sequence boundary.
The formal-use owner refuses the missing element-lifetime proof.

- Objective: materialize the selected scalar alias through the already
  validated copy builtin before retaining it in the returned local result.
- Priority: preserve the participant slice, exact unique-match rule and
  ambiguous-match result; keep raw borrowed return refused; then official seed.
- Fact owner: existing scalar copy/retention semantics; participant identity
  and lookup meaning remain with the DIR intent-step owner.
- Last consumer: the caller's using-alias value, then participant index lookup.
- Forbidden fallback: a user String-call whitelist, array-mode grant, inferred
  returned-value lifetime, whole-array copying or source-text reconstruction.
- Gate: copied selected scalar succeeds; raw indexed selection returned from
  the same loop remains refused. Verify exact native C/LLVM lookup results,
  then rerun the unchanged official MIR root before installation.
- Both gate26 analyzers independently reproduced copied-result admission and
  raw-result `unproved_formal_element_use_entry` before the production change.
  Fresh gate27 then passed 19 analyzer positives/32 compile-only refusals per
  C/LLVM in `.tmp/self_hosted/collection-borrowed-descriptor-read.b3RKyW`.
  Native C/LLVM each executed five exact lookup assertions: single match,
  ambiguity, absence, range isolation and zero count. Manifest
  `.tmp/inout-array-release-participant-scalar-copy.sha256` binds owner/probe/
  executable identities. Official seed v20 and installation remain OPEN.

## Reached inherited contract-name collection

Seed v20 passed alias selection and refused node 54573 at
SelfDirIntentStepAppendActionContractRange. The function stores an indexed
borrowed name in a local, may map `self`, then shallowly inserts that value
into a different descriptor. These are two independent lifetime boundaries.

- Objective: copy the selected scalar at local materialization and use the
  established owned insertion API for the persistent destination element.
- Priority: exact range, mapping, duplicate rejection and existing partial
  failure meaning; keep raw retention refused; then official seed/install.
- Fact owners: existing scalar copy and builtin element-ownership contracts;
  DIR intent-step owns range/mapping/duplicate semantics.
- Last consumers: requires/authorized-name accumulation in the resolved step.
- Forbidden fallback: admitting all borrowed locals, treating shallow push as
  deep ownership, a user-callee whitelist, or invented cross-storage lifetime.
- Gate: composed copied-scalar/owned-insertion succeeds; raw local/shallow
  transfer remains refused. Native C/LLVM mapping/range/duplicate assertions
  must stay unchanged, followed by the original official MIR root.
- Both gate27 analyzers admitted the composed candidate and refused the raw
  counterpart at `unproved_formal_element_use_entry` before the production
  change. The initial positive had an invalid plain ArrayDrop on String
  elements; its corrected owned-string cleanup is the observed positive.
  Fresh gate28 passed 20 analyzer positives/33 compile-only refusals per C/LLVM
  in `.tmp/self_hosted/collection-borrowed-descriptor-read.kkJRKH`. Native
  C/LLVM each passed five exact mapping/range/duplicate assertions, bound by
  `.tmp/inout-array-release-contract-name-copy.sha256`. Official seed v21 is
  running with unchanged input/budget; installation remains OPEN.

## Reached direct contract-fact borrowing

Seed v21 passed element-use admission but refused the same node 54573 at
`unproved_formal_indexed_read_entry`. The action-contract table is a local
aggregate alias, not a direct admitted readonly formal field. Existing alias
negatives must not be relaxed to make compiler implementation code pass.

- Objective: pass the canonical action-contract field directly to the step
  resolver as a required typed `ref` input and delete its local aggregate alias.
- Priority: same semantic contract identity and range; exact readonly root;
  missing/aliased fact refusal; preserve indexed-read negative gates; then seed.
- Fact owner: SemanticAstActionContractFacts inside the admitted signature
  facts. This is a borrowed view of that owner, not a second producer.
- Last consumer: SelfDirIntentStepFromArtifact; its single caller supplies
  `signatures.action_contracts` after existing artifact/contract validation.
- Forbidden fallback: a callee whitelist, accepting copied aggregate roots,
  guessing member authority, `new ? old` contract lookup or optional input.
- Gate/falsifiers: direct nested field forwarded to a proved readonly formal
  succeeds; the same field copied into a local alias and indexed remains
  refused. Both current C/LLVM analyzers reproduced these outcomes before
  the API change. Source pins ratchet alias removal; DIR oracle parity and
  the unchanged official seed own behavioral acceptance.
- The production caller/ref-input change passed the owned DIR oracle parity
  gate: exact node/edge/topology rows, intent defaults, transfer detail and
  inline intents match native, while provenance/count/endpoint mutations are
  refused. Log `.tmp/inout-array-release-dir-graph-ref-contract-gate.log`.
  Native C/LLVM participant and contract-name probes were rebuilt against
  this final owner and each passed their five assertions. The direct nested
  ref ABI fixture also executed `true` on C and LLVM; manifests bind these
  current artifacts. Its initial `Input` nominal constructor hit an AIR
  IO_READ capability refusal; the noncolliding ContractEnvelope fixture is
  the executed case. Fresh member gate5 passed 4 positives/9 refusals per C/LLVM
  in `.tmp/self_hosted/member-indexed-read.4gaBJr`. Official seed v22 is running
  on the original MIR root and unchanged budget; installation remains OPEN.
- Seed v22 passed direct member-read admission and then refused the same
  argument at the generic readonly sequence boundary: `default`, callee
  SelfDirIntentStepAppendActionContractRange, argument 0. Its names input was
  still value mode. Declare that existing read-only input `ref names`, matching
  the already tested accumulator fixture and the canonical borrowed field.
  This is an explicit borrow boundary, not a summary/mode-only permission grant.
  Ratchet the equivalent value-formal refusal alongside current ref positives,
  refresh DIR/native assertions, then rerun the unchanged official root.
- Both current gate5 analyzers independently refused the value-formal case
  at `default`, callee ReadValue, ordinal 0 before the API change. Fresh gate6
  passed 4 positives/10 compile-only refusals per C/LLVM in
  `.tmp/self_hosted/member-indexed-read.Kv8Ttd`; all three diagnostic facts
  are pinned. DIR parity gate2 is running after the required ref declaration;
  fresh native value/ABI receipts and official seed v23 remain next.
- DIR parity gate2 subsequently passed exact rows and negative mutations.
  Native C/LLVM participant and name-collection probes each passed five exact
  assertions against the ref-names owner, and nested ref forwarding again
  executed true on both. Their manifests were refreshed. Official seed v23
  is running on the unchanged semantic input/budget; installation remains OPEN.

## Reached mutable derived name ownership

Seed v23 passed readonly entry and refused the destination requires_names at
`unproved_inout_copy_entry`, node 54573. The resolver extracts a clause field
and then tries to use it as a clean owned mutable String array. The current
field facts do not establish that permission; empty length alone is no grant.

- Objective: create owned mutable derived requires/authorized-name arrays via
  the existing Clone builtin, leaving the parsed clause facts unchanged.
- Priority: preserve explicit names, semantic defaults and duplicate behavior;
  independent storage and owned elements; retain unproved-field mutation refusal;
  then original seed and installed-driver evidence.
- Fact owners: Clone allocation/element-copy contract and existing definition
  storage/element owners. Runtime String array clone copies each String value;
  it is not merely a descriptor copy or a new ownership assumption.
- Last consumer: the reached inout inherited-contract accumulator, followed
  by the resolved-step constructor. Clause facts remain the parse owner.
- Forbidden fallback: mode-only or empty-length permission, treating a field
  extraction as proved deep ownership, weakened clean-element checks or a
  special compiler-callee exception.
- Gate/falsifiers: owned field clone may be mutated/cleaned while its input
  stays intact; equivalent unproved extracted field mutation remains refused.
  Validate with current C/LLVM analyzers and native value execution, then DIR
  oracle parity and the unchanged official MIR root.
- Both gate6 analyzers reproduced the clone positive and unproved-field
  `unproved_inout_copy_entry` negative before the producer change. Gate29 then
  passed 21 positives/34 compile-only refusals per C/LLVM in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.RSBubr`. Native rejected
  the positive's temporary constructor at the required named ref boundary;
  the corrected named binding executed on both backends and printed the
  copied then surviving input values exactly. Manifest
  `.tmp/inout-array-release-member-clone.sha256` owns current value evidence.
  Native production name/lookup/ABI receipts were refreshed against the clone
  source. Fresh gate30 is running; DIR gate3 and official seed v24 remain next.
- Fresh gate30 passed 21 positives/34 compile-only refusals per C/LLVM in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.eWeKz1`. DIR gate3
  subsequently passed exact rows and negative mutations. Official seed v24
  is running on the unchanged input/budget; default installation remains OPEN.

## Reached participant scalar accumulation

Seed v24 passed the derived-name boundary and refused node 55663 at
`borrow_boundary_escape`, boundary ArrayPush. Exact parser mapping names
SelfDirIntentFactsFromArtifact's participant loop: a borrowed signature String
is shallow-inserted into a separately retained participant table.

- Objective: materialize participant alias/type-name elements with the existing
  ArrayPushOwnedString contract; preserve participant identity and ordering.
- Priority: canonical signature facts; independent copied elements; retained
  borrowed/shallow refusal; exact DIR oracle parity; original official seed.
- Fact owners: admitted intent signature facts and existing builtin copy/element
  ownership contracts. No descriptor or member authority is inferred.
- Last consumer: participant table assembly and SelfDirIntentStepFromArtifact.
- Forbidden fallback: a readonly sequence value grant, treating shallow push
  as owned insertion, or relaxing source/member permission for compiler code.
- Gate/falsifier: copied member Strings can be cleaned without invalidating the
  input; the equivalent shallow accumulation remains refused at owned_string_drop.
  Test both current analyzers, native C/LLVM values, DIR parity and original seed.
- Fresh gate31 passed C/LLVM each 22 positives/35 refusals in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.OgVPGU`. Native C/LLVM
  each printed the exact alias/type values, cleaned their copied arrays, then
  read the still-valid original alias. Manifest
  `.tmp/inout-array-release-member-scalar-owned.sha256` binds current input,
  producer and executables. DIR gate4 then official seed v25 run serially.
- A development-profile observer of the full original MIR root timed out at
  its unchanged 300-second budget without a terminal verdict. This is not
  semantic evidence; the original official bootstrap remains the admission gate.
- DIR gate4 passed. Official seed v25 refused the same node 55663, now at
  ArrayPushOwnedString; `.tmp/self_hosted/codegen_nominal_array_declaration/run.JkaueP`.
  The copied-element change alone cannot prove continuing storage permission.
  Next inspect the reached resolver's actual formal-effect carrier and its
  scalar uses; do not weaken retirement/escape admission from a guessed mode.

## Reached participant resolver formal-element proof

The current read-only LLVM observer reproduced node 55663 against the unchanged
MIR root. Its valid formal-effect carrier reports ParticipantIndex argument 0
and UniqueParticipantForType arguments 0/1 as indexed-read effect 3, but
StepFromArtifact participant aliases/type-name arguments 8/9 as unproved-element
effect 5. Log `.tmp/inout-array-release-participant-formal-effect.run.log`.
The parent loop cannot preserve storage rights across that unresolved call.

- Objective: materialize the resolver's retained/passed indexed String scalars
  through existing Concat copy semantics; preserve the caller's readonly summary.
- Priority: exact formal/call identity; scalar independence; unchanged lookup,
  selected participant and diagnostics; negative raw-element gate; original seed.
- Fact owners: canonical signature/formal effects and builtin scalar-copy facts.
  This does not create a returned-String lifetime certificate or storage grant.
- Last consumer: SelfDirIntentFactsFromArtifact's repeated participant growth
  after the step resolver call. Only the two reached participant String arrays
  and their retained or user-call scalar uses change; pure equality stays borrowed.
- Forbidden fallback: mode-only permission, named-callee exceptions, admitting
  raw retained indexed elements or dropping the precomputed negative effect.
- Gate/falsifier: repeated growth around a resolver that copies selected scalars
  succeeds; identical raw retained elements still make the caller fail closed.
  Confirm effects 8/9, native values, DIR oracle parity, then original seed.
- Gate32 passed C/LLVM each 23 positives/36 compile-only refusals in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.lTDlUX`. Native repeated
  resolver values passed on both backends. The unchanged MIR root now reports
  exact ordinals 8/9 as effect 3; its next refusal is node 55849 in FactsReady,
  `unproved_formal_indexed_read_entry`. DIR gate5 passed. Official seed v26
  reproduced that same next refusal in
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.VniKuJ`.

## Reached participant/step validation views

Node 55849 is SelfDirIntentFactsReady's lookup across facts.participants.aliases
and facts.steps.using_aliases. Nested aggregate field authority is not proved
by the existing direct-field read contract. The original public Ready boundary
and its shape/receipt checks must remain the authority.

- Objective: extract the existing participant/step membership predicate behind
  direct required readonly participant/step views derived by that same wrapper.
- Priority: one canonical whole-facts owner; existing header/range admission;
  direct readonly views; unchanged placement/who/authority rejection; seed.
- Fact owner: SelfDirIntentFacts and its existing readiness boundary. An inner
  membership predicate is not a second artifact admission or new receipt.
- Last consumer: FactsReady's step-range loop; the wrapper alone derives views
  from its own facts. ParticipantIndex becomes an explicit readonly array input.
- Forbidden fallback: external replacement views at the public Ready API,
  whole-root aliases, clone-per-step copies, general nested-member permission
  or missing/invalid-row acceptance.
- Gate/falsifier: direct composed readonly views admit; copying an input root
  inside the membership consumer stays refused. Preserve the full DIR oracle's
  row/provenance/endpoint mutations and the original official MIR-root input.
- Both prior analyzers admitted composed direct views and refused a copied
  participant root. The implementation moves the stable ParticipantIndex name
  to the participant contract owner, adds explicit ref/range checks, and removes
  only FactsReady's inline membership predicate. The public Ready signature and
  shape/receipt/range census remain unchanged. Fresh member gate7 passed C/LLVM
  5 positives/11 refusals in `.tmp/self_hosted/member-indexed-read.eXURNt`.
  Current DIR gate6 adds native C/LLVM valid/malformed-view assertions; formal
  observation then official seed v27 follow serially with source frozen.
- DIR gate6 passed the full original oracle/mutation cases and executed the
  new membership/malformed-view fixture on native C/LLVM. The optional formal
  observer exceeded its original 300s budget without a verdict; it is not
  current whole-root admission. Official seed v27 runs directly under its
  unchanged 1800s budget; source remains frozen. Full component inventory also
  exceeded its 60s static budget after the checker unit tests passed, not a
  structural PASS. No new time/memory allowance or weakened claim was added.

## Reached iteration binding-type scalar use

Official seed v27 passed the participant/view boundary and refused node 58661
at unproved_formal_element_use_entry in the compound seed call of
SemanticAstIterationTypeFactsFromAdmittedArtifactWithFunctionTables. Exact
mapping is `.tmp/inout-array-release-bootstrap-v27-exact-boundary-context.log`.
The reached SemanticAstIterationSeedVisibleRows stores binding_type_names[i]
in a scalar local before owned insertion; the formal-use owner deliberately
does not grant an unproved lifetime relation to that retained raw local.

- Objective: keep validity comparisons as immediate borrowed reads and pass the
  indexed type directly to existing owned insertion, without a retained local.
- Priority: same iteration/visibility/type checks and rows; exact builtin copy;
  raw-local refusal; original compound call and official seed.
- Fact owner: iteration type rows plus existing formal-use/builtin-retention
  owners. No new lifetime certificate, callee whitelist or mode permission.
- Last consumer: the iteration's names/types/modes seed accumulation.
- Forbidden fallback: accepting all raw indexed locals or using an unnecessary
  second scalar copy to conceal the retained-local seam.
- Gate/falsifier: direct checked indexed insertion admits; equivalent retained
  raw local stays refused. Preserve actual seeding outputs, then original seed.
- Both prior analyzers admitted direct checked owned insertion and refused
  the retained-raw-local counterpart at unproved_formal_element_use_entry.
  Fresh gate33 passed C/LLVM each 24 positives/37 refusals in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.P88YYH`. The optional
  owned-environment static script timed out at its unchanged 60s budget without
  a verdict. Native direct-seed values and official seed v28 follow serially;
  no extra allowance, default installation, full integration or CI is claimed.

## Reached aggregate-root type scalar return

Official seed v28 passed the iteration seed boundary and refused node 62011
at unproved_formal_element_use_entry. The exact caller is
SemanticAstCollectionMemberMoveIdentityForNode; its root identity callee retains
resolved_leaf_types[node_id] and returns that raw String in an aggregate.
Mapping: `.tmp/inout-array-release-bootstrap-v28-exact-boundary-context.log`.

- Objective: copy the selected type scalar at the existing Concat boundary
  before retaining and returning it; leave root identity and generic equality
  checks unchanged.
- Priority: stable binding identity; same-graph type provenance; independent
  returned scalar; raw aggregate return refusal; original official seed.
- Fact owners: member-root identity and existing scalar-copy/formal-use owners.
  No new String lifetime certificate, borrowed storage right or named exemption.
- Last consumer: the member move identity caller's readonly resolved-type view.
- Forbidden fallback: raw borrowed element escape in a returned record,
  mode-only permission or a second source of type/binding identity.
- Gate/falsifier: a record containing the copied type remains readable after
  caller input cleanup; the same record with a raw indexed scalar is refused.
  Then re-run the original MIR-root admission under the existing seed budget.
- Prior C/LLVM analyzers admitted the copied-record fixture and refused its
  raw-record counterpart at unproved_formal_element_use_entry. Native C/LLVM
  independently read the copied record after input cleanup, exact Box<String>
  output. `.tmp/inout-array-release-aggregate-scalar-copy.sha256` binds those
  fixture/executable inputs; it is not a production-root proof. Fresh gate34
  and official seed v29 follow serially at unchanged budgets, with source frozen.
- Gate34 passed C/LLVM each 25 analyzer positives/38 compile-only refusals,
  `.tmp/self_hosted/collection-borrowed-descriptor-read.Hl2Gy4`. Official seed
  v29 is now the active gate; no installed-driver or current CI claim is made.

## Reached member-read canonical type input

Official seed v29 passed the member-root return boundary and refused node 62749
in SemanticAstCollectionMemberRootReadOccurrenceFromGraph's caller. That reached
callee passes resolved_leaf_types[node] directly to the user canonical-type
function, without an owned scalar boundary.

- Objective: copy only that indexed type input through existing Concat before
  the canonical-type call; leave binding/topology/readonly forwarding unchanged.
- Priority: exact formal identity; unchanged canonical type result; scalar
  independence; raw user-call refusal; original seed.
- Fact owners: member-read permission, canonical type and existing scalar-copy
  owners. A copied String does not promote the borrowed input array's authority.
- Last consumer: ordered member transition's root-read occurrence lookup.
- Forbidden fallback: admitting arbitrary indexed scalar user calls, replacing
  canonical type parsing or weakening blocked-root/field-move obligations.
- Gate/falsifier: existing copied indexed user-call positive and raw-call negative
  remain paired in the scalar gate; execute actual canonical parsing values on
  native C/LLVM, then re-run the original MIR-root admission.
- Exact mapping: `.tmp/inout-array-release-bootstrap-v29-exact-boundary-context.log`;
  seed refusal: `.tmp/self_hosted/codegen_nominal_array_declaration/run.cRjgkR`.
- Native C/LLVM executed the actual canonical parser for generic label removal
  and scalar/base identity after input cleanup, exact marker PASS. Receipt:
  `.tmp/inout-array-release-canonical-scalar.sha256`. This producer value probe
  is now part of the existing member-identity gate; its broader unit matrix has
  not yet rerun on this source. Gate35 then official seed v30 run serially at
  the original budgets, with source frozen; no whole-root/install/CI claim.
- Gate35 passed C/LLVM each 25 analyzer positives/38 compile-only refusals,
  `.tmp/self_hosted/collection-borrowed-descriptor-read.Ftgl32`. Official seed
  v30 is now the active gate. The normal native/driver hashes remain 88526595
  and 707dcd40 respectively; they have not been replaced by isolated native v3.

## Reached collection definition type classification

Official seed v30 passed member-read canonical parsing and refused node 70067
in the ownership verdict's definition-facts call. The reached definition owner
passes inferred_types[row] and inferred_types[binding.row] directly to its user
String-array predicate; those formal element uses remain unproved.

- Objective: copy one inferred type scalar per reached row/binding before its
  predicate uses; keep definition identities, ranges and storage facts unchanged.
- Priority: admitted type/row joins; independent scalar; unchanged String-array
  classification; raw-user-call refusal; original official seed.
- Fact owners: collection assignment definitions and the existing scalar-copy
  and formal-use owners. Type classification does not grant storage provenance.
- Last consumer: the ownership verdict's definition fact construction.
- Forbidden fallback: admitting arbitrary raw indexed user calls, guessed local
  identity, dropped assignment/type readiness or stale definition activation.
- Gate/falsifier: existing copied scalar user-call and raw-call gates remain;
  execute the actual predicate on String-array/non-String-array cases, then
  re-run original MIR-root admission under the existing budget.
- Mapping: `.tmp/inout-array-release-bootstrap-v30-exact-boundary-context.log`;
  refusal: `.tmp/self_hosted/codegen_nominal_array_declaration/run.U1ov5M`.
- Gate36 passed at the unchanged 300s budget: C/LLVM each 25 analyzer positives,
  38 compile-only refusals and executed actual String-array/non-String-array
  predicate values after input cleanup. Evidence:
  `.tmp/self_hosted/collection-borrowed-descriptor-read.QxeCYj`. These scalar
  value checks are not whole definition-facts, installed-driver or CI proof.
  Official seed v31 follows with source frozen under the original 1800s budget.

## Reached call-effect type classification

Official seed v31 passed definition construction and refused node 70212 in
the ownership verdict's call-effect construction. The reached call-effect owner
passes inferred_types[binding.row] raw to the same String-array predicate.
Its other inferred-type consumers forward to the existing copied local identity
query; they do not introduce another raw inferred scalar use.

- Objective: copy only the predicate's inferred type input at the existing
  Concat boundary. Preserve the binding.ok short-circuit and all effect bounds.
- Priority: exact current binding; unchanged type predicate; independent scalar;
  unchanged retention/retirement/escape facts; raw user-call negative; seed.
- Fact owners: call-effect facts and the existing scalar-copy/formal-use owners.
- Last consumer: the ownership verdict's definition-bound negative effect table.
- Forbidden fallback: granting release from a type check, suppressing a negative
  event or reading the type array when binding identity is unresolved.
- Gate/falsifier: retain copied scalar/raw user-call pair and actual predicate
  values on C/LLVM; re-run the same original MIR root and official seed.
- Mapping: `.tmp/inout-array-release-bootstrap-v31-exact-boundary-context.log`;
  refusal: `.tmp/self_hosted/codegen_nominal_array_declaration/run.4dhMSt`.
- Gate37 passed C/LLVM each 25 analyzer positives/38 compile-only refusals and
  actual copied type classification values in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.MOjhQz`. Official seed
  v32 runs next, original budget, source frozen. Installation and CI remain OPEN.

## Reached ownership scan scratch lifetime

Official seed v32 passed call-effect construction and refused node 70449 at
unproved_formal_execution_context. The source wrapper calls the ownership
verdict owner, whose two local scratch-cleanup defers mark the whole callable
opaque under the existing execution-context contract. That contract stays intact.

- Objective: separate ordered event/finalization execution from its caller-owned
  scratch lifetime, preserving every semantic result and synchronous cleanup.
- Priority: exact admitted facts and event order; typed success/failure result;
  cleanup on every normal return; deferred/captured borrow refusal; original seed.
- Fact owners: the existing ownership verdict boundary admits/prepares facts;
  a responsibility-named ownership scan owner executes the same ordered scan.
- Last consumers: the scan consumes indexed-borrow scratch and continuing-
  retention flags. The caller retires both exactly once after the scan returns.
- The scan returns Void and fills an existing typed verdict through inout.
  This keeps scratch inout calls resource-result-free, preserving the existing
  public ArrayDrop no-alias obligation. No one-element out array or global
  error API is introduced. The caller's initial verdict is an explicit error,
  never a guessed successful result.
- Forbidden fallback: relaxed defer/async context admission, copied whole-program
  type arrays, compiler-private retirement of user arrays, duplicate event scans,
  silent success when a result is missing, or an old scan alongside the new one.
- Gate/falsifier: source inventory pins single delegation/explicit cleanup and
  rejects the old scan/defers; C/LLVM success and early-failure cleanup execution;
  existing deferred-borrow negatives; same original MIR-root seed. This split
  is active P0 work, not unrelated cap/SoT cleanup or C substitution progress.
- Mapping: `.tmp/inout-array-release-bootstrap-v32-exact-boundary-context.log`;
  refusal: `.tmp/self_hosted/codegen_nominal_array_declaration/run.6vkeOW`.
- The old scan moved without duplicated execution: its 21 success/error exits
  now fill the typed result. The original entrypoint/signature stays stable;
  it delegates once and explicitly retires both scratch buffers. Full native
  analyzer-import AIR validation passed, 0 errors/9 warnings, in
  `.tmp/inout-array-release-ownership-scan-native-air.log`.
- Gate38 passed at the original 300s budget: C/LLVM each 26 analyzer positives,
  39 compile-only refusals, actual copied type values and native typed success/
  early-failure cleanup execution. A non-capturing local cleanup defer still
  refuses at unproved_formal_execution_context; no context rule was relaxed.
  `.tmp/self_hosted/collection-borrowed-descriptor-read.aT7pYd`.
  Official seed v33 subsequently refused the verdict publication boundary below.

## Reached verdict result-plan publication

Official seed v33 refused node 70281: argument 7 of
SemanticAstCollectionOwnershipVerdictOkWithFacts retained an Array<Int> field
from a readonly result plan. Mapping is in
`.tmp/inout-array-release-bootstrap-v33-exact-boundary-context.log`.

- Objective/priority: keep the admitted plan identity and values, but publish
  independent verdict storage without promoting a readonly descriptor.
- Fact owner: the result plan remains readonly input; the ownership scan owns
  the one publication copy through existing Clone. The canonical verdict
  constructor remains the last consumer and retains its unchanged signature.
- Forbidden fallback: mode-based ownership, raw borrowed member carriage,
  relaxed public release provenance, or a copied whole-program input.
- Falsifier: a cloned member-array verdict stays unchanged when the source is
  mutated; raw member-array handoff to the ordinary publication function still
  refuses, matching the reached production call. Native C/LLVM execution and
  the unchanged original MIR-root seed supply separate evidence.
- The first two test drafts attempted public ArrayDrop on member/extracted
  numeric arrays. Those are outside its named literal/own-formal frontier;
  the test now checks snapshot independence, not unsupported member cleanup.
  This does not change the GUI caller-buffer cleanup gate or original seed.
  A direct nominal numeric-array constructor draft was accepted by the source
  observer (gate41, `.tmp/self_hosted/collection-borrowed-descriptor-read.Io01Lu`).
  It is not the production ordinary-call boundary; direct-constructor borrow
  admission parity was not established and is not claimed by this gate.
  Gate42 passed: C/LLVM each 27 analyzer positives/40 compile-only refusals,
  copied type values, typed success/failure cleanup and actual numeric snapshot
  independence. `.tmp/self_hosted/collection-borrowed-descriptor-read.AMmY44`.
  Official seed v34 runs at the unchanged 1800s budget, source frozen.
  Fresh fixed point, default installation and CI remain OPEN.
- The collection cap block was directly checked after the reached split:
  17 -> 16 over-cap owners, without raising caps. Full inventory remains OPEN.

## Reached MIR binding type publication

Official seed v34 passed the verdict publication boundary and refused node
70720 in SelfMirCollectionOwnershipMemberSourceReady. Its binding-type owner
returned a selected raw String from a readonly local/parameter type inventory.
Mapping: `.tmp/inout-array-release-bootstrap-v34-exact-boundary-context.log`.

- Objective/priority: preserve binding identity and exact type text while
  returning an independent scalar; keep missing/duplicate failure behavior.
- Owner/last consumer: SelfMirCollectionOwnershipBindingType selects the
  inventory row and copies only that scalar; MemberSourceReady consumes it.
  Its canonical field-type check also copies its selected scalar before the
  ordinary type-normalization call.
- Forbidden fallback: granting retention to borrowed scalars, name-based
  binding lookup, inferred success on missing/duplicate rows, whole-array copies.
- Gate/falsifier: native C/LLVM local/parameter values with explicit caller
  copies before input cleanup, missing/duplicate parameter guards, existing
  raw-scalar negatives and the same original whole-MIR-root seed.
- Gate43 passed the analyzer/control checks but its value probe was refused:
  native indexed_string_borrow_owner classifies any ordinary String call result
  as unknown, not independently owned. That fail-closed rule is unchanged.
  Gate44's value probe copies explicitly at its own boundary; it is not proof
  of ordinary String-result lifetime. Whole-root/installation/CI remain OPEN.
- The integration script's syntax-transition pin now follows the moved scan
  owner. The semantic transition and its public admission owner did not change.
  Its body-consumer pin also follows assembly's existing WithFormalEffects
  call; a negative pin prevents reinference through the old wrapper there.
- Gate44 passed C/LLVM each 27 analyzer positives/40 compile-only refusals,
  previous value/cleanup controls, and MIR binding type values with explicit
  caller copies plus missing/duplicate guards.
  `.tmp/self_hosted/collection-borrowed-descriptor-read.IM8nX1`.
  Seed v35 was incomplete after goal continuation: its handle was missing,
  process inventory found no matching bootstrap/codegen process, and the log
  had no semantic verdict/gen2 receipt. No success is inferred. Official seed
  v36 uses the same source/input and unchanged 1800s budget, source frozen.
  Default installation, full integration and CI remain OPEN.

## Reached MIR match fact publication

Official seed v36 passed binding-type lookup, then refused node 71293:
SelfMirCfgAttachLastMatchCase forwards match arrays to AttachCase, whose shallow
pushes retain raw indexed Strings. Exact mapping:
`.tmp/inout-array-release-bootstrap-v36-exact-boundary-context.log`.

- Objective/priority: keep match instruction/range identities and exact text;
  publish independent scalar text; preserve destination ownership limitations;
  validate before mutation; original whole-root admission.
- Owner/last consumer: match_fact_owner owns case attachment and row append;
  CFG composition and MIR JSON/backend consumers receive the same row schema.
- Case arrays and append's source are readonly ref inputs. The owner copies
  each retained String with Concat before its existing shallow push. Existing
  destination columns have unknown/mixed element provenance, so they do not
  gain deep-drop authority. An initial owned-push draft was correctly refused
  by native uniform-element ownership and source aggregate admission; it was
  replaced, not admitted by weaker checks or whole-prefix Clone operations.
- Incoming binding shape/text checks moved before every destination write.
  Invalid input keeps the pre-existing unchanged-row return contract; no new
  success/error API or semantic admission authority was introduced.
- Forbidden fallback: raw borrowed payload publication, owned-push into an
  unproved prefix, deep-release promotion, cloned cumulative tables per case,
  partial mutation before returning the prior record.
- Gate/falsifier: actual attachment text after source cleanup, offset append,
  invalid attachment leaves existing rows unchanged; copied versus raw indexed
  publication source pair; imported match-owner source admission; original MIR
  root. Gate45 passed C/LLVM each 28 analyzer positives/41 compile-only refusals
  and imported match-owner source admission. Actual native C/LLVM attachment
  snapshot, offset append and invalid-case no-mutation checks also passed.
  `.tmp/self_hosted/collection-borrowed-descriptor-read.mNokLy`,
  `.tmp/inout-array-release-borrowed-read-gate45.log`.
  Fresh seed/default installation/full integration/CI remain OPEN. The user
  authorized an all-current-work checkpoint commit/push before those finish,
  including user/other-worker changes, excluding generated `gmon.out`.

## Reached MIR runtime ABI type publication

Official seed v37 passed match publication, then refused node 71641 at
SelfMirExpressionRuntimeAbiLocalType: a selected raw type String escaped the
readonly local-type inventory. Mapping:
`.tmp/inout-array-release-bootstrap-v37-exact-boundary-context.log`.

- Objective/priority: preserve exact last-binding lookup and failure guards;
  publish only an independent selected scalar; original whole-root admission.
- Owner/last consumer: expression_runtime_abi_owner selects the binding type;
  its Slot ABI projection consumes that type through the existing ABI owner.
- Forbidden fallback: raw indexed String publication, guessed missing type,
  copied whole inventories, changing shadowing into duplicate rejection.
- Gate/falsifier: imported owner source admission, native C/LLVM exact last
  binding/device values after input cleanup using explicit caller copies,
  missing and malformed-length guards, existing raw-scalar refusal pair,
  then the same original MIR root. Ordinary String-result lifetime remains
  unproved by native; the value probe deliberately does not claim that grant.
- All-current-work checkpoint 4415adf4 was committed/pushed. The subsequent
  external documentation commit 15e99c0e cancelled its CI. Latest CI failed a
  stale derived-fact symbol; the registry pin now follows the live generic
  fact-row producer and three existing collection local views are registered.
  Narrow authority-edge gate passed 95 authorities/200 derived carriers and
  96th-owner refusal. No authority count/status or semantic permission changed.
- Gate46 passed the C/LLVM source rows and both imported owners, then all
  prior native value controls, but reached its original 300s limit before the
  last ABI execution. Do not call the combined run green. Its completed C
  artifact and an independently compiled LLVM artifact passed the new exact
  value/guard oracle separately on unchanged source. Receipt:
  `.tmp/inout-array-release-runtime-abi-type.sha256`; source gate evidence:
  `.tmp/self_hosted/collection-borrowed-descriptor-read.9GMYnb`.

## Reached routine resource type consumer migration

Official seed v38 passed the expression ABI lookup, then refused node 71741:
the routine ABI consumer duplicated the same raw local-type return. Mapping:
`.tmp/inout-array-release-bootstrap-v38-exact-boundary-context.log`.

- Objective/priority: one exact last-binding type lookup; preserve values and
  malformed/missing guards; delete the borrowed-return duplicate; same MIR root.
- Owner: SelfMirExpressionRuntimeAbiLocalType, already imported by the routine
  ABI owner. Last consumer: both branches of SelfMirRoutineExpressionResourceType.
- Forbidden fallback: the old SelfMirRoutineExpressionRuntimeAbiLocalType,
  a second lookup or new/old dispatch, missing-type defaults, permission grants.
- Gate/falsifier: source admission of the imported routine ABI owner, native
  C/LLVM existing type-value/guard oracle, structural old-symbol refusal and
  original whole-root seed. This is a reached query migration, not a claim of
  a new C-path substitution or a CLOSED registry row.
- Native C/LLVM values and last-binding/missing/malformed guards passed with
  the routine import. Two migrated calls and old-symbol deletion were checked;
  `.tmp/inout-array-release-routine-abi.sha256` identifies source/binaries.
  The full source observer did not return a verdict within 60s, not admission.
  Linux CI 37307402549 independently refused the same 71741 boundary as v38.
- Installed P0 remains BLOCKED on an admitted whole-root body bundle owned by
  SemanticAstBodyTypeBundleFromAnalysis. Last consumer is the gen0 nominal-root
  codegen boundary; exact falsifier is mir_collection_receiver_root.pgy. The
  next original-input seed tests that boundary; no registry row is closed.

## Reached MIR instruction use publication

Official seed v39 passed routine ABI lookup migration, then refused node 71932:
SelfMirRoutineAddInstruction forwards uses to SelfMirCfgAddInstruction, whose
row loop retained raw indexed Strings. Mapping:
`.tmp/inout-array-release-bootstrap-v39-exact-boundary-context.log`.

- Objective/priority: preserve instruction IDs, use order/ranges and block
  counts; retain independent use text; same original MIR root admission.
- Owner/last consumer: SelfMirCfgAddInstruction publishes instruction rows;
  SelfMirRoutineAddInstruction only forwards readonly use input into that owner.
- Forbidden fallback: raw use-element publication, destination deep-drop
  promotion, cloning the whole accumulated instruction table per insertion.
- Gate/falsifier: native C/LLVM exact use values after source-array cleanup,
  append of an empty use row preserves offsets/counts, existing copied/raw-row
  source pair, structural raw-push refusal, original whole-MIR-root seed.
  The installed rung remains BLOCKED at whole-root body admission; this change
  does not close a registry row or count as C-path replacement progress.
- Native C/LLVM exact retained use text after source cleanup and empty-row
  append IDs/offsets/counts passed. Existing copied/raw-row source controls
  passed in both observer backends. Source/binary receipt:
  `.tmp/inout-array-release-instruction-use.sha256`.
  Whole-root seed v40, fresh installation and CI remain OPEN.
- Seed v40's native original-root control rejected two unnamed use-array
  results at the new routine ref boundary. The two simple-statement consumers
  now bind their derived uses once for that call; no array clone, new permission
  or public user syntax was introduced. The original native MIR root is the
  compatibility/identity falsifier before another official seed.
- Original native MIR root C/LLVM compilation/execution passed the unchanged
  six-line identity/mutation oracle after those bindings. Receipts:
  `.tmp/inout-array-release-mir-root-named-uses.sha256`,
  `.tmp/inout-array-release-mir-root-named-uses-llvm.sha256`.
  The nominal gate now exposes the actual native cause at that boundary.
  Seed v41 on the same original root remains the self-host falsifier; no
  installed-driver, full inventory, full integration or CI success is inferred.

## Reached MIR expression LocalRef publication

Official seed v41 passed named instruction-use consumers, then refused node
71966 in SelfMirRoutineAttachLastExpressionGraph: AttachExpr0 retained raw
indexed local-ref text. Mapping:
`.tmp/inout-array-release-bootstrap-v41-exact-boundary-context.log`.

- Objective/priority: preserve exact last-binding/version selection and row
  ranges; store independent LocalRef text; same original MIR root admission.
- Owner/last consumer: local_ref_fact_owner publishes instruction-local-ref
  columns; routine graph attachment and MIR projection consume those columns.
- Forbidden fallback: borrowed local-ref publication, prior-binding fallback
  after a shadowing version, whole-prefix cloning, destination deep-drop grants.
- Gate/falsifier: native C/LLVM shadowing/version selection, source-string
  cleanup survival, malformed input and repeated attachment leave rows unchanged,
  append offsets/counts/primary refs, existing copied/raw-row source controls,
  structural raw-push refusal, then original whole-MIR-root seed.
  Fresh installed P0 remains BLOCKED on the admitted body bundle, with its
  existing owner/last-consumer/falsifier; no SoT status change is inferred.
- The first native value probe refused ArrayDrop of the multiply borrowed
  numeric version scratch. That remains outside this String-publication oracle:
  the probe releases its owned source String arrays, not numeric scratch, and
  makes no numeric cleanup claim. The native release admission rule is unchanged.
- Native C/LLVM shadowing/version selection, source String cleanup survival,
  malformed/repeated no-mutation guards and nonempty-prefix append offsets
  passed. Existing copied/raw-row source controls and the original native MIR
  root's exact C/LLVM six-line oracle also passed. Receipts:
  `.tmp/inout-array-release-local-ref.sha256`,
  `.tmp/inout-array-release-mir-root-local-ref.sha256`.
  Effective owner size is 142 under the unchanged 180 cap. Official seed v42,
  fresh installed-driver proof, full integration/inventory and CI remain OPEN.

## Reached MIR unique-use accumulation

IMPLEMENTATION CANDIDATE, base a59c01a6c13cd8a9f410db435dcf0c95fee67e98.
Official seed v42 passed LocalRef publication and refused node 72305 in
SelfMirExpressionGraphUsesAppend at unproved_formal_shallow_mutation_entry.
Mapping: `.tmp/inout-array-release-bootstrap-v42-exact-boundary-context.log`.

- Objective/priority: retain independent use text, preserve first occurrence
  order and deduplication, then re-run the same original MIR root.
- Owner/last consumer: SelfMirUsesAppendUnique owns insertion; graph-use and
  assignment consumers forward their selected text into that owner.
- Forbidden fallback: retaining raw borrowed String input, guessing ownership
  from inout mode, destination deep-drop promotion, or cloning the whole prefix.
- Edit scope: that existing insertion, focused use-text fixtures, source pins
  and this navigation evidence only. Root integrates; no parallel rung edits.
- Gate/falsifier: copied/raw unique-use source pair, native C/LLVM exact order,
  deduplication and source-cleanup survival plus graph invalid/no-mutation,
  followed by original MIR-root compatibility and official seed. Existing
  static/focused/integration budgets stay unchanged.
- Installed P0 remains BLOCKED at the admitted whole-root body bundle owned by
  SemanticAstBodyTypeBundleFromAnalysis and consumed by gen0's nominal-root
  codegen boundary. No CLOSED or C-path substitution claim is made.
- First candidate (ArrayPush of Concat) passed native values but the focused
  source pair still refused the copied case at the same formal shallow-mutation
  entry. It is not source admission. The owner now uses the existing
  ArrayPushOwnedString insertion contract; fixtures establish their prefix
  through that same owned insertion. This proves new elements, not arbitrary
  pre-existing prefix ownership, and introduces no semantic permission rule.
- That owned-insertion candidate passed the mutation boundary but exposed
  unproved_formal_element_use_entry for raw indexed text forwarded through an
  ordinary String formal. Graph-use and assignment consumers now copy their
  selected scalar before that call, following the existing scalar boundary.
  The copied source fixture models both obligations; the raw negative retains
  neither grant. No scalar-formal effect or collection permission is relaxed.
- Final copied/raw pair passed both C/LLVM observers; raw insertion retains
  the exact formal shallow-mutation diagnosis. Native C/LLVM source-cleanup
  survival, order/deduplication, invalid-graph no-mutation and original MIR-root
  six-line values passed. Structural mutation-graph pins and Bash syntax passed.
  Receipts: `.tmp/inout-array-release-unique-use.sha256` and
  `.tmp/inout-array-release-mir-root-unique-use.sha256`. The combined descriptor
  matrix was extended, not claimed fully rerun. Linux CI 37314795454 separately
  established the same v42 node 72305/boundary; logs are in
  `.tmp/inout-array-release-ci-37314795454-codegen.log`.
  Next falsifier is official seed v43 on the unchanged original MIR control.

## Reached aggregate finalization failure location

IMPLEMENTATION CANDIDATE, base ad4f133a895f01b7663132cb5c5c741cba34cf3c.
Official seed v43 passed unique-use accumulation, then refused the original
MIR control at aggregate_release_incomplete with node=-1.

- Objective/priority: attribute this existing refusal to its exact field,
  carrier, alias or call event before changing any safety condition.
- Fact owner: aggregate release finalization and exclusivity predicates;
  last consumer: ownership scan's existing typed failure verdict.
- Forbidden fallback: guessing a guilty field from the first requirement,
  changing boolean admission, weakening restoration/exclusivity, or adding
  stdout inside production semantic analysis.
- Edit scope: existing plan.diagnostic_syntax_id publication on a failed
  checked event and the focused source diagnostics gate. Root integrates.
- Gate/falsifier: existing 5-positive/12-negative C/LLVM aggregate source
  matrix, including concrete syntax/callable evidence for incomplete plans,
  then exactly one original whole-root observation. Budgets remain unchanged.
- Installed body-bundle rung remains BLOCKED; no new implementation track,
  C substitution, registry status or performance claim is introduced.
- The existing owned-table positive refused its pre-release Ready(facts) call
  as aggregate_release_incomplete, at Main syntax 796. The unchanged older
  observer independently reproduced that refusal; diagnostic publication did
  not introduce it. The full C MIR-root observation returned no verdict before
  its existing 300s limit; this is incomplete, not semantic evidence.
  LLVM observes exactly the same original root next, without changing input,
  safety conditions or the observation budget.

### Shared readonly-call proof objective (implementation candidate)

- Priority: retain exclusive release and alias refusal while allowing a
  synchronous, completely observed non-retaining aggregate read call.
- Owners: formal execution-context owner already knows omitted/deferred
  execution; member-read owner already closes blocked root/forwarding facts.
  The aggregate finalizer consumes those facts, never reconstructs a second
  AST use inventory or grants from a parameter mode alone.
- Proposed carrier boundary: retain the existing context's candidate set and
  artifact generation with its unsafe set; formal effects carry that admitted
  context to finalization. Missing candidate/generation/context fails closed.
- Forbidden fallback: treating absence from an unqualified negative map as
  proof, granting from ref/default mode, name whitelists, guessed callee IDs,
  or relaxing alias, return, opaque/deferred or restoration counterexamples.
- Single integration gate: original MIR-root seed, with existing aggregate
  and member-read source matrices and new readonly alias/opaque falsifiers
  as prerequisite evidence. Root owns all edits; no parallel implementation.
- LLVM observation on unchanged original MIR root returned an exact refusal:
  syntax 50574 in SemanticAstInitializerTypeFactsFromArtifactWithIterationRowsObserved,
  at its WithFunctionTables call before final release. Receipt:
  `.tmp/inout-array-release-aggregate-diagnostic-llvm.sha256`, diagnostic:
  `.tmp/inout-array-release-bootstrap-v43-aggregate-exact-context-llvm.log`.
- Context schema/generation readiness and control predicates now have one
  fact owner; the existing builder alone scans the named callable set. Formal
  effects retain all signature-callable context coverage for readonly loans;
  array preservation retains its exact existing inventory subset. The old
  unused FromFacts wrapper was removed. Empty String-formal inventories do not
  silently bypass context proof for aggregate formals.
- Member forwarding seeds blocked roots from this same context before its
  existing closure. Finalization requires current caller/callee coverage and
  the closed non-retaining formal fact, never ref/default mode alone. Carrier,
  alias, return, reserve and restoration checks remain unchanged.
- C passed all 6 positives/16 negatives in gate4, while LLVM refused direct
  HashMap-member receivers. Descriptor-local snapshots were then refused by
  native ref-boundary admission. Neither constraint was relaxed. Named ref
  row-query owners now consume the same direct map views; generation permission
  stays solely in CallableReady, and root-block lookup is shared by indexed
  reads and aggregate loans. No native backend feature or copied map was added.
- Observed on this implementation candidate: gate6 passed the full C/LLVM
  6-positive/16-falsifier source matrix. Gate7's added generation unit rejects
  disabled context, wrong digest/count, absent candidate coverage, an opaque
  callable and out-of-range callable identities while retaining the original
  context's permission. The combined gate7 reached 300s after all C rows and
  LLVM rows 0..18; with every input/import/native hash unchanged, the remaining
  LLVM rows and guards separately passed and all C/LLVM outputs matched. This
  does not turn that timed-out combined run into a time-budget pass.
  `.tmp/inout-array-release-aggregate-release-gate6.log`,
  `.tmp/self_hosted/aggregate-release-source.iVOx1C`.
- Current member gate8 passed both backends, 5 positives/11 refusals each;
  `.tmp/inout-array-release-member-read-gate8.log` and
  `.tmp/inout-array-release-shared-context-member.sha256`.
- The original whole MIR root now returned body_ok=true/empty diagnostic in
  the fresh LLVM observer within its original 300s limit. Native C/LLVM exact
  six-line receiver identity/invalid graph controls also passed; before/after
  input/import/executable hashes matched. Existing compiler warnings remain.
  `.tmp/inout-array-release-shared-context-whole-root-llvm.log`,
  `.tmp/inout-array-release-shared-context-whole-root.sha256`,
  `.tmp/inout-array-release-shared-context-mir-root-native.sha256`.
  Official seed v44 is next; these observations do not admit an installed
  driver, fixed point, CLOSED family or C-path substitution.
- Reached context/control separation reduced the direct collection cap block
  from 16 to 15 failures without raising limits. New fact/builder/target/member
  owners pass their existing caps; full structural inventory is still OPEN.
  Current authority-edge attempts 3/4 produced no verdict in 60s. Published
  CI 37316971496 failed the prior v43 boundary; Windows/macOS/TSAN/Rocq passed.

### Reached Slice length C projection objective

IMPLEMENTATION CANDIDATE, base df0df7a40ee18d32220238724780926f7613a12c.
Official seed v44 reached code emission and refused the unchanged whole MIR
root with `unsupported collection runtime kind: Slice<String>`; source/import
hashes matched through the terminal failure. No seed or installed receipt.

- Objective/priority: use the existing borrowed Slice ABI for the reached
  ArrayLength consumer, preserving single evaluation and fail-closed admission.
- Fact owner: SliceRuntimeFact and compiler-owned data/length layout; last
  consumer: RewriteSemanticCall's ArrayLength branch. It currently forwards
  Slice types into the array-only runtime owner.
- Forbidden fallback: reinterpreting a Slice as an Array, synthesizing array
  mutation/drop permission, unsupported element defaults, or native bypass.
- Edit scope: the Slice read-length runtime fact/block, its existing direct-
  MIR fact constructor, the Slice length projection and focused fixtures/pins.
  Root alone integrates; no parallel implementation or GUI source edits.
- Gates: current native C/LLVM and fresh self-host C exact Int/String, empty,
  value/ref Slice length behavior; missing/unsupported Slice fact refusal;
  then the original MIR control through official seed v45. Existing static,
  focused and integration budgets remain unchanged.
- Initial three-call-site triage guessed for-each. A real source probe instead
  failed earlier at statement_type_unresolved, and original compiler source
  inspection identified ArrayLength on Slice formals as the reached path.
  The for-each trial and its new fixtures were withdrawn before commit; no
  separate iteration/semantic support track is being opened.
- The eight-row simple length fixture reaches the old emitter's Slice<Int>
  refusal and the changed emitter executes all rows. A stronger compound
  ArrayLength(factory(...).Slice(...)) probe reached a separate missing codegen
  operand type, not a bad Slice length ABI. It is retained as a fail-closed
  negative; no type-text reconstruction or second projection track is added.
- Final focused gate2 passed within the original 300s limit. Fresh native C
  and LLVM and a freshly built self-host C emitter each execute all eight exact
  Int/String/default/ref/empty length rows. Both native fact observers passed
  supported/direct-MIR fact agreement, three unsupported families and three
  missing/unsupported-symbol refusal modes. The compound missing-type negative
  remains refused before any C program publication. Input/import hashes match.
  `.tmp/inout-array-release-slice-length-gate2.log`,
  `.tmp/self_hosted/slice-length-codegen.bwqMT6`.
  The initial positive-with-compound probe failed at a different required type
  fact; it was not counted as a completed gate. Official seed v45 is next.
- Linux CI 37329776930 independently established v44's Slice<String> failure;
  Windows/macOS/TSAN/Rocq passed. Current-head integration/default installation
  remains OPEN; no CLOSED family or C-path substitution claim is made.

## Reached Slice runtime namespace boundary (2026-10-06)

- Observed checkout: main @ 7e054c1ff5da65f1eed9d77f63429225ec1ae285.
  Native v4 SHA-256 c4f4dd1fb3e735f5afa0516a7c9d0485e481f74833febedf0092ef1a8e074f98.
  Gate3 lengths and native public gate16 passed. Official seed v45 passed
  source admission/emission but its original MIR root C failed compilation:
  private Slice definitions collided with pgy_runtime.h's public type/functions,
  which expect a distinct four-field Array rather than the private carrier.
  All 6963 source/input/native hashes matched through failure. CI 37337689083
  independently failed the same root C compile stage; four other jobs passed.
- Objective: compile the original MIR-root emission beside its required runtime
  header without redefining or calling native Array/Slice ABI symbols.
- Priority: semantic/ABI identity, exact source projection, fail-closed admission,
  executable negative ratchet, original integration input and budget.
- Fact owner: existing source SliceRuntimeFact and its private symbol projection.
  Compiler ABI rows and direct-MIR public Slice facts remain unchanged.
- Last consumers: source AbiLayoutCValueType, Slice/index/length expression
  emission, then SliceRuntimeCBlockForFact and the C compiler.
- Forbidden fallback: passing a private Array to native Slice functions,
  redefining native Slice types, casting away descriptor mismatch, removing the
  required runtime include, or accepting missing/unsupported element facts.
- Edit scope: source Slice fact/type spelling and its exact existing ABI consumer;
  a HashMap-header coexistence/get/copy fixture and owned diagnostic publication.
  No runtime/native source change, GUI coupling, new self-host rung or CLOSED row.
- Gates: existing Slice length/fact refusals plus native C/LLVM and fresh self-host
  C header coexistence; then official seed v46 on the unchanged original MIR root.
  Focused 300s and integration 1800s limits remain unchanged.
- Focused gate4 passed within 300s: existing eight length rows and the seven-row
  HashMap-header coexistence/get/copy oracle match for native C/LLVM and fresh
  self-host C. Source fact namespace and unchanged direct-MIR public type are
  checked; missing/unsupported fact refusals remain. Emission is ratcheted against
  redefining native Slice type/function names. No original-root/fixed-point claim.
  `.tmp/inout-array-release-slice-length-gate4.log`,
  `.tmp/self_hosted/slice-length-codegen.WPgus8`.

## Reached generic tuple text publication (2026-10-06)

- Observed checkout: main @ 32d2b1a922d0285b57c0a692e191ec5710263a77.
  Official seed v46 passed the original MIR root's C compile/exact execution,
  four native/self-host controls and eight cycle refusals. Gen0 compiling its
  own source then refused borrow_boundary_escape at syntax 49433, ArrayPush.
  The production parser mapped that handle to GenericInstanceClosureFromRecipes,
  `ArrayPush(actuals, actual)`, where actual is a readonly recipe/substitution
  String. Current source/input/native hashes matched after the terminal failure.
- Objective/priority: publish owned tuple text, preserving template/instance
  identity, ordered deduplication, finite-closure refusal and fail-closed borrowing.
- Fact owner: existing GenericInstanceClosure and its tuple projection/append/
  recipe publication functions. No formal mode or lifetime permission change.
- Last consumers: closure.actual_types and the returned actual tuple, then
  codegen's admitted specialization facts and self-host source admission.
- Forbidden fallback: retaining indexed borrowed strings with ArrayPush,
  allowing readonly input mutation, relaxing borrow escape or native bypass.
- Edit scope: the owner's three String publication sites use ArrayPushOwnedString;
  the append site first copies its existing member text into a fresh owned buffer
  with the same primitive. Native rejects OwnedPush on an unproved member view.
  A SliceCopy trial passed native values but source admission could not prove
  uniform ownership for its result; that trial was withdrawn instead of relaxing
  either guard. Numeric storage, identity and expansion rules remain unchanged.
- Gates: native C/LLVM tuple values after caller cleanup, existing finite/expanding/
  missing/epoch cases, current source owner admission and raw-retention refusal;
  then fresh official seed v47 on the same compiler source/MIR controls and budgets.
- Native owned-copy values include cleanup of the returned actual tuple. Source
  admission of that extra value fixture still refuses its returned-array deep-drop
  at owned_string_drop; the production owner algorithm itself is admitted. Keep
  this separate unproved returned-array grant OPEN and ratchet its refusal rather
  than weakening it or presenting native lifetime evidence as source permission.
- Final focused gate4 passed within 300s on native v4: C/LLVM execute existing
  ordered/forwarding/dedup/finite/expanding/missing/epoch oracles and copied text
  after caller/returned-tuple cleanup. Both source observers admit the production
  owner algorithm and retain the raw formal-element entry and unproved returned-
  array drop refusals. No unsafe input is emitted or executed. Import/input/binary
  hashes are recorded. `.tmp/inout-array-release-generic-owned-actuals-gate4.log`,
  `.tmp/self_hosted/generic-owned-actuals.uRErHr`.
- CI 37340961801 independently passed the original root C controls and then
  refused exactly syntax 49433/ArrayPush in gen1 source admission, matching v46.
  Windows/macOS/TSAN/Rocq passed; current integration/default install remains OPEN.

## Reached readonly wrapper membership boundary (2026-10-06)

- Observed checkout: main @ bebfc16440367b68b52af2e8b6e245b291312151.
  Seed v47 again passed original MIR C controls, then gen1 source admission
  refused syntax 54263 / unproved_formal_indexed_read_entry. The production
  parser mapped it to CodegenValueWrapperUsageCollectType's call to Contains
  after recursive inout growth. All source/input/native hashes matched.
- Objective: identify the exact call-effect fact that removes reading permission
  from the current inout descriptor after synchronous recursive growth. Preserve
  the existing wrapper inventory and its canonical ordering/deduplication.
- Priority: one admitted effect owner, current descriptor lifetime, fail-closed
  consumption/unknown/escape behavior, negative ratchet, then executable replacement.
- Fact owners: existing collection formal-effects, argument permission effects and
  call-effect facts. Last consumer: FormalIndexedReadReady at Contains' call entry,
  then the production wrapper inventory and gen1 source admission.
- Forbidden fallback: mode-only permission, retaining stale sibling views, copying
  the inventory merely for lookup, replacing an absent fact with a guess, weakening
  borrow escape, compiler/runtime ABI changes or a native bypass.
- Edit scope: first inspect the existing formal effect and unknown/escape/retirement
  site maps. Root integrates the reached correction; no parallel implementation
  track or unrelated scalar/type-query migration is active.
- Exact observation: the production Contains formal has mode 0 / effect 3; the
  CollectType formal has mode 1 / effect 6, no unknown or escape site, and retiring
  / non-deep site 40176 in the standalone imported input. The production parser
  identifies that site as the synchronous recursive CollectType call immediately
  before the refused membership read. The existing ArgumentPermissionEffect already
  separates sibling retirement from current descriptor retirement, but the formal
  call-effect carrier discards that distinction.
- Reached correction: carry that existing descriptor_retiring bit through the same
  physical call pass into a formal descriptor-retirement site map. Formal current
  reads and shallow forwarding consume this map; unknown/escape, consumption,
  repeat/defer and sibling-view obligations retain their existing owners. This
  grants neither owned String elements nor deep-drop permission. No new SoT row
  or family is declared CLOSED. The original seed/MIR inputs are unchanged.
- Withdrawn trials: Contains' Slice parameter/current-view callers passed native
  values but source admission refused its index comparison. A common indexed-
  sequence type query did not repair that refusal. A named-scalar Slice trial then
  exposed unproved_formal_element_use_entry at the recursive call. All six source
  files were restored exactly to bebfc164; none of these trials is current code.
- Baseline fixture retains the original Array<String> implementation. Its native
  C/LLVM values were observed, but baseline gate3 timed out in the C source observer
  at 60s before a verdict. This is incomplete evidence, not a semantic failure or
  a passing gate. .tmp/self_hosted/value-wrapper-view.PEvIKR.
- Allowed validation: inspect/compile source-only diagnostic observers without
  emitting or running the supplied unsafe source; focused work remains bounded
  by 300s and official integration by 1800s. Seed v48 has not started. Fresh source
  admission/negative evidence must precede another frozen original-input seed.
- CI 37345034540 for exact bebfc164 passed Windows/macOS/TSAN/Rocq and failed
  gen1 at the same syntax 54263 / unproved_formal_indexed_read_entry. Dependent
  Linux jobs were skipped. Current fixed point/default installation remains OPEN.
- Focused gate4 passed on current native v4 within 300s: production wrapper and
  recursive-read native C/LLVM values, three source positives and twelve refusal
  cases per backend. Fresh LLVM production-owner admission also passed, with
  sibling retirement 40176 and descriptor retirement -1; before/after hashes match.
  .tmp/inout-array-release-current-formal-descriptor-gate4.log,
  .tmp/self_hosted/value-wrapper-view.8K3BIz,
  .tmp/inout-array-release-formal-descriptor-frontier-v2-observe.log.
  Initial cleanup-added recursive fixtures were refused by native/source; those
  release claims were removed from the read-only fixture rather than weakening
  cleanup guards. No recursive release or default installation is claimed.
- Publication/terminal integration: all 17 changes are committed/pushed as
  d2fe7ab6ef5dbfac4589585d6933c669beb7da76. Fresh official v48 used fixpoint-only
  mode within 1800s, passed the unchanged original C controls, then refused gen1
  syntax 55727 / owned_string_drop. The production parser identifies
  CodegenExpressionMemberTypeFromGraph's branch-local [variant_key] deep-drop.
  Source/input/native hashes matched through failure. This read-permission
  correction has passed its focused gate; its larger executable rung remains OPEN.
  .tmp/inout-array-release-codegen-bootstrap-v48.log,
  .tmp/inout-array-release-bootstrap-v48-boundary.log.
- CI 37355012849 independently failed that same new node/boundary; Windows,
  macOS, TSAN and Rocq passed, dependent Linux jobs skipped. The next reached
  inspection belongs to existing owned-String domain/literal-transfer owners;
  do not revive the withdrawn Slice trial or infer heap/exclusive ownership from
  a String return type. No fixed point/default GUI completion is claimed.

### Reached qualified-key query lifetime (2026-10-06)

- Status: implementation candidate at main b0767df2e6a36ea04e30ea385205b0d453cf46c1.
  Root alone edits/integrates this reached seam; no parallel implementation.
- Objective: preserve member-type selection while ending a temporary query
  key's lifetime in the existing type-environment owner. Priorities: same row
  identity/lookup precedence, actual retirement, unchanged fail-closed proof,
  then patch size. Row serialization/index and LookupKindType still own lookup;
  CodegenTypeOwnedQualifiedKey still owns qualified-key construction.
- Last consumer: CodegenExpressionMemberTypeFromGraph asks for a scalar match
  and constructs its escaping key only on success. The type-env query retires
  its temporary key before that decision returns. No guessed user-function
  non-retention, conditional-move grant, copied result leak, or native fallback.
- Independent edit scope: type_env.pgy query resource lifetime and its one
  reached expression consumer; ownership/domain/transfer owners are unchanged.
  The earlier full-root diagnostic timed out at 180s with no verdict. This
  supplies no guard result or passing evidence.
- Allowed gates: 60s static, 300s focused C/LLVM current-source values and
  preserved owned-String refusals, then 1800s original-input official bootstrap.
  Integration belongs to Root. Fixtures falsify qualified identity, missing
  row/kind, mismatched enum owner, and global/preseal/local precedence. Only
  observed terminal gate results may promote this candidate.
- Observed focused gate2 PASS within 300s: native C/LLVM actual member-selection
  and row-precedence values, actual source query-lifetime admission, eight
  preserved ownership refusals per backend. Input/import/native/binary hashes
  match. Shell syntax and diff checks pass. Value builds retain 13 warnings,
  observer builds nine. .tmp/inout-array-release-qualified-kind-match-gate2.log,
  .tmp/self_hosted/qualified-kind-match.difPMY. Original-input bootstrap and
  exact-head CI have not run on this slice yet; no installed/closure claim.
