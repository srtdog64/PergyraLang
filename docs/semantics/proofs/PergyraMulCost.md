# Pergyra multiplication: what Coq can establish

`PergyraMulCost.v` proves the current checked multiplication boundary:

- a successful `CheckedMul` returns the mathematical product;
- the returned value is representable as a signed 32-bit `Int`;
- failure is exactly the non-representable-product case;
- the modeled compiler step has a fixed abstract cost and no variable bit-width
  parameter.

The proof and benchmark target the explicit `CheckedMul` builtin. The bare
`a * b` operator currently has a separate native machine-arithmetic lowering;
this proof does not silently claim that operator is checked or asymptotically
different.

This is intentionally narrower than an asymptotic multiplication theorem.
Pergyra's current `Int` is fixed-width; arbitrary-precision integers and an
`n`-parameterized multiplication family are not implemented. The abstract cost
in the Coq file is not a CPU-cycle claim. `tests/pergyra_mul_coq_benchmark.sh`
therefore performs the second, empirical leg: it first kernel-checks the Coq
file, then compares the same checked-multiply workload across hand-C, optional
C++, Pergyra-C, and Pergyra-LLVM with output equality as a hard precondition.

The current Windows run (2026-07-19) measured one best-of-three sample on this
checkout as approximately `pgy-C/hand-C = 0.98x` and
`pgy-LLVM/hand-C = 1.37x`. These numbers are host/toolchain observations, not a
complexity proof or a portable speed guarantee.

## Research boundary

Harvey--van der Hoeven reduce matrix transposition to integer multiplication:
their result says that an `Omega(n^2 log n)` lower bound for binary matrix
transposition would imply an `Omega(n log n)` lower bound for integer
multiplication. It does **not** prove that transposition lower bound.

The network-coding route likewise depends on a separate conjecture. The
`Tape No-Coding-Gain Lemma` and `Adaptive Laminar Direct-Sum Lemma` in the
attached note remain research obligations; they are not smuggled into the
Pergyra Coq corpus as axioms.

References:

1. D. Harvey and J. van der Hoeven, *Integer multiplication is at least as hard
   as matrix transposition*, arXiv:2503.22848,
   <https://arxiv.org/abs/2503.22848>.
2. P. Afshani, C. B. Freksen, L. Kamma, and K. G. Larsen, *Lower Bounds for
   Multiplication via Network Coding*, arXiv:1902.10935,
   <https://arxiv.org/abs/1902.10935>.
