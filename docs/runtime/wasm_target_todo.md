# WASM Target (TODO)

Status: the C-backend route to WebAssembly is verified end to end. The reactive
demo compiles to wasm32-wasi via the zig-bundled clang and runs on node's WASI,
and the exported ViewAfter entry point is callable from JS (ViewAfter(5) = 5).
See examples/reactive_dom_demo/web/. This note records the two follow-ups.

## 1. LLVM-backend route to wasm (runtime link)

pgy --emit-llvm produces Pergyra's own LLVM IR, which clang lowers to wasm the
same way Rust reaches wasm. The IR route currently fails at link, not at
codegen: the runtime helpers it references (StringConcat, pgy_int_to_string,
pgy_log_string, the panic export, pgy_args_init) are static inline functions in
the runtime headers, so they have no standalone object file to link against.

The C-backend route works precisely because those helpers are header-inlined
into the emitted C, making the translation unit self-contained.

To open the LLVM route, the runtime must provide a wasm-buildable object that
exports those helper symbols (a non-inline runtime build, or a small wasm
runtime shim that defines them), then link it with the emitted IR object. The
ucontext coroutine include also needs the shim used for the C route
(examples/reactive_dom_demo/web/ucontext_shim.h), because wasm32-wasi has no
ucontext.

## 2. Direct wasm backend (pgy --emit-wasm)

pgy currently has C and LLVM backends and reaches wasm through their output plus
the shared LLVM/clang lowering. A direct wasm backend would emit wasm from the
Pergyra pipeline without an external C or clang step, making the whole path
self-owned. This is a real new backend, parallel to the C and LLVM ones, and is
not required for the web thesis: the C route already gets Pergyra reactive logic
into WebAssembly. It is a purity and self-hosting goal, worth listing but lower
priority than closing the in-browser DOM wiring.

When self-hosting reaches the backend stage, a wasm backend written in Pergyra
would also be the cleanest way to make the toolchain fully Pergyra-owned from
source to wasm.

## 3. Size note for inline embedding

The reactive demo wasm is about 22 KB because the runtime's inline panic path
pulls in the printf formatting machinery, which gc-sections cannot drop since
the panic is header-inlined and reachable from the zone/world runtime. This is
fine for a fetched .wasm file but too large to inline as base64 in constrained
hosts. A smaller wasm would need a panic-trap build that severs the printf
dependency (a non-inline panic the linker can replace with a trap stub), which
ties back to item 1's non-inline runtime build.
