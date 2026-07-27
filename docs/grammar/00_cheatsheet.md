# Pergyra 문법 한눈에 (Cheat-Sheet)

> 헷갈리면 **여기부터**. 깊은 규칙·예외는 [01_syntax.md](01_syntax.md), 형식 문법은
> [02_grammar.md](02_grammar.md), 네이밍은 [03_naming.md](03_naming.md),
> 구성체 선택은 [../200_object_to_action_boundary_patterns.md](../200_object_to_action_boundary_patterns.md).
> 이 문서는 *현재 파서가 실제로 받는 것*을 요약한다(미래 계획은 맨 아래 §9).

## 0. 항상 참인 6가지

- 블록은 `{ ... }`, **정규 표기는 BSD/Allman** (formatter 기준; 파서는 K&R도 읽음).
- 키워드 = **소문자**. 타입·내장 API = **PascalCase**. 필드·로컬·파라미터 = **camelCase**.
- identity 타입(`subject` `relation` `effect` `zone` `world`) = 함수 인자로 **자동 참조 전달**.
  canonical value 타입(`struct` `object` `tobject` `class` `vessel`) = **복사 전달**.
  단, hosted receiver는 별도 축이다. `subject`와 `vessel`의 `self`는
  pointer-self이고 `class`/`object`/현재 `tobject`의 `self`는 value-self다.
  현재 C/LLVM은 일반 vessel 파라미터까지 포인터로 내리는 알려진 결함이 있어,
  vessel의 복사 전달은 아직 실행 보장으로 쓰면 안 된다.
- doc comment `/// @effects ...` 를 파서가 읽는다.
- declaration 이름은 **일반 식별자만**(예약어 재사용 불가).
- 선언 토큰은 lexer에서 이미 분리됨 — `subject`/`class`, `struct`/`object`/`tobject` 등은 *별도 토큰*(alias 아님).

## 1. ★ 세미콜론 규칙 (가장 헷갈리는 것)

**기본: 전부 세미콜론 `;`.** 예외는 *딱 한 패밀리*뿐:

> **`zone` / `world` / `effect` / `relation` 본문의 *사실 선언*은 `;` 없음.**
> (slot / state / apply / refresh / maintain / authority / link / activate ...)

그 외 전부 `;` 가 붙는다 — **`intent` step clause(`where: X;`)도 포함**.

| 위치 | 종결 |
|---|---|
| func / action / method 본문 (코드 문장) | `;` |
| `subject` / `class` 필드 (`let x: Int;`) | `;` |
| `enum` / `struct` / `object` / `tobject` 멤버 | `;` (enum variant는 `,`) |
| `intent` 헤더 + step clause (`priority: 5;`, `where: Z;`) | `;` |
| **`zone` / `world` / `effect` / `relation` 본문 사실선언** | **없음** |
| 도메인 블록 안의 `func` 본문(코드) | 코드라서 `;` |

기억법: **"zone·world·effect·relation 본문만 `;`이 없다."** 나머지는 다 `;`.

**왜 없는가 (이걸 알면 안 헷갈림):** 이 4개는 *순차 문장*이 아니라 **"상위 세계"를 기술하는
사실 집합**이다. `;` 는 "이것이 *일어난다*"(imperative 순서)를 뜻하고, 무종결은 "이것이
*이다*"(declarative 세계)를 뜻한다. 즉 **구두점이 register를 표시**한다 — code 층(`;`) vs
세계 층(무종결). Pergyra의 "구문이 도메인 의미를 담는다" 원칙이 세미콜론에까지 적용된 것이라,
*버그가 아니라 의도된 신호*다. `zone`/`world`/`effect`/`relation` 본문에서 `;`을 보면 "코드를
세계 선언으로 잘못 쓴 것"이고, 그 반대도 마찬가지다.

> 현재 *파서는* world 본문의 `;`을 **관용적으로 받아준다**(거부 안 함). 즉 위 규칙은 지금은
> *canonical 스타일*이지 hard 파서 규칙이 아니다. authored 예제는 무종결을 따르고,
> `grammar-cheatsheet-contract-test-smoke` 게이트가 tests/ 예제에서 register를 강제한다. 파서가
> 직접 거부하게 하는 것(register를 load-bearing으로)은 §9의 미래 단계.

## 2. 선언 스켈레톤 (한 줄씩)

```text
enum E       { A, B, C }                              // variant = 콤마
struct S     { x: Int; y: Int; }                      // value, 복사
object O     { x: Int; }                              // local read-only view; query func만
tobject T    { x: Int; }                              // immutable boundary DTO; canonical method-free
class  C     { let x: Int;  func F(self, n: Int) -> Int { ... } }   // 수동 도구
subject P    { let x: Int;
               action A(self, n: Int) -> Void within Z authorized by self { ... }
               func   F(self) -> Bool { ... } }       // 능동 주체
vessel  V    { let x: Int;  func F(self) -> Int { ... } }   // canonical: 인자는 value, hosted self는 pointer-self; 현재 param ABI 결함 있음
ability Ab   { fields x: Int;  func M() -> Bool; }    // 계약: 요구 필드 + 요구 메서드(시그니처만)
role  R for P { impl ability Ab { func M() -> Bool { ... } } }
effect Eff for bearer: P { object slot v: O   refresh v from bearer }     // 본문 ; 없음
relation Rel for a: P, b: P { object slot s: O   refresh s from a }       // 본문 ; 없음
zone  Z      { subject slot s: P   state st: effect e on s   apply e to s by s
               authority s   func ShowState(self) -> Void { ... } }       // 사실선언 ; 없음
world W      { zone z: Z   state active: zone z   activate z
               func ShowWorldState(self) -> Void { ... } }                // 사실선언 ; 없음
intent I(z: Z, p: P) { exclusive; priority: 5; rollback: full;
               step S { where: Z; using: z; who: p; on: ...; guard: ...; post: ...; } }  // ; 있음
event Ev(price: Int);
func F<T>(x: T) -> R where T: Ability { ... }
```

## 3. clause 패밀리 (어느 단어가 어디서)

세 묶음이 *서로 다른 위치*에 산다. 같은 개념이 아니라 *다른 역할*임:

| 묶음 | 위치 | 단어 | 형태 |
|---|---|---|---|
| **action clause** | `action` 시그니처 | `within Z`, `requires Ab`, `authorized by p`, `causes e` | 키워드, **선택적**, 순서 자유 |
| **intent step clause** | `intent` step 본문 | `where: Z;`, `who: p;`, `using: x;`, `on: ...;`, `guard: ...;`, `pre:`, `post:`, `expect:` | `라벨: 값;` |
| **type clause** | 제네릭 시그니처 | `where T: Ability` | 타입 제약 |

> **`within` (action) ≠ `where:` (step).** `within`은 action의 *선택적 zone 계약 fact*,
> `where:`는 step의 *위치 사실*(선언 데이터). 역할이 다르므로 철자가 다른 게 맞다.

## 4. 필드: 저장 vs 계약

| 쓰임 | 키워드 | 의미 |
|---|---|---|
| `subject`/`class`/`struct` 멤버 | `let x: Int;` | **실제 저장**(인스턴스가 *소유*) |
| `ability` 멤버 | `fields x: Int;` | **요구 필드 계약**(구현자가 *노출*해야 함 — 저장 아님) |

둘은 *다른 ownership 질문*이라 어휘가 다르다(정당). 단 `fields`(bare 복수명사)는 철자가 불규칙
— §9의 규칙화 후보.

## 5. 가변성 surface (4좌표, flatten 금지)

| 표기 | 의미 |
|---|---|
| `let x` | 바인딩(나중 변경이 의도라고 약속 안 함) |
| `let mut x` | 로컬/필드 변경 *의도*를 소스에 기록 |
| `inout p` | 값-결과 caller-visible 변경(라이브 borrow 아님) |
| `slot` / `pin ... as view` | 자원·권한 변경(런타임 안전 경계) |

## 6. 슬롯 / view (요약)

```text
with slot<Int> as counter { Write(counter, 100); Log(Read(counter)); }   // 정적 증명 경로
let (h, token) = ClaimSecureSlot<Int>(SECURITY_LEVEL_HARDWARE);          // 토큰-게이트 슬롯
pin slot as view: ReadView<T> { ... }                                    // typed lease
```

## 7. 제어 / 병렬 (요약)

```text
if c { } else { }      while c { }      for x in xs { }
match v { case A: ...   case B(p): ...   default: ... }
parallel { ... }       let h = spawn F(args); await h;
select { ... }
```

`channel`은 예약어로 등록돼 있지만 아직 파서가 읽지 않는다. `channel<Int> ch;`는
현재 파스되지 않으므로 위 목록에서 뺐다 — 상태는
`docs/199_language_word_and_dogfood_grammar.md` §5가 소유한다.

## 8. 능력 게이트 / effect 선언

```text
func ReadConfig() with caps io_read { ... }     // runtime capability 선언
func ObserveClock() with caps clock { ... }     // clock도 capability
func DoWork() with effects io, alloc { ... }    // compiler effect mask 선언
```

## 9. 규칙화 추적 (아직 — 미래 surface-breaking window)

현재는 위가 *실제*다. 아래는 일관성 감사(2026-06-22)에서 합의된 *방향*이며, 적용 전까지는
위 규칙이 진실이다:

- **세미콜론 = 규칙화 대상 아님(원칙으로 확정).** §1의 register(`;`=code 층, 무종결=세계 층)는
  *의도된 신호*이므로 flatten하지 않는다. 게이트는 "통일"이 아니라 *register 위반*을 잡는다:
  `zone`/`world`/`effect`/`relation` 본문의 사실선언에 `;`이 붙으면 = 세계 층을 code로 오용 →
  `grammar-cheatsheet-contract-test-smoke` 게이트가 authored 예제(tests/)에서 거부.
  **미래 단계**: 파서가 world 본문의 `;`을 직접 *거부*하게 해 register를 load-bearing으로
  승격(현재는 파서가 관용 허용). 그 전까지는 게이트가 스타일을 지킨다.
- **`fields` → `require`**: `ability { require x: Int; }` (또는 `require let x: Int`)로 규칙화.
  내부 AST가 이미 `AST_REQUIRE_FIELD`. `let`로 통일하지 *않음*(계약≠저장 신호 보존).
  [docs/134](../134_language_surface_hygiene.md) "allowed debt must be named" 하에 추적.

> 일관성 원칙(이걸로 미래 drift 판정): **같은 *개념*이 다른 *철자/구두점* = 버그.
> 다른 *축*이 다른 어휘 = 직교성(정상).**
