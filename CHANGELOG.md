# Changelog

All notable changes to the Pergyra Language will be documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Parser support for leading-dot enum/result variant shorthand such as `.Some(x)`, `.None`, `.Ok(v)`, `.Err(e)`

### Changed
- Parser regression tests now cover docs-style shorthand in `return`, `let`, and `match case` positions

## [0.3.0] - 2026-03-31

### Added
- **LLVM backend as default** — `--backend=llvm` is now the default compilation path
- **Generic monomorphization** — `func Identity<T>(x: T)` instantiates as `Identity_Int` at call site
- **AES-256-CTR encryption** — FIPS 197 compliant, constant-time S-Box, HMAC-SHA256 authentication
- **Green thread Windows porting** — `fiber.c` uses `VirtualAlloc`, scheduler uses `GetSystemInfo`
- **LSP server** (`bin/pgy-lsp`) — JSON-RPC 2.0, diagnostics, hover
- **Pattern matching destructuring** — `case Ok(x):` / `case Err(e):` in match statements
- **Lambda lookahead parser** — `(x: Int) => { ... }` no longer misparses as grouped expression

### Changed
- Parser split into 6 files: `parser.c`, `parser_expr.c`, `parser_stmt.c`, `parser_decl.c`, `parser_domain.c`, `parser_async.c`
- `system()` replaced with `_spawnvp`/`execvp` (no shell — immune to command injection)
- REPL temp files use TMPDIR + PID + salt (no fixed filenames)
- IV size increased from 8 to 12 bytes (96-bit nonce for CTR mode)
- MAC now covers IV: `HMAC-SHA256(key, iv || ciphertext)`

### Fixed
- Class method `self` parameter: value semantics instead of pointer
- Struct constructor zero-init: `Point p = {0}` instead of invalid `Point()`
- `auto __tmp` removed from runtime macros (C11 standard compliance)

## [0.2.0] - 2026-03-15

### Added
- **Module/import system** — `import "path.pgy";` with AST merging
- **Result<T,E> error handling** — `Ok()`, `Err()`, `IsOk()`, `Unwrap()`, `UnwrapOr()`
- **Standard library builtins** — `Abs`, `Min`, `Max`, `StringLength`, `Print`, `ToString`
- **REPL** — `pgy --repl` interactive shell
- **unsafe/defer keywords** — `unsafe { ... }` blocks, `defer { ... }` scope-exit
- **dyn keyword** — dynamic role slot binding with `bind party.slot = Role;`
- **LLVM backend** — initial implementation with select statement codegen

### Changed
- Ability/Role/Party/World fully implemented in both C and LLVM backends

## [0.1.0] - 2026-03-01

### Added
- Core pipeline: Lexer → Parser → AST → Semantic → HIR → C Backend
- Type system: Int, Long, Float, Double, Bool, String, Void
- Slot<T> / SecureSlot<T> container isolation
- Actor model with message passing
- Channel<T> CSP-style communication
- Ability/Role/Party/Systemic/World hierarchy
- match/for/while/if control flow
- Parallel blocks with task spawning
- Event system with subscribe/unsubscribe
- 165 unit tests (65 semantic + 100 transpile)
