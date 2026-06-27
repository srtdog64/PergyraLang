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

Compiler-stage directories are source-owner surfaces, not a copy of the C
folder topology. They stay under `src/self_hosted/` only when they contain
Pergyra source or source-owner documentation. Oracle harnesses, parity scripts,
and long-lived test payloads belong under `tests/self_hosted/`.

```
src/self_hosted/
  README.md                       -- this file
  PROGRESS.md                     -- honest substitution percentage
  lexer/                          -- token stream substitution owner
    main.pgy (+ owner modules as split) + intent.md
  parser/                         -- AST text substitution owner
    main.pgy (+ owner modules as split) + intent.md
  semantic/                       -- diagnostic verdict substitution owner
    main.pgy (+ owner modules as split) + intent.md
  codegen/                        -- bounded AST-text to C emitter owner
    main.pgy + intent.md
    input/ run/ text/ type_facts/ abi_layout/ runtime_abi/ emission/
  mir_lower/                      -- MIR JSON fact-only lowering substitute
    main.pgy + owner modules + intent.md
  air/  hir/  mir/                -- IR-stage placeholders
  compiler/                       -- PgyCompilerWorld hard-substitution owner
  runtime/                        -- native runtime kernel stays C; portable policy can move
  lsp/                            -- language-server placeholder
  lib/                            -- shared Pergyra owners (path, diagnostics, text scan)
  fuzz/                           -- deterministic Pergyra-origin corpus generators
  tools/                          -- peripheral audit tools (NOT counted toward substitution)
    <tool_name>/
      intent.md                   -- input/output contract + oracle
      main.pgy                    -- entrypoint only
      *_owner.pgy                 -- named source-of-truth owners
```

`tests/self_hosted/` owns the oracle side of the same track:

```
tests/self_hosted/
  parity/                         -- C / LLVM / Pergyra comparison harness
    README.md
    lexer_parity.sh + parser_parity.sh + <tool>_parity.sh
```

Some legacy `fixture/` and `expected/` directories are still collocated under
stage/tool source directories. They are test payloads, not source owners. New
parity fixtures should be added under `tests/self_hosted/`, and existing
payloads should migrate there stage-by-stage with the corresponding parity
script in the same change. Do not add a new top-level test harness directory
under `src/self_hosted/`.

The target shape is not "one folder, one monolithic `main.pgy`". `main.pgy`
is the CLI/orchestration entrypoint; semantic decisions belong in named
source-of-truth owner modules. `semantic/` also owns its program-input fact in
`source_bundle_owner.pgy`: imports are expanded there before `CheckProgram`
consumes the bundle, so the entrypoint does not define "program" by accident.
`lexer/` owns its argv/default source path and file-read boundary in
`source_input_owner.pgy`; `codegen/` owns its AST path/read boundary in
`input/ast_input_owner.pgy`, its AST-text line inventory in
`input/ast_text_inventory_owner.pgy`, its CLI orchestration in
`run/codegen_run_owner.pgy`, expression/text scanning in `text/`, type evidence
in `type_facts/`, emitted-symbol rows in `compiler/symbol_table_owner.pgy`,
ABI type spelling in `abi_layout/`, runtime helper symbol facts in
`runtime_abi/`, and C emission participants in `emission/`; `mir_lower/`
owns its MIR JSON path/read/schema boundary in `mir_json_input_owner.pgy` and
document-order Program assembly in
`program_lower.pgy`; `semantic/` follows the same entrypoint-plus-owner shape.
`parser/` has started the same transition with
error, cursor/token, source path/import input, root Program assembly, type-name, expression, statement/block,
function-declaration, top-level declaration dispatch, branch declaration
owners (`type`/`ability`/`event`/`enum`/`zone`/`effect`/`relation`/`role`/
`intent`/nominal-domain hosts), and compact-tree text owners. `main.pgy` is now
entrypoint orchestration only for the parser tool.

Peripheral self-host tools follow the same shape when they graduate past a
scaffold. For example, `tools/diagnostic_catalog_checker/main.pgy` only calls
the run owner; `scan_owner.pgy` owns catalog fact extraction, `report_owner.pgy`
owns the `pgy.selfhost.diagnostic-catalog.v1` JSON schema, and `run_owner.pgy`
owns filesystem input plus exit policy. A parity harness must copy or compile
the tool directory as a unit so owner imports are tested instead of flattened
back into a hidden monolith.
`tools/air_graph_json_validator/` follows the same rule: scan owns AIR JSON
count/finding facts, report owns `pgy.selfhost.air-graph-validator.v1`, and run
owns fixed fixture input plus exit policy.

The compiler world is different from a C-style driver folder.
`compiler/world.pgy` is a parse-gated Pergyra source file that names the
self-hosted compiler as `PgyCompilerWorld` and `CompilePergyraProgram`.
`compiler/path_manifest_owner.pgy` owns the current self-host source/test/parity
path values consumed by that world. Source intake, lexing, parsing, semantic
checking, MIR lowering, emission, and parity are derived stage zones/intents
under that root compiler intent. Stage
directories still own their facts; `PgyCompilerWorld` owns the visible compiler
flow. New hard-substitution stages should attach to that intent vocabulary
instead of creating another folder-local orchestration alias.

Import de-duplication is now a compiler fact and a self-hosted parser fact.
Sibling owner modules should declare the fact-owner imports they actually
consume. The entrypoint must not act as a dependency aggregator or preserve a
hidden declaration order; if an owner needs another owner fact, import that
owner directly and let the import graph materialize it once.

The compiler-stage shape is executable policy, not just a convention:
`tests/self_hosted_component_contract_smoke.sh` requires every active
compiler-stage `main.pgy` to stay an entrypoint-only boundary (one `Main`
function, no local helper functions, no control-flow/parser/JSON/diagnostic
work), and requires each stage owner source to stay at or below the 600-line
split-review cap. It also requires every active stage `.pgy` source to be
listed in [`OWNERS.md`](OWNERS.md), which is the durable owner manifest for
self-hosted stage responsibilities. If a stage needs new semantics, add or
split a named owner module, document its owner responsibility there, and import
it from the entrypoint.

Build and verification are intentionally separate. A normal compiler build does
not run the self-hosted parity suite. For fast local checks, use
`make self-host-preparation-contract-test-smoke`; for development/CI evidence,
use `make self-host-preparation-test-smoke`, which also runs the heavy
C/LLVM/Pergyra parity bundle.

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
  `lib/path.pgy` owns self-hosted source/import path string facts such as
  dirname, absolute-path detection, joining, and `./` / `../` import-relative
  normalization. Runtime file-access authorization still belongs to the native
  runtime IO path resolver.
  `lib/json.pgy` owns bounded JSON string-read primitives plus JSON string,
  field, object, and array emission for fact-shaped tools; schema-specific
  object decisions and full object/array iteration stay with the consuming
  owner until a full JSON parse/emit owner lands.
  `make self-host-preparation-test-smoke` now runs every parity script, not just
  the scaffold check. This is dogfood evidence only; the compiler core remains C.
- **2026-05-28** -- first compiler-internal substitution candidates land as
  C-side-mirroring siblings: `lexer/` gates byte-equality against
  `pgy --tokens` (7 fixtures), and `parser/` gates a growing text-tree
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
- **2026-06-23** -- semantic program input is no longer a hidden `main.pgy`
  decision. `source_bundle_owner.pgy` owns root-source plus recursive import
  expansion, `semantic_parity.sh` includes an import-backed function-call
  fixture, and `selfcheck_sources.sh` now checks `src/self_hosted/semantic/main.pgy`
  as a real imported source bundle instead of generating a grep-concatenated
  semantic unit.
- **2026-06-23** -- the real-source semantic selfcheck now checks
  `src/self_hosted/lexer/main.pgy` through the same source-bundle owner. The
  retired lexer grep-concat unit and lexer `fixture/source.txt` input side
  channel are contract-gated against reappearing.
- **2026-06-25** -- `src/self_hosted/lexer/scan_owner.pgy` now declares its
  `char_owner.pgy` and `token_owner.pgy` dependencies directly and joins the
  real-source semantic selfcheck. `main.pgy` stays an entrypoint and no longer
  imports scan-loop internals.
- **2026-06-25** -- `src/self_hosted/semantic/source_bundle_owner.pgy` now
  declares its path/text-scan dependencies directly and joins the real-source
  semantic selfcheck. `semantic/main.pgy` stays an entrypoint and no longer
  imports source-bundle internals.
- **2026-06-25** -- `src/self_hosted/semantic/diagnostic_owner.pgy` now
  declares its shared renderer and diagnostic-code vocabulary dependencies
  directly. `semantic/main.pgy` consumes the diagnostic owner instead of
  importing those internals.
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
  `main.pgy` is only the CLI/orchestration entrypoint; source-of-truth
  decisions live in named owner modules such as `type_env`, `expr_rewrite`,
  `stmt_emit`, `function_emit`, and `program_emit`. It currently stands at
  rung-0..20 with 63 fixtures, including `StringTrim`, `FileExists` /
  `ReadFile` file I/O, `Args()` user-argument snapshots, value-passed
  `Int` / `Bool` / `Float` / `String` field structs plus nested struct-valued
  fields, Array<Int> parameter/return flow, `Result<Int>` `?` early-return
  lowering, `Option<Int>` value flow, and `ArrayReverse` value copy lowering.
- **2026-06-23** -- codegen AST input is no longer owned by `main.pgy`.
  `ast_input_owner.pgy` owns `Args()[0]`/default fixture selection,
  missing-file diagnostics, and AST file reads; `main.pgy` now only wires
  owned AST text into `GenerateC`.
- **2026-06-21** -- `fuzz/backend_parity_generator/` adds the first
  Pergyra-origin deterministic backend fuzz corpus generator. It is soft
  self-host evidence, not compiler-core substitution: the Pergyra program owns
  generated source text and manifest shape, while the shell parity driver only
  compiles the generator through C/LLVM, checks byte-identical generated
  corpora, and optionally runs the generated cases through both backends.
  Focused gates: `make self-host-fuzz-backend-generator-parity-test-smoke` and
  `make fuzz-backend-parity-test-smoke`; `make fuzz-backend-parity-matrix-test-smoke`
  runs a bounded multi-seed variant of the generated C/LLVM oracle. The
  generator C/LLVM corpus parity leg is also wired into
  `make self-host-preparation-test-smoke`, so the default hard-preparation
  bundle now covers the generator itself.

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
