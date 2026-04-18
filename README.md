<p align="center">
  <img src="assets/branding/PergyraLangLogo_256.png" alt="Gyri — Pergyra Mascot" width="180" />
  <br/>
  <sub>Meet <strong>Gyri</strong>, the Nautilus.</sub>
</p>

<h1 align="center">Pergyra</h1>

<p align="center">
  <em>An intent-first language that closes complex behavior into executable units and derives the rest of the structure from purpose.</em>
</p>

<p align="center">
  <a href="docs/README_ko.md">한국어</a> ·
  <a href="docs/01_intent_first_design.md">Intent-First Design</a> ·
  <a href="docs/grammar/01_syntax.md">Syntax Reference</a> ·
  <a href="docs/grammar/02_grammar.md">Grammar</a> ·
  <a href="docs/grammar/03_naming.md">Naming Convention</a> ·
  <a href="docs/INDEX.md">All Documentation</a>
</p>

> **Current Status**: Executable experimental alpha, currently in a **late-stage alpha / beta-closure sprint**. The remaining beta work is not about widening the language surface; it is about freezing a narrower stable subset and aligning `syntax -> semantic -> runtime -> C -> LLVM -> diagnostics -> regression -> docs` on that subset.
> **Stable subset being frozen for beta**: generics (`exact/ability/multi-bound` plus implemented default type argument actual resolution), `own/ref` anchored slot-handle boundaries, collections (`List<T>`, `Set<T>`, `HashMap<String, T>`, `HashMap<Int, T>`), and runtime observability (`last / history / active / recent`).
> **Explicit reject / beta-out-of-scope**: general ownership on non-anchored value types, unsupported map key kinds, broader generic generalization, richer multi-instance observability queries, and the full quantum resource model. `QubitSlot` / `ClaimQubit` / `Measure` / `Entangle` remain a partial `v2 / experimental` surface.

---

## What is Pergyra?

Pergyra is a compiled language with C and LLVM backends. It distinguishes **who acts**, **where they act**, and **what qualifies them** at the language level.

```
.pgy → Lexer → Parser → Semantic Typed AST → HIR → DIR → RIR → MIR
                                                                 ├→ LLVM Backend → Binary
                                                                 └→ C Backend    → GCC → Binary
```

- `HIR` normalizes language structure and pass-friendly program shape
- `DIR` locks domain contracts such as role/ability, zone/world, intent-step relations
- `RIR` locks slot/resource/projection/authority/lifecycle semantics
- `MIR` locks CFG/SSA/cleanup/resource-flow before backend emission
- Backends consume `MIR`, not `RIR`

## Quick Start

```bash
# Build
make all

# Run
./bin/pgy examples/hello.pgy --run -v

# Inspect IR layers
./bin/pgy examples/hello.pgy --hir
./bin/pgy examples/hello.pgy --dir
./bin/pgy examples/hello.pgy --rir
./bin/pgy examples/hello.pgy --mir

# LLVM backend (optional)
make LLVM_ENABLED=1 all
./bin/pgy examples/hello.pgy --emit-llvm -o hello.ll
```

Requires GCC (C11) and GNU Make. LLVM 14+ optional.

Recent regression entrypoints:

```bash
make test-transpile
make test-abi
make llvm-test-backend-compare
make example-test-smoke
make ir-pipeline-test-smoke
make fmt-test-smoke
```

Current CI support matrix:

- Linux: C backend + LLVM backend regression coverage
- Windows: C backend regression coverage always; LLVM smoke + backend compare run when the Windows LLVM toolchain is present (`make ci-windows` now capability-detects LLVM instead of forcing `LLVM_ENABLED=0`)

Failure policy snapshot:

- recoverable failure: intent/authority/boundary/timeout/remote failures should surface as `Bool`, `Result<T>`, or queryable runtime state
- contract violation: released slot, invalid token, ownership-boundary misuse, and similar invariant breaks remain hard-fail territory
- internal compiler/runtime bug: immediate internal error or panic, never presented as a normal user-code failure

Recent backend hygiene snapshot:

- LLVM declaration-side helper cleanup now shares implicit-self detection, host decl/method lookup, and pointer-self classification instead of repeating the same logic across declaration/intent/MIR-expression paths
- negative-path memory tests now suppress expected panic/tracing stderr so CI logs reflect regressions instead of deliberate failure probes

Stable example guidance:

- smoke-covered examples: see [docs/65_stable_example_surface_board.md](docs/65_stable_example_surface_board.md)
- current stable entry examples include:
  - `examples/logistics_intent_probe/`
  - `examples/resource_scheduler_async_probe/`
  - `examples/order_analytics/`
  - `examples/battle_simulator/`
  - `examples/biome_simulator/`
- contract compression canonical pairs:
  - `examples/intent_contract_pair_minimal.pgy`
  - `examples/authority_contract_pair_minimal.pgy`
  - `examples/transfer_contract_pair_minimal.pgy`
- design-sketch examples such as `examples/party_system_demo.pgy` and
  `examples/world_roster_city.pgy` are not stable syntax references

Authoring-surface references:

- compression overview: [docs/61_surface_compression_examples.md](docs/61_surface_compression_examples.md)
- pain-point board: [docs/58_keyword_authorship_pain_points.md](docs/58_keyword_authorship_pain_points.md)
- compression plan: [docs/59_authoring_surface_compression_plan.md](docs/59_authoring_surface_compression_plan.md)

## Basics

### Variables

```pergyra
let x: Int = 10;
let name: String = "Alice";

// := shorthand (type inferred)
count := 0;
msg := "hello";
```

### Functions

```pergyra
func Add(a: Int, b: Int) -> Int
{
    return a + b;
}

func main()
{
    let result := Add(3, 4);
    PrintInt(result);
}
```

### Control Flow

```pergyra
if hp > 0
{
    Print("alive");
}
else
{
    Print("dead");
}

while i < 10
{
    PrintInt(i);
    i = i + 1;
}
```

## Declaration Keywords

Pergyra uses 6 keywords to declare types. Each keyword carries **intended** distinct semantics.

| Keyword | Role | Memory | Behavior | Implementation |
|---------|------|--------|----------|----------------|
| `subject` | Active entity (protagonist) | Reference (ptr self) | action + func | ✅ Full |
| `class` | Passive thing (tool) | Value | func only | ✅ Full |
| `struct` | Pure data | Value | None | ✅ Full |
| `vessel` | Internal state (inside subject) | Value | None | ✅ Semantic + codegen surface |
| `object` | Internal projection/view contract | Value | func only | ✅ Distinct projection contract |
| `tobject` | Transfer/export boundary contract | Value | func only | ✅ Distinct transfer contract |

> **현재 구현 상태**: `subject`와 `class`는 시맨틱/코드젠 수준에서 분리되어 있다. `object`와 `tobject`는 선언 키워드와 계약이 모두 distinct하며, `object`는 local/internal projection contract, `tobject`는 publish/transfer/export boundary contract로 고정되어 있다.

```pergyra
subject Player
{
    let name: String;
    let hp: Int;
    vessel stats: PlayerStats;
}

class Weapon
{
    let name: String;
    let damage: Int;

    func DamageText(self) -> String
    {
        return self.name;
    }
}

struct Position
{
    let x: Int;
    let y: Int;
}

vessel PlayerStats
{
    let level: Int;
    let xp: Int;
}

object PlayerView
{
    let name: String;
    let hp: Int;
}

tobject PlayerReceipt
{
    let name: String;
    let hp: Int;
}
```

Rule of thumb:
- use `object` for internal projection/read models
- use `tobject` for publish/transfer/export boundaries
- observing an embedded zone projection from `world` still counts as `object`-style projection observation, not automatic boundary publication

## Domain Modeling

### Ability + Role

```pergyra
ability Combatable
{
    func CanFight(self) -> Bool;
}

role Combatable for Player
{
    func CanFight(self) -> Bool
    {
        return self.hp > 0;
    }
}
```

### Zone + Subject Action

```pergyra
subject Player
{
    let hp: Int;

    action Attack(self, target: Player)
        within BattleZone
        authorized by self
    {
        target.hp = target.hp - 10;
    }
}

zone BattleZone
{
    subject slot attacker: Player;
    subject slot defender: Player;
    authority attacker;
}
```

> `action`은 `subject` 내부에 선언합니다. zone은 subject slot과 authority만 정의합니다.

### World

```pergyra
world GameServer
{
    zone battle: BattleZone;
}
```

### Relation + Effect

```pergyra
relation Alliance between subject, subject
{
    let trust: Int;
}

effect Poison for bearer: subject
{
    let damage_per_tick: Int;
    let remaining: Int;
}
```

`relation` is not a plain struct. It declares shared state between endpoints, and the compiler can validate that zone links satisfy the declared endpoint contract.

## Intent

Intents declare **why** a subject moves. They are callable, drive subjects through zone-bound steps, and provide built-in observability.

```pergyra
intent DriveCar(cockpit: CockpitZone, driver: Driver)
{
    exclusive;
    priority: 3;

    step Ignite
    {
        where: CockpitZone;
        using: cockpit;
        who: driver;
        on: driver.Ignite();
        compensate: driver.RollbackIgnite();
        pre: true;
        post: driver.started;
    }

    success: true;
    failure: false;
}

func Main() -> Void
{
    let d = Driver(0, false);
    let cockpit = CockpitZone(Driver(99, true));
    let ok = DriveCar(cockpit, d);    // call intent like a function
    Log("ok=" + ToString(ok));
    Log("trace:");
    Log(IntentLastTrace());           // built-in observability
}
```

Features:
- `exclusive` / `concurrent` — conflict policy
- `priority` — numeric priority for scheduling
- `on:` — event trigger (action call)
- `compensate:` — rollback action on failure
- `pre` / `post` / `guard` / `invariant` / `expect` — conditions
- `IntentLastTrace()`, `IntentHistoryCount()` — runtime observability
- `intent:` inside a step — sub-intent orchestration

Example:

```pergyra
intent Charge(checkout: CheckoutZone, buyer: Buyer)
{
    step verify
    {
        where: CheckoutZone;
        using: checkout;
        who: buyer;
        expect: true;
    }
}

intent Checkout(checkout: CheckoutZone, buyer: Buyer)
{
    step pay
    {
        intent: Charge(checkout, buyer);
        expect: true;
    }
}
```

## Built-in Collections

Available without import:

```pergyra
let map: Map<String, Int> = Map();
MapSet(map, "key", 42);
let val: Int = MapGet(map, "key");

let list: List<Int> = List();
ListPush(list, 10);
let item: Int = ListGet(list, 0);

let queue: Queue<Int> = Queue();
QueuePush(queue, 1);
let front: Int = QueuePop(queue);
```

> `Set<T>`은 현재 `SetNew`, `SetAdd`, `SetHas`, `SetRemove`, `SetSize` 표면이 semantic/C/LLVM 경로에 연결되어 있다. `Array<T>`는 리터럴 `[1, 2, 3]`과 `ArrayPush`/`ArrayPop`/`ArrayLength` 함수로 사용한다.

## Standard Library (`use`)

```pergyra
use fsm;
use timer;
use cooldown;

let state := FsmNew();
FsmAddState(state, 0, "idle");
FsmAddState(state, 1, "attack");
FsmTransition(state, 0, 1);

let t := TimerNew(100);
TimerTick(t, 16);

let cd := CooldownNew(60);
CooldownTrigger(cd);
```

## Naming Convention

| Target | Style | Example |
|--------|-------|---------|
| Keywords | lowercase | `let`, `func`, `subject` |
| Types / Built-in functions | PascalCase | `Player`, `ToString`, `MapSet` |
| Local variables | camelCase | `playerHp`, `eventCount` |
| Constants | UPPER_SNAKE | `MAX_HP`, `TILE_SIZE` |
| File names | snake_case | `battle_zone.pgy` |

Full details: [Naming Convention](docs/grammar/03_naming.md)

## Project Structure

```
PergyraLang/
  src/
    lexer/          # Tokenizer
    parser/         # AST generation
    semantic/       # Type checking, slot analysis
    codegen/        # C / LLVM backends
    compiler/       # Compilation driver
    runtime/        # Runtime library
  examples/         # .pgy examples
  docs/             # Design documents
  editor/           # VS Code extension
  assets/branding/  # Gyri the Nautilus
```

## Example Trust Levels

Not every example has the same contract strength.

- `compile-smoke covered`: the example is exercised by current regression smoke and is the recommended reference surface
- `design sketch`: the example may intentionally show future or aspirational syntax and should not be used as a stable reference

Source of truth:

- [Stable Example Surface Board](docs/65_stable_example_surface_board.md)

Recommended stable examples to start from:

- `examples/logistics_intent_probe/` — IR/domain pipeline probe
- `examples/resource_scheduler_async_probe/` — async/parallel/resource probe
- `examples/order_analytics/` — larger compile-smoke covered application example
- `examples/subject_object_tobject/` — nominal/projection baseline
- `examples/ownership_forwarding_probe/` — current `own/ref` anchored-slot boundary subset

Design-sketch examples:

- `examples/party_system_demo.pgy`
- `examples/world_roster_city.pgy`

## Testing

```bash
make test-all                  # All tests
make test-semantic             # Semantic analysis
make test-transpile            # C backend
make llvm-test-backend-compare # C/LLVM parity
```

## Documentation

- [Syntax Reference](docs/grammar/01_syntax.md)
- [Grammar Definition](docs/grammar/02_grammar.md)
- [Naming Convention](docs/grammar/03_naming.md)
- [Compiler Pipeline](docs/20_compiler_pipeline_guide.md)
- [Language Status](docs/18_language_status.md)
- [Stable Example Surface Board](docs/65_stable_example_surface_board.md)
- [Design Vision](docs/00_vision.md)
- [한국어 README](docs/README_ko.md)

## License

BSD 3-Clause License. See [LICENSE](LICENSE).
