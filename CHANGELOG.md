# Changelog

All notable changes to the Pergyra Language will be documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

- compiler/rir: refined conservative flow merge semantics with derived
  `authority-loss` and `projection-invalidation` flags. `--rir` now preserves
  these semantics at both scope and `flow-block` level, and regression tests
  lock the new authority/projection merge behavior.
- compiler/mir: promoted liveness and DCE from analysis-only scaffolding to
  real lowering passes. MIR lowering now recomputes liveness, removes dead
  `def` / dead `phi` instructions conservatively, records per-routine
  `dce_removed_count`, and requires DCE state in MIR validation.
- Fixed the `relation ... between ...` parser surface so `subject`, `class`,
  and `subject[]`/`class[]` endpoints are consumed through the normal
  name-token path instead of incorrectly requiring raw `TOKEN_IDENTIFIER`.
  Shared-domain semantic regression tests for `between` syntax are green
  again.
- Extended `RIR` conservative flow semantics with `invalidation`. Scope-level
  and block-level flow dumps now preserve detach/release/abort-style invalidation
  signals alongside `authority`, `projection`, and `world-handoff`.
- Extended `MIR` with routine-level value/use-def summaries. Each routine now
  materializes per-value `def_block`, `def_inst`, `use_count`,
  `live_in/out block count`, and `reaches_cleanup`, and MIR validation now
  requires this summary for non-empty routines.
- Added `examples/resource_scheduler_async_probe/` as a strict async/parallel
  resource-discipline probe. It combines `Channel<Int>`, `parallel`,
  `Slot<subject>`, helper-based `ref Slot<subject>` mutation,
  `DeviceSlot<Int>`, and `RemoteFuture<Int>` in one example, with exact
  stdout/results goldens and smoke coverage.
- Fixed async parser handling so `async func` parameters now accept `ref` /
  `own` modifiers the same way as regular function declarations.
- Deepened the parallel slot analyzer so helper calls with `ref Slot<subject>`
  or `own Slot<subject>` parameters are tracked conservatively. Patterns such
  as two parallel tasks mutating the same slot through a helper call are now
  rejected with a real parallel slot conflict instead of slipping through.
- Added `examples/logistics_intent_probe/` as a four-layer compiler probe that
  deliberately crosses `DIR`, `RIR`, `MIR`, and runtime intent execution in one
  small scenario. It includes exact stdout/results goldens and a dedicated
  `tests/ir_pipeline_probe.sh` dump smoke for `--dir`, `--rir`, and `--mir`.
- Fixed intent lowering so failed intents now return `false` instead of
  returning the value of the `failure:` predicate. This bug surfaced while
  building the new logistics probe: a failed step could previously produce
  `ok=true` if the failure predicate evaluated to `true`.
- Fixed C intent lowering so `__intent_failed` is now set on all failure paths,
  not only when the intent had compensation steps.
- Deepened `RIR` handle semantics so nominal `relation/effect/zone/world`
  types are now explicit resource facts in function params and intent
  participants, and `using:` / `transfer:` lower to conservative handle ops
  (`Read`, `Move`, `Claim`) instead of staying purely declarative metadata.
- Deepened `MIR` SSA and exceptional CFG again. Blocks now retain explicit
  `ssa_entry_versions` / `ssa_exit_versions`, phi incoming values read
  predecessor exit maps directly, and intent exceptional flow now converges
  through a real cleanup root before splitting into rollback and invalidation
  blocks.
- Deepened `RIR` branch/join handling into an actual lattice-propagation layer.
  Scope dumps now include `flow-block[...]` facts with `entry_state`,
  `exit_state`, join markers, loop widening markers, and separate entry/exit
  conflict flags instead of only flat normalized summaries.
- Deepened `MIR` from block-local rename into an instruction-oriented SSA/use
  skeleton. MIR now materializes `def` instructions, block entry/exit SSA
  versions, versioned phi inputs, and versioned uses on branch/return/resource
  instructions.
- Expanded MIR exceptional control flow beyond a single cleanup edge. Intent
  routines now expose rollback and invalidation cleanup structure separately,
  so `rollback` policy and detach/invalidation markers live in richer
  exceptional CFG blocks instead of a single undifferentiated cleanup block.
- Added `MIR` as a real code layer with [`src/compiler/mir.h`](/mnt/e/PergyraLang/src/compiler/mir.h),
  [`src/compiler/mir.c`](/mnt/e/PergyraLang/src/compiler/mir.c), `pgy --mir`,
  and [`src/test_mir.c`](/mnt/e/PergyraLang/src/test_mir.c). The current MIR
  is a routine/block/instruction skeleton that bridges HIR CFG blocks and RIR
  ops, including intent cleanup blocks for `CompensateIntentStep` and
  `AbortIntent`.
- Deepened `MIR` so it is no longer a pure block shell. `phi` nodes are now
  materialized with incoming predecessor values, block-local defs get
  versioned SSA-style names, branch/return terminators are recorded as MIR
  instructions, and intent routines now expose explicit cleanup successor edges
  in addition to the cleanup block itself.
- Extended `RIR` from a pure op/fact dump to a normalized resource summary
  layer. Each scope now materializes tracked resource/projection state with
  `initial_state`, `final_state`, `last_op`, and transition-error flags, and
  `pgy --rir` shows those summaries directly.
- Extended `DIR` from a coarse declaration graph to a richer intent graph.
  It now records participant-to-type edges, step-to-zone/ability/authority/
  effect edges, and explicit predecessor dependencies between ordered intent
  steps.
- Extended `DIR` role analysis so ability completeness is now explicit in the
  graph. Complete role impls emit a `role-complete` edge and incomplete impls
  emit `role-missing-method` edges that point at the missing ability method.
- Extended `RIR` zone/world collection so layer slots and world zone handles
  are now materialized as resource facts, and lifecycle ops such as
  `AttachEffect` / `DetachEffect` / `LinkRelation` / `UnlinkRelation` update
  normalized state summaries instead of remaining dump-only metadata.
- Updated pipeline docs and TODOs so the repository now describes the actual
  code-layer status as `HIR + DIR + RIR + MIR started`, instead of treating
  MIR as a documentation-only future placeholder.

- Fixed nested generic type-argument parsing so practical types such as
  `HashMap<String, List<String>>` now parse as real type arguments instead of
  falling through the declaration-only generic parser path.
- Fixed C backend lowering for function-typed locals and function-returning
  functions. Local policy values such as
  `let f: func(Int) -> Int = AddOne;` and returned callables now emit proper C
  function-pointer declarators instead of degrading to `void *` / scalar locals.
- Fixed the remaining C forward-declaration edge for nested specialized
  collections in function signatures. Types such as
  `HashMap<String, List<String>>` and `HashMap<String, Player>` now emit their
  collection specializations before forward prototypes, so the generated C
  compiles without the old unknown-type/conflicting-prototype failure.
- Fixed lexer string scanning for escaped quotes and backslashes. JSON-like
  payloads such as `"{\"ok\":true}"` now parse correctly, which unblocks
  practical `http`/adapter examples and request-body literals.
- Fixed LLVM parameter metadata registration for collection-typed and
  function-typed parameters across free functions and hosted methods. Practical
  examples now keep `List<T>`, `HashMap<String, V>`, and callable parameter
  semantics when accessed from parameters, not only from local `let` bindings.
- Added `examples/adapter_policy_stack/` as a practical adapter-heavy scenario
  demonstrating function-valued policy injection, nested collection composition,
  and `page / api / report` library layering on both C and LLVM.

- Added compiler-known stdlib module resolution for `use <module>;`.
  The import resolver now locates `stdlib/<module>.pgy` and merges it into
  the AST, so `use datetime;` is an actual language surface instead of a
  documentation-only placeholder.
- Added `stdlib/datetime.pgy` with `LocalDate`, `LocalTime`, `DateTime`,
  `FormatDate`, `FormatTime`, `FormatDateTime`, and `SameDate`, and migrated
  `examples/calendar_working/` to that standard module.
- Tightened LLVM collection parity further: `List<T>` iteration and
  `HashMap<String, Int/Class/Subject>` access now work in practical examples,
  substantially shrinking the earlier C-only collection gap.
- Split `examples/shopping_mall_checkout_refund/` into explicit `api/` and
  `report/` adapter layers so the scenario now demonstrates
  `request dto -> adapter handler -> intent -> zone/world -> response/report dto`
  instead of routing all adapter logic through `world.pgy`.
- Fixed LLVM standalone helper lowering for pointer-self nominal parameters.
  Top-level functions that take `subject` / `zone` / `world`-style hosts now
  use pointer semantics in both signature emission and call lowering, which
  restores parity for adapter-style flows such as
  `examples/shopping_mall_checkout_refund/` and the new
  `zone_param_mutation` backend-compare case.
- Added `examples/calendar_working/` to example smoke so `use datetime;`,
  `List<T>` iteration, and `${expr}` string interpolation are exercised on
  both C and LLVM in CI.

- Added `using: zoneAlias;` to `intent step` and lowered it on both C and LLVM
  backends as a live concrete zone-instance binding path. Intent steps now
  re-sync the bound zone instance while they execute instead of treating
  `where: ZoneType` as static metadata only.
- Added backend parity coverage for `intent_zone_binding`, proving `using:`
  keeps subject state, zone state, and bound object projection in sync on both
  backends.
- Changed `pgy scaffold project` to emit an intent-first project shape:
  `intents/`, `subjects/`, `zones/`, `world.pgy`, and `main.pgy` now form the
  starter layout instead of a single flat `domain.pgy`.
- Restructured `examples/dnd_tavern_campaign/` into intent-first folders
  (`intents/`, `subjects/`, `zones/`) and updated the campaign intents to bind
  the live `JourneyZone` instance through `using: journey;`.
- Added `compensate:` to `intent step`, with reverse-order rollback on both C
  and LLVM backends when a step fails after side effects have already run.
- Added minimal intent observability built-ins: `IntentLastTrace()` and
  `IntentLastFailure()` now expose the most recent runtime trace/failure
  summary from the active intent registry.
- Added backend parity coverage for `intent_trace_compensate`, proving
  compensation, last-trace history, and failure strings stay equal on C and
  LLVM.
- Added `rollback: full | current | none` to `intent` declarations. The
  default remains `full`, `current` compensates only the latest completed
  step, and `none` skips compensation entirely on both C and LLVM backends.
- Added active intent registry built-ins:
  `IntentActiveCount()` / `IntentActiveName(i)` /
  `IntentActiveHandle(i)` / `IntentActivePriority(i)` /
  `IntentActiveConcurrent(i)` / `IntentActiveTrace(i)`.
- Added backend parity coverage for `intent_observability_rollback`, proving
  multi-instance observability and rollback policy stay equal on C and LLVM.
- Added first-class `guard` and `invariant` clauses to `intent step`. The
  parser, semantic pass, C backend, LLVM backend, backend parity tests, and
  DND campaign now all exercise them.
- Added first-class `intent` declaration parsing/AST/semantic support. The
  compiler now understands `intent`, `involves`, `step`, `where`, `who`,
  `requires`, `authorized by`, `causes`, `expect`, `success`, and `failure`
  as a static orchestration contract surface.
- Upgraded `intent` from declaration-only to executable lowering: `intent Name(args...)`
  is now callable on both C and LLVM backends, with `exclusive`/`concurrent`,
  `priority`, `pre`, `post`, `expect`, and repeated `on:` clauses preserved in
  generated runtime functions.
- Added HIR retention for top-level `intent` declarations instead of skipping
  them after semantic analysis.
- Added a first runtime conflict scheduler for `intent` execution on both C
  and LLVM backends. Generated intent functions now register active instances,
  reject conflicting same-subject `exclusive` entries, allow
  `concurrent`/`concurrent` coexistence, and allow higher-priority nested
  intents to override lower-priority active intents.
- Updated `examples/dnd_tavern_campaign/` so campaign phases now invoke
  `TavernRecruitment(...)`, `DelveThreeFloors(...)`, and `SlayDragon(...)`
  directly, proving that intent moves subjects in the live scenario.
- Added parser and semantic regression coverage for `intent` declarations,
  including subject-binding and unknown-actor failure cases.
- Added stronger `intent step` semantic validation so `who` / `authorized by`
  must match subject-slot types in the referenced zone and `requires`
  abilities must be implemented by the declared actor subject type.
- Added backend-compare coverage for programs that carry `intent`
  declarations but still execute normal code paths, proving current
  parser/semantic/HIR-skip behavior is backend-equal.
- Added backend-compare coverage for runtime intent conflicts:
  `intent_conflict_runtime` now proves `exclusive` blocking,
  `concurrent` coexistence, and higher-priority nested override stay
  backend-equal.
- Added `examples/dnd_tavern_campaign/intents.pgy` so the tavern campaign now
  includes first-class static intent contracts for recruitment, floor delving,
  and dragon-slaying on top of the existing runtime world state machine.
- Clarified docs so `intent` is now described as a language declaration with a
  still-pending runtime engine, rather than a plain library/module concept.
- Added `effect pool <name>: <EffectType> capacity <N>` zone syntax with parser/semantic/C transpile support for fixed-capacity effect instancing.
- Added LLVM runtime/codegen lowering for `zone effect pool` storage so pooled
  effect layers are now emitted as concrete `{items, active, count, cap}`
  structs instead of remaining semantic-only on the LLVM path.
- Added C backend `HasLayer(...)` helper lowering with automatic zone rdlock wrapping and generation stale-warning checks.
- Added multithreaded zone layer stress coverage to `test_concurrency`.

### Added
- Ontology-first scaffold guidance in `pgy scaffold` / `pgy new`, including a
  new `class` scaffold kind and starter project/simulator templates that begin
  from the `subject/class/object/dto` split instead of only dumping a raw world
  shell
- `examples/dnd_tavern_campaign/story_cards.pgy` for scene-choice cards,
  companion reaction cards, and boss phase cards that drive denser transcript
  narration
- Scripted, seeded-random, and player-input entry modes for
  `examples/dnd_tavern_campaign/`, including `random_main.pgy` and
  `player_main.pgy` plus transcript-bearing `results.random.txt` and
  `results.player.txt`
- `OpenTavernCampaign()` factory entry for `examples/dnd_tavern_campaign/` so
  the scenario now opens through a place/campaign builder instead of spelling a
  long `TavernCampaignWorld(...)` constructor inline in `main.pgy`
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
- LLVM nominal lookup is now scope-aware for class dispatch. In larger
  scenarios, stale local class tracking no longer shadows current host fields,
  so subject-owned class tools such as `Adventurer.weapon: WeaponCard` can call
  `weapon.Summary()` and similar hosted funcs consistently on both backends.
- LLVM nominal inference now tracks unqualified host methods that return class
  values, closing backend mismatches on flows such as
  `let weapon = MemberWeapon(); weapon.Strain(...)`.
- `examples/dnd_tavern_campaign/` scene/event pacing is now more data-driven:
  GM prompts, stakes, rewards, companion reactions, and boss phase intent are
  emitted from story-card factories instead of only the main world loop
- `examples/dnd_tavern_campaign/` combat flow now uses game-layer weapon
  factories and strategy cards instead of only world-hosted fixed action text,
  so seat-by-seat combat narration carries class-shaped loadout and posture
  data through the state-machine transcript
- `examples/dnd_tavern_campaign/` now emits a denser GM-style transcript with
  explicit state-entry/state-exit markers, six scene beats per phase, five
  node/event beats per floor, and more detailed skirmish/boss round logging
- DND transcript helpers now key class-sensitive flavor text off stable class
  codes instead of comparing strings to literals in generated C, eliminating
  backend warnings from the richer story/combat logging path
- Runtime hashmap helpers now use `pgy_runtime_strdup(...)` instead of raw
  `strdup(...)`, removing portability warnings from larger C example builds
- C domain/world constructor lowering now preserves omitted `shared` default
  initializers in generated compound literals, fixing `fsm_factory` parity for
  `gridNoise` and related world-shared defaults
- `examples/fsm_factory/` no longer needs the old `ResetFactory()` seed step;
  constructor/default-state behavior now matches well enough that the scenario
  can construct `FactoryWorld(alpha, beta)` and simulate immediately on both
  backends
- `examples/dnd_tavern_campaign/` now runs as an explicit world-owned game
  state machine with numbered tavern/floor/dragon choices, deterministic
  rolled branching, and a full story transcript in `results.txt`
- Slot built-ins and slot member-call sugar now agree on both backends for
  non-primitive payloads such as `Slot<Vec2>`, so `Read(slot)` and
  `slot.Read()` share the same type inference and runtime behavior
- Example smoke exact stdout comparison now trims leading blank lines after
  normalization so transcript-style scenarios do not fail on harmless banner
  spacing differences
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
- ci/examples: make `tests/example_contract_smoke.sh` exact-golden comparison resilient when external `diff` is unavailable by falling back through `cmp`, `git diff --no-index`, or Python unified diff output; update `examples/biome_simulator/expected_results.txt` to the current stable report so generic expected data no longer lags behind the backend-specific goldens.
- codegen/runtime: fix `String == ...` / `String != ...` lowering so C and LLVM both route through `pgy_string_equals(...)` instead of pointer comparison, add runtime support plus a transpile regression, and unblock packet/sink filtering in the upgraded observer pattern example without leaking compiler warnings into exact stdout.
- examples/docs: deepen `examples/pattern_library_basics/` observer translation from hardcoded `PublishCombat/PublishUi/PublishAudit` sink helpers into `RelaySinkSpec` + `RelayPacket` + `RelayBundle` + `DispatchPacket(...)`, update the exact golden output, and align pattern-library docs so `observer` now consistently means explicit relay bundles and sink specs instead of a handful of ad hoc sink functions.
- examples/docs: deepen `examples/pattern_library_basics/` state translation from a raw `TransitionState(current, event)` helper into `TransitionRuleFactory(...)` + `TransitionContext` + `ApplyTransitionRule(...)`, update the exact golden output, and align pattern-library docs so `state` now consistently means explicit transition rule tables with context application rather than a single opaque transition function.
- transpiler/examples: fix C backend local type inference for string-concat initializers so `let title = a + \"...\" + b` emits `char*` instead of falling back to `int32_t`, add a regression in `test-transpile`, and unblock staged factory/spec-builder examples that finalize titles through local concat.
- examples/docs: deepen `examples/pattern_library_basics/` factory translation from a single `UnitFactory(...)` helper into a staged `RoleTemplateFactory(...)` + `OriginTemplateFactory(...)` + `UnitDraft` + `FinalizeUnitSpec(...)` spec-builder path, update the exact golden output, and align pattern-library docs so `factory` now consistently means staged template/spec composition rather than a one-step constructor wrapper.
- docs/examples: add Pergyra-style GOF pattern catalog and `examples/pattern_library_basics/` covering `singleton`, `factory`, `strategy`, `state`, and `observer` as contextual registry, spec builder, policy card, explicit transition, and relay sink patterns rather than inheritance-heavy OOP clones; document higher-order function injection (`picker` / resolver policy) as the next pattern-library step while keeping the stable example on the current card/resolver path, register the example with exact stdout golden, and document it in `docs/testdoc/pattern_library_basics.md`.
- examples/docs: deepen the stable strategy pattern path in `pattern_library_basics` from plain card lookup into `StrategyContext` / `ApplyStrategy(card, context)`, showing the same resolver reused across different injected domains (`raid`, `dispatch`) while higher-order function injection remains the next-step target.
- parser/examples: avoid `context` as a local variable or parameter name in the upgraded strategy example because it currently behaves like a contextual-reserved identifier; use `ctx` instead and note the sharp edge in the testdoc.
- parser: downgrade `context` from a global lexer keyword so it can be used as an ordinary identifier in locals, parameters, and fields; add parser coverage and update grammar/testdoc notes accordingly.
- parser/semantic/codegen/examples: add `func(...) -> ...` type syntax for callable parameters, preserve typed lambda parameters in the AST, lower callable parameters through C function pointers and LLVM function-pointer calls, and upgrade `pattern_library_basics/strategy.pgy` to real injected score/line policies including lambda-based overrides.
- examples/tests/docs: promote `examples/dnd_tavern_campaign/WeaponCard` into a real `class`, embed it directly into `Adventurer`, route loadout/combat narration through subject-owned class methods, and add semantic/transpile regressions for `subject` owning a `class` value and calling its hosted `func`.
# 2026-04-05

- intent runtime now materializes bound `who` actors into matching live zone
  subject slots when `using:` binds a concrete zone instance, on both C and
  LLVM backends
- intent runtime history now exposes `IntentLastName()`,
  `IntentLastHandle()`, `IntentLastStepCount()`, and `IntentLastFailed()`
  alongside existing trace/failure builtins
- intent runtime now also exposes structured step-level history through
  `IntentHistoryCount()` / `IntentHistoryStepName()` /
  `IntentHistoryStepZone()` / `IntentHistoryStepOk()` /
  `IntentHistoryStepFailure()`
- added `pgy_intent_trace_materialize_export(...)` runtime trace lines so
  intent traces now record actor-to-zone-slot materialization explicitly
- strengthened backend parity coverage for intent trace/rollback/materialize
  via `tests/cases/backend_compare/intent_trace_compensate`
- documented the post-implementation strengths and weaknesses of Pergyra in
  `docs/35_hands_on_language_assessment.md`
- intent: added `transfer: source -> target;` to `intent step`, with semantic validation that both bindings are zone participants, target matches `where`, and `who` actors match subject slots on both sides; C/LLVM lowering now performs live handoff materialization, dual-zone sync, and `[transfer] ...` runtime trace lines, with backend parity coverage in `tests/cases/backend_compare/intent_cross_world_transfer/`
- intent/semantic: suppress the misleading "intent is declarative only" warning
  for steps that already declare explicit `on:` expressions; same-name action
  matching is now only warned about when a step has no executable `on:` path
- examples/docs/tests: add `examples/shopping_mall_checkout_refund/`, an
  intent-first commerce scenario with `intents/`, `subjects/`, `zones/`,
  `pages/`, `world.pgy`, exact stdout/results goldens, and JS/backend flavored
  transcript output demonstrating `page != zone`, checkout/payment/refund
  transfer, profile sync, and runtime `IntentHistory*` inspection on both C
  and LLVM backends
- intent/codegen/runtime/examples: close the remaining `using:` gap by
  rebinding bound `who` actors to the live zone subject slots during each
  step body and restoring them afterward; this lets zone methods mutate deep
  nested actor state directly while keeping intent clauses, trace, and final
  canonical actor state consistent on both C and LLVM, and the shopping-mall
  scenario now demonstrates the stronger lowering with zone-method based
  checkout/refund/account sync steps
# 2026-04-05

- compiler/rir: started the new Resource IR layer with [`src/compiler/rir.h`](/mnt/e/PergyraLang/src/compiler/rir.h) and [`src/compiler/rir.c`](/mnt/e/PergyraLang/src/compiler/rir.c). `pgy --rir` now dumps scope-based resource facts and explicit ops for slot claim/read/write/release, projection refresh/publish, zone lifecycle operations, authority/capability facts, and intent commit/abort/compensate skeletons.
- tests/docs: added [`src/test_rir.c`](/mnt/e/PergyraLang/src/test_rir.c), `make test-rir`, and updated the compiler pipeline docs so `DIR` and `RIR` are both documented as started-but-not-backend-owning layers. The fixed compiler contracts now explicitly drive the implementation instead of living only as design notes.
- docs/architecture: fixed the compiler contract around `HIR -> DIR -> RIR -> MIR` and separated it from the current implementation state. Added [`docs/37_compiler_contracts.md`](/mnt/e/PergyraLang/docs/37_compiler_contracts.md) to pin the IR layer responsibilities, the resource state lattice, the intent compensation model, the projection sync contract, and the authority/capability split as the forward compiler contract.
- docs/dir: strengthened the new DIR start point so `pgy --dir` now prints stable unresolved ids, resolved zone-parameter participants, and `transfer: source -> target` metadata in intent steps instead of ambiguous `-0` output. Added a second DIR regression covering transfer aliases and zone-typed intent participants.

- intent/runtime: added richer trace-id and identity-aware step history. `IntentLastTraceId()` and `IntentActiveTraceId()` now expose per-instance trace ids, and `IntentHistoryStepPhase/Actor/Slot/FromZone/FromSlot/ToZone/ToSlot` now expose typed step identity for materialize/transfer flows on both C and LLVM.
- intent/codegen: cross-world handoff traces are now reflected in typed history instead of only flat trace strings, so transfer/materialize debugging can be done through builtins rather than transcript scraping.
- transpiler/llvm: fixed Bool stringification parity around intent/result reporting and world-zone query reporting. `ToString(Bool)` now prints `true/false` consistently on LLVM, and C type inference now recognizes `HasZoneProjection/HasZoneLayer/HasZoneState` plus intent observability builtins as Bool/Int/String instead of falling back to integer lowering.
- tests: added `tests/cases/backend_compare/intent_rich_history_identity/` and expanded intent parity coverage for rich history / transfer identity, while updating DND and shopping-mall exact goldens to the new Bool text output.
- language: `for-in` now accepts `List<T>` in semantic analysis and C transpilation, so `for event in events` works for generic list-backed flows instead of only `Array<T>` / `Slice<T>`.
- parser: string interpolation `${expr}` now reparses embedded expressions and wraps them in `ToString(...)`, which makes calendar/report-style text with ints and member access type-check again.
- codegen: `ToString(...)` in the C backend now handles `String` passthrough and `Bool` text emission instead of always lowering to `pgy_int_to_string(...)`.
- examples: added [`examples/calendar_working/main.pgy`](/mnt/e/PergyraLang/examples/calendar_working/main.pgy) as a direct proof that Pergyra can now build a basic calendar-style schedule renderer using `List<Event>`, `for-in`, and interpolation.
- docs: updated [`docs/35_hands_on_language_assessment.md`](/mnt/e/PergyraLang/docs/35_hands_on_language_assessment.md) so the old “calendar even this cannot be built” section reflects the current state instead of stale pre-fix claims.
- Lifted `shopping_mall_checkout_refund` onto real `use http; use storage; use page;` surfaces. The example now emits `HttpIntentRequest/HttpIntentResponse`, page route/action binding lines, and transcript persistence through `SaveTranscript(...)` / `RenderStorageWrite(...)` instead of direct ad hoc transport/storage strings.
- Fixed stdlib `use` deduplication in the import resolver so repeated `use http; use storage; use page;` across imported files no longer redeclare the same stdlib types.
- Fixed CI portability in `tests/example_contract_smoke.sh` by normalizing CRLF/LF in exact comparisons and running generated example binaries from a repo-local `.tmp` directory instead of `/tmp`, which avoided `noexec` failures in GitHub Actions. Updated `.github/workflows/ci.yml` to `actions/checkout@v5`.
# 2026-04-05

- HIR를 순수 top-level bucket classifier에서 한 단계 올려 `decl index` / `routine summary` / direct-call snapshot을 가진 indexed program view로 확장했다. `hir_find_decl(...)`, `hir_find_routine(...)`, `HIRPhase`, `HIRRoutine.direct_calls`가 추가되어 향후 최적화 패스와 분석 패스가 AST 전체를 다시 훑지 않고도 top-level/routine 수준 정보를 읽을 수 있다.
- 같은 HIR 확장선에서 `HIRRoutine.signature_type_refs`와 `hir_run_routine_pass(...)`를 추가했다. 이제 패스는 함수/intent의 시그니처 타입 의존성과 control-flow/action-like 플래그를 기준으로 루틴을 필터링해 순회할 수 있다.
- function body에 대해 CFG v0를 추가했다. `HIRRoutine.cfg`는 basic block, branch/goto/return/unreachable terminator, loop-header 표식을 가지며, AST 블록을 그대로 backend에 던지는 대신 루틴 수준 분석/최적화 패스의 시작점을 제공한다.
- HIR CFG를 SSA 준비 단계까지 더 밀었다. basic block predecessor edge를 materialize하고, direct-call name을 실제 routine index로 연결하는 `callee_routine_ids`와 보수적 `is_entry_reachable` 비트를 추가했다. 이제 HIR는 단순 routine summary를 넘어 inter-routine graph와 block predecessor를 함께 제공한다.
- 같은 CFG 정규화 위에 block reachability, reverse-post-order, immediate dominator 계산까지 추가했다. 이제 SSA 재구성이나 dominance 기반 dead-code/inlining 후보 분석이 HIR 위에서 직접 시작될 수 있다.
- dominance 위에 `dominance_frontier`와 natural-loop `loop_depth`도 추가했다. 이제 HIR block은 backedge 기반 loop membership과 phi placement 직전 정보를 직접 들고 있으므로, 다음 단계의 SSA 재구성이나 loop pass가 AST 재탐색 없이 HIR만으로 시작될 수 있다.
- HIR를 SSA 직전 분석 표면까지 더 확장했다. 각 block은 `local_defs`와 `phi_candidates`를 갖고, 각 routine은 `reachable_block_count`, `dead_block_count`, `phi_candidate_count`, `phi_candidate_block_count`를 요약한다. 또한 `pgy --hir-cfg`, `--hir-dom`, `--hir-ssa` dump view를 추가해 CFG/dominance/SSA-prep 상태를 직접 확인할 수 있게 했다.
- HIR SSA-prep를 한 단계 더 밀었다. 각 block은 이제 dominator-tree child와 `phi_nodes` skeleton을 갖고, 각 phi node는 아직 rename 전이지만 incoming predecessor 목록까지 materialize한다. `hir_run_block_pass(...)`도 추가해서 routine-level이 아니라 block-level analysis/cleanup pass를 HIR 위에서 직접 시작할 수 있게 했다.
- compiler: `driver_run_pipeline()` now always lowers and validates `DIR/RIR/MIR` before backend dispatch, and backend runners accept a `CompilerIRBundle` even though current C/LLVM codegen is still HIR-driven internally.
- compiler: added structural validators for `DIR`, `RIR`, and `MIR`, and wired validator coverage into `test_dir`, `test_rir`, and `test_mir`.
- parser: AST pretty-printer now re-escapes string literals so escaped JSON/newline output matches parsed surface syntax during parser/debug tests.
# 2026-04-06

- codegen/compiler/tests: add the first real `MIR -> C backend` vertical slice. The C backend now accepts `transpile_with_mir(...)`, `compiler_emit_c()` passes `bundle->mir`, and a conservative subset of simple top-level branch/return functions is emitted from MIR blocks/terminators instead of the AST/HIR body walker. Added a transpile regression that locks the emitted `goto _pgy_mir_bb_*` body shape and `/* emitted-from-mir */` marker.
- docs: update the compiler pipeline/development status docs so they no longer claim `DIR/RIR/MIR` are analysis-only in every case; they now explicitly note the first MIR-driven codegen slice while preserving the larger point that most backend emission is still HIR-driven.
- tests: tighten `read_file_text()` in `src/test_transpile.c` to check `fread()` results instead of ignoring them, keeping the transpile suite warning-free under the new MIR vertical-slice regression.
# 2026-04-06

- compiler/codegen: expanded the first MIR->C vertical slice beyond simple function CFGs so `intent` cleanup now emits `cleanup/rollback/invalidation` labels from MIR exceptional block ids while preserving existing step semantics
- compiler/codegen: `emit_intent_decl(...)` now routes failure/success cleanup gotos through MIR cleanup blocks when a matching `MIR_SCOPE_INTENT` routine is available
- tests: added MIR-backed transpile regression for intent exceptional CFG emission in [`src/test_transpile.c`](src/test_transpile.c)
- docs: updated IR pipeline and current-state documents to reflect that MIR now directly drives both simple function CFG emission and intent cleanup CFG emission in the C backend
- compiler/layout: split `src/compiler/mir.c` into a smaller core file plus [`src/compiler/mir_base.inc`](src/compiler/mir_base.inc) and [`src/compiler/mir_public.inc`](src/compiler/mir_public.inc), and split public/dump surface from `src/compiler/rir.c` into [`src/compiler/rir_public.inc`](src/compiler/rir_public.inc) to start bringing IR implementation files back under control
- compiler/layout: continue the IR file split by carving [`src/compiler/rir_flow.inc`](src/compiler/rir_flow.inc) and [`src/compiler/rir_builder.inc`](src/compiler/rir_builder.inc) out of `src/compiler/rir.c`, so resource lattice merge/CFG flow enrichment and AST scope collection no longer live in one 1.6k-line implementation file
- compiler/rir: extend `RIR-flow` so branch/join analysis now carries conservative semantic flags for `authority`, `projection`, and `world-handoff` in addition to resource state lattice facts. `scope semantics=` and block-level `sem-entry/sem-exit` are now part of the dump/contract surface, and resource-free CFG blocks can still preserve projection/handoff meaning.
- tests: add a RIR regression that locks projection-flow merge across an `if` join and world-handoff conservative semantics on an intent transfer scope
