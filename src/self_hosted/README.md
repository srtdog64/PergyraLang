# `src/self_hosted/` -- Soft Self-Host Track

This directory holds **Pergyra-language implementations** of
compiler-adjacent tools. It is the working surface for Stage 1 (soft
self-host) and Stage 2 (partial self-host) of the staged roadmap.

**This directory is not the compiler core.** The C implementation under
`src/` remains the oracle for soft and partial self-host. See
[docs/self_hosted/01_staged_roadmap.md](../../docs/self_hosted/01_staged_roadmap.md).

## Entry Contract

Every tool in this directory must satisfy the agent-entry contract
([docs/self_hosted/00_agent_entry.md](../../docs/self_hosted/00_agent_entry.md)):

- **one** explicit input contract,
- **one** explicit output contract,
- **one** smoke test,
- **one** parity check against the C implementation.

A tool that does not yet pass this contract is a *scaffold* and must say so in
its `intent.md`.

## Layout

Compiler-stage directories mirror C-side `src/` siblings (one per
compiler component, no umbrella).

```
src/self_hosted/
  README.md                       -- this file
  PROGRESS.md                     -- honest substitution percentage
  lexer/                          -- mirrors C-side src/lexer/
    main.pgy + fixture/ + expected/ + intent.md
  parser/                         -- mirrors C-side src/parser/
    main.pgy + fixture/ + expected/ + intent.md
  semantic/                       -- mirrors C-side src/semantic/
  codegen/                        -- mirrors C-side src/codegen/ (placeholder)
  air/  hir/  mir/                -- IR-stage placeholders
  compiler/                       -- driver placeholder, mirrors src/compiler/
  runtime/                        -- stays C; placeholder for layout parity
  lsp/                            -- mirrors C-side src/lsp/ (placeholder)
  lib/                            -- shared Pergyra helpers (e.g. text_scan)
  tools/                          -- peripheral audit tools (NOT counted toward substitution)
    <tool_name>/
      intent.md                   -- input/output contract + oracle
      main.pgy
      expected/
  parity/                         -- C / LLVM / Pergyra comparison harness
    README.md
    lexer_parity.sh + parser_parity.sh + <tool>_parity.sh
```

## Current Status

- **2026-05-26** -- directory bootstrapped. First tool:
  `tools/diagnostic_catalog_checker/`. Pergyra implementation is a rung-2
  partial checker; the C-side reference still lives in
  `tests/diagnostic_registry_smoke.sh`. Parity rung 1 checks clean-repo exit
  class and schema-header visibility; rung 2 checks clean JSON plus live
  `codes`, `documented`, `missing`, `duplicates`, and `orphans` counters
  against shell drift detectors, then verifies a synthetic missing-code fixture
  returns `ok:false` with a `findings[]` entry and exit code `1`, plus a
  missing-input fixture that returns `input_error` and exit code `1`. **No claim
  of self-host is implied by the existence of these files.**
- **2026-05-27** -- soft self-host track now contains **10 rung-2 tools**:
  diagnostic catalog checker, stable subset section checker, AIR graph JSON
  validator, backend output comparator, module manifest resolver, stdlib
  dispatch inventory checker, doc link checker, production header size checker,
  production C size checker, and examples inventory checker. The shared
  `lib/text_scan.pgy` owns reusable scan helpers used by multiple tools.
  `make self-host-preparation-test-smoke` now runs every parity script, not just
  the scaffold check. This is dogfood evidence only; the compiler core remains C.
- **2026-05-28** -- first compiler-internal substitution candidates land as
  C-side-mirroring siblings: `lexer/` gates byte-equality against
  `pgy --tokens` (6 fixtures), and `parser/` gates a growing text-tree
  subset against `pgy --ast` (9 fixtures: hello, multi-statement,
  parameters, no-arg / ident-arg calls, let with mixed literals, multi-
  function, return). The earlier `compiler/lexer/lex_minimal/` and
  `compiler/parser/parse_minimal/` nesting was flattened to mirror
  C-side `src/<component>/` exactly. This is rung-1 only: it proves the
  side-by-side substitution loop, not full compiler parity.
- **2026-06-15** -- soft self-host track now contains **12 rung-2 tools**.
  `tools/ast_read_surface_checker/` reads the shared
  `tests/ast_read_surface_ratchet.txt` ratchet and proves the same
  enum/source_ast/source_decl/routine-source-decl counts as the shell smoke,
  including a synthetic growth fixture. This strengthens capability 5 evidence
  but remains peripheral audit tooling, not compiler-core substitution.
- **2026-06-15** -- the first semantic compiler-internal rung lands under
  `semantic/`. It checks a deliberately tiny typed `let` / return subset,
  compiles through C and LLVM where available, and compares the same fixtures
  against the C compiler accept/reject oracle. This starts semantic
  substitution without pretending the full type checker is replaced.
- **2026-06-16** -- `make self-host-preparation-test-smoke` is green again on
  main after refreshing the doc-link checker expected counts for the current
  `docs/INDEX.md`. The measured compiler-internal substitution is now 8,642
  Pergyra LOC vs 254,742 C/header/inc LOC (~3.39% LOC-scale): lexer and parser
  substitution are active, semantic is rung-2 with scoped block and
  simple/compound undefined-identifier parity, and HIR/MIR/codegen/runtime/
  compiler/LSP remain 0%.

## Non-Negotiable Rules

1. Do not start a full compiler rewrite from this directory.
2. The C compiler remains the oracle during soft/partial self-host.
3. Prefer stable JSON/IR inputs over direct compiler internals.
4. A tool ships only when its parity check passes the C oracle.
5. Build artifacts belong under ignored scratch space such as `.tmp/`; do not
   leave compiler outputs beside `main.pgy` sources.
