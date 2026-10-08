# Ownership cleanup / DX architecture recheck — 2026-10-08

Status: `DESIGN REVIEW; IMPLEMENTATION HOLD`. Automatic ownership cleanup,
not tracing GC, is the user-selected direction. The algorithm below is a
proposal mapped to current source, not an implemented feature or new fact owner.
Base: `main @ 3658548d24bca3d721e4f1974ac7a10da99f7aa8`, preserved dirty tree
(476 status entries at review entry). No rollback, install, commit/push, or GUI
readiness message accompanies this review.

## Objective card

Ordinary value programming must not manually enact the compiler's lifetime
proof. Derive ownership, scoped borrowing and cleanup from admitted source
facts; synthesize and verify the necessary operations once in the middle end.
Priority: semantic identity/safety, authority/resource/lifecycle boundaries,
one fact owner, ordinary authoring cost, then implementation convenience/cost.
Fact sources are existing binding/place, storage authority, generation,
formal-effect and CFG owners. The last consumer is the actual call/resource
lifetime and its defined exits, not an arbitrary manually introduced local.
Forbidden: granting from own/ref, field/root names or heap domain; hidden clone,
GC/RC fallback; backend-invented ownership; disabling existing alias/refusal
checks; or making the self-host compiler's functional style language canon.

## Current source and executable findings

- `docs/mut_borrow_parameters.md` selects value-result inout inspired by Swift.
  This does not prove general Swift copy-on-write is implemented: aggregate /
  collection copy-out and physical exclusivity (C2) remain open.
- Native `type_checker_helpers_late.c` rejects non-identifier inout actuals and
  tells users to extract/restore a local. Its comment records an earlier C lost
  update and LLVM invalid IR. Self-host `ast_inout_argument_alias_verdict_owner`
  and `codegen/emission/expr_semantic_call_argument_owner` have the same direct
  binding restriction. Removing one guard is not field-path support.
- Native `type_checker_ownership_call.c` requires a named boundary for the
  tested ref aggregate temporary. Self-host call rendering has only a bounded
  direct String temporary exception, not general aggregate lifetime synthesis.
- `SelfMirCfgAttachLastDestructure` and `SelfMirAppendCfg` take own parents,
  extract nested fields and restore them manually. Field inout could remove
  that source protocol after migration; it cannot alone prove storage
  independence, element ownership or cleanup.
- The uncommitted Owned Functional Updates section had been placed under an
  accepted-contract heading. That generalized a compiler workaround without
  resolving the ownership-changing field/cleanup design. It is now on hold;
  legitimate consuming own transfers are not erased.
- The preceding selected-index String-copy candidate passed a narrow frontier
  but not the full codegen root/seed/DRV-2. It is an unlanded candidate, not a
  substitute for missing scoped-view evidence. No further annotations or copies
  are added in this review. Existing safety fixes and negative cases are retained.

Installed native comparator `bin/pgy.exe` SHA-256:
`f6559da94876c93a2e7303866429ef31ef284b1f29ca3d68f67ff5e710cbc1c9`.
Installed driver is unchanged, SHA-256:
`707dcd40049a1697a5827b2a7c8d3cf509573aa3c0031f2eee338c9fa0d78ec7`.
Scratch sources/logs: `.tmp/architecture_dx_recheck_2026-10-08/`.

| Fixed source | Actual native C build | Actual native LLVM build |
| --- | --- | --- |
| Named `Push(items)` | PASS | PASS |
| Field `Push(holder.items)` | semantic refusal | semantic refusal |
| Named `Count(items)` with ref Array<Int> | PASS | PASS |
| Temporary `Count(MakeItems())` with ref Array<Int> | semantic refusal | semantic refusal |

These use real backend compilation, not only --emit-c. No negative program is
run. This is installed-native evidence, not public self-host/automatic-cleanup
or whole-chain proof. The preceding full codegen diagnostic file is empty and
has no final PASS receipt; it is not a complete remaining-failure census.

Current lexical inventory strips strings and comments, then counts
`\b(own|inout|ref)\s+[A-Za-z_]\w*\s*:` in all src/self_hosted .pgy files:

| Same counting boundary | own | inout | ref | files |
| --- | ---: | ---: | ---: | ---: |
| Committed HEAD | 98 | 814 | 5888 | 2531 |
| Preserved working source, including untracked | 194 | 860 | 6214 | 2566 |

This excludes implicitly typed mode receivers and is not an AST-complete count.
The user's historical table was not regenerated and must not be silently mixed
with this boundary. The working delta is +96 own, +46 inout and +326 ref; a raw
count does not classify necessity. A later ratchet needs stable AST ownership,
added/deleted sites and ordinary-program ceremony as well as totals/density.
No counter gate or raised baseline is implemented by this review.

## Algorithms: primary sources and selection

| Algorithm | Useful invariant | Boundary for Pergyra |
| --- | --- | --- |
| [Swift Ownership SSA](https://github.com/swiftlang/swift/blob/main/docs/SIL/Ownership.md) | Owned IR values have one lifetime-ending use on each reachable path; borrowed values depend on a live base through end-borrow | Adopt explicit internal def/use and scoped-loan evidence, not Swift ARC or source ownership annotations. SIL ownership alone does not establish exclusive physical backing. |
| [Rust drop elaboration](https://rustc-dev-guide.rust-lang.org/mir/drop-elaboration.html) and [RFC 320](https://rust-lang.github.io/rfcs/0320-nonzeroing-dynamic-drop.html) | Initialization/move dataflow turns drop obligations into unconditional, absent, conditional or per-field cleanup | Adapt CFG obligation synthesis, including partial initialization. This is not adoption of Rust's lifetime surface or entire borrow checker; it assumes ownership was proved first. |
| [Futhark alias/consumption analysis](https://futhark-lang.org/blog/2026-09-22-aliasing.html) | Track aliases and consuming vs observing calls separately from ordinary value types | Inform callee effect/alias summaries; do not import consuming annotations or assume all results are fresh. Its pure-array restrictions are not Pergyra's full domain model. |
| [Perceus](https://www.microsoft.com/en-us/research/publication/perceus-garbage-free-reference-counting-with-reuse-2/) | Compiler inserts precise reference counts and can reuse storage | Not the selected base: garbage-free is not count-free. Runtime RC/reuse and its sharing/cycle policy would be a different decision. |

Recommendation (our adaptation, not a published turnkey Pergyra algorithm):
**admitted ownership/loan facts + internal ownership def/use + CFG cleanup
elaboration**. It needs no tracing collector or universal reference count.
Stack-local cleanup flags for branch-dependent ownership are bookkeeping, not
GC; flags decide whether an already-proved obligation exists, never exclusivity.

These are separate problems, not an automatic proof shortcut. Ownership SSA
represents and verifies admitted ownership; drop elaboration synthesizes its
cleanup. Neither discovers unique physical storage from arbitrary aliases.
The inference front end must first consume source-bound allocation, alias,
retention and transfer facts. Implicit borrowing must preserve value-default
behavior; it cannot silently change an observable copy into shared mutation.

## Concrete ownership-cleanup algorithm proposal

1. **Fix physical identity before cleanup.** Associate each admitted allocation
   generation with its actual owner and typed field/element policy. Reuse
   existing storage-producer and generation facts. A heap result, fresh binding,
   nominal constructor or own mode is not an ownership certificate. Borrowed,
   static and transferred storage have different obligations.
2. **Derive call summaries.** On the admitted call graph, determine read-only
   use, mutation, consumption, return aliasing, out-store and deferred/async
   retention. Solve recursive dependencies as a finite fixed point. Unknown
   effects remain unknown, especially at extern/worker boundaries. This is the
   basis for ordinary implicit physical borrowing, not a name whitelist.
3. **Represent loans and transfers internally.** Every field view carries its
   base storage generation, projection and valid scope. Borrow begin/end are
   compiler facts, not user lifetime syntax. Transfer/return hands an obligation
   to the receiving owner; a view does not gain its base's cleanup permission.
4. **Generate obligations.** Successful initialization of owned storage creates
   a typed cleanup obligation. A move/return transfers it; explicit early
   release discharges it. Rebinding cleans the old generation only after the
   new RHS and old-generation consumers obey the language's evaluation contract.
5. **Propagate over CFG.** For each actual place/generation, compute may-live
   and must-live obligations plus pending dependent loans. At a join, union
   records possible obligations and intersection records unconditional ones.
   Loops require a fixed point; do not unfold iterations or lose moved branches.
   Storage aliases must already be resolved; two binding names are not two
   allocations and two fields are not automatically disjoint.
6. **Place cleanup at defined exits.** Lexical routine/block or declared
   resource boundaries own deterministic cleanup timing. End the relevant
   loans before cleaning their base; preserve existing defer/compensation and
   value-result copy-out order. Do not eagerly destroy observable resources
   just because a last textual read was seen. Fatal termination/unwinding and
   task cancellation use their actual contracts, not invented normal exits.
7. **Lower each obligation once.** Always live: unconditional cleanup. Never
   live: no cleanup. Branch-dependent: an internal flag, edge-specific cleanup
   or an equivalent admitted representation. Partial aggregates: clean only
   initialized owned components under their element/variant policy. A returned
   value or explicitly released generation is excluded. Array backing ownership
   does not imply every String pointer in it is independently owned.
8. **Verify the synthesized plan.** On every reachable contract-defined exit,
   each owned generation is transferred or cleaned exactly once, every loan
   ends before its base cleanup, and no use/escape occurs after cleanup. Keep
   explicit-drop and synthesized-drop accounting together. C/LLVM consume the
   same versioned MIR facts; they do not rediscover cleanup from source text.

Conceptual rule: `init -> local obligation`, `borrow -> dependent lifetime`,
`move/return -> obligation transfer`, `release -> obligation discharge`.
Automatic cleanup inserts the last operation only for residual *proved* local
obligations. An escaping view cannot be repaired by simply inserting free.

## Whole-chain map and dependency plan

| Reached family | Existing source anchors | Required change, not done here |
| --- | --- | --- |
| Source place / call identity | ast_expression_place_fact_owner; ast_signature_fact_owner; ast_inout_argument_alias_verdict_owner; native type_checker_ownership_call | Admit stable typed writable field paths and call-scoped temporaries from real identity/effects, not spelling. |
| Allocation / generation | ast_collection_definition_storage_authority_owner; ast_collection_definition_query_owner; ast_collection_aggregate_generation_definition_fact_owner | Infer and carry exact cleanup authority, physical aliases and initialized components. |
| Borrow / retention | ast_collection_formal_effect_fact_owner; ast_text_formal_borrow_owner; ast_collection_aggregate_field_view_generation_owner | Produce source-bound loan ends and retention summaries; distinguish a scoped local view from an escaping value. |
| Release obligations | ast_collection_aggregate_release_plan_owner; preservation/transfer owners; array_storage_element_lifetime_owner | Extend existing verified lifetime meaning to synthesized obligations, not a second release policy. Keep manual-release guards through migration. |
| Exits / projection | mir/routine_defer_owner; routine_control_transfer_owner; intent_resource_lifetime_owner; native MIR parameter bindings; self-host call argument owner | Carry one agreed exit plan and consume it in real C/LLVM paths. Existing intent cleanup does not imply ordinary aggregate cleanup. |
| Actual consumers | MIR cfg/build/destructure chain; codegen type-env state/snapshots and statement/query callers | Migrate complete consumers to the admitted plan; delete replaced manual transport/release paths only after evidence. |

Implementation order: first enumerate all reached failures on the fixed
production input (scratch diagnostics if needed); resolve C2/physical sharing
and row-table lifetime unknowns; fix one objective/owner/call-and-exit contract;
produce source facts and MIR cleanup together; migrate all reached consumers;
retain negative gates; then fresh seed, actual DRV-2, installed/default routes
and exact-SHA CI. No new implementation or parallel research track is opened
by this memo, and a passed most-recent fixture is not closure.

Field inout is a preferred companion design, not implemented or independently
approved by the cleanup choice. The compiler must evaluate its place once,
prove every actual/receiver's physical alias relation, and synthesize normal
copy-out. Indexed/effectful paths need additional stability/bounds decisions.
Own consuming transfer and inout update remain semantically different.

## Fit limits and falsifiers

WO-REG-1 covers certified transient String allocations, not generic Array
backing or recursive aggregate finalization. Region reuse for row tables is
unproven. Unrestricted shared mutable/cyclic raw ownership cannot be reclaimed
by wishing it unique: define a supported ownership/resource graph or an explicit
sharing contract; no silent GC, RC or copying fallback is proposed.

Memory safety is a correctness prerequisite, not the chosen market wedge.
docs/198 is proposed strategy input, not semantic authority. Its market ranking
and a particular safety blocker are different decisions. Reducing ceremony
must not admit an actual dangling view or weaken authority/lifecycle contracts.

Falsifiers: duplicate/overlapping path; root plus child actual; distinct roots
sharing backing; receiver alias; index evaluation duplicated or invalidated by
growth; borrowed temporary returned/out-stored/deferred/async/extern-retained;
normal early return, branch-only move and loop exit; partial initialization;
explicit-drop plus synthesized-drop double cleanup; stale/missing generation,
effect or exit facts. Retain the two previously admitted String factory/out-store
counterexamples; ownership-negative programs stay analyze-only.

Outstanding: no complete fixed-input refusal census, automatic cleanup producer,
approved field-path implementation plan, inferred-borrow migration, or ceremony
ratchet exists yet. Exact analyzer deletions and public shared-value behavior
depend on those facts. This memo selects and maps a plausible algorithm; it
does not prove general ownership inference or claim implementation/CI closure.

## Observed review verification

- `tests/language_contract_golden_smoke.sh`: PASS (exit 0).
- `tests/self_host_hard_contract_smoke.sh`: PASS (exit 0). This verifies contract
  wiring/source structure, not automatic cleanup or completed substitution.
- Installed-native C/LLVM probe builds: the two positive controls pass and the
  two unsupported actual forms refuse, as recorded above. No runtime cleanup
  or public self-host result is inferred from these compilations.
- Local Markdown link targets in the seven reviewed documents: PASS. The
  pre-existing vision-to-intent relative link was repaired while reviewing it.
- `git diff --check`: PASS. HEAD and installed executable hashes are unchanged;
  the final review tree has 480 status entries, including preserved work.

No feature implementation, staging, commit/push, fresh seed/DRV-2, full backend
matrix, CI run, or GUI readiness notification was performed for this review.
