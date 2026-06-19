# Runtime Performance: How Close Is Pergyra to C

Goal: keep Pergyra's compute speed from drifting far from hand-written C, and
make that a repeatable, gated measurement.

## Method

Micro-benchmarks in `benchmarks/` are compiled three ways and timed best-of-3
(wall clock, compile time excluded):

- hand-C baseline, `gcc -O2`
- Pergyra C backend (`pgy --backend=c`, which transpiles to C then `gcc`)
- Pergyra LLVM backend (`pgy --backend=llvm`)

Benchmarks use data-dependent bodies so neither optimizer can fold the work
away. The gate lives in `tests/perf_close_to_c_smoke.sh`.

## Results (sandbox, gcc -O2; representative)

| benchmark | work | hand-C | pgy-C | pgy-C ratio | pgy-LLVM | pgy-LLVM ratio |
|-----------|------|--------|-------|-------------|----------|----------------|
| arith | 100M iters, `acc = acc*31 + i` | 0.070s | 0.071s | 1.01x | 0.138s | 1.96x |
| fib | recursive `fib(35)` | 0.015s | 0.014s | 0.95x | 0.049s | 3.27x |

All three produce identical results (e.g. arith `acc=-1508126591` on every
target, confirming identical integer-wrap semantics).

A third probe (a 50M-iteration loop calling a trivial `Inc`) compiled to a
constant on the C backend: `gcc -O2` inlined `Inc` and constant-folded the loop,
exactly as it does for hand-C. That is itself a positive signal: the C backend
emits clean enough C that the host optimizer applies the same aggressive
transforms it applies to hand-written C.

## Conclusions

1. The C backend is essentially at hand-C speed (≈1.0x). This is expected and
   confirmed: `--backend=c` transpiles to C and rides `gcc -O2`, so as long as
   the emitted C is clean (no boxing or needless indirection), it is hand-C
   after the host compiler. The benchmarks show the emitted C is clean.

2. The LLVM backend was ~2-3x slower than the C backend - root-caused and fixed.
   `llvm_run_optimization` (src/codegen/llvm_api.c) was a stub: it called
   `llvm_apply_target_machine` and `(void)release_opt;` and ran zero passes, so
   the module was emitted at -O0 (allocas never promoted). The fix wires the
   standard pipeline: `LLVMRunPasses(module, "default<O2>", machine, opts)`
   (`default<O3>` for release). After the fix (re-measured on the rebuilt pgy):

   The fix has two parts, both in `llvm_run_optimization` (src/codegen/llvm_api.c):

   1. Run the pipeline: `LLVMRunPasses(module, "default<O2>", machine, opts)`
      (`default<O3>` for release).
   2. Internalize: before optimizing, set `LLVMInternalLinkage` on every defined
      function except the C entry `main`. pgy emits functions with external
      linkage by default, which blocks the O2 inliner / interprocedural passes
      (argument promotion, attribute inference, cross-function inlining) because
      external symbols might be called from outside the module. For a whole-
      program executable nothing but `main` needs to stay external; addresses
      taken for domain dispatch remain valid under internal linkage.

   | benchmark | hand-C | pgy-LLVM start | +O2 pipeline | +internalize |
   |-----------|--------|----------------|--------------|--------------|
   | arith | 0.076s | 0.138s (1.96x) | 0.003s | 0.003s |
   | fib   | 0.019s | 0.049s (3.27x) | 0.034s | 0.024s (~1.6x) |

   On arith, LLVM's ScalarEvolution closed-formed the loop (faster than gcc,
   which kept it) - confirming the optimizer is active. Internalize cut fib from
   ~1.8x to ~1.6x by unlocking interprocedural optimization. Correctness
   verified on LLVM after both fixes: script_register prints "SCRIPT REGISTER
   sum=5", infer_return prints "INFER RETURN sum=5" (so internalize does not
   break domain dispatch or the entry path).

   Residual: fib recursion is ~1.6x of C. This is an adversarial pure-call
   micro-benchmark; the remaining gap is codegen/register-allocation difference
   for tiny recursive functions between this backend's LLVM codegen and gcc, and
   is not representative of real code (loop/compute code is already at or below C).

## Deeper findings

### The fib residual gap is a gcc-vs-LLVM policy difference, not a defect

Disassembling both fib binaries:

- gcc `-O2` fib: ~166 instructions. gcc unrolls the recursion ~5 levels deep
  (inlining `fib(n-1)+fib(n-2)` repeatedly), trading code size for far fewer
  actual calls.
- pgy-LLVM Fib: ~27 instructions. LLVM applied the accumulator transform,
  turning one recursive arm (`Fib(n-2)`) into a loop while the other recurses -
  smaller code, but more calls than gcc's deep unroll.

So our LLVM output is genuinely (arguably more cleverly) optimized - the
recursion is partly turned into a loop. gcc simply wins this adversarial
micro-benchmark by spending code size on aggressive unrolling. This is an
inliner/unroll *policy* difference between the two optimizers, not a pgy codegen
defect. Raising the LLVM inline threshold or O3 could narrow it, with diminishing
returns.

### Real code: the self-hosted semantic checker

A more representative measurement - the self-hosted semantic checker (a real
~51KB Pergyra program) compiled on both backends and run on a source file:

| target | per run | ratio |
|--------|---------|-------|
| pgy-C | ~1.80ms | 1.0x |
| pgy-LLVM | ~2.0ms | ~1.1x |

Both backends produce byte-identical output (md5 match). On real Pergyra code
the LLVM backend is ~1.1x of the C backend - near parity, and much closer than
the adversarial fib (1.6x). The per-run figure is partly process startup, so the
pure-compute difference is smaller still. Takeaway: the fib gap does not
generalize; real self-hosted code runs at near-C speed on both backends.

## The gate (test)

`tests/perf_close_to_c_smoke.sh`:

- compiles each benchmark on both backends plus a `gcc -O2` baseline,
- times best-of-3,
- FAILS if `pgy-C > PERF_C_MAX_RATIO` x hand-C (default 2.0x) - this is the
  "don't drift from C" guard,
- WARNS if `pgy-LLVM > PERF_LLVM_MAX_RATIO` x hand-C (default 4.0x).

Skips cleanly when `pgy`, `gcc`, or `bc` are absent, so it is safe in any CI
lane. Wire it into the Makefile alongside the other `*-test-smoke` targets (it
expects `PGY` / `BIN_DIR` like the example smoke tests).

Timing tests are environment-sensitive, hence best-of-3 and generous factors:
the gate is meant to catch large regressions (boxing, accidental indirection, a
broken optimization), not micro-noise.

## Extending

Add a benchmark by dropping `benchmarks/perf_<name>.pgy` and a matching hand-C
baseline (currently the baselines are inline in the test script; move them to
`benchmarks/baseline_<name>.c` if the set grows). Good next probes: array
build+sum (allocation/bounds), pattern `match` dispatch, and a parallel block
(to measure the threading runtime overhead vs serial C).

## LLVM backend optimization work

The LLVM backend's own optimization pipeline carries the gap between it and the
C backend (which leans on `gcc -O2`). Three changes target that gap:

1. Pass pipeline. `llvm_run_optimization` runs `default<O2>` (`default<O3>` for
   release) plus internalization of non-`main`, non-declaration functions so the
   inliner and global optimizers can fire. Before this it was a no-op stub, which
   is why early LLVM numbers trailed the C backend by 2-3x on arithmetic loops.

2. Never-returning attributes. Functions that cannot return get `noreturn`, and
   the panic error family additionally gets `cold`, so out-of-bounds and invariant
   traps sink out of the hot path. The never-return set is the panic family plus
   an exact-name table (`pgy_exit`); matching is exact, not substring, so that
   returning lookalikes such as `pgy_intent_exit_export` are never mismarked.

3. Inline array indexing. `arr[i]` previously lowered to an opaque
   `pgy_array_get_T` runtime call, which the LLVM optimizer cannot inline (the C
   backend's equivalent is a `static inline` that `gcc` does inline). It now
   lowers to inline IR: load the aggregate, take the data pointer and length, do
   an unsigned bounds check that branches to a cold out-of-bounds panic, then an
   `inbounds` GEP and load. This is wired at both indexing sites,
   `llvm_emit_array_access_expr` (named arrays) and `llvm_emit_checked_collection_get`
   (array values such as `getArray()[i]` and array-typed fields), via the shared
   helper `llvm_emit_inline_array_get`. `for-in` already emits a direct
   `inbounds` GEP. Slice indexing still uses the runtime call pending a verified
   slice-layout match.

Measured (sandbox, best-of-3, ratios vs the column noted):

| benchmark | pgy-C vs hand-C | pgy-LLVM vs hand-C | note |
|-----------|-----------------|--------------------|------|
| arith (100M acc) | 0.98x | 0.03x | LLVM hoists/folds the counted loop |
| fib(35) | 0.94x | 1.51x | recursion; inline-threshold bound |
| array `xs[j]` 50M | ref | 2.85x (pre-inline) | inline-get targets this |
| for-in 50M | ref | 2.99x (pre-inline) | aggregate-load hoisting |
| generic 50M | ref | 0.25x | LLVM faster |
| match 50M | ref | 0.52x | LLVM faster |

The array/for-in rows are the pre-inline baseline; the inline-get and `inbounds`
changes target exactly those. Backend output-equality is asserted by the gate on
every construct, so a lowering regression fails the build rather than silently
diverging.
