# `src/self_hosted/` -- Self-Hosted Substitution Track

This directory holds **Pergyra-language implementations** of
compiler-adjacent tools and staged compiler-pass substitutes. It is the working
surface for soft self-host tools, partial self-host validators, and hard
substitution rungs that have C/LLVM oracle parity.

**This directory is not the compiler core.** The C implementation under
`src/` remains the oracle for soft, partial, and hard substitution. See
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
    main.pgy + fixture/ + expected/ + intent.md
  codegen/                        -- mirrors C-side src/codegen/
    main.pgy + fixture/ + expected/ + intent.md
  air/  hir/  mir/                -- IR-stage placeholders
  compiler/                       -- driver placeholder, mirrors src/compiler/
  runtime/                        -- native runtime kernel stays C; portable policy can move
  lsp/                            -- mirrors C-side src/lsp/ (placeholder)
  lib/                            -- shared Pergyra helpers (e.g. text_scan)
  fuzz/                           -- deterministic Pergyra-origin corpus generators
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
  `lib/diagnostic.pgy` owns stable diagnostic-block rendering so compiler
  slices do not hand-build raw error strings or JSON in their `main.pgy` files.
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
- **2026-06-16** -- runtime self-hosting is split explicitly. The native runtime
  kernel stays C, while portable runtime policy/checker work may move to
  Pergyra and stay classified as soft self-host evidence until a generated
  program links a Pergyra-written runtime component.
- **2026-06-16** -- the first AIR graph consumer slice lands as soft self-host
  evidence: node-id uniqueness, live-dump node-count integrity, live-dump
  back-reference range checking, fixture-shaped edge referential integrity, and
  root reachability/worklist checks are Pergyra-origin tools with C/LLVM parity
  legs. They do not count as compiler-core substitution yet, but they prove the
  deterministic graph traversal substrate needed by the first middle-end pass.
- **2026-06-17** -- the first hard compiler-core rung opens under `codegen/`.
  The Pergyra emitter consumes `pgy --ast` text and emits standalone C for a
  bounded `Int` / `Bool` / `String` / growable `Array<Int>` / `Array<String>`
  function subset. The parity gate builds the emitter through C and LLVM,
  compiles the emitted C, and compares run-stdout against the C-backend oracle.
  It currently stands at rung-0..15 with 35 fixtures, including `StringTrim`,
  `FileExists` / `ReadFile` file I/O, `Args()` user-argument snapshots, and
  value-passed Int-field structs plus Array<Int> parameter/return flow.
- **2026-06-21** -- `fuzz/backend_parity_generator/` adds the first
  Pergyra-origin deterministic backend fuzz corpus generator. It is soft
  self-host evidence, not compiler-core substitution: the Pergyra program owns
  generated source text and manifest shape, while the shell parity driver only
  compiles the generator through C/LLVM, checks byte-identical generated
  corpora, and optionally runs the generated cases through both backends.
  Focused gates: `make self-host-fuzz-backend-generator-parity-test-smoke` and
  `make fuzz-backend-parity-test-smoke`; `make fuzz-backend-parity-matrix-test-smoke`
  runs a bounded multi-seed variant of the generated C/LLVM oracle.

## Non-Negotiable Rules

1. Do not start a full compiler rewrite from this directory.
2. The C compiler remains the oracle during soft, partial, and hard substitution.
3. Prefer stable file/IR inputs over direct compiler internals. Use JSON when
   the owner format is JSON; use diagnostic blocks for diagnostic verdicts.
4. A tool ships only when its parity check passes the C oracle.
5. Build artifacts belong under ignored scratch space such as `.tmp/`; do not
   leave compiler outputs beside `main.pgy` sources.
6. Fuzz generators belong under `src/self_hosted/fuzz/`, not `tools/`, unless
   they are audit tools with a C/shell oracle and the full tool scaffold
   contract. Their stable contract is deterministic corpus generation plus a
   parity driver that proves backend-equivalent generator output.
