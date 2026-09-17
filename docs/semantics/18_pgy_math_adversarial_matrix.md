# 18. PgyMath/Pergyra Adversarial Matrix

Last updated: 2026-09-17

Status: `measured-open-boundaries`

Executable gate:

```text
PGY_MATH_ROOT=<checkout> make pgy-math-adversarial-matrix-test-smoke
```

## Objective card

- Objective: attack every trust seam between submitted mathematical contract
  IR, Pergyra source, compiler facts, backend artifacts, and runtime values.
- Priority: semantic mismatch and stale-certificate acceptance first, identity
  collision and ABI lifetime second, specification quality and resource limits
  third.
- Fact owners: PgyVerify owns submitted affine `Int` contract meaning; Pergyra
  source/MIR owners own executable semantics; a future production certificate
  owner must bind the two plus compiler, verifier, toolchain, ABI, and backend
  artifact identities.
- Last legitimate consumer: backend publication immediately before the exact
  executable artifact is emitted.
- Forbidden fallback: treating backend parity as mathematical equivalence,
  treating a theorem name as its proposition identity, or treating a
  manifest-only layer as digest-bound evidence.
- Gate/falsifier: every finding is `CLOSED`, `OPEN`, or `UNMEASURED`. The gate
  fails if an item disappears or silently changes class. Closing an item
  requires updating both its implementation and exact expected inventory.

## Current measured split

PgyMath's machine-readable verifier audit covers source/build/toolchain replay,
mathematical integer semantics, semantic tautologies, theorem identity, strict
JSON, canonicalization, downgrade refusal, and per-request limits.

Pergyra's dynamic matrix covers native/self-host C/LLVM on signed integer
boundaries, namespace flattening collisions, registry shadowing, scalar
`ToString` stress, registry forgery, and source/AIR/MIR binding mutation. It
also pins the still-open ABI/backend manifest-only and production-consumer
seams.

An `OPEN` row is a reproduced trust gap, not a test failure hidden as green.
The aggregate command passes only when the complete expected open inventory is
still visible. `UNMEASURED` means no valid oracle exists on this host; current
Windows runs do not claim leak-sanitizer or global verifier process-budget
coverage.

## Promotion order

1. Define Pergyra `Int` verification semantics: checked `Int32`, wrapping
   `Int32`, or mathematical `Int` with an executable big-integer owner.
2. Add production source-to-normalized-IR binding and consume it immediately
   before backend publication.
3. Digest ABI facts, backend consumption evidence, and emitted artifacts.
4. Bind compiler, verifier, Lean toolchain, and elaborated theorem proposition
   identities.
5. Add semantic specification-liveness checks and a process-global verifier
   concurrency/orphan-process budget.
6. Run leak-enabled emitted-code sanitizers on a supported Linux/WSL toolchain.
