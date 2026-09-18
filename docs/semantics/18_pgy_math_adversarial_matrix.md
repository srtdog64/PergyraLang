# 18. PgyMath/Pergyra Adversarial Matrix

Last updated: 2026-09-18

Status: `measured-open-boundaries / clean-cross-repo-replay-blocked`

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
- Fact owners: `docs/semantics/11_arithmetic_ub_model.md` owns ordinary Pergyra
  `Int` as wrapping signed 32-bit arithmetic
  (`pergyra.int.wrapping.i32.v1`). PgyVerify currently owns submitted contracts
  over Lean's unbounded mathematical `Int` (`lean.int.unbounded.v1`). A future
  production certificate owner must carry the selected numeric-domain identity
  and bind it with compiler, verifier, toolchain, ABI, and backend artifact
  identities.
- Last legitimate consumer: backend publication immediately before the exact
  executable artifact is emitted.
- Forbidden fallback: treating backend parity as mathematical equivalence,
  treating a theorem name as its proposition identity, or treating a
  manifest-only layer as digest-bound evidence.
- Gate/falsifier: every finding is `CLOSED`, `OPEN`, or `UNMEASURED`. The gate
  fails if an item disappears or silently changes class. Closing an item
  requires updating both its implementation and exact expected inventory.

## Current measured split

An earlier machine-readable PgyMath report claimed coverage of
source/build/toolchain replay, mathematical integer semantics, semantic
tautologies, theorem identity, strict JSON, canonicalization, downgrade
refusal, and per-request limits. The pinned verifier tests visibly cover a
bounded subset including identifier injection, duplicate keys, noncanonical
integers, tautological specifications, timeout inflation, and request budgets.
However, a clean checkout of the admitted PgyMath commit
`74185a3ad8c54711f3c2e30597ab0a05603a79a6` does not contain the
`verify/adversarial-audit.mjs` aggregate consumed by this repository's gate.
The previously recorded PgyVerify `CLOSED=7 / OPEN=6 / UNMEASURED=1` report is
therefore historical evidence, not a clean-checkout reproduction at the pinned
commit. The cross-repository aggregate remains blocked until that owned audit is
published by PgyMath.

The gate now requires `PGY_MATH_ROOT` to be a clean Git checkout at the exact
commit admitted by `scripts/pgy_math_registry_admission.py`. An untracked audit
file beside the pinned source is rejected as evidence.

Pergyra's dynamic matrix covers native/self-host C/LLVM on signed integer
boundaries, namespace flattening collisions, registry shadowing, scalar
`ToString` stress, registry forgery, and source/AIR/MIR binding mutation. It
also pins the still-open ABI/backend manifest-only and production-consumer
seams. Its integer probes reproduce the executable domain as wrapping signed
32-bit arithmetic; they do not make an unbounded-`Int` theorem equivalent.

An `OPEN` row is a reproduced trust gap, not a test failure hidden as green.
The aggregate command passes only when the complete expected open inventory is
still visible. `UNMEASURED` means no valid oracle exists on this host; current
Windows runs do not claim leak-sanitizer or global verifier process-budget
coverage.

## Promotion order

1. Carry an explicit numeric-domain identity through the PgyVerify request,
   generated registry, and proof receipt. Do not reopen ordinary Pergyra `Int`:
   its current semantic owner already fixes wrapping signed 32-bit arithmetic.
2. Add a proof owner for `pergyra.int.wrapping.i32.v1` (for example, Lean
   `BitVec 32` or an equivalent modular model), or require a proved no-overflow
   refinement before importing a `lean.int.unbounded.v1` theorem.
3. Add production source-to-normalized-IR binding and consume it immediately
   before backend publication.
4. Digest ABI facts, backend consumption evidence, and emitted artifacts.
5. Bind compiler, verifier, Lean toolchain, and elaborated theorem proposition
   identities.
6. Add semantic specification-liveness checks and a process-global verifier
   concurrency/orphan-process budget.
7. Run leak-enabled emitted-code sanitizers on a supported Linux/WSL toolchain.
