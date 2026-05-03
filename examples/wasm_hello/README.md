# wasm_hello

Minimal dogfood bridge for the beta WebGL/WASM path.

This is not a stable WebGL language surface. It is a host bridge proof for
emitted C, and future WebGL APIs belong in the `pgy.render.webgl` module
ecosystem track after beta closure.

This example intentionally uses the beta path:

```text
Pergyra -> C backend --emit-c -> optional Emscripten/WebGL bridge
```

It does not require or claim a native LLVM wasm backend, and it does not freeze
renderer syntax or WebGL module APIs. The host imports are declared as
`extern "C"` and are expected to be provided by the JavaScript or embedding
shell when linked with Emscripten.

Smoke gate:

```sh
make dogfood-webgl-test-smoke
```
