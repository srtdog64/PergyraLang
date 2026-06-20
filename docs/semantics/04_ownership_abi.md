# 04. Ownership / ABI Proof Obligations

Last updated: 2026-06-21

Status: `IN PROGRESS / BLOCKER`

Keywords and surfaces: `own`, `ref`, anchored slot handles, slot boundaries, runtime ABI ownership.

## Stable Surface

- Anchored slot-handle own/ref subset.
- Slot as modular resource boundary: source code observes Slot contracts, not
  backend pointer/address ownership.
- Boundary-visible aggregate provenance.
- Movable value transfer/borrow where explicitly covered.
- Ownership diagnostics for destructure/member/container/return/channel/helper-chain paths.
- Arena discipline: scratch/result/persistent/runtime lanes.
- SecureSlot and authority token invariants for the stable anchored boundary subset.
- `Slice<T>` as a local borrowed view over an existing owner.

Out of beta:

- General ownership lattice.
- Non-anchored general value own/ref.
- Universal move semantics for every aggregate shape.
- Arbitrary runtime pointer ownership transfer.

## Judgments

```text
Gamma; ResourceState |- borrow(x) ok
Gamma; ResourceState |- move(x) => ResourceState'
Gamma; ResourceState |- release(slot) => ResourceState'
ABI |- returned_value owns lane
ABI |- scratch_value does not escape
Gamma; ResourceState |- secure_read(slot, token) ok
Gamma; ResourceState |- authority_use(zone, token) ok
```

## Theorem: Anchored Ownership Safety

Anchored slot-handle operations cannot observe a released slot, cross an invalid boundary, or duplicate a move-only resource without a contract violation or hard-fail.

Assumptions:

- Stable own/ref applies only to anchored slot-handle boundaries.
- Released slots and invalid tokens are invariant breaks or hard failures.
- General ownership is not accepted as stable beta surface.

Current evidence:

- Ownership classifier fixes the stable subset.
- Channel, destructure, member, return, container, and helper-chain ownership regressions exist.
- Non-anchored/general own/ref is an explicit reject or out-of-beta surface.

Remaining proof obligation:

- Finish ABI ownership seams for returned strings/helper payloads and runtime-owned values.

## Theorem Boundary: Slot Runtime Safety Is Not Borrow Safety

The ownership proof pack separates two claims:

- `Slot runtime safety`: generation checks, token checks, release state, and
  pin state make invalid runtime access reject or hard-fail.
- `Borrow-checker-equivalent safety`: ownership classifier, CFG/body dataflow,
  no-escape checks, cleanup insertion, and task/channel boundary rules prove
  invalid access is not accepted for the stable source subset.

The first claim is modeled by `docs/semantics/08_slot_capability_calculus.md`.
The second claim remains a section `0b` / section `4` beta blocker until CFG
facts cover no-escape, no-suspend, write exclusivity, and drop/release exactly
once on all relevant exits.

Public docs must not collapse these two claims. A runtime Slot check may be
excellent fail-safe behavior, but it is not a Rust-style borrow checker proof.

## Theorem: Secure Token Unforgeability

Secure slot tokens and authority-bearing tokens cannot be forged, copied into an unsupported trust boundary, or used to access a slot/zone authority boundary they were not issued for.

Required invariants:

- Token material is not constructible by source-level expressions outside compiler/runtime issuance points.
- Secure slot token mismatch cannot read, write, release, or otherwise mutate the protected slot.
- Authority-bearing tokens cannot be transported through unsupported channels or stored in stable beta containers unless the surface explicitly defines that transfer.
- Runtime snapshots and observability strings must not expose secret token material.

Current evidence:

- Secure slot read/write/pin/release paths validate token pairing.
- Raw `SlotRelease` cannot release secure slots; secure release requires
  `SlotReleaseSecure` after token validation.
- Stale-generation handles and revoked tokens are rejected by the runtime Slot
  capability tests.
- Forged zero-token read/write/release is covered for both inline C runtime and
  exported C/LLVM-linkable runtime entrypoints.
- SecureSlot token ABI is build-mode stable: debug/release inline C, exported
  runtime, and LLVM-linkable runtime all use the same `PgyToken<T>` layout with
  `can_write` and `can_read` capability bits. Release-mode inline secure-slot
  reads/writes/releases keep hard-fail token and occupancy checks, and the
  legacy release-mode SecureSlot macro has been removed; only plain `Slot<T>`
  has a zero-overhead release layout.
- The ABI spec carries matching debug/release SecureSlot layout rows for every
  stable primitive payload (`Int`, `Long`, `Float`, `Double`, `Bool`, `String`),
  and `make test-abi` checks runtime size and token offsets against that spec.
- Runtime authority failure surface exposes reason/code state for missing-zone,
  missing-participant, and authority-token-mismatch without exposing secret
  token material.
- Authority token mismatch has C/LLVM ABI and backend-compare coverage through
  `authority_failure_abi` and `authority_failure_surface`.
- Unsupported beta transport of authority-bearing `Token<T>` is rejected on
  blocking channel send/receive, non-blocking and timeout channel helpers,
  channel close, cancellation payloads, and direct named `spawn` boundaries.
- `runtime-authority-contract-test-smoke` and `runtime-abi-lifetime-test-smoke` guard parts of the runtime ABI vocabulary and borrowed-string lifetime surface.

Remaining proof obligation:

- Extend the same token-transport reject gate whenever a new beta transport
  surface is admitted.

## Theorem: Authority Transfer Single-Owner

Zone authority transfer and handoff cannot create two active owners for one authority boundary.

Required invariants:

- A handoff either materializes a new owner and invalidates the previous authority frontier, or fails with a recoverable authority/boundary state.
- A failed handoff cannot leave source and target both active.
- Projection and observability state after handoff must report the same authority owner on C and LLVM.

Current evidence:

- Handoff projection/world-state/layer-state frontier ABI cases exist.
- Authority guard snapshots and intent authority snapshots share the same runtime reason vocabulary.

Remaining proof obligation:

- Generalize handoff authority ownership beyond the currently covered frontier slices.
- Add explicit invalid-authority transfer tests for C and LLVM parity.

## Theorem: Arena Lifetime Non-Escape

Values allocated in a scratch arena cannot be returned, cached, or stored in long-lived runtime ABI state unless copied into a result/persistent lane.

Assumptions:

- Scratch, result, persistent, and runtime-owned lanes are documented per subsystem.
- Long-lived metadata stores stable indexes or owned copies, not scratch pointers.

Current evidence:

- Arena direction is fixed as `Arena + Index reference + lane-specific arena separation`.
- Several semantic and backend scratch/result paths have been split.
- Stable runtime string ABI exports are documented as `runtime-borrowed string`
  values: the caller must not free them.
- Intent observability string exports copy mutable registry values into
  thread-local borrowed snapshots. The returned pointer does not alias registry
  storage, but callers must consume or copy it before a later borrowed string query on the same thread can reuse that snapshot slot.
- Authority failure strings are thread-local runtime snapshots and are valid
  until the next authority validation updates that thread's snapshot.
- `runtime-abi-lifetime-test-smoke` verifies that stable intent
  last/history/active/recent and authority string export functions return
  borrowed runtime state and do not allocate/free/strdup in the ABI return path.
- Stable string helper returns are `result-owned string` values: the caller owns
  and must eventually release the returned pointer unless a higher-level Pergyra
  runtime owner consumes it immediately.
- Stable string-array helper returns are `result-owned array` values: the array
  shell and its string payload elements are copied into result-owned runtime
  memory, not borrowed from source inputs or map storage.
- Stable integer file descriptors are `runtime-owned handle` values: the caller
  receives a numeric handle, while the runtime owns the backing `FILE *` table
  entry until `pgy_file_close` releases that slot for reuse.
- `runtime-abi-lifetime-test-smoke` also verifies that result-owned string and
  string-array helpers allocate/copy their payloads and do not return string
  literals, stack buffers, or source input pointers.
- The same smoke verifies that `pgy_file_open` resolves paths through the same
  runtime path policy as `ReadFile`/`WriteFile`, releases the resolved path
  buffer before returning, reuses released runtime-owned handle slots, and that
  `pgy_file_close` clears the runtime table entry.
- File/string helper error exits must preserve ownership as well as success
  exits: resolved file paths are freed on every `pgy_read_file`/`pgy_write_file`
  failure path, and string helpers guard length arithmetic before allocating
  result-owned buffers. The lifetime smoke now checks these guard terms for both
  inline and LLVM-linkable runtime surfaces.
- Interactive input is also result-owned on both surfaces: inline `pgy_input`
  duplicates the stack buffer before returning, matching the LLVM-linkable
  export instead of returning a static borrowed buffer.
- Exported array slice helpers must not derive a backing pointer for zero-length
  slices and must use subtract-form range checks (`start > length || len >
  length - start`) so overflow cannot turn an invalid slice into an apparently
  valid pointer range.
- Generated C/LLVM `Slice<T>.Slice(start, len)` follows the same borrowed-view
  rule: it does not own or clone backing storage, it checks range with the
  subtract-form condition, it rejects non-empty null backing storage, and it
  returns a null-backed empty slice instead of deriving a pointer for length 0.
- `Queue<String>` stores result-owned string payloads, not borrowed input
  pointers. Generated-C inline queue push duplicates the string, and LLVM raw
  queue lowering routes `Queue<String>` through string-specific raw exports
  instead of generic pointer `memcpy`.
- LLVM-linkable `Channel<String>` follows the same message-payload transfer
  rule: send duplicates the input string into channel-owned storage, receive
  transfers that owned payload out of the channel slot, and channel destroy
  frees any unreceived pending payloads.
- `List<String>` uses the same string-specific raw export rule on LLVM: push
  and set duplicate incoming strings, get returns the list-owned borrowed
  pointer, and remove frees the list-owned element before shifting later
  entries. The generated-C inline string list exposes the same set/remove
  ownership behavior.
- `Set<String>` follows the same owned-copy rule on the LLVM raw path. Add
  duplicates string input, membership probes borrow only for the duration of the
  call, and remove frees the set-owned payload before tombstoning the slot so
  probe chains remain valid after deletion.
- LLVM raw `HashMap<K, String>` uses string-value-specific exports for the
  stable key subset. `MapSet` duplicates the value payload, `MapGet` returns a
  map-owned borrowed pointer, and `MapRemove` frees both the runtime-owned key
  and runtime-owned string value while preserving the tombstone probe chain.
- Raw `HashMap<K, V>` removal must also tombstone deleted key slots instead of
  clearing the occupancy flag. Keys are runtime-owned strings internally; a
  cleared slot would truncate the linear-probing chain and make later keys
  unreachable after a remove. The same tombstone rule applies to generated-C
  inline `HashMap` and `Set` specializations.

Remaining proof obligation:

- Extend the same lifetime gate to additional runtime-owned handles as they
  become beta-stable.

## Theorem: Slice Borrowed View Safety

`Slice<T>` is a borrowed contiguous view, not an owner. It may carry a raw
pointer in C/LLVM ABI lowering, but source-level ownership remains attached to
the backing owner (`Array<T>`, a pinned Slot view, or a host buffer with an
explicit ABI contract).

Required invariants:

- Creating a slice must not free, clone, or transfer the backing owner.
- `Array<T>.Slice(start, len)` and `Slice<T>.Slice(start, len)` must use
  subtract-form bounds checks: `start > length || len > length - start`.
- Empty slices must be null-backed so backends do not materialize pointer
  arithmetic for a zero-length view.
- `Slice<T>[index]` must go through checked runtime helpers in generated C and
  LLVM for the stable surface.
- `SliceCopy(Slice<T>) -> Array<T>` is the explicit stable escape hatch from
  a borrowed local view to an owned array snapshot. The copy is element-wise
  and follows the existing `Array<T>` runtime element ownership convention.
  For `Slice<String>`, the stable producer duplicates string payloads so the
  returned `Array<String>` follows the result-owned string-array policy.
- A slice cannot cross async/spawn/world boundaries as an owned value unless a
  later transport contract explicitly copies or pins the backing owner.

Current evidence:

- Inline and LLVM-linkable runtime surfaces define `PgySlice_<T>` as
  `{ data, length }` beside the owning `PgyArray_<T>` layout.
- C generated `Slice<T>.Slice(start, len)` uses subtract-form checks and the
  shared `out-of-bounds` panic class.
- LLVM member-call `Slice()` lowering emits the same subtract-form check,
  null-backing guard for non-empty views, and null-backed empty slice result
  before constructing the returned slice value.
- Runtime, generated C, and LLVM expose `pgy_slice_copy_<T>` for
  `SliceCopy`, and the backend-compare `slice_copy` fixture gates C/LLVM
  parity for materializing a borrowed view as an owned `Array<T>`. The
  runtime ABI lifetime smoke gate also locks the String specialization to a
  duplicate-on-copy path.
- Semantic ownership classification treats `Slice<T>` as `BORROW_TRACKED`, so
  ref-spawn, blocking channel send/receive, and non-blocking channel helper
  send/receive attempts are rejected until an owner/provenance proof is
  available. Rejected spawn/escape diagnostics name the value as a
  `borrowed Slice view`, name the backing-owner provenance, and point to
  `SliceCopy(view)` as the owned-snapshot escape hatch.
- `runtime-abi-lifetime-test-smoke`, `runtime-panic-contract-test-smoke`,
  `runtime-panic-codegen-test-smoke`, `perf-contract-smoke`, `test-semantic`,
  and `cfg-body-dataflow-test-smoke` gate those terms.

Remaining proof obligation:

- Promote valid slice boundary forms into AIR/CFG evidence only after a concrete
  owner/provenance contract exists. Invalid slice transport is intentionally
  rejected before AIR graph synthesis, so AIR is not the evidence owner for the
  current rejected-source surface.

## Theorem Boundary: Packed Layout Is Not Ordinary Mutability

Source-level bit packing, explicit field offsets, union overlap, and
niche-optimized `Option<T>` are not beta-stable language surfaces. Runtime ABI
structs may have frozen C layouts through `pgy_abi_spec.h`, but that is not the
same claim as giving users unchecked layout control for ordinary Pergyra
structs.

Required invariants before any source-level packed layout is accepted:

- A packed field must have a `LayoutFact` row that names storage unit, byte
  offset, bit offset, bit width, read mask, write mask, shift, extension
  policy, and read-modify-write effect.
- C and LLVM must consume the same `LayoutFact` row. A backend must not invent
  C bitfields, local mask/shift lowering, or target-specific packing as a
  compatibility fallback.
- `let mut` is local-storage mutability. It does not by itself prove that a
  partial-width field is addressable, borrowable, passable as `inout`, atomic,
  or safe to update through an implicit read-modify-write.
- `inout` / value-result mutation cannot be applied to a bit slice unless the
  layout/effect owner proves the whole writeback policy. A bit slice is not an
  ordinary mutable lvalue.
- Slot, SecureSlot, DeviceSlot, Pin, authority tokens, and capability handles
  are not packable until a dedicated layout owner proves their ABI, lifetime,
  and security invariants.

Future niche optimization follows the same rule. `NonZero<T>`, `NonNull<T>`,
`NonEmpty<T>`, or similar proof types must be established by semantic/DAG
analysis, carried as MIR ABI facts, and only then used to encode `None` in an
otherwise invalid bit pattern. A backend-local `Option<T>` size shortcut is a
miscompile risk, not an optimization.

Current evidence:

- `docs/semantics/pass_contract_manifest.md` marks backend-local layout guesses,
  backend-local option niches, packed-field addressability, and slot handle
  packing as forbidden reads/invalidated facts for the ABI layout row.
- `TODO.md` records the first implementation unit as a negative gate: reject
  any source spelling that tries to mutate, borrow, or `inout` a packed bit
  slice before the required layout/effect evidence exists.

Remaining proof obligation:

- Add the layout/niche specification, then add negative fixtures for
  `let mut` / `inout` / address-like packed-field access and C/LLVM ABI golden
  fixtures proving both backends consume identical `LayoutFact` rows before
  enabling source-level packed fields.

## Theorem: ABI Ownership Parity

C and LLVM must agree on who owns every stable runtime value returned through the ABI.

Current evidence:

- ABI same-process and backend compare tests cover many current runtime paths.
- Runtime-borrowed string exports for intent observability and authority failure
  snapshots now have an explicit smoke gate.
- Result-owned string and string-array helper payloads now have an explicit
  allocation/copy smoke gate.
- The file-descriptor runtime-owned handle table now has an explicit release and
  reuse smoke gate.
- Result-owned file/string helpers now have explicit failure-path release and
  allocation-size guard gates, reducing pointer lifetime drift between C and
  LLVM runtime surfaces.

Remaining proof obligation:

- Add explicit ownership assertions for runtime-owned handles beyond the current
  file-descriptor surface.
