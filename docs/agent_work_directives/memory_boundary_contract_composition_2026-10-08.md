# Memory boundary contract composition

Status: `AUDIT COMPLETE; composition requirements adopted, production OPEN`. Base:
`3658548d24bca3d721e4f1974ac7a10da99f7aa8`, preserved dirty main
(168 entries, empty index at the initial snapshot).

The user explicitly selected a coherent contract joining Slot, automatic
ownership cleanup and non-owning graph links. This scope fixes composition
requirements and a typed audit of the existing proofs. It does not take over
Claude's canonical cores or change the I1-I8 implementation order in
`ownership_clean_implementation_2026-10-08.md`.

## Objective card

- Objective: one storage owner, access rights bound to that owner, and one
  retirement edge; graph connections do not become storage owners.
- Priority: existing semantic identity and D1, same-owner admission,
  fail-closed lifetime/authority checks, explicit proof limits, then DX/cost.
- Owners: doc 27 / OwnershipCleanCore for ordinary values; docs 08 and 13 /
  SlotCalculus for the covered resource boundary and ABI; new doc 28 for
  composition requirements, not a replacement heap or Slot definition.
- Last consumers: target C/LLVM and self-host emission plus runtime access
  and retirement. The graph-to-owner binding producer does not exist yet;
  a composed proof is conditional on that admitted binding, not its issuer.
- Forbidden: a second allocator/heap model, Slot per ordinary value or link,
  graph links contributing ownership, a fresh root standing in for a fresh
  node, free-before-release-admission, hidden GC/RC/copy fallback, or a
  model verdict presented as production support.
- Verification: fresh Rocq 9.3.0 / rocqchk typed composition audit, reusing
  the existing two approved Slot abstractions only; documentation/links;
  unchanged Slot and ABI owner gates. No compiler-scale benchmark track.
- Falsifiers: missing or mismatched root/footprint binding, stale/released root,
  missing token, pinned retirement, live root with a missing/reused node,
  double cleanup, and a cyclic/multiply-linked graph with one root drop.

## Reached chain and implementation limits

| Boundary | Current owner and consumer | Composition obligation |
|---|---|---|
| Ordinary value storage | doc 27 and OwnershipCleanCore INV/elab/texec; production refinement OPEN | preserve the canonical footprint and D1; no drops on shared backing |
| Local Slot layout | doc 13, pgy_runtime_plain_slot_inline.h, MIR ABI rows and C/LLVM consumers | occupied is not a generational graph identity certificate |
| Generational Slot access | SlotCalculus; slot_manager storage/security/pin operations | root generation, token and pin predicates retain their meaning |
| Resource ABI projection | mir_abi_resource_runtime_row_for_type_name, abi.runtime_call_rows consumers | backends consume admitted rows, not invented root/link policies |
| Graph links | dated graph proposal and existing OwnershipGraphLinks design model; no stored-reference production owner | explicit admitted root/footprint binding and current node membership are mandatory |
| Retirement | canonical inv_drop/TE_Drop and Slot HandleRelease/Step_Release | pair guards with the same root; real atomicity and payload finalizer refinement remain OPEN |

The audit reuses the existing graph model's ODrop and GInv, with an explicit
correspondence to the canonical TDrop footprint and the Slot root. It may use
logical edge metadata over canonical Block identities; it must not claim a
new stored-reference interpreter, lock protocol or runtime graph ABI. The
graph model arrived during the initial audit; the complete chain map and
remaining change set were revised before extending the audit. No core is
edited. A disconnected node can remain owned
until explicit deletion or owner end; that is not immediate GC-like recovery.

## Edit scopes and sequence

GPT owns the new doc 28, test audit and focused gate in this scope. Append
navigation to the semantics README, doc 102, vision, doc 207 sections after
13, Make/CI and this chat's handoff. Update only this chat's graph memo.
Do not edit doc 27, canonical Core/Composition/ReadOnly/Exits, SlotCalculus,
Slot ABI, src/**, installed executables or registry statuses.

1. Write the composition requirements and evidence/OPEN matrix.
2. Build typed test consumers over existing heap, graph and Slot predicates;
   prove live-block coverage, graph/canonical retirement agreement and Slot
   release under the stated guards, not issuer correctness or atomicity.
3. Add positive graph-view witnesses and material refusal witnesses.
4. Run the focused gate, then documentation/ABI and the existing proof corpus
   at the integration boundary; record exact hashes and observed results.

## Commands and integration

Static checks: 60 seconds. Focused audit: 300 seconds. Corpus: 1800 seconds.
Use the project-owned WSL Rocq prefix through scripts/run_rocq_toolchain.sh;
never system Coq or an expanded assumption budget. Integration owner is this
chat; the focused gate is tests/memory_boundary_composition_smoke.sh.
No commit, push, install, GUI message or production implementation in this
contract scope. Documents are adopted requirements; audit results are bounded
formal observations, not graph-feature or self-host closure.

## Receipt

Observed on 2026-10-08 under the admitted Rocq 9.3.0 / Stdlib 9.2.0:

- Focused gate PASS: 5 fresh modules plus the approved export/binding
  consumer, only the existing two Slot abstractions. No admits, added
  axioms or unsafe kernel features. Log:
  `.tmp/memory-boundary-composition/kernel.log`.
- Full formal inventory and fresh corpus PASS: 63 proofs plus the existing
  approval consumer, same two-abstraction budget. Log:
  `.tmp/memory-boundary-composition/formal.log`. The focused gate reuses
  canonical modules; do not count the two runs as 68 independent models.
- Typed composition checks the same actual graph footprint and canonical
  owned root, equality of graph/canonical retirement heaps, preservation of
  both invariants and admission of the same Slot release. The fixed cyclic
  graph satisfies both invariants. Missing/wrong footprint, stale/released
  root, missing token, pin, graph borrow, missing/reused node and double-drop
  witnesses remain negative. Slot verifier acceptance is never fabricated.
- Documentation quality, ABI ownership shape, Bash syntax, Make target
  resolution and workflow YAML parsing PASS. UTF-8 and local links PASS for
  the scoped documents; new-file whitespace and scoped diff check PASS.
  A whole-file whitespace scan found an existing blank-with-space at vision
  line 610, outside this patch. It is preserved, not claimed cleaned.
- Existing Slot C/LLVM baseline PASS: 4 positive and 7 rejection cases per
  backend. The gate explicitly uses the native pipeline, not self-host
  substitution. Log: `.tmp/memory-boundary-composition/slot-baseline.log`.

Final pinned source SHA-256:

| Input | SHA-256 |
|---|---|
| OwnershipCleanCore.v | `57c55889218b1f27075105d21573eb060b1709bdae2aacae756e7729c2ad15ce` |
| OwnershipGraphLinks.v | `6045e9ea9efca71327bc0e49f454bc63b500858caf1633b42ecad300688eefda` |
| SlotCalculus.v | `99c06b1bc7e4d1a9dac5b9d01107924b6d0a59d72b796a9689a8c4b2e1870909` |
| MemoryBoundaryCompositionAudit.v | `c96920a370cd9415271d626889abecd64005afe021bd9c71b94c160fc4c371a6` |
| Focused gate | `36ffdd13286d6f9a5598652c20a9bfbbe40c10fce30aa565e4378cf90bb85403` |

The canonical sources were not edited in this scope. Core's earlier
`c629a6a3...` checkpoint is superseded by the separately recorded header-only
update; verification here binds the current `57c55889...` source, not that
older receipt. Existing nested-inductive/deprecation/loadpath warnings and
Rocq's reported indices-not-mattering theory dependencies remain visible in
the logs; this is not a warning-free proof run.

HEAD remains `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, preserved dirty main
(171 status entries at the receipt, empty index). Native binary `f6559da9...`
and self-driver `707dcd40...` are unchanged. New files are doc 28, this
directive, the typed audit and its focused gate. Navigation was appended to
the vision, semantics README, doc 102, doc 207, this chat's graph memo and
handoff; Make and stable-proof CI run the new focused gate. Unrelated/shared
dirty files are not this scope's patch.

No production compiler/runtime/ABI edit, owner-registry closure, stage,
commit/push, installation, remote CI or GUI message occurred. Next falsifier:
an actually issued graph/Slot/canonical-root correspondence must survive
deletion/growth and reach one atomic/coherent retirement edge on C/LLVM;
refuse missing correspondence before payload destruction. Source graph
observations, finalizers, escape and runtime cost remain OPEN. The existing
I1-I8 implementation order and active self-host rung are unchanged.

## Reuse reinforcement (Claude, 2026-10-08, at the user's request)

The user asked Claude to fix the audit, red-team it, and make node reuse
the central falsifier, because a model in which storage cannot be reused
proves nothing about reuse. This supersedes the input table above for the
three changed files.

| Input | SHA-256 |
|---|---|
| SlotCalculus.v | `a05e7a54cccbbdc5c77b8e30fcc11d54505e734c71e5d0b692bfb2d8b5bbe100` |
| OwnershipGraphLinks.v | `8dba4bc615ac2ddd4c437e585ba5649f7bbd0c22fabf489a2106cabeab9d543c` |
| MemoryBoundaryCompositionAudit.v | `7753602519e7cf391d671026e503b295ece120136c7340dfe49761da7c808d23` |
| doc 28 | `8b8e8a357c81e08973e58be1b2f11fba0a97a98743e03ddaa59551f281b370fd` |
| doc 08 | `9c11ccea36ff6018716d464c8a1bf03e24008508bb3dee0c6c56dcbc4a6d613b` |

Findings and changes:

1. **SlotCalculus could not express root reuse safely.** A release emptied
   the slot, and every empty id was claimed at generation 1, so the
   previous occupant's handle read the next one. The runtime already
   advances the generation (`slot_manager_core_ops.c`); the model did not.
   - Release now leaves a tombstone that keeps the generation, and a new
     `Step_Reclaim` claims it at the next generation.
   - `stale_step` and `stale_handle_never_admitted` mechanize doc 08's ABA
     theorem; `gen_one_reclaim_resurrects` keeps the old rule as a
     counterexample.
   - `pin_non_eviction` now states that a live pinned slot stays live.
   - Lemma names, the two approved parameters, and the pinned header text
     are unchanged.
   - This edits a file outside this directive's original scope, at the
     user's request.
2. **The graph model never reused storage.** Its allocator is now
   adversarial: operations take the blocks to use, and any distinct,
   non-live choice is admitted, including blocks freed the step before.
   Every theorem was re-proven under it. `reused_blocks_do_not_revive_link`
   reuses B's exact block for D.
3. **The audit's link was an address.** `(root handle, block)` admitted B's
   old link after D reused the block (`address_link_admits_reused_node`).
   Access now resolves a generation link (`AccessValid`), and
   `access_targets_owned_live_storage` derives membership instead of
   assuming it.
4. **Store ids were never reissued.** `store_id_reuse_resurrects` shows what
   reissuing does. Rooted links carry their root handle's generation
   (`rooted_link_refused_after_root_reuse`).
5. **Retirement assumed the whole program was a graph.**
   `retirement_agrees_split` splits the heap. `one_canonical_owner_per_block`
   and `retirement_consumes_once` cover the double-cleanup row.
6. **Canonical layer.** `reuse_never_aliases_names` shows that ordinary
   values tolerate reuse of dead blocks, because they store no references.

Gates (Rocq 9.3.0, `rocqchk`, two approved abstractions only):

- `tests/memory_boundary_composition_smoke.sh`: PASS;
- `tests/coq_kernel_check.sh`: 63 proofs;
- `tests/formal_semantics_smoke.sh`: PASS.

Logs: `.tmp/rocq93_ownership/memory-boundary-reuse-claude.log`,
`kernel-reuse-claude.log`, `formal-smoke-reuse-claude.log`. No compiler,
runtime, or installed-binary change; no commit, push, or GUI message.
