# Ownership cleanup: verified composition

Status: IMPLEMENTATION COMPLETE for this bounded proof/extraction slice;
production refinement and SoT closure are not claimed. Base:
`3658548d24bca3d721e4f1974ac7a10da99f7aa8`, preserved dirty main.
Canonical core input SHA-256:
`f8ac63cf9a9a47389863761b3c84a49279b7e683f807956950844bc3d0d69d00`.

The user's current request explicitly opens Rocq improvements following the
research review and asks whether monad-like branching can reduce effects.
This bounded proof/extraction slice supplements the C/G split; it does not
open a parallel compiler implementation or transfer ownership of C2/C3.

## Objective card and complete affected chain

- Objective: prove safe sequential/conditional composition on the existing
  ownership machine; remove redundant `Skip`/sequence nodes from extracted
  cleanup output without removing observable or resource effects.
- Priority: exact semantic preservation, fail-closed counterexamples,
  executable extraction coverage, then representation size. This is a
  user-requested formal investigation, not a measured runtime optimization.
- Fact owner: `OwnershipCleanCore.elab` and `texec`. The new composition file
  imports them; it defines no second heap, ownership machine, or copy policy.
- Chain: canonical elaboration -> proved target normalization -> extraction
  -> existing observer/control runner -> hash-bound receipt. The production
  MIR/backend paths are not consumers of this new pass yet.
- Last legitimate consumer: the existing extraction smoke/receipt, with a
  fresh kernel check of its imported proofs. C/LLVM integration remains open.
- Forbidden fallback: erasing a guard just because its arms agree; treating
  branch arms as sequential; idempotent `Drop`; deleting `Emit`; hand-written
  OCaml ownership inference; old observer signatures or old green receipts.

## Ordered edit scope and acceptance

1. Add `OwnershipCleanComposition.v`: unit/associativity laws, guarded branch
   distribution, recursive `Skip` normalization, exact `texec` equivalence
   (including final heap/environments/allocation counter/trace), inherited
   closed-program cleanup, no increase in syntax nodes, and counterexamples.
2. Update only the extraction observer to forward the current explicit mode
   table and four-field compiled routine. Exercise borrow and sink modes,
   live-source reuse and multi-round call-summary propagation.
3. Consume the composition pass in the same smoke gate and hash binding.
   Compare syntax counts separately from copy/drop sites; do not call syntax
   reduction a reduction in allocations or observable effects.
4. Register the imported proof and document the verified boundary in doc 207,
   the research audit and this session's handoff card. Preserve other cards
   and concurrently edited semantic/C2 documents.

Independent edit scope: composition proof, extraction/OCaml observer, its
smoke/receipt wiring, documentation owned by this slice; canonical core and
production compiler/runtime are read-only. No delegation, new worktree,
installation, commit, push, GUI message or SoT status change in this slice.

Integration owner: this task. Integration gate:
`tests/ownership_cleanup_smoke.sh` plus the fresh full proof-corpus kernel
gate and formal inventory. Static budget 60 s, focused 300 s, corpus 1800 s.
Tests are implementation candidates until actual receipts are observed.

Falsifiers: invalid condition with two empty arms, both arms dropping the
same owner (valid exclusive branch but invalid sequence), duplicate emits,
copy of a still-live source, borrowed drop, and a three-callee chain for
which one inference round is insufficient. No generalized effect inference,
closure borrow-elision, early-exit or physical-runtime theorem is claimed.

## Reached validation bottleneck

The first updated smoke passed the fresh three-module kernel and all 79
observer controls, then exceeded its unchanged 300-s budget. A scoped process
check found the receipt's whole-repository `git status` child waiting on the
Windows mount (155 s observed, 2% CPU), not the extracted benchmark process.
The 30 fixed workload recipes and repetitions are unchanged. The receipt now
collects status only for its seven hash-bound inputs, with an explicit scope
field and 20-s metadata limit. It still rejects changed input hashes before
and after measurement; no semantic gate, cost input, or allowance was weakened.
Repository-wide dirty state is separately observed with the host Git at handoff.
The unrelated direction-residue scan also timed out at its 60-s budget;
its previous green receipt is not presented as a current pass.

## Observed acceptance

- `OwnershipCleanCore` remains f8ac63cf9a9a...; the new composition is
  e12836730e707... . No canonical ownership rule or C2 fact was overwritten.
- Exact `texec` equivalence and refusal/copy-site propositions are consumed
  by the extraction file's typed contracts, not just by theorem-name checks.
- Final focused gate: three fresh modules kernel-checked with zero axioms;
  15 decision + 24 refusal + 40 sink/composition controls; all 30 fixed cost
  cases. Receipt `pergyra.ownership-clean.elab-cost.v3` binds seven inputs.
- Full formal gate: all 59 proof files plus approval binding consumer PASS;
  the two existing SlotCalculus abstractions remain the corpus assumption
  budget. No admits/unsafe kernel features.
- Four concrete semantic controls refute guard erasure, distinguish exclusive
  and sequential drop, and refute idempotent Emit. Normalization preserves
  heap, environments, allocation frontier and trace, not just printed output.
- Direction residue scan is explicitly UNVERIFIED on this rerun (60-s timeout).
  No worktree, commit/push, installed-driver or GUI mutation occurred.

Logs and exact hashes are in the follow-up section of
`docs/audits/2026-10-08_ownership_cleanup_recent_research.md`. The next proposed
proof boundary is read-only local-loan elision, not a new user-facing monad
syntax or a claim that Skip normalization reduces allocations.
