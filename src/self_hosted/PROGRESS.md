# Self-Host Progress

**This is the canonical progress measurement for Pergyra self-hosting.**
The number that matters is *how much of the C/LLVM compiler has been
substituted by Pergyra-written equivalents* -- not how many peripheral
audit tools exist.

Last updated: 2026-06-17

## Headline Number

**Compiler-internal substitution: ~3.55% LOC-scale** (9,053 Pergyra LOC vs 254,742
C LOC across `src/lexer/`, `src/parser/`, `src/semantic/`, `src/codegen/`,
`src/runtime/`, `src/compiler/`, `src/lsp/`). The verified substitutes are the
lexer, parser, a bounded semantic verdict rung, and -- as of 2026-06-17 -- the
**first codegen rungs** (`src/self_hosted/codegen/`, 411 LOC; rung-0 string Log,
rung-1 integer let/arithmetic, rung-2 assign + `while`/`if`/`else` control flow).
HIR/MIR, the rest of codegen, runtime, compiler driver, and LSP substitution are
still 0%.

**Hard migration opened (2026-06-17):** the codegen rung is the first *hard
compiler-core* substitute, landed after the BDFL decision lifted the
`docs/self_hosted/README.md` freeze. Hard migration proceeds rung-by-rung, each
gated against the C/LLVM oracle before the next opens -- not as an unverified
compiler fork. See `src/self_hosted/codegen/README.md`.

**Parser at scale (2026-05-31):** the Pergyra-origin parser produces
byte-equal output vs `pgy --ast` on **105 of 117** committed
`examples/*.pgy` files (89.7%). Four files byte-drift on deferred
semantic rewrites (intra-namespace call mangling in `composite_intent_*`
+ `function_clause_order_minimal` + `surface_compression_maximal`),
one file crashes the self-host parser
(`six_item_alignment_demo`). Previous: 86 -> 83 -> 80 -> 79 -> 77 -> 72 -> 72 -> 63 -> 59 -> 58 -> 57 -> 53 -> 48 -> 46 -> 43 -> 37 -> 25 -> 11.
Refresh:
`bash src/self_hosted/parity/parser_scale_probe.sh`. 7 of the 117
examples fail under `pgy --ast` itself (C-skip).

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
  `role_ability_demo`, `world_roster_city`, `party_system_demo`,
  `parallel`, `secure_slots`, `structured_comments`,
  `vessel_action_design`.
- Self-host parser byte-drifts vs live `pgy --ast`:
  `composite_intent_orchestration_compressed`,
  `composite_intent_orchestration_explicit`,
  `function_clause_order_minimal`,
  `surface_compression_maximal`.
- Self-host parser crashes:
  `six_item_alignment_demo`.

Reading this honestly: the self-host journey has *just started*. The
first compiler-internal substitute (`src/self_hosted/lexer/`) lands a
Pergyra-written lexer that handles ~97% of the example token surface.
The parser (`src/self_hosted/parser/`) follows at ~52%: it covers a real
domain grammar subset and has C/LLVM byte-equal parser parity over the
committed fixture set, but still stops short of the remaining scale-probe drift
list and the full parser recovery surface.

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
| `src/parser/`   |   21813 |        6856 | ~52%     | `src/self_hosted/parser/` parses 188 committed fixtures byte-equal `pgy --ast` on both C and LLVM parser binaries, and **105 of 117** `examples/*.pgy` byte-equal at scale (89.7%; 2026-05-31). Top-level: `[async]? [export]? func<T,U>`, `subject`/`class`/`vessel`/`struct`/`object`/`tobject` with `<T,U>` and `func`/`action` methods, `enum`, `namespace`, `event`, `ability`, `role`/`impl`, `zone` (subject/object/tobject slots), `intent ... with retry(n)` metadata, `import "PATH.pgy";` (reads file relative to source dir, recursively parses, force-exports its funcs). Stmt: `let IDENT/(IDENTS)`, assign, `+=`/`-=`/`<-`, `return`, `if`/`else if`/`else`, `while`, `for`, `break`, `continue`, `defer`, `match`, `parallel`, `with slot<TYPE> as VAR { stmts }`. `expr`: `! - <- spawn[blocking] await` > `*/% > +- > \|> > cmp > && > \|\|`. Primaries: STRING/NUMBER/IDENT/`( )`/`[ ]`/lambda, postfix `(args)` / `[idx]` / `.member` / `?` / turbofish. |
| `src/semantic/` |   47541 |        1202 | rung-2 subset | Checks a bounded function-body subset against the C compiler oracle on C/LLVM-generated binaries: typed `let`, return typing, unary/binary expression typing, function-call return/arity/argument typing, scoped branch bodies, branch conditions, assignment, bare call statements, and simple/compound undefined identifier use across 30 fixtures. |
| `src/codegen/`  |  111465 |         411 | rung-0/1/2 | **C-emit rung-0/1/2 (2026-06-17).** Pergyra emitter consumes `pgy --ast` text for `func Main()` with `Log(<strexpr>)` (string literal \| `Concat(...)`), `Log(ToString(<intexpr>))`, integer `Let:`/`Assign:` (`+ - * / %`, negatives match oracle), and `while`/`if`/`else` control flow lowered structurally from the AST's nested Block/Then/Else. Emits standalone C; **11 fixtures run-stdout equal** to the C/LLVM oracle on tools built through both backends. Gate: `parity/codegen_parity.sh` (`make self-host-codegen-parity-test-smoke`). Out-of-subset input is an observable `Exit(1)`. |
| `src/runtime/`  |   31985 |           0 | 0%       | native runtime kernel stays C; portable runtime policy libraries may move later |
| `src/compiler/` |   39863 |           0 | 0%       | not started       |
| `src/lsp/`      |    1072 |           0 | 0%       | not started       |
| **Total**       | **254742** |  **9053**  | **~3.55% LOC-scale** | lexer/parser/semantic + codegen rung-0/1/2; no HIR/MIR/runtime/compiler/LSP substitution yet |

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
   literals, and common operators. The lexer is now judged by the scale parity
   gate below rather than the older small fixture count.
2. **Lexer at scale** -- *substantially done* (2026-06-16). Pergyra
   lexer runs against 115 `examples/*.pgy` + 80
   `tests/cases/backend_compare/**/main.pgy` files; **191 of 195
   byte-equal** vs `pgy --tokens` (97.9%). Remaining 4 need string
   interpolation or `/** doc */` lexing -- both larger surface than the
   current scope warrants. Coverage target met.
3. **Parser bootstrap** -- *expanding* (2026-06-16). `src/self_hosted/parser/`
   parses 188 committed fixtures byte-equal `pgy --ast` on parser binaries
   generated by both C and LLVM, and **105 of 117** `examples/*.pgy` files at
   scale. It now covers the domain declaration surface (`subject`, `object`,
   `tobject`, `vessel`, `ability`, `role`/`impl`, `zone`, `world`, `party`,
   `event`, `intent ... with retry(n)` metadata), imports, common statement
   forms, full expression precedence, lambda primaries, postfix calls/indexing/
   member access, and deep nested generic type names. Remaining parser work is
   grammar breadth plus the scale-probe drift list, not C-only backend evidence.
4. **Semantic subset** -- *rung-2 active* (2026-06-16). The current rung
   checks `func`, typed `let`, literal/identifier types, return typing, scoped
   `if` / `while` bodies, unary
   and binary expression operators, call return/arity/argument typing, branch
   conditions, assignment, bare call statements, and simple/compound undefined
   identifier use in Pergyra, then compares against the C compiler accept/reject
   oracle on C and LLVM binaries across 30 fixtures. Next expansion should add
   a broader builtin/type symbol table and diagnostic-code parity before
   broadening into declarations.
5. **AIR graph consumer passes** -- *rung-1 active* (2026-06-16). Five
   Pergyra-origin graph consumers now run in the self-host preparation suite:
   node-id uniqueness, live-dump node-count integrity, live-dump
   back-reference range checking, fixture-shaped edge referential integrity,
   and root reachability via a push-only worklist. These are still peripheral
   because they do not replace `src/self_hosted/air/`, but they prove the
   deterministic graph substrate the first middle-end pass needs.
6. **C-emit codegen subset** -- *rung-0/1/2 active* (2026-06-17). A Pergyra program
   (`src/self_hosted/codegen/main.pgy`) takes `pgy --ast` text for the `func Main()`
   subset -- `Log(<strexpr>)`, `Log(ToString(<intexpr>))`, integer `Let:`/`Assign:`,
   and `while`/`if`/`else` control flow lowered structurally from the AST's nested
   Block/Then/Else nodes -- and emits standalone C; round-trip
   C-emit-by-Pergyra -> gcc -> run -> stdout matches the C/LLVM oracle on 11
   committed fixtures, with the emitter built through both backends. Next rungs:
   multiple user functions/calls, then `String`-typed lets/assigns, then `for`
   loops + `break`/`continue`.
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
  has `subject`, `object`, `class` keywords, but the surface for nested
  trees with mixed-type children is not exercised yet by self-host code.
- **Raw pointer / FFI** -- if a Pergyra component needs to call into
  the C compiler's runtime (e.g. share the diagnostic emitter), there
  is no FFI today. The alternative is *no FFI*: build the Pergyra-side
  compiler as a parallel binary that emits text, not as a library that
  plugs into the C compiler.
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
  introducing raw aggregate keys as a second collection truth.
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
  the scale-probe drift list, not C-only backend evidence.

The remaining work is no longer substrate availability; it is actual semantic
and codegen pass work against the C
compiler oracle.

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
