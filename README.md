<p align="center">
  <img src="assets/branding/PergyraLangLogo_256.png" alt="Gyri — Pergyra Mascot" width="180" />
</p>

<h1 align="center">Pergyra</h1>

<p align="center">
  <em>A domain modeling language with subject-first design and compile-time verified contracts.</em>
</p>

<p align="center">
  <a href="docs/README_ko.md">한국어</a> ·
  <a href="docs/grammar/01_syntax.md">Syntax Reference</a> ·
  <a href="docs/grammar/02_grammar.md">Grammar</a> ·
  <a href="docs/grammar/03_naming.md">Naming Convention</a>
</p>

---

## What is Pergyra?

Pergyra is a compiled language with C and LLVM backends. It distinguishes **who acts**, **where they act**, and **what qualifies them** at the language level.

```
.pgy → Lexer → Parser → Semantic → HIR → LLVM Backend → Binary
                                      \→ C Backend    → GCC → Binary
```

## Quick Start

```bash
# Build
make all

# Run
./bin/pgy examples/hello.pgy --run -v

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

Pergyra uses 6 keywords to declare types. Each keyword carries distinct semantics.

| Keyword | Role | Memory | Behavior |
|---------|------|--------|----------|
| `subject` | Active entity (protagonist) | Reference | action + func |
| `class` | Passive thing (tool) | Value | func only |
| `struct` | Pure data | Value | None |
| `vessel` | Internal state (inside subject) | Value | None |
| `object` | Read-only view | Value | func only |
| `tobject` | Transfer object (DTO) | Value | None |

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
```

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

### Zone + Action

```pergyra
zone BattleZone
{
    subject slot attacker: Player;
    subject slot defender: Player;

    action Attack(self, target: Player)
        requires Combatable
        within BattleZone
    {
        target.hp = target.hp - 10;
    }
}
```

### World

```pergyra
world GameServer
{
    zone battle: BattleZone;
}
```

### Relation + Effect

```pergyra
relation Alliance
{
    let trust: Int;
}

effect Poison
{
    let damage_per_tick: Int;
    let remaining: Int;
}
```

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

## Built-in Collections

Available without import:

```pergyra
let map := MapNew();         // HashMap<Int>
MapSet(map, "key", 42);
let val := MapGet(map, "key");

let list := ListNew();       // List<Int>
ListPush(list, 10);
let item := ListGet(list, 0);

let set := SetNew();         // Set<String>
SetAdd(set, "hello");

let queue := QueueNew();     // Queue<Int>
QueuePush(queue, 1);
let front := QueuePop(queue);
```

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
| Types / Built-in functions | PascalCase | `Player`, `PrintInt`, `MapNew` |
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
