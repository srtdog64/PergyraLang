# MIR owned-value lifetime: S1/S2 execution boundary

Status: investigation and implementation plan; current results belong to
[blocker](../truth/state/blocker.md) and [gates](../truth/state/gates.md).
Base: `6ff288d70013898a21e867e663f97eaf07e642f8`. Integration: GPT root.
This continues the [memory plan](selfhost_compile_memory_plan_2026-11-17.md),
not another implementation track. One edit lane, no delegated edits.

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
