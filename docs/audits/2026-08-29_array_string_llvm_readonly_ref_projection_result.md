# ArrayString LLVM readonly-ref target projection result — 2026-08-29

Status: `LOCAL GREEN — PUBLICATION PENDING`

Exact base: `4a97f19a17a64e36a66b29747098a80811f11285` on
`origin/main`.

This audit records evidence for one reached executable consumer. It does not
own compiler semantics, registry status, or a successor rung.

## Result

The scalar LLVM program root now carries its one target-qualified
`DirectMirArrayStringAbiProjection` through routine, operation, and expression
rendering to the read-only ArrayString parameter load. The final load
cross-seals that projection against the program's admitted
`DirectMirScalarProgramArrayStringAbiFact` and derives its alignment from
`projection.storage.align`.

The old `load %pgy.array.string ... align 8` decision is deleted from the
consumer source. Generated LLVM still spells the canonical admitted alignment
`8`. C signatures and pointer forwarding are unchanged, and neither the LLVM
expression owner nor the read-only consumer derives a second projection.

## Falsifying evidence

- `direct_mir_scalar_array_string_readonly_ref_owner.sh` executes the same
  program through C and LLVM and checks equal output.
- It rejects read-only carriage, pass-shape, resource, type, ABI-required, and
  ABI-layout alignment drift. Every requested artifact remains absent on
  failure.
- The focused gate and component contract require root-to-consumer projection
  carriage, the final fact/projection/target cross-seal, and projected storage
  alignment. They reject a consumer-local projection and restoration of the
  old load-alignment literal.
- The final source owners remain within their existing caps: LLVM emission
  `360/360`, operation `165/165`, expression `360/360`, logical assignment
  `149/150`, member rebind `79/80`, Option-try `70/70`, and read-only consumer
  `64/80`. No cap was raised.

## Observed local gates

- final-source Pergyra-built DRV-2 generation: PASS; the final formatting-only
  rerun reproduced and reused the same fingerprinted driver
- production `driver_bootstrap_main.pgy` C emission: PASS, 11,348,955-byte
  artifact
- focused C/LLVM read-only parity and six negative mutations: PASS
- full self-host component contract: PASS
- SoT authority edge: PASS, `CLOSED=55 BRIDGE=32 ACTIVE=1`
- SoT live adequacy: PASS; Coq/Rocq explicitly declared skipped because no
  prover is installed on this runner
- single-owner and hard-contract gates: PASS
- likeness: PASS at the tightened `result_use=4501/4501` ratchet
- documentation quality and `git diff --check`: PASS

## Admission correction and remaining boundary

The first candidate was the LLVM owner-handle ArrayString parameter binding.
A temporary source-level executable probe showed that it is not a bounded
alignment-only rung: moving a local ArrayString into an owner-handle call lets
both caller and callee free the same storage, while direct fresh-return and
literal arguments are not yet admitted. That candidate therefore requires a
caller-side move-retirement fact and its own executable falsifier. The probe
was removed and no claim from it was counted as completed work.

The reached program-extension and ArrayString ABI registry rows now record the
read-only consumer, its focused execution evidence, projection-less fallback,
and literal LLVM load-alignment fallback. The ArrayString ABI row remains
`BRIDGE`: owned parameter binding, value-result transfer, owned return,
mutation, cleanup, and other expression materializers still require migration.
No `56/31/1` closure is claimed.
