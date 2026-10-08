# Ownership clean: Claude / GPT work split

Status: `SUPERSEDED` for work items by
[`ownership_clean_implementation_2026-10-08.md`](ownership_clean_implementation_2026-10-08.md)
(2026-10-08); kept as coordination evidence. Base:
`main @ 3658548d24bca3d721e4f1974ac7a10da99f7aa8`, plus the preserved dirty
tree. Restore point for the dirty tree as of 2026-10-08 09:55 KST:
`refs/backup/main-dirty-2026-10-08` (commit `ceafa8f4`, tracked files) and
`.tmp/backup/untracked-2026-10-08.tgz` (250 untracked files).

The user split this work between two agents. Each agent works only inside
its own scope and reads this file before every session.

Semantic authority is not in this file. It lives in:

- [`docs/semantics/27_ownership_clean.md`](../semantics/27_ownership_clean.md)
  (model and decisions);
- [`docs/semantics/proofs/OwnershipCleanCore.v`](../semantics/proofs/OwnershipCleanCore.v)
  (proof);
- executable gates.

## Shared objective card

- **Objective:** replace manual release and its proof scaffolding with
  compiler-owned ownership cleanup, as specified in `27_ownership_clean.md`.
  Delivery means DRV-2 builds, CI is green, and the Alrescha GUI session is
  told only after P0 and CI are verified.
- **Priority:** first semantic identity and soundness (the elaboration must
  refine `OwnershipCleanCore`), then fail-closed admission, executable
  negative evidence, deletion of old paths, and finally patch size.
- **Fact owners:**
  - the ownership-clean MIR elaboration (Claude) owns move/copy/drop facts;
  - the runtime and backends (GPT) consume them and never infer ownership;
  - the Rocq toolchain owner (GPT) admits the prover.
- **Last consumers:** the C and LLVM emitters, the self-host emitter, the
  installed driver, and CI.
- **Forbidden:**
  - a second ownership model or heap authority beside
    `OwnershipCleanCore.v`;
  - backend-invented ownership;
  - a hidden GC or RC fallback;
  - deleting a release check before its replacement and negative gate exist;
  - new own/ref/carrier/restore ceremony as a fix;
  - Coq 8 runs presented as proof evidence.
- **Prover:** Rocq 9.3.0 with stdlib 9.2.0
  (`scripts/rocq_toolchain_owner.sh`). Every proof imports `Stdlib.*`.

## Work items

### Claude

| ID | Work | Depends on |
|---|---|---|
| C0 | Merged into G2. Every file Claude edited in the previous session also carries GPT hunks, so one agent reverts all of them. Claude authorizes reverting its own part: the aggregate-release lineage/exclusivity/element-borrow/transition/place-use/schema/member-transition changes, the `driver_rung2_owner` and `canonical_mir_execution_owner` `own` additions, the 4 `aggregate_release_*` fixtures, their gate rows, and the matching caps. | — |
| C1 | Formal cores, kept in step with `27_ownership_clean.md`: (a) calls and inferred parameter conventions (borrow, inout, sink), with recursion through a summary fixpoint; (b) places, i.e. member-path `inout` with disjointness; (c) zone/region bulk release. | — |
| C2 | The MIR ownership contract: copy/move/drop facts and the per-type drop-glue descriptor, written as `27_ownership_clean.md` §5. This is the only interface G3/G4 consume. | C1a draft |
| C3 | Native MIR elaboration pass: liveness, move/copy, settle, arm drops, loop-head certificate check, emitting C2 facts, with its fact-owner tests and negative gates. | C2 |
| C4 | Interim DRV-2 bridge, gated by a decision. Measure the compiler's peak memory when compiler source keeps only shallow retirement (no `ArrayDropOwnedStrings`, no owned String pushes). If it stays under the 3 GiB cap, convert those sites so DRV-2 has no aggregate-release obligations. If it does not, record BLOCKED until C3+G3. | G2 |
| C5 | The same elaboration in the self-hosted compiler. | C3 green |
| C6 | Integration: native gate, DRV-2, installed driver, CI, Alrescha message. | all |

### GPT

| ID | Work | Depends on |
|---|---|---|
| G0 | Finish the Rocq 9.3.0 migration that is in progress. The fresh corpus kernel check must include `OwnershipCleanCore.v`. If that file fails under 9.3, report the exact error in the coordination log below; Claude fixes Claude's files. | — |
| G1 | Remove `OwnershipCleanup.v` as a separate model. Either delete it, or turn its useful content into an extraction and performance harness over `OwnershipCleanCore.elab`, with no separate state machine or heap. Run cost tests on the extracted `elab`: fixed workloads, growth in statements and loop nesting, receipts with exact hashes. | G0 |
| G2 | Revert GPT's 2026-10-07/08 MIR-builder own/carrier chain, the aggregate-release expansions, and Claude's C0 set to `3658548d`. That covers MIR/semantic/codegen/compiler own threading; the new `aggregate_*_generation/publication/transfer/carrier`, `member_read_local_origin` and related owners and fixtures; their caps; and the "Owned Functional Updates" section of `docs/mut_borrow_parameters.md`. **Keep:** the proof/Rocq lane, `AGENTS.md`, the native assignment write-place fix in `type_checker_assignment.c` with its semantic cases, the stdlib argument-retention rows, and the 2026-10-08 direction records (`docs/audits/ownership_dx_architecture_recheck_2026-10-08.md`, `docs/self_hosted/13` "Ownership Cleanup Synthesis Direction"). Mark the v11–v14 handoff text SUPERSEDED rather than deleting it. Write the list of reverted paths into the coordination log. | — |
| G3 | Runtime and backends, consuming C2 facts only: the String owned/static distinction, per-type drop glue in C and LLVM, deep drops for Array, Map, List and aggregates, C/LLVM parity, and ASan/LSan zero-leak and zero-UAF gates over the shared fixture set. | C2 |
| G4 | Drop-glue emission in the self-host codegen, consuming C5 facts. | C5 |
| G5 | Deletion sweep. Remove the manual release calls (about 447), the `ast_collection_aggregate_*` release analyzer (34 files), the carrier/restore code paths, own threading, and the gates that only test those mechanisms. Add negative ratchets so they cannot come back. Re-measure peak memory against the 3 GiB cap. | C3, C5, G3, G4 |
| G6 | Bookkeeping that follows from G-work: component-smoke caps and pins, `OWNERS.md` rows, gate inventories. | as needed |

## Edit scopes

**Claude's scope:**

- `docs/semantics/27_ownership_clean.md`
- `docs/semantics/proofs/OwnershipCleanCore.v` and the C1 core files
- the new native ownership-clean MIR pass files (`src/mir/*ownership_clean*`
  or `src/compiler/*ownership_clean*`)
- the new self-host elaboration owners
  (`src/self_hosted/mir/*ownership_clean*`)
- C4 conversion sites, after G2 lands

**GPT's scope:**

- `scripts/rocq_*`, `scripts/install_rocq_*`, `tests/coq_*`
- proof import prefixes in other proof files
- the proof CI and Makefile wiring
- `docs/semantics/proofs/OwnershipCleanup.v`
- G2 paths
- the `src/runtime/**` drop and String representation
- C/LLVM drop-glue emission in `src/codegen/**`
- self-host emitter drop glue
- the G5 deletion set

**Shared, append-only files:**

- `tests/formal_semantics_smoke.sh` (proof list)
- `docs/102_formal_semantics_and_proof_obligations.md`
- `docs/semantics/README.md`
- `src/self_hosted/OWNERS.md`
- `docs/current_work_handoff.md`. Each agent edits only its own card. The
  active self-host card is GPT's until G2 marks it SUPERSEDED; after that,
  Claude owns the ownership-clean card.

**Rules for shared files and overlap:**

- Re-read the file immediately before writing it. Add rows; do not reformat,
  reorder, or revert the other agent's hunks.
- If a scope overlaps, stop, write a line in the coordination log, and wait
  for the user.

## Commands and budgets

- Static checks: 60 s.
- Focused proof or gate: 300 s.
- Fresh corpus kernel check or integration shard: 1800 s.
- Each agent builds into its own output directory and does not overwrite the
  installed `bin/pgy.exe` or `bin/pgy-self-driver.exe` until C6.
- Commits use explicit paths only, never `git add -A`, and only cover the
  committing agent's own scope.
- No `Co-Authored-By: Claude` line.
- Pushing or a GUI message requires C6 evidence.

## Integration gate (owner: Claude)

1. Rocq 9.3.0 fresh corpus kernel check including `OwnershipCleanCore` (G0
   receipt).
2. Native ownership-clean execution gate (C3+G3). The shared fixtures (GUI
   shape, branch, loop, pack/push, copy-of-live) must:
   - produce output identical to the current no-free build on C and LLVM;
   - show zero leaks and zero UAF under ASan/LSan for the closed fixtures;
   - refuse the negative fixtures.
3. Self-host parity with elaboration on (C5+G4): gen2 == gen3.
4. DRV-2 build, then the installed driver, then exact-SHA CI green (C6).

Every output stays an implementation candidate until its receipt exists.

## Decisions

**D1, copy policy: decided (B)** by the user on 2026-10-08. The type classes
and the elision order are in `27_ownership_clean.md` §4. This affects work
items as follows:

- C3 implements copy insertion, the move-at-last-use and read-only-elision
  attempts, and the explicit-copy refusal.
- G3 makes `Clone` deep for nested collections.

**Handoff after C1 sink and C2 (2026-10-08).** The user directed that, once
Claude finishes the standard specification, implementation goes to GPT.
That specification is now in place: `OwnershipCleanCore.v` with sink
inference, and section 5 of `27_ownership_clean.md`, the MIR ownership
contract. Recommended GPT order:

1. G1: retarget the extraction harness to the new API (coordination log
   below).
2. G3: runtime glue and String storage classes (27 section 5.4), and the
   runtime-call ABI rows for lend, consume, and reset (5.5).
3. C3: the native pass, following 27 section 5 exactly. Where read-only alias
   elision or the places core is still missing, report
   `OWN_EXPLICIT_COPY_REQUIRED` rather than alias.

The user confirms who owns C3 and C5. Claude's remaining proof work is the
read-only alias elision core, the places core, and the exits core; the first
two carry the most DX weight (27 section 5.3).

## Coordination log

- 2026-10-08 Claude: wrote `OwnershipCleanCore.v` (no axioms; Coq 8.18 coqc and
  coqchk smoke only, so Rocq 9.3 is pending G0), `27_ownership_clean.md`, the
  index rows, and the proof-list registration. GPT's `Stdlib.*` prefix
  migration also touched `OwnershipCleanCore.v`; Claude accepts that change.

- 2026-10-08 GPT G0: stable Rocq core/runtime 9.3.0, Stdlib 9.2.0 and
  project-local opam 2.6.1 are installed/admitted. Earlier fresh 58-file corpus,
  approval export, kernel negative controls and formal gate passed. The newer
  OwnershipCleanCore revision FAILS under 9.3.0: ./OwnershipCleanCore.v:1212,
  characters 59-72, `Error: No such contradiction`. Receipt:
  .tmp/rocq93_ownership/kernel-admitted-final.log. Per G0, Claude owns the fix;
  GPT has not changed that proof. Earlier green is not current-model evidence.
- 2026-10-08 GPT G1: deleted the independent OwnershipCleanup.v. Fresh OCaml
  extraction of OwnershipCleanCore.elab passes 8 move/copy controls, 9 refusal
  controls and 25 fixed cost cases on the earlier source hash recorded in
  .tmp/ownership-cleanup/model-cost.json. New-model re-admission is pending the
  G0 failure above; those timings do not prove compiler/runtime cleanup or D1.
- 2026-10-08 GPT G2: restored 196 tracked chain files to 3658548d and removed
  234 backed-up new chain files; exact reverted paths are listed in
  ownership_clean_g2_rollback_2026-10-08.md. Recovery copies are under
  .tmp/ownership-cleanup/g2-recovery/. All 7536 source imports resolve. The
  twelve named native/retention files match ceafa8f4 exactly. Installed native
  and driver hashes are unchanged. Hard wiring, retention, CI profile and
  language golden pass; whole world/component inventories exceeded the 60-s
  local static budget (no larger allowance or full-green claim). Old v11-v14
  handoff/directive continuation is SUPERSEDED; baseline manual retirements
  remain until G5 dependencies. G3 waits for C2, G4 for C5, G5 for their actual
  replacement gates; no compiler/runtime edits or GUI message in those lanes.
- 2026-10-08 Claude: `OwnershipCleanCore.v` now covers calls. It adds borrowed
  readonly parameters, inout move-in/move-out, and a frame heap. `elab` takes
  `elab s L B`, and function tables are elaborated by `elab_fun` and
  `elab_proc`. The file has 2570 lines and 125 theorems. All theorems close
  under the global context, and Coq 8.18 coqc and coqchk smoke pass; Rocq 9.3
  is pending G0. G1 extraction must target this signature.

- 2026-10-08 GPT G0 current-C1 receipt: fresh Rocq 9.3.0 kernel check PASSES
  all 58 corpus files plus the approval export/binding consumer. The checked
  OwnershipCleanCore SHA-256 is
  d4cca76d5d3ff463126452ddfd928a936ca2e5be19bf91f640deee9c865b3c8b;
  the current file still matches that snapshot. Log:
  .tmp/rocq93_ownership/kernel-current-c1.log (exit 0). The earlier line-1212
  failure does not reproduce on this source and is superseded for this hash;
  its cause is not inferred from a later pass. G1 now forwards elab(s,L,B),
  and its fresh extraction passes 10 decision/13 refusal controls and all
  30 fixed cost cases on the same model hash. Receipt schema:
  pergyra.ownership-clean.elab-cost.v2. This admits the model only, not C2,
  compiler refinement, runtime cleanup, the D1 policy or GUI readiness.

- 2026-10-08 GPT G1/G6 final local receipt: current caller/function/procedure
  extraction now passes 15 decision and 24 refusal controls plus 30 fixed
  cost cases. Logs: model-calls-final.log and formal-current-c1.log under
  .tmp/rocq93_ownership/; current model hash remains d4cca76d5d3ff463... .
  The beta-readiness structural consumer now requires the shared stable
  proof job instead of apt Coq, and passes. G2's 196 base equalities, 234
  deletion/recovery paths, and 12 keep-file hashes were rechecked after G0.
  Detailed receipt: docs/audits/ownership_clean_g_receipt_2026-10-08.md.
  G3/G4/G5 remain dependency-pending; no installed driver/push/GUI claim.

- 2026-10-08 GPT requested read-only algorithm analysis: the current model's
  theorem is conditional on source execution, callee-body admission and
  INV/CORR, not elab success alone. Six scratch scope observations pass under
  zero-assumption Rocq checking (review-scope-current.log). C2 must consume
  existing typed/name/arity facts and bind physical copy/drop glue; the model
  does not supply that refinement. gui_calls still has two callee copies,
  and finite big-step/normal-exit/whole-value scope is unchanged. Findings and
  fixed-input cost boundaries are appended to the G receipt audit. No C-file
  or Claude proof change was made for this analysis.

- 2026-10-08 GPT requested concrete-code tests: pinned installed native
  compiler baseline passes five user-shaped .pgy programs on explicit C and
  LLVM routes (30 exact stdout observations) plus two compile-only borrowed-
  deep-drop refusals. All tracked input/compiler/runtime hashes were stable;
  run completed in 16.952 s without replacing either installed executable.
  Baseline receipt and repeat runner are under
  .tmp/ownership-cleanup/code-baseline/. C2/C3 remain absent, so these are NOT
  automatic-cleanup costs, memory/leak/UAF evidence, current-source build
  provenance, G3 completion or CI/GUI readiness. Same fixtures are future
  integration falsifiers, not permission to invent backend ownership.

- 2026-10-08 Claude C1 sink core and C2 contract. `OwnershipCleanCore.v`
  now proves calls with inferred parameter modes (borrow or sink). It has
  3131 lines and 150 theorems, with no admits or axioms. SHA-256:
  f8ac63cf9a9a47389863761b3c84a49279b7e683f807956950844bc3d0d69d00.
  - Rocq 9.3.0: `rocq compile` and `rocqchk` pass, and 20 named theorems are
    closed under the global context. The fresh 58-proof corpus kernel check
    passes (`.tmp/rocq93_ownership/kernel-sink-claude.log`).
  - Falsifier closed: `gui_calls_sink_moves_only` gives 0 copies where
    `gui_calls_copy_twice` (borrow-only) gives 2.
  - API change that G1 must follow. Until it does, the OCaml driver of
    `tests/ownership_cleanup_smoke.sh` cannot compile against the new
    extraction. Claude did not run that smoke, so GPT's cost receipt is
    untouched.
    - `elab M s L B`, where `M : Modes` and `borrow_all` reproduces the old
      decisions;
    - `elab_fun M g d` and `elab_proc M g d` return
      `(sink params, borrowed params, body, result)`;
    - one target call form `TCall k x g sys bys`, with `TCallIO` removed;
    - new exports: `infer_modes`, `gui_modes`, `gui_calls_reuse`, `upd_funs`.
    - Expected control values, from a scratch extraction:
      `.tmp/rocq93_ownership/sink-controls-claude.log`. Under `borrow_all`
      every old decision is unchanged. Only the inout control's live-in list
      changes, from `[1;0;1]` to `[1;0]` (the same set).
  - `27_ownership_clean.md` section 5 is the C2 contract that G3 and G4
    consume. G3 is unblocked.
  - Claude made no edits in GPT's scope.

- 2026-10-08 GPT, direct user request for Rocq/research improvements: added a
  bounded composition proof importing the canonical core (unchanged
  f8ac63cf9a9a...), without a second ownership machine or changes to C2/C3.
  G1 now consumes `elab M s L B` and the four-field routine, with typed proof
  consumers. Fresh extraction passes 15 decision + 24 refusal + 40
  sink/composition controls and 30 fixed cost cases; full formal gate passes
  59 files plus approval consumer. Skip normalization reduces syntax nodes,
  not resource effects. Directive: ownership_clean_composition_2026-10-08.md.
  Direction residue scan timed out at 60 s (not marked green). A receipt-only
  whole-tree WSL status bottleneck was bounded to seven admitted input paths;
  source hashes, semantic inputs and budgets remain checked. C2's new section
  5 was observed; G3 is not described as blocked on an absent document. No
  production, install, SoT, commit/push or GUI claim in this scoped work.

- 2026-10-08 GPT, direct continuation of the user's Rocq request: completed
  bounded local readonly alias elision as an imported supplement, without
  modifying Core (f8ac63cf9a9a...) or C2/C3/C5. The original program must be
  admitted; readonly reads are substituted with the root and the canonical
  cleanup retains it until the last alias use. Proved same trace/empty heap
  and target allocation frontier 3 -> 2 on branch witnesses, plus a concrete
  mutation counterexample. Supplement hash 14b0122898faf83bffb13836087a6c6bbdd0bf97ef255d08e57fe28a5eb5d8bb.
  Extraction passes 111 controls, 30 fixed cost cases and two branch cost
  witnesses; 4-module kernel has zero axioms. Full corpus passes 60 proofs
  plus approval consumer. Native Git Bash passes the unchanged complete-source
  residue/selftest within the same 60-s budget. Details: readonly directive,
  docs/207 section 10 and the recent-research audit. General member-projection
  loans, places, exits and physical/MIR refinement remain outside this slice.
  C3/C5 takeover was asked nonblockingly but not confirmed; existing ownership
  is unchanged. No compiler/install/CI/SoT/Git-publication/GUI completion claim.

- 2026-10-08 Claude, at the user's request: the composition laws and the
  read-only alias elision in docs/207 sections 9 and 10 are adopted as the
  standard design. They are normative in 27_ownership_clean.md section 2.6.
  - Core, Composition and ReadOnly compile under Rocq 9.3.0, and `rocqchk`
    passes. Their 9 key theorems are closed under the global context.
  - Two refinements of the same design are scratch-checked with no
    assumptions, in .tmp/ownership-cleanup/claude-refinements/:
    - `copy_propagation_elision`: admission refuses only writes to the alias
      or the root, so calls, packs and pushes in the overlap are allowed;
    - `drops_commute`: release order inside a set is free.
  - Moving both into the repository proofs, together with the extraction
    observer, is the next design change. Projection aliases
    (`let items = h.items`) still wait for the places core.

- 2026-10-08 Claude, at the user's request: the wider read-only admission and
  `drops_commute` are now in the repository proofs. The places core is done.
  - The core gives every value one block per node and proves a focus
    statement: member-path inout, update from self, and read-only part view,
    with no copy (`pack_child_segment` shows the blocks moved are the part's
    own).
  - Hashes: Core 900c2b27, Composition 3bb896b3, ReadOnly d3b42c35.
  - `tests/ownership_cleanup_smoke.sh` passes (4 proofs, 0 axioms; places
    controls added). The fresh corpus passes 60 proofs.
  - Work items continue in ownership_clean_implementation_2026-10-08.md.
