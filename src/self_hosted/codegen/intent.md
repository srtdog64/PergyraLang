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
  evidence. Each value binding carries separate semantic type and runtime-kind
  rows. Expression typing consumes the semantic row; collection and
  Option/Result runtime lowering consumes the runtime-kind row. Neither side
  may reconstruct its fact from the other's spelling.
- `AbiLayoutZone` owns ABI/layout facts consumed by emitters as read-only
  evidence. C, LLVM, and self-hosted codegen must not infer field order, tags,
  niches, or pointer ownership from emitted text or backend-local spelling.
- `AbiLayoutOwner` owns self-host C ABI type spelling for the supported
  signature, empty parameter-list, local declaration, field, and nominal struct
  type subset. It is not the full
  cross-backend row projection; that remains an active expansion surface until
  C/LLVM also consume the same ABI rows.
- `AstTextInventoryOwner` owns the transitional AST-text node inventory and
  declaration/signature row facts. It is an input fact boundary, not a generic
  parser helper.
- Statement routing facts live in `AstTextRowFactOwner` plus typed arena
  projection. The retired `AstTextStatementOwner` must not reappear as a second
  statement truth.
- `CompilerSymbolTableOwner` owns emitted-symbol spelling rows consumed by the
  self-host C subset. Codegen reads that compiler-world owner directly instead
  of keeping a second C-only mangle owner. Function/method emission,
  struct field declaration/literal/access spelling, source-to-C binding names,
  `inout` temporary parameter names, foreach loop temporary names,
  try/match emission temporary names, and namespace-qualified call
  lowering all consume this owner.
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
- `CompilerDriverPipeline` owns the hard source-to-typed-AST boundary. The
  codegen run path consumes `CompileSourceToAstArtifact` so literal kinds and
  expression edges survive without codegen importing parser implementation
  owners.
- `HostIORuntimeOwner` owns self-host C host file/stdin/argv/process entrypoint runtime
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
  subset, including string comparison and `ToFloat(String)`. The helper
  definitions stay in `program_emit`; expression and statement emitters consume
  this owner instead of locally spelling those `pgy_*` or target-library names.
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
| AST text statement rows | bridge owner, not final zone | statement facts are consumed through row-fact owner plus typed arena projection while the text bridge remains active |
| self-host C ABI type spelling | owner, not zone yet | canonical C spelling for supported signatures, empty parameter lists, locals, fields, and nominal struct type names |
| symbol/name-mangling facts | compiler-world owner, not codegen zone | read-only canonical spelling rows for supported self-host emission, including source-to-C binding names consumed by `expr_binding_rewrite_owner`, struct field spellings, `inout`, foreach loop temporaries, and try/match temporary names |
| collection runtime helper symbols | owner, not zone yet | canonical C helper names for supported self-host array runtime calls, including the bootstrap typed AST-line record array |
| math/random helper and target-library symbols | owner, not zone yet | canonical C names for supported self-host math/random calls |
| host I/O/process helper, entrypoint, and target-library symbols | owner, not zone yet | canonical C names for supported self-host file/argv/process calls plus the C process entrypoint |
| Option/Result runtime helper symbols | owner, not zone yet | canonical C helper names for supported self-host Option/Result runtime calls |
| string/text helper and conversion target-library symbols | owner, not zone yet | canonical C names for supported self-host string/text builtin calls |
| program/function/stmt/expr emit files | no | recursive participants over the same output/type resources |

The filesystem mirrors that owner shape without pretending that every action is
a zone:

- `input/` owns AST path/read boundaries plus the transitional AST-text
  inventory, row-fact, typed-arena, array-literal, enum-variant, try-let, and
  indexed-assignment owners.
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
- `compiler/runtime_call_abi_row_owner.pgy` projects those runtime call facts
  and the native Slot/SecureSlot/DeviceSlot MIR resource runtime-call table into
  a stable row artifact; codegen participants keep consuming the domain runtime
  ABI owners, while parity gates consume the projection.
- The component contract rejects quoted runtime helper or target-library call
  spellings in `emission/`, `text/`, and `input/`, so spelling ownership cannot
  drift back into action participants.
- `emission/` contains the action participants that write or route emitted C.

## Input Contract

The tool reads one AST text path from `Args()[0]`, with the no-argument
`hello_ast.txt` fixture as the default probe. `input/ast_input_owner.pgy` owns
path selection, the missing-file diagnostic, and the file-read boundary.
`hir/ast_text_inventory_owner.pgy` owns raw AST-text line splitting, typed
`CodegenAstTextNode` inventory, indent counting, coarse node
kinds, blank-line filtering, `[export]` line normalization, program/function
declaration routing predicates, declaration collector prepass facts, function
signature/header facts, and cursor expectation checks.
`../hir/ast_text_arena_projection_owner.pgy` owns `AstTreeArtifact`
construction and parent/indent/child projection into the typed `AstArena`.
`input/ast_arena_codegen_view_owner.pgy` owns the
`CodegenTypedAstBridgeReady` fail-closed view consumed before emission.
`../parser/expression_graph_owner.pgy` owns array-literal roots and ordered
element edges. The semantic expression codegen view carries that graph and
statement emission consumes each element handle fail-closed.
`../semantic/ast_enum_fact_owner.pgy` owns enum names, ordered variants, and
payload arity. `input/semantic_enum_codegen_view_owner.pgy` projects the
payload-free subset fail-closed; codegen does not read or split enum aux text.
`input/semantic_expression_codegen_view_owner.pgy` projects parser-owned try
nodes and operand edges fail-closed. `emission/try_let_emit_owner.pgy` consumes
that graph and Option/Result ABI facts; codegen cannot recover try structure
from initializer text or a parallel local-binding string row.
`../semantic/ast_assignment_fact_owner.pgy` owns assignment target, receiver,
index, and RHS rows. `input/semantic_assignment_codegen_view_owner.pgy`
projects those rows fail-closed; codegen does not reinterpret assignment arena
payloads.
`../semantic/ast_statement_fact_owner.pgy` also owns `ArrayPush` target/value
and `ArraySet` target/index/value rows. The semantic statement codegen view
projects them fail-closed; no collection mutation AST-text owner exists. One
expected-type expression-graph renderer owns indexed-assignment, `ArrayPush`,
and `ArraySet` values across scalar, String, and struct element types.
`text/enum_literal_owner.pgy` owns payload-free enum literal projection facts
for call arguments and match cases so emission participants consume the env
row instead of rebuilding enum keys or symbols locally.
`type_facts/type_env.pgy` preserves semantic value type (`type`) separately
from runtime value kind (`v`). Function parameters, declared locals, readonly
references, and loop bindings project both rows. Semantic expression graph
typing reads only `type`; a missing row fails closed instead of treating
`OptionInt`, `ArrayInt`, or another runtime spelling as a source type.
`text/expr_sequence_owner.pgy` owns top-level comma-separated expression
sequence facts for array literals, call arguments, and struct literal field
lists while expression payloads remain string-backed.
`text/struct_literal_call_owner.pgy` owns the legacy compact-expression struct
call envelope for explicit non-graph lanes: `Name(...)` recognition plus the
typed type-name/inner-payload fact row. Named struct literals in migrated call
arguments and general local/assignment/return values consume parser-owned
struct/field graph nodes through the semantic graph emitters.
`text/struct_literal_field_owner.pgy` owns the corresponding legacy typed field
entry row while those payloads remain string-backed.
`text/struct_field_access_owner.pgy` owns dotted member-access spelling
projection while member payloads remain string-backed.
Statement-row facts for `Let`, `Assign`, `Log`, `Return`, `Defer`, `ArrayPop`,
`ArraySet`, `ArrayPush`, `Exit`, `Break`, `Continue`, `For`, `While`, `If`,
`Else` routing, and bare call statements live in the row-fact owner plus typed
arena projection. `GenerateC` and statement emission consume those owners and
must not recover those facts locally.
Parameter mode is part of that input contract: the parser AST text must preserve
`inout`, `own`, and `ref` parameter rows. This codegen rung consumes `inout`
through function-env `pm` facts, lowers it as value-result copy-in/copy-out, and
rewrites call arguments from those facts. It preserves but fail-closes on `own`
and `ref` until their ABI and ownership facts have owners.
This is a transitional text bridge; the mixed AST-like tagged-node owner remains
an active expansion surface. `SemanticAstExpressionSurfaceFacts` now owns
normalized top-level operator rows for all three payload lanes. The `Log`
atom-expression path consumes that row through
`semantic_expression_codegen_view_owner.pgy` and
`expr_semantic_shape_emit_owner.pgy`; it does not rediscover the top-level
additive position. Scalar/String returns reuse the atom lane. Ordinary
scalar/String local initializers and assignments consume the value lane.
Root `if`/`while` conditions consume distinct semantic `||`, `&&`, `==`, and
`!=` facts. They now consume stable semantic expression node handles and child
edges for recursive logical/equality structure; codegen cannot split condition
text or call the legacy recursive boolean scanner. Condition graph production
is still a compact-text bridge until the parser emits those same rows directly;
postfix try is already parser-owned and is not part of that bridge.
The codegen arena view is now structural/provenance-only: direct atom, type,
value, auxiliary-value, parameter-type, and parameter-mode accessors are
absent. The remaining blocker is indexed-assignment target-index projection,
Option/Result wrapper internals, special unary forms, non-condition recursive
expression text, and initial compact-tree construction. Those bridges remain inside
named owners rather than reopening a codegen arena read.
`run/codegen_run_owner.pgy` owns the CLI-to-output orchestration that feeds the
owned input into `GenerateC`; it also owns the codegen parity fixture manifest
by walking `src/self_hosted/codegen/fixture` and retaining only rows with paired
`expected/*_stdout.txt` outputs. `main.pgy` only calls that run owner.
`emission/struct_value_emit.pgy` remains a legacy compact-expression owner for
unmigrated lanes. Collection values and general struct-valued local
initialization, assignment, and value return consume expected-type semantic
graph facts. `emission/option_value_emit_owner.pgy` consumes the shared semantic
call spine and expected-type ABI row for `Option<struct>` constructors and
payloads in the `Some` lane. Contextual `None` initialization, reassignment,
and return consume that same expected-type row; C and LLVM native consumers
must obtain the type from MIR local facts, and the parity owner requires the
typed struct-option `None` constructor in emitted C. The semantic struct view
first joins nominal field rows and rejects
unknown, duplicate, missing, or type-incompatible fields. The accepted subset is:

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

The same TestHarness manifest owns a two-parameter payload-enum reject source
and its expected diagnostic artifact. Both C-built and LLVM-built tools must exit non-zero and
match that artifact through the Pergyra output comparator; accepting the input,
dropping payload arity, or recovering it from enum aux text fails this gate.

The TestHarness also owns a role-operator source and expected stdout. Both
backend-built tools must consume semantic role target/method rows, emit a valid
receiver ABI, and match the native C oracle output.

Runtime/header materialization consumes semantic expression-surface facts.
Backend builtin groups remain codegen policy, but codegen may not recover their
presence by reading arena atom/value/auxiliary rows.

Runtime type-family materialization consumes canonical semantic type-surface
facts; direct arena type-name scans are forbidden.

Canonical semantic node-kind facts own both runtime statement-kind
materialization and declaration classification where no richer declaration row
is required. Direct arena kind scans and backend-local kind tag aliases are
forbidden. The aggregate runtime usage projection accepts only semantic
expression, type, and kind facts.

Executable entrypoint cardinality and function-node selection consume ordered
semantic signature facts. Codegen must not rescan arena function names or use
an integer sentinel to recover the selected function.

Statement dispatch consumes three semantic authorities: local-binding rows for
`Let`, assignment rows for `Assign`, and statement-kind rows for all other
emitted statements. Arena predicates may retain `Else`/`Block`/`Then` structure
and provenance, but may not decide semantic statement kinds.

Top-level declaration dispatch consumes semantic node identity: signature rows
for functions, nominal-constructor rows for nominal declarations, role rows for
roles, enum rows for enums, and canonical node-kind rows for ability/event
classification. Codegen arena predicates may not reclassify those declaration
kinds.
