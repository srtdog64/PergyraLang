# Codegen Substitution Intent

## Intent

Provide the first Pergyra-written code generation slice for compiler-internal
substitution. The slice is deliberately bounded: it consumes stable `pgy --ast`
text for a small `Int` / `Bool` / `String` / growable `Array<Int>` /
`Array<String>` function subset and emits standalone C whose observable stdout
matches the C/LLVM oracle.

This is a hard self-host rung, not a full backend replacement. Unsupported input
must fail visibly instead of falling through to an unverified translation.

## Resource-Zone Shape

Codegen should use zones only where there is distinct resource ownership.
The one-line test is: does this boundary own a distinct resource that others
must access through a view, fact, or intent boundary? If not, it is an action
participant, not a zone.

- `EmissionZone` owns the emitted C text buffer.
- `TypeEnvZone` owns type binding facts consumed by emitters as read-mostly
  evidence.
- `AbiLayoutZone` owns ABI/layout facts consumed by emitters as read-only
  evidence. C, LLVM, and self-hosted codegen must not infer field order, tags,
  niches, or pointer ownership from emitted text or backend-local spelling.
- `AbiLayoutOwner` owns self-host C ABI type spelling for the supported
  signature, local declaration, and field subset. It is not the full
  cross-backend row projection; that remains an active expansion surface until
  C/LLVM also consume the same ABI rows.
- `SymbolMangleOwner` owns emitted-symbol spelling for the self-host C subset.
  It is read-only at this rung; it becomes a zone only if it later owns mutable
  cross-backend symbol state.
- `ProgramEmitter` is the emission participant that drives writes into
  `EmissionZone`; it is not a zone.

`program_emit`, `function_emit`, `stmt_emit`, `expr_rewrite`, and
`struct_value_emit` are participants in the emission action graph, not zones.
They all cooperate over the same output resource or read the same type facts,
so wrapping each file in a zone would be ceremony rather than isolation.

Projection-nerve rule: codegen is the bundle that carries compiler-world facts
into backend artifacts. It does not own a second semantic truth. `TypeEnvZone`
and `AbiLayoutZone` feed the bundle; `EmissionZone` owns the outgoing artifact;
the emitter files are nerves inside that bundle.

Concrete split for the current codegen cluster:

| Candidate | Zone? | Owner reason |
|---|---:|---|
| emitted C text buffer | yes | single mutable output resource |
| type environment | yes | separate read-mostly type-fact resource |
| ABI layout facts | yes | separate read-only layout/ownership-shape fact resource |
| self-host C ABI type spelling | owner, not zone yet | canonical C spelling for supported signatures, locals, and fields |
| symbol/name-mangling facts | owner, not zone yet | read-only canonical C spelling for supported self-host emission |
| program/function/stmt/expr emit files | no | recursive participants over the same output/type resources |

The filesystem mirrors that owner shape without pretending that every action is
a zone:

- `input/` owns AST path/read boundaries.
- `run/` owns the CLI orchestration boundary.
- `text/` owns reusable text and expression scanning facts.
- `type_facts/` owns read-mostly type evidence.
- `symbol_facts/` owns emitted-symbol spelling facts.
- `abi_layout/` owns self-host C ABI type spelling facts.
- `emission/` contains the action participants that write or route emitted C.

## Input Contract

The tool reads one AST text path from `Args()[0]`, with the no-argument
`hello_ast.txt` fixture as the default probe. `input/ast_input_owner.pgy` owns
path selection, the missing-file diagnostic, and the file-read boundary.
`run/codegen_run_owner.pgy` owns the CLI-to-output orchestration that feeds the
owned input into `GenerateC`; `main.pgy` only calls that run owner.
`emission/struct_value_emit.pgy` owns struct-valued expression lowering for the
statement paths that need it. That AST must come from the live compiler's
`pgy --ast` output for committed codegen fixtures. The accepted subset is:

- one or more `func` declarations with exactly one `Main`;
- `Int`, `Bool`, `String`, `Void`, growable `Array<Int>` / `Array<String>`
  local surfaces, and `Array<Int>` parameter/return flow;
- top-level `struct` declarations with `Int` fields, struct literals,
  member reads, struct parameters, and struct returns;
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
or arbitrary nested/mixed struct layout.

## Oracle

`tests/self_hosted/parity/codegen_parity.sh` builds this tool through the C and
LLVM backends, derives `pgy --ast` text from the live compiler, runs this tool to
emit C, compiles the emitted C, and compares the resulting program stdout with
the committed expected output. The expected output is guarded against drift by
re-running the original fixture through the C backend oracle.
