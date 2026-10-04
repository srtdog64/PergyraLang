# Pergyra 언어 레퍼런스 & 의미론

> 이 문서는 Pergyra 언어의 문법 표면(surface)과 **의미론(semantics)**을 함께
> 설명하는 레퍼런스입니다. 홈페이지 게시용 원본이며, 구문·설명·예제·표면 상태·
> 형식 의미론을 기술합니다. 문법의 최종 권위는 컴파일러 소스(렉서/파서)와
> `src/lexer/language_keyword_registry.def`, 정형 의미론의 권위는
> `docs/semantics/proofs/`의 Coq/Rocq 코퍼스입니다.

---

## 목차

1. [개요](#1-개요)
2. [표기 규약](#2-표기-규약)
3. [어휘 구조](#3-어휘-구조)
4. [프로그램 구조](#4-프로그램-구조)
5. [타입](#5-타입)
6. [변수와 바인딩](#6-변수와-바인딩)
7. [표현식](#7-표현식)
8. [문장과 제어 흐름](#8-문장과-제어-흐름)
9. [함수](#9-함수)
10. [도메인 구성체](#10-도메인-구성체)
11. [자원과 소유권](#11-자원과-소유권)
12. [동시성과 비동기](#12-동시성과-비동기)
13. [오류·보상 처리](#13-오류보상-처리)
14. [표준 라이브러리 표면](#14-표준-라이브러리-표면)
15. [문법 표면 상태](#15-문법-표면-상태)
16. [의미론](#16-의미론)

---

## 1. 개요

Pergyra(`.pgy`)는 **의도(intent)를 1급으로 두는 도메인 모델링 언어**입니다.
슬로건은 **"개발자가 즐거워야 유저도 즐겁다"** 입니다.

핵심 설계 원칙:

- **주소가 아니라 자원 점유권** — 포인터 대신 `Slot`으로 자원을
  `Claim / Read / Write / Release` 합니다.
- **기본 계산은 관용적으로** — `func`, `let`, `if`, `while`, `match`는 일부러
  친숙하게 두고, "어려움"은 도메인 구문에만 집중합니다.
- **도메인 의미를 구문으로** — `subject`, `ability`, `effect`, `zone`,
  `world`, `intent`가 소유권·권한·효과·경계를 명시적으로 표현합니다.
- **fail-closed** — 미구현 구문은 조용히 무시되지 않고 거부되며
  `Reason:` / `Fix:` 진단을 냅니다.

```pergyra
// "누가, 무엇을 위해, 어떤 자격으로, 어떤 결과로" 행동하는가를 선언한다.
intent Patrol(battle: BattleZone, hero: Hero) {
    step Guard {
        using: battle;
        on: hero.Guard();
        expect: true;
    }
}
```

두 가지 정체성:

1. **1차 정체성 — 도메인 모델링 언어**: 복잡한 도메인에서
   *왜(intent), 어떤 세계/장면에서(world/zone), 누가(subject), 무슨 자격으로
   (ability), 무슨 결과로(effect)* 행동하는가를 선언하고 컴파일 타임에 검증.
2. **2차 정체성 — A2M(Agent-to-Machine) 인터페이스 언어**: AI 에이전트가
   기계·장비·외부 시스템을 안전하게 통제하기 위한 인터페이스 언어.

---

## 2. 표기 규약

문법 표기는 다음 규약을 따릅니다.

| 표기 | 의미 |
|---|---|
| `NAME`, `type`, `expr` | 비단말(자리표시자) |
| `"keyword"`, `;`, `->` | 리터럴 단말 |
| `[ ... ]` | 선택(0 또는 1회) |
| `{ ... }` | 반복(0회 이상) |
| `a \| b` | 둘 중 하나 |
| `(` `)` | 그룹 |

명시하지 않은 한, 함수 본문의 문장은 `;`로 끝나고, `zone`/`world` 내부의
토폴로지 선언은 `;`를 생략할 수 있습니다.

---

## 3. 어휘 구조

### 3.1 키워드

키워드는 3계층으로 분류되며, 단일 레지스트리
`src/lexer/language_keyword_registry.def`가 소유합니다.

| 클래스 | 의미 | 예 |
|---|---|---|
| **예약어(RESERVED)** | 렉서가 토큰으로 고정 | `func`, `struct`, `intent`, `zone`, `match` |
| **문맥 키워드(CONTEXTUAL)** | 렉서는 식별자로, 파서가 문맥에서 선택 | `action`, `effect`, `authority`, `step`, `who`, `using` |
| **유연 키워드(SOFT)** | 아직 완전히 예약되지 않음 | `current`, `full`, `none` |

**예약어 전체 목록**(알파벳순):

```
ability    async    await    bind    break    case    class    collapse
compensate continue default  defer   dyn      effect   else    enum
event      export   extends  extern  fail     false    for      func
if         impl     import   in      include  innate   intent   let
local      match    namespace nondeterministic   object  override
own        parallel party    private public   ref      reflect  relation
remote     return   role     roster  secure   select   shared   slot
spawn      struct   subject  tobject transaction  true    type     unsafe
use        vessel   where    while   with     world    zone
```

### 3.2 식별자

```
identifier ::= letter { letter | digit }
letter     ::= "A".."Z" | "a".."z" | "_"
```

### 3.3 리터럴

```pergyra
let i: Int = 42;              // 정수 (32비트)
let l: Long = 42L;            // Long (64비트, 'L' 접미사)
let f: Float = 3.14;          // 실수
let s: String = "hello";      // 문자열
let m: String = """멀티
라인""";                      // 멀티라인 문자열
let g: String = f"값={i}";    // 보간 문자열 (f"...")
let h: String = "${i}점";     // 보간 문자열 ("${expr}")
let b: Bool = true;
```

> 문자 리터럴(`'a'`)은 아직 없으며, 문자는 길이 1짜리 `String`으로 취급됩니다
> (`out-of-beta`). 16진/2진/8진 리터럴도 `out-of-beta`입니다.

### 3.4 연산자와 구두점

| 분류 | 연산자 | 비고 |
|---|---|---|
| 산술 | `+` `-` `*` `/` `%` | |
| 비교 | `==` `!=` `<` `<=` `>` `>=` | |
| 논리 | `&&` `\|\|` `!` | |
| 대입 | `=` | |
| 화살표 | `->` 반환 | `=>` 람다 |
| 채널 | `<-` send/recv | `ch <- v`, `<-ch` |
| 파이프 | `\|>` | `xs \|> f` |
| 범위 | `..` | `0..n`, `xs[a..b]` |
| 가변 | `...` | 예약(spread/rest) |
| 옵션 | `??` 코얼레스 | `?.` 예약, `?` 전파 |
| 이벤트 | `+=` 구독 | `-=` 해지 |
| 구두점 | `(` `)` `{` `}` `[` `]` `,` `.` `:` `;` | `@`는 예약 |

> **주의**: `+=` / `-=` 는 복합 대입이 아니라 **이벤트 구독/해지** 연산자입니다.
> 일반 복합 대입(`x += 1`)은 아직 없습니다(의도된 `out-of-beta`).

### 3.5 주석

```pergyra
// 한 줄 주석
/* 여러 줄
   주석 */
/// 문서 주석 (구조화 태그 지원)
```

---

## 4. 프로그램 구조

### 4.1 진입점

```pergyra
func Main() -> Void {
    Log("hello");
}
```

### 4.2 네임스페이스와 모듈

```pergyra
namespace App.Core {
    export func Helper() -> Int { return 1; }
}

import "path/to/module.pgy";
use App.Core;
```

- `export` — 공개 API 표시.
- `public` / `private` — 선언 가시성(안정화 진행 중).

---

## 5. 타입

### 5.1 기본 타입

| 타입 | 설명 |
|---|---|
| `Int` | 32비트 부호 정수 |
| `Long` | 64비트 부호 정수 |
| `Float` | 부동소수점 |
| `Bool` | `true` / `false` |
| `String` | 유니코드 문자열 |
| `Void` | 반환 없음 |

> 암묵적 수치 변환은 없습니다(정밀도 손실 방지). 명시 변환은 `ToInt`/`ToFloat`.

### 5.2 struct — 값 레코드

```pergyra
struct ScorePair {
    left: Int;
    right: Int;
}

let p: ScorePair = ScorePair(70, 20);   // 선언 순서 생성자
```

`struct`는 권한·행위자 의미가 없는 수동(passive) 값 레코드입니다.

### 5.3 class — 수동 유틸리티 클래스

```pergyra
class Greeter {
    func Greet(name: String) -> String {
        return Concat("hi ", name);
    }
}
```

> 도메인 권한·상태를 가진 능동 개체는 `class`가 아니라 `subject`를 사용합니다.

### 5.4 enum — 대수적 데이터 타입(ADT)

```pergyra
enum Result2 { Ok(Int); Err(String); }
```

### 5.5 type 별칭

```pergyra
type UserId = Int;
```

### 5.6 제네릭

```pergyra
func First<T>(xs: Array<T>) -> Option<T> { ... }

// 제약: ability 바운드
func Describe<T>(x: T) -> String where T: Printable { ... }
```

- 바운드: `where T: Ability + Other`(다중 바운드).
- 기본 타입 인자, HKT, comptime, dependent type은 `out-of-beta`/`reject`.

### 5.7 튜플

```pergyra
let pair: (Int, String) = (1, "one");
```

### 5.8 컬렉션 타입

| 타입 | 용도 |
|---|---|
| `Array<T>` | 고정 크기 배열 |
| `List<T>` | 가변 시퀀스 |
| `Queue<T>` | FIFO |
| `Set<T>` | 멤버십 집합 |
| `HashMap<K,V>` / `Map` | 키-값 |

### 5.9 Option / Result

```pergyra
Option<T>       // 없음 또는 값
Result<T, E>    // 성공 또는 실패
```

---

## 6. 변수와 바인딩

### 6.1 let

```pergyra
let x: Int = 1;        // 타입 명시
let y = 2;             // 타입 추론
let mut z: Int = 0;    // 가변 선언
```

### 6.2 디스트럭처링 (위치 기반)

```pergyra
let (a, b) = pair;     // let (a, b) = expr
```

> 명명 필드 디스트럭처링 `let { x } = ...`은 예약되어 거부됩니다.

### 6.3 파라미터 모드

| 모드 | 의미 |
|---|---|
| `own` | 소유권 이전 |
| `ref` | 불변 차용 |
| `&` | 불변 수신자(차용) |
| `inout` | value-result(copy-in/copy-out) 가변 |

> `&mut`은 거부됩니다 — 이 언어의 가변은 live borrow가 아니라 value-result이므로
> `inout`이 유일한 가변 철자입니다.

---

## 7. 표현식

### 7.1 호출·멤버·람다·파이프

```pergyra
obj.Method(arg);            // 멤버 호출
obj.field;                  // 멤버 접근
let f = x => x + 1;         // 람다
let r = xs |> map |> sum;   // 파이프
```

### 7.2 배열·인덱싱·슬라이싱

```pergyra
let xs: Array<Int> = [1, 2, 3];
xs[0];                      // 인덱싱
xs[1..3];                   // 슬라이스 (차용 뷰)
xs[..2];                    // 처음부터
xs[1..];                    // 끝까지
xs.Slice(1, 2);             // 명시 슬라이스
SliceCopy(view);            // 소유 스냅샷
```

### 7.3 맵/셋 리터럴

```pergyra
{ "a": 1, "b": 2 }          // 맵 리터럴
{ 1, 2, 3 }                 // 셋 리터럴
{:}                         // 빈 맵 마커
```

### 7.4 옵션 연산

```pergyra
opt ?? default;             // Option<T> ?? T -> T
expr?                       // 오류 전파 (try/propagate)
```

---

## 8. 문장과 제어 흐름

### 8.1 if / while / for / loop

```pergyra
if cond { ... } else { ... }

while i < n { ... }

for i in 0..n { ... }       // 범위 루프
for v in xs { ... }         // 컬렉션 루프
for event in events { ... } // 리스트 순회

loop { ... }                // 무한 루프
break;
continue;
```

### 8.2 match

```pergyra
match score {
    case 100: return "perfect";
    case 90: return "great";
    default: return "open";
}

// enum/Result/Option 디스트럭처
match result {
    case .Ok(v):  return v;
    case .Err(e): return 0;
}

// 가드와 or-패턴
match x {
    case a | b if a > 0: return a;
    default: return 0;
}
```

> `goto`와 switch fallthrough는 없습니다(CFG 안전성과 충돌, `reject`).

---

## 9. 함수

### 9.1 func

```
func name ( params ) [ "->" type ] [ where_clause ] [ with_clause ] { body }
```

```pergyra
func Clamp(value: Int) -> Int {
    if value < 0 { return 0; }
    return value;
}
```

### 9.2 계약 절

```pergyra
func Read(p: String) -> String where RequiresFile { ... }      // where
func F() -> Void with effects Io { ... }                       // 효과 선언
func G() -> Void with caps Write { ... }                       // 능력 선언
```

### 9.3 호스티드 메서드(self)

```pergyra
subject Account {
    func Balance(self) -> Int { ... }
}
```

---

## 10. 도메인 구성체

Pergyra의 핵심 계층입니다.

| 구성체 | 역할 | 유사 개념 |
|---|---|---|
| `subject` | 상태를 가진 능동 개체 | DDD 집계 루트 |
| `object` | 내부 읽기 투영 | DTO/read model |
| `tobject` | 전송/경계 투영 | transport DTO |
| `vessel` | 내부 상태 컨테이너 | (부분) |
| `ability` | 행동 계약 | 인터페이스/트레이트 |
| `role` | 계약 구현 | trait impl |
| `effect` | 행위가 남긴 도메인 사실 | (도메인 정책) |
| `relation` | 쌍 단위 도메인 연결 | ORM 연관 |
| `party` / `roster` | 참여·합성 단위 | (독자적) |
| `zone` | 자원·권한 경계 | 액터/능력 스코프 |
| `world` | 상위 합성 경계 | 액터 시스템 경계 |
| `intent` | 오케스트레이션 척추 | saga/workflow |

### 10.1 subject와 action

```pergyra
subject Wallet {
    let mut balance: Int;

    action Spend(self, amount: Int) -> Void
        requires Spendable      // 능력 요구
        within PaymentZone      // 경계
        authorized by self      // 승인 주체
        causes Spent            // 발생 효과
    {
        self.balance = self.balance - amount;
    }
}
```

action 계약 절(콜론 없음):

```
requires Ability [, Ability]
within ZoneName
causes EffectName
authorized by subject [, subject]
```

### 10.2 ability와 role

```pergyra
ability Spendable {
    func CanSpend(amount: Int) -> Bool;
}

role WalletSpendable for Wallet {
    impl ability Spendable {
        func CanSpend(amount: Int) -> Bool {
            return amount >= 0;
        }
    }
}
```

### 10.3 effect와 relation

```pergyra
effect Guarded for bearer: Hero { }

relation Ownership {
    owner: Subject;
    owned: Subject;
}
```

### 10.4 zone

```
zone Name { { zone_item } }
```

```pergyra
zone BattleZone {
    subject slot hero: Hero           // 타입 슬롯 (';' 생략 가능)
    effect slot guarded: Guarded
    effect pool events: Event         // 효과 풀
    relation slot link: Ownership
    subjects [a, b]: Hero             // 그룹 슬롯
    authority hero requires Prepared  // 권한 규칙
    forbids unsafe                    // unsafe 금지
    refresh mirror from buyer         // 파생 상태 갱신
}
```

### 10.5 world

```pergyra
world CheckoutWorld {
    zone cart: CartZone
    zone payment: PaymentZone
}
```

world 안에 zone을 내장하면 `Clone(...)`으로 경계 포크를 명시해야 합니다.

### 10.6 intent와 step

```
intent Name ( params ) [ "->" type ] [ "with" retry(n) ] { { intent_item } }
```

intent 선언 절:

```pergyra
intent Checkout(cart: CartZone) {
    exclusive;                // 또는 concurrent
    priority: 10;
    rollback: full;           // full | current | none
    who: buyer;               // 기본 참여자
    where: ShopZone;          // 기본 경계
    involves buyer: Buyer;    // 참여자 바인딩
    with total: Int;          // 값 바인딩

    step Pay after Check {
        using: cart;
        on: buyer.Pay();
        expect: total > 0;
    }

    success: complete;
    failure: rollback;
}
```

step 절(콜론 사용):

| 절 | 의미 |
|---|---|
| `where:` | 경계 타입 |
| `who:` | 참여자 |
| `using:` | zone 인스턴스 |
| `intent:` | 하위 intent |
| `transfer:` / `move ... to` | 전송 |
| `on:` | 액션 호출(결과 바인딩 선택) |
| `compensate:` | 보상 액션 |
| `success:` / `failure:` | 결과 분기 |
| `pre:` / `post:` | 사전/사후 조건 |
| `guard:` | 실행 중 안전 조건 |
| `invariant:` | 불변식 |
| `requires:` | 능력 요구 |
| `authorized by:` | 승인 주체 |
| `causes:` | 발생 효과 |
| `expect:` | 검증 기대값 |

> **압축(compact)**: `who`/`within`/`requires`/`causes`/`authorized by`는
> 액션·zone 계약에서 파생되면 생략할 수 있습니다. 컴파일러는 명시된 증거만
> 검증하며, 목표 문장에서 정책을 "발명"하지 않습니다.

### 10.7 intent 작성 루프 (인간/AI)

Pergyra intent는 인간이 읽고 AI가 채울 수 있게 설계됩니다.

```pergyra
// 최소 제안
intent Login for User in AuthZone {
    on: user.Validate(credentials);
    on: session.Issue(user);
    expect: session.active;
}
```

컴파일러가 불완전하면 거부하고 근거를 제시합니다:

```text
NO
Reason:
- step `session.Issue(user)` writes SecureSlot<SessionToken>
- AuthZone does not provide TokenIssuer authority for that write
Fix:
- add `authorized by TokenIssuer`
- or move token issuing into a zone that owns that authority
```

루프: `인간 목표 → AI 채운 intent → 컴파일러 YES/NO → Reason/Fix → 패치`.

---

## 11. 자원과 소유권

### 11.1 Slot 모델

포인터의 역참조 대신, 자원은 **점유권(claim)** 으로 다룹니다.

```text
Slot이 어디 있는지는 모른다. 존재한다는 것만 안다.
Claim / Read / Write / Release
```

| 개념 | 포인터 모델 | Slot 모델 |
|---|---|---|
| 정체 | 메모리 주소 | 자원 핸들 |
| 읽기 | `*ptr` | 점유 상태에서 `Read(slot)` |
| 쓰기 | `*ptr = v` | `Write(slot, v)` |
| 해제 | `free(ptr)` | `Release(slot)` |
| 복사 | 별칭 발생 | 금지(단일 소유) |
| 전제 | 주소를 앎 | 점유했을 때만 접근 |

### 11.2 Slot 계열

| 타입 | 설명 |
|---|---|
| `Slot<T>` | 로컬 anchored 자원 핸들 |
| `SecureSlot<T>` | 토큰(권한) 기반 접근 |
| `DeviceSlot<T>` | 디바이스 자원 |
| `ReadView<T>` / `WriteView<T>` | 스코프 타입 뷰 |
| `MoveToken<T>` | 이동 전용 토큰 |
| `QubitSlot` | (v2 계획) |

```pergyra
with SecureSlot<File>(ReadOnly) as f {
    let data = Read(f);
}
```

### 11.3 소유·참조 수식자

```pergyra
own x: T      // 소유권 이전
ref x: T      // 불변 차용
shared        // 공유
secure        // 권한 기반
inout         // value-result 가변
```

### 11.4 Clone — 경계 포크

```pergyra
let cart: CartZone = CartZone(Clone(buyer));
```

경계를 넘는 복사는 `Clone(...)`으로 명시해야 하며, 없으면 거부됩니다.

### 11.5 defer / pin / unsafe

```pergyra
defer cleanup();          // 스코프 종료 시 정리 (MIR cleanup 소유)
pin slot;                 // 자원 고정
unsafe { ... }            // 경계 표시 (Slot/권한 우회 수단이 아님)
```

---

## 12. 동시성과 비동기

### 12.1 async / await

```pergyra
async func Fetch() -> String {
    let data = await RemoteCall();
    return data;
}

let block = async { await Work(); };
```

### 12.2 spawn / parallel

```pergyra
spawn Worker();                 // 작업 실행
spawn blocking HeavyJob();      // 블로킹 스레드 풀
spawn async () { ... };         // 익명 async

parallel {                       // 다중 arm
    { TaskA(); }
    { TaskB(); }
}

parallel (x in xs) join with all {      // 데이터 병렬 join
    Process(x);
}

parallel (i in lo..hi) join with sum {  // reduce 조합자
    Compute(i);
}
```

join 조합자: `all`(기본), `any`, `sum`, `product`, `min`, `max`.

### 12.3 Channel과 select

```pergyra
let ch: Channel<Int> = Channel(4);   // 용량 4 생성
ch <- 42;                            // send
let v: Int = <- ch;                  // receive

// 비차단/타임아웃 보조 (Option 반환)
let got: Option<Int>  = TryRecv(ch);
let timed: Option<Int> = RecvTimeout(ch, 1000000);     // 마이크로초
let ok: Option<Bool>  = TrySendStatus(ch, 5);
let sent: Option<Bool> = SendTimeoutStatus(ch, 5, 1000000);

select {
    case msg = <-ch: Log(msg);       // 수신 + 바인딩
    case <-ch:       Log("recv");    // 수신만
    default:         Log("none");
}
```

### 12.4 Future / 취소

```pergyra
Future<T>          // 로컬 future
RemoteFuture<T>    // 원격 작업 결과 (await -> Result<T>)
Cancel(future)     // 취소
```

> 로컬 `Future<T>`는 `await -> T`, 원격 `RemoteFuture<T>`는 `await -> Result<T>`.
> `DeviceSlot`의 원격 읽기는 `SubmitDeviceRead(slot)` → `RemoteFuture<T>`.

---

## 13. 오류·보상 처리

Pergyra는 예외가 아니라 **Result/실패 계약** 중심입니다.

```pergyra
let r: Result<Int, Err> = Work();
let v = r ?? 0;              // Option 코얼레스

transaction { ... }          // 트랜잭션
compensate Undo();           // 보상
rollback: full;              // 롤백 정책
retry(3);                    // 재시도
defer cleanup();             // 정리
fail;                        // 명시 실패
```

> `throw`/`try-catch`는 없습니다(의도적 `reject`). 복구 가능한 실패는 `Result`,
> 하드 실패는 런타임 경계에서 처리됩니다.

---

## 14. 표준 라이브러리 표면

### 14.1 스칼라 수학/변환

```
Sqrt Pow Floor Ceil Abs Min Max Clamp
Sin Cos Tan Asin Acos Atan Atan2 Round Exp MathLog Log10 Log2
ToInt ToFloat ToString
PI E
```

### 14.2 문자열

```
Concat StringConcat Substring StringTrim StringReplace
ToUpper ToLower
```

### 14.3 컬렉션

| 컬렉션 | API |
|---|---|
| Array | `ArrayLength`, `xs[i]` |
| List | `ListNew` `ListPush` `ListGet` `ListSet` `ListRemove` `ListSize` |
| Queue | `QueueNew` `QueuePush` `QueuePop` `QueueSize` `QueueEmpty` |
| Set | `SetNew` `SetAdd` `SetHas` `SetRemove` `SetSize` `SetValues` |
| Map | `MapNew` `MapSet` `MapGet` `MapHas` `MapRemove` `MapSize` `MapKeys` |

### 14.4 I/O·시간·난수

```
Print Log LogBanner
FileExists ReadFile FileOpen FileWrite FileRead FileClose
Now Sleep SeedRandom Random
```

---

## 15. 문법 표면 상태

각 패턴의 구현 상태(레전드)는 다음을 따릅니다.

| 상태 | 의미 |
|---|---|
| `stable` | 구문→의미→런타임→C/LLVM→진단→회귀 전부 합의 |
| `partial` | 일부 구현, 전 owner 미완결 |
| `out-of-beta` | 유용하지만 베타 범위 밖 |
| `native-different` | Pergyra 고유의 다른 답 존재 |
| `reject` | 의도적으로 배제 |
| `reserved` | 문법/토큰 예약, 미구현(fail-closed) |

주요 표면 요약:

| 패턴 | Pergyra | 상태 |
|---|---|---|
| 함수 | `func f(...) -> T` | `stable` |
| 변수 | `let`, `let mut` | `stable` |
| 값 레코드 | `struct` | `stable` |
| 유틸리티 클래스 | `class` | `stable` |
| 능동 도메인 개체 | `subject` | `stable` |
| 읽기 투영 / 전송 투영 | `object` / `tobject` | `stable` |
| ADT | `enum` | `stable` |
| 계약 | `ability` / `role` | `stable` |
| 효과 / 관계 | `effect` / `relation` | `stable` |
| 경계 | `zone` / `world` | `stable` |
| 오케스트레이션 | `intent` / `step` | `stable` |
| 제네릭 | `T`, `where T: Ability` | `stable` |
| Option/Result | `Option<T>` / `Result<T,E>` | `stable` |
| 패턴 매칭 | `match` / `case` / guard / or-pattern | `stable` |
| 제어 흐름 | `if` `while` `for` `break` `continue` | `stable` |
| async/await/spawn | `async` `await` `spawn` | `stable` |
| parallel | `parallel` / join form | `stable` |
| 채널 | `Channel<T>` send/recv | `stable` |
| select | `select` | `partial` |
| 이벤트 | `event`, `+=`/`-=` | `partial` |
| 람다 | `=>` | `partial` |
| 파이프 | `\|>` | `partial` |
| 튜플 | `(T, U)` | `partial` |
| 타입 별칭 | `type Name = T` | `partial` |
| 디스트럭처링 | `let (a, b) = ...` | `partial` |
| 슬라이싱 | `xs[a..b]` | `partial` |
| 문자열 보간 | `f"..."` / `"${...}"` | `partial` |
| 맵/셋 리터럴 | `{k:v}` / `{v}` | `partial` |
| FFI | `extern` | `partial` |
| 취소 | `Future<T>` / `Cancel` | `partial` |
| 보상/롤백 | `compensate` `rollback` `retry` | `partial` |
| 복합 대입 | (없음; `+=`/`-=`는 이벤트) | `out-of-beta` |
| 캐스트/타입테스트 | `as` / `is` | `reserved` / `out-of-beta` |
| 명명/기본 인자 | `f(x:1)` / `f(x=1)` | `reserved` |
| 옵셔널 체인 | `?.` | `reserved` |
| 예외 | `throw`/`try-catch` | `reject` |
| goto/fallthrough | — | `reject` |
| 매크로 | — | `reject` |

---

## 16. 의미론

Pergyra의 의미론은 크게 다섯 축으로 구성됩니다. 이 절은 각 축이 **무엇을
의미하고, 어떻게 형식화되는지**를 설명합니다. 정형 의미론의 권위는
`docs/semantics/proofs/`의 Coq/Rocq 코퍼스이며, 아래 기술은 그 투영입니다.

### 16.1 의미 축 (Semantic Axes)

키워드는 5개의 의미 축으로 분류됩니다(레지스트리의 `PGY_KEYWORD_AXIS_*`).

| 축 | 의미 | 대표 키워드 |
|---|---|---|
| `TYPE_CONTRACT` | 타입·계약 | `ability` `class` `struct` `enum` `func` `impl` `role` `where` `extends` `override` `innate` `dyn` `reflect` |
| `DOMAIN` | 도메인(누가/어디서/무엇을) | `subject` `action` `zone` `world` `intent` `effect` `authority` `party` `relation` `event` `slot` |
| `EXECUTION` | 실행(언제/어떻게) | `async` `await` `spawn` `parallel` `transaction` `compensate` `rollback` `defer` `retry` `select` |
| `RESOURCE` | 자원(점유/경계) | `own` `ref` `shared` `secure` `inout` `local` `capacity` `collapse` `pin` |
| `GENERAL` | 일반 계산 | `let` `match` `if` `while` `for` `in` `case` `default` `true` `false` |

이 축 분리는 "같은 단어가 여러 의미를 몰래 겹치지 않게" 하기 위한
직교성(orthogonality) 장치입니다. 각 키워드는 정확히 하나의 축을 선언합니다.

### 16.2 연산 의미론 (Operational Semantics)

핵심 추상 기계는 `PergyraCore.v`가 정의하는 **레이블 전이 시스템**입니다.

구성(configuration):

```text
config ::= ( actor, holdings, here, elog, store )

actor    : principal                    -- 현재 행위자
holdings : principal -> list cap        -- 권한 분포
here     : zone                         -- 존 거주
elog     : list effect_log_entry        -- 효과 로그 (각 항목은 pre-effect store 스냅샷)
store    : slot -> lcstate              -- 슬롯 타입상태 (Empty | Filled | Released)
```

액션(action) 형태와 스텝 관계:

```text
action ::= Cross z' | Emit e | Acquire s | Use s | Release s
         | Delegate b k | Rollback
```

각 스텝 규칙은 **능력(capability)과 타입상태(typestate)로 게이트**되어
구성상 fail-closed입니다:

| 규칙 | 전제 | 효과 |
|---|---|---|
| `SCross` | `has_cap c (gz z')` | zone 이동 |
| `SEmit` | `has_cap c (ge e)` | 효과 로그에 기록 |
| `SAcquire` | `has_cap c (ga s)` ∧ `store s = Empty` | `Empty → Filled` |
| `SUse` | `store s = Filled` | 무상태 변화 |
| `SRelease` | `store s = Filled` | `Filled → Released` |
| `SDelegate` | `has_cap c k` | 권한 위임 |
| `SRollback` | 로그 헤드 ∧ `Forall (has_cap) (ct e)` | 보상 대상 복원 |

다중 스텝 `steps`는 `step`의 반사·전이 폐포입니다. 이 기계가 "권한 없는
전이"를 애초에 유도하지 못한다는 사실이 Pergyra 도메인 의미의 뼈대입니다.

### 16.3 Slot 능력 계산 (Slot Capability Calculus)

`SlotCalculus.v`가 자원·권한·세대(generation)를 형식화합니다.

```text
SlotId / Generation / Token / Value
Heap   = SlotId -> option Slot      (None = 미할당/해제)
CapEnv = Token -> bool              (능력 환경)
Slot   = ( value, generation, pin )
Handle = ( slot_id, generation )    -- 세대 검사 = 댕글링 탐지
```

스텝 규칙(각각 토큰 검증 + pin 상태 전제):

| 규칙 | 전제 |
|---|---|
| `Claim` | fresh id + `verify_token(..., Claim)` → gen=1, Unpinned |
| `Read` | `verify_token(..., Read)` — 상태 무변화 |
| `Write` | `verify_token(..., Write)` — gen/pin 유지 |
| `Pin` | `verify_token(..., Pin)` ∧ Unpinned → Pinned |
| `Unpin` | Pinned → Unpinned |
| `Release` | `verify_token(..., Release)` ∧ **Unpinned** → None |

핵심 정리(커널 검증됨):

- **`stale_handle_*_impossible`** — 세대 불일치 핸들은 읽기/쓰기/해제 불가
  (use-after-free 방지).
- **`unissued_token_*_impossible`** — 미발급 토큰으로는 접근 불가
  (능력 기반 보안).
- **`pin_non_eviction`** — pin된 슬롯은 어떤 Step 규칙으로도 해제/퇴거 불가
  (6개 규칙 전수 사례분석).
- **`released_slot_*_impossible`** — 해제된 슬롯은 접근 불가.

의미론적으로 Slot은 "주소를 아는 것"이 아니라 "권한을 가진 것"으로 접근을
결정합니다. 이는 세마포어의 P/V 추상화를 메모리/상태 관리에 적용한 것입니다.

### 16.4 소유권 의미론

Pergyra는 **Rust식 라이프타임 프로그래밍을 하지 않습니다.** 비즈니스 객체
수명을 정적으로 전부 예측하려 하지 않습니다.

- **Slot = 자원 경계**: 정적 검사는 안전하지 않은 경계 전이를 거부하고,
  런타임 핸들은 세대(generation)·토큰(token)·자원 상태를 검증합니다.
- **복사 금지, 단일 소유권**: `Claim/Release` 중심, 이동 중심.
- **경계 포크는 명시적**: 경계를 넘는 복사는 `Clone(...)`으로만 가능.

이것은 "빠진 Rust borrow checker"가 아니라 **의도된 설계 선택**입니다.

### 16.5 효과·권한 의미론

- **effect**: 행위가 일으킨 도메인 사실. `causes EffectName`으로 선언되고,
  zone의 `effect slot`/`effect pool`이 그 수명을 소유합니다.
- **authority**: 누가 무엇을 승인하는가. `authorized by subject`와 zone의
  `authority ... requires`가 권한을 계층화합니다.
- **능력(capability)**: 접근은 주소가 아니라 보유 능력/토큰으로 결정됩니다.
- **fail-closed**: 증거가 없으면 컴파일 타임에 거부하거나 런타임에 하드 실패.
  "조용한 기본값"은 없습니다.

### 16.6 IR 의미 소유 (Semantic Ownership)

의미는 단일 트리를 복사한 pass 사슬이 아니라, **직교하는 fact 그래프**가
축별로 소유합니다.

```text
Source -> Lexer -> Parser(AST)
  -> TypeDag (타입/제네릭/능력 fact)
  -> HIR     (의미 개체)
  -> DIR     (도메인·목적 관계)
  -> RIR     (자원·권한·전송 fact)
  -> MIR     (실행·CFG·SSA·소유권·정리 fact)   -- CPU-family 투영층
  -> AIR     (증거/증명서, 기계 중립 fact층)   -- 검증 전용, 코드젠 IR 아님
  -> VerifiedProjectionPlan -> C | LLVM
```

- **AIR = epsilon 격리층**: 추상화 하강에서 불가피한 손실(epsilon-loss)을
  경계에서 격리하고, 허용된 손실마다 명시적 증거를 요구합니다. 의미적 진실은
  소유 계층(CFG/DAG/RIR/MIR/ABI)에 남습니다.
- 백엔드는 소스 텍스트·AST·이름·Slot 모양에서 의미를 복구하지 않습니다.
  동일한 owner fact를 C와 LLVM이 peer projection으로 소비합니다.

### 16.7 정형검증 (Formal Verification)

`docs/semantics/proofs/`의 Coq/Rocq 코퍼스가 위 의미론의 일부를 기계 검증합니다.

- **56개 증명**이 `coqchk`(신뢰 커널)로 검증됨, `Admitted` 0.
- **공리 예산 정확히 2개** — `SlotCalculus.verify_token`, `SlotCalculus.MaxSlotId`
  (의도된 추상; 그 외 `Axiom`/`Admitted`는 게이트가 거부).
- type-in-type·unsafe (co)fixpoint·positivity 가정 전부 `<none>`,
  `Set is predicative`.

**증명된 것**: Slot 메모리 모델의 안전성(세대 불일치 접근 불가, 미발급 토큰
접근 불가, pin 비퇴거, 해제 슬롯 접근 불가).

**의도적으로 증명하지 않은 것**(별도 proof obligation): Rust식 borrow checking,
aliasing-XOR-mutability, lexical no-escape, async/task 경계 안전성, CFG cleanup
삽입. 이 범위는 `SlotCalculus.v` 헤더가 명시합니다.

### 16.8 기계 중립 의미론

C와 LLVM은 **첫 번째 검증 투영**이지 최종 실행 존재론(ontology)이 아닙니다.

- 장기적 진실 소유자는 fact 파이프라인입니다: `intent`, `effect`, `authority`,
  `coordination`, `slot`, `world`, `zone`은 AIR/MIR/ABI owner fact로 살아남아야
  합니다.
- 미래 NPU/텐서/데이터플로우/이벤트 기반 백엔드는 새 소스 언어가 아니라 **동일한
  owner fact**를 소비해야 합니다. 백엔드가 수용 불가하면 거부가 fact 기반으로
  드러나야 합니다.
- 이 계약은 `docs/semantics/18_machine_neutral_compute.md`가 소유합니다.

---

## 참고 문서

- [grammar/README.md](README.md) — 컴팩트 문법 맵
- [grammar/syntax_units/README.md](syntax_units/README.md) — 문법 단위별 설명
- [docs/124_syntax_pattern_matrix.md](../docs/124_syntax_pattern_matrix.md) — 타 언어 패턴 대조표
- [docs/00_vision.md](../docs/00_vision.md) — 언어 비전
- [docs/104_air_compiler_architecture.md](../docs/104_air_compiler_architecture.md) — AIR/증거 아키텍처
- [docs/semantics/proofs/PergyraCore.v](../docs/semantics/proofs/PergyraCore.v) — 추상 기계(권위)
- [docs/semantics/proofs/SlotCalculus.v](../docs/semantics/proofs/SlotCalculus.v) — Slot 계산(권위)
- [src/lexer/language_keyword_registry.def](../src/lexer/language_keyword_registry.def) — 키워드 레지스트리(권위)
