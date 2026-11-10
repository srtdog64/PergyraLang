# 2026-11-05 raw-pointer trace and canonical-heap refinement

Repository HEAD: `60fd1e26d8ea3987efbcbfc65f59b892b234701a`, with the
preserved shared dirty worktree. No compiler executable or installed driver
was changed by this proof slice. Sources checked in fresh gate snapshots:

| Source | SHA-256 |
| --- | --- |
| `RawPointerOwnershipTrace.v` | `eaa0c0c41fe3a4254eaed617c977050e2e00e48f669c416b441744d5617b522b` |
| `RawPointerOwnershipClean.v` | `1762df307d7d80ecee73048f0f259cde86f0c2c46661b4a703a7960d081f322a` |
| `OwnershipCleanCore.v` | `57c55889218b1f27075105d21573eb060b1709bdae2aacae756e7729c2ad15ce` |

The existing WSL C toolchain at
`OPAMROOT=/home/c/.local/share/pergyra-rocq/opam` was used through
`scripts/run_rocq_toolchain.sh`. Rocq and rocqchk reported 9.3.0; Stdlib was
9.2.0; this existing Rocq was compiled with OCaml 4.14.1. No installation was
performed in this slice.

## Observed verification

1. The focused `tests/coq_kernel_check.sh` on the three sources above passed:
   three modules freshly compiled and kernel-checked, axiom budget zero,
   no admits or unsafe kernel features. The canonical core emitted its
   existing nested-list automation warnings; proof checking succeeded.
2. `tests/formal_semantics_smoke.sh` passed. Its full kernel gate checked
   89 modules plus the approval export/binding consumer, retaining the same
   two approved Slot abstractions and their types. No admits or unsafe kernel
   features were reported. The wrapper counted 77 source proof files.
   Log: `.tmp/raw-pointer-strengthening-20261105/formal.log`.

## Strength of the result

The new model proves affine ownership issuance from allocation for arbitrary
finite successful instruction traces. Owned raw-pointer copy is refused;
move removes the old binding; borrowed alias copy creates no owner. Its body
instruction set contains no Drop. Automatic local-exit cleanup is total from
every reachable state, including active View/raw scopes, and leaves one
release, no owner, and no admitted access or repeat cleanup.

`checked_trace_refines_core_ownership` constructs the corresponding canonical
core ownership execution. `automatic_exit_refines_core_drop` proves the
release preserves the live blocks of other core owners. The strongest closed
theorem, `closed_raw_program_frees_canonical_heap`, starts with an allocated
core footprint and ends with empty owned bindings and an empty live heap.
The initial core invariant and correspondence are constructed, not axioms.
An admitted witness combines View and raw access, borrowed alias copy, two
owner moves, and exit from an open raw scope. Separate negative theorems
cover shallow owner copy, missing cleanup, expired borrow, and moved-owner use.

This is a one-local-allocation, finite-trace ownership-event refinement.
Machine loads/stores, pointer arithmetic, source/CFG-to-instruction lowering,
production provenance issuance, runtime pin/unpin, FFI, concurrency, and
physical C/LLVM allocator behavior remain outside it. The production raw
escape contract still refuses `SlotRawPointer`; these proofs do not close
V1, ship v0.05, or establish uniqueness among languages.
