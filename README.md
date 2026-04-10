<p align="center">
  <img src="assets/branding/PergyraLangLogo_256.png" alt="Gyri — Pergyra Mascot" width="180" />
  <br/>
  <sub>Meet <strong>Gyri</strong>, the Nautilus.</sub>
</p>

<h1 align="center">Pergyra</h1>

<p align="center">
  <em>A domain modeling language with subject-first design and compile-time verified contracts.</em>
</p>

<p align="center">
  <a href="docs/README_ko.md">한국어</a> ·
  <a href="docs/grammar/01_syntax.md">Syntax Reference</a> ·
  <a href="docs/grammar/02_grammar.md">Grammar</a> ·
  <a href="docs/grammar/03_naming.md">Naming Convention</a> ·
  <a href="docs/INDEX.md">All Documentation</a>
</p>

> **Current Status**: Executable experimental alpha. Core compiler pipeline works (`HIR -> DIR -> RIR -> MIR`) with C and LLVM backends, and the repository currently carries 1200+ direct regression checks across semantic/transpile/security/ABI/smoke suites.
> Standard library, tooling, and ecosystem are still incomplete, but LSP/debugger/formatter are no longer stubs.
> **Quantum surface exists in v1 as a partial runtime/semantic skeleton** (`QubitSlot`, `ClaimQubit`, `Measure`, `Entangle`), while the full quantum resource model remains a v2 target.

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
| `vessel` | Internal state (inside subject) | Value | None | ⚠️ Parser only |
| `object` | Internal projection/view contract | Value | func only | ⚠️ Shares struct-style syntax today |
| `tobject` | Transfer/export boundary contract | Value | func only | ⚠️ Shares struct-style syntax today |

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
- [Design Vision](docs/00_vision.md)
- [한국어 README](docs/README_ko.md)

## License

BSD 3-Clause License. See [LICENSE](LICENSE).
