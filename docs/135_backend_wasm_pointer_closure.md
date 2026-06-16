# Backend, WASM, And Pointer Closure

Status: beta-closure source-of-truth guard.

This note pins how to describe three related risk areas without overstating or
understating the implementation: LLVM backend debt, WebAssembly support, and
pointer/arena lifetime risk. It does not replace the detailed ledgers. It is
the short citation point for external or status-facing wording.

## 1. LLVM Backend Debt

The correct claim is: verified subset plus named remaining debt.

`docs/62_llvm_backend_debt_ledger.md` is not evidence that the backend is
generally unsound. It is evidence that the project tracks backend debt by owner
and gate. The debt categories that matter for beta wording are:

- MIR declaration and inventory representation debt.
- expression type exactness and coverage debt.
- local placement, escape, and ABI-shape debt.
- runtime hygiene and toolchain cost debt.

Forbidden wording:

- "LLVM debt proves codegen is logically unsound."
- "LLVM support is complete because the smoke suite passes."
- "C and LLVM are interchangeable for every language surface."

Allowed wording:

- "The frozen subset is tested by C/LLVM smoke and backend-compare gates."
- "Remaining LLVM work is named by owner and must keep fail-closed gates."
- "C remains the fast reference path while LLVM parity is narrowed through MIR
  inventory and backend-compare evidence."

## 2. WASM Target Status

The C-backend route to WebAssembly is verified end to end.

That means Pergyra can emit C, compile that C to `wasm32-wasi`, run the reactive
demo under Node WASI, and call the exported `ViewAfter` entry point from JS.

LLVM-to-wasm route is a runtime-link debt. The current blocker is not expression
lowering to LLVM IR; it is the lack of a wasm-buildable runtime object or shim
for helpers that are currently static-inline in C runtime headers, plus the
known `ucontext` shim requirement.

direct wasm backend is post-beta. A direct `pgy --emit-wasm` path would be a
new backend beside C and LLVM. It is a self-hosting and toolchain-purity goal,
not a beta-stable claim and not a blocker for the current WebAssembly bridge.

## 3. Pointer And Arena Lifetime

Pointer safety is not claimed globally. The beta claim is narrower: stable ABI
pointers must be classified, and known stable producers must have lifetime gates.

The active pointer classes are the ones in
`docs/128_pointer_risk_register.md`: borrowed, result-owned, runtime-owned,
container-owned, and scratch. Any pointer outside those classes is beta debt.

The primary gate for stable ABI lifetime claims is
`runtime-abi-lifetime-test-smoke`. It is allowed to prove specific ABI and
runtime helper contracts. It is not a universal pointer/lifetime proof for the
entire compiler.

Open risk areas stay explicit:

- static scratch pointers in codegen helpers must be consumed immediately and
  must not be cached;
- cross-arena references use index or stable handle, not long-lived raw
  pointers;
- result-visible diagnostics and stores must not retain scratch arena payloads;
- raw escape remains scoped capability work, not an implicit safe default.

## 4. DOP And FP Core Rule

The memory model is a core language contract, not a performance-only detail.
Data-oriented layout and functional-style stability only compose if ownership,
arena reset, and ABI escape lanes stay explicit.

Therefore the correct posture is conservative:

- do not market a broad Rust-level lifetime proof;
- do not hide raw pointer use in generated C/LLVM;
- do keep stable ABI pointer classes, arena ownership lanes, and runtime gates
  as the source of truth.

## 5. Closure Checklist

Before beta-facing text cites these areas, it must satisfy all of these:

- LLVM wording says verified subset plus named remaining debt.
- WASM wording separates the verified C-backend route from LLVM-to-wasm and
  direct wasm.
- Pointer wording cites `runtime-abi-lifetime-test-smoke` only for the stable
  ABI contracts it actually gates.
- Arena wording says cross-arena references use index or stable handle.
- No document claims a universal pointer/lifetime proof.
