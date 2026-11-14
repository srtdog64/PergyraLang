# Raw-pointer ownership cleanup: Rocq feasibility model

This is the proof sketch for the [v0.05 decision](../../truth/decisions/2026-11-05_v0_05_sot_and_unsafe_ownership_scope.md).
The Region operations are in [`RawPointerOwnershipClean.v`](RawPointerOwnershipClean.v).
[`RawPointerOwnershipTrace.v`](RawPointerOwnershipTrace.v) proves affine
issuance, arbitrary finite checked traces, total automatic local-exit cleanup,
and refinement of ownership events to the canonical cleanup heap.
[`RawViewMemoryBridge.v`](RawViewMemoryBridge.v) adds checked scalar contents,
offset loads/stores, alias visibility, frame preservation and memory-trace
erasure to that ownership machine. The production connection is a checked
MIR projection plus native execution, not a proof of the whole compiler.

## Objective card and whole chain

- **Objective:** keep one owner of an allocation while a typed View or a raw
  pointer accesses it; synthesize one release after the final legal use on
  every admitted exit, with no user-written owner restoration.
- **Priority:** allocation identity and one owner; scoped access; transfer;
  fail-closed missing evidence; actual release; C/LLVM parity.
- **Fact owner:** allocation provenance and current holder must be admitted
  compiler facts, carried from the source/semantic boundary to MIR. The model's
  `region_epoch` and `region_holder` are those facts, not bits inferred from
  the address. The concrete producer for public raw-pointer values is still
  **OPEN**; `unsafe(raw)` and `SlotRawPointer` are not currently admitted.
- **Last consumer:** ownership cleanup elaboration at each normal, break,
  return, and error exit, then C/LLVM lowering of the admitted MIR release.
- **Forbidden fallback:** address equality as proof of ownership; a borrowed
  raw alias as a second owner; auto-free of an opaque foreign pointer; manual
  deep-drop or wrapper restoration as the default; separate backend guesses.
- **Falsifiers:** stale pointer after address reuse; borrow reused in a later
  `unsafe(raw)` scope; old owner used after transfer; release while View/raw
  access is live; duplicate release; opaque pointer treated as owned.

The concrete chain to close later is source allocation/Slot or declared FFI
handoff → semantic capability and provenance admission → stable owner and
scope facts in MIR → cleanup elaboration across all CFG exits → one C/LLVM
release operation → actual allocator/runtime observation. The current
`OwnershipCleanCore.v` and `OwnershipCleanViewScope.v` cover the general
ownership machine and a bounded static View scope. The new trace model imports
the canonical core: owner moves execute its `TMove`, and the final release
executes its `TDrop` under the same disjoint heap invariant. The concrete
production MIR and load/store refinement is **OPEN**. It must not be replaced
by a document claim or by re-reading source text in a backend.

## What the model establishes

### Trace strengthening (2026-11-05)

The trace proof consumes the existing Region operations. Its boundary is one
closed local allocation through an arbitrary finite checked instruction trace,
with an affine pointer store. Allocation issues one owner, move removes the
old binding, a borrow never grants release authority, and copying an owned
pointer is refused. `arbitrary_checked_trace_preserves_owner` proves that
every successful trace retains exactly one current owner. Body instructions
contain no Drop; `automatic_exit_is_total` closes active View/raw scopes,
consumes the stored owner, and releases once for every modeled exit kind.

`checked_trace_refines_core_ownership` produces a canonical-core execution for
every checked body trace. `automatic_exit_refines_core_drop` then proves that
the core release preserves every other owner's live blocks. The strongest
closed theorem, `closed_raw_program_frees_canonical_heap`, starts with a real
core allocation footprint `alloc epoch (vsize value)` and ends with an empty
live heap and no owned binding. Its allocation correspondence is constructed
by `initial_raw_core_link` and `initial_raw_core_inv`, rather than assumed as
an axiom. This is ownership-event and abstract-heap refinement; it does not
give machine-load semantics to a raw access instruction. Scope metadata and
access admission project to ownership-neutral core steps. Runtime pin/unpin,
loads, stores, and source-to-instruction lowering still need their own bridge.

The admitted witness uses View access, raw access, copied borrowed aliases,
two owner moves, and automatic exit while a raw scope is still open. Its
corollary executes the canonical heap cleanup for every modeled exit kind.
Negative theorems show that copying the owner would violate affinity,
omitting exit cleanup leaves the allocation live, moved bindings cannot be
used, and stale-scope borrows are refused. The focused gate kernel-checks the
imported core and both raw models together; the full gate includes them too.

`RawPointer` has an address and allocation epoch, while `Region` has a live
state, one holder, a View lease, a raw scope generation, and a release count.
The role of a pointer is owned, borrowed, or opaque. An owned raw result may
carry the holder obligation; a borrowed alias retains its holder and the
issuing scope generation but cannot release the allocation. An opaque pointer
has no compiler-owned release obligation. `unsafe(raw)` admits raw access; it
does not manufacture ownership.

The Rocq theorems show that this small machine can:

- run View access, then raw access, then compiler-inserted cleanup for each
  modeled exit, with one release and no second release;
- block cleanup while a View or raw scope is live;
- reject an old pointer after allocation-epoch change, even if its address is
  reused;
- reject a borrowed raw pointer in a later raw scope, even when the owner and
  address are unchanged;
- revoke old-owner access and release after transfer; and
- refuse release through borrowed or opaque pointers and refuse a raw scope
  that lacks the capability or an owner.

The trace extension proves the model's insertion and ownership-event rules;
production compiler insertion still needs a refinement proof. It does not
model pointer arithmetic and bounds,
casts that lose provenance, foreign mutation, reentrant FFI, concurrency,
panic unwinding, allocator reuse behavior, or the production provenance
issuer. The trace model proves affine issuance from its allocation entrypoint,
but `Owns` remains a ghost fact whose production issuer and complete alias
coverage must be established by the compiler. A release counter alone does
not prove that a hidden alias is safe. The live/epoch access guard in the model also
needs either a static proof or an actual runtime check; it is not a free
property of a machine pointer. These obligations require separate owner
facts, semantic negatives, and physical release tests before v0.05 can claim
this property.

## Verification boundary

The repository's proof gate copies every `.v` into a fresh snapshot, compiles
it with the pinned Rocq 9.3.0/Stdlib 9.2.0 toolchain, and rechecks it with
`rocqchk` under the existing assumption budget. Run
`tests/proof/coq_kernel_check.sh` for that gate. A local compile with another Coq
version is only a syntax/proof development aid; it is not this gate.
The [2026-11-05 run record](../../truth/evidence/2026-11-05_raw_pointer_ownership_rocq_feasibility.md)
contains the pinned focused and full-corpus results for this proof source.
The [trace/core refinement run](../../truth/evidence/2026-11-05_raw_pointer_trace_core_refinement.md)
records the stronger trace theorems, the imported-core gate, and the full
formal-semantics result using the existing project toolchain.

The [compiler/runtime bridge record](../../truth/evidence/2026-11-05_raw_view_memory_bridge_compiler_runtime.md)
connects an admitted Array owner and Slice View to actual internal raw
load/store, MIR binding IDs and retirement ABI, the Pergyra MIR reader, and
native C/LLVM allocation/free observations. Its importer reads graph facts;
it never obtains ownership from identifier text. Kernel-checked concrete
branches check results, while the general memory theorem establishes
in-bounds access, same-cell alias visibility, unchanged other cells, affine
ownership and one cleanup. Integer projection requires no overflow.

This route uses the runtime's existing View implementation. Entering the
model's raw region represents those internal accesses; it does not establish
a public raw capability or prove a production capability issuer. Public
`SlotRawPointer`, arbitrary pointer arithmetic, foreign writes, concurrency,
installed-driver replacement and whole-compiler refinement remain separate
obligations. The next falsifier is the same admitted program through rebuilt
installed C/LLVM drivers, followed by a separately specified public raw ABI.

The model establishes **conditional feasibility**, not that no other language
can do this. Comparative claims require separate evidence about actual
implementations and developer-visible obligations.
