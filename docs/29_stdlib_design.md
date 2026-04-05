# Pergyra Standard Library Design (2026-04-05)

## 원칙

**빌트인(런타임 C)** — `.pgy` 임포트가 아닌 런타임 내장.

이유:
- HashMap, List, ReadLine 등은 C 레벨 메모리 관리가 필요
- `.pgy`로 만들면 unsafe 블록에서 포인터를 직접 다뤄야 → 포인터 은닉 철학 위반
- Go 모델: `map`, `slice`, `fmt.Scanln`이 전부 빌트인

**게임 시스템은 코어 문법이 아니라 라이브러리 계층** — entity/object pool,
encounter/turn/state machine, strategy/AI, content tables는 언어 키워드로
올리지 않고 `use` 표면의 std/game 라이브러리로 둔다.

이유:
- 코어 언어는 `subject / vessel / object / dto / relation / effect / zone / world / Slot<T>` 의미론을 작고 강하게 유지해야 함
- 대규모 게임 설계는 프로젝트별 조합 폭이 커서 코어 키워드보다 library/DSL 형태가 더 적합함
- 같은 개념이라도 텍스트 RPG, 전술 RPG, 오픈월드 시뮬레이터가 요구하는 pool/fsm/strategy 모양이 다르므로 코어에 고정하면 오히려 빨리 굳어짐

자세한 철학은 [`30_pattern_library_model.md`](/mnt/e/PergyraLang/docs/30_pattern_library_model.md)를 따른다.

## 3단계 모듈 체계

```pergyra
// 1단계: 빌트인 — import/use 없이 바로 사용
let x = MapNew();
Log("hello");
let name = Input("Name: ");

// 2단계: 표준 라이브러리 — use 키워드
use pool;
use fsm;
let enemies = PoolNew(100);

// 3단계: 유저 모듈 — import 키워드
import "creatures.pgy";
```

| 단계 | 키워드 | 대상 | 경로 지정 |
|------|--------|------|----------|
| 빌트인 | 없음 | 런타임 내장 타입/함수 | 불필요 |
| 표준 라이브러리 | `use` | 컴파일러가 위치를 아는 std 모듈 | 불필요 (이름만) |
| 유저 모듈 | `import` | 프로젝트 로컬 .pgy 파일 | 파일 경로 |

- `use` = "이 프로그램이 이 기능을 사용한다"
- `import` = "이 파일을 이 프로그램에 포함시킨다"
- 현재 구현에서 `use module;`는 compiler-known `stdlib/<module>.pgy`를 AST에 병합한다.

## 네이밍 규칙

- 슬래시/점 없음 — 단순 PascalCase
- `HashMap`, `Set`, `List` 등 직접 타입명으로 사용
- 함수도 `ReadLine()`, `Print()` 등 단순 호출
- 표준 라이브러리 이름은 소문자 단어: `pool`, `fsm`, `timer`, `ecs`

## 라이브러리 목록

### Tier 1 — 게임/앱 최소 요건

| 이름 | 타입 | 핵심 API | 상태 |
|------|------|----------|------|
| **HashMap** | `HashMap<K, V>` | `MapNew`, `MapSet`, `MapGet`, `MapHas`, `MapRemove`, `MapKeys`, `MapSize` | TODO |
| **List** | `List<T>` | `ListNew`, `ListPush`, `ListGet`, `ListSet`, `ListSize`, `ListRemove` | TODO |
| **ReadLine** | `func` | `ReadLine() -> String` | TODO |
| **Print** | `func` | `Print(s)` (줄바꿈 없음, Log는 줄바꿈 있음) | TODO |

### Tier 2 — 유틸리티 (빌트인)

| 이름 | 타입 | 핵심 API | 상태 |
|------|------|----------|------|
| **Set** | `Set<T>` | `SetNew`, `SetAdd`, `SetHas`, `SetRemove`, `SetSize` | TODO |
| **Queue** | `Queue<T>` | `QueueNew`, `QueuePush`, `QueuePop`, `QueueSize`, `QueueEmpty` | TODO |
| **Clock** | `func` | `Now() -> Int` (ms), `Sleep(ms)` | TODO |
| **Format** | `func` | `Format(template, args...)` | 미래 |

### Tier 4 — 표준 라이브러리 (`use` 키워드)

| 이름 | `use` | 핵심 API | 용도 |
|------|-------|----------|------|
| **datetime** | `use datetime;` | `LocalDate`, `LocalTime`, `DateTime`, `FormatDate`, `FormatTime`, `FormatDateTime`, `SameDate` | 캘린더/결제/리포트 날짜 표면 |
| **http** | `use http;` | `HttpRequest`, `HttpResponse`, `RouteSpec`, `OkResponse`, `ErrorResponse`, `JsonResponse` | transport/intent adapter 경계 |
| **storage** | `use storage;` | `SnapshotMeta`, `SnapshotRecord`, `StorageSave`, `StorageLoad`, `StorageAppendLog` | snapshot/repository/persistence 표면 |
| **page** | `use page;` | `PageRoute`, `PageAction`, `PageMessage`, `MountPage`, `BindAction`, `RenderSection` | page/projection/action binder 표면 |
| **pool** | `use pool;` | `PoolNew`, `PoolSpawn`, `PoolDespawn`, `PoolGet`, `PoolAlive` | 오브젝트 풀, 엔티티 재활용 |
| **fsm** | `use fsm;` | `FsmNew`, `FsmAddState`, `FsmTransition`, `FsmCurrent` | 상태 머신 |
| **encounter** | `use encounter;` | `EncounterNew`, `EncounterStep`, `EncounterResolve`, `TurnOrder` | 전투/조우 상태 머신 |
| **strategy** | `use strategy;` | `StrategyPick`, `StrategyRoll`, `PolicyTable` | AI/행동 전략 |
| **tables** | `use tables;` | `TableRoll`, `TablePick`, `WeightedPick`, `LootTable` | 콘텐츠/확률 테이블 |
| **timer** | `use timer;` | `TimerNew`, `TimerTick`, `TimerDone`, `CooldownNew`, `CooldownReady` | 쿨다운, 타이머 |
| **event_bus** | `use event_bus;` | `BusNew`, `BusSubscribe`, `BusEmit`, `BusProcess` | 이벤트 버스/큐 |
| **ecs** | `use ecs;` | `WorldNew`, `Spawn`, `Query`, `AddComponent` | 미래: ECS 패턴 |
| **tween** | `use tween;` | `Lerp`, `EaseIn`, `EaseOut`, `Tween` | 미래: 보간/애니메이션 |

## 게임 프레임워크 경계

다음은 **언어 코어가 아니라 게임 라이브러리**로 다룬다.

- entity/object pool
- encounter/turn state machine
- strategy/AI policy
- content/loot/monster/event tables
- http request/response transport
- storage/repository/snapshot persistence
- page/action/projection adapters

즉 방향은:

```pergyra
use pool;
use fsm;
use encounter;
use strategy;
use tables;
use http;
use storage;
use page;
```

이지:

```pergyra
entity pool ...
strategy ...
```

처럼 새 코어 키워드를 늘리는 방향이 아니다.

그리고 이 계층은 단순 stdlib보다 **generic pattern library**에 가깝다.
즉 먼저 generic pattern을 제공하고, 실제 프로젝트가 domain을 주입한다.

GOF 기초 패턴도 같은 원칙으로 번역한다.

- `singleton` -> process-global static이 아니라 contextual runtime registry
- `factory` -> subclass hierarchy가 아니라 staged template/spec builder
- `strategy` -> interface object보다 policy card / policy table
- `strategy` -> function-typed picker / resolver도 허용
- `state` -> hidden state object보다 explicit FSM / transition rule + context application
- `observer` -> hidden callback graph보다 relay bundle / sink spec / report sink / event bus

자세한 번역 기준은 [`31_gof_pattern_catalog.md`](/mnt/e/PergyraLang/docs/31_gof_pattern_catalog.md)를 따른다.

## 앱 인프라 경계

`http`, `storage`, `page`는 **코어 문법이 아니라 표준 라이브러리 계층**이다.

- `page`는 projection surface
- `http`는 transport / adapter surface
- `storage`는 persistence / repository surface
- 실제 실행 경계는 여전히 `intent`와 `zone/world`

즉 앱 구조는 보통 다음을 따른다.

```text
page -> http adapter -> intent -> zone/world -> dto/object -> page
                     \-> storage
```

이는 `page == zone`, `api == zone`으로 보지 않고,
표면과 실행 경계를 분리하려는 Pergyra 철학과 맞춘다.

### Tier 3 — 기존 (이미 빌트인)

| 카테고리 | API |
|----------|-----|
| Math | `Min`, `Max`, `Abs`, `Sqrt`, `Pow`, `Floor`, `Ceil`, `Random` |
| String | `StringLength`, `StringSplit`, `StringJoin`, `StringContains`, `StringReplace`, `Trim`, `Upper`, `Lower`, `Substring`, `ToString` |
| Convert | `ToInt`, `ToFloat` |
| I/O | `Log`, `FileOpen`, `FileRead`, `FileWrite`, `FileClose` |
| Array | `ArrayLength`, `ArrayPush`, `ArraySort`, `ArrayMap`, `ArrayFilter`, `ArrayReverse` |
| Option | `Some`, `None`, `IsSome`, `IsNone` |
| Result | `Ok`, `Err`, `IsOk`, `IsErr` |
| Slot | `ClaimSlot`, `Read`, `Write`, `Release`, `Move` |
| Channel | `Channel`, `<-` (send/recv) |
| Async | `spawn`, `await` |

## 구현 위치

```
src/runtime/pgy_runtime.h      — 매크로, 인라인 함수, 타입 정의
src/runtime/pgy_runtime_lib.c  — LLVM 링킹용 extern 심볼
src/semantic/type_checker_builtins.c — 시맨틱 타입 체크
src/codegen/transpiler.c       — C 코드젠 emit
```

## HashMap 설계

```c
// C 런타임 내부
typedef struct {
    char **keys;
    void **values;
    size_t *hashes;
    size_t count;
    size_t capacity;
    size_t value_size;
} PgyHashMap;
```

Pergyra 표면:
```pergyra
let inventory = MapNew();           // HashMap<String, Int>
MapSet(inventory, "sword", 1);
MapSet(inventory, "potion", 3);
let count = MapGet(inventory, "potion");   // 3
let has: Bool = MapHas(inventory, "shield");  // false
let keys = MapKeys(inventory);      // Array<String>
```

## List 설계

```c
// C 런타임 내부 (subject 포인터 배열)
typedef struct {
    void **items;
    size_t count;
    size_t capacity;
    size_t item_size;
    bool is_pointer_type;  // subject면 true
} PgyList;
```

Pergyra 표면:
```pergyra
let party = ListNew();
ListPush(party, hero);        // subject 자동 reference
ListPush(party, mage);
let member = ListGet(party, 0);   // hero 참조
let size = ListSize(party);       // 2
```

## ReadLine 설계

```c
char *pgy_read_line(void) {
    static char buf[1024];
    if (fgets(buf, sizeof(buf), stdin) == NULL) return "";
    buf[strcspn(buf, "\n")] = '\0';
    return buf;
}
```

Pergyra 표면:
```pergyra
Print("> ");
let input = ReadLine();
if input == "quit" {
    Log("Goodbye!");
}
```
