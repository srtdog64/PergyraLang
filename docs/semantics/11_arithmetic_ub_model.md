# 11. Arithmetic UB Model (Two Layers, Fail-Closed)

Companion to `proofs/CheckedArith.v` (machine-verified) and the runtime guards in
`src/runtime/pgy_runtime_lib_authority_file_core.h` /
`pgy_runtime_panic_checked_inline.h`. Keeps the C/LLVM panic-class parity
required by `docs/100b` (divide-by-zero must stay green on both backends).

## 0. Two layers of UB

Undefined behavior reaches a Pergyra program through **two** distinct layers, and
both must be closed for the slot/witness guarantees to hold:

1. **Surface UB** — operations the source language exposes that have no defined
   result (out-of-bounds, divide-by-zero, double-free, `None` unwrap, stale slot
   generation). These are closed by **fail-closed runtime checks** that panic
   with a structured class/reason.
2. **Backend-inherited UB** — the *emitted* C or LLVM IR is itself UB, so the
   optimizer is licensed to miscompile it (silently wrong value, or a hardware
   trap) **even when the surface check looks present**. This is the dark side of
   the dual-emit thesis: a correct surface check is worthless if the lowering
   that implements it is itself UB and gets folded away.

A claim of "we handle X" is only true when **both** layers are closed and the
**C and LLVM backends agree** (the parity gate is what makes layer 2 observable).

## 1. Corrected status map

The earlier working map over-claimed two items and mis-attributed a third. The
audited state (2026-06-21), per-backend, empirically reproduced:

| Class | C backend | LLVM backend | Layer-2 status |
|---|---|---|---|
| OOB / bounds | panic | panic | closed |
| uninit (calloc/zero) | defined | defined | closed |
| double-free / stale slot gen | panic | panic | closed |
| `None` / unwrap-on-Err | panic | panic | closed |
| **integer overflow (+,-,\*)** | wrap (`-fwrapv`) | wrap (`add` w/o `nsw`) | **closed** — both define wrap; the earlier "C-UB-inherited" note was wrong |
| **divide-by-zero** | panic | panic | **closed** (was an LLVM crash; see §2) |
| **signed div overflow `INT_MIN / -1`** | panic | panic | **closed** (was silent garbage on LLVM; see §2) |
| `INT_MIN % -1` | `0` (true remainder) | `0` | closed |

`+ - *` overflow is **defined wraparound**, not UB: the emitted-C driver passes
`-fwrapv` and the LLVM backend emits `add/sub/mul` without `nsw`. The two
backends therefore agree by construction; this is a deliberate semantic, not an
inherited hole.

## 2. The division holes (found, fixed, verified)

Signed division in C is UB on exactly two inputs — `rhs == 0` and
`INT_MIN / -1` (true quotient `+2^31` is not representable; on x86 the `idiv`
faults). Both backends route `/` and `%` through one checked runtime helper, but
three defects let layer-2 UB through on **LLVM only**:

1. **Helper relied on `-fwrapv`.** The helper guarded `rhs == 0` but not
   `INT_MIN / -1`; the emitted-C runtime was built with `-fwrapv` (so it wrapped
   safely) while the LLVM-linked runtime was not (silent garbage). **Fix:** a
   self-contained `if (lhs == INT_MIN && rhs == -1)` guard in all four helpers
   (`*_div_*` panic `arithmetic-overflow`; `*_mod_*` return the true remainder
   `0`). Now flag-independent.
2. **Literal fast-path admitted `-1`.** The "skip the checked helper when the
   divisor is a nonzero literal" optimization treated `-1` as safe, but `-1`
   re-enables the `INT_MIN` overflow. The predicate
   (`codegen_scalar_arithmetic_policy.c`) was renamed to
   `..._is_safe_divisor_i32_literal` and now also excludes `-1`, so both backends
   route a literal `-1` divisor through the checked helper.
3. **Runtime-bitcode inlining folded the guard.** The LLVM backend links the
   runtime *bitcode* into each module and runs `-O3`, which inlined the checked
   helper, constant-folded a literal `INT_MIN / -1` (instcombine rewrites
   `sdiv x, -1` to a negation), and **discarded the guard** — and the inlined
   `panic_emit` mis-lowered `stderr`/`abort` and crashed with an access violation
   instead of printing and aborting. **Fix:** `llvm_exclude_critical_runtime_from_bitcode`
   strips the checked-arithmetic and panic-family function bodies from the linked
   bitcode (leaving external declarations), so those calls resolve to the
   separately compiled gcc runtime object — identical to the C backend, which
   never folds its runtime. The hot primitives (`Substring`, `StringConcat`, …)
   still inline.

Defense-in-depth: `-fwrapv` and `-fno-strict-aliasing` are now passed on every
runtime/driver compile path (emitted-C driver and the LLVM runtime build), so the
linked runtime stays UB-free under `-O3` even if a future caller is folded.

### Empirical parity (reproduced)

| program | C | LLVM |
|---|---|---|
| `a / 0` | PANIC `divide-by-zero` | PANIC `divide-by-zero` |
| `INT_MIN / -1` | PANIC `arithmetic-overflow` | PANIC `arithmetic-overflow` |
| `INT_MIN % -1` | `0` | `0` |
| `100 / -1` | `-100` | `-100` |
| `100 / 7` | `14` | `14` |

The guard fires *only* on the genuine overflow input (`100 / -1` divides
normally). A stale `pgy_runtime_lib.bc` silently re-opens hole 3 — regenerate it
after any runtime change with `scripts/build_runtime_bc.sh` (a machine-local,
gitignored artifact).

## 3. Mechanization

`proofs/CheckedArith.v` models the checked helper over `Z` with the i32
representable interval and `Z.quot` / `Z.rem` (truncated, C semantics), and proves
machine-checked:

- `div_total` — every input maps to a clean panic or a value (never stuck/UB).
- `div_none_iff` — it panics on **exactly** the two C-UB inputs (`rhs = 0` or
  `INT_MIN/-1`) and nothing else.
- `div_some_quot` — a returned value is the true truncated quotient (no silent
  wrong answer).
- `div_some_representable` — a returned value is always representable in i32 (the
  one overflowing quotient `+2^31` is exactly the panic case).
- `mod_none_iff` / `mod_some_representable` — modulo panics only on divide-by-zero
  (`INT_MIN % -1 = 0`), and its result is always representable.

A single total spec consumed by both backends is precisely what the C == LLVM
parity gate observes at runtime.

## 4. Type confusion / invalid discriminant (closed)

For **well-typed** code this is closed by construction: `check_match_exhaustiveness`
rejects a match over a finite variant space (enum/Option/Result) that is neither
fully covered nor has a default. A valid in-language value therefore always
selects an arm; the trailing "no arm matched" path is unreachable. The only way
to reach it is an invalid tag arriving from the **unsafe/FFI boundary or memory
corruption** (§5).

That residual is now **fail-closed uniformly**. The point where it surfaces is a
non-void function reaching its end without returning (the no-arm-matched merge of
an exhaustive match lowers to exactly this shape). All emitters now panic there
with the same internal-invariant class and message ("non-void function reached
end without return") instead of leaving UB:

- AST-direct C (generic functions) — `transpiler_func_class_flow_emit.c` (was
  already fail-closed; the reference behaviour).
- MIR C (the default for non-generic functions) — `transpiler_mir_terminator_emit.c`.
- MIR LLVM — `llvm_mir_block_emit.c` (Closure #74 site), calling
  `pgy_runtime_panic_internal_invariant_export` before the trailing `unreachable`
  that keeps the IR well-formed.

A divergent guard (one path/backend panicking, another UB) would itself violate
the C == LLVM evidence contract, so the rule is *all paths identical or none* —
here, all identical. No well-typed program is affected; this only converts an
FFI/corruption-supplied invalid tag from UB into a loud panic. The structural
contract is gated by `runtime-panic-contract-test-smoke`: the emitted C owners
must carry `PGY_PANIC(...)`, and the LLVM MIR owner must call the shared panic
export before `unreachable`. The ordinary well-typed match corpus remains under
backend-compare parity; the invalid-tag path is a hard-fail boundary, not a
source-visible value result.

## 5. FFI / `extern` escape hatch

`extern` declarations and `unsafe { ... }` blocks are the explicit trust
boundary. Values crossing it (including tagged unions with out-of-range tags and
the arithmetic operands above) are outside the safety guarantees by design — this
is the acknowledged opt-out, syntactically marked, not a silent hole. The checked
arithmetic helpers (§2) turn an FFI-supplied bad operand into a loud panic at the
first in-language arithmetic use, and the §4 guard does the same for an invalid
match discriminant — both convert an FFI/corruption-supplied bad value into a
fail-closed panic rather than UB, though the boundary itself remains the
programmer's responsibility.
