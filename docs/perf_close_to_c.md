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
| array `xs[j]` 50M | ref | 2.85x -> 1.42x | inline-get closed ~half the gap |
| for-in 50M | ref | 3.01x | aggregate/length reload not yet hoisted |
| generic 50M | ref | 0.27x | LLVM faster |
| match 50M | ref | 0.51x | LLVM faster |

The array row is measured before and after the inline indexing change: the
opaque `pgy_array_get_T` call (2.85x) became inline IR (1.42x), with identical
output on both backends. `for-in` is unchanged because its cost is the
per-iteration reload of the aggregate base and length, not the bounds check;
hoisting that invariant out of the loop is the next probe. Backend
output-equality is asserted by the gate on every construct, so a lowering
regression fails the build rather than silently diverging.

## Self-hosted workload (the real dogfood measure)

The micro-benchmarks isolate one construct each. The self-hosted compiler stages
(`src/self_hosted/*/main.pgy`) are the realistic mixed workload, so they are the
honest answer to "how slow is the LLVM backend on actual Pergyra code." The
self-hosted lexer was run on three real inputs, timed per-run over many
iterations to drown out process startup, with backend output compared byte for
byte:

| input | lines | tokens | pgy-C | pgy-LLVM | ratio | output |
|-------|-------|--------|-------|----------|-------|--------|
| semantic/main.pgy | 1529 | 8,524  | 38 ms  | 65 ms  | 1.70x | identical |
| parser/main.pgy   | 3356 | 21,226 | 234 ms | 418 ms | 1.78x | identical |
| codegen/main.pgy  | 1927 | 7,939  | 78 ms  | 151 ms | 1.92x | identical |

So on real code the LLVM backend trails the C backend by ~1.7-1.9x. A startup-only
run (`examples/hello.pgy`, 16 tokens) shows ~1.0x, which is misleading: it
measures process launch, not compute.

The cause was pinned down by disassembling the optimized lexer binary on both
backends and counting residual calls to each hot helper:

| helper | C backend | LLVM backend |
|--------|-----------|--------------|
| `CharAt` (Pergyra func) | 14 calls | 0 (inlined) |
| `IsAlpha` / `IsDigit` (Pergyra) | 0 (inlined) | 0 (inlined) |
| `Substring` (runtime) | 0 (inlined) | 17 calls |

The LLVM backend actually inlines the small Pergyra helper functions more
aggressively than gcc does (`CharAt` disappears entirely). The gap is the
runtime primitive `Substring`: in the C backend it is a `static inline` in a
runtime header compiled alongside the generated code, so `gcc -O2` inlines it
away; in the LLVM backend the runtime is linked as opaque external symbols, so
every `Substring` site stays a real call plus its allocation. This is the same
class as the `arr[i]` fix, one level deeper.

Two attribute passes were added and verified (output identical on all three
inputs, micro gate green): `readnone` on pure math runtime helpers, `readonly`
on pure string reads (`StringIndexOf`, `StringContains`, `pgy_string_equals`,
`ToInt`, `ToFloat`), and `nounwind`/`willreturn` broadly. These are correct and
help redundant-pure-call and math-heavy code, but they do not move the lexer
because its cost is the opaque allocating `Substring` call, which cannot be
CSE'd or inlined as an external symbol.

The real lever, therefore, is structural: compile the runtime to LLVM bitcode
and link it into the module before the optimization pass, so the inliner can
fold `Substring` and the other `static inline`-in-C runtime primitives the way
gcc already does. That is the LLVM-side equivalent of the C backend's
header-inlined runtime and is expected to close most of the remaining ~1.8x.

### Runtime bitcode inlining (on by default)

`llvm_link_runtime_bitcode` (in `src/codegen/llvm_api.c`) loads a runtime bitcode
module and `LLVMLinkModules2` links it into the program module immediately before
the O2 pass; the existing internalize step then gives the merged runtime
definitions internal linkage, so the inliner folds the hot ones and dead-strips
the rest. Every failure path (no path configured, file missing, parse or link
error) is a silent no-op that leaves the runtime as external calls, so a
toolchain without the bitcode is unaffected.

The bitcode path comes from `PGY_RUNTIME_BC` (env, for a relocated binary) or the
build-time `PGY_RUNTIME_LIB_BC` define. The Makefile bakes `PGY_RUNTIME_LIB_BC`
to `$(CURDIR)/src/runtime/pgy_runtime_lib.bc`, so once that file exists
`--backend=llvm` inlines the runtime **by default** -- no env var needed.

The `.bc` is produced by clang (matching the linked libLLVM). clang is not a
build dependency for pgy itself, and the artifact bakes absolute `__FILE__`/root
paths, so it is **gitignored and generated locally** rather than committed:

```
make runtime-bc            # or: scripts/build_runtime_bc.sh
```

A missing `.bc` is a silent no-op (the runtime stays external calls and the build
is correct, just without the speedup), so generating it is optional and a
toolchain without clang is unaffected. Regenerate it after any runtime change so
the inlined copy does not drift from the linked runtime object; `codegen_parity`
(which exercises the LLVM backend) catches a drift as an output mismatch.

#### Two failure modes that made this a silent no-op (both fixed)

The mechanism was implemented but produced *zero* measured gain until two bugs
were found:

1. **Empty bitcode.** `pgy_runtime_lib.c` -- the file that defines the non-inline
   runtime symbols -- is wrapped entirely in `#ifdef PGY_LLVM_ENABLED`. The build
   script omitted that define, so clang compiled the file to an empty module: a
   299 KB-worth of functions collapsed to a 2 KB bitcode with **0 definitions**,
   and linking it did nothing. `build_runtime_bc.sh` now passes
   `-DPGY_LLVM_ENABLED` and verifies the artifact is non-empty (`llvm-nm` must
   find defined symbols) before "succeeding". On a mingw host it also targets
   `x86_64-w64-mingw32` and adds the mingw system include so `<pthread.h>`
   resolves and the ABI matches the gcc-built program.

2. **Unopenable path.** `PGY_RUNTIME_BC` must be a path the compiler binary can
   `fopen`. A git-bash `/e/PergyraLang/...` style path handed to a native Windows
   `pgy.exe` fails to open -> silent no-op (the binary was byte-identical to the
   no-bitcode build). Use a native path (`E:/PergyraLang/...`); the baked
   `PGY_RUNTIME_LIB_BC` define already uses `$(CURDIR)`, which is native.

#### Measured (Windows, mingw gcc / LLVM 22, self-hosted lexer on parser/main.pgy)

Compute time = total minus the ~120 ms process-startup floor (best-of-5):

| build | Substring call sites | compute vs C backend |
|-------|----------------------|----------------------|
| LLVM, no bitcode  | 17 | 1.67x |
| LLVM, +bitcode    | 3  | ~0.9x (at/below C) |

The inliner folds 14 of 17 `Substring` sites; the externally-linked allocating
call becomes inline, allocation-visible IR that the optimizer can fold and CSE.
That closes the entire ~1.7x lexer gap and reaches parity with -- sometimes
slightly below -- the C backend. Correctness is unchanged: `codegen_parity`
(c + llvm, 48 fixtures) stays green and the self-hosted lexer/parser/semantic/
codegen produce byte-identical output on both backends with the bitcode linked.

## Versus Rust, and the one structural gap (string allocation)

A four-way comparison -- hand-C (gcc -O2), Rust (rustc -O3), Pergyra C backend,
Pergyra LLVM backend -- on identical-output micro-benchmarks (Windows, gcc/LLVM
22, Rust 1.89, best-of-N).

**Maturity caveat first.** These are *runtime-speed* numbers. They do NOT mean
Pergyra's compiler/IR competes with Rust MIR or Swift SIL as engineering
artifacts. MIR/SIL are IRs proven over years in production across ownership,
lifetime, optimization, diagnostics, incremental builds, LLVM lowering, and ABI
stability. Pergyra has the *shape* (layered HIR/RIR/MIR/AIR, dual C/LLVM parity
gates, ABI-layout docs, MIR-level DCE) but those properties are still *closing*,
not closed: SoT closure, C/LLVM parity, self-hosted substitution (just at
rung-0b), layout/ABI spec, and golden-test coverage are in progress. "Pergyra
runs at C/Rust speed on these benchmarks" is a statement about clean codegen
riding gcc/LLVM, not about IR maturity.

**Compute-bound** (arith `acc=acc*31+i`, fib): the C backend tracks hand-C
(~1.03x); Rust and the LLVM backend ride LLVM and on counted loops the LLVM
backend's ScalarEvolution closes the loop to a near-constant time, so its
micro-numbers reflect loop elimination, not steady-state throughput -- the
honest LLVM datapoint is the non-foldable self-hosted lexer above. Net: for
pure compute, all three land in the same LLVM/gcc-native class.

**The real gap is allocation.** On a tight `Substring`+`IndexOf` loop (40M
iters), idiomatic Rust uses a zero-copy `&str` slice while Pergyra's `Substring`
heap-allocates a copy per call:

| variant | 40M iters | vs Rust slice |
|---------|-----------|---------------|
| Rust `&str` slice (no alloc)      | ~153 ms  | 1.0x |
| Pergyra `Substring` (allocates)   | ~1587 ms | ~10.4x |
| same loop via `PgyStrView` (borrow) | ~189 ms | ~1.24x |

Apples-to-apples (Rust `.to_string()` per substring, C `malloc`+`memcpy` per
substring) all land near the allocating Pergyra number, so the language overhead
is small -- the 10x is purely *allocate-a-copy vs borrow-a-slice*, an API/idiom
difference, not a codegen-quality defect.

### Closing it: non-owning string views

`src/runtime/pgy_runtime_strview_inline.h` adds `PgyStrView { const char *data;
int32_t length; }` -- the string counterpart of the existing array `PgySlice_T
{ data; length; }`, and of Rust's `&str`. `pgy_strview(s, start, len)` borrows a
range with the same clamping as `Substring` but **zero allocation**;
`pgy_strview_indexof` / `pgy_strview_len` operate on the range in place. The
`benchmarks/perf_strview_baseline.c` proof shows it closes the gap from ~10.4x to
~1.24x of Rust's slice (8.4x faster than the allocating `Substring` loop) with
byte-identical output.

This is the *runtime primitive*, proven standalone. The remaining work (next
rung) is the language surface: a `StrView`/`&String` view type the type system
tracks, view-returning builtins (`Substring` overload / `SubView`), and lowering
through both backends with the borrow's lifetime tied to its source -- so .pgy
code can opt into zero-allocation string scanning the way Rust code uses `&str`.
Until then the primitive only proves the path; idiomatic Pergyra string code
still allocates.
