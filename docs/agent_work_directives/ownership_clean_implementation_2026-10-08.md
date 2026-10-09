# Ownership clean: implementation directive

> Latest authorization, 2026-10-09: the user's implementation/full-work
> instructions supersede the historical scope hold below. Follow
> [the active execution directive](ownership_cutover_execution_2026-10-09.md).
> Its P0 freeze, P1 dependency gates and final installation boundary still
> apply. The original status and base below are historical, not a new result.

Status: `ACTIVE COORDINATION — IMPLEMENTATION HOLD, FINAL REVIEW FIRST`.
2026-10-09 follow-up authorization permits only the four prerequisites in
[`ownership_cutover_preflight_2026-10-09.md`](ownership_cutover_preflight_2026-10-09.md):
local baseline, coupled contract decisions and necessary importing proofs.
It does not authorize P2-P7 consumer migration or the full cutover.
It replaces the work-item table of
[`ownership_clean_work_split_2026-10-08.md`](ownership_clean_work_split_2026-10-08.md),
which stays as coordination evidence.

Base: `main @ 3658548d24bca3d721e4f1974ac7a10da99f7aa8`, plus the preserved
dirty tree (160 status entries when this file was written). The restore
points of the work split remain valid.

**Assignment (user, 2026-10-08).**

- The bounded core specification and proofs exist; this is not a proof of
  multi-inout normalization, borrowed views or physical runtime refinement.
  GPT owns implementation. The earlier 2026-10-09 instruction narrowed work
  to contradiction repair and read-only final review; the later authorization
  above reopens the bounded preflight. Its frozen local baseline is
  `a75da80435e0d051f71cfacf76d44ea825ed87d7`. Full P0 matrix/memory and
  compiler/runtime implementation remain unrun/held. New importing evidence
  is bounded by doc 27 §5.10.4; production refinement remains OPEN.
- Claude keeps the remaining proof cores and reviews against them.
- Outputs of this directive are implementation candidates. They count as
  progress only when the integration gate below passes.

## Authority

Semantic authority is not in this file. It lives in:

- [`docs/semantics/27_ownership_clean.md`](../semantics/27_ownership_clean.md):
  - §2: the model and the algorithm;
  - §3: language rules;
  - §4: D1 = (B), the copy policy;
  - §5: **the MIR ownership contract**, the only interface the
    implementation consumes.
- The checked proofs (Rocq 9.3.0 / Stdlib 9.2.0, `rocqchk`, no admits, no
  axioms):

  | File | SHA-256 | Covers |
  |---|---|---|
  | `docs/semantics/proofs/OwnershipCleanCore.v` | `57c55889218b1f27075105d21573eb060b1709bdae2aacae756e7729c2ad15ce` | the machine, `elab`, sink modes with fail-closed summaries, ascending inference, places (focus), unpack, regions, the soundness theorem |
  | `docs/semantics/proofs/OwnershipCleanComposition.v` | `3bb896b33bd8e7a50724d94beb76550e171a8de6fbdb152fbaff42b7c24cb85f` | equational laws for target rewrites, `drops_commute` |
  | `docs/semantics/proofs/OwnershipCleanReadOnly.v` | `ff572b832a37ec410607657bb4c0da88f6e22b3aa0cda1217894293b9456e1b7` | read-only alias elision (copy propagation) |
  | `docs/semantics/proofs/OwnershipCleanExits.v` | `0258fb65e1e3102b02ff3b3b9d5c779ec36b4cd43af494c0e1ee496a96070d36` | break, continue, return, throw, try; release of partial parts on error |

- Explanation and design history:
  [`docs/207_compiler_owned_cleanup_algorithm.md`](../207_compiler_owned_cleanup_algorithm.md).
  This file is not normative.

Current source and observed gates establish facts about today's behavior,
not the desired semantics of the replacement. Doc 27 owns the adopted target
contract; its bounded proofs retain their actual scope. Record disagreement
as a defect/OPEN refinement rather than changing the contract to today's
shallow implementation or settling it independently in a backend.

## Objective card

- **Objective:** every heap value is released exactly once, at its last use,
  by code the compiler derives, and program output is unchanged. The 3 GiB
  compiler cap is met without the manual release calls.
- **Priority:** first, refine the model exactly; then fail-closed admission,
  C/LLVM parity, and executable negative evidence; then delete the old
  paths; finally, patch size.
- **Fact owner:** the ownership-clean owner (27 §5.1). It enters after existing
  DCE, performs its own ordered normalization/propagation and selections, and
  consumes checked final-generation analysis before `mir_validate`/emission.
  Post-normalization DCE is a P1 decision; any such rewrite invalidates the
  previous certificate.
- **Last consumers:** the C emitter, the LLVM emitter, the self-host emitters,
  and `--observe-ownership`.
- **Forbidden** (27 §5.1, plus the following):
  - inventing a rule that is not in 27 or the proofs. If the pass needs one,
    stop, log it, and ask Claude for a core;
  - aliasing where the contract says copy or refuse;
  - deleting a manual release before its replacement and its negative gate
    exist;
  - RC, a GC, or a "leak instead" fallback;
  - reading a missing or wrong-length summary as all-borrowed (27 §2.3
    step 6). An unresolved summary and a verified borrow are different
    facts, and only the second admits a call;
  - emitting a drop for a type whose copies can still share a backing. On
    today's shallow descriptor copies, drop insertion alone is a double free
    (`alias_copy_double_free`); the storage model changes first (I1);
  - new `own`/`ref` or carrier/restore ceremony;
  - a Coq 8 or extracted-model run presented as implementation evidence.

## Whole-chain map

| Step | Owner today | Change |
|---|---|---|
| Admission facts: types, callee, arity, D1 class, places and temporary identities | semantic admission, MIR builder | preserve the facts/exclusivity, replace old variable-only and source-own-only policy; a missing fact is `OWN_MISSING_ADMISSION_FACT` |
| Runtime-call ABI rows: lend/consume/reset, result storage/origin and operation postconditions | existing Slot resource rows and limited Allocator/TextBuilder join; collection/String owner not yet declared | P1 names the owner and registry identity under docs/180/192 before use; missing owner/column refuses (27 §5.10) |
| Borrowed views | Slice backing/place facts and indexed String origin | extend backing-owner liveness; never treat a view as an independent owning descriptor |
| Ownership facts | none | new pass, `pgy.mir.ownership.v1` (27 §5.3) |
| MIR JSON | `mir_json_dump.c` (`pgy.mir.v1`), self-host `json_projection_owner.pgy`, consumers `mir_lower` and `direct_mir_*` | one family; every producer and consumer changes in the same slice (docs/192 rejects unknown fields) |
| C emission | `transpile_from_mir` | emit exactly the facts: drop, copy, move, focus as a bitwise move out and back |
| LLVM emission | `llvm_codegen_from_mir` | the same |
| Storage model and runtime glue | shallow descriptor copies; no per-type glue | one owner per backing; deep copy and drop descriptors (27 §5.4). This replaces the storage model and precedes every drop |
| Self-host | the direct-MIR cleanup policy owners (a second authority) | the same pass in Pergyra (C5); gen2 == gen3 |
| Old paths | `collection_ownership_receipt` state machine, `semantic_collection_admit_owned_string_drop`, direct-MIR cleanup owners, `ast_collection_aggregate_*`; historical counts differ by snapshot | P0 fixes the source inventory; delete only after replacement and negative gates (I7) |

## Work items (GPT), in dependency order

| ID | Work | Acceptance |
|---|---|---|
| I1 | Storage model and runtime glue (G3): per-type drop and deep-copy descriptors; replace/discard/remove/clear/grow/failure postconditions; drops for `List`, `Set`, `Queue`; `HashMap` drops emitted; allocator preservation; deep nested clone; both shallow paths of 27 §5.4 removed | Unit tests per descriptor and mutation under ASan/LSan; same C/LLVM table; no covered copy shares backing, removed/replaced payloads release once, recoverable/continuing failure preserves old ownership. Terminal panic/abort has separate evidence, not restored-ownership claims. I5 emits no drop for an uncovered type |
| I2 | Declared ABI rows: lend/consume/reset; result storage and ownership origin, backing/view dependency and mutation/continuing-failure postconditions. Existing Slot and Allocator/TextBuilder scope is not general collection/String coverage | Missing owner, column, origin or checked view lifetime refuses; one semantic row per operation for both backends; exact negative oracle per code |
| I3 | Fact family and JSON: in-memory `MIRProgram` facts, a JSON projection, the docs/192 row, and the registry row decision (27 §5.1) | Native/self-host canonical JSON byte parity for the same admitted identity snapshot; unknown/stale/cross-snapshot facts refused; raw independently minted IDs are not equated or erased |
| I4 | Native pass (C3): admission read and ordered expression/multi-inout normalization, copy propagation (27 §2.6), mode-inference fixpoint from no summaries, focus/unpack selection and final-generation analysis certificate, region/exit edges, elaboration and edge drops, D1 check on remaining copies, refusals and observer | `--observe-ownership` shows the model's counts on the fixtures in 27 §5.8; missing/wrong-length summary refuses; normalization preserves evaluation order and all inout outputs; no old liveness certificate survives a def/use rewrite |
| I5 | Backend consumption in C and LLVM, facts only | C/LLVM stdout equals the no-free build; ASan/LSan clean on normal/handled-error §5.8 fixtures; negative fixtures refuse with no artifact. Create the manual-call-free positive successor of `inout_string_array_deep_drop.pgy`; retain the retired call as a manifest-listed negative, and migrate each parity safety property in the same slice, not a blanket acceptance flip |
| I6 | Self-host pass (C5) and emitter (G4) | gen2 == gen3; the self-host observer equals the native one on the fixtures |
| I7 | Migration and deletion (G5): classify the manual releases (27 §5.5), delete them and the old owners, add negative ratchets | The direction ratchet extends to the deleted owners; the 3 GiB cap is re-measured without them |
| I8 | Integration (C6): DRV-2, actual installer/launcher flow in an isolated candidate prefix, exact-SHA CI | All mandatory local gates and candidate CI before main landing; official bin replacement and its route revalidation only after final approval. A standalone candidate executable is not installed-driver evidence; GUI readiness only after official installation checks |

Each item names its falsifier before it starts. A red dependency's focused
acceptance gate blocks its next consumer. The whole tree may temporarily fail
the full matrix, but no unsafe drop, old-owner deletion or public installation
is excused by a future final gate. I1-I8 land atomically under
`ownership_cutover_plan_2026-10-08.md`, not as partial public semantics.

## Proof work status

The three remaining cores landed on 2026-10-08 (27 §2.3 steps 9–11):
overlapping projections and dead-aggregate parts (unpack), exits with
release of partial parts on error, and regions. The call rules now fail
closed on missing and wrong-length summaries, and inference is proven to
ascend from no summaries.

Implementation and checked/proved production refinements remain OPEN
(27 §4 and §5.10): ordered multi-inout/result/source normalization, loan/view
current-lifetime facts, the actual collection/String ABI owner and mutation/
continuing-failure postconditions, as well as member-path identity and index
checks, the real
call-graph fixpoint, deep glue in the runtime, the physical allocator,
panic and abort, and async/worker/Slot/FFI boundaries. If the pass needs a
rule that is not in 27 or the proofs, stop, log it here, and ask Claude for
a core.

## Review notes (2026-10-08)

A review the user passed on, and where each point now stands:

1. **The hard parts remain.** Agreed. 27 §4 lists them as the
   implementation's obligations. The proofs fix the decisions, not the MIR
   identities, the fixpoint over the real call graph, the glue, the
   allocator, or the async/FFI/region runtime.
2. **Drops on today's shallow sharing double-free.** Agreed. 27 §1, §4
   (order step 0), §5.4 (order), and §5.9 item 9, together with I1 and I5
   above, make the storage-model replacement a precondition of any drop.
3. **No borrow fallback on a missing summary.** Now enforced in the model,
   not only in prose. `Modes` holds `option` summaries, `no_summaries`
   replaces the old all-borrow default, and elaboration refuses a missing
   or wrong-length summary at the call and at the routine. The G1 driver
   pairs each such refusal with an admitted control, so no refusal is
   vacuous.
4. **Naming.** The user adopted "소유권 기반 자동 메모리 관리"
   (ownership-based automatic memory management) through a separate chat;
   27 §0 records it. 27 also states that the mechanism is static ownership
   elaboration and not garbage collection: no tracing, no reference count,
   and no collector at run time.

## Edit scopes

- **GPT:** every implementation file: `src/compiler/**`, `src/codegen/**`,
  `src/runtime/**`, `src/self_hosted/**` for the pass, the emitters and the
  runtime, `scripts/**`, `tests/**` (including the G1 harness), CI wiring,
  and the I7 deletion set.
- **Claude:**
  - `docs/semantics/27_ownership_clean.md`, which is normative; GPT proposes
    changes in the coordination log;
  - the three proofs above and new proof cores;
  - `docs/207_compiler_owned_cleanup_algorithm.md` §11.
- **Shared, append-only:** `tests/formal_semantics_smoke.sh` (the proof list),
  `docs/102_formal_semantics_and_proof_obligations.md`,
  `docs/semantics/README.md`, `src/self_hosted/OWNERS.md`, and
  `docs/current_work_handoff.md` (each agent edits only its own card).
- Overlap rule: stop, write a line in the coordination log, and wait for the
  user.

## Commands and budgets

- Model and extraction gate, 300 s:

  ```bash
  wsl -d Ubuntu-E-WSL --exec env OPAMROOT=/home/c/.local/share/pergyra-rocq/opam bash scripts/run_rocq_toolchain.sh bash tests/ownership_cleanup_smoke.sh
  ```

- Fresh proof corpus, 1800 s: the same wrapper with
  `tests/coq_kernel_check.sh`.
- Static owner gates: 60 s. Focused parity: 300 s. Integration shard: 1800 s.
- Build into your own output directory. Validate the real installer/launcher
  flow in an isolated prefix. Read-only support/route-injection confirmation
  is a pre-start check; if unsupported, keep the install obligation BLOCKED
  pending a user decision instead of silently expanding P1 to modify tools.
  Do not overwrite `bin/pgy.exe` or `bin/pgy-self-driver.exe` before I8 and
  final approval. If isolated installation is unsupported, keep that
  obligation BLOCKED pending a user decision, not a standalone-exe substitute.
- Commit with explicit paths only. Never add `Co-Authored-By: Claude`.
- No main landing, installed-binary replacement or GUI message before I8.
  Once local mandatory gates and Claude review pass, an explicitly authorized
  candidate commit/push may supply exact-SHA remote CI for I8. Candidate CI is
  not main landing; do not use the former no-push-before-I8 circular condition.
  Dirty-tree observations are not clean-SHA CI evidence. Re-run on the same
  worktree with all gate input differences zero against the approved candidate
  SHA; preserve WIP until its inclusion/publication is authorized. Confirm
  unpublished ancestors and an actual candidate CI trigger (main-only push
  is insufficient); final SHA changes require new binding and CI.
  The official P0 baseline has the same zero-gate-input-diff precondition and
  its checkpoint method was resolved by the approved frozen `a75da804` local
  baseline. The full P0 matrix/memory measurements are still unrun; current
  dirty preflight observations do not replace them. No implicit stash/reset/
  extra worktree.

## Integration gate (owner: GPT; Claude reviews)

The cutover plan's eight landing conditions own the complete acceptance set.
The four rows below are a cross-reference, not a second/narrower gate list.
In particular pressure, registry, caps, reachability and no-admitted-manual-
call evidence cannot be omitted. Current work is the authorized importing
proof/contract preflight, not production execution or this full landing gate.

1. The model and extraction gate, and the fresh corpus, pass on the hashes
   above, or on newer hashes that Claude records here.
2. The 27 §5.8 fixtures pass on C and LLVM: equal stdout, ASan/LSan clean,
   observer counts equal to the model witnesses, and the negative fixtures
   refused.
3. Self-host: gen2 == gen3 with the pass on.
4. DRV-2, isolated candidate installer/driver route, and exact-SHA CI are
   green on the complete committed source snapshot. Official installation
   and official-path revalidation follow final approval; no readiness message
   substitutes for those checks.

## Evidence at hand-off (2026-10-08, Claude)

- `tests/ownership_cleanup_smoke.sh` passes. Its kernel check covers 5
  files (the four proofs and the extraction contract) with 0 axioms.
  Controls: 15 decision, 24 refusal, 10 summary, 43 sink/composition, 37
  read-only, 9 places, and 8 exits. The 30 cost cases are in
  `.tmp/ownership-cleanup/model-cost.json`. Log:
  `.tmp/rocq93_ownership/cores3-smoke-claude.log`.
- The fresh corpus passes: 62 proofs (including the other chat's
  `OwnershipCleanGCComparison.v`), the approval consumer, and an axiom
  budget of 2. Log: `.tmp/rocq93_ownership/kernel-cores3-full-claude.log`.
- G1 harness hashes: extraction
  `90cab8f276a0568eb3280eb48d4f90c78461f7532726f53285a38cd5eb2f8e5c`, driver
  `6b7dc102cb3f7c86c36f24f715f64018fcd22a825050dd7b164ebb84c23fcbb9`, smoke
  `a7598a10cd3931b568db2ae7d66c2313eba89201037252ac523c0a9de0a0fae0`.
- No compiler, runtime, or installed-binary change. No commit, push, CI, or
  GUI message.

## Coordination log

- 2026-10-09 GPT: user-directed exception 2 permits the documentary edits in
  doc 27 and doc 207 §11 normally assigned to Claude. Two actual read-only
  Claude reviews checked the contractual changes; this is not model proof or
  implementation. Their final two Medium documentary fixes were subsequently
  applied by GPT and explicitly rechecked, not reported as a third review.
- 2026-10-09 GPT, latest scope: user requested contradiction repair first,
  then one more final check before implementation. Only documents changed;
  proof/compiler/runtime changes, bootstrap, installation and publication
  remain held. The earlier start request is superseded for current execution.
- 2026-10-09 GPT: repaired pass order/analysis epoch, pre-MIR admission, view origin,
  mutation lifecycle, canonical JSON and candidate-CI sequencing to agree with
  the cutover plan. Existing proof hashes are unchanged; P1 normalization and
  view/runtime refinement are not marked proved. Main worktree, 3 GiB cap and
  negative safety fixtures remain in scope; no installation or landing yet.

- 2026-10-08 Claude: wrote this directive at the user's request. The core
  gained one-block-per-node footprints and the focus statement; ReadOnly and
  Composition gained the wider admission and `drops_commute`; the G1 harness
  was retargeted and extended with 9 places controls.
- 2026-10-08 Claude: landed the three remaining cores (unpack, regions,
  exits) and fail-closed summaries, and updated 27 and the hashes above.
- 2026-10-08 Claude, overlap: the `Modes` change breaks every caller of the
  old all-borrow default, so the G1 harness changed in the same slice,
  inside GPT's `tests/**` scope: `tests/ocaml/ownership_clean_driver.ml`
  (summary and exits controls; no default table for calls),
  `tests/coq/OwnershipCleanupExtraction.v` (summary, ascent, and exit
  contracts), and one `cp` line in `tests/ownership_cleanup_smoke.sh`.
  `tests/formal_semantics_smoke.sh` gained the `OwnershipCleanExits.v` entry
  (append-only). GPT owns these files from here.
- 2026-10-08 Claude, not mine: `docs/semantics/proofs/OwnershipCleanGCComparison.v`
  (mtime 16:02, registered in `tests/formal_semantics_smoke.sh`) appeared
  while this work ran. It already uses `no_summaries`. Against the current
  Core it does not compile (`Nat.eqb_refl` not found at line 182; an earlier
  snapshot failed at line 46), so `tests/coq_kernel_check.sh` on the full
  corpus is red. I did not edit it; its author fixes it or the user decides.
  It belongs to `ownership_automatic_memory_gc_comparison_2026-10-08.md`, a
  bounded supplement the user opened in another chat. That chat also added
  27 §0 (the adopted name). Its scope excludes Core, ReadOnly, Composition,
  Exits, and 207 §11. My mechanism paragraph in 27 defers to §0 for the name.
- 2026-10-08 Claude: that file now compiles (SHA-256 `9ae1efcf...`), and
  the full fresh corpus is green with 62 proofs. Nothing is blocked.
- 2026-10-08 Claude: the Core header comment was corrected (exits, summaries,
  unpack and regions are now in scope); no definition or proof changed. New
  hash `57c55889...`; the other chat's card still names `c629a6a3...`, and
  its comparison compiles against the new Core (fresh corpus, 63 proofs).
- 2026-10-08 Claude: added `OwnershipGraphLinks.v` (graph stores: one owner,
  many links) at the user's request, as a design check for
  `docs/audits/ownership_graph_links_design_2026-10-08.md`. It is not an
  I1-I8 item and does not change the MIR contract.
- 2026-10-08 Claude: the next Claude/GPT split, across this directive and
  the node-teardown model, is in
  [`claude_gpt_ownership_collaboration_2026-10-08.md`](claude_gpt_ownership_collaboration_2026-10-08.md).
  The I1-I8 table here is unchanged and is GPT's item GT3 there.
- 2026-10-08 Claude: the user decided that mixed manual/automatic ownership
  is itself a defect, so I1-I8 now land together as one cutover:
  [`ownership_cutover_plan_2026-10-08.md`](ownership_cutover_plan_2026-10-08.md).
  The per-item acceptance conditions above still apply inside the cutover
  tree; landing happens once, when the plan's full landing gate is green.
