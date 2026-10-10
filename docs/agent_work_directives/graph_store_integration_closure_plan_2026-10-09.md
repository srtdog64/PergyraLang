# Graph store integration closure plan

Status: **REVIEW COMPLETE; production implementation OPEN**.
Base: `main @ 85fff5aa339ae136ac0797c4a6a95a5285ada70e`, with the existing
uncommitted graph proofs, consumers and publication changes preserved.
Date: 2026-10-09 KST.

This is coordination, not a semantic owner or a successor self-host rung.
The user selected optional store-local reclamation, requested independent
multi-model review, and added snapshots, cache behavior and owner-directed
deletion to the review. The current ownership-cutover P1/DRV-2 stays active.

## Shared objective card

- Objective: connect graph access, ownership and retirement without turning
  ordinary automatic cleanup into tracing GC or requiring manual deep-drop.
- Priority: semantic observations and one owner; complete roots and authority;
  failure-atomic retirement; finite reuse safety; measured cost; patch size.
- Fact owners: docs 27/28, canonical cleanup and teardown authority. Final MIR
  analysis/layout owners must produce the root evidence; register the actual
  production binding/root fact owners before introducing those fact families.
- Last consumers: C/LLVM emission and installed self-host routes, then the
  runtime's physical retirement and allocator reuse publication.
- Forbidden fallback: guessed roots, caller-supplied authority maps, missing
  count as zero, unchecked cached admission, hidden whole-store recount,
  hidden RC/COW, and a native prototype reported as compiler integration.
- Current integration gate: `tests/graph_cycle_reclaim_smoke.sh`, with the
  independent consumer and its selftest. This checks models, not production.
- Falsifying cases: borrowed last candidate after an earlier deletion; a
  full-inventory snapshot observing a supposedly dead node; omitted caller,
  aggregate, view, return or handled-error root; foreign/root-reused binding.

## Review roles and edit boundaries

GPT-6 Astra reviewed algorithm selection and research, GPT-6.1 Sol traced the
compiler/runtime chain, and GPT-5.6 Sol reviewed adversarial boundaries. All
three were read-only. The main task independently compiled the falsifiers and
is the integration owner. No worktree or parallel implementation lane is
opened. Results and the resolved counter-policy disagreement are in the
[review](../audits/graph_store_policy_snapshot_review_2026-10-09.md).

Main-owned changes in this slice: review/coordination documents, the bounded
policy section of doc 28, the independent graph consumer, its focused gate and
selftest, and the active handoff note. The reached proof indexes (semantics
README and doc 102) still projected policy adoption as OPEN; update only those
bounded projections and the new falsifier scope to agree with doc 28. This is
not another semantic owner or a general index rewrite. Do not overwrite the existing model,
publication/CI edits or `gmon.out`. AGENTS.md is outside the edit scope.

Allowed commands: read-only Git/source/Actions inspection; `apply_patch` in
the listed scope; isolated proof snapshots through the admitted Rocq runner;
focused proof/selftest and documentation gates. No shared process-name kill,
official binary replacement, source reset, history rewrite or publication is
part of this review slice. Static checks budget 60 s, focused checks 5 min,
integration shards 30 min. A timeout is incomplete evidence, not acceptance.

## Ordered closure, not independent implementation tracks

1. **Finish the existing ownership cutover boundary.** Use doc 27 and its
   current directive to admit call/output/view ownership and cleanup facts.
   Do not add graph-specific hand-written restoration or drop rituals to
   circumvent that producer. Preserve the actual P1 gates and memory cap.
2. **Connect the real store and authority.** Creation issues an opaque binding
   among store incarnation, canonical root epoch/holder, node identities and
   physical footprints. Transfer/consumption update that single owner.
   Copied links grant no retirement authority. Keep node payload blocks and
   store-owned slab/table blocks distinct; check external footprints at
   allocation/growth. Do not fabricate a second forest/lease ledger.
   Account for graph stale incoming edges versus canonical teardown's cleared
   fields through one edge owner or an explicit observational refinement,
   including duplicate edges and raw inventory observations.
3. **Land the baseline lifecycle first.** Domain-directed node removal uses
   the canonical retirement unit and whole-unit loan/pin preflight. Compiler-
   owned store-end cleanup retires the remainder once. Deleting too early may
   cause an explicit stale-link result; it is not preservation of reference
   program behavior. Forgetting deletion retains until owner end, not UB.
   The baseline does not need optional-collector reachability roots or count
   maintenance; do not make completing those prerequisites for baseline use.
4. **Define observable inventory and snapshots before adding reclaim.** A
   strong key directory, enumeration or full-store snapshot can make nodes
   observable without external node links. Preserve them until domain
   removal, or specify a separate logical/weak surface; never silently narrow
   full-store copying to root-closure copying. Detached snapshots require
   stable source, fresh destination identity/storage/authority, admitted
   copyable payloads, independent relative links and atomic publication.
   Failure cleans the partial destination, not the source. A proof-evidence
   snapshot is not a payload copy. No authority/lease/affine bit-copy.
5. **Issue and check actual reclaim roots.** After final MIR transformations,
   connect instruction-level liveness, phi predecessor uses, suspended caller
   frames, recursively typed aggregate/enum/capture layouts, view backing,
   results, all inout restorations and handled-error packets to checked
   certificates, including the observable inventory contract from step 4.
   Register arguments before a callee checkpoint; publish result/error/inout
   handoff roots before retiring its frame. No interval may leave both sides
   unregistered. Generation-bind the certificates and backend consumption.
6. **Integrate the optional counted policy.** Recommendation: choose policy at
   creation, keep OFF stores free of collector-specific bookkeeping, and
   maintain ON counts exactly from the initial empty graph. First proposed
   implementation profile is a new flat-owned graph store with singleton
   node retirement units, not a flattening of existing Qt-style ownership.
   Hierarchical integration must select disjoint exact units wholly inside
   garbage; it may neither expand through rooted children nor orphan them.
   Preflight the whole batch and stable root/count/store/lease/authority
   generation; then use an allocation-free, callback-free, non-refusing
   commit and publish reuse last. Keep the store's root right while it lives;
   a per-batch internal authorization must not consume that entire right.
   Checked count arithmetic refuses overflow/underflow before mutation.
   Exhausted IDs/generations never wrap; cleanup must still retire payloads,
   while exhausted identities become permanently non-reissuable. A saturating
   Unknown counter is a different proof contract, not an exact-count fallback.
7. **Close the actual source-to-runtime chain.** Run the same source cases on
   C/LLVM and the installed self-host routes, with allocation failure at each
   admission/publication point, wrong holder/epoch, nested loan/pin, duplicate
   edges, stale views/certificates, exact-width counter exhaustion and reused
   physical blocks. Compare baseline and optional-policy observations; do not
   confuse root preservation with full-inventory preservation. Native memory
   checks, installed-driver evidence and exact candidate-SHA CI are required
   before implementation closure. Delete any development bypass and ratchet
   its absence. The current model gate cannot replace these obligations.

## Cost acceptance at the same integration boundary

No separate optimization track is opened. When the production profile is
executable, compare ON/OFF on the same packed layout and semantic workload;
separately compare layout choices. Include cyclic churn, long-lived stores,
strong cache hit/eviction, sparse inventory and repeated immutable readers.
Record retained/peak bytes, metadata and copied bytes, edge-update cost,
actual node/edge visits, throughput and tail teardown/reclaim latency.
Record hardware counters only when available, rather than inventing cache
measurements. Include root-production cost in compilation/runtime attribution.

`fuel` rounds are not a work/latency bound. The proposed runtime budget must
charge actual visits and scratch space, with explicit unchanged-state
deferral. No broad performance superiority is established. If cost blocks
the next named closure gate, follow the existing closure-blocking policy at
that owner and fixed input; do not enlarge its budget to claim success.
