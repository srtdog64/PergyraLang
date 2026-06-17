# Codegen Substitution Intent

## Intent

Provide the first Pergyra-written code generation slice for compiler-internal
substitution. The slice is deliberately bounded: it consumes stable `pgy --ast`
text for a small `Int` / `Bool` / `String` / growable `Array<Int>` /
`Array<String>` function subset and emits standalone C whose observable stdout
matches the C/LLVM oracle.

This is a hard self-host rung, not a full backend replacement. Unsupported input
must fail visibly instead of falling through to an unverified translation.

## Input Contract

The tool reads one AST text path from `Args()[0]`. That AST must come from the
live compiler's `pgy --ast` output for committed codegen fixtures. The accepted
subset is:

- one or more `func` declarations with exactly one `Main`;
- `Int`, `Bool`, `String`, `Void`, and growable `Array<Int>` / `Array<String>`
  local surfaces;
- `let`, assignment, `return`, `if` / `else`, `while`, `for`, `break`, and
  `continue`;
- calls, integer arithmetic/comparison/logical expressions, `Log`, `Exit`,
  `ToString`, `Concat`, `StringLength`, `Substring`, `StringIndexOf`,
  `StringTrim`, `FileExists`, `ReadFile`, `Args`, array indexing,
  `ArrayLength`, and `ArraySet`.

## Output Contract

The tool prints one C translation to stdout. The emitted C is not required to
byte-match the C backend. It is required to compile with the platform C compiler
and produce stdout byte-equal to the committed expected output for the fixture.

Out-of-subset input exits non-zero. The current rung proves run-output parity
only; it does not claim memory ownership parity, string freeing, block scoping,
or arbitrary user struct layout.

## Oracle

`src/self_hosted/parity/codegen_parity.sh` builds this tool through the C and
LLVM backends, derives `pgy --ast` text from the live compiler, runs this tool to
emit C, compiles the emitted C, and compares the resulting program stdout with
the committed expected output. The expected output is guarded against drift by
re-running the original fixture through the C backend oracle.
