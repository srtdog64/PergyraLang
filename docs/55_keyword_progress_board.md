# Keyword Progress Board

마지막 업데이트: 2026-04-11

이 문서는 현재 언어 키워드 전체에 대해 구현/문서/테스트 기준 진행률을 기록한다.

판정 기준:

- `100%`: 표면, semantic, backend, 테스트, 문서가 모두 정렬됨
- `80~95%`: 실사용 가능, 일부 문서/경계/추가 회귀만 남음
- `50~75%`: 부분 구현, 주 경로는 있으나 계약이 덜 닫힘
- `20~45%`: parser/초기 semantic만 있거나 일부 경로만 있음
- `0~15%`: 설계/문서만 있거나 사실상 미구현

## A. 예약 키워드 진행률

| 키워드 | 진행률 | 상태 메모 |
|---|---:|---|
| `let` | 100% | 핵심 표면 정렬 완료 |
| `func` | 95% | MIR/LLVM 전환은 async subset이 남음 |
| `class` | 85% | 보조 nominal 축으로 안정적, direct MIR method path는 일부 진행 중 |
| `subject` | 95% | 코어 host 축으로 정렬 완료 |
| `struct` | 95% | 안정적 value type 표면 |
| `tobject` | 95% | `dto` 제거 후 표면 정렬 완료 |
| `enum` | 90% | tagged/value sum type 경로 실사용 가능 |
| `type` | 85% | alias 표면 안정, richer tooling은 남음 |
| `ability` | 95% | 기본 공개 계약으로 정렬됨, `private ability`는 숨김 surface로 동작, generic declaration + `where` surface + generic impl/requires satisfaction 지원, `export ability`는 중복 표기 |
| `role` | 85% | ability 구현과 where 일부 연결 |
| `party` | 80% | core subset은 있으나 advanced collaboration demos는 아직 sketch 성격이 남음 |
| `channel` | 90% | runtime/select/backpressure surface 연결 |
| `import` | 90% | 모듈 표면 안정 |
| `use` | 88% | stdlib merge, duplicate use warning, known stdlib module contract, `use datetime;` exported surface 회귀, imported ability/module visibility 계약 일부 반영 |
| `export` | 90% | explicit export가 imported module 경계에서 실제로 비-export 심볼을 숨기고 회귀 테스트도 존재, exported nominal constructor surface까지 반영됨 |
| `namespace` | 80% | 표면 안정, tooling 보강 여지 |
| `extern` | 80% | C/LLVM 경로 연결 |
| `public` | 78% | explicit nominal field/method visibility가 same-host/private 규칙에 더해 explicit cross-module visibility 경계와 exported nominal constructor surface까지 연결됨 |
| `private` | 78% | explicit nominal field/method visibility가 same-host/private 규칙에 더해 explicit cross-module visibility 경계와 non-exported nominal constructor 차단까지 연결됨 |
| `where` | 80% | generic/intention clause 공용 reserved token으로 정리, richer constraint는 남음 |
| `as` | 75% | alias/type helper 표면 안정 |
| `impl` | 85% | role/ability 구현 핵심 표면 |
| `include` | 75% | role composition helper 수준 |
| `require` | 96% | ability require field 타입/중복 진단 + role impl 시 bound subject host 만족성 검사 + imported ability의 hidden nominal require type/ability 차단 + action requires의 foreign hidden ability 차단 + generic ability declaration/arity/type-arg validation + generic impl/requires satisfaction + richer required/actual generic mismatch diagnostics까지 연결됨 |
| `override` | 55% | role/party 보조 modifier는 있으나 inheritance override는 코어 대상 아님 |
| `extends` | 30% | party 보조 관계 표면은 있으나 inheritance 코어 축은 아님 |
| `if` | 100% | 안정 |
| `else` | 100% | 안정 |
| `for` | 90% | 안정, 일부 optimizer/tooling 여지 |
| `in` | 90% | loop/member syntax 안정 |
| `while` | 95% | 안정 |
| `return` | 100% | 안정 |
| `break` | 95% | 안정 |
| `continue` | 95% | 안정 |
| `match` | 85% | exhaustiveness/quality 진단은 더 확장 가능 |
| `case` | 85% | match/select arm 표면 안정 |
| `default` | 85% | match/select fallback 안정 |
| `with` | 85% | scoped resource binding 안정 |
| `parallel` | 90% | core execution primitive로 재정의 완료, rules 확장은 남음 |
| `async` | 80% | suspension surface 정렬 중 |
| `await` | 85% | future/result 규칙 안정 |
| `spawn` | 85% | parallel family 아래 surface로 정렬 중 |
| `select` | 85% | readiness surface 정렬 중 |
| `defer` | 75% | 표면 안정, deeper cleanup/debugger 연동은 남음 |
| `unsafe` | 60% | 표면 위주 |
| `bind` | 85% | object/tobject projection contract 핵심 |
| `secure` | 95% | SecureSlot 중심 capability 첫 단계 + runtime file I/O/root policy/fingerprint hardening + parallel context에서 secure-effect helper/메서드 호출 차단 + SecureSlot/Token<T> 파라미터 기반 secure effect 추론 + named paired token/정적 타입 pairing 검사 + channel transport 차단 + zone authority가 있는 boundary publish/bind는 explicit `by`를 강제 + authority-bearing intent/effect/helper/action flow는 explicit `authorized by`를 요구 |
| `slot` | 96% | universal resource anchor로 매우 중요, secure/runtime policy와도 정렬됨 |
| `shared` | 80% | host-local contextual state 표면 안정 |
| `dyn` | 45% | dyn role slot과 runtime vtable swap 표면은 존재, 일반 dynamic dispatch 축은 아님 |
| `own` | 85% | boundary/resource mode로 실사용 가능 |
| `ref` | 85% | boundary/resource mode로 실사용 가능 |
| `true` | 100% | 안정 |
| `false` | 100% | 안정 |

## B. 컨텍스트 키워드 진행률

| 키워드 | 진행률 | 상태 메모 |
|---|---:|---|
| `object` | 95% | local projection contract로 정리 완료 |
| `vessel` | 85% | subject 내부 수용체 모델 안정 |
| `relation` | 80% | domain/runtime projection sync 경로 존재 |
| `effect` | 95% | closure + join/meet API + partial-order compare/conflict API, authority/resource helper, contract check, branch/match/disjoint-branch join 회귀까지 반영, richer authority/resource 통합 partial order는 일부 남음 |
| `zone` | 85% | core execution/authority boundary |
| `roster` | 90% | `systemic` 제거 후 일관화 완료 |
| `world` | 85% | cross-zone orchestration 경계는 실구현, 일부 fuller hierarchy demos는 아직 sketch |
| `event` | 80% | surface와 codegen 연결 |
| `action` | 90% | subject 공적 동사로 정렬 |
| `intent` | 90% | 1급 orchestration core |
| `involves` | 80% | participant binding 안정 |
| `step` | 85% | MIR carrier까지 이동 |
| `who` | 85% | intent participant binding 안정 |
| `using` | 80% | intent zone binding 경로 존재 |
| `requires` | 75% | action/intent/where 제약 표면 존재 |
| `authorized` | 75% | authority 계약 존재 |
| `by` | 75% | clause helper |
| `within` | 75% | zone-scoped action 계약 존재 |
| `causes` | 75% | effect contract 존재 |
| `expect` | 80% | intent check MIR carrier 존재 |
| `success` | 75% | intent contract 존재 |
| `failure` | 75% | intent contract 존재 |
| `rollback` | 85% | intent policy/runtime 연결 |
| `cleanup` | 80% | MIR exceptional topology 연결 |
| `compensate` | 85% | intent rollback carrier 연결 |
| `exclusive` | 80% | runtime conflict registry 연결 |
| `concurrent` | 80% | runtime conflict registry 연결 |
| `priority` | 80% | runtime nesting/override 연결 |

## C. 의도적으로 비코어/보류된 키워드 축

이 항목들은 단순히 "구현이 덜 됐다"가 아니라, 현재 언어 철학상 코어로 올릴지 자체가 보류되었거나 축소된 표면이다.

1. `extends`
2. `dyn`

## D. 현재 가장 부족한 키워드 축

1. `public` / `private`의 module/export boundary 확장
2. `async` family의 문서/테스트 재정렬 잔여분
3. `override`
4. `effect`의 authority/resource 통합 partial order
5. `secure`의 zone/authority declaration 일반화 잔여분
6. `subject/class/struct/object/tobject`의 lexer token aliasing 제거

## E. 현재 가장 잘 닫힌 키워드 축

1. `subject`
2. `slot`
3. `object`
4. `tobject`
5. `parallel`
6. `intent`
7. `roster`

## F. 한 줄 요약

언어 전체는 이미 “돌아가는 키워드 집합” 단계는 넘었다.
지금 남은 일은 모든 키워드를 같은 방식으로 키우는 것이 아니라, 코어 키워드와 비코어/보류 키워드를 분리해서 각자의 밀도로 정리하는 것이다.
