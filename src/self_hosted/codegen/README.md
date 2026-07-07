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

## Rung-0..20 (2026-06-24) - active

`main.pgy` is the thin CLI entrypoint. It imports owner modules through
resource-shaped subdirectories:

- `input/` owns AST path selection, file reads, AST-text line inventory, and
  runtime/header usage facts derived from that inventory.
- `run/` owns CLI-to-output orchestration.
- `text/` owns AST/expression text scanning primitives.
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
String builtins: `Concat`, `ToString`, `StringLength`, `Substring`,
`StringIndexOf`, `StringTrim`, `StringJoin`, `Join`. Scalar conversion:
`ToFloat`. Array combinators: `ArraySort`, `ArrayReverse`, `ArrayMap`,
`ArrayFilter` for `Array<Int>` plus unary `Int -> Int` / `Int -> Bool`
function references.
Result values: `Result<Int>` with `Ok`, `Err`, `IsOk`, `IsErr`, `Unwrap`,
`UnwrapOr`, and postfix `?` early-return lowering for `Int` payload lets inside
`Result<Int>` functions.
Option values: `Option<Int>` / `Option<String>` with `Some`, `None`, `IsSome`,
and `UnwrapOption`.
Defer: block-local `Defer: / Block:` scope-exit statements with LIFO ordering
for the supported statement subset.
Tool I/O: `FileExists`, `ReadFile`, `WriteFile`, `Args`.
Deterministic RNG: `SeedRandom(seed)` plus `Random(n)`, with parity fixtures
checking replay semantics instead of pinning a cross-libc random sequence.
Arrays: growable `Array<Int>` / `Array<String>` locals plus `Array<Int>`
parameters and returns.
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

- `Int` param -> `long long`, return -> `long long`
- `Bool` param/return -> `bool` (`<stdbool.h>`); `true` / `false` / `!` pass through
- `Float` param/return -> `double`
- `String` param -> `const char*`, return -> `const char*`
- `Array<Int>` param/return -> `pgy_ai`; `Array<String>` param/return -> `pgy_as`
- struct param/return -> value-passed C typedef for the generated struct
- `Result<Int>` param/return -> value-passed `pgy_result_int`
- `Option<Int>` param/return -> value-passed `pgy_option_int`
- `Option<String>` param/return -> value-passed `pgy_option_string`
- `inout` parameters are preserved by the parser AST text, recorded as per-function
  `pm` facts, lowered as C pointer parameters with local copy-in/copy-out, and
  call arguments are rewritten to `&name` only from that recorded mode fact.
  `own` and `ref` are preserved by the parser but fail closed in this bounded
  self-host C emitter until their ABI and ownership semantics have owners.

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
- `Let: <name> : Result<Int> = Ok(v)|Err(s)|Call(...)` -> value-passed
  `pgy_result_int`; `IsOk` / `IsErr` branch conditions and `Unwrap` /
  `UnwrapOr` integer expressions lower through local helpers. `Let: <name> :
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
  `match_variant` and `match_bindings` facts before this emitter sees the
  lowered `Let: v : Int = UnwrapOption(...)` line.
- `Defer:` -> local block-exit emission in reverse registration order. Return
  paths emit the currently active defer stack before returning; broader
  resource/defer semantics remain owned by the native backend path.

`<intexpr>` / `<cond>` is the C-compatible parenthesized infix the AST printer
produces (`+ - * / %`, comparisons, `&& ||`, `!`, identifiers, `Name(args)`
calls). `Int` is C `long long`, with `/` and `%` matching the oracle including
negatives. Conditions are re-parenthesized so bare predicates (`if flag`) emit
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
`ReadFile` boundary. `input/ast_text_inventory_owner.pgy` owns the transitional
AST-text line inventory consumed by `GenerateC`: raw line splitting, typed
`CodegenAstTextNode` inventory, leading indent counting, coarse
node kinds, empty-line removal, `[export]` line normalization, program/function
declaration routing predicates, declaration collector prepass facts, and cursor
expectation diagnostics live
there, not in emission participants. `input/ast_text_typed_arena_owner.pgy`
owns the parent/indent/child projection into `AstArena` and the bridge readiness
guard. This is a compatibility bridge, not the final typed/tagged AST owner.
`input/ast_text_array_literal_owner.pgy` owns transitional `Let` array literal
shape and top-level element facts so statement emission does not split array
initializer text locally.
`text/enum_literal_owner.pgy` owns payload-free enum literal projection facts
for call arguments and match cases so emission participants consume the env
row instead of rebuilding enum keys or symbols locally.
`text/expr_sequence_owner.pgy` owns top-level comma-separated expression
sequence facts used by array literals, call arguments, and struct literal field
lists so emission participants do not reimplement list splitting.
`text/struct_literal_call_owner.pgy` owns struct literal call-envelope facts:
`Name(...)` recognition plus the typed type-name/inner-payload fact row.
`text/struct_literal_field_owner.pgy` owns the typed struct literal field-entry
fact row, including positional field fallback from collected field rows.
`text/struct_field_access_owner.pgy` owns dotted member-access field spelling
projection from source-space field facts into emitted C field names.
Function signature and statement body emission now
consume this typed node owner for function headers, parameters, return lines,
body markers, and statement reads. Parameter mode spelling (`inout`, `own`,
`ref`) is a fact preserved by the native and self-host AST printers; codegen
consumes that fact through function-env `pm` rows and must not infer mutation
mode from `ArrayPush` or other statement text. `run/codegen_run_owner.pgy` owns the CLI-to-output
orchestration that wires that owned AST text into `GenerateC`; `main.pgy` only
calls the run owner. `emission/struct_value_emit.pgy` owns struct-valued
expression lowering used by `let`, assignment, and return paths;
`emission/stmt_emit.pgy` consumes that boundary instead of owning struct literal
policy directly. `compiler/symbol_table_owner.pgy` owns emitted-symbol spelling
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
`abi_layout/abi_layout_owner.pgy` owns C ABI type spelling for parameter,
return, local, struct/class field, nominal struct type, and empty
parameter-list declarations in the supported subset; emitters must consume that
owner instead of locally mapping `Int` / `String` / aggregate or
empty-signature facts to C spellings.
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
`runtime_abi/option_result_runtime_owner.pgy` owns C Option/Result runtime
helper symbol spelling for the supported `Option<Int>` / `Option<String>` /
`Result<Int>` subset.
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
Option/Result, math, string, and host-I/O call symbols into a runnable
`runtime_call_abi` artifact. `self-host-runtime-call-abi-row-parity-test-smoke`
compiles that projection through C and LLVM when available, so runtime helper
spelling changes are visible as ABI-row diffs instead of backend-local drift.

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
