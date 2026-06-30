# Self-Host Progress

**This is the canonical progress measurement for Pergyra self-hosting.**
The number that matters is *how much of the C/LLVM compiler has been
substituted by Pergyra-written equivalents* -- not how many peripheral
audit tools exist.

Last updated: 2026-06-26

## Headline Number

**Hard self-host contract (2026-06-22):** hard self-host is now gated as
staged substitution rather than tracked as a separate cleanup project. The
contract lives in `docs/self_hosted/10_hard_self_host_contract.md`, and
`tests/self_host_hard_contract_smoke.sh` keeps the docs, Makefile wiring, active
hard rungs, C oracle, LLVM oracle, bridge/fallback split, codegen bootstrap, and
MIR JSON fact-only lowering aligned. The substitution percentage below is
unchanged by that contract gate; future percentage increases require a Pergyra
implementation to replace a real compiler stage/pass beside the C/LLVM oracle.

**Compiler-internal substitution: ~6.40% source-tree LOC-scale** (15,914 tracked Pergyra LOC vs 248,794
C LOC across `src/lexer/`, `src/parser/`, `src/semantic/`, `src/codegen/`,
`src/runtime/`, `src/compiler/`, `src/lsp/`). The verified substitutes are the
lexer, parser, a bounded semantic verdict rung, and -- as of 2026-06-17 -- the
**first codegen rungs** (`src/self_hosted/codegen/`, 4,394 LOC; rung-0 string Log,
rung-1 integer let/arithmetic, rung-2 assign + `while`/`if`/`else`, rung-3
multi-function definitions + calls + `return`, rung-4 `String` types with a
variable/function type environment + runtime `pgy_concat`, rung-5 `for` loops +
`break`/`continue`, rung-6 `Bool` type + `StringLength`/`Substring` builtins,
rung-7/8 fixed `Array<Int>`/`Array<String>` literals + indexing +
`ArrayLength`/`ArraySet`, rung-9 `StringIndexOf` builtin + `Exit`, rung-10
**growable arrays** (`ArrayPush`) via a `{data,len,cap}` struct rep with
env-aware index-expression rewriting, rung-11 `StringTrim` builtin, rung-12
`FileExists`/`ReadFile` file I/O, rung-13 `Args()` user-argument snapshots,
rung-14 value-passed Int-field structs, rung-15 `Array<Int>` param/return flow,
rung-19 typed `Int` / `Bool` / `Float` / `String` struct field facts, and rung-20 nested struct-valued field facts).
The codegen entrypoint is now split into thin `main.pgy` orchestration plus
resource-owner folders: `input/` for AST path/read ownership and AST-text line
inventory ownership, including marker-node predicates and
function/return/enum/nominal/role/parameter/field payload accessors plus
simple statement fact accessors (`Let`, `Assign`, `Log`, `Return`, `Defer`,
`ArrayPop`, `Exit`, `Break`, `Continue`) for the transitional `pgy --ast`
bridge, `run/` for the
CLI boundary, `text/` for text/expression scanning, `type_facts/` for type
evidence, compiler-world symbol rows for emitted-symbol spelling,
`abi_layout/` for self-host C ABI type spelling, `runtime_abi/` for `Array<Int>` /
`Array<String>` plus bootstrap `Array<CodegenAstTextNode>` C collection runtime
helper symbol spelling, supported
math/random helper symbol spelling, supported host file/argv helper symbol
spelling, supported `Option<Int>` / `Result<Int>` helper symbol spelling, and
supported string/text runtime helper symbol spelling, and `emission/`
for C-emission action participants. That keeps
`program_emit`, `function_emit`, `stmt_emit`, `expr_rewrite`, and
`struct_value_emit` out of fake zone folders while still making the real
resource owners visible. Parameter-mode facts (`inout` / `own` / `ref`) now
survive `pgy --ast`; the self-host C codegen consumes `inout` from function-env
`pm` facts and lowers it as value-result copy-in/copy-out instead of guessing
from `ArrayPush` or other statement text. The real-source semantic selfcheck now accepts 87
self-host owner/source files through both C and LLVM. The rest of codegen,
runtime, compiler driver, and LSP
substitution are still 0%; the MIR-lowering substitution has now *started* (see
below).

**MIR-lowering substitution started (2026-06-18, path (a) rung-0b):** the C
compiler now emits MIR JSON (`pgy --mir-json`, schema `pgy.mir.v1`) with the CFG
skeleton, explicit expression/source-shape facts (`expr0`, `expr1`,
`source_type`), source-local type facts (`source_locals`), and a transitional
`ast` compatibility text field captured by the MIR source-shape owner. A new
Pergyra tool `src/self_hosted/mir_lower/` consumes that JSON and reconstructs the
`--ast` tree, which the existing codegen lowers to C. The whole MIR -> C path is
now Pergyra and run-equivalent to the C backend on the supported rung-0b CFG
subset (linear code, signatures/return, if/else, nested if, while, and
`for i in a..b`), plus selected codegen fixture surfaces that already lower from
MIR facts (args, arrays, Bool/string/Float builtins, Bool-literal branch
reassignment, straight-line calls, direct integer arithmetic, builtin-name
string literals, directory walking, exit-guard branches, multiple Void routines
with bare-call statements, string concat/equality, `Result<Int>` `?`
early-return flow, recursion, loop-control
`continue`/`break` edge blocks, trailing-newline Log normalization, nested
string concatenation, string array concatenation, string case/index/trim
builtins, `Join`/`ToFloat` string utility flow, array pop, array for-each,
array sort/map/filter/reverse combinators, `Result<Int>` core constructors and
inspection helpers, typed struct field declarations/value flow,
plain class/subject/object/tobject/vessel declarations and class methods through MIR-owned
nominal-kind/field/method/owner facts,
payload-free enum declarations through MIR-owned variant facts,
break edges after non-empty statement blocks, inferred `Random()` Int locals,
match-case integer pattern conditions, runtime-aligned absolute-path I/O policy,
file read/write, and phi-bearing loop headers classified by CFG backedges rather
than phi presence alone, plus MIR-owned array destructure binding facts), gated by
`parity/mir_json_parity.sh`
(`make self-host-mir-json-parity-test-smoke`, 85 fixtures plus 0 clean-reject
fixtures). The gate now
requires the MIR JSON fact surface and checks the `for`
header is reconstructed from `arg0` plus `expr0`/`expr1` bounds, and checks
struct/class declarations, nominal family declarations, owner-qualified class methods, payload-free enum
variants, match-case integer branch conditions reconstructed from
`match_patterns`, and `Option<Int>` `Some(v)`/`None` branches reconstructed from
MIR-owned `match_variant` and `match_bindings` facts. It also checks nested `if`
branches inside loops are not misclassified as loops from phi facts alone. The
gate checks destructure binding-name facts and rejects unsupported declaration
facts before generated C emission. It rejects
reintroducing reads of the transitional `ast` compatibility text. This is the
first verified slice of the actual compiler-core (~96% of the LOC), not the
codegen subset. It is now fact-only for the supported MIR JSON statement,
expression, source-local, CFG, match-case, I/O policy, typed struct field
declaration, field-only class/subject/object/tobject/vessel declaration/method,
ability signature declaration, payload-free enum surfaces, and the Int role
operator dispatch surface. The committed MIR-lower/codegen fixture inventory is
currently **85 PASS / 0 gap plus 0 clean rejects** through this
path. The nominal family now flows through MIR-owned `nominal_kind`/field facts
and reconstructs `Class:` / `Subject:` / `Object:` / `TObject:` / `Vessel:`
instead of collapsing those labels to a generic class alias. Ability
declarations now flow through MIR-owned method signature facts and are treated
as zero-artifact declaration hosts by the self-host codegen pre-passes. Role
declarations now flow as MIR-owned `kind:"role"` facts with `for_type`, impl
ability spans, and method signature facts; the supported Int/`Arithmetic.Add`
operator path is consumed by self-hosted MIR lowering/codegen instead of being a
clean-reject boundary.
Richer projection/identity semantics beyond field-only nominal
declarations and payload enum variants remain observable boundaries, so the
self-host path fails closed instead of silently
dropping operator-overload/domain nominal semantics or emitting undefined C
symbols. New fixtures must preserve that by adding owning facts rather than
text fallback.
`self_hosted_component_contract_smoke` now also ratchets that frontier against
the parity harness itself: the MIR JSON positive fixture inventory must stay at
85, the clean-reject inventory must stay at 0, the scorecard must cite the same
85 PASS / 0 gap plus 0 clean reject boundary, and stale fixture-count wording
is rejected. The positive inventory now includes `examples/binary_search.pgy`
as an example-origin fixture, not only purpose-built self-host/codegen fixtures.

The self-hosted `mir_lower/` implementation is now split by source-of-truth
owner rather than living as one monolithic `main.pgy`: `error_owner` owns the
diagnostic boundary, `mir_json_input_owner` owns argv/file/schema input gating,
`json_fact_read` owns bounded JSON/MIR fact access, `decl_lower` owns declaration
inventory reconstruction, `program_lower` owns document-order Program assembly
and supported routine selection, `routine_inventory_owner` owns routine
discovery and bounded routine header facts, `routine_lower` owns CFG/body
reconstruction for a selected routine, and `stmt_render` owns instruction fact
-> AST statement rendering. The entrypoint `main.pgy` is orchestration only, and
each `mir_lower` source file is below the 600-line owner cap.

**Hard migration opened (2026-06-17):** the codegen rung is the first *hard
compiler-core* substitute, landed after the BDFL decision lifted the
`docs/self_hosted/README.md` freeze. Hard migration proceeds rung-by-rung, each
gated against the C/LLVM oracle before the next opens -- not as an unverified
compiler fork. See `src/self_hosted/codegen/README.md`.

**Self-hosting achieved for codegen (2026-06-17, strengthened 2026-06-26):**
the codegen tool *self-hosts*. A Pergyra-built copy of the owner graph emits C
that gcc-compiles and **reproduces its own source-compilation exactly** --
`gen2 == gen3` byte-identical (last observed 3971 generated-C lines) -- and the
Pergyra-built tool emits byte-identical C to the oracle-built tool on the sample
fixtures. Breadth: the same codegen also compiles the lexer (587 lines) and parser (3338 lines); each codegen-built binary matches its oracle-built counterpart on a sample source -- three real self-host components self-built. Wider survey: the codegen compiles **all 22 of 22** committed self-host components/tools to valid C, each verified run-equivalent to the oracle-built binary on a sample -- the entire committed self-host toolchain (lexer, parser, semantic, codegen itself, + 18 audit tools) is self-built by the Pergyra-written codegen. This includes namespace-imported audit tools (`TextScan::` qualified calls, flattened to `NS_Func` -- import/namespace + DirWalk support added). The earlier 18/22 ceiling was a `pgy --ast` bug (for-each `for x in lines` rendered as `For: x in (null)..(null)`, dropping the collection); FIXED in src/parser/ast_print.c (emit the iterable) + the self-host parser, regenerated 5 parser fixtures, and added for-each lowering + bare-void-return + word-boundary builtin matching to the codegen. The latest hard gap was another `pgy --ast` fact-loss bug: parameter modes were dropped, so `inout` arrays were printed as value parameters and the generated code copied mutations into dead locals. FIXED by preserving `inout` / `own` / `ref` in the native and self-host AST printers, recording codegen `pm` facts, lowering `inout` signatures/calls from those facts, and adding the bootstrap-only `Array<CodegenAstTextNode>` record-array substrate. Parser parity (186 sources) stays byte-equal. The bootstrap gate verifies codegen self-hosts (gen2==gen3) + builds lexer + parser + semantic + mir_lower + 13 audit tools and the backend fuzz generator, all matching oracle-built. Gated by `parity/codegen_bootstrap.sh`
(`make self-host-codegen-bootstrap-test-smoke`).

Reaching the fixpoint drove out and fixed real gaps: `else if` chains,
string-literal-safe builtin rewriting, recursive `Concat`/`ToString`/call-argument
lowering (`Concat` -> `pgy_concat` is a pure name rewrite -- same args -- so it
lowers anywhere), bare-call statements, **string `==`/`!=` -> `strcmp(...)==0`**
(C `==` on `char*` compares pointers; the silent root cause of a non-working
first attempt), and a latent **forward-declaration bug** -- Pergyra arrays pass by
value with a shared element buffer, so `ArraySet` persists across calls but
`ArrayPush` does not; the per-`EmitFunction` `protos` push never reached
`GenerateC`, leaving prototypes empty (fixtures worked only because callees
precede callers). Fixed with a `CollectProtos` pre-pass.

This is the codegen *component* self-hosting, not the whole compiler: the
self-host codegen is a standalone AST->C emitter for the supported subset, not a
replacement of the C backend's MIR-lowering. HIR/MIR, the rest of codegen,
runtime, compiler driver, and LSP remain 0%.

**Real-example round-trip (2026-06-17):** beyond the 35 hand-written parity
fixtures, the codegen tool was surveyed against all 118 `examples/*.pgy`. It
compiles **20** to run-stdout-equal output vs the oracle (binary_search,
hash_map, linked_list, queue, deque, graph_bfs, insertion_sort, union_find,
break_continue, for_test, class_test, etl_workflow, hello, + 7 contract/
projection/transfer minimals); 86 are correctly rejected as out-of-subset with an
observable `Exit(1)`; 12 fail under the oracle itself (C-skip). Two bugs surfaced
from the C/LLVM/Pergyra tri-compare: (1) a **codegen self-bug** -- `Log(<int>)`
logged directly (not via `ToString`) was emitted with `%s`; fixed by routing
`Log` / array-index element types through `ExprKind` (silent-failure examples
11 -> 0). (2) an **oracle bug** -- the C and LLVM backends miscount arity for
`Array<String>` parameters (the self-host emitter handles them correctly); filed
separately.

**Lexer parity (2026-06-23):** the committed lexer gate compiles the
Pergyra-origin lexer through both C and LLVM, then proves byte-equal token
output and live `pgy --tokens` drift on 7 source fixtures:
`hello`, `array_literal`, `break_continue`, `basic`, `heap`, and
`binary_search`, plus backend-compare `string_escape_sequences`. `main.pgy` is
now only the entrypoint; character/codepoint classification, token keyword/line
rendering, and the scan loop live in `char_owner.pgy`, `token_owner.pgy`, and
`scan_owner.pgy`; lexer tool input is only `Args()[0]` or the no-arg
`examples/hello.pgy` default. The broader lexer scale probe now measures
**993 of 993** examples + backend_compare sources byte-equal to the C lexer
oracle.

**Parser at scale (2026-06-23):** the Pergyra-origin parser produces
byte-equal output vs `pgy --ast` on **120 of 121** committed
`examples/*.pgy` files. There are now **zero byte-drift cases** in the
scale probe: every example that both the live C oracle and the self-host
parser complete is byte-equal. There are also **zero self-host parser exits**;
the one remaining non-match fails under `pgy --ast` itself and is a C-skip
(`secure_slots`). The scale probe is a
coverage probe, not a hard parity gate, but it now fails closed: it removes any
stale generated parser binary before compile and exits if compile does not
produce a runnable parser. The probe and parser entrypoint consume source paths
only through `Args()[0]`; the old `fixture/source.txt` side channel is retired.
The file-based probe exposed an `if let Some(resource)` payload loss in the
self-host parser's generated C, now closed by `ParseIfLetPayload` returning the
payload fact instead of relying on branch-local `String` reassignment.
Previous historical match counts:
105 -> 86 -> 83 -> 80 -> 79 -> 77 -> 72 -> 72 -> 63 -> 59 -> 58 -> 57 -> 53 -> 48 -> 46 -> 43 -> 37 -> 25 -> 11.
Refresh:
`bash tests/self_hosted/parity/parser_scale_probe.sh --failing`.

**Rung-1 parity (2026-06-16):** the committed
`parser_parity.sh` `SOURCE_PAIRS` array now exercises **186
sources** vs `pgy --ast` on both generated C and LLVM parser binaries
(was 83 on 2026-05-29; +103 overall). The added fixture surface covers Option/Result
destructure, slot sugar, transfer short syntax, array literal,
common collection algorithms (queue, stack, deque, heap,
linked_list, hash_map, union_find, graph_bfs), string + stdlib +
io + math builtin surfaces, async/spawn/select/defer/for control
flow, pipe + try operator, ownership /
concurrency / event demos (event_basic, event_minimal,
event_lambda, event_lambda_full, event_closure_probe),
notebook_style_analysis, tagged_union, battle_*, calendar_*,
beta_*, intent contract minimal shapes, authority contract,
action contract inheritance, generic ability requires, four
ad-hoc bsd_test fixtures and the full 11-file bsd_test{,2..11}
family, qubit_test, qubit_quantum_ext, RemoteFuture, for_in_array,
generic_class, subject_object_tobject, vessel_method_test,
test_parallel, eda/etl workflows, channel_parallel,
producer_consumer, projection_*, collections_closure_probe,
class_method_test, channel_test, spawn_blocking_test,
import_test, slots, slots_simple, and a deep nested generic type fixture
(`HashMap<String, List<HashMap<Int, Array<String>>>>`). The parser parity
gate now compiles the Pergyra-origin parser through both C and LLVM before
comparing each fixture byte-for-byte.

Examples that **cannot** be added as fixtures (current state):
- `pgy --ast` itself fails (skipped):
  `secure_slots`.
- Self-host parser byte-drifts vs live `pgy --ast`:
  none as of the 2026-06-22 scale probe.
- Self-host parser exits before producing byte-equal AST:
  none as of the 2026-06-22 scale probe.

Reading this honestly: the self-host journey has *just started*. The
first compiler-internal substitute (`src/self_hosted/lexer/`) lands a
Pergyra-written lexer that handles the measured examples + backend_compare
token surface byte-for-byte.
The parser (`src/self_hosted/parser/`) follows at ~52%: it covers a real
domain grammar subset and has C/LLVM byte-equal parser parity over the
committed fixture set, but still stops short of the remaining scale-probe
exit list and the full parser recovery surface.

Compiler-stage substitutes mirror the C-side `src/<component>/` layout
as siblings of `src/self_hosted/` (`lexer/`, `parser/`, `semantic/`,
`codegen/`, `air/`, `hir/`, `mir/`, `compiler/`, `runtime/`, `lsp/`).
Everything listed under `src/self_hosted/tools/` is *peripheral audit
tooling*. Those tools do not substitute any compiler component; they
only observe text artifacts the C compiler produces. Their LOC is
**not** counted in the substitution percentage.

## Component Coverage

| Component       | C LOC   | Pergyra LOC | Coverage | Status            |
|-----------------|---------|-------------|----------|-------------------|
| `src/lexer/`    |     921 |         677 | measured corpus parity | **993 of 993 sources byte-equal** (examples + backend_compare). `main.pgy` is orchestration only; source input, character/codepoint handling, token classification/output formatting, and scan-loop state are separate SoT owner modules. `scan_owner.pgy` now declares its real owner dependencies (`char_owner.pgy`, `token_owner.pgy`) and is part of the real-source semantic selfcheck set. Escaped strings, interpolation, and doc/block comments are covered by the measured corpus. 7 representative sources are committed as parity fixtures. |
| `src/parser/`   |   20579 |        8127 | ~52%     | `src/self_hosted/parser/` parses 186 committed fixtures byte-equal `pgy --ast` on both C and LLVM parser binaries, and **120 of 121** `examples/*.pgy` byte-equal at scale (2026-06-22; zero byte-drift, zero self-host parser exits, 1 C-skip). Parser ownership is now split into declaration, expression, statement, import/source, cursor, type-name, diagnostic, and tree-text owners; `main.pgy` is parser-tool entrypoint orchestration only. |
| `src/semantic/` |   46203 |        2716 | rung-2 subset | Checks a bounded function-body subset against the C compiler oracle on C/LLVM-generated binaries across 107 fixtures. `main.pgy` is orchestration only; CLI diagnostic/run boundary, diagnostic-code vocabulary, C oracle code mapping, source-bundle/import expansion, source scanning, diagnostic rendering, local environment lookup, expression typing, expression diagnostics, call checking, body/function checking, and program checking live in named owner modules. |
| `src/codegen/`  |  107123 |        4394 | rung-0..20 | **C-emit rung-0..20 (2026-06-24).** Pergyra emitter consumes `pgy --ast` text and emits standalone C for the supported scalar/string/array/result/option/struct/defer/file/argv/random/float subset. `ast_input_owner.pgy` owns AST path selection, `ast_text_inventory_owner.pgy` owns the typed `CodegenAstTextNode` bridge inventory, program-level declaration routing, declaration collector prepasses, function signature/header facts, statement body reads, `inout`/`own`/`ref` parameter-mode preservation, and cursor expectations, `codegen_run_owner.pgy` owns CLI-to-output orchestration, `type_facts/` owns type routing, `compiler/symbol_table_owner.pgy` owns function/method/operator/enum emitted-symbol rows, `compiler/abi_layout_row_owner.pgy` owns supported concrete ABI rows and `abi_layout/abi_layout_owner.pgy` consumes those rows for parameter/return/local/field C ABI spelling before user-struct lookup, `runtime_abi/collection_runtime_owner.pgy` owns supported array runtime helper call spelling including the bootstrap `Array<CodegenAstTextNode>` lane, `runtime_abi/math_runtime_owner.pgy` owns supported math/random runtime helper call spelling, `runtime_abi/host_io_runtime_owner.pgy` owns supported host file/argv runtime helper call spelling, `runtime_abi/option_result_runtime_owner.pgy` owns supported Option/Result runtime helper call spelling, `runtime_abi/string_runtime_owner.pgy` owns supported string/text runtime helper call spelling, and `emission/` contains action participants. `lib/json.pgy` now owns JSON string/field/object/array emission for self-host tools while schema object shape remains tool-owned. **63 fixtures run-stdout equal** to the C/LLVM oracle on tools built through both backends; bootstrap fixpoint is `gen2 == gen3`. Gate: `parity/codegen_parity.sh` (`make self-host-codegen-parity-test-smoke`) and `parity/codegen_bootstrap.sh` (`make self-host-codegen-bootstrap-test-smoke`). Out-of-subset input is an observable `Exit(1)`. |
| `src/runtime/`  |   29627 |           0 | 0%       | native runtime kernel stays C; portable runtime policy libraries may move later |
| `src/compiler/` |   43304 |           0 | 0%       | compiler-world contracts exist in Pergyra, but native compiler driver replacement has not started |
| `src/lsp/`      |    1037 |           0 | 0%       | not started       |
| **Total**       | **248794** |  **15914**  | **~6.40% source-tree LOC-scale** | lexer/parser/semantic + codegen rung-0..20; MIR JSON lowering and compiler-world contracts are tracked separately above |

Notes:

- *Coverage %* is a rough functional estimate, not a LOC-equivalence
  number. The lexer is 646 LOC and is judged by byte-equal fixture coverage,
  not by line-count parity with the C lexer.
- *Runtime kernel stays C* by current design: allocator/OS/thread/panic/slot
  exports are what the target Pergyra program links against, so substituting
  that native kernel in Pergyra would create a bootstrap cycle. Counted as 0%
  intentionally. Runtime-adjacent Pergyra tools count as soft self-host evidence.
  They remain outside compiler-internal substitution until a Pergyra-written
  runtime component is linked into generated programs.
- `src/lsp/` is the Language Server Protocol implementation. Lower
  priority than the core compiler.

## Peripheral Audit Tools (Not Counted In Coverage)

These 18 tools live in `src/self_hosted/tools/` but do **not** count
toward compiler-internal substitution. They are dogfood validators
that read text artifacts and emit drift verdicts; the C compiler
keeps running fine with or without them.

| Tool                              | LOC (Pergyra) | Function |
|-----------------------------------|---------------|----------|
| `diagnostic_catalog_checker`      | 303           | docs/72 vs diag_codes.h drift |
| `stable_subset_section_checker`   | 122           | docs/107 canonical anchors |
| `air_graph_json_validator`        | 314           | `pgy --air-json` shape gate |
| `air_graph_id_uniqueness`         | 132           | AIR graph duplicate node-id check |
| `air_graph_node_count_integrity`  | 140           | live AIR graph id-count summary check |
| `air_graph_ref_live`              | 138           | live AIR graph back-reference range check |
| `air_graph_ref_integrity`         | 143           | AIR graph dangling endpoint check |
| `air_graph_reachability`          | 166           | AIR graph root reachability/worklist check |
| `backend_output_comparator`       | 135           | paired text diff verdict |
| `module_manifest_resolver`        | 121           | language_module_manifest.json |
| `stdlib_dispatch_inventory_checker` | 107         | C/LLVM dispatch table count parity |
| `doc_link_checker`                | 143           | docs/INDEX.md dead-link audit |
| `production_header_size_checker`  | 108           | DirWalk-owned `.h` 600-LOC cap |
| `production_c_size_checker`       | 127           | DirWalk-owned `.c` 699-LOC cap |
| `examples_inventory_checker`      | 112           | DirWalk-owned examples/ count + non-empty |
| `ast_read_surface_checker`        | 219           | CFG/MIR SoT ratchet parity |
| `linter`                          | 182           | LSP-style diagnostic JSON parity |
| `runtime_boundary_checker`        | 82            | native-kernel vs portable-policy runtime boundary |
| **Total peripheral**              | **2794**      | |

Plus `src/self_hosted/lib/text_scan.pgy` (~47 LOC) shared across scan-based
tools.

## Substitution Roadmap (Honest Order)

The realistic incremental path toward genuine self-host:

1. **Lexer expansion** -- *substantially done* (2026-06-16). Handles
   common keywords, line + block comments, integer + float literals, string
   literals, and common operators. The committed executable gate is the
   7-source C/LLVM parity harness; the broader scale number below is a
   historical measurement and should not be treated as a committed scale gate.
2. **Lexer at scale** -- *historical measurement refreshed* (2026-06-23).
   Pergyra lexer was measured against `examples/*.pgy` plus
   `tests/cases/backend_compare/**/main.pgy`; **993 of 993 byte-equal** vs
   `pgy --tokens`. String interpolation, escaped strings, and doc/block comment
   lexing are now in the measured surface. Coverage target met for this corpus.
3. **Parser bootstrap** -- *expanding* (2026-06-22). `src/self_hosted/parser/`
   parses 186 committed fixtures byte-equal `pgy --ast` on parser binaries
   generated by both C and LLVM, and **120 of 121** `examples/*.pgy` files at
   scale with zero byte drift and zero self-host parser exits. It now covers the domain declaration surface (`subject`, `object`,
   `tobject`, `vessel`, `ability`, `role`/`impl`, `zone`, `world`, `party`,
   `event`, `intent ... with retry(n)` metadata), imports, common statement
   forms, full expression precedence, lambda primaries, postfix calls/indexing/
   member access, and deep nested generic type names. Remaining parser work is
   replacing this text-mirror substitute with structured AST ownership and
   keeping the single C-oracle skip honest, not clearing completed-output drift.
4. **Semantic subset** -- *rung-2 active* (2026-06-23). The current rung
   checks `func`, typed `let`, literal/identifier types, return typing, scoped
   `if` / `while` / `for` bodies, unary
   and binary expression operators, call return/arity/argument typing, branch
   conditions, assignment, bare call statements, and simple/compound undefined
   identifier use in Pergyra, then compares against the C compiler accept/reject
   oracle. Recursive import expansion is now owned by `source_bundle_owner.pgy`,
   and the import-backed call fixture proves signatures are consumed from the
  source bundle instead of from a hidden single-file `main` assumption. The
  real-source selfcheck now feeds 87 accepted self-host owner/source files
   through that source-bundle owner rather than a generated import-stripped
   unit. The accepted manifest spans lexer/parser/mir-lower/codegen/compiler-world
  entrypoints, the compiler path manifest owner, target-capability envelope
  owner, hard-rung AIR/artifact/test-harness/subprocess/ABI-row/symbol-row
  compiler-world envelopes, codegen symbol-mangle, ABI-layout, collection-runtime,
  math-runtime, host-I/O-runtime, Option/Result-runtime, and string-runtime owners, semantic run/program/body/call/expression owner files, and audit-tool
   slices inside the current
   subset. The oracle parity runs on C and LLVM
   binaries across 107 fixtures. The same gate now validates the 17-code
   self-hosted semantic diagnostic vocabulary plus its C oracle JSON root-code
   mapping: committed expected `Code:` fields and literal
   `SemanticError...("code")` call sites must be registered in
   `diagnostic_code_owner.pgy`, and invalid fixtures must be rejected by the C
   oracle with that mapped JSON code. The implementation is split
   into source-of-truth owners (`text_scan_owner`, `source_bundle_owner`,
   `diagnostic_owner`, `env_owner`, `expr_type_owner`,
   `expr_validation_owner`, `call_check_owner`, `body_check_owner`,
   `program_check_owner`, `diagnostic_code_owner`, and `semantic_run_owner`) with a thin `main.pgy`
   entrypoint. Expression diagnostics consume `ExprType(...)` facts instead of
   living inside the type-query owner. The builtin/type table now includes the
   scalar math signatures `Sqrt`, `Pow`, `Floor`, `Ceil`, and `Random`,
   C-oracle string-plus and Bool arithmetic result typing, trig/log
   Float signatures from `Sin` through `Log2`, string split/join alias
   signatures, and the first-argument scalar utility contracts for `Abs`,
   `Min`, `Max`, and `Clamp`, newline-free `Print` output calls,
   `Some(expr) -> Option<ExprType(expr)>`, `None -> Option<Unknown>`,
   `None() -> Option<Unknown>`, `UnwrapOption(Option<T>) -> T`,
   `IsSome`/`UnwrapOption` builtin argument rejection for non-Option operands
   and non-concrete `Option<Unknown>` operands, comment-skipping brace/statement scanning,
   and the codegen entrypoint source.
   The next semantic expansion should broaden declarations
   only after that shared-code boundary or another equally narrow fact owner is
   available.
5. **AIR graph consumer passes** -- *rung-1 active* (2026-06-16). Five
   Pergyra-origin graph consumers now run in the self-host preparation suite:
   node-id uniqueness, live-dump node-count integrity, live-dump
   back-reference range checking, fixture-shaped edge referential integrity,
   and root reachability via a push-only worklist. These are still peripheral
   because they do not replace `src/self_hosted/air/`, but they prove the
   deterministic graph substrate the first middle-end pass needs.
6. **C-emit codegen subset** -- *rung-0..20 active* (2026-06-24). A Pergyra
   program (`src/self_hosted/codegen/main.pgy`) takes `pgy --ast` text and emits
   standalone C for: string `Log`/`Concat`, `Log(ToString(<intexpr>))`, integer
   `Let:`/`Assign:`, `while`/`if`/`else` and `for i in a..b` + `break`/`continue`
   (structural lowering), multiple `Int`/`Bool`/`String`/`Void` functions with
   calls, recursion, `return`, `String` types (routed by a variable + function
   type environment; `Concat`/`Substring`/`StringLength`/`StringIndexOf`/
   `StringTrim`/`StringJoin`/`Join` -> runtime helpers, `ToFloat` -> `atof`),
   `Bool` (`<stdbool.h>`), growable
   `Array<Int>`/`Array<String>` as a `{data,len,cap}` struct
   (`ArrayPush`/`ArrayLength`/`ArraySet`/`xs[i]` via env-aware index rewriting),
   `Array<Int>` `ArraySort`/`ArrayReverse`/`ArrayMap`/`ArrayFilter`, `Result<Int>`
   `Ok`/`Err`/`IsOk`/`IsErr`/`Unwrap`/`UnwrapOr`, `Option<Int>`
   `Some`/`None`/`IsSome`/`UnwrapOption`, block-local `defer`,
   `Exit(n)`,
   `FileExists`/`ReadFile` file I/O, `Args()` snapshots, and
   value-passed `Int` / `Bool` / `Float` / `String` field structs plus nested struct-valued fields with
   literals/member reads/params/returns,
   and `Array<Int>` parameter/return flow.
   `lib/json.pgy` now owns the first document-level schema and numeric-field
   readers consumed by the AIR graph JSON validator, in addition to the shared
   JSON string/field/object/array emission helpers consumed by production size
   checkers, the stable-subset section checker, and the module manifest
   resolver. The module manifest resolver now consumes bounded module-array
   object/field counts from the JSON owner instead of global substring counts.
   Round-trip C-emit-by-Pergyra -> gcc -> run -> stdout matches the C/LLVM oracle
   on 59 committed fixtures, with the emitter built through both backends. Next
   rungs: string freeing / block scoping, broader nested AST-node shapes, then round-trip
   self-compilation.
7. **Bootstrap loop** -- the Pergyra-written compiler subset compiles
   itself, output runs.

Steps 1-4 are active staged substitution. Step 6 (codegen) opened 2026-06-17
after the hard-migration freeze was lifted; step 5 (AIR consumers) and step 7
(bootstrap) remain ahead.

## Surface Lifts Required Before Substitution Can Continue

These Pergyra surface gaps will block compiler-internal substitution
beyond the lexer:

- **Process arguments** -- `Args() -> Array<String>` has landed for generated
  binaries, returning the user arguments as an owned snapshot. The lexer and
  parser parity runners now pass source paths through argv, so the first
  compiler-internal substitutes consume the same tool surface they need for
  standalone dogfood runs.
- **Struct-over-arbitrary-types** -- needed to model AST nodes. Pergyra
  already exercises mixed tree shapes as parser/backend evidence:
  `node_traversal_sum`, `tree_walk_recursive`, `tree_grow_recursive`,
  `nested_generic_containers`, and the parser's deep
  `HashMap<String, List<HashMap<Int, Array<String>>>>` fixture prove user
  classes/records and nested generics across C/LLVM-facing gates. These
  mixed tree shapes are parser/backend evidence, not compiler-model
  substitution. It is still not yet a self-hosted compiler AST model: current
  self-hosted parser and codegen rungs consume text AST artifacts instead of
  owning a mixed tagged-node tree in Pergyra. The next closure is a compiler
  pass that owns explicit Pergyra node records/classes and proves traversal
  against the C oracle.
- **Raw pointer / FFI** -- if a Pergyra component needs to call into
  the C compiler's runtime (e.g. share the diagnostic emitter), there
  is no stable FFI today. This is intentional for the current compiler-pass
  path: `unsafe` is only a scoped marker, raw pointer helpers stay
  runtime-internal, and `raw_escape_contract_smoke` rejects system-tier escape.
  The alternative remains *no FFI*: build the Pergyra-side compiler as a
  parallel binary that emits text, not as a library that plugs into the C
  compiler. FFI remains intentionally absent from the compiler-pass path until
  a stable ABI contract exists.
- **Subprocess execution** -- needed for in-Pergyra drift guards that
  re-run the C compiler. Currently the parity scripts do this from
  bash; a Pergyra runner would need `Subprocess(...)`.
- **Deterministic collection iteration** -- compiler passes need stable
  output ordering, not just functional map/set lookup. `stage4_determinism_smoke`
  now compares insertion-order variants for `HashMap<String|Int|Long|Bool, T>`
  `MapKeys` and `Set<String|Int|Long|Bool>` `SetValues` through generated
  Pergyra programs on C and LLVM. Compiler-facing symbol/record-like identities
  are canonical string keys, and handle-like identities are stable integer or
  long IDs; the Stage 4 fixture exercises those canonical shapes instead of
  introducing raw aggregate keys as a second collection truth. Compiler passes
  should consume those stable snapshots (`MapKeys` / `SetValues`) rather than
  depending on hash storage traversal.
- **Allocator/arena ownership surface** -- `AllocatorSystem`,
  `AllocatorPool`, `AllocatorDebug`, `AllocatorTracing`, `AllocatorScratch`,
  `AllocatorResult`, and `AllocatorPersistent` now produce the single stable
  `Allocator` value on C and LLVM. The lane-named constructors carry distinct
  runtime kinds and lower through dedicated LLVM runtime init exports instead
  of aliases. `BoxArray(capacity, allocator)` consumes a named allocator local
  so fused array storage keeps an owner with a valid lifetime.
  `AllocatorDestroy(namedAllocator)` is the stable cleanup operation, so
  compiler pass lanes can use `defer { AllocatorDestroy(lane); }` instead of an
  out-of-language cleanup convention.
- **Filesystem directory walk** -- `DirWalk(String) -> Array<String>` has landed
  for generated binaries on C and LLVM. It returns a deterministic sorted
  regular-file snapshot with `/` separators and is gated by
  `filesystem_directory_walk_smoke`. `examples_inventory_checker` now consumes
  `DirWalk("examples")` directly, so the clean example inventory no longer has a
  committed manifest alias. `production_header_size_checker` and
  `production_c_size_checker` now consume `DirWalk("src")` directly, so their
  clean inventories no longer depend on committed file-list fixtures.
  `ast_read_surface_checker` now keeps only the metric/ceiling ratchet spec in
  `tests/ast_read_surface_ratchet.txt` and owns live file discovery through
  `DirWalk(scope)`. Remaining manifest-owned surfaces are document contracts,
  not clean directory file lists.
- **Parser LLVM depth/type-inference parity** -- `parser_parity.sh` now
  compiles the self-host parser through both C and LLVM and includes a deep
  nested generic type fixture. Remaining parser work is grammar breadth and
  the scale-probe exit list, not C-only backend evidence.

The remaining work is mostly actual semantic and codegen pass work against the
C compiler oracle. The one substrate-shaped item that remains as compiler-core
design work is mixed AST-like tree ownership inside a Pergyra pass; current
evidence proves language shape and backend/parser behavior, not compiler-model
substitution.

## How to Update This Document

When a tool lands or expands, update three things:

1. **Headline Number** -- recalculate Pergyra LOC vs C LOC for
   *compiler-internal substitutes only* (not peripheral tools).
2. **Component Coverage table** -- bump the relevant row's `Pergyra LOC`
   and `Coverage %` (be honest -- LOC equivalence is a bad proxy; use
   functional coverage estimates).
3. **Substitution Roadmap** -- check off completed steps, add detail
   where the next step diverged from the plan.

Do **not** add peripheral audit tools to the substitution percentage.
Their job is to keep the C compiler honest, not to replace it.
