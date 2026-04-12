# PergyraLang 실제 사용 Pain Point 보고서

작성일: 2026-04-12
마지막 업데이트: 2026-04-12 (P0-1, P0-2, P0-3 수정 완료)
검증 방법: 실제 .pgy 파일 작성 → 컴파일 → 실행 → 오류/경고 분석

---

## 수정 완료 항목

### ✅ P0-1: `for-in`이 Array에서 `.count` 대신 `.length`를 참조해야 함

**수정 파일:** `src/codegen/transpiler.c` (line 3586-3611)
**변경 내용:** Array는 `.length`, List는 `.count`를 사용하도록 타입별 분기 추가
**검증:** `for item in array` 컴파일/실행 성공

### ✅ P0-2: `Split()` / `Join()` — 런타임 함수 없음

**수정 파일:** `src/runtime/pgy_runtime.h` (StringSplit/StringJoin 추가)
**변경 내용:** `StringSplit(s, delim) → PgyArray_String`, `StringJoin(arr, sep) → char*` 구현
**검증:** `Split("a,b,c", ",")` → 3개 요소, `Join(parts, " | ")` → "a | b | c"

### ✅ P0-3: `None` 심볼 미정의 — `Option<T>`의 None 사용 불가

**수정 파일:** 
- `src/semantic/type_checker.c` — AST_IDENTIFIER에서 `None` 특별 처리 추가
- `src/semantic/type_system.c` — `Option<unknown>` → `Option<T>` 할당 허용
- `src/codegen/transpiler.h` — `expected_type` 필드 추가
- `src/codegen/transpiler.c` — let 선언 시 target type 설정
- `src/codegen/transpiler_expr_emitters.inc` — None emitter가 expected_type 확인

**변경 내용:** 
- `let x: Option<String> = None;` 시맨틱 검증 통과
- `Option<unknown>`가 모든 `Option<T>`에 할당 가능
- 코드 생성 시 `None_String()`, `None_Int()` 등 올바른 타입 생성

**검증:** `Option<Int>`와 `Option<String>` 모두 `None` 할당 성공

---

### ✅ P1-7: `struct`/`subject` 선언 시 26개의 warning 노이즈

**수정 파일:** `src/codegen/transpiler.c` (2개 위치)
**변경 내용:** `PGY_SLOT_DEFINE`/`PGY_SECURE_SLOT_DEFINE`/`PGY_BOX_DEFINE` 매크로 생성 전후에 `#pragma GCC diagnostic push/pop`으로 `-Wunused-function` 경고 억제
**검증:** struct 2개 + subject 1개 선언 시 **0 warning** (이전: 26개)

---

## 현재 미수정 항목

### 1. enum match에서 OR 패턴이 variant에서 동작 안 함

**현상:**
```pergyra
match dir {
    case North | South:    // Error: OR patterns currently support only literal/simple expression cases
        Log("vertical");
    case East | West:
        Log("horizontal");
}
```

OR 패턴은 리터럴 Int에는 동작하지만 enum variant에는 동작 안 함.

**영향:** match의 유용성이 반감. case를 일일이 나열해야 함.

---

### 2. `Split()` / `Join()` — 시맨틱은 인식, 런타임에 함수 없음 **(수정 완료 ✅)**

~~**현상:**~~
~~`StringSplit()`/`StringJoin()`을 emit하지만 런타임에 함수가 없음.~~

**에러:**~~
~~`error: call to undeclared function 'StringSplit'`~~

**수정 완료:** `pgy_runtime.h`에 `StringSplit`/`StringJoin` 구현 추가

---

### 3. `None` 심볼 미정의 — `Option<T>`의 None 사용 불가 **(수정 완료 ✅)**

~~`let nothing: Option<String> = None;  // Undefined symbol 'None'~~

**수정 완료:** `type_checker.c`에서 `None` 식별자 특별 처리, `type_system.c`에서 `Option<unknown>` → `Option<T>` 할당 허용, 코드젠에서 `expected_type` 기반 타입 해결

---

### 4. enum match에서 OR 패턴이 variant에서 동작 안 함

**현상:**
```pergyra
match dir {
    case North | South:    // Error: OR patterns currently support only literal/simple expression cases
        Log("vertical");
    case East | West:
        Log("horizontal");
}
```

OR 패턴은 리터럴 Int에는 동작하지만 enum variant에는 동작 안 함.

**영향:** match의 유용성이 반감. case를 일일이 나열해야 함.

---

### 5. match exhaustive checker 버그 — 모든 case를 작성해도 "missing cases" 오류

**현상:**
```pergyra
match dir {
    case North: return "North";
    case South: return "South";
    case East:  return "East";
    case West:  return "West";
}
```
```
error: Non-exhaustive match for 'Direction'; missing cases: North, South, East, West
```

모든 4개 case를 명시했는데도 누락 오류. exhaustive checker가 variant 이름을 인식 못 함.

---

### 6. `defer` 블록 스코프 버그 — 변수가 보이지 않음

**현상:**
```pergyra
let resource: Slot<Int> = ClaimSlot<Int>();
defer { Release(resource); };
```
생성된 C 코드에서 `resource`가 defer 블록 scope에서 보이지 않음.

**에러:**
```
error: use of undeclared identifier 'resource'
```

**영향:** defer가 실제로 동작 안 함. 수동 Release 필요.

---

## P1 — 심각한 사용성 문제

### 7. `struct`/`subject` 선언 시 26개의 warning 노이즈

**현상:**
단순 struct 2개 선언 시 26개의 "unused function" warning 발생.
```
warning: unused function 'pgy_claim_Vec2'
warning: unused function 'pgy_write_Vec2'
warning: unused function 'pgy_read_Vec2'
... (각 타입당 13개 × 2타입 = 26개)
```

**원인:** 모든 `struct`/`subject`에 대해 `PGY_SLOT_DEFINE`, `PGY_SECURE_SLOT_DEFINE`, `PGY_BOX_DEFINE` 매크로가 Slot/SecureSlot/Box 함수를 무조건 생성하지만, 실제 값 타입으로 사용할 때는 이 중 어느 것도 호출되지 않음.

**영향:** 
- 정상적인 코드에서도 warning이 쏟아져 실제 문제를 찾기 어려움
- CI에서 warning을 error로 설정하면 빌드 자체가 막힘
- "hello world" 레벨의 코드에서 26 warning은 언어 첫인상을 치명적으로 훼손

---

### 8. 컬렉션 생성 API 혼란 — `Array()`, `ListNew()`, `HashMap()` 중 뭐가 맞나?

**현상:**
| 문법 | 결과 |
|------|------|
| `let a: Array<T> = Array();` | ❌ Undefined function 'Array' |
| `let a: Array<T> = [1, 2, 3];` | ✅ 리터럴만 가능 |
| `let l: List<T> = ListNew();` | ✅ 동작 |
| `let m: Map<K,V> = Map();` | ❌ Unknown type 'Map' |
| `let m: HashMap<K,V> = HashMap();` | ❌ Undefined function 'HashMap' |

**문제:**
- `Array<T>`는 리터럴 `[]`로만 생성 가능. 동적 생성 API(`ArrayNew()`)가 없거나 불명확
- `List<T>`는 `ListNew()`로 생성. `Array`와 `List`의 용도 차이가 불명확
- `Map`/`HashMap`은 문서에 있으나 실제로 사용 가능한 예제가 없음

**영향:** 컬렉션을 쓰려면 예제를 뒤져야 함. 직관적인 API 아님.

---

### 9. `Result<T>`에서 값 추출 방법 불명확

**현상:**
```pergyra
func RiskyOperation(x: Int) -> Result<Int> { ... }
let r: Result<Int> = RiskyOperation(5);
// r에서 Int 값을 어떻게 꺼내는가?
```

`Ok`/`Err` 생성자는 동작하지만, 결과에서 값을 추출하는 표준 방법이 없음.
- `UnwrapResult()`? → 미정의
- `?` 연산자? → 시맨틱은 있으나 사용법이 문서화되지 않음
- match 패턴? → `case Ok(v):` 같은 variant destructuring이 제한적

**영향:** Result<T>를 받아도 쓸 방법이 모호함.

---

### 10. 문자열 보간이 `+`와 `ToString()`의 반복

**현상:**
```pergyra
// 현재
Log(name + " (Level " + ToString(level) + ")");

// f-string은 동작함
Log(f"HP: {hp}, MP: {mp}, Level: {level}");
```

f-string이 동작하는 것은 확인됨. 하지만 문서와 예제에서 `+`/`ToString` 패턴이 여전히 널리 사용되어 초보자가 f-string 존재를 알기 어려움.

---

## P2 — 설계적 불편함

### 11. "간단한 데이터"를 선언할 적당한 타입이 없음

**현상:**
게임에서 `Player`, `Monster` 같은 엔티티를 만들고 싶을 때:
- `struct` → 값 타입, 필드 접근 가능. 하지만 Slot 매크로 warning 26개 발생
- `subject` → Slot Claim/Release 필요. 단순 데이터에 overkill
- `class` → hosted func 가능. 하지만 value self 모델이 직관적이지 않음

**결과:** 단순 데이터를 위해 `struct`를 선택하면 warning 폭탄, `subject`를 선택하면 Slot boilerplate.

---

### 12. `struct` 필드 수정 가능 여부 불명확

**현상:**
```pergyra
struct Player {
    let name: String;
    let hp: Int;
}

let p: Player = Player("Hero", 100);
p.hp = 90;  // 될까? 안 될까?
```

`struct` 필드에 `let`을 쓰면 불변? `let` 없이 쓰면 가변? 문법이 모호함.

---

### 13. 함수 반환값에서 match가 모든 경로에서 return를 보장하지 않음

**현상:**
```pergyra
func DirectionName(dir: Direction) -> String {
    match dir {
        case North: return "North";
        case South: return "South";
        case East:  return "East";
        case West:  return "West";
    }
}
```
```
warning: non-void function does not return a value in all control paths
```

match가 exhaustiveness를 검증하지 못해 C 코드에서 default return이 누락됨.

---

## 요약 — 우선순위별 액션 아이템

| 우선순위 | 항목 | 예상 작업량 |
|----------|------|-------------|
| **P0-1** | Array for-in `.count` → `.length` 수정 | 1시간 |
| **P0-2** | `StringSplit`/`StringJoin` 런타임 구현 또는 표면 제거 | 2-4시간 |
| **P0-3** | `None` 심볼 정의 (Option<T>) | 30분 |
| **P0-4** | enum match OR 패턴 + exhaustive checker 수정 | 2-4시간 |
| **P0-6** | defer 변수 스코프 버그 수정 | 2-4시간 |
| **P1-7** | struct/subject 불필요한 Slot 매크로 생성 억제 | 2-4시간 |
| **P1-8** | 컬렉션 생성 API 통일 (`ArrayNew()`, `HashMapNew()` 등) | 4-8시간 |
| **P1-9** | `Result<T>` 언랩 API 명확히 (`UnwrapResult`, `?` 연산자 문서화) | 2-4시간 |
| **P2-11** | 가벼운 값 타입 (`data` 또는 `record` keyword) 검토 | TBD |
| **P2-12** | struct 필드 가변성 규칙 명시 | 1시간 |
| **P2-13** | match 기반 함수의 default return 자동 생성 | 1-2시간 |

---

## 긍정적 발견 사항

다음은 **잘 동작하는** 기능들입니다:

| 기능 | 상태 | 비고 |
|------|------|------|
| `func` 선언/호출 | ✅ | |
| `if/else`, `while` | ✅ | |
| `for item in array` (List) | ✅ | Array는 버그 |
| enum 선언 | ✅ | |
| match (단일 case) | ✅ | OR/variant 제외 |
| `type` alias | ✅ | |
| `struct` 선언/생성/접근 | ✅ | warning 제외 |
| f-string 보간 | ✅ | `f"HP: {hp}"` |
| `defer { ... };` (파싱) | ✅ | 스코프 버그 있음 |
| 파이프 연산자 `\|>` | ✅ | |
| `Option<T>` + `Some` | ✅ | `None` 제외 |
| `Array<T>` 리터럴 `[1,2,3]` | ✅ | |
| `List<T>` + `ListNew/Push/Size` | ✅ | |
| `ArraySort`, `ArrayMap`, `ArrayFilter` | ✅ | |
| 디스트럭처링 `let (a,b,c) = arr` | ✅ | |
| `ToString`, `Log` | ✅ | |
