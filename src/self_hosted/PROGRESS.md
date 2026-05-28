# Self-Host Progress

**This is the canonical progress measurement for Pergyra self-hosting.**
The number that matters is *how much of the C/LLVM compiler has been
substituted by Pergyra-written equivalents* -- not how many peripheral
audit tools exist.

Last updated: 2026-05-28

## Headline Number

**Compiler-internal substitution: ~0.26%** (559 Pergyra LOC vs 211,294 C
LOC across `src/lexer/`, `src/parser/`, `src/semantic/`, `src/codegen/`,
`src/runtime/`, `src/compiler/`, `src/lsp/`).

Reading this honestly: the self-host journey has *just started*. The
first compiler-internal substitute (`lex_minimal`) lands a Pergyra-written
lexer that handles only the tokens present in two committed fixtures. It
proves Pergyra *can* substitute a compiler component (byte-equal output
vs `pgy --tokens`), but the actual coverage is tiny.

Everything else listed under `src/self_hosted/tools/` is *peripheral
audit tooling*. These tools do not substitute any compiler component;
they only observe text artifacts the C compiler produces. Their LOC is
**not** counted in the substitution percentage.

## Component Coverage

| Component       | C LOC   | Pergyra LOC | Coverage | Status            |
|-----------------|---------|-------------|----------|-------------------|
| `src/lexer/`    |     996 |         559 | **~95%** | **112 of 115 examples byte-equal** vs `pgy --tokens`. Remaining 3 use string interpolation (`$"...{var}..."`) or `/** doc */` comments (`party_system_demo`, `world_roster_city`, `structured_comments`). 6 representative sources committed as parity fixtures. |
| `src/parser/`   |   19024 |           0 | 0%       | not started       |
| `src/semantic/` |   45595 |           0 | 0%       | not started       |
| `src/codegen/`  |   81815 |           0 | 0%       | not started       |
| `src/runtime/`  |   28510 |           0 | 0%       | runtime stays C (target language hosts runtime) |
| `src/compiler/` |   34282 |           0 | 0%       | not started       |
| `src/lsp/`      |    1072 |           0 | 0%       | not started       |
| **Total**       | **211294** |   **559**  | **~0.26%** | lexer step ~done |

Notes:

- *Coverage %* is a rough functional estimate, not a LOC-equivalence
  number. `lex_minimal` is 312 LOC but only handles a bounded subset of the
  token classes the C lexer recognizes, and on only two source files.
- *Runtime stays C* by current design: the runtime is what the target
  Pergyra program links against, so substituting it in Pergyra would
  create a bootstrap cycle. Counted as 0% intentionally.
- `src/lsp/` is the Language Server Protocol implementation. Lower
  priority than the core compiler.

## Peripheral Audit Tools (Not Counted In Coverage)

These 10 tools live in `src/self_hosted/tools/` but do **not** count
toward compiler-internal substitution. They are dogfood validators
that read text artifacts and emit drift verdicts; the C compiler
keeps running fine with or without them.

| Tool                              | LOC (Pergyra) | Function |
|-----------------------------------|---------------|----------|
| `diagnostic_catalog_checker`      | 282           | docs/72 vs diag_codes.h drift |
| `stable_subset_section_checker`   | 133           | docs/107 canonical anchors |
| `air_graph_json_validator`        | 180           | `pgy --air-json` shape gate |
| `backend_output_comparator`       | 149           | paired text diff verdict |
| `module_manifest_resolver`        | 134           | language_module_manifest.json |
| `stdlib_dispatch_inventory_checker` | 103         | C/LLVM dispatch table count parity |
| `doc_link_checker`                | 155           | docs/INDEX.md dead-link audit |
| `production_header_size_checker`  | 133           | `.h` 600-LOC cap |
| `production_c_size_checker`       | 130           | `.c` 600-LOC cap |
| `examples_inventory_checker`      | 119           | examples/ presence + non-empty |
| **Total peripheral**              | **1518**      | |

Plus `src/self_hosted/lib/text_scan.pgy` (~47 LOC) shared across tools 5,
6, 8, 9, 10.

## Substitution Roadmap (Honest Order)

The realistic incremental path toward genuine self-host:

1. **Lexer expansion** -- ✅ *substantially done* (2026-05-28). Handles
   ~30 keywords, line + block comments, integer + float literals,
   string literals, all common single-char and 2-char operators
   (`->`, `==`, `!=`, `<=`, `>=`, `&&`, `||`, `..`, `<-`, `=>`, `|>`,
   `::`, `:=`, `+=`, `-=`, `*=`, `/=`). 112/115 example files
   byte-equal vs `pgy --tokens`. Remaining 3 need string-interpolation
   (`$"...{var}..."`) and `/** doc */` comment lexing -- both
   significantly bigger surface than what's currently in scope.
2. **Lexer at scale** -- run the Pergyra lexer against every file under
   `tests/cases/backend_compare/` and assert byte-equal vs `pgy --tokens`.
   Target coverage: 90%+ of `src/lexer/` (examples already at 97%).
3. **Parser bootstrap** -- Pergyra-side recursive-descent parser for the
   subset the Pergyra lexer covers. Output: a JSON AST that the C parser
   can also emit for parity.
4. **Semantic subset** -- check `func`, `let`, basic types in Pergyra.
   Compare against the C semantic verdict.
5. **C-emit codegen subset** -- a Pergyra program that takes a tiny AST
   and emits valid C output. Round-trip: C-emit by Pergyra -> C-compile
   -> run -> stdout matches expected.
6. **Bootstrap loop** -- the Pergyra-written compiler subset compiles
   itself, output runs.

Steps 1-2 are realistic this quarter. 3+ are post-beta.

## Surface Lifts Required Before Substitution Can Continue

These Pergyra surface gaps will block compiler-internal substitution
beyond the lexer:

- **`Args() -> Array<String>`** -- so a Pergyra binary can be invoked
  with arbitrary source paths. Currently every Pergyra tool hardcodes
  its input path.
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

The first three lifts are the candidate scope before step 3 of the
substitution roadmap can start.

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
