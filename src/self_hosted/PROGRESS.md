# Self-Host Progress

**This is the canonical progress measurement for Pergyra self-hosting.**
The number that matters is *how much of the C/LLVM compiler has been
substituted by Pergyra-written equivalents* -- not how many peripheral
audit tools exist.

Last updated: 2026-06-22

## Headline Number

**Hard self-host contract (2026-06-22):** hard self-host is now gated as
staged substitution rather than tracked as a separate cleanup project. The
contract lives in `docs/self_hosted/10_hard_self_host_contract.md`, and
`tests/self_host_hard_contract_smoke.sh` keeps the docs, Makefile wiring, active
hard rungs, C oracle, LLVM oracle, bridge/fallback split, codegen bootstrap, and
MIR JSON fact-only lowering aligned. The substitution percentage below is
unchanged by that contract gate; future percentage increases require a Pergyra
implementation to replace a real compiler stage/pass beside the C/LLVM oracle.

**Compiler-internal substitution: ~4.04% LOC-scale** (10,299 Pergyra LOC vs 254,742
C LOC across `src/lexer/`, `src/parser/`, `src/semantic/`, `src/codegen/`,
`src/runtime/`, `src/compiler/`, `src/lsp/`). The verified substitutes are the
lexer, parser, a bounded semantic verdict rung, and -- as of 2026-06-17 -- the
**first codegen rungs** (`src/self_hosted/codegen/`, 1689 LOC; rung-0 string Log,
rung-1 integer let/arithmetic, rung-2 assign + `while`/`if`/`else`, rung-3
multi-function definitions + calls + `return`, rung-4 `String` types with a
variable/function type environment + runtime `pgy_concat`, rung-5 `for` loops +
`break`/`continue`, rung-6 `Bool` type + `StringLength`/`Substring` builtins,
rung-7/8 fixed `Array<Int>`/`Array<String>` literals + indexing +
`ArrayLength`/`ArraySet`, rung-9 `StringIndexOf` builtin + `Exit`, rung-10
**growable arrays** (`ArrayPush`) via a `{data,len,cap}` struct rep with
env-aware index-expression rewriting, rung-11 `StringTrim` builtin, rung-12
`FileExists`/`ReadFile` file I/O, rung-13 `Args()` user-argument snapshots,
rung-14 value-passed Int-field structs, rung-15 `Array<Int>` param/return flow).
The rest of codegen, runtime, compiler driver, and LSP substitution are still
0%; the MIR-lowering substitution has now *started* (see below).

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
reassignment, multiple Void routines with bare-call statements, string
concat/equality, recursion, `continue`, and file read/write), gated by
`parity/mir_json_parity.sh`
(`make self-host-mir-json-parity-test-smoke`, 32 fixtures). The gate now
requires the MIR JSON fact surface and checks the `for`
header is reconstructed from `arg0` plus `expr0`/`expr1` bounds. The gate also
rejects reintroducing reads of the transitional `ast` compatibility text. This
is the first verified slice of the actual compiler-core (~96% of the LOC), not
the codegen subset. It is now fact-only for the supported MIR JSON statement and
expression surfaces; next is broadening that surface rather than preserving text
fallback.

**Hard migration opened (2026-06-17):** the codegen rung is the first *hard
compiler-core* substitute, landed after the BDFL decision lifted the
`docs/self_hosted/README.md` freeze. Hard migration proceeds rung-by-rung, each
gated against the C/LLVM oracle before the next opens -- not as an unverified
compiler fork. See `src/self_hosted/codegen/README.md`.

**Self-hosting achieved for codegen (2026-06-17):** the codegen tool *self-hosts*.
A Pergyra-built copy of the tool, run on the tool's own source (`main.pgy`,
1504 lines), emits C that gcc-compiles and **reproduces its own
source-compilation exactly** -- `gen2 == gen3` byte-identical -- and the
Pergyra-built tool emits byte-identical C to the oracle-built tool on the sample
fixtures. Breadth: the same codegen also compiles the lexer (587 lines) and parser (3338 lines); each codegen-built binary matches its oracle-built counterpart on a sample source -- three real self-host components self-built. Wider survey: the codegen compiles **all 22 of 22** committed self-host components/tools to valid C, each verified run-equivalent to the oracle-built binary on a sample -- the entire committed self-host toolchain (lexer, parser, semantic, codegen itself, + 18 audit tools) is self-built by the Pergyra-written codegen. This includes namespace-imported audit tools (`TextScan::` qualified calls, flattened to `NS_Func` -- import/namespace + DirWalk support added). The earlier 18/22 ceiling was a `pgy --ast` bug (for-each `for x in lines` rendered as `For: x in (null)..(null)`, dropping the collection); FIXED in src/parser/ast_print.c (emit the iterable) + the self-host parser, regenerated 5 parser fixtures, and added for-each lowering + bare-void-return + word-boundary builtin matching to the codegen. The final component (semantic) needed `ArrayPop` + type-aware bare `return` (String→`return ""`, non-Void→`return 0`, Main→`return 0`); added both. Parser parity (188 sources) stays byte-equal. The bootstrap gate verifies codegen self-hosts (gen2==gen3) + builds lexer + parser + semantic + 13 audit tools, all matching oracle-built. Gated by `parity/codegen_bootstrap.sh`
(`make self-host-codegen-bootstrap-test-smoke`).

Reaching the fixpoint drove out and fixed real gaps: `else if` chains,
string-literal-safe builtin rewriting, recursive `Concat`/`ToString`/call-argument
lowering (`Concat`→`pgy_concat` is a pure name rewrite -- same args -- so it
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

**Lexer parity (2026-06-18):** the committed lexer gate compiles the
Pergyra-origin lexer through both C and LLVM, then proves byte-equal token
output and live `pgy --tokens` drift on 6 source fixtures:
`hello`, `array_literal`, `break_continue`, `basic`, `heap`, and
`binary_search`. The broader lexer scale result remains the 2026-06-16
measurement below; there is not yet a committed lexer-scale probe script.

**Parser at scale (2026-06-22):** the Pergyra-origin parser produces
byte-equal output vs `pgy --ast` on **120 of 121** committed
`examples/*.pgy` files. There are now **zero byte-drift cases** in the
scale probe: every example that both the live C oracle and the self-host
parser complete is byte-equal. There are also **zero self-host parser exits**;
the one remaining non-match fails under `pgy --ast` itself and is a C-skip
(`secure_slots`). The scale probe is a
coverage probe, not a hard parity gate, but it now fails closed: it removes any
stale generated parser binary before compile and exits if compile does not
produce a runnable parser. Previous historical match counts:
105 -> 86 -> 83 -> 80 -> 79 -> 77 -> 72 -> 72 -> 63 -> 59 -> 58 -> 57 -> 53 -> 48 -> 46 -> 43 -> 37 -> 25 -> 11.
Refresh:
`bash src/self_hosted/parity/parser_scale_probe.sh --failing`.

**Rung-1 parity (2026-06-16):** the committed
`parser_parity.sh` `SOURCE_PAIRS` array now exercises **188
sources** vs `pgy --ast` on both generated C and LLVM parser binaries
(was 83 on 2026-05-29; +105 overall). The added fixture surface covers Option/Result
destructure, slot sugar, transfer short syntax, array literal,
common collection algorithms (queue, stack, deque, heap,
linked_list, hash_map, union_find, graph_bfs), string + stdlib +
io + math builtin surfaces, async/spawn/select/defer/for control
flow, walrus surface, pipe + try operator, ownership /
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
Pergyra-written lexer that handles ~97% of the example token surface.
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
| `src/lexer/`    |    1003 |         584 | **~97%** | **191 of 195 sources byte-equal** (115 examples + 80 backend_compare). Remaining 4 use string interpolation (`$"...{var}..."`) or `/** doc */` comments. 6 representative sources committed as parity fixtures. |
| `src/parser/`   |   21813 |        6856 | ~52%     | `src/self_hosted/parser/` parses 188 committed fixtures byte-equal `pgy --ast` on both C and LLVM parser binaries, and **120 of 121** `examples/*.pgy` byte-equal at scale (2026-06-22; zero byte-drift, zero self-host parser exits, 1 C-skip). Top-level: `[async]? [export]? func<T,U>`, `subject`/`class`/`vessel`/`struct`/`object`/`tobject` with `<T,U>` and `func`/`action` methods, `type` alias/record alias, `enum`, `namespace`, `event`, `ability`, `role`/`impl`, `party`, `roster`, `world`, `zone` (subject/object/tobject slots), `intent ... with retry(n)` metadata, `import "PATH.pgy";` (reads file relative to source dir, recursively parses, force-exports its funcs). Stmt: `let IDENT/(IDENTS)`, assign, `+=`/`-=`/`<-`, `return`, `if`/`else if`/`else`, `if let Some(...)`, `while`, `for`, `loop`, `break`, `continue`, `defer`, `match`, `parallel`, `parallel on`/`every`, `continuous`, `transaction`, `fail`, `with slot<TYPE> as VAR { stmts }`. `expr`: `! - <- spawn[blocking] await` > `*/% > +- > \|> > cmp > && > \|\|`. Primaries: STRING/NUMBER/IDENT/`( )`/`[ ]`/lambda/object-init/tuple-erasure, `async {}`/parallel expression blocks, postfix `(args)` / `[idx]` / `.member` / `?` / turbofish, and dollar interpolation. |
| `src/semantic/` |   47541 |        1202 | rung-2 subset | Checks a bounded function-body subset against the C compiler oracle on C/LLVM-generated binaries: typed `let`, return typing, unary/binary expression typing, function-call return/arity/argument typing, scoped `if`/`while`/`for` bodies, branch conditions, assignment, bare call statements, and simple/compound undefined identifier use across 65 fixtures. |
| `src/codegen/`  |  111465 |        1689 | rung-0..15 | **C-emit rung-0..15 (2026-06-17).** Pergyra emitter consumes `pgy --ast` text and emits standalone C for: string `Log`/`Concat`, `Log(ToString(<intexpr>))`, integer `Let:`/`Assign:` (`+ - * / %`, negatives match oracle), `while`/`if`/`else` and `for i in a..b` + `break`/`continue` (structural lowering), multiple `Int`/`Bool`/`String`/`Void` functions with calls, recursion, `return`, `String` types (routed by a per-function variable + global function type environment; `Concat`/`Substring`/`StringLength`/`StringIndexOf`/`StringTrim` → runtime helpers), `Bool` (`<stdbool.h>`), **growable `Array<Int>`/`Array<String>`** as a `{data,len,cap}` struct (`[..]` literal → `new()`+`push`; `ArrayPush`/`ArrayLength`/`ArraySet`/`xs[i]` → struct helpers via env-aware index-expression rewriting), the `Exit(n)` statement, **`FileExists`/`ReadFile` file I/O**, `Args()` user-argument snapshots, value-passed Int-field structs with literals/member reads/params/returns, `Array<Int>` parameter/return flow, `ArrayPop`, type-aware bare returns, and the `ToUpper`/`ToLower`/`StringReplace`/`Abs`/`Min`/`Max` builtins (pure name-rewrites to runtime helpers), and the **string `+` concatenation operator** (in a string context a top-level `+`, after stripping the AST's redundant parens, lowers to nested `pgy_concat`), **`WriteFile(path, content)`** (Void, one-shot overwrite via `pgy_writefile`, the pair of the existing `ReadFile`), and a **`Log` newline-semantics fix** (the oracle strips all trailing newlines from a logged string then appends exactly one -- `Log("c\n\n")` -> `c\n`; string Log now routes through a `pgy_log` helper instead of a raw `printf("%s\n", ...)`, closing a real self-vs-oracle run-equivalence divergence on newline-terminated strings), and the **file-handle I/O API** (`FileOpen`->Int handle over a static `FILE*` table, `FileWrite`/`FileClose`/`FileRead` one-line reads, plus `Print` for newline-free output), and **`Float`** (C `double`: Float locals, `Sqrt`/`Pow`/`Floor`/`Ceil` math via `<math.h>`, `Log(Float)` as `%f`, Float `+`, and deterministic `Random(n)` = `rand() % n`; Float params/returns are deliberately rejected so complex Float programs stay a clean observable rejection rather than broken C). The codegen also **rejects `event` declarations** with an observable CODEGEN ERROR (no handler/dispatch model -- a no-op fake would mask real event dispatch). With that, **every one of the 118 `examples/*.pgy` is either compiled to run-equivalent C or cleanly rejected -- zero in-subset silent-broken-output cases**, satisfying the §1.1 observable-rejection invariant across the whole corpus. **48 fixtures run-stdout equal** to the C/LLVM oracle on tools built through both backends (incl. recursive Fibonacci, string index-of split, bool predicates, growable int + string array push/iterate, file read, argv snapshot, struct value flow, array param/return). Gate: `parity/codegen_parity.sh` (`make self-host-codegen-parity-test-smoke`). Out-of-subset input is an observable `Exit(1)`. |
| `src/runtime/`  |   31985 |           0 | 0%       | native runtime kernel stays C; portable runtime policy libraries may move later |
| `src/compiler/` |   39863 |           0 | 0%       | not started       |
| `src/lsp/`      |    1072 |           0 | 0%       | not started       |
| **Total**       | **254742** |  **10299**  | **~4.04% LOC-scale** | lexer/parser/semantic + codegen rung-0..15; no HIR/MIR/runtime/compiler/LSP substitution yet |

Notes:

- *Coverage %* is a rough functional estimate, not a LOC-equivalence
  number. The lexer is 584 LOC and is judged by byte-equal fixture coverage,
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
| `diagnostic_catalog_checker`      | 266           | docs/72 vs diag_codes.h drift |
| `stable_subset_section_checker`   | 122           | docs/107 canonical anchors |
| `air_graph_json_validator`        | 165           | `pgy --air-json` shape gate |
| `air_graph_id_uniqueness`         | 132           | AIR graph duplicate node-id check |
| `air_graph_node_count_integrity`  | 140           | live AIR graph id-count summary check |
| `air_graph_ref_live`              | 138           | live AIR graph back-reference range check |
| `air_graph_ref_integrity`         | 143           | AIR graph dangling endpoint check |
| `air_graph_reachability`          | 166           | AIR graph root reachability/worklist check |
| `backend_output_comparator`       | 135           | paired text diff verdict |
| `module_manifest_resolver`        | 121           | language_module_manifest.json |
| `stdlib_dispatch_inventory_checker` | 105         | C/LLVM dispatch table count parity |
| `doc_link_checker`                | 143           | docs/INDEX.md dead-link audit |
| `production_header_size_checker`  | 108           | DirWalk-owned `.h` 600-LOC cap |
| `production_c_size_checker`       | 127           | DirWalk-owned `.c` 699-LOC cap |
| `examples_inventory_checker`      | 112           | DirWalk-owned examples/ count + non-empty |
| `ast_read_surface_checker`        | 219           | CFG/MIR SoT ratchet parity |
| `linter`                          | 173           | LSP-style diagnostic JSON parity |
| `runtime_boundary_checker`        | 82            | native-kernel vs portable-policy runtime boundary |
| **Total peripheral**              | **2597**      | |

Plus `src/self_hosted/lib/text_scan.pgy` (~47 LOC) shared across scan-based
tools.

## Substitution Roadmap (Honest Order)

The realistic incremental path toward genuine self-host:

1. **Lexer expansion** -- *substantially done* (2026-06-16). Handles
   common keywords, line + block comments, integer + float literals, string
   literals, and common operators. The committed executable gate is the
   6-source C/LLVM parity harness; the broader scale number below is a
   historical measurement and should not be treated as a committed scale gate.
2. **Lexer at scale** -- *historical measurement* (2026-06-16). Pergyra
   lexer was measured against 115 `examples/*.pgy` + 80
   `tests/cases/backend_compare/**/main.pgy` files; **191 of 195
   byte-equal** vs `pgy --tokens` (97.9%). Remaining 4 need string
   interpolation or `/** doc */` lexing -- both larger surface than the
   current scope warrants. Coverage target met.
3. **Parser bootstrap** -- *expanding* (2026-06-22). `src/self_hosted/parser/`
   parses 188 committed fixtures byte-equal `pgy --ast` on parser binaries
   generated by both C and LLVM, and **120 of 121** `examples/*.pgy` files at
   scale with zero byte drift and zero self-host parser exits. It now covers the domain declaration surface (`subject`, `object`,
   `tobject`, `vessel`, `ability`, `role`/`impl`, `zone`, `world`, `party`,
   `event`, `intent ... with retry(n)` metadata), imports, common statement
   forms, full expression precedence, lambda primaries, postfix calls/indexing/
   member access, and deep nested generic type names. Remaining parser work is
   replacing this text-mirror substitute with structured AST ownership and
   keeping the single C-oracle skip honest, not clearing completed-output drift.
4. **Semantic subset** -- *rung-2 active* (2026-06-16). The current rung
   checks `func`, typed `let`, literal/identifier types, return typing, scoped
   `if` / `while` / `for` bodies, unary
   and binary expression operators, call return/arity/argument typing, branch
   conditions, assignment, bare call statements, and simple/compound undefined
   identifier use in Pergyra, then compares against the C compiler accept/reject
   oracle on C and LLVM binaries across 65 fixtures. Next expansion should add
   a broader builtin/type symbol table and diagnostic-code parity before
   broadening into declarations.
5. **AIR graph consumer passes** -- *rung-1 active* (2026-06-16). Five
   Pergyra-origin graph consumers now run in the self-host preparation suite:
   node-id uniqueness, live-dump node-count integrity, live-dump
   back-reference range checking, fixture-shaped edge referential integrity,
   and root reachability via a push-only worklist. These are still peripheral
   because they do not replace `src/self_hosted/air/`, but they prove the
   deterministic graph substrate the first middle-end pass needs.
6. **C-emit codegen subset** -- *rung-0..15 active* (2026-06-17). A Pergyra
   program (`src/self_hosted/codegen/main.pgy`) takes `pgy --ast` text and emits
   standalone C for: string `Log`/`Concat`, `Log(ToString(<intexpr>))`, integer
   `Let:`/`Assign:`, `while`/`if`/`else` and `for i in a..b` + `break`/`continue`
   (structural lowering), multiple `Int`/`Bool`/`String`/`Void` functions with
   calls, recursion, `return`, `String` types (routed by a variable + function
   type environment; `Concat`/`Substring`/`StringLength`/`StringIndexOf`/
   `StringTrim` -> runtime helpers), `Bool` (`<stdbool.h>`), growable
   `Array<Int>`/`Array<String>` as a `{data,len,cap}` struct
   (`ArrayPush`/`ArrayLength`/`ArraySet`/`xs[i]` via env-aware index rewriting),
   `Exit(n)`, `FileExists`/`ReadFile` file I/O, `Args()` snapshots, and
   value-passed Int-field structs with literals/member reads/params/returns,
   and `Array<Int>` parameter/return flow.
   Round-trip C-emit-by-Pergyra -> gcc -> run -> stdout matches the C/LLVM oracle
   on 35 committed fixtures, with the emitter built through both backends. Next
   rungs: string freeing / block scoping, richer struct fields / nested
   AST-node shapes, then round-trip
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
