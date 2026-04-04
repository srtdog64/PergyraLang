# Changelog

All notable changes to the Pergyra Language will be documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Parser support for leading-dot enum/result variant shorthand such as `.Some(x)`, `.None`, `.Ok(v)`, `.Err(e)`
- `Option<T>` source-level surface: `Some`, `None`, `IsSome`, `IsNone`, `UnwrapOption`
- `Option<T>` destructuring in `match` for both C and LLVM backends
- Semantic destructuring bindings for `Option<T>`, `Result<T>`, and tagged enum variants in `match`
- Limited exhaustiveness checking for `Option<T>`, `Result<T>`, and enum variant matches
- Redundancy warnings for duplicate covered variants and redundant `default` in variant-style matches
- Semantic effect-contract checking for explicit `/// @effects ...` declarations versus inferred function body effects
- Channel convenience built-ins: `TryRecv -> Option<T>`, `RecvTimeout -> Option<T>`, `TrySend`, `SendTimeout`
- Channel backpressure observation built-ins: `ChannelLength`, `ChannelCapacity`, `ChannelFull`
- Channel backpressure observation built-ins expanded with `ChannelSpace` and `ChannelClosed`
- `select` round-robin starting-point fairness in both C and LLVM backends
- Cooperative task cancellation surface: `Cancel(task)` and `IsCancelled()`
- Cooperative cancellation now propagates through spawned descendant tasks via inherited cancellation chains
- Source-level effect signature surface: `func F() -> T with effects remote, secure { ... }`
- `Box<T>` explicit handle surface: `Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`
- `Box<class>` can now serve as the explicit object-handle path for function parameters and returns in the C/semantic surface
- `subject` keyword now parses as a class-compatible subject declaration alias
- `relation`, `effect`, and `zone` now parse as top-level declaration keywords and pass semantic/HIR/codegen no-op handling
- `relation`, `effect`, and `zone` now support minimal `subject slot` / `object slot` body surface in parser and semantic passes
- `zone` now supports `relation slot` / `effect slot`, and `world` now supports `zone` slots for minimal layer composition
- `relation` and `effect` now support optional `for name: Type[, ...]` headers for subject endpoint/target declaration
- `zone` now supports `apply effectSlot to targetSlot` for minimal effect attachment semantics
- `zone` now supports `link relationSlot between left, right` for minimal relation wiring semantics
- `zone` now supports `detach effectSlot from targetSlot` and `unlink relationSlot between left, right` for minimal release semantics
- `zone` apply/detach and link/unlink now validate effect/relation endpoint arity and basic subject-type compatibility
- `zone` now emits warnings for subject-heavy shapes that exceed the recommended small active-subject profile
- Added `dto` keyword as a struct-compatible projection/declaration alias
- Added `ToObject(TargetStruct, subjectBinding)` as a minimal subject-to-object projection surface
- Added `ToDto(TargetDto, subjectBinding)` as a minimal subject-to-dto projection surface
- Added LLVM lowering parity for `ToObject` / `ToDto` subject projection built-ins
- Added optional domain-slot initializers so `object slot view: PlayerView = ToObject(PlayerView, player)` can be modeled directly in `relation` / `effect` / `zone`
- Added `refresh objectSlot from subjectSlot` as an explicit zone projection-refresh surface
- Added `maintain effectSlot on targetSlot` and `maintain relationSlot between left, right` as zone lifecycle-rule surfaces
- Added lifecycle warnings for duplicate or conflicting `maintain` rules versus `detach` / `unlink`
- Added `authority <subjectSlot>` and optional `by <subjectSlot>` authority annotations for zone lifecycle/projection operations
- Added `state name: effect ... on ...` / `state name: relation ... between ..., ...` as zone lifecycle state aliases
- Added zone lifecycle shorthand forms `apply/link/detach/unlink/maintain <stateName>`
- Loop resource-state flow restoration now avoids restoring through transient loop-body scopes
- `type_create_function` no longer performs `memcpy` on zero-parameter function signatures

### Changed
- Parser regression tests now cover docs-style shorthand in `return`, `let`, and `match case` positions
- Status docs and TODO now reflect current `Option<T>`/slot sugar/type inference implementation state
- Channel non-blocking/timeout built-ins intentionally reject movable resource channels until conditional ownership transfer is modeled explicitly
- Cancellation is currently best-effort/cooperative rather than preemptive
- World architecture docs now treat `world` as the final top-level execution boundary and fix the long-term layer model as `ability -> role -> party -> relation -> effect -> zone -> world`
- Architecture docs now adopt a subject-first ontology: `struct` is the value type, `subject` is the identity-bearing host type, current `subject`/`class` syntax is treated as the subject surface, and `actor` is positioned as a subject execution profile
- Core docs now keep `entity` out of the language ontology and define `object` as a passive interpretation mode of `subject` rather than a separate top-level kind
- Core ontology docs now define `dto` as the compact external-boundary projection of an object representation
- Zone docs now treat authority, `by` actors, and state aliases as the current lifecycle/projection surface

### Fixed
- Restored the missing builtin-name argument in `ChannelLength/ChannelCapacity/ChannelFull` semantic diagnostics, fixing the optimizer-sensitive `test-semantic` crash in the async-system suite

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
