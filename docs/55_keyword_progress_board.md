# Keyword Progress Board

Current vocabulary authority (2026-07-27):

- `src/lexer/language_keyword_registry.def` owns the 71 reserved, 71
  contextual, and 3 soft language-word identities.
- This board records maturity notes only; it is not a lexer/parser/LSP keyword
  list. In particular, `fields` is contextual rather than lexer-reserved.
- See `docs/semantics/language_keyword_registry.md` for ownership and gate
  contracts.

Anti-hype status note (2026-04-29):

- Percentages in this board are local keyword-surface ledger values, not
  whole-language beta readiness, production readiness, or formal proof status.
- `100%` means "closed for the currently frozen subset": parser, semantic,
  backend, tests, and docs agree for the scoped subset only.
- `100%` does not mean the keyword is complete for all future semantics,
  arbitrary user programs, all platforms, or all backend edge cases.
- If this board conflicts with `docs/100_beta_readiness_checklist.md`, the
  beta readiness checklist is the stronger source of truth.

마지막 업데이트: 2026-04-12

이 문서는 현재 언어 키워드 전체에 대해 구현/문서/테스트 기준 진행률을 기록한다.

판정 기준:

- `100%`: 현재 frozen subset에서 표면, semantic, backend, 테스트, 문서가 정렬됨
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
| `struct` | 100% | 안정적 value type 표면으로 parser/semantic/C/LLVM/tests/docs가 정렬됨 |
| `tobject` | 100% | `dto` 제거 후 boundary projection/publish contract surface가 parser/semantic/C/LLVM/tests/docs까지 정렬됨 |
| `enum` | 90% | tagged/value sum type 경로 실사용 가능 |
| `type` | 85% | alias 표면 안정, richer tooling은 남음 |
| `ability` | 97% | 기본 공개 계약으로 정렬됨, `private ability`는 숨김 surface로 동작, generic declaration + `where` surface + generic impl/requires/party-role-slot satisfaction 및 bound revalidation이 연결됨, `export ability`는 중복 표기 |
| `role` | 85% | ability 구현과 where 일부 연결 |
| `party` | 80% | core subset은 있으나 advanced collaboration demos는 아직 sketch 성격이 남음 |
| `channel` | 90% | runtime/select/backpressure surface 연결 |
| `import` | 90% | 모듈 표면 안정 |
| `use` | 92% | stdlib merge, duplicate use warning, compiler-known stdlib module set가 실제 `stdlib/*.pgy`와 정렬됨, layered stdlib/domain kit 회귀가 존재함. 남은 일은 module contract summary와 richer diagnostics/tooling 노출 |
| `export` | 100% | explicit export가 imported module 경계에서 실제로 비-export nominal/domain/callable surface를 숨기고, constructor/callable/action-contract leakage 차단과 문서/회귀가 정렬됨 |
| `namespace` | 80% | 표면 안정, tooling 보강 여지 |
| `extern` | 80% | C/LLVM 경로 연결 |
| `public` | 100% | explicit nominal field/method visibility에 더해 top-level nominal + core domain declaration + `func/intent/event` callable surface까지 module/export boundary가 연결되고 문서/회귀가 정렬됨 |
| `private` | 100% | explicit nominal field/method visibility에 더해 top-level nominal + core domain declaration의 non-exported constructor/type boundary, 그리고 `func/intent/event` non-exported callable 차단까지 문서/회귀와 함께 정렬됨 |
| `where` | 90% | generic/intention clause 공용 reserved token으로 정리, exact bound + ability-style bound + multi-bound baseline과 ability generic reference bound revalidation이 연결됨. 남은 일은 broader type-family generalization과 richer diagnostics/tooling |
| `as` | 75% | alias/type helper 표면 안정 |
| `impl` | 85% | role/ability 구현 핵심 표면 |
| `include` | 75% | role composition helper 수준 |
| `fields` | 100% | canonical ability field contract surface로 정리 완료. parser/semantic/docs/tests/examples/smoke가 모두 `fields` 기준으로 정렬되었고 legacy `require` alias는 제거됨 |
| `override` | 55% | role/party 보조 modifier 표면은 있으나 inheritance override를 코어 completion 대상으로 올리지는 않음 |
| `extends` | 30% | party 보조 관계 표면은 있으나 inheritance 축 자체를 코어 completion 대상으로 보지 않음 |
| `if` | 100% | 안정 |
| `else` | 100% | 안정 |
| `for` | 90% | 안정, 일부 optimizer/tooling 여지 |
| `in` | 90% | loop/member syntax 안정 |
| `while` | 100% | 안정 |
| `return` | 100% | 안정 |
| `break` | 100% | 안정 |
| `continue` | 100% | 안정 |
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
| `unsafe` | 60% | explicit escape hatch 표면과 body type-check/transpile 회귀는 있으나 deeper safety contract 키워드로 키울 계획은 없음 |
| `bind` | 85% | object/tobject projection contract 핵심 |
| `secure` | 95% | SecureSlot 중심 capability 첫 단계 + runtime file I/O/root policy/fingerprint hardening + parallel context에서 secure-effect helper/메서드 호출 차단 + SecureSlot/Token<T> 파라미터 기반 secure effect 파생 + named paired token/정적 타입 pairing 검사 + channel transport 차단 + zone authority가 있는 boundary publish/bind는 explicit `by`를 강제 + authority-bearing intent/effect/helper/action flow는 explicit `authorized by`를 요구 |
| `slot` | 96% | universal resource anchor로 매우 중요, secure/runtime policy와도 정렬됨 |
| `shared` | 80% | host-local contextual state 표면 안정 |
| `dyn` | 45% | dyn role slot과 runtime vtable swap 표면은 존재하나 일반 dynamic dispatch 코어 축은 아님 |
| `own` | 100% | stable surface는 `own SecureSlot<subject-host>`로 고정되었고, unsupported general ownership은 explicit reject와 문서/예제/semantic 회귀로 정렬됨 |
| `ref` | 100% | stable surface는 `ref Slot<subject-host>`로 고정되었고, return/channel escape·alias/rebind·borrow-after-move 금지와 문서/예제/semantic 회귀가 정렬됨 |
| `true` | 100% | 안정 |
| `false` | 100% | 안정 |

## B. 컨텍스트 키워드 진행률

| 키워드 | 진행률 | 상태 메모 |
|---|---:|---|
| `object` | 95% | local projection contract로 정리 완료 |
| `vessel` | 85% | subject 내부 수용체 모델 안정 |
| `relation` | 80% | domain/runtime projection sync 경로 존재 |
| `effect` | 96% | closure + join/meet API + partial-order compare/conflict API, authority/resource helper, contract check, branch/match/disjoint-branch join 회귀까지 반영. resource-boundary conflict class는 이제 `remote`뿐 아니라 `collapse`까지 정렬됨. richer authority/resource 통합 partial order는 일부 남음 |
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
| `requires` | 95% | action/intent/zone authority/party role slot contract에 type-reference AST가 연결되고 generic ability ref validation, generic mismatch diagnostics, matching action inheritance diagnostics, LSP hover/completion, semantic 회귀가 정렬됨. 남은 일은 richer generic constraint validation과 broader authoring surface 문서화 |
| `authorized` | 90% | authority-sensitive action/step/effect/secure flow에 explicit clause 강제가 연결되고 inherited-action diagnostics와 semantic 회귀가 존재함 |
| `by` | 90% | `authorized by` 및 authority-bearing bind/publish/boundary flow tail로 실제 계약 의미를 가지며 parser/semantic/LSP 회귀가 정렬됨 |
| `within` | 90% | action zone contract, lexical zone context, step/action/transfer 유도, inherited-zone diagnostics, semantic 회귀까지 연결됨 |
| `causes` | 90% | action/step effect contract, authority-sensitive diagnostics, derived/inherited explanation, semantic 회귀가 정렬됨 |
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

1. `override`
2. `extends`
3. `dyn`
4. `unsafe`

## D. 현재 가장 부족한 키워드 축

1. `public` / `private`의 module/export boundary 확장
2. `effect`의 authority/resource 통합 partial order
3. `own` / `ref`의 일반 타입 전반 closure
4. `use` / contract summary의 module boundary closure

## E. 현재 가장 잘 닫힌 키워드 축

1. `subject`
2. `slot`
3. `fields`
4. `object`
5. `tobject`
6. `parallel`
7. `intent`
8. `roster`

## F. 한 줄 요약

언어 전체는 이미 “돌아가는 키워드 집합” 단계는 넘었다.
지금 남은 일은 미완성 키워드를 무조건 다 키우는 것이 아니라,
코어 completion 대상과 explicit non-core/experimental 키워드를 분리해
보드가 실제 구현 깊이를 정직하게 반영하도록 만드는 것이다.
