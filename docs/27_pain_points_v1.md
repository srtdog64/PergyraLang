# Pergyra Pain Points v1 (2026-04-04)

실전 멀티파일 시뮬레이터 (battle_sim, space_station, biome_simulator) 구현 과정에서 발견된 이슈와 수정 기록.

## #1 `event`가 예약 키워드 — **수정 완료**

- `event`를 contextual keyword로 변경 (lexer keyword 테이블에서 제거)
- 파서에서 `parser_match_contextual_keyword(parser, "event")`로 체크
- 변수명 `event` 사용 가능해짐

## #2 nested vessel method dispatch — **수정 완료**

- `resolve_nominal_host_expr_type_name`을 확장하여 `obj.field` 패턴에서 field 타입을 조회
- `infer_expression_type_name`에 `AST_MEMBER_ACCESS` case 추가
- `is_nominal_host_type_name`에서 vessel도 nominal host로 인정

## #3 `!vessel.Method()` 시맨틱 타입 추론 — **수정 완료**

- **근본 원인**: `type_is_nominal_host_type`이 vessel을 nominal host로 인정하지 않아 method call 리턴 타입 해석을 건너뜀
- **수정**: `!decl->data.class_decl.is_struct`에 `|| decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL` 추가
- workaround (변수 분리) 없이 `!vessel.Method()` 직접 호출 가능

## #4 let 변수 String 타입 추론 — **수정 완료**

- `infer_expression_type_name`에 `AST_MEMBER_ACCESS` case 추가 (field 타입 조회)
- let 초기화에서 모든 initializer의 타입을 fallback 추론하도록 확장
- string `+` chain에서 leftmost leaf를 재귀적으로 찾는 로직 추가

## #5 relation/effect C struct 선언 순서 — **수정 완료**

- `emit_program`에서 zone emit 전에 relation/effect emit pass (3.75) 추가
- zone이 relation/effect 필드를 참조할 때 C struct가 이미 정의되어 있도록 보장

## #6 role method self pointer — **수정 완료**

- role method에서 `void *_raw_self` 시그니처 유지 (vtable 호환)
- body에서 `SubjectType *self = (SubjectType *)_raw_self;` 캐스팅 추가
- `current_class_name`을 role의 target subject로 설정하여 `self->field` 접근 가능

## #7 zone/world bare field access — **수정 완료** (추가 발견)

- zone/world func 안에서 `self.` 없이 bare name으로 shared field 접근
- **수정**: `type_check_overlay_decl_common`에서 scope_enter 후 shared field + domain slot을 scope에 등록
- zone의 subject/object slot, world의 zone slot도 등록

## 현재 남아있는 제한 (workaround 필요)

- **deep nested member access 타입 추론**: `self.meadow.grazer.name + " E:"` 같은 3단 이상 member access에서 String 타입 추론 실패. workaround: `FileWrite` 분리 호출
- **method call 반환 타입 코드젠 추론**: vessel method call의 반환 타입을 코드젠의 `infer_expression_type_name`이 모름. workaround: `let x: Type = obj.Method();`로 타입 어노테이션

## 발견 경위

- battle_sim.pgy: subject param 불가, vessel mutation 문제, string concat chain
- space_station 멀티파일: event 키워드, nested vessel method, `!` 추론, struct 순서, role self
- biome_simulator 멀티파일: zone/world bare field access, deep nested string concat
