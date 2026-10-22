# 메인 전달: `let` / `var`와 `let mut`의 의미 경계

상태: 검토·제안. 문법 변경이나 불변성 집행 완료를 선언하지 않는다.
기준: `main @ b01b01941632d4469e33de9be8c8bd3d3b3963be`, 2026-10-23 KST.
사용자 요청: Rust의 `let mut`와 Swift 방식을 비교해 메인 작업자가 판단할
문서로 남긴다. 이 검토에서는 parser 변경이나 대량 소스 전환을 하지 않는다.

## 결론

기존 `let` / `var` 표기를 유지하는 쪽을 권한다. **철자보다 중요한 것은
바인딩 재대입, 값의 변경, 정체성을 가진 자원의 상태 전이를 구별하는 것**이다.
`let mut`로 되돌려도 현재 빠진 불변성 검사는 저절로 생기지 않는다.

현재 계약과 진행 상태의 주인은
[문법](../grammar/01_syntax.md#21-값-선언-let-var)과
[기존 집행·전환 지시서](../agent_work_directives/let_var_binding_mutability_2026-10-13.md)다.
이 문서는 새 의미 소유자나 그 지시서를 대체하는 결정이 아니다.

## 공식 언어와의 비교

| 경계 | Swift | Rust | Pergyra에 적용할 판단 |
|---|---|---|---|
| 바인딩 | `let`은 상수, `var`는 변수 | 기본 `let`, 변경 가능한 바인딩은 `let mut` | 이미 착지한 `let` / `var`를 유지하는 것이 가장 작은 변경 |
| 값인 struct·collection | `let`으로 보관한 값의 변경을 거부 | 바인딩과 접근 권한에 따라 변경을 제한 | Pergyra의 값 의미론에는 이 불변성 집행이 필요 |
| 참조·정체성 | `let` class 참조도 객체의 가변 속성은 바뀔 수 있음 | 내부 가변성은 별도 타입·규칙으로 가능 | `let`이 모든 도달 가능한 자원의 동결을 뜻한다고 일반화하면 안 됨 |
| 호출자 값 변경 | `inout`은 명시적 변경 경계 | 가변 참조와 빌림 규칙을 사용 | 기존 `inout` 계약과 호출 요약을 소비하며 Rust의 빌림 문법을 수입하지 않음 |

출처(2026-10-23 확인):
[Swift Declarations](https://docs.swift.org/latest/documentation/the-swift-programming-language/declarations/),
[Swift Properties](https://docs.swift.org/swift-book/LanguageGuide/Properties.html),
[Rust Variables and Mutability](https://doc.rust-lang.org/book/ch03-01-variables-and-mutability.html),
[Rust Interior Mutability](https://doc.rust-lang.org/reference/interior-mutability.html).

Swift의 표기를 따른다고 Swift의 class 참조 의미나 collection의 copy-on-write
구현을 채택하는 것은 아니다. Pergyra `class`는 자체 값/hosted 계산 계약을,
`subject`, Slot, channel 등은 자체 정체성·권한·수명 계약을 따른다.

## 현재 소스에서 확인한 공백

- parser는 `let` / `var`를 구별하고 `let mut`을 거부한다.
- `type_checker_ownership_let.c`는 `is_mut_binding`을 기록한다.
- `type_checker_assignment.c`의 일반 로컬 대입 경로는 그 플래그로 쓰기를
  거부하지 않는다. 필드의 `let` 쓰기 거부와 로컬 바인딩의 불변성은 다르다.
- 문법의 미구현 안내도 로컬 재대입·필드/원소 변경·변경 builtin·`inout`의
  집행이 남았다고 명시한다. 이번 검토의 근거는 소스이며, 과거 바이너리의
  실행 결과를 최신 HEAD의 새 재현으로 세지 않는다.

## 집행 전에 메인에서 확정할 경계

1. **값 변경:** `let` scalar 재대입, 값 struct/class 필드·중첩 필드 쓰기,
   Array/Map 원소 변경, compound assignment, 변경 builtin, `inout` 전달은
   하나의 바인딩/place 사실을 통해 거부한다. `var`의 같은 연산은 허용한다.
2. **정체성의 상태 전이:** `let subject`의 action, Slot payload 변경,
   channel 전송은 바인딩 재대입과 같지 않다. 해당 authority/capability/수명
   주인의 승인으로 판정할지 명시하고 양성·음성 예제를 함께 고정한다.
   "receiver가 let이면 모든 호출 거부"라는 공통 규칙으로 뭉개지 않는다.
3. **이동과 읽기:** 불변 바인딩도 마지막 사용에서 이동·정리할 수 있다.
   불변성은 소유권의 정지나 불멸성이 아니다. 빌린 값의 끝과 backing의
   해제를 구별하고, 내부 cleanup은 사용자 값 변경 진단의 대상이 아니다.
4. **별칭:** `let a`만 보고 backing이 영원히 자라지 않는다고 추론하지 않는다.
   다른 접근 경로의 변경·escape가 없다는 소유권 사실도 필요하다. 실제
   집행이 없는 현재 `let`을 새 lifetime 증거로 사용하면 안 된다.

## 실행 카드와 반례

- 목적: 표기는 그대로 두고 값 변경의 의미를 native/self-host에서 일치시킨다.
- 우선순위: 기존 의미·정체성 경계 > 한 사실 주인 > 모든 쓰기 경로 집행 >
  반례 > 진단 기반 소스 전환. CI를 위해 쓰기 오류를 경고로 낮추지 않는다.
- 사실 주인: parser의 binding-mutability와 의미 분석의 resolved binding/place.
- 마지막 소비자: 대입·변경 builtin·`inout`을 승인하는 의미 판정. Backend가
  표기나 이름으로 가변성을 다시 결정하지 않는다.
- 금지: `let mut` 호환 별칭, 파일별 면제, 사용처를 보고 `let`을 몰래 가변으로
  승격, 키워드 개수 검사로 의미 검사를 대체, 불변성을 GC/RC의 근거로 사용.
- 게이트: 위 각 쓰기 경로의 `let` 거부와 `var` 성공, shadowing, 중첩 place,
  branch·loop, 빌림, 이동 이후 사용, resource action 경계, 양 front end 진단,
  unchanged canonical과 bootstrap/installed C·LLVM 통합 검사.

전환은 현재 미커밋 ownership 작업 위에 대량 치환하지 않는다. 기존 지시서의
진단 span 기반 전환과 의미 집행을 같은 착지 단위로 준비하고, 실제 실행
결과가 있는 범위만 닫는다. 전체 푸시와 정확한 published-SHA CI 확인은
2026-10-23 사용자 답변에 따라 메인 작업자가 맡는다.
