# Graph allocation and external ownership composition

Status: `IMPLEMENTATION COMPLETE; bounded formal composition, production OPEN`.
Base: `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, preserved dirty main.
The user requested the missing allocation/growth connection proof after the
mixed-heap counterexample. This scope does not resume the held production
self-host rung, change ordinary-value semantics, or add runtime graph support.

## Objective card

- Objective: graph allocation and growth preserve the whole live heap split
  and cannot take storage belonging to another admitted owner.
- Priority: one heap vocabulary and graph transition owner, external ownership
  preservation, refusal before transition, legal dead-storage reuse, negative
  ratchet, then patch size.
- Fact owners: OwnershipCleanCore owns Block/free/ordinary ownership;
  OwnershipGraphLinks owns graph operations and their allocation admission;
  doc 28 owns composition requirements. HeapSplit in the typed audit relates
  the existing authorities, not a third heap or allocator.
- Last formal consumers: allocation/growth HeapSplit preservation and the
  existing retirement agreement. Production root/footprint issuance and C/LLVM
  consumers remain OPEN; this is not an installed-driver or SoT closure claim.
- Forbidden fallback: a whole-heap consumer using the graph-fragment gexec
  as an alternative to frame-aware admission; a missing frame treated as empty;
  copying external storage, hidden GC/RC, or assuming the desired disjointness
  as the postcondition instead of deriving it from the pre-transition checks.
- Gate: tests/memory_boundary_composition_smoke.sh, fresh Rocq 9.3.0 / rocqchk
  and existing two approved Slot abstractions only, then the full formal gate.
- Falsifiers: external live block chosen for a new table, node or grown table;
  duplicate candidates; old graph table reused legally; another store's live
  storage chosen; missing frame; growth under a borrow; live external values
  surviving grow followed by retirement.

## Complete reached chain and edit scope

The graph fragment consumes explicit adversarial allocator candidates through
ONew and OInsert. The three storage-adding branches are new store, vacant-slot
insert, and append/table growth. Existing bfree checks protect only gheap;
GInv certifies only graph stores. The canonical HeapSplit additionally carries
all other owners' live storage. A growth frees only its own prior table;
delete/drop only shrink graph storage; begin/end/write keep its footprint.

1. Add one mandatory frame-aware composition boundary to OwnershipGraphLinks.
   It consumes an explicit external footprint, refuses a missing footprint,
   checks only actually allocated candidates before calling the fragment,
   and retains the existing generation, duplicate, graph-live and borrow guards.
2. Prove fragment storage coverage, graph-invariant preservation and external
   disjointness for every admitted step, including a fixed-frame run.
3. In MemoryBoundaryCompositionAudit, derive HeapSplit for the actual new,
   insert and growth heap updates using the canonical free operation. Retain
   the unsafe fragment counterexample and require the composed route to refuse
   it without changing the input state. Prove positive reuse and retirement
   with the other owner still live.
4. Update doc 28 and navigation/receipt to distinguish the fragment, composed
   boundary and remaining production issuer/physical-placement obligations.

Independent edit scopes: this chat owns the graph model addition, typed audit,
its focused gate if needed, doc 28 and this receipt/handoff. No subagents or
parallel implementation tracks. Preserve Core, SlotCalculus, ABI, src/**,
installed executables, unrelated dirty changes and all existing proof APIs.

Integration owner: this chat. Static gates: 60 seconds; focused proof gate:
300 seconds; integration corpus: 1800 seconds. Use the repository's explicit
WSL OPAMROOT and scripts/run_rocq_toolchain.sh. Do not enlarge assumptions,
validation budgets or change the fixed counterexample to obtain green.
Outputs are bounded formal implementations and observed receipts, not
production admission, allocator refinement, performance or CI evidence.
No staging, commit/push, installation or GUI message in this scope.

## Observed receipt

At the 2026-10-08 integration boundary:

- Focused gate PASS: five fresh modules plus the approved API consumer,
  Rocq 9.3.0 / Stdlib 9.2.0 and rocqchk. Graph frame and allocation split
  theorems are closed under the global context; the composed Slot retirement
  retains only the existing two approved abstractions. No new axiom, admit,
  unsafe fixpoint or assumed positivity. Log:
  `.tmp/memory-boundary-composition/kernel.log`.
- Full formal inventory and fresh 63-module corpus PASS with the same two
  abstractions and approval consumer. Shared core modules are reused, not
  68 independent models. Log:
  `.tmp/memory-boundary-composition/allocation-formal.log`.
- External live-block choices are refused unchanged in all three adding
  branches. Missing-frame, duplicate payload, table/payload overlap, live
  graph storage, another store's table and borrowed-growth cases refuse.
  An unused table candidate does not prevent a legal vacant-slot insert.
- Positive new/vacant/growth consumers derive the actual whole-heap updates.
  Growth reuses its own old table immediately; the resulting canonical
  invariant feeds retirement_agrees_split. framed_growth_retirement_executes
  observes graph drop, canonical texec and Slot Step_Release, leaving the
  other owner's block and value. The Slot release premise is not fabricated.
- Guard mutation rejected as expected: only a scratch copy of gexec_framed
  had its external bfree admission removed. The fresh kernel gate returned
  nonzero at external_allocation_refused, unable to establish the unchanged
  RNotFree state. The real source was not mutated. Log:
  `.tmp/memory-boundary-composition/allocation-guard-mutation.log`.
- Documentation quality, ABI ownership shape and scoped patch whitespace
  passed. Existing nested-inductive, deprecation/masking/loadpath warnings
  and the full corpus's theory dependencies remain visible in the logs.

Pinned SHA-256 inputs:

| Input | SHA-256 |
|---|---|
| OwnershipCleanCore.v | `57c55889218b1f27075105d21573eb060b1709bdae2aacae756e7729c2ad15ce` |
| SlotCalculus.v | `86d344846150d965122a11afaad09a5aaea7a69fa9aaa03dc59281de05a242cd` |
| OwnershipGraphLinks.v | `035bf8a85705c075cd7a3ee2276b9ba9aafd91d634a91dcbaaebabab45a62eac` |
| MemoryBoundaryCompositionAudit.v | `20f1866a327405a31ef1db51766b021e92e103af54749a3d4377f079d70508e5` |
| Focused gate | `2a08dc685237a1d421ccbd3ea54d6329a3d2f83e66fe382f73dc4e825d78dd2d` |
| Doc 28 | `2ed409afe699970199eb299381f0ec7fec12ef6f2a0e7dabc62886c12a1fbab6` |

Core and SlotCalculus were preserved in this scope; the Slot hash already
superseded the earlier reuse receipt at entry. HEAD is unchanged at the base,
173 dirty status entries and an empty index at the receipt. Eight material
scope files: GraphLinks, the audit, focused gate, doc 28, semantics README,
doc 102, this directive and handoff. Unrelated/shared changes remain dirty.

Next falsifier: production issuance must certify that the external footprint
is exactly the other live owners in the canonical heap, and carry the same
snapshot through actual allocation/growth and retirement. Physical address
placement injectivity, copy-before-free, failure atomicity, source graph
semantics and C/LLVM remain OPEN. No runtime/compiler/ABI edit, installed
artifact, remote CI or SoT/self-host substitution is claimed.
