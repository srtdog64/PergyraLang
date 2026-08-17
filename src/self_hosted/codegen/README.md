# Codegen Substitution Track

Pergyra-written C emitters. This is the **first hard compiler-core substitution
track** opened after the 2026-06-17 BDFL decision that lifted the
`docs/self_hosted/README.md` (2026-06-13) "hard compiler-core migration is not
open" freeze. Before that date this folder was a reserved stub.

Architecture note:
[`docs/self_hosted/13_compiler_substrate_architecture.md`](../../../docs/self_hosted/13_compiler_substrate_architecture.md)
is the owner contract for this folder's long-term shape. Codegen is a backend
resource cluster: `TypeEnvZone` owns type facts, `AbiLayoutZone` owns ABI/layout
facts, `EmissionZone` owns emitted C, and `ProgramEmitter` is the participant
that writes through the emission boundary. The current self-host C subset uses
`abi_layout/abi_layout_owner.pgy` for C ABI type spelling and `runtime_abi/`
for supported self-host C runtime helper symbol spelling. The files under
`emission/` are action participants, not separate zones.

## Rung-0..21 (2026-07-12) - active

`main.pgy` is the thin CLI entrypoint. It imports owner modules through
resource-shaped subdirectories:

- `input/` owns AST path selection, file reads, codegen-only arena views,
  expression/kind/type usage fact rows, and aggregate runtime/header usage
  facts derived from HIR rows.
- `run/` owns CLI-to-output orchestration.
- `text/` owns codegen-specific expression text facts. Shared compact AST-text
  scanning belongs to `../hir/ast_text_scan_owner.pgy`.
- `type_facts/` owns type binding facts consumed as read-mostly evidence.
- `../compiler/symbol_table_owner.pgy` owns emitted-symbol spelling rows,
  including struct field spellings in declarations, literals, and member
  access.
- `abi_layout/` owns self-host C ABI type spelling for signatures, locals, and fields.
- `runtime_abi/` owns self-host C collection, math/random, host I/O/argv,
  Option/Result, and string/text runtime helper symbol spelling.
- `emission/` owns participants in the C-emission action graph.

These folders are not a copy of the native C backend topology. `program_emit`,
`function_emit`, `stmt_emit`, `expr_rewrite`, `literal_rewrite`, and
`struct_value_emit` are participants over the same output/type resources, not
separate zones. Together they consume
self-parser AST text for an `Int` / `Bool` / `String` / `Array<Int>` /
`Array<String>` / `Option<Int>` / `Option<String>` / `Void` function subset and emit a
self-contained C program
whose **run-stdout** matches the C/LLVM oracle.

Nominal and enum value declarations are not emitted by a fixed category pass.
`emission/type_declaration_emit_owner.pgy` derives a dependency schedule from
semantic nominal-field and enum-payload type rows, then delegates selected
declarations to the existing emitters. One canonical recursive wrapper
inventory turns concrete `Option<T>` and explicit `Result<T,E>` types into
dependent graph nodes, including nested wrapper dependencies. It does not
generate Option declarations merely because a nominal/enum exists, and neither
wrapper family is appended by a later whole-program scan. Missing wrapper facts
and direct by-value cycles fail closed. The bootstrap C seed mirrors the
dependency and canonical C-suffix policy from MIR declaration headers; neither
path branches on a fixture, variant name, wrapper nesting, or payload arity.
String builtins: `Concat`, `ToString`, `StringLength`, `Substring`,
`StringIndexOf`, `StringTrim`, `StringJoin`, `Join`. Scalar conversion:
`ToFloat`. Array combinators: `ArraySort`, `ArrayReverse`, `ArrayMap`,
`ArrayFilter` for `Array<Int>` plus unary `Int -> Int` / `Int -> Bool`
function references.
Result values: the fixed bootstrap `Result<Int>` row plus explicit
`Result<T,E>` specializations selected once from semantic type usage. `Ok`,
`Err`, `IsOk`, `IsErr`, `Unwrap`, and `UnwrapErr` consume that specialization;
`UnwrapOr` and postfix `?` remain bounded to the existing `Int` payload rung.
Option values: `Option<Int>` / `Option<String>` with `Some`, `None`, `IsSome`,
and `UnwrapOption`.
Defer: block-local `Defer: / Block:` scope-exit statements with LIFO ordering
for the supported statement subset.
Tool I/O: `FileExists`, `ReadFile`, `WriteFile`, `Args`. The installed C route
also lowers the 51 intent-observability calls from the canonical ABI row and
enables `pgy_runtime.h` only when that usage receipt is present; it does not
own a second source-name/runtime-symbol table.
Deterministic RNG: `SeedRandom(seed)` plus `Random(n)`, with parity fixtures
checking replay semantics instead of pinning a cross-libc random sequence.
Arrays: growable `Array<Int>` / `Array<String>` locals plus `Array<Int>`
parameters and returns.
Source builtin calls are recognized in one expression scan by
`emission/runtime_call_rewrite_owner.pgy`; C spelling still comes from the
individual `runtime_abi` owners. The retired per-builtin repeated scans may not
return.
The bootstrap-only typed AST bridge also supports
`Array<CodegenAstTextNode>` through the collection/runtime and ABI owners; that
record-array lane exists to move the codegen input off parallel text arrays,
not to claim arbitrary generic array algorithms over records.
User structs: top-level `struct` declarations with `Int` / `Bool` / `Float` /
`String` fields plus previously declared struct-valued fields, struct literals,
member reads, nested member reads, struct parameters, and struct returns.

**Functions:** one or more functions, exactly one named `Main`. Each emits a C
function; non-`Main` functions get forward declarations, so call order and
recursion are free. `Main` lowers through the host/process entrypoint ABI owner,
including the argv-capable entrypoint when the fixture uses `Args()`.

- `Int` param/return -> `int32_t`; `Long` param/return -> `int64_t`
- `Bool` param/return -> `bool` (`<stdbool.h>`); `true` / `false` / `!` pass through
- `Float` param/return -> `double`
- `String` param -> `const char*`, return -> `const char*`
- `Array<Int>` param/return -> `pgy_ai` with `int32_t` elements;
  `Array<Long>` -> `pgy_al` with `int64_t` elements; `Array<String>` -> `pgy_as`
- struct param/return -> value-passed C typedef for the generated struct
- `Result<Int>` param/return -> value-passed `pgy_result_int`; explicit
  `Result<T,E>` -> one generated tagged value type from `ResultRuntimeFact`
- `Option<Int>` param/return -> value-passed `pgy_option_int`;
  `Option<Long>` -> distinct value-passed `pgy_option_long`
- `Option<String>` param/return -> value-passed `pgy_option_string`
- `Allocator` local -> value-passed `PgyAllocator`; `TextBuilder` local ->
  move-only `PgyTextBuilder` consumed by `Finish` or `Drop`
- `inout` parameters are preserved by the parser AST text, recorded as per-function
  `pm` facts, lowered as C pointer parameters with local copy-in/copy-out, and
  call arguments are rewritten to `&name` only from that recorded mode fact.
- `ref` parameters lower as readonly C pointers and source references consume
  their recorded binding row. `own` uses direct owner-handle carriage for the
  bounded supported value layouts. Neither carriage implies LLVM optimizer
  attributes or a general memory-safety claim.

**Body statements:**

- `Log(<strexpr>)` -> the string runtime log helper; numeric logs consume
  owner-owned target-library format and ABI cast facts before emitting C.
- `Let: <name> : Int|Bool|String|Array<Int>|Array<String> = <expr>` -> typed C declaration.
- `Assign: <name> = <expr>` -> routed by the variable's recorded type.
- `While: (<cond>) { ... }`, `If: (<cond>) Then { ... } [Else { ... }]`,
  `For: <var> in <a>..<b> { ... }` (exclusive upper bound -> C `for`), and
  `Break` / `Continue` -> structural lowering of the AST's indentation-nested
  `Block` / `Then` / `Else` via a recursive emitter.
- `Return: <expr>` / bare `Return:` -> routed by the function's return type;
  bare-return defaults consume ABI owner facts and fail closed when no default
  value is owned for the return type.
- `Let: <name> : Array<Int>|Array<String> = [a, b, c]` -> growable struct
  (`pgy_ai` / `pgy_as`, a `{data,len,cap}`); the literal lowers to `new()` plus
  one `push` per element (`[]` is just `new()`). `ArrayPush(xs, v)`,
  `ArraySet(xs, i, v)`, `ArrayLength(xs)`, and index reads `xs[i]` lower to
  collection runtime helper calls; indexed assignments (`xs[i] = v`) lower
  through the same set helper owner. `ArraySort(xs)` sorts the shared `Array<Int>`
  buffer and returns the value, matching the oracle's value-with-shared-buffer
  behavior; `ArrayReverse(xs)` returns a fresh reversed `Array<Int>` value;
  `ArrayMap(xs, F)` / `ArrayFilter(xs, P)` are supported for unary
  `Int -> Int` / `Int -> Bool` functions. Index reads are rewritten env-aware in arbitrary
  expressions (`total + xs[j]` -> a typed collection get helper), with string
  literals copied verbatim so a `[` inside a string is never touched (rung-7/8/10).
- `Let: <name> : StructName = StructName { field: value, ... }` -> a C
  compound literal routed by collected field-type facts. Struct-returning calls
  can initialize struct locals, and member reads (`p.x`) pass through as C field
  access while `ExprKind` consumes the field-type fact for `Log` / condition
  routing. Struct-valued fields recurse through the same fact-owned literal
  boundary, and dotted reads such as `line.end.x` resolve through field facts.
- `Let: <name> : Result<T,E> = Ok(v)|Err(e)|Call(...)` -> the explicit tagged
  Result specialization owned by `ResultRuntimeFact`; `IsOk` / `IsErr` branch
  conditions and `Unwrap` / `UnwrapErr` select its owned helper symbols. The
  fixed `Result<Int>` row retains `UnwrapOr`. `Let: <name> :
  Int = Call(...)?` inside a `Result<Int>` function lowers to a temporary
  `pgy_result_int`, propagates `Err` with the active defer stack emitted before
  return, and binds the unwrapped `Int` payload on the success path.
- `Let: <name> : Option<Int>|Option<String> = Some(v)|None|Call(...)` ->
  value-passed `pgy_option_int` / `pgy_option_string`; `Some(...)` chooses the
  constructor from the payload expression kind, while typed `None` is emitted
  from the declared/return `Option<T>` type. `IsSome` branch conditions and
  `UnwrapOption` expressions lower through common field-access helpers.
  Match-case `Some(v)` binding is not
  reconstructed from source text here; the MIR JSON path must provide
  `match_variant`, `match_bindings`, and `match_binding_types` facts before
  this emitter sees the lowered `Let: v : Int = UnwrapOption(...)` line. The
  same carried type row owns `Ok(v)` and `Err(e)` bindings.
- `Defer:` -> local block-exit emission in reverse registration order. Return
  paths emit the currently active defer stack before returning; broader
  resource/defer semantics remain owned by the native backend path.
- `TextBuilderNew` / `Append` / `Finish` / `Drop` plus `AllocatorResult` /
  `AllocatorDestroy` lower through owner-projected ABI rows. Program C-unit
  assembly, binding-reference rewriting, and repeated token replacement use
  this path in the emitter itself, so gen2 and gen3 exercise it rather than
  treating it as a fixture-only builtin.

`<intexpr>` / `<cond>` is the C-compatible parenthesized infix the AST printer
produces (`+ - * / %`, comparisons, `&& ||`, `!`, identifiers, `Name(args)`
calls). `Int` is C `int32_t` and `Long` is `int64_t`; `/` and `%` select the
matching checked-width helper, including negative and signed-minimum cases.
Conditions are re-parenthesized so bare predicates (`if flag`) emit
valid C. `<strexpr>` is a string literal, `Concat(<strexpr>, <strexpr>)`, a
string-typed identifier, or a string-returning call. The builtins
`StringLength(x)`, `Substring(s, start, len)`, `StringIndexOf(hay, needle)`, `StringTrim(s)`,
and `StringJoin(xs, sep)` / `Join(xs, sep)` are rewritten to runtime helpers
owned by `runtime_abi/string_runtime_owner.pgy` (`StringIndexOf` remains
`strstr`-based and returns -1 when absent). `ToFloat(s)` lowers through the
string runtime owner to the target `atof` symbol. `Exit(n)` lowers through the
host I/O owner to the target `exit` symbol. `Args()` lowers to a stable user-argv
snapshot (`argv[1..]`) stored in the same growable string-array representation.

**Type routing:** `Assign` / `Log` / `Return` need to know whether an operand is
a string or an integer. The emitter threads a per-function variable type
environment, seeded with parameters and extended by `Let`, plus a global
function return-type table built in a pre-scan. There is no block scoping or
string freeing yet; the guarantee is run-stdout parity, not memory correctness.

Anything outside the subset is an observable `Exit(1)` failure; no silent
fallback. The intent contract is pinned in `intent.md`; this README is
explanatory.

`input/ast_input_owner.pgy` owns the AST path policy (`Args()[0]` or the
no-argument `hello_ast.txt` fixture), the missing-file diagnostic, and the
`ReadFile` boundary. `hir/ast_text_inventory_owner.pgy` owns the transitional
AST-text line inventory used to construct the shared artifact: raw line splitting, typed
`CodegenAstTextNode` inventory, leading indent counting, coarse
node kinds, empty-line removal, `[export]` line normalization, program/function
declaration routing predicates, declaration collector prepass facts, and cursor
expectation diagnostics live
there, not in emission participants.
`../hir/ast_text_arena_projection_owner.pgy` owns the single
`AstTreeArtifact` construction plus provenance and parent/indent/child
projection into `AstArena`. Its temporary node inventory does not cross the
artifact boundary. `input/ast_arena_codegen_view_owner.pgy` owns codegen-only
fail-closed predicates over that arena. This is a compatibility bridge, not
the final typed/tagged AST owner.
`../semantic/ast_artifact_verdict_owner.pgy` owns executable `Main`
cardinality. Program emission accepts that verdict as evidence and is forbidden
from recounting the entrypoint locally.
`../parser/expression_graph_owner.pgy` owns array-literal roots and ordered
element edges. `input/semantic_expression_codegen_view_owner.pgy` carries that
graph and statement emission consumes each element by handle and expected
element type; no local-binding body string or dedicated array view exists.
`input/semantic_expression_codegen_view_owner.pgy` projects the semantic graph
by artifact node and payload lane. `emission/expr_semantic_graph_emit_owner.pgy`
is the only expression-emission consumer; the retired top-level shape emitter
cannot return. Semantic shape rows remain only as a root-text consistency check
on the graph view. `emission/log_emit_owner.pgy` owns log type routing and
scalar formatting ABI. Scalar/String returns reuse the atom graph; ordinary
scalar/String local initializers and assignments consume the value graph.
Indexed assignment, `ArrayPush`, and `ArraySet` values share one expected-type
graph consumer for scalar, String, and struct elements. Unsupported graph
shapes fail closed instead of falling back to the retired shape emitter.
Root `if`/`while` conditions additionally consume separate semantic `||`,
`&&`, equality-position, and equality-kind facts plus stable expression node
handles and child edges. String and payload-free enum equality share one
projection with the legacy leaf path. Recursive condition codegen no longer
splits text; the remaining transition is parser-to-semantic graph production.
`text/enum_literal_owner.pgy` owns payload-free enum literal projection facts
for call arguments and match cases so emission participants consume the env
row instead of rebuilding enum keys or symbols locally.
`text/expr_sequence_owner.pgy` owns top-level comma-separated expression
sequence facts used by remaining compact call and struct literal field bridges.
Hard array-literal emission does not consume it.
`text/struct_literal_call_owner.pgy` owns the legacy compact-expression struct
call envelope for explicit non-graph lanes: `Name(...)` recognition plus the
typed type-name/inner-payload fact row. Named struct literals in migrated call
arguments and general local/assignment/return values instead consume
parser-owned struct/field graph nodes through the semantic graph emitters.
`text/struct_literal_field_owner.pgy` owns the corresponding legacy typed field
entry row, including positional field fallback from collected field rows.
`text/struct_field_access_owner.pgy` owns dotted member-access field spelling
projection from source-space field facts into emitted C field names.
Function signature and statement body emission now
consume this typed node owner for function headers, parameters, return lines,
body markers, and statement reads. Parameter mode spelling (`inout`, `own`,
`ref`) is a fact preserved by the native and self-host AST printers; codegen
consumes that fact through function-env `pm` rows and must not infer mutation
mode from `ArrayPush` or other statement text. `run/codegen_run_owner.pgy` owns the CLI-to-output
orchestration that wires that owned AST text into `GenerateC`; `main.pgy` only
calls the run owner. `emission/struct_value_emit.pgy` remains a legacy
compact-expression owner for unmigrated lanes; collection values and general
struct-valued `let`, assignment, and return paths consume expected-type
semantic graph facts. `emission/expr_semantic_option_value_owner.pgy` owns the
same graph boundary for all contextual `Option<T>` constructors and payloads;
`emission/option_value_emit_owner.pgy` is only the statement adapter.
Constructor identity comes from the semantic direct-call target fact, while
constructor spellings come only from the expected-type runtime ABI row. The shared
semantic struct view validates field names, cardinality, and field value types
against nominal constructor rows before emission. Contextual `None` local
initialization, reassignment, return, and typed call arguments use the same
expected-type ABI row;
the native C and LLVM assignment consumers obtain that expected type from MIR
local facts, and the emitted-C parity ratchet requires
`pgy_option_none_Pair()` rather than an `Option<Int>` fallback.

Runtime array usage also separates declarations from materialization.
`input/nominal_array_usage_owner.pgy` ignores `Array<T>` only when `T` is a
declared generic formal and scans concrete generic specialization actuals for
the array element types that runtime ABI emission must retain. The hard record
array fixture requires this path to materialize `pgy_Point_array`.
`compiler/symbol_table_owner.pgy` owns emitted-symbol spelling
rows for function names, owner-qualified methods, role operator names,
payload-free enum variants, struct fields, source-to-C binding names, `inout`
temporary parameter names, foreach loop temporary names, and try/match emission
temporary names in the supported subset; emitters must
consume that compiler-world owner instead of locally concatenating owner/member,
field, binding, or temporary spellings. Local/parameter/loop declarations record
source-to-C binding rows in `type_facts/type_env.pgy`, and
`emission/expr_binding_rewrite_owner.pgy` consumes those rows before expression
emission so C-reserved source names do not reopen a backend-local spelling path.
Projection also fails closed if the symbol row envelope is not ready.
`emission/function_binding_env_owner.pgy` owns one function-value binding fact
that joins source name, semantic type, runtime kind, C name, and environment
rows. Definitions, prototypes, parameters, locals, and assignment probes
consume that same fact. Assignment emission only reads `cbind`; it does not
treat target text as a C name or invoke the symbol projection again.
Let, try-let, loop-variable, and collection-mutation statement paths consume
the same owner-directed C binding. Collection mutation accepts only `cbind`:
readonly-reference rows already encode their dereference there, so raw `cref`
is not a compatible fallback.
`abi_layout/abi_layout_owner.pgy` owns C ABI type spelling for parameter,
return, local, struct/class field, nominal struct type, and empty
parameter-list declarations in the supported subset; emitters must consume that
owner instead of locally mapping `Int` / `String` / aggregate or
empty-signature facts to C spellings.
`runtime_abi/text_builder_runtime_owner.pgy` owns Allocator/TextBuilder call
spelling. `runtime_abi/runtime_header_owner.pgy` composes the canonical runtime
inline owners for allocator, TextBuilder, and allocator-backed BoxArray
consumers; generated C no longer carries a second hand-written layout or
implementation block. The compiler runtime-call artifact carries the same
operations and call shapes; emitters do not invent helper names locally.
`runtime_abi/collection_runtime_owner.pgy` owns C collection runtime helper
symbol spelling for the supported `Array<Int>` / `Array<String>` subset and the
bootstrap-only `Array<CodegenAstTextNode>` typed AST-line bridge. It also
normalizes the current AST-text bridge spellings `Array<Int: Int>` /
`Array<String: String>` / `Array<CodegenAstTextNode: CodegenAstTextNode>` to
canonical collection kind facts at that owner boundary.
`program_emit.pgy` remains the generated helper definition host; expression and
statement emitters must consume `collection_runtime_owner.pgy` instead of
locally spelling `pgy_ai_*` / `pgy_as_*` helper names.
`runtime_abi/math_runtime_owner.pgy` owns C math/random helper and
target-library symbol spelling for the supported `Abs` / `Min` / `Max` /
`Sqrt` / `Pow` / `Floor` / `Ceil` / `SeedRandom` / `Random` subset.
`runtime_abi/host_io_runtime_owner.pgy` owns C host file/stdin/argv runtime helper
symbol spelling for the supported file, byte-count stdin, directory-walk, and
`Args()` subset.
`abi_layout/enum_abi_value_fact_owner.pgy` owns one semantic-enum-to-C-value fact
for payload-free and tagged layouts, including the bare-return default.
General ABI layout and `Option<T>` materialization consume that fact; they do
not reclassify enum layouts independently.
`runtime_abi/option_result_runtime_owner.pgy` owns C Option/Result runtime
helper symbol spelling and concrete Option value facts for the supported
scalar, nominal, enum, Result, and nested Option payloads.
`runtime_abi/result_runtime_owner.pgy` owns the C type and helper symbols for
each explicit `Result<T,E>` usage, while
`emission/result_runtime_emit_owner.pgy` materializes that tagged definition.
It is one type-directed runtime projection, not one emitter per fixture.
`program_emit.pgy` remains the generated helper definition host; expression and
statement emitters must consume `option_result_runtime_owner.pgy` instead of
locally spelling `pgy_option_*` / `pgy_result_*` helper names.
`runtime_abi/string_runtime_owner.pgy` owns C string/text runtime helper and
target-library symbol spelling for the supported builtin rewrite subset
(`Concat`, string length/search/trim/replace/case/join/subspan helpers, string
comparison, `ToString`, `ToInt`, `Print`, and string `Log`). `program_emit.pgy`
remains the generated helper definition host; expression and statement emitters
must consume `string_runtime_owner.pgy` instead of locally spelling those
`pgy_*` or target-library helper names.
Expression/statement emitters should not locally spell `pgy_*` runtime helper
names or supported target-library call names. `strcmp`, `sqrt`, `pow`, `floor`,
`ceil`, `atof`, and `exit` are owner facts consumed by emission participants.
`compiler/runtime_call_abi_row_owner.pgy` projects those collection,
Option/Result, math, string, and host-I/O call symbols plus the native
Slot/SecureSlot/DeviceSlot MIR resource runtime-call table into a runnable
`runtime_call_abi` artifact. The artifact records call shape as an owner fact,
so runtime helper spelling and argument/return shape changes are visible as
ABI-row diffs instead of backend-local drift.
`self-host-runtime-call-abi-row-parity-test-smoke` compiles that projection
through C and LLVM when available.
`self-host-component-contract-test-smoke` also rejects quoted `pgy_*` runtime
helper names and supported target-library call names under `emission/`,
`text/`, and `input/`; new call spellings must enter through `runtime_abi/`
owners before codegen participants can consume them.

`fixture_manifest_owner.pgy` owns the committed codegen parity fixture frontier.
The run boundary emits that manifest, and later hard-substitution gates consume
the same owner instead of carrying parallel fixture lists.

Parity gate: `tests/self_hosted/parity/codegen_parity.sh` builds `main.pgy`
through the requested backend set, builds the self-host parser as the AST text
producer, asks `RunCodegenFromArgs --fixture-manifest` for the active codegen
fixture inventory, runs codegen on each parser-produced AST text artifact,
gcc-compiles the emitted C, runs it, and compares run-stdout against the
committed expected output. A live-drift guard re-derives that expected output
from the C-backend oracle executable. LLVM is mandatory when the compiler build
supports LLVM and explicitly skipped for C-only platform CI. Run
`make self-host-codegen-parity-test-smoke`.

The run-output equivalence criterion, not byte-equal C, follows
`src/self_hosted/PROGRESS.md` roadmap step 6: the oracle emits MIR-lowered C with
runtime headers; the Pergyra emitter produces standalone C. They are judged equal
only by observable program behavior.

Golden/platform contract: `hello` is the no-argument fixture. It must remain in
the active codegen fixture manifest emitted by `RunCodegenFromArgs
--fixture-manifest` and must pass when `PGY_SELFHOST_CODEGEN_BACKENDS=c` is the
only requested backend. The parity runner therefore builds command arrays with
the executable as element 0 instead of expanding a possibly empty argument
array; this keeps macOS bash 3.2, Git Bash, and Linux bash behavior aligned
under `set -u`.

## Next Rungs

1. string freeing / arena ownership and block scoping (memory correctness, not
   just run-stdout parity)
2. broader nested AST-node shapes
3. broader MIR-JSON driven codegen substitution, so new surfaces enter through
   `mir_lower` facts before they reach the AST-text compatibility bridge

LLVM emission substitution is later than C emission.
