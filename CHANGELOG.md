# Changelog

All notable changes to the Pergyra Language will be documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Regression-grade DND campaign scenario: `examples/dnd_tavern_campaign/`
  with tavern recruitment, deterministic DND sheet/spec factories, a three-floor
  dungeon, dragon boss resolution, transcript-first `results.txt`, and exact
  stdout/results goldens in example smoke
- `docs/testdoc/README.md` and `docs/testdoc/campaign_graph_fsm.md` to start a
  testdoc workflow where large scenarios are kept together with their design
  notes, discovered pain points, and exact regression coverage
- CLI optimization profiles: `--opt=dev|release`
- `tests/bench_backend.sh` for quick C vs LLVM compile+run timing on a scenario
- Backend-specific biome simulator result goldens to keep example smoke exact
  while C/LLVM parity is still being tightened
- New folder-based complex FSM scenario: `examples/fsm_factory/` with multi-file
  subject/zone/world orchestration, exact stdout goldens, and exact results-file
  golden coverage in example smoke
- New folder-based raid graph+FSM scenario: `examples/raid_graph_fsm/`
  with slot-based turn staging, world-owned graph costs, raider subject FSMs,
  and report-file smoke
- `src/semantic/type_checker.c` split into a smaller main file plus
  `type_checker_helpers.inc`, `type_checker_operator_expr.inc`, and
  `type_checker_decls.inc` to keep semantic analysis manageable
- `src/codegen/transpiler.c` split into a smaller main file plus
  `transpiler_helpers.inc` and `transpiler_domain_role.inc`
- `src/codegen/transpiler_helpers.inc` split again into a smaller core helper
  include plus `transpiler_expr_emitters.inc`
- `src/codegen/llvm_expr.c` split into a smaller main file plus
  `src/codegen/llvm_expr_helpers.inc` to keep LLVM expression codegen more manageable
- `src/tests/semantic/test_semantic_runtime.inc` split into themed semantic test
  includes to keep the runtime/domain regression corpus readable
- Folder-based example smoke entries: a path may now point to an example directory with `main.pgy`
- New multi-file example scenarios: `examples/battle_simulator/` and `examples/biome_simulator/`
- Richer biome simulator scenario: vegetation pools, species-specific traits, four-creature biome loops, world season/storm aggregation, and file report output
- Practical simulator pain-point tracker: `docs/27_simulator_pain_points.md`
- Nested vessel-backed projection paths in `ToObject` / `ToDto` and domain `refresh` / `publish`
- `vessel slot` support inside `zone` declarations for grouped passive state
- Folder-level exact `expected_results.txt` golden comparison for simulator outputs
- Backend-aware exact stdout goldens for folder-based simulator smoke scenarios
- Backend-aware exact `expected_results.<backend>.txt` golden comparison for simulator outputs
- Wider contextual-keyword identifier support for locals and parameters such as `world`, `zone`, `effect`, and `actor`
- Parser support for leading-dot enum/result variant shorthand such as `.Some(x)`, `.None`, `.Ok(v)`, `.Err(e)`

### Changed
- `examples/dnd_tavern_campaign/` now uses table/spec factories for class
  flavor, tavern hooks, floor scripts, dragon epilogue text, and zone/world
  morale tracking so the scenario reads more like a campaign transcript than a
  constructor dump
- `examples/dnd_tavern_campaign/` now also exercises `relation` / `effect`
  runtime layers and world-visible `HasZoneLayer` / `HasZoneState` queries via
  party-bond and battle-blessing campaign state
- Those DND campaign layers now feed back into route pressure, trap pressure,
  morale, and dragon preparation, so relation/effect runtime is no longer just
  visible in reports but also changes scenario outcomes
- The C backend now filters early standalone helper forward declarations to
  signatures whose types are already known, fixing factory-style helpers such
  as `BuildJourneyZone() -> JourneyZone` in large multi-file scenarios
- Example smoke now prefers the newer `/tmp/pgy-PergyraLang-bin/pgy` build over
  a stale repo-local `bin/pgy` unless `PGY_BIN` is explicitly set
- LLVM world `all/any` derived-state queries now compose correctly for example
  scenarios such as `battle_simulator`
- LLVM slot built-ins now use the stable `Write(slot, value)` / `Read(slot)`
  path for exact scenario parity, and `raid_graph_fsm` now uses that stable
  slot surface while its richer graph/FSM logic drives real room movement and loot
- LLVM backend optimization pipeline now uses a host-tuned target machine with
  `default<O3>` passes, and the native link step now compiles the runtime with
  `-march=native -mtune=native` for a fairer comparison against the C backend
- LLVM object generation now initializes targets once and reuses the same host
  target machine across optimization and object emission
- Backend optimization is now profile-driven: `dev` uses lighter codegen/link
  settings for faster iteration, while `release` keeps aggressive optimization
- The CLI default optimization profile remains `release` so existing behavior
  and exact scenario outputs stay stable unless `--opt=dev` is requested
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
- Added derived world-state contracts: `state name: zone zoneSlot projection projectionSlot`, `layer layerSlot`, `state zoneStateName`
- Added `HasZone(zoneOrState)` as a world lifecycle query surface
- Added `HasZoneProjection(zoneSlot, projectionSlot)`, `HasZoneLayer(zoneSlot, layerSlot)`, and `HasZoneState(zoneSlot, stateName)` as world-level cross-layer query built-ins
- Derived world states now lower in C/LLVM to `zone active && embedded zone projection/layer/state flag`
- Added composed world-state contracts: `state name: all zoneOrState[, ...]` and `state name: any zoneOrState[, ...]`
- Composed world states now lower in C/LLVM to combined `__zone_active_*` / `__zone_state_*` flag expressions
- Composed world states now warn on duplicate inputs and redundant mixing of a direct zone slot with its plain `state name: zone zoneSlot` alias
- World lifecycle now warns on duplicate `activate` / `deactivate` directives and conflicting `activate` + `deactivate` on the same underlying zone
- World lifecycle now also warns when `activate` and `maintain` redundantly target the same underlying zone
- Composed world states now warn when they directly reference raw world zone slots instead of plain world-state aliases
- `activate/deactivate/maintain <zoneSlot>` now resolve direct world zone slots consistently in C/LLVM sync paths
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
- `relation` / `effect` headers now accept `for object ...` bindable targets/endpoints, and zone contracts accept matching object slots in `apply/link/detach/unlink`
- Domain-local `refresh` / `publish` now accept object sources as well as subject sources, while still rejecting dto sources
- Fixed LLVM relation/effect projection-ready struct layout for object-target overlays so `object_layer_binding` no longer corrupts memory during `--emit-llvm`
- Added `vessel` as a nominal declaration flavor and `subject`-local `vessel name: Type;` field surface
- Added `action` as a subject-first declaration surface with minimal `requires` / `within` / `causes` / `authorized by` clause parsing and semantic validation
- Added a hard semantic error for legacy `func` declarations inside `subject`; `action` is now the only public subject verb surface
- Fixed async `func` / subject `action` flag overlap in the shared AST union so LLVM async entrypoints no longer mis-diagnose as actions
- Tightened `action` clause validation so `authorized by` requires subject hosts, `within` checks matching zone subject/authority coverage, and `causes` checks effect target compatibility plus zone effect-slot presence
- Added runtime bridging from zone-local subject `action` calls to matching `effect slot` activation and embedded effect sync in both C and LLVM backends
- Added C/LLVM lowering for nested nominal host calls such as `self.player.Attack()` so embedded subject/class dispatch no longer falls back to raw member-call syntax
- `object` / `dto` declarations now allow passive helper methods again
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
- `ToObject` now accepts only `object` declarations, `ToDto` only `dto` declarations, and `refresh` / `publish` follow the same nominal projection split
- Direct `ToObject` / `ToDto` outside relation/effect/zone/world context now emit semantic warnings so domain-local projection flow is the preferred path
- `relation` / `effect` declarations now support domain-local `refresh` / `publish` projection sync and `<Type>_sync(self)` helpers around methods in both C and LLVM backends
- `relation` / `effect` positional constructors are now type-checked as nominal overlay instances and lower to runtime instances in both C and LLVM paths
- Zone layer slots now lower as typed `relation` / `effect` runtime instances in both C and LLVM backends instead of placeholder pointers
- Zone sync now binds subject slots into embedded relation/effect layer endpoints or targets before calling `<Layer>_sync(&self->layer)`
- Direct `apply/link/detach/unlink` and `maintain effect/relation/state` now propagate real layer/state runtime changes in LLVM as well as C
- Added runtime regression coverage for embedded zone overlay projection reads such as `self.poison.view.hp` and `self.trust.packet.name`
- Added optional `self` lowering for bare field access and bare hosted helper calls across subject/class/relation/effect/zone/world bodies, with LLVM parity for bare host-field nested member chains like `battle.player.name` and `battle.Tick()`
- Added `pgy scaffold` and `pgy new` CLI support for generating starter `subject`, `vessel`, `object`, `dto`, `zone`, `world`, `simulator`, and `project` templates
- Added runtime regression coverage for world-to-zone cross-layer queries over embedded projection/layer/state flags
- Added LLVM/C vessel pointer-self parity for hosted `vessel func` emission and dispatch
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
- Vessel/action docs now match implementation more closely: `subject` uses `action` only, while `object` / `dto` keep passive helper `func`
- Core docs now define `object` as a passive state target that can hold state and react without initiating intent
- Core ontology docs now define `dto` as the compact external-boundary projection of an object representation
- Zone docs now treat authority, `by` actors, and state aliases as the current lifecycle/projection surface
- Class/object model docs now describe the first real behavioral split between `subject` and `class` instead of treating them as parser-only flavors
- Ownership/object-model docs now describe subject-host slot boundary transfer as a current C/semantic capability and move LLVM parity to remaining work
- Ownership/object-model and status docs now describe relation/effect runtime instance construction and method-call parity in both C and LLVM backends
- Added initial JavaScript backend policy doc that keeps inheritance/super out of the core language and prefers delegation/composition in JS lowering
- Ontology/projection docs now emphasize `subject -> relation/effect/zone/world context -> object/dto projection` instead of treating `dto` as a peer ontology axis
- `HasProjection(...)` now has semantic/C/LLVM runtime parity across relation/effect/zone projection-state queries
- World-level cross-layer queries now lower to embedded zone runtime flags in both C and LLVM backends

### Fixed
- The C backend now emits hosted method forward declarations for large host
  declarations such as `class`, `party`, `systemic`, `relation`, `effect`,
  `zone`, and `world`, so helper-order-sensitive scenario code no longer needs
  manual source reordering just to compile
- LLVM `ToString(Bool)` now widens bools before calling the integer-to-string
  runtime helper, fixing verifier failures in the DND campaign and other
  transcript-heavy scenarios
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
- Fixed world sync lowering to run as `command pass(reset/directives) -> zone sync pass -> derived pass` in both C and LLVM backends, and added transpile coverage for the emitted phase order
- Added incremental world propagation fields (`__zone_dirty_*`, `__world_derived_dirty`) so C/LLVM world sync can re-sync dirty zones and skip unnecessary derived recomputation
- Initialized world constructors with dirty zone/derived flags and made world methods conservatively invalidate embedded zones before post-sync, with LLVM smoke coverage for world-owned zone replacement propagation
- Added LLVM/runtime coverage for deeper nested member assignment (`self.zone.subject.field = value`) and secure boundary slot forwarding with paired token propagation
- semantic: `world` decl lookup helper가 `world/systemic`도 찾도록 보강되어, world value에 embed된 zone을 옛 바인딩으로 다시 mutate하는 경로가 semantic error로 제대로 차단된다
- build: 초대형 테스트 파일 `src/test_semantic.c`, `src/test_transpile.c`를 include 단위로 분리했고, parser 공용 `ast.c`의 디버그 출력 섹션을 `src/parser/ast_print.c`로 분리해 3000+ 라인 파일을 줄였다
- build: `src/codegen/llvm_domain.c` 상단 helper 블록도 `llvm_domain_helpers.inc`로 분리해 core codegen 파일 길이를 줄였다
- Fix C/LLVM campaign graph/FSM parity by making `ToString(Int)` allocate a
  fresh string instead of reusing a single static buffer. Nested report/log
  concatenation now produces stable values on the C backend, and
  `examples/campaign_graph_fsm/` is promoted to an exact-golden multi-file
  scenario with matching stdout/results on both backends.
