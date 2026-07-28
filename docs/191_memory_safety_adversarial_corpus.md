# Memory Safety Adversarial Corpus

Status: active beta-closure contract

This document defines the executable memory-safety corpus that must travel with
hard self-hosting and bootstrap substitution. It does not replace the pointer
risk register. It turns the highest-risk lifetime claims into stable fixtures,
named verdict owners, and negative gates.

## 1. Objective Card

- Objective: reject or guard representative dangling-pointer, use-after-free,
  double-release, stale-handle, borrow-escape, and concurrent-lifetime hazards
  before a self-hosted compiler path replaces the C oracle.
- Priority: semantic identity, owner-directed facts, missing-fact rejection,
  C/LLVM projection parity, then sanitizer evidence.
- Fact owners: semantic resource state for source rejection, MIR boundary facts
  for lowering rejection, runtime generation/pin state for dynamic guards.
- Last legitimate consumers: semantic diagnostics, MIR verifier, runtime guard,
  and backend emitters consuming an already-decided row.
- Forbidden fallback: executing undefined C and treating its result as the
  language verdict; rescanning source text or AST to recover a missing fact.
- Gates: `memory-adversarial-catalog-test-smoke`,
  `slot-contract-test-smoke`, the row-specific gates in the manifest, and the
  bounded Linux `test-asan` job.

## 2. Oracle Hierarchy

The C compiler is the current implementation and execution oracle for defined
programs. It cannot be the semantic oracle for undefined behavior. A C
use-after-free may appear to work, crash, or change under optimization, so its
exit code is not a stable language truth.

The safety hierarchy is therefore:

1. Pergyra semantic or MIR facts own `ALLOW`, `REJECT`, and missing-fact
   fail-closed decisions.
2. C, LLVM, and self-hosted projections must preserve the owned decision,
   diagnostic identity, and runtime behavior for accepted programs.
3. ASan/UBSan are fault witnesses. They detect compiler/runtime implementation
   defects and calibrate the host toolchain, but do not define source semantics.

`tests/cases/memory_adversarial/c_witness/heap_use_after_free.c` is deliberately
invalid C. Its only purpose is to prove that the sanitizer job is capable of
detecting a real heap UAF before a clean sanitizer result is trusted.

## 3. Canonical Corpus

The machine-readable owner is
`tests/cases/memory_adversarial/manifest.tsv`. Every row has a stable identity,
hazard, source surface, expected disposition, oracle class, fixture, gate, and
status.

- `CLOSED`: a live fixture and blocking gate own the stated disposition.
- `PARTIAL`: executable evidence exists, but the general proof obligation is
  still open. It must not be presented as language-wide closure.
- `OPEN`: no fixture, gate, or oracle may be implied. Adding one requires
  changing the row and landing the evidence in the same change.

The first corpus fixes these already landed cases in one inventory:

- Slot, SecureSlot, and DeviceSlot read/write after release.
- Slot and DeviceSlot double release.
- stale generation/ABA, release while pinned, and double unpin.
- detached async local capture and borrowed Slice transport.
- raw pointer escape and the compiler's scope-owned Symbol lifetime regression.

The first explicit open set is:

- FFI callback after the captured owner returns.
- iterator use after collection mutation or reallocation.
- channel destruction while a waiter is blocked.
- parallel runtime data races requiring a TSan-quality witness.
- instance handles escaping their sandbox or content-instance lifetime.
- the general proof that scratch/arena pointers cannot enter persistent caches.

## 4. Self-Host And Bootstrap Contract

Hard substitution does not pass merely because generated C exhibits the same
accidental behavior. For every memory-safety row reached by a self-host rung:

1. the Pergyra implementation must consume the same stable semantic or MIR
   fact as the C implementation;
2. a missing fact must reject rather than choose a C-compatible default;
3. diagnostic code and disposition must match the oracle corpus;
4. accepted programs must retain C/LLVM/self-host execution parity; and
5. the old owner path must be deleted and ratcheted against reintroduction.

The execution order after the foundational Slot UAF rows is use-after-scope,
stale/ABA handles, borrow plus reallocation, concurrent destruction/data races,
then FFI/raw boundaries. This follows evidence ownership rather than surface
syntax size.

## 5. Performance Boundary

Memory evidence is not evidence that generated Pergyra programs are generally
slower than C. The committed same-machine BN measurements show:

- serial n=9: raw hand-C 14.45 s, Pergyra-to-C 14.39 s, and hand-C with
  Pergyra-style access checks 15.70 s;
- parallel n=10: hand-C OpenMP 40.26 s, Pergyra join-with-sum 47.64 s, and
  Fortran OpenMP 60.96 s;
- parallel n=9: hand-C OpenMP 2.00 s and Pergyra pool-auto 2.42 s.

These numbers establish competitiveness on one workload, not a general
language ranking. There is no committed Rust leg in this benchmark, so a Rust
comparison remains unmeasured. Compiler build time and compiler peak memory are
separate maturity problems and must not be inferred from generated-code runtime.

## Emitted-code UB: first measurement (2026-07-28)

`tests/sanitizer_compile_smoke.sh` asks whether the **compiler** commits UB while
compiling. `tests/emitted_c_sanitizer_smoke.sh` asks the other half — whether the
code the compiler **emits** commits UB when it runs — and until now that surface
had never been measured. It matters here specifically because Pergyra lowers to
C, so the language-level safety claims are delivered *through* emitted C, and
because `backend_compare` compares the C lowering against the LLVM lowering: UB
present in **both** passes that comparison unnoticed. A sanitizer is the
independent judge a differential oracle structurally lacks.

First full run, Ubuntu 24.04 / gcc 13.3, every case built with
`-fsanitize=undefined,address -fno-sanitize-recover=all` and executed:

| | count |
|---|---:|
| cases in corpus | 923 |
| **built and run under ASan+UBSan** | **914** |
| **UB or memory errors found** | **0** |
| not measurable (did not build) | 9 |

The nine are pre-existing and reproduce with a plain `gcc`, so they are not
sanitizer artifacts — the gate reports them as skips rather than blaming the
sanitizer for unrelated breakage:

- 2 fail in the C compiler on emitted code (`forward_ability_order` emits a
  `pgy_region_destroy(&__pgy_region_2)` referencing an undeclared identifier;
  `generic_class_method` likewise);
- 7 are refused by `pgy` itself, fail-closed, with "C MIR source operation has
  no active instruction-owned runtime-call ABI row" (`object_layer_binding`,
  `secure_slot_view`, `zone_effect_pool_runtime`, and four `pin_*` view cases).

Scope: this covers out-of-bounds access, use-after-free, null/misaligned pointer
use, bad shift widths, division by zero, and invalid bool/enum loads. It does
**not** cover uninitialized reads (MemorySanitizer, not run) and says nothing
about the LLVM lowering, which was not exercised here. Two classic emitted-C
traps are already defused by construction in `compiler.c` (`-fwrapv`,
`-fno-strict-aliasing`), so they are outside what this run could have found.
