# Pergyra 현재 구현 문법 레퍼런스

이 문서는 `lexer`, `parser`, `semantic`, `tests`, `examples` 기준의 **실제 구현 문법**을 정리한다.
설계 아이디어와 장기 비전은 별도 문서에 둘 수 있지만, 이 문서는 “지금 파서가 읽고 컴파일러가 처리하는 형태”를 기준으로 한다.

관련 문서:
- 최소 요약: [01_syntax.md](01_syntax.md)
- 네이밍 규칙: [03_naming.md](03_naming.md)
- 비전: [00_vision.md](../00_vision.md)

## 1. 기본 규칙

- 키워드는 소문자 기준이다.
  예: `let`, `func`, `with`, `parallel`, `if`, `for`, `async`, `await`
- Language-word source of truth:
  `src/lexer/language_keyword_registry.def`. Reserved token identity and
  contextual/soft vocabulary are separate row classes in that registry.
- Lexer-reserved keywords (70): `ability`, `as`, `async`, `await`, `bind`,
  `break`, `case`, `class`, `collapse`, `compensate`, `continue`, `default`,
  `defer`, `dyn`, `effect`, `else`, `enum`, `event`, `export`, `extends`,
  `extern`, `fail`, `false`, `for`, `func`, `if`, `impl`, `import`, `in`, `include`,
  `innate`, `intent`, `let`, `local`, `match`, `namespace`,
  `nondeterministic`, `object`, `override`, `own`, `parallel`, `party`,
  `private`, `public`, `ref`, `reflect`, `relation`, `remote`, `return`, `role`,
  `roster`, `secure`, `select`, `shared`, `slot`, `spawn`, `struct`,
  `subject`, `tobject`, `transaction`, `true`, `type`, `unsafe`, `use`, `vessel`, `where`,
  `while`, `with`, `world`, `zone`.
- `world`, `roster`, `relation`, `effect`, `zone`, `intent`, `vessel`, and
  `event` are lexer-reserved declaration tokens, not contextual words.
- Parser-contextual words are matched as identifiers by the owning parser.
  Examples include `action`, `requires`, `within`, `causes`, `authorized`,
  `by`, `involves`, `step`, `who`, `expect`, `success`, and `failure`.
- The exhaustive 70 reserved + 73 contextual + 3 soft inventory (146 rows) and
  consumer projection contract are documented in
  `docs/semantics/language_keyword_registry.md`; this prose list is illustrative.
- `context`는 현재 ordinary identifier다.
- 내장 API와 타입은 PascalCase 기준이다.
  예: `Int`, `String`, `ClaimSlot`, `Read`, `Write`, `Release`
- 문장 종료는 세미콜론 `;` 이다.
- 블록은 `{ ... }` 로 쓴다.
- **중괄호 스타일의 canonical form은 BSD (Allman)이다.**
- K&R 표기는 파서가 읽을 수는 있지만, 그것을 문서 표준으로 간주하지 않는다.
- 문서, scaffold, formatter가 내놓는 기준 표기는 BSD로 고정한다.
- 현재 구현 기준으로 “문서상 제안만 있고 미구현인 문법”은 이 문서에 실지 않는다.

### 1.1 참조 전달 규칙

- `subject`, `relation`, `effect`, `zone`, `world`는 identity-bearing 타입이다.
- 이 타입들을 함수 파라미터로 전달하면 **자동으로 reference(참조) 전달**된다.
- 사용자는 포인터를 의식하지 않아도 된다 -- 언어가 내부적으로 처리한다.
- `struct`, `vessel`, `class`, `object`, `tobject`는 canonical value 타입이다 --
  일반 파라미터는 복사 전달되어야 한다. 현재 C/LLVM의 vessel 파라미터는
  pointer-self fact를 잘못 재사용하는 알려진 구현 결함이다.
- 이 규칙은 함수 인자 carriage다. hosted receiver는 별도이며 `subject`와
  `vessel`은 pointer-self, `class`/`object`/현재 `tobject`는 value-self다.
  구성체별 authoring contract는
  [`../200_object_to_action_boundary_patterns.md`](../200_object_to_action_boundary_patterns.md)를 따른다.

## 2. 리터럴과 표현식

### 2.1 리터럴

```pergyra
42;
"hello";
true;
[1, 2, 3];
```

지원되는 리터럴:
- 정수
- 문자열
- 불리언
- 배열 리터럴

### 2.2 기본 표현식

```pergyra
a + b * c;
a == b;
a && b || !c;
obj.Method(42);
array[i + 1];
value = other;
pipeline = x |> F |> G;
let y = Validate(x)?;
```

주요 표현식 종류:
- 이항/단항 연산
- 함수 호출
- 멤버 접근
- 배열 인덱싱
- 배열 리터럴
- 대입
- `await`
- `spawn`
- 채널 송수신
- 파이프 `|>`
- postfix `?` (Result<T> unwrap + early return)
- 컴파일타임 리플렉션 `reflect`
- `${}` 문자열 보간: `"hello ${name}"`

문자열 stable subset:
- `"..."`는 escape `\n`, `\r`, `\t`, `\\`, `\"`, `\0`를 지원한다.
- `"""..."""`는 raw multiline payload이며 보간하지 않는다.
- `"${expr}"`와 `f"{expr}"`는 단순 expression interpolation만 지원하고 `ToString(expr)`로 낮아진다.
- `f"\{name}"` and `"\${name}"` escaped interpolation openers remain literal text.
- malformed interpolation expressions remain literal text.
- nested brace matching, format specifier, multiline interpolation은 베타 밖이다.

### 2.3 우선순위

대체로 다음 순서를 따른다.

1. 호출, 멤버 접근, 배열 접근
2. 단항 `!`, `-`, `reflect`
3. `*`, `/`, `%`
4. `+`, `-`
5. 비교 `< <= > >=`
6. 동등 `== !=`
7. 논리 `&&`
8. 논리 `||`
9. 파이프 `|>`
10. 할당 `=`

### 2.4 컴파일타임 리플렉션 (`reflect`)

`reflect`는 컴파일타임 prefix 연산자다. 대상을 받아 그 대상을 기술하는
`projection`을 만든다. 전적으로 컴파일타임에 평가되며 런타임 코드를 내지
않는다.

```pergyra
let p: projection = reflect Account;
let n: String = p.name;            // "Account"
```

현재 구현 범위:

- 대상이 타입 이름(식별자)일 때 `reflect TypeName`은 컴파일타임에 `projection`이
  되고, 모든 필드 접근은 컴파일타임에 폴딩된다.
- 필드: `.name`(타입/선언 이름), `.kind`(선언 종류:
  `"struct"`/`"class"`/`"subject"`/`"vessel"`/`"object"`/`"tobject"`/`"enum"`/`"primitive"`),
  `.effects`(effect 집합 문자열, 예
  `"io,alloc"`; 함수/intent가 아니면 빈 문자열), `.fields`(클래스/구조체 필드의
  `"name:Type"` 쉼표 결합, 예 `"id:Int,owner:String"`), `.methods`(메서드 이름의
  쉼표 결합). 함수를 대상으로 하면 `.params`(파라미터 `"name:Type"` 결합)와
  `.returns`(반환 타입 이름)도 폴딩된다. intent를 대상으로 하면 `.steps`(step
  이름 결합), `.retry`(선언된 retry 횟수), `.involves`(참여자 `"alias:Subject"`
  결합)도 폴딩된다.
- direct `(reflect T).field`와 `let p: projection = reflect T; p.field` 둘 다
  지원한다.
- `projection`은 컴파일타임 전용 값 타입이며 런타임 흔적을 남기지 않는다.
- `.authority`(특정 authority 이름), resilience 정책, 그리고 제네릭/도메인 엔티티
  대상은 후속 단계다.

## 3. 선언

### 3.1 변수 선언 (`let`)

바인딩은 `let` 하나로 통일한다 — 타입 추론은 타입 생략, 명시는 `: Type`.

```pergyra
let x = 42;                          // 타입 추론
let mut count: Int = 0;
let name: String = "Pergyra";
let values: Array<Int> = [1, 2, 3];
```

과거에는 `x := 42` 단축 선언(`let`의 축약)도 받았으나, `let x = 42`와 의미가
중복이라 제거됐다(A안, 2026-06). 추론=타입 생략, 명시=`let x: T = v`로 1:1.

| 형태 | 타입 추론 | 타입 명시 | 용도 |
|------|----------|----------|------|
| `let x = 42;` | O | O (`let x: Int = 42;`) | 기본 선언 |
| `let mut x = 42;` | O | O (`let mut x: Int = 42;`) | mutation intent가 있는 선언 |

#### 왜 `let`인가

- 제네릭 타입(`Array<Pair<Int, String>>`)이 선언 시작에 오면, 파서가 타입과 비교 연산(`<`)을 구분할 수 없다
- `let` 키워드가 "여기서 선언이 시작된다"는 명확한 신호를 파서에 준다
- 타입 추론 선언도 `let x = expr` 하나로 고정한다. 단축 선언은 같은 의미의
  두 번째 철자를 만들기 때문에 stable surface에서 제거됐다.

#### 가변성

Pergyra의 mutability 표면은 `let` / `let mut`으로 분리된다.
`let`은 binding introducer이고, `let mut`은 이후 mutation intent를
명시한다. 이 분리는 약점이 아니라 강점이다. mutation이 필요한 저장소가
source에 드러나고, `inout` value-result mutation이나 Slot/Pin 권한
mutation과 섞이지 않는다.

```pergyra
let x = 42;
let mut y = 42;
y = y + 1;    // mutation intent가 명시됨
let mut z: Int = x + 1;
```

현재 beta 구현에서 `let mut`은 parser surface와 nominal field assignment
gate에 반영되어 있다. 특히 `let name: Type;` field는 immutable field
binding이고, `let mut name: Type;` field만 assignment 가능하다. plain local
reassignment compatibility는 별도 strict gate가 닫히기 전까지 남을 수
있지만, 문서와 새 예제의 표준 spelling은 mutation intent가 있는 바인딩에
`let mut`을 쓴다.

- 일부 자원 타입은 `let` 초기화 규칙이 더 엄격하다.
  예: `QubitSlot`, `DeviceSlot`, `ReadView<T>`, `WriteView<T>`, `MoveToken<T>`

구조 분해(destructuring):

```pergyra
let (a, b, c) = Split("x y z", " ");
```

- 튜플/배열 반환값을 여러 변수에 동시 바인딩할 수 있다.

### 3.2 함수와 action

Pergyra에는 `method` 키워드가 없다. 함수는 `func`와 `action` 두 가지뿐이다.

#### 용어 정의

| 용어 | 의미 |
|------|------|
| **free func** | top-level 함수. 타입에 소속되지 않음 |
| **hosted func** (귀속 func) | 타입 안에 선언되고 `self`를 받는 func. 해당 타입에 귀속됨 |
| **general func** | subject 안의 일반 func. 사적 판단, 내부 계산용 |
| **action** | subject 전용. zone/effect/authority와 연동되는 공적 행위 |

> **"method"라는 단어는 Pergyra에서 쓰지 않는다.**
> OOP의 "method"가 암시하는 상속/오버라이딩/가상 디스패치와 거리를 두기 위함이다.

```pergyra
// free func
func Add(a: Int, b: Int) -> Int
{
    return a + b;
}

// hosted func (vessel에 귀속)
vessel HP
{
    func Percentage(self) -> Int
    {
        return (current * 100) / max;
    }
}

// subject: general func (사적 판단) + action (공적 행위)
subject Fighter
{
    func IsAlive(self) -> Bool         // general func
    {
        return hp > 0;
    }
    action Attack(self, target: Fighter) -> Int   // action
        requires Combatable
        within BattleZone
        causes DamageEffect
    {
        // ...
    }
}
```

#### func vs action

| 구분 | `func` | `action` |
|------|--------|----------|
| 허용 위치 | top-level 및 허용 host (`class`, `object`, `vessel`, `role`, `subject`; 현재 `tobject`) | subject 전용 |
| self 바인딩 | 선택 (self가 있으면 hosted func) | 필수 |
| capability/effect | `with caps`, `with effects` 가능 | 동일하게 가능 |
| action contract | 없음 | `requires`, `within`, `causes`, `authorized by` |
| 소설 비유 | 머릿속 계산 (관객이 안 봄) | 무대 행동 (관객이 봄) |
| subject 안 용어 | general func | action |

`func == pure`, `action == impure`는 문법 규칙이 아니다. action은 subject가
공적 전이, 권한, resource/stage handoff를 소유할 때 사용한다. `struct` hosted
func는 semantic에서 거부되고, `tobject` helper는 현재 허용되지만 canonical
boundary DTO는 method-free다.

#### action 전용 clause

```pergyra
action Attack(self, target: Fighter) -> Int
    requires Combatable              // ability 자격
    within BattleZone                // zone 제약
    causes DamageEffect              // effect 선언
    authorized by self, target
{
    // ...
}
```

#### 백엔드별 lowering

컴파일러가 free func / hosted func / action을 구분하고, 백엔드별로 적절한 형태로 emit한다:

```
C 백엔드:
  free func     -> FuncName(...)
  hosted func   -> ReceiverCarriage에 따라 TypeName_FuncName(Type self, ...)
                   또는 TypeName_FuncName(Type *self, ...)
  action        -> TypeName_ActionName(Type *self, ...)

직접 JS 백엔드 (beta+1 이후 재검토, 베타 dogfood 경로 아님):
  note         -> direct JS backend is beta+1, not the beta dogfood path
  free func     -> function FuncName(...) { }
  hosted func   -> class Type { FuncName() { this.xxx } }
  action        -> class Type { ActionName() { this.xxx } }
```

`subject`/`vessel` hosted receiver와 subject-only `action`은 pointer-self다.
`class`/`object`/현재 `tobject` hosted `func`는 value-self다. 이 hosted receiver
carriage를 일반 함수 parameter carriage로 재사용하지 않는다. 현재 vessel 일반
parameter까지 간접 전달하는 C/LLVM 동작은 알려진 결함이지 위 규칙의 예외가
아니다.

제네릭과 `where` 절:

```pergyra
func Identity<T>(value: T) -> T {
    return value;
}

func Sort<T>(items: Array<T>) -> Array<T>
where T: Comparable {
    return items;
}
```

지원되는 요소:
- free function (top-level)
- self-bound function (타입 내부, self 파라미터)
- action (subject 전용, zone/effect 연동)
- 제네릭 함수
- `where` 제약
- `async func`
- `export func`

주의:
- `async func`는 현재 제네릭/`where` 절을 지원하지 않는다.

### 3.3 타입 선언

```pergyra
struct Vec3 {
    x: Float;
    y: Float;
    z: Float;
}

subject Player {
    private let name: String;
    public let health: Int;
}

enum Color { Red, Green, Blue }

enum Shape {
    Circle(Int),
    Rect(Int, Int),
    None
}
```

지원되는 선언:
- `struct`
- `object` (internal projection/value declaration)
- `subject` (identity-bearing active host; `class` alias가 아님)
- `class`
- `tobject` (transfer-object declaration)
- `vessel` (subject-owned passive state/resource host)
- `enum`
- `relation`
- `effect`
- `zone`
- `extern "C"` block

추가 메모:
- `export`는 module boundary modifier다. `public/private`와 같은 축이 아니다.
- `object`와 `tobject`는 struct-style body syntax를 공유하지만 같은 의미가 아니다.
- `object`는 local/internal projection contract다. zone/world 실행 경계 안에서 source 상태를 읽기 위해 `refresh`되는 view 모델이다.
- `tobject`는 transfer/boundary projection contract다. `publish`를 통해 zone/world/export 경계를 넘기는 전달 모델이다.
- 따라서 `tobject`는 `object`의 단순 축약형이 아니며, projection state와 boundary contract에서 별도 취급한다.
- semantic은 현재 `object`/`tobject` passive helper `func`를 허용한다.
  canonical authoring은 object의 관측/query만 남기고 tobject를 method-free로
  쓰며, 이 차이는 열린 semantic closure다.
- canonical `object`/`tobject` field는 construction 이후 read-only/immutable이지만
  현재 bare/nested field write 검사에는 gap이 있다. `publish`는 detached value
  projection이며 tobject 전용 channel/API/IPC transport를 보장하지 않는다.
- `ability`는 기본 공개 계약이다. cross-module에서 숨기고 싶을 때만 `private ability`를 사용한다. 따라서 `export ability`는 허용되더라도 중복 표기다.
- `relation`, `effect`는 현재 optional `for name: Type[, ...]` header와 `subject slot`, `object slot`, `tobject slot`, `refresh`, `publish`, `bind`, `shared`, `func`의 최소 조합을 지원한다.
- `shared`는 visibility 키워드가 아니라 host-local contextual state marker다. 즉 `party` / `relation` / `effect` / `zone` / `world`가 문맥 전체에서 공동으로 읽고 갱신하는 상태를 뜻한다.
- `HasProjection(<slotName>)`는 relation/effect/zone declaration / method 안에서만 유효하며, 선언된 object/tobject projection slot을 Bool로 조회한다.
- `relation` / `effect` / `zone`의 subject/vessel이 아닌 projection slot initializer는 semantic error다. `object slot`은 `refresh`, `tobject slot`은 `publish`, target kind에 맡기는 경우는 `bind`로 materialize한다.
- `zone` body는 현재 `subject slot`, caller-admitted `binding slot`, derived `object slot`, detached `tobject slot`, `relation slot`, `effect slot`, `effect pool <name>: <EffectType> capacity <N>`, `authority <subjectSlot> [requires <Ability>[, ...]]`, `state <name>: effect <effectSlot> on <targetSlot>`, `state <name>: relation <relationSlot> between <left>, <right>`, `apply <effectSlot> to <targetSlot>`, `apply <stateName>`, `detach <effectSlot> from <targetSlot>`, `detach <stateName>`, `link <relationSlot> between <left>, <right>`, `link <stateName>`, `unlink <relationSlot> between <left>, <right>`, `unlink <stateName>`, `refresh <objectSlot> from <subjectOrBindingSlot>`, `publish <tobjectSlot> from <subjectSlot>`, `bind <slotName> from <subjectOrBindingSlot>`, `maintain <effectSlot> on <targetSlot>`, `maintain <relationSlot> between <left>, <right>`, `maintain <stateName>`, `shared`, `func`를 지원한다.
- Native semantic은 `apply <stateName>`을 state declaration의 exact effect/target
  slot로 정규화한 뒤 DIR/MIR `apply-effect` row에 운반한다. 현재 production
  self-host source parser는 직접형 `apply <effectSlot> to <targetSlot>`만 admit하고
  state alias form은 fail closed한다. 이 self parser/DIR parity는 열린 항목이다.
- `zone`의 `apply/link/detach/unlink/refresh/publish/bind/maintain`은 optional `by <subjectSlot>` authority annotation을 받을 수 있다.
- `bind <slotName> from <sourceSlot>`는 target slot kind를 declaration에서 유도한다. object slot이면 `refresh`, tobject slot이면 `publish`와 같은 projection contract를 사용한다.
- `HasLayer(<layerSlot>)`는 zone declaration / zone method 안에서만 유효하며, 선언된 relation/effect layer slot을 Bool로 조회한다. C backend는 generated helper가 `PGY_ZONE_RDLOCK`과 generation stale-warning을 자동으로 감싼다.
- `HasState(<stateName>)`는 zone declaration / zone method 안에서만 유효하며, 선언된 zone state alias를 Bool로 조회한다. 인자는 identifier나 string literal을 받을 수 있다.
- `HasState(<effectState>, <targetSlot>)`와 `HasState(<relationState>, <leftSlot>, <rightSlot>)`는 선언된 state alias와 slot 조합이 정확히 맞는지까지 검증한다.
- `zone` body는 여기에 더해 `relation slot`, `effect slot`을 지원한다.
- `world` body는 `roster`, `zone`, `state <name>: zone <zoneSlot>`, `state <name>: zone <zoneSlot> projection <projectionSlot>`, `state <name>: zone <zoneSlot> layer <layerSlot>`, `state <name>: zone <zoneSlot> state <zoneStateName>`, `state <name>: all <zoneOrState>[, ...]`, `state <name>: any <zoneOrState>[, ...]`, `activate <zoneOrState>`, `deactivate <zoneOrState>`, `maintain <zoneOrState>`, `shared`, `func`를 지원한다.
- `shared`는 프로그램 전역 global이 아니라 해당 host에 귀속된 "local-global" 상태다. 예: `zone`의 `shared round`, `relation`의 `shared trust`, `world`의 `shared season`.
- `HasZone(<zoneOrState>)`는 world declaration / world method 안에서만 유효하며, 선언된 zone slot 또는 world state alias를 Bool로 조회한다.
- `HasZoneProjection(<zoneSlot>, <projectionSlot>)`는 world declaration / world method 안에서만 유효하며, embedded zone의 선언된 object/tobject projection slot sync-ready flag를 Bool로 조회한다.
- `HasZoneLayer(<zoneSlot>, <layerSlot>)`는 world declaration / world method 안에서만 유효하며, embedded zone의 선언된 relation/effect layer active flag를 Bool로 조회한다.
- `HasZoneState(<zoneSlot>, <stateName>)`는 world declaration / world method 안에서만 유효하며, embedded zone의 선언된 state alias flag를 Bool로 조회한다.
- `intent`는 parser/semantic/HIR/codegen이 직접 이해하는 orchestration declaration이다. 현재는 `intent Name(args...)`, legacy `involves`, `step`, `exclusive`/`concurrent`, `rollback`, `priority`, `where`, `who`, `using`, `transfer`, repeated `on`, repeated `compensate`, `pre`, `guard`, `post`, `invariant`, `requires`, `authorized by`, `causes`, `expect`, `success`, `failure`를 파싱하고 검증하며, `Intent(args...)` 호출은 C/LLVM generated function으로 lowering된다.
- 현재 runtime은 executable intent function + basic conflict scheduler + active registry observability + trace/rollback engine이다. `exclusive` 충돌 차단, `concurrent` 병행 허용, higher-`priority` nested override, `rollback: full/current/none`, `IntentLastTrace()` / `IntentLastFailure()` / `IntentLastName()` / `IntentLastHandle()` / `IntentLastTraceId()` / `IntentLastStepCount()` / `IntentLastFailed()`, `IntentHistoryCount()` / `IntentHistoryStep*()`, `IntentActiveCount()` / `IntentActive*()`, `using:` 기반 live zone-instance sync와 participant-to-zone-slot materialization, `transfer:` 기반 cross-zone handoff materialization과 typed transfer trace/history까지는 구현되어 있다. 남은 건 richer multi-instance timeline query와 richer rollback policy detail이다.
- 파생 world state는 읽기 전용 contract이며 `activate/deactivate/maintain` 대상으로는 plain `state <name>: zone <zoneSlot>` alias만 허용한다.
- `all` / `any` 조합 state는 duplicate input과 direct zone slot + plain zone alias 중복을 semantic warning으로 정리한다.
- `all` / `any` 조합 state는 raw zone slot을 직접 입력으로 받는 것도 warning으로 정리하며, plain world state alias를 통한 조합을 권장한다.
- `projection` / `layer` / `state` suffix는 같은 줄에서만 world-state source modifier로 해석된다.

미지원:
- `type` alias (파서/시맨틱 미구현)

### 3.4 모듈

```pergyra
import "math.pgy";

namespace Math
{
    export func Add(a: Int, b: Int) -> Int
    {
        return a + b;
    }
}
```

지원되는 요소:
- `import`
- `namespace`
- `export`

## 4. 타입 표기

### 4.1 기본 타입

- `Int`
- `Long`
- `Float`
- `Double`
- `Bool`
- `String`
- `Void`
- `projection` (컴파일타임 전용, `reflect`의 결과 타입)

### 4.2 컬렉션/자원 타입

```pergyra
Array<Int>
Slice<Float>
Channel<Int>
Future<Int>
RemoteFuture<Int>
Slot<Int>
SecureSlot<Int>
ReadView<Int>
WriteView<Int>
MoveToken<Int>
QubitSlot
DeviceSlot<Int>
Rc<Int>
Weak<Int>
Box<Array<Int>>
Allocator
```

```pergyra
Option<Int>
Result<Int>
```

현재 구현이 해석하는 대표 타입군:
- 배열/슬라이스
- 채널/퓨처
- 슬롯 계열
- 양자/디바이스 자원
- 공유 소유권/박스/할당기
- Option/Result 타입

## 5. 제어문

### 5.1 조건문

```pergyra
if x > 10 {
    Log("big");
} else {
    Log("small");
}
```

### 5.2 반복문

```pergyra
for i in 0..10 {
    Log(i);
}

for item in array {
    Log(item);
}

while cond {
    if stop { break; }
    continue;
}
```

지원:
- `for i in start..end`
- `for name in collection` (배열 등 컬렉션 순회)
- `while`
- `break`
- `continue`

### 5.3 패턴/선택

```pergyra
match value {
    case 0:
        Log("zero");
    case 1 if value > 0:
        Log("one");
    default:
        Log("other");
}

select {
    case v = <-ch:
        Log(v);
    default:
        Log(0);
}
```

지원:
- `match`
- `case`
- `default`
- `guard` (`case x if cond:`)
- `select`
- `case <-ch:`
- `case v = <-ch:`

## 6. 슬롯과 자원 셀

### 6.1 기본 슬롯

```pergyra
let s: Slot<Int> = 42;
Log(s);
s = 7;
Release(s);
```

또는 명시형:

```pergyra
let s: Slot<Int> = ClaimSlot();
Write(s, 42);
let v: Int = Read(s);
Release(s);
```

### 6.2 `with`

```pergyra
with slot<Int> as hp {
    hp.Write(100);
    Log(hp.Read());
}

with SecureSlot<Int>(SECURITY_LEVEL_HARDWARE) as hp {
    hp.Write(100);
}
```

주의:
- `SECURITY_LEVEL_*`는 현재 `with SecureSlot<T>(...)` 문법에서 **파싱/보존만**
  되며, 생성 C/LLVM `SecureSlot<T>` ABI의 BASIC/HARDWARE/ENCRYPTED 정책
  선택으로는 연결되지 않는다. `SlotManager` C 런타임의 보안 레벨은 별도
  API 계층이다.

### 6.3 View / Move

```pergyra
let rv: ReadView<Int> = ViewRead(s);
let wv: WriteView<Int> = ViewWrite(s);
let mt: MoveToken<Int> = Move(s);
let dst: Slot<Int> = mt;
```

## 7. 병렬 / 비동기 / 채널

### 7.1 `parallel`

```pergyra
parallel {
    WorkA();
    WorkB();
}
```

- `parallel`은 pthread 기반 병렬 실행 경로다.

### 7.2 `spawn` / `await`

```pergyra
async func Fetch() -> Int {
    let task = spawn Compute(42);
    let value: Int = await task;
    return value;
}
```

- `spawn`은 `Future<T>` 계열을 만든다.
- `spawn blocking`은 블로킹 작업을 별도 스레드에서 실행한다.
- `await`는 async 문맥 안에서 사용한다.
- 현재 구현은 코루틴 런타임 경로를 사용한다.

`spawn blocking` 예시:

```pergyra
let task = spawn blocking HeavyWork();
let result: Int = await task;
```

`RemoteFuture<T>`는 `await` 결과가 `Result<T>`가 된다.

### 7.3 `async` 블록

```pergyra
async {
    ch <- 11;
}
```

- `async { ... }` 는 detached async block으로 해석되는 parser/runtime 경로다.
- 베타 안정 표면: 태스크 생성은 named `spawn Worker(args...)`이다.
- capture-bearing detached `async { ... }` 블록은 lifetime/cancel/error 경계가
  아직 고정되지 않아 베타 안정 태스크 생성 표면이 아니다.

### 7.4 채널

```pergyra
let ch: Channel<Int> = Channel(4);
ch <- 42;
let value: Int = <-ch;
```

## 8. 도메인 계층

### 8.1 ability / role

```pergyra
ability Damageable {
    fields health: Int;
    func TakeDamage(amount: Int) -> Void;
}

role PlayerDamageable for Player {
    impl ability Damageable {
        func TakeDamage(amount: Int) -> Void {
            Log(amount);
        }
    }
}
```

### 8.2 party / roster / world

```pergyra
party Team {
    role slot tank: Damageable & Taunting;
    shared formation: String = "standard";
}

// Stable: top-level role slot ability intersections only.
// Reserved: Array<Damageable & Taunting>, Damageable | Taunting.

roster CombatSystem {
    party slot team1: Team;
}

world GameWorld {
    roster combat: CombatSystem;
}
```

### 8.3 participant / event / lambda

```pergyra
subject Counter {
    let count: Int;
}

subject Counter {
    let count: Int;
}

event OnHit(damage: Int);

OnHit += (d: Int) => { Log(d); };
OnHit(77);
```

callable type:

```pergyra
func Apply(base: Int,
           policy: func(Int) -> Int) -> Int {
    return policy(base);
}
```

## 9. 기타 문법

### 9.1 안전하지 않은 블록 / defer / bind

AST와 parser 기준으로 다음 문법 진입점이 있다.

```pergyra
unsafe {
    Dangerous();
}

defer {
    Cleanup();
};
```

```pergyra
bind team.fighter = Warrior;
```

주의:
- `unsafe`와 `defer`는 시맨틱/백엔드 경로가 존재하며 회귀 테스트로 검증된다.
- `bind`는 파서/시맨틱/코드젠 경로가 있으며, role slot에 구체 타입을 동적으로 바인딩하는 데 사용한다.

## 10. 상태 구분

이 문서 기준 구분:

- **지원됨**: parser + semantic + backend/tests/examples로 확인된 문법
- **부분 지원**: parser/semantic은 있으나 제한이 크거나 backend 차이가 있는 문법
- **실험적**: 설계 방향은 있으나 문서상 고정 문법으로 간주하면 안 되는 영역

현재 특히 주의할 영역:
- `defer`
- 일부 고수준 domain 문법의 세부 의미론
- effect 표기 문법의 확장
- backend별 세부 동작 차이
## Visibility grammar note

`public` and `private` are no longer just nominal/member modifiers.

Current stable grammar surface allows top-level visibility modifiers on:

- nominal declarations: `subject`, `class`, `struct`, `object`, `tobject`, `vessel`
- domain declarations: `ability`, `party`, `roster`, `world`, `zone`, `relation`, `effect`
- callable declarations: `func`, `intent`, `event`

This means module visibility is expressed directly on the declaration that owns the exported surface, instead of being restricted to type-like declarations only.

## Common Syntax Pattern Grammar Policy

The syntax pattern status table lives in
[../124_syntax_pattern_matrix.md](../124_syntax_pattern_matrix.md).
This grammar reference mirrors the current contract rather than redefining it.

Grammar-level classification for common patterns:

| Pattern family | Accepted grammar | Reserved or rejected grammar |
|---|---|---|
| Calls | ordinary positional calls, callable values, function references | named arguments remain semantic-rejected before dispatch |
| Parameters | typed function/async/lambda parameters, generic default type parameters | value default arguments in parameter lists |
| Lambdas | `=>` expression/block bodies; callable-let lambdas may copy-capture value-type locals (Stage A, docs/135) | escaping lambdas, ref/own/resource capture, full closure environments |
| Strings | normal strings, raw multiline strings, simple interpolation | nested interpolation protocol and format specifiers |
| Collections | array literals, map literals, indexed access, `xs[a..b]`, `xs[a..]`, `xs[..]`, `.Slice(start, len)`, `SliceCopy(view)` | list/set literal lowering, spread/rest `...` |
| Destructuring | positional `let (a, b) = value;` | named field destructuring |
| Matching | match, guards, or-patterns | broad structural/list patterns |
| Conversion | explicit helper calls | broad expression `as`/`is` conversion/type-test syntax |
| Construction | constructors/factory calls, declaration initializers, parser-level `Type { field: value }` initializer sugar | fully frozen object-initializer dispatch/partial-construction semantics |
| Optionality | `Option<T>`, `Result<T,E>`, postfix `?` result propagation, `Option<T> ?? T` | `?.` |
| Metadata | structured doc comments | `@` attributes/decorators |
| Unsafe | `unsafe { ... }` boundary marker | raw pointer escape without a scoped unsafe capability contract such as `unsafe(raw) { ... }` |
| Generic shorthand | explicit type args/default type args | `_` placeholder elision |

Reserved grammar must fail explicitly. It should not be accepted silently by the
parser or treated as beta-stable because a token exists.
## Executable implementation crosswalk

Use [13_implementation_status.md](13_implementation_status.md) for the
fixture-backed parser, semantic, self-host, and backend status. It is the
current gate-backed supplement to this reference.
