# 2026-11-05 raw-pointer ownership Rocq feasibility check

Local repository HEAD: `60fd1e26d8ea3987efbcbfc65f59b892b234701a`.
The shared worktree was dirty; this record does not claim a committed source
snapshot. Proof source:
`docs/semantics/proofs/RawPointerOwnershipClean.v`, SHA-256
`1762df307d7d80ecee73048f0f259cde86f0c2c46661b4a703a7960d081f322a`.

Toolchain: Rocq 9.3.0, `rocqchk` 9.3.0, Stdlib 9.2.0, OCaml 4.14.2,
from the isolated WSL C opam root
`/home/c/.opam-pgy-rocq-20261105`. The repository's
`scripts/rocq_toolchain_owner.sh` admitted this exact pair.
The isolated opam root was removed after these checks; it is not a persistent
repository toolchain or a change to the main compiler executable.

Observed gates:

1. `pgy_rocq_check_isolated` on the proof source: `PASS`; one proof compiled
   and kernel-checked; no declared axioms, admits, or unsafe kernel features.
2. `tests/coq_kernel_check.sh` on the fresh full proof corpus: `PASS`; 88
   proofs kernel-checked plus the approval binding consumer; the existing two
   approved Slot abstractions retained their pinned types; no admits or unsafe
   kernel features. The gate's source snapshot included the proof SHA above.

The checked theorems demonstrate a conditional owner/scope design for View
and owned raw-pointer cleanup. They do not establish a production provenance
issuer, compiler elaboration, C/LLVM release, or a whole-language safety claim.
`SlotRawPointer` remains rejected by the current language contract. The next
falsifier is an installed-driver C/LLVM program that allocates an owned raw
resource, uses View and scoped raw access, then takes normal and early exits
with exactly one observed physical release and preserved refusal controls.
