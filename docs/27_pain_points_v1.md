# Pergyra Pain Points v1 (2026-04-04)

실전 멀티파일 시뮬레이터 (battle_sim, space_station) 구현 과정에서 발견된 코드젠/시맨틱 이슈.

## #1 `event`가 예약 키워드 (Blocker — 파서)

```pergyra
let event = Random(4);  // 파서 에러: TOKEN_EVENT
```

- `world`, `systemic`, `relation`, `effect`, `zone`, `vessel`, `action`은 contextual keyword
- `event`는 `TOKEN_EVENT`로 lexer에 하드코딩되어 있어 변수명 사용 불가
- **수정**: `event`도 contextual keyword로 변경

## #2 nested vessel method dispatch (Blocker — 코드젠)

```pergyra
let alive: Bool = rookie.vitals.IsAlive();
```

생성된 C (잘못됨):
```c
bool alive = rookie.vitals.IsAlive();  // struct에 IsAlive 멤버 없음
```

올바른 C:
```c
bool alive = VitalSigns_IsAlive(rookie->vitals);
```

- `obj.vessel.Method()` 패턴에서 vessel 타입을 인식해 `VesselType_Method(obj->vessel)` 형태로 emit해야 함
- 현재 member access + call 코드젠 경로가 vessel을 nominal host로 인식하지 못함
- **근본 원인**: `resolve_nominal_host_expr_type_name`이 nested member access를 추적하지 못함

## #3 `!vessel.Method()` 시맨틱 타입 추론 실패 (Blocker — 시맨틱)

```pergyra
let rested: Bool = !vitals.IsExhausted();
// ERROR: '!' operator requires Bool, got '<unknown>'
```

- vessel method 호출의 리턴 타입을 시맨틱이 추론 못 함
- workaround: `let x: Bool = vitals.IsExhausted(); let y: Bool = !x;`
- **근본 원인**: 시맨틱의 method call 타입 해석이 vessel(struct-like) 타입의 method를 검색하지 못함

## #4 let 변수 String 타입 추론 — `+` chain (Known — 코드젠)

```pergyra
let line = "  " + member.name;
line = line + " HP:" + ToString(hp);  // line 타입을 Int로 추론
```

생성된 C:
```c
int32_t line = StringConcat("  ", member->name);  // 타입 불일치
line = (line + " HP:") + ...;                     // char* + char* = C error
```

- `let x = expr;` 에서 init이 call이면 추론 등록되지만, binary `+`는 Int fallback
- **부분 수정 완료**: `infer_expression_type_name` fallback 추가했으나, string `+` 시작이 member access일 때 여전히 실패
- **근본 원인**: `infer_expression_type_name`이 member access의 field 타입을 모름

## #5 relation/effect C struct 선언 순서 (Known — 코드젠)

```c
// zone struct가 relation/effect struct보다 먼저 emit되면 에러
typedef struct { RadiationExposure radiation; } EngineeringBay;  // RadiationExposure 미정의
```

- HIR이 type → function → domain 순서로 emit하는데, zone이 relation/effect를 필드로 가지면 선언 순서 충돌
- **수정**: domain 타입을 HIR type 섹션에서 먼저 forward-declare하거나, emit 순서를 relation/effect → zone 순서로 보장

## #6 role method에서 self가 value-self (Known — 코드젠)

```pergyra
role Engineer for CrewMember {
    impl Repairable {
        func GetRepairPower(self) -> Int {
            return self.skills.engineering * 2;
        }
    }
}
```

생성된 C (잘못됨):
```c
return (self.skills.engineering * 2);  // self는 CrewMember (subject = pointer)
```

올바른 C:
```c
return (self->skills.engineering * 2);  // subject는 pointer-self
```

- role은 subject에 바인딩되므로 pointer-self여야 하는데 value-self로 emit
- **근본 원인**: role method emit 경로가 role의 target subject의 nominal_kind를 확인하지 않음

## 우선순위

| # | 심각도 | 수정 난이도 | 영향 범위 |
|---|--------|------------|-----------|
| 1 | Blocker | 쉬움 | lexer 1줄 변경 |
| 2 | Blocker | 중간 | transpiler method call dispatch |
| 3 | Blocker | 중간 | semantic method call type resolution |
| 4 | Known | 중간 | transpiler type inference |
| 5 | Known | 쉬움 | transpiler emit ordering |
| 6 | Known | 쉬움 | transpiler role method emit |

## 발견 경위

- battle_sim.pgy: subject param 불가, vessel mutation 문제, string concat chain → #4 발견
- space_station 멀티파일: event 키워드 충돌 → #1, nested vessel method → #2, `!` 추론 → #3, struct 순서 → #5, role self → #6
