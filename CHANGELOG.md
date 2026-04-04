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
- Channel status built-ins: `TrySendStatus -> Option<Bool>`, `SendTimeoutStatus -> Option<Bool>`
- Channel backpressure observation built-ins: `ChannelLength`, `ChannelCapacity`, `ChannelFull`
- Channel backpressure observation built-ins expanded with `ChannelSpace` and `ChannelClosed`
- `select` round-robin starting-point fairness in both C and LLVM backends
- Cooperative task cancellation surface: `Cancel(task)` and `IsCancelled()`
- Cooperative cancellation now propagates through spawned descendant tasks via inherited cancellation chains
- Source-level effect signature surface: `func F() -> T with effects remote, secure { ... }`
- `Box<T>` explicit handle surface: `Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`
- `Box<class>` can now serve as the explicit object-handle path for function parameters and returns in the C/semantic surface
- `subject`, `class`, `struct`, `object`, and `dto` declarations now preserve distinct nominal flavors in the parser AST
- `subject slot` and `ToObject` / `ToDto` source validation now require `subject` declarations instead of accepting bare `class`
- `role` declarations now reject non-subject nominal bindings, and `party` role slots now require abilities backed by actual subject-bound role impls
- `relation`, `effect`, and `zone` now parse as top-level declaration keywords and pass semantic/HIR/codegen no-op handling
- `relation`, `effect`, and `zone` now support minimal `subject slot` / `object slot` body surface in parser and semantic passes
- `zone` now supports `relation slot` / `effect slot`, and `world` now supports `zone` slots for minimal layer composition
- `relation` and `effect` now support optional `for name: Type[, ...]` headers for subject endpoint/target declaration
- `zone` now supports `apply effectSlot to targetSlot` for minimal effect attachment semantics
- `zone` now supports `link relationSlot between left, right` for minimal relation wiring semantics
- `zone` now supports `detach effectSlot from targetSlot` and `unlink relationSlot between left, right` for minimal release semantics
- `zone` apply/detach and link/unlink now validate effect/relation endpoint arity and basic subject-type compatibility
- `zone` now emits warnings for subject-heavy shapes that exceed the recommended small active-subject profile
- Added `object` keyword as a struct-compatible passive/projection declaration alias
- Added `dto` keyword as a struct-compatible projection/declaration alias
- Added `ToObject(TargetObject, subjectBinding)` as a minimal subject-to-object projection surface
- Added `ToDto(TargetDto, subjectBinding)` as a minimal subject-to-dto projection surface
- Added `dto slot` surface to relation/effect/zone domain bodies
- Added LLVM lowering parity for `ToObject` / `ToDto` subject projection built-ins
- Added optional domain-slot initializers so `object slot view: PlayerView = ToObject(PlayerView, player)` can be modeled directly in `relation` / `effect` / `zone`
- Added `refresh objectSlot from subjectSlot` as an explicit zone projection-refresh surface
- Added `publish dtoSlot from subjectSlot` as an explicit zone dto-projection surface
- Added `maintain effectSlot on targetSlot` and `maintain relationSlot between left, right` as zone lifecycle-rule surfaces
- Added lifecycle warnings for duplicate or conflicting `maintain` rules versus `detach` / `unlink`
- Added `authority <subjectSlot>` and optional `by <subjectSlot>` authority annotations for zone lifecycle/projection operations
- Added `authority <subjectSlot> requires Ability[, ...]` to validate authority subjects against role-implemented abilities
- Added world lifecycle surface: `state name: zone zoneSlot`, `activate/deactivate/maintain zoneOrState`
- Added `HasZone(zoneOrState)` as a world lifecycle query surface
- Added `state name: effect ... on ...` / `state name: relation ... between ..., ...` as zone lifecycle state aliases
- Added zone lifecycle shorthand forms `apply/link/detach/unlink/maintain <stateName>`
- Added `HasState(stateName)` as a zone-state query builtin for zone declarations and zone methods
- Added `HasLayer(layerSlot)` as a zone-layer query builtin for declared `relation slot` / `effect slot` names
- Added `HasProjection(slotName)` as a relation/effect/zone projection-query builtin for declared `object slot` / `dto slot` names
- Expanded `HasState` with slot-aware forms for effect targets and relation endpoints
- Added C backend lowering for zone/world lifecycle state as `__state_*`, `__zone_active_*`, and `__zone_state_*` flags with generated sync helpers
- Added C backend contextual lowering for `HasLayer(...)`, `HasState(...)`, and `HasZone(...)` inside zone/world methods
- Added C backend contextual lowering for `HasProjection(...)` inside relation/effect/zone methods
- Added C backend incremental sync semantics so zone/world methods run generated sync helpers before and after body execution, applying `refresh` / `publish` projections and lifecycle flags
- Added LLVM parity for zone/world sync helpers, zone/world method pre/post sync, and contextual `HasLayer(...)` / `HasState(...)` / `HasZone(...)` lowering
- Added C backend struct/method emission for `relation` and `effect` declarations instead of treating them as declaration-only no-ops
- `world`, `systemic`, `relation`, `effect`, and `zone` now behave as contextual keywords so they remain valid local variable names outside declaration positions
- Added subject/class lowering split in both C and LLVM backends: `subject` methods use pointer-self cells, while `class` methods use value-self dispatch
- Added semantic split so `subject` forbids plain copy / plain value parameter / plain value return, while `class` remains passable and copyable by value
- Added actor-as-subject-profile semantic alignment so `actor` participates in role binding, `subject slot`, `ToObject` / `ToDto`, and subject copy restrictions
- Added `subject Name actor { ... }` as a subject-first actor profile surface that lowers through the existing actor pipeline
- Added a semantic warning on standalone `actor Name { ... }` declarations so `subject Name actor { ... }` becomes the preferred actor surface
- Added plain/secure `Slot<subject>` / `Slot<actor>` local object-cell anchor support across semantic, C transpile, and LLVM smoke coverage
- Added actor constructor compound-literal lowering in the C backend so `actor` values participate in subject-profile object-cell codegen paths
- Added `own/ref Slot<subject-host>` / `own/ref SecureSlot<subject-host>` function-boundary transfer in semantic and C/LLVM backend lowering
- Added automatic paired-token exposure for secure boundary slot parameters (`s_token` inside function bodies)
- Added C transpiler regression coverage for subject-host slot boundary lowering
- Added LLVM smoke coverage for subject-host slot boundary transfer
- Added semantic restriction that `object` / `dto` declarations are field-only passive projection forms and cannot declare methods
- `ToObject` now accepts only `object` declarations, `ToDto` only `dto` declarations, and `refresh` / `publish` follow the same nominal projection split
- Direct `ToObject` / `ToDto` outside relation/effect/zone/world context now emit semantic warnings so domain-local projection flow is the preferred path
- `relation` / `effect` declarations now support domain-local `refresh` / `publish` projection sync and `<Type>_sync(self)` helpers around methods in both C and LLVM backends
- `relation` / `effect` positional constructors are now type-checked as nominal overlay instances and lower to runtime instances in both C and LLVM paths
- Loop resource-state flow restoration now avoids restoring through transient loop-body scopes
- `type_create_function` no longer performs `memcpy` on zero-parameter function signatures

### Changed
- Parser regression tests now cover docs-style shorthand in `return`, `let`, and `match case` positions
- Status docs and TODO now reflect current `Option<T>`/slot sugar/type inference implementation state
- Channel non-blocking/timeout built-ins intentionally reject movable resource channels until conditional ownership transfer is modeled explicitly
- Cancellation is currently best-effort/cooperative rather than preemptive
- World architecture docs now treat `world` as the final top-level execution boundary and fix the long-term layer model as `ability -> role -> party -> relation -> effect -> zone -> world`
- Architecture docs now adopt a subject-first ontology: `struct` is the value type, `subject` is the identity-bearing host type, `class` remains as a separate nominal surface, and `actor` is positioned as a subject execution profile
- Actor docs now reflect current implementation state: `actor` is already treated semantically as a subject execution profile, and `subject Name actor { ... }` is now supported alongside transitional standalone actor syntax
- Core docs now keep `entity` out of the language ontology and define `object` as a passive interpretation mode of `subject` rather than a separate top-level kind
- Core ontology docs now define `dto` as the compact external-boundary projection of an object representation
- Zone docs now treat authority, `by` actors, and state aliases as the current lifecycle/projection surface
- Class/object model docs now describe the first real behavioral split between `subject` and `class` instead of treating them as parser-only flavors
- Ownership/object-model docs now describe subject-host slot boundary transfer as a current C/semantic capability and move LLVM parity to remaining work
- Ownership/object-model and status docs now describe relation/effect runtime instance construction and method-call parity in both C and LLVM backends
- Added initial JavaScript backend policy doc that keeps inheritance/super out of the core language and prefers delegation/composition in JS lowering
- Ontology/projection docs now emphasize `subject -> relation/effect/zone/world context -> object/dto projection` instead of treating `dto` as a peer ontology axis
- `HasProjection(...)` is currently a semantic/C-backed relation/effect/zone query surface; LLVM/runtime parity for zone object/dto projection state remains follow-up work

### Fixed
- Restored the missing builtin-name argument in `ChannelLength/ChannelCapacity/ChannelFull` semantic diagnostics, fixing the optimizer-sensitive `test-semantic` crash in the async-system suite
- Added missing `-lm` linkage in build/runtime native link paths so runtime math helpers link cleanly in tests and LLVM smoke

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
