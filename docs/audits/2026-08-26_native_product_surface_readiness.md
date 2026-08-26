# Native product-surface readiness audit — 2026-08-26

## Scope and method

- Directive: `docs/agent_work_directives/remaining_native_boundary_readiness_audit_2026-08-26.md`
- Audited revision: `acdab822b7d1ce27c636f73392ebb1d7738bf08a`
- Surfaces: formatter, REPL, debugger, scaffold/new, and package init.
- Readiness means an existing typed Pergyra producer is reached from a
  production root and owns the complete observable being replaced. Native
  implementations, tests, validators, templates, and similarly named backend
  formatting functions are evidence, not candidate owners.
- No build or executable test was run for this audit.

## Summary

| Surface | Whole-surface readiness | Production-reachable Pergyra equivalent | Progress classification |
| --- | --- | --- | --- |
| Formatter | `NOT READY` | None | Product-tool dogfood only |
| REPL | `NOT READY`; compile/run sub-boundary is `READY` | Installed self-host default C compile/run, but no Pergyra REPL session owner | Replacing the direct native compile call counts as compiler substitution; replacing the UI loop alone is product-tool dogfood |
| Debugger | `NOT READY` | None for a debug session or execution cursor | Whole tool is product dogfood; a separately proved frontend replacement could count, but is not ready here |
| Scaffold/new | `NOT READY` | None | Product-tool dogfood only |
| Package init | `NOT READY` | None | Package-metadata/product dogfood only |

There is one narrow `READY` candidate: the REPL's internal compile/run call,
not the REPL product surface as a whole.

## 1. Formatter

### Observed public boundary and effect

- `src/pgy_driver.c:197-200` sends `pgy fmt <file>` directly to
  `driver_run_fmt_command`; package-mode `pgy fmt [--check|--write]` goes
  through the C package dispatcher.
- `src/compiler/pkg.c:101-126` resolves the manifest entry and calls the same C
  formatter.
- `src/compiler/fmt.c:34-143` owns token-stream layout. It normalizes
  indentation and braces while preserving token text/comments.
- `src/compiler/fmt.c:226-339` owns stdout mode, `--check`, in-place mutation,
  parseability and idempotence checks, temporary-file cleanup, exit status, and
  diagnostics.

### Pergyra owner and classification

No production Pergyra root accepts a source-format request or emits this
formatted-source observable. Specialized owners such as
`direct_mir_llvm_text_format_owner.pgy` format backend artifacts, not Pergyra
source. They are not equivalents.

Replacing this surface would improve product-tool dogfood, but would not
replace a compiler artifact path. Reuse of the lexer/parser by the C formatter
does not turn source formatting into compiler substitution.

**First missing fact:** a typed source-format plan/producer that owns comment
carriage, token spacing, indentation, stable output, and the stdout/check/write
outcome. No formatter or template system should be created merely to delete C
LOC.

## 2. REPL

### Observed public boundary and effect

- `src/pgy_driver.c:68` admits `--repl`; `src/pgy_driver.c:234-236` calls
  `repl_run()` before installed-driver compiler selection.
- `src/compiler/repl.c:63-143` owns prompts, exit/quit, declaration
  accumulation, multiline brace collection, synthesis of a temporary `Main`,
  temporary path/file lifecycle, and the final `Bye!` observable.
- For every executable line, `src/compiler/repl.c:148-149` calls
  `driver_run_pipeline(&rf)` directly with `do_run = true`. This is a live
  implicit native compiler execution inside a product tool.

### Pergyra owner and classification

There is no production-reachable Pergyra REPL session owner. In particular,
the compile driver argv owner does not own prompts, accumulated declarations,
multiline admission, or session cleanup.

The narrower compile/run request already has a production equivalent:
`src/pgy_driver.c:321-328` routes the same default C binary/run shape to
`c_runner_execute_installed_self_host_c`, which materializes C through the
installed Pergyra driver (`src/compiler/c_runner.c:21-92`). Missing
`PGY_SELF_DRIVER_BIN` fails explicitly in
`src/compiler/self_host_driver.c:196-202`.

Therefore:

- the complete REPL remains **`NOT READY`**;
- deleting only its `driver_run_pipeline` compile bypass in favor of the
  already installed self-host compile/run boundary is **`READY`** and would
  count as actual compiler substitution;
- rewriting the prompt/session loop in Pergyra would be product-tool dogfood,
  not additional compiler substitution.

**First missing whole-surface fact:** a typed REPL session transition fact that
owns declaration state, multiline completeness, request outcome, continuation
after a rejected line, and cleanup. It is not needed to close the narrower
native compile bypass.

## 3. Debugger

### Observed public boundary and effect

- `src/pgy_driver.c:213-214` routes `pgy debug <file>` directly to the C
  debugger.
- `src/compiler/debugger.c:1-14` explicitly describes an AST-walking source
  debugger, not DWARF/CodeView or GDB/LLDB integration.
- `src/compiler/debugger.c:305-377` reads source, invokes the native lexer,
  parser, and semantic analyzer, prints diagnostics, and starts an interactive
  walk.
- `src/compiler/debugger.c:196-296` owns prompts, stepping, breakpoints,
  context, backtrace, quit, and the source-AST traversal order.
- `tests/tooling_conformance_smoke.sh:90-107` exercises only the current native
  parse/semantic/interactive-quit observable; it is not Pergyra ownership
  evidence.

### Pergyra owner and classification

Existing Pergyra parser/semantic facts do not own a debugger session, source
cursor, breakpoint table, command transition, or AST-walk execution. No
production Pergyra debugger equivalent was found.

Replacing the complete debugger is product-tool dogfood. A future, separately
bounded removal of its C parser/semantic admission could count as frontend
substitution, but this surface has neither a complete debug owner nor an
executable owner/missing-owner falsifier today.

**First missing fact:** a typed debug-session transition owner that binds an
admitted program identity to the current source location, command, breakpoint
state, and next execution cursor.

## 4. Scaffold and new

### Observed public boundary and effect

- `src/pgy_driver.c:215-216` routes `pgy scaffold` to the C owner.
- `src/pgy_driver.c:217-230` rewrites `pgy new <dir>` to
  `scaffold project <dir>` and calls that same owner.
- `src/compiler/driver_scaffold.c:476-535` owns the kind dispatch for class,
  object, project, simulator, subject, tobject, vessel, world, and zone.
- The same file owns single-file template contents and filesystem writes.
  `src/compiler/driver_scaffold_project.c:167-413` owns project paths, source
  templates, manifest/lock text, ordered writes, diagnostics, and success
  output. The resulting directory tree is the observable.

### Pergyra owner and classification

No production-reachable Pergyra code owns this template inventory or its
filesystem mutation protocol. Compiler fixtures and files containing the word
“scaffold” are not project-template producers.

Replacing scaffold/new would be product-tool dogfood only. Generated Pergyra
source does not make the generator a compiler path.

**First missing fact:** a versioned scaffold plan that binds kind, target,
ordered path/content inventory, collision policy, and an explicit commit or
partial-failure outcome. Do not build it solely to remove native C.

## 5. Package init

### Observed public boundary and effect

- `src/pgy_driver.c:201-202` routes `pgy init [name]` directly to
  `driver_run_pkg_init`.
- `src/compiler/pkg.c:130-182` owns name validation, refusal to overwrite an
  existing `pgy.toml`, manifest creation, conditional `main.pgy` creation,
  manifest reload, and lock publication.
- `src/compiler/pkg_manifest.c:548-556` owns package-name admission and
  `src/compiler/pkg_manifest.c:575-596` owns the `pgy.toml` template.
- `src/compiler/pkg_lock.c:287-322` owns the deterministic `pgy.lock` text and
  publication diagnostic.
- `tests/package_module_resolver_smoke.sh:73-95` observes those native files and
  messages. It does not establish a Pergyra producer.

### Pergyra owner and classification

The self-hosted module-manifest resolver is a bounded consumer/tool, not an
initializer that owns `pgy.toml`, `main.pgy`, and `pgy.lock` mutation. Installed
self-host package compilation owns compilation after metadata admission; it
does not own package initialization.

Replacing `pgy init` is package-metadata/product dogfood, not compiler
substitution.

**First missing fact:** a typed package-init transaction plan that owns the
validated name, exact manifest/main/lock payloads, existing-file policy,
ordered publication, and explicit partial-failure/commit outcome.

## Sole ready candidate and smallest future falsifier

Candidate: delete the REPL's direct native compile/run call while retaining the
existing C session UI. Route its synthesized temporary source through the same
installed self-host C compile/run boundary already used by public default C.
The fact owner remains the installed Pergyra compiler world; the C REPL is only
the last orchestration consumer.

The smallest gate should be a sourced sibling of
`tests/self_hosted/parity/installed_driver_cli_mode_owner.sh`, reused by
`self-host-installed-driver-cli-mode-test-smoke` (`Makefile:4074-4076`). This
reuses the existing `pgy` and installed self-host driver build; it needs no new
Make target or CI job.

Exact falsifiers:

1. Pipe `Log("repl-self-host");` followed by `exit` into public `pgy --repl`.
   Require the prompt/session markers, exactly one `repl-self-host` program
   line, clean installed-driver compilation, and `Bye!`.
2. Repeat with `PGY_SELF_DRIVER_BIN` naming a nonexistent executable. Require
   the installed-driver-unavailable diagnostic and no compiled/program payload;
   the REPL must not retry natively.
3. Submit an unsupported source line followed by `exit`. Require an explicit
   typed rejection, no program payload for that line, and no native retry.
4. Add a static ratchet rejecting `driver_run_pipeline(` in
   `src/compiler/repl.c`. Positive transcript parity alone is insufficient,
   because the current native bypass can produce the same user output.

No other surface has an existing complete Pergyra owner, so no second
successor candidate is selected by this report.
