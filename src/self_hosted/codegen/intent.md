# Codegen Substitution Intent

## Intent

Provide the first Pergyra-written code generation slice for compiler-internal
substitution. The slice is deliberately bounded: it consumes stable self-parser
AST text for a small `Int` / `Bool` / `String` / growable `Array<Int>` /
`Array<String>` function subset and emits standalone C whose observable stdout
matches the C/LLVM oracle.

This is a hard self-host rung, not a full backend replacement. Unsupported input
must fail visibly instead of falling through to an unverified translation.

## Compiler World Binding

- **world_zone**: `EmissionZone`
- **stage_actor**: `ProgramEmitter`
- **stage_intent**: `EmitProgramArtifact`
- **intent_cluster**: `BackendPipeline`
- **payload_contract**: `TypedAstArenaPayloadContractReady`
- **manifest_binding**: `codegen|EmissionZone|ProgramEmitter|EmitProgramArtifact|TypedAstArenaPayloadContractReady`
- **resource_inputs**: `TypeEnvZone`, `AbiLayoutZone`, `TargetCapabilityZone`

## Resource-Zone Shape

Codegen should use zones only where there is distinct resource ownership.
The one-line test is: does this boundary own a distinct resource that others
must access through a view, fact, or intent boundary? If not, it is an action
participant, not a zone.

- `EmissionZone` currently owns the emitted C text buffer.
- `TypeEnvZone` owns type binding facts consumed by emitters as read-mostly
  evidence.
- `AbiLayoutZone` owns ABI/layout facts consumed by emitters as read-only
  evidence. C, LLVM, and self-hosted codegen must not infer field order, tags,
  niches, or pointer ownership from emitted text or backend-local spelling.
- `AbiLayoutOwner` owns self-host C ABI type spelling for the supported
  signature, local declaration, and field subset. It is not the full
  cross-backend row projection; that remains an active expansion surface until
  C/LLVM also consume the same ABI rows.
- `AstTextInventoryOwner` owns the transitional AST-text node inventory and
  declaration/signature row facts. It is an input fact boundary, not a generic
  parser helper.
- `AstTextStatementOwner` owns transitional statement-row facts for control flow,
  bare call statements, and simple/collection mutation statements. Statement
  emitters consume this owner instead of slicing AST text locally.
- `CompilerSymbolTableOwner` owns emitted-symbol spelling rows consumed by the
  self-host C subset. Codegen reads that compiler-world owner directly instead
  of keeping a second C-only mangle owner. Function/method emission and
  namespace-qualified call lowering both consume this owner.
- `CollectionRuntimeOwner` owns self-host C collection runtime helper symbol
  spelling for the supported `Array<Int>` / `Array<String>` subset and the
  bootstrap-only `Array<CodegenAstTextNode>` typed AST-line bridge. The helper
  definitions are still emitted by `program_emit`; the consumers do not spell
  the helper names directly. It is also the only place that normalizes the
  current AST-text bridge spelling `Array<Int: Int>` / `Array<String: String>` /
  `Array<CodegenAstTextNode: CodegenAstTextNode>` into canonical collection
  kind facts.
- `MathRuntimeOwner` owns self-host C math/random helper and target-library
  symbol spelling for the supported `Abs` / `Min` / `Max` / `Sqrt` / `Pow` /
  `Floor` / `Ceil` / `SeedRandom` / `Random` subset.
- `HostIORuntimeOwner` owns self-host C host file/stdin/argv/process runtime
  helper and target-library symbol spelling for the supported file,
  byte-count stdin, directory-walk, `Args()`, and `Exit(Int)` subset. The
  generated helper definitions in `program_emit.pgy` consume its secure-open
  symbol fact so `ReadFile`, `WriteFile`, and handle-based `FileOpen` use one
  POSIX open-time nofollow boundary instead of local direct `fopen` calls.
- `OptionResultRuntimeOwner` owns self-host C Option/Result runtime helper
  symbol spelling for the supported `Option<Int>` / `Result<Int>` subset. The
  helper definitions stay in `program_emit`; expression and statement emitters
  consume this owner instead of locally spelling `pgy_option_*` /
  `pgy_result_*` names.
- `StringRuntimeOwner` owns self-host C string/text runtime helper and
  conversion target-library symbol spelling for the supported builtin rewrite
  subset, including `ToFloat(String)`. The helper definitions stay in
  `program_emit`; expression and statement emitters consume this owner instead
  of locally spelling those `pgy_*` or target-library names.
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

Target split rule: this rung is still the C-emission owner. It must not be
described as a peer C/LLVM/SelfHosted backend replacement until the LLVM and
SelfHosted emission zones consume the same fact rows and feed the same
ArtifactZone comparison contract.

Run-boundary rule: `run/codegen_run_owner.pgy` must consume
`CompilerTargetCapabilityEnvelopeReady()` before `GenerateC`. The target
capability envelope is not documentation-only for this rung; unsupported target
fallback must fail before C emission starts.

Concrete split for the current codegen cluster:

| Candidate | Zone? | Owner reason |
|---|---:|---|
| emitted C text buffer | yes | single mutable output resource |
| type environment | yes | separate read-mostly type-fact resource |
| ABI layout facts | yes | separate read-only layout/ownership-shape fact resource |
| AST text node inventory | bridge owner, not final zone | transitional self-parser AST-text node rows until a tagged AST owner replaces line text |
| AST text statement rows | bridge owner, not final zone | statement facts are consumed from one owner while the text bridge remains active |
| self-host C ABI type spelling | owner, not zone yet | canonical C spelling for supported signatures, locals, and fields |
| symbol/name-mangling facts | compiler-world owner, not codegen zone | read-only canonical spelling rows for supported self-host emission |
| collection runtime helper symbols | owner, not zone yet | canonical C helper names for supported self-host array runtime calls, including the bootstrap typed AST-line record array |
| math/random helper and target-library symbols | owner, not zone yet | canonical C names for supported self-host math/random calls |
| host I/O/process helper and target-library symbols | owner, not zone yet | canonical C names for supported self-host file/argv/process calls |
| Option/Result runtime helper symbols | owner, not zone yet | canonical C helper names for supported self-host Option/Result runtime calls |
| string/text helper and conversion target-library symbols | owner, not zone yet | canonical C names for supported self-host string/text builtin calls |
| program/function/stmt/expr emit files | no | recursive participants over the same output/type resources |

The filesystem mirrors that owner shape without pretending that every action is
a zone:

- `input/` owns AST path/read boundaries plus the transitional AST-text
  inventory and statement-row fact owners.
- `run/` owns the CLI orchestration boundary.
- `text/` owns reusable text and expression scanning facts. Top-level boolean
  operator lookup is an `Option<Int>` fact; consumers must not use `-1` as the
  absence path for that scanner result. Delimiter matching, statement delimiter
  lookup, range delimiter lookup, top-level string concatenation, and top-level
  comma scanners are also `Option<Int>` facts; the emitters must prove presence
  before slicing expression text.
- `type_facts/` owns read-mostly type evidence.
- `abi_layout/` owns self-host C ABI type spelling facts.
- `runtime_abi/` owns self-host C collection, math/random, host I/O/argv, Option/Result, and string/text runtime helper symbol facts.
- `emission/` contains the action participants that write or route emitted C.

## Input Contract

The tool reads one AST text path from `Args()[0]`, with the no-argument
`hello_ast.txt` fixture as the default probe. `input/ast_input_owner.pgy` owns
path selection, the missing-file diagnostic, and the file-read boundary.
`input/ast_text_inventory_owner.pgy` owns raw AST-text line splitting, typed
`CodegenAstTextNode` inventory, indent counting, coarse node
kinds, blank-line filtering, `[export]` line normalization, program/function
declaration routing predicates, declaration collector prepass facts, function
signature/header facts, and cursor expectation checks.
`input/ast_text_typed_arena_owner.pgy` owns parent/indent/child projection into
the typed `AstArena` and the `CodegenTypedAstBridgeReady` guard that consumes
the typed AST arena payload contract before emission.
`input/ast_text_statement_owner.pgy` owns statement-row facts, including
`Let`, `Assign`, `Log`, `Return`, `Defer`, `ArrayPop`, `ArraySet`,
`ArrayPush`, `Exit`, `Break`, `Continue`, `For`, `While`, `If`, `Else`
routing, and bare call statements. `GenerateC` and statement emission consume
those owners and must not recover those facts locally.
Parameter mode is part of that input contract: the parser AST text must preserve
`inout`, `own`, and `ref` parameter rows. This codegen rung consumes `inout`
through function-env `pm` facts, lowers it as value-result copy-in/copy-out, and
rewrites call arguments from those facts. It preserves but fail-closes on `own`
and `ref` until their ABI and ownership facts have owners.
This is a transitional text bridge; the mixed AST-like tagged-node owner remains
an active expansion surface.
`run/codegen_run_owner.pgy` owns the CLI-to-output orchestration that feeds the
owned input into `GenerateC`; it also owns the codegen parity fixture manifest
by walking `src/self_hosted/codegen/fixture` and retaining only rows with paired
`expected/*_stdout.txt` outputs. `main.pgy` only calls that run owner.
`emission/struct_value_emit.pgy` owns struct-valued expression lowering for the
statement paths that need it. That AST comes from the self-host parser for
committed codegen fixtures. The accepted subset is:

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
LLVM backends, builds the self-host parser as the AST producer, gets the active
fixture rows from the compiled tool's `--fixture-manifest` mode, derives AST
text with that parser, runs this tool to emit C, compiles the emitted C, and
compares the resulting program stdout with the committed expected output. The
expected output is guarded against drift by re-running the original fixture
through the C backend oracle.
