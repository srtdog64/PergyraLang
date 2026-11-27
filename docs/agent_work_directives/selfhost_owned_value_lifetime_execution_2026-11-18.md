# MIR owned-value lifetime: S1/S2 execution boundary

Status: investigation and implementation plan; current results belong to
[blocker](../truth/state/blocker.md) and [gates](../truth/state/gates.md).
Base: `6ff288d70013898a21e867e663f97eaf07e642f8`. Integration: GPT root.
This continues the [memory plan](selfhost_compile_memory_plan_2026-11-17.md),
not another implementation track. One edit lane, no delegated edits.
At published `d21aa036d`, a scratch [fixed-input census](../audits/selfhost_full_mir_independent_refusal_census_2026-11-18.md)
reached all 9,623 route instances and found 109 distinct first refusals,
including 35 collection-transition failures. It did not emit a program or
expose later failures masked by an earlier refusal. The next diagnostic
isolates the 28 owned-flow failures before choosing a production change.
Seven representative guard-5 cases independently fail the same owned-flow
terminal check: a row marked `retired` is still live at an exit. For the
reached value-result parameter field-carrier path, the MIR member-rebind owner
is the fact owner and `DirectMirOwnedCollectionBlockOutput` is the last
consumer. The current restore query resolves a record source only as a local
declaration; the source row in `SemanticAstCollectionExclusiveAddDep` is the
`inout into` parameter. Reuse the already typed value-result parameter
identity query, require exactly one declaration/parameter match, and retain
the exact field and value-local checks. Do not accept by source spelling or
by the row's `retired` label alone. A positive restored inout field and a
forged source/field negative on C and LLVM are the focused falsifiers. The
existing nested-return gate also reaches a local that is explicitly deep
dropped and then restored as an empty descriptor. The full fixed MIR reaches
a release function that drops three fields before restoring any. In the same
block, the last use or definition of that local before the restore must be a
verified deep drop; any intervening local use or definition blocks this case.
A generic already-retired exception could restore storage that a callee now
owns. Rerun both focused paths, the fixed MIR and published CI. This repair
does not by itself establish all 28 guard-5 cases or S1/S2.
## Active return and call transfer (2026-11-28)

Base `e6097ba08096d3ab1f38ea499b373c9149b8fde2`. The fixed MIR has 42
`call-result` rows. The two with `retired` element class each join one call
definition and one bare `Array<String>` return in
`SemanticAstSignatureTypeTextShape`. The v11 first refusal was ordinal 1438.
The small branch return fixture reproduces the transition refusal on the
later installed driver on C and LLVM. Slot work is deferred by the user.
See the [return transfer run](../truth/evidence/2026-11-28_direct_mir_owned_result_return.md)
for installed-driver results and the newly reached callable boundary.

- The semantic result plan already mints a `call-result` origin and callee
  syntax ID in the collection row. `SelfMirCollectionOwnershipFactRows`
  publishes it. The common reader validates each claimed callee through
  `MirCollectionOwnedResultSourceReady`, including its grounded returns.
  A routine that only forwards call results omitted declaration LocalRefs.
  The reader cannot join its returned SSA use to the proof row, and the
  caller's routine index fails. Use the existing shared wire-requirement
  policy for retired call results so both the writer and reader preserve
  the identity. The two retired call results in the fixed MIR already
  have these refs. A class or return type alone grants no ownership.
- An owned String push into a live call result was missing its receipt.
  Issue it at the existing semantic call-completion transition and project
  it onto that instruction even when a later return retires the binding.
  The transition consumer still requires the receipt on the exact receiver.
- At the typed definition, `DirectMirOwnedCollectionDefinitionReady` still
  joins the exact local and direct-call instance to that callee. For a
  retired call-result claiming a bare return, reuse the existing return
  transfer proof to recover the exact consuming block. Require the allocation
  live at that exit and apply the transfer in the CFG flow before checking
  terminal states. No birth, an earlier move/drop or an uncovered exit fails.
  The checked transition reaches both Array cleanup emitters. The callee
  releases nothing at that exit and the caller can retire its own result.
- Falsifiers: extend the installed-driver bare-return gate to a call result
  grown and returned from one branch and a separate return from the other.
  Compare C/LLVM output under ASan/LSan and refuse missing or inconsistent
  result evidence, a wrong callable identity or a missing push receipt
  without producing an artifact. Preserve the existing empty-literal, record
  transfer, hand-off and member cleanup gates. Retry the fixed semantic input
  and the full bootstrap on the candidate.

## Next DRV-2 dependency: borrowed record arrays

The callable-envelope diagnostic visited all 8,528 instances in the freshly
produced full MIR. Its only refusal is the readonly record-array formal in
the return transfer run. The diagnostic stops before graph admission.
The [readonly-carriage run](../truth/evidence/2026-11-28_record_array_readonly_carriage.md)
records the focused execution results and the remaining lifetime boundary.

- Use the admitted nominal array declaration and signature carriage as the
  fact owners. The record array has a compiler-owned layout and no public
  physical layout ID. It satisfies the shared readonly array role beside the
  existing Int/Bool/String array roles. No role-method name exceptions.
- Carry the same admitted type through the call-carriage checker and the
  C and LLVM parameter projections. Borrow an addressable descriptor and
  read the already projected nominal layout. Reborrowing a readonly formal
  forwards its pointer. A by-value formal uses an entry-frame descriptor
  slot when forwarded by address. Writes and copy-out still require the
  existing mutable carriage. The last consumers are the routine signature,
  call argument and parameter load on both backends.
- Extend the record-array gate with executed local, forwarded and by-value
  descriptor borrows on C and LLVM. Refuse a bad layout or mutable use with
  no output. Keep existing scalar-array and by-value record-array gates.
  Retry the full input without using the MIR-to-AST route as a fallback.
- The [fixed-input refusal scan](../truth/evidence/2026-11-28_drv2_fixed_mir_refusal_scan.md)
  records the diagnostic candidates and the isolation limit. Verify the
  next reached owner with ordinary admission on the current publication.

## Array field copy selected by a later mutation

See the [installed-driver run](../truth/evidence/2026-11-28_collection_clone_definition_ownership.md).

At the next collection check, the semantic copy selector lands a Clone on
the initializer of `wrapper_target_names`. The ownership verdict still
publishes a `member-move` from the old field. The typed definition consumer
sees an array Clone and refuses the conflicting birth. A nested field copy
followed by `ArraySet` and a readonly observation of the original record
reproduces it. The native oracle prints `original` and then `changed`.

- Finish the structural copy selection before the value-definition and
  element-lifetime scan. Keep the existing copy-site and later-observation
  checks as the selector. The ordered mutation proof still reads its actual
  definition and call effects. Do not issue a second copy from the emitter.
- Activate the selected member initializer as fresh owned storage at its
  existing definition identity. A Clone reads its source field without
  retiring it. Its element ownership and source identity travel through the
  same ownership verdict as explicit Clone. Actual member moves retain their
  path and retirement checks. An earlier unresolved move still blocks a copy
  from that same field.
- Reuse the owned String store receipt on the copied receiver. Select a
  String copy for a borrowed store value at that exact expression. Read the
  existing element-loan facts before any replacement may free an old value.
  C and LLVM must consume the same checked operation and retire the copied
  storage through their normal cleanup path.
- The original and copy must diverge after a set on both targets. Remove
  the initializer Clone, change its receiver identity, or omit the owned-set
  receipt and require refusal without emission. Keep member restoration and
  element-loan checks. Re-run source admission, bootstrap, and the unchanged
  full-input bounds before advancing the rung.

## Objective card

- Objective: make ownership ends explicit in MIR and consumed by both
  general emitters, then compile the complete fixed DRV-2 in one process
  within the existing boundary. A smaller probe only falsifies a local claim.
- Priority: no premature/double free; stable value/storage identity; one
  owner; fail-closed checking; C/LLVM behavior; fixed point; measured bound.
- Fact owner: MIR value-lifetime plan, under the existing ownership family.
  `semantic.hashmap_collection_ownership` remains ACTIVE until the reached
  consumers, missing-fact behavior and forbidden paths are actually closed.
- Last consumers: C/LLVM value/edge emission and runtime retirement after the
  admitted last use; emitters may not invent provenance or a release right.
- Forbidden: runtime ownership flags, GC/RC, raw-address guesses, a larger
  budget, a reduced canonical input, new per-program-shape routes, source
  manual drops or a copy that bypasses an unproved ownership boundary.
- Shared integration gate: full codegen/driver fixed point, installed C/LLVM
  lifetime and forged-retire tests at `-O0`, full fixed memory inputs, then
  required executed CI on the same published source generation.

## Current reached chain and migration map

1. `semantic/ast_body_collection_ownership_stage_owner` owns source ownership
   verdicts and selected Clone overlays; `mir/artifact_lower_owner` attaches
   them to `SelfMirProgramFacts`. Builtin result origins derive from the
   existing `call_handoff_rule.def`, not a new emitter table.
2. `mir/collection_ownership_fact_owner`, `call_handoff_row_owner` and
   `collection_ownership_receipt_projection_owner` carry existing identity
   and transfer facts. Extend their value vocabulary without making handoff
   notes, expression text or source spelling an alternate authority.
3. `instruction_json_artifact_writer_owner` and its generated memory
   projection publish the facts. Common `mir_lower` readers validate identity
   before either target consumes them. No fact means unproved/no retirement;
   a supplied release with a missing or inconsistent premise is refused.
4. `DirectMirScalarCfgProgramGraphPlanAdmission` folds routine admission;
   `CollectionOwnershipTransitionAppendRoutine` currently joins receipts,
   locals, CFG effects and terminal states. Its whole reached failure set
   must be understood before replacing that contract, rather than adding
   another exception for the first rejected routine.
5. `owned_local_lifetime_owner`, `owned_storage_result_recipe_owner`,
   `owned_expression_temporary_owner` and `array_string_cleanup_policy_owner`
   still make legacy decisions. Move the reached decisions and consumers
   behind the MIR plan; remove their old verdicts/wrappers only with the new
   consumption and matching behavioral falsifiers. The String seed bridge
   does not establish this migration.

## Required order and unresolved questions

1. Repair the reached identity seam first: the collection binding join still
   selects a GraphPlan local by its source spelling. Its existing local-ref
   producer already distinguishes shadowed declarations; normalize the
   ownership-bearing no-wire case there too, then consume only declaration
   identity in the join. All binding-move, definition, block and member-restore
   callers share that owner. No name fallback or new owner is permitted.
   Preserve source-name agreement as a check, never the selection criterion.
   The normalizer's two internal `ref` array parameters borrow the admitted
   declaration inventory through this join; they add no user annotation or
   copy. Remove that explicit carriage only when readonly ABI inference can
   preserve the same inventory identity and lifetime.
2. Trace instruction/value identity through actual lowering, including a
   source instruction that produces zero or several operations. Retain exact
   operation-to-receipt alignment rather than reconstructing it by spelling.
   The existing return-transfer endpoint must compose through typed record
   fields too: semantic named-input reservation can consume an inner field's
   local responsibility, with the caller later retiring that field. Its MIR
   consumer currently proves only a direct field of the returned constructor.
   Keep the existing definition, unique-use, strict-evaluation and all-exit
   checks; replace that shallow path lookup with one typed constructor-DAG
   query in the same endpoint owner. Neither the semantic `retired` label nor
   a constructor occurrence alone grants a release. C/LLVM consume the same
   checked transition; no emitter exception or new owner is needed. The
   coverage input digest keeps its identity and moves to the existing coverage
   issuer; the final plan imports that issuer for the same replay. The typed
   query's two readonly parameters borrow admitted inventories for that query,
   rather than copying storage or adding user annotations. Falsifiers:
   nested field transfer followed by actual caller cleanup; duplicate field
   use, hidden call retention, wrong field type and an uncovered exit. This
   endpoint repair is a prerequisite, not migration of the legacy release
   policy or completion of S1/S2.
   The next reached prerequisite is the array-transform signature/ABI seam.
   The full fixed MIR has two builtin reverse calls, on local Array<Int> and
   Array<String>; the latter refuses. The existing runtime already exports
   typed String and Bool reversals. Keep expression identity 136, select its
   concrete ABI from the admitted argument type in the existing transform
   owner, and migrate signature, readiness, C helper emission, LLVM body and
   foreign-declaration consumers together. The ABI row owner issues one typed
   row input; no provisional Int ABI may stand in for an unknown argument.
   Preserve the existing proven-local receiver boundary and allocation/element
   ownership. Unknown types and forged ABI identities fail closed; no fresh
   allocation, Clone, new route or release authority follows from reversal.
   Move the manifest-only sample readiness check to its sole existing manifest
   consumer. The existing array-transform component owns its typed C/LLVM
   projections from explicit admitted facts; orchestration only consumes
   them. Keep caps and owner count unchanged. Acceptance: Int
   controls plus String/Bool even/odd/empty/single-element and owned-element
   cleanup on C/LLVM; forged identities refuse without artifacts; then retry
   the same full MIR and installed-driver/memory/CI chain.
   The executable family test also reaches the existing String storage-sharing
   consumer: a checked transform returns the same descriptor, not a new holder.
   Propagate that alias through the expression DAG at this existing consumer;
   its eventual binding, constructor, call or return still decides retention.
   A discarded/observed result does not retain it. Keep returns through this
   new alias conservatively shared until a return-transfer owner proves them,
   and keep the existing sharing guard, preservation proof and release rights.
   Falsifiers include retaining the transform result in another local, record,
   callee or return; the family is not a global readonly receiver exemption.
   The added internal `ref` parameters borrow the existing admitted ABI and
   expression inventories for this projection; they add no annotation or
   Clone to the canonical input. Replace that explicit carriage only when
   readonly ABI inference preserves the same inventory identity and lifetime.
   The existing owned-local lifetime owner recognizes a checked transform
   only as a nonretaining root observation when its result is discarded.
   It does not propagate an origin through a retaining constructor: that
   would grant cleanup before the existing recipe proves transfer. A binding,
   call or return remains conservatively blocked until that proof exists.
   The first full family heap probe found 28 bytes in five Int/Bool allocations, including the pre-existing
   Int alias control. Do not call that full probe leak-free or hide that
   control; check isolated new typed lifetimes as well as the unchanged
   conservative alias behavior. No release right follows from the transform
   alone; the original literal/recipe and actual retaining uses still decide.
   The next reached return-transfer edge is a bare `Array<String>` result
   after owned element pushes (`SemanticAstSignatureTypeTextChildren` on the
   fixed input). The semantic row marks its empty-literal binding retired,
   while the existing transfer checker proves only a returned record field.
   Extend that same checker to accept a direct local result under its exact
   definition, unique last use, eager evaluation and all-exit obligations;
   do not treat the `retired` label itself as release authority. The caller's
   returned carrier remains the last cleanup owner. A small source/MIR
   witness that pushes one owned String and returns its Array currently
   refuses before either backend emits; a duplicate return use, a hidden
   retaining call, a wrong return type or an uncovered exit must still refuse.
   This is one S1 prerequisite, not the full retired-claim migration.
   The first returned-array execution also exposed a separate S1 lifetime
   edge: `ArrayPushOwnedString` copies a fresh `Concat` result and leaves the
   source allocation behind (3 bytes under LSan on both backends). Extend the
   existing expression-edge retirement owner only for its uniquely consumed,
   fresh String right operand; C must evaluate it once before the copying
   push and free it after, while LLVM frees the same admitted value after its
   call. A literal or borrowed/local operand grants no free. Verify the
   returned array's element cleanup and the temporary separately, including
   a borrowed literal control and an omitted-retire mutation under -O0 LSan.
3. Fix owned-value identity, origin, recipe, disposition and end points in
   the MIR owner. Use existing SSA/local-ref and CFG facts. Include definition
   reset on loop backedges, aliases and mutually exclusive definitions.
   The expression admission currently resolves `operand.value_row` but
   persists only `node_local_rows`. Before SSA lifetime synthesis, preserve
   that admitted value identity; do not rediscover a version from a source
   name or assume a mutable local is one value. The falsifier is an old alias
   read after its source receives a new String definition.
4. Issue and check String lifetimes: a born owned value ends exactly once
   on every path; no use follows an end; borrowed/static/unproved values
   grant no retirement. Record the value, path and violated rule on failure.
5. Carry/check retire instructions and consume them on both C and LLVM;
   migrate existing String policies. Extend the same plan to array storage
   and elements in S2; do not repair a disposable parallel cleanup policy.
6. Before mixed-origin normalization or S5c, demonstrate executable Clone
   transport and independent copies. A separate ledger alone is insufficient
   ([bounded reconciliation](../audits/selfhost_clone_carrier_reconciliation_2026-11-18.md)).
7. Keep the backend sharing guard until `alias_read_after` moves or refuses
   before execution. Include borrowed descendants, hand-offs, redefinitions,
   early return and loop-born storage in the full integration boundary.

Read/source inspection and isolated diagnostic builds are allowed. Static
gates: 60 s; focused parity: 5 min; integration shard: 30 min. One heavy
memory measurement at a time; existing 14 GiB/1,800 s process bounds stay.
Scratch diagnostics never emit or execute a program whose admission they
weakened, never modify the shared native checker, and are not release proof.
