# Intent graph 삭제·보강 의미 감사 — 2026-09-05

Status: AUDIT COMPLETE — 구현 정책과 SoT 상태를 소유하지 않는 관찰 기록.

기준 revision은 `bf8b33d078b27c41cc6cdb7ffed2e8fa5c62ef22`이다.
다른 세션이 수정한 native compiler가 설치돼 있으므로 아래 실행 결과는
이 로컬 binary 조합에 한정한다. compiler rebuild와 registry 변경은 하지 않았다.

- Native SHA-256: `0F9F4F30255D6850B5A773E21D5815F776B305E5C01A7A2C3DF6D373BB15A29E`
- DRV-2 SHA-256: `FB37EA36D92E9C28B6BB7162F87BA00E733255AD5E46B24A166578713DF75847`
- 지시서: [Intent and language-axis semantic audit](../agent_work_directives/intent_and_language_axes_semantic_audit_2026-09-05.md)

## 판정

**Intent의 잠정 판정은 CONDITIONAL이다. 조건은 action 수가 아니라 한 목적에
귀속되는 checked bundle의 존재다.** typed terminal 귀속은 단일 action에도
추가 정적 보장을 준다. 따라서 첨부 제안의 `0 cross-step invariant → func/action`
규칙은 현재 구현의 구별 능력까지 지워 버리는 지나치게 강한 축약이다.

관찰한 범위에서 `Intent`는 단순한 실행 결과 축약보다 강하다. 그러나 이 감사는
모든 기존 구문 조합에 대한 비표현성 정리가 아니다. 일반 `func + match` 대체
한 가지가 보존하는 값과 잃는 계약을 직접 구별한 결과다. 언어 표면의 Intent를
무조건 강제하거나 모든 내부 workflow를 Intent로 올릴 근거로 쓰면 안 된다.

## 목적 카드

- 목적: participant/predecessor/outcome/authority/zone/effect/compensation/
  terminal이 실제로 어디서 검사되고 어떤 경로로 실행되는지 분해한다.
- 우선순위: 동일 의미 보존 여부 → accepted/rejected 구별 → 실제 실행 →
  fact owner와 소비자 → 개념 비용. spelling 변경은 의미 삭제로 세지 않는다.
- fact owner: canonical [docs/01](../01_intent_first_design.md),
  [docs/173](../173_intent_axis_strengthening.md), 아래 구현 owner와 실행 gate.
- 마지막 소비자: primary agent의 통합 감사와 이후 한 개의 구현 rung 선택.
- 금지 fallback: step/action 개수 판정, trace만으로 정적 정리 주장,
  native MIR 공급 경로를 Pergyra source 전체 구현으로 승격.
- falsifier: Intent 없이 같은 값뿐 아니라 동일한 terminal 귀속·coverage
  거절과 나머지 경계 의무도 기존 fact 조합으로 보존하는 encoding.

## 실제 compiler-visible graph의 범위

`MirIntentExecutionPlan` v3은 실물이다.
[schema owner](../../src/self_hosted/mir/intent_execution_schema_owner.pgy)의
step/terminal row가 선언 syntax identity와 payload declaration identity를
보존하고, [fact owner](../../src/self_hosted/mir/intent_execution_fact_owner.pgy)의
`MirIntentExecutionPlanReady`가 교차 결합을 확인한다. 다만 사용자가 나열한
모든 축이 이 struct 하나에서 완전히 증명되는 것은 아니다.

| 축 | 관찰한 owner와 동작 | 이 감사에서 주장할 수 없는 범위 |
|---|---|---|
| participant | [signature owner](../../src/self_hosted/semantic/ast_intent_signature_fact_owner.pgy)가 intent parameter/value identity를 소유한다. [native participant checker](../../src/semantic/type_checker_intent_participants.c)는 `who`가 선언 participant이며 해당 Zone subject slot과 맞는지 검사한다. | 모든 action/helper 본문의 transitive used-participant set을 계산했다는 INT-1 전체 증거 |
| predecessor | [transition owner](../../src/self_hosted/semantic/ast_intent_transition_fact_owner.pgy)는 typed step의 명시 predecessor를 앞선 step identity와 결합한다. MIR owner는 같은 routine, 중복 identity, cycle을 검사한다. | 일반 dependency DAG의 다중 선행자·join, 임의 데이터 사용에 대한 전체 dependency closure |
| typed outcome | enum 선언 identity, variant local index, payload type 및 declaration identity를 함께 나른다. 단일 on-call의 결과를 한 번 묶는 기존 gate가 실행됐다. | enum 결과를 쓴다는 사실만으로 모든 outcome protocol이 자동 완결된다는 주장 |
| authority | [Zone authority owner](../../src/self_hosted/semantic/ast_zone_authority_fact_owner.pgy)와 [native step checker](../../src/semantic/type_checker_intent_authority.c)가 별도 책임을 가진다. action 계약에서 상속되는 권한도 있다. | plan v3 하나가 delegation history 및 모든 interprocedural authority 의무까지 닫았다는 주장 |
| Zone | v3 step에 `where_zone_names`와 양의 `where_zone_syntax_ids`가 존재하고 readiness에서 필수다. 기존 compensation gate가 identity와 실행을 확인했다. | Zone의 residence/resource/execution/authority 정책 전부에 대한 독립성 증명 |
| effect | [native step sequence](../../src/semantic/type_checker_intent_step_sequence.c)는 `causes`의 effect 선언 및 Zone effect layer를 검사한다. effect 자체의 상세 판정은 별도 lane이 맡았다. | v3 plan이 일반 effectful-step 집합 및 full-rollback coverage receipt까지 포함한다는 주장 |
| compensation | [control emitter](../../src/self_hosted/codegen/emission/intent_execution_plan_control_emit_owner.pgy)의 `CodegenIntentPlanEmitPredecessorCompensation`은 실패한 현재 step을 제외하고 성공 완료한 predecessor를 역순으로 보상한다. 같은 step 안 복수 보상도 역순이다. | 보상 코드가 현실 상태를 수학적으로 원복한다는 증명, 일반 INT-2 coverage 완료 |
| terminal | source owner는 성공 terminal 하나, 각 step의 실패 terminal, leaf 성공 및 exact admitted payload를 검사한다. MIR owner는 source/result variant와 payload declaration identity를 교차 검사한다. | source→MIR→C 전체 self-host producer가 이 plan을 이미 모두 생산한다는 주장 |
| trace | 기존 typed compensation gate에서 history/last failure/active count를 C·LLVM·self 소비 경로로 비교했다. | trace 자체의 비라이브러리 표현성. docs/173은 Purpose/Trace를 library-가능 bucket으로 둔다. |

이 구조를 가장 정확하게 부르면 **축별 fact를 같은 목적 identity에 귀속시키는
source binder와, 그중 typed transition을 실행하는 봉인된 계획**이다. Intent가
모든 축의 단독 semantic owner가 되는 구조는 canon의 방향이 아니다.

## 단일 action 삭제·대체 실험

보존하려는 작은 동작은 성공 때 receipt 값 `7`, 실패 때 problem 값 `9`를
반환하며 각 요청마다 action을 정확히 한 번 호출하는 것이다. 공통 선언은
[terminal fixture](../../tests/concept_semantics/intent/terminal_fixture.pgy)에 있다.

| 소스 | Native C / LLVM 관찰 | 정적 계약 |
|---|---|---|
| [single_step_exact](../../tests/concept_semantics/intent/single_step_exact.pgy) | `delivered=7`, `calls=1`, `rejected=9`, `calls=2` | 한 step의 양 variant와 성공/실패 terminal을 연결한다. 성공 terminal은 action이 내놓은 receipt를 그대로 운반한다. |
| [function_exact](../../tests/concept_semantics/intent/function_exact.pgy) | 동일 | `func + match`가 값 및 exactly-once 호출을 직접 구현한다. 별도 purpose/Zone step 귀속은 없다. |
| [function_rebuilt](../../tests/concept_semantics/intent/function_rebuilt.pgy) | 동일 | 일반 함수는 `AuditReceipt(7)`을 새로 만들어 반환해도 합법이다. 현재 데이터에서는 값이 같지만 action receipt의 귀속을 보장하지 않는다. |

이 비교는 완전한 의미 동등성 주장이 아니다. Intent의 Zone binding, 목적
identity, 관찰성 등의 의무를 일반 함수가 자동으로 계승한 것은 아니다.
비교한 관찰량은 결과값과 호출 횟수다.

이어 두 가지 별도 소스를 거절시켰다.

| 소스 | Native 의미 검사 | Self source 의미 검사 |
|---|---|---|
| [single_step_rebuilt_reject](../../tests/concept_semantics/intent/single_step_rebuilt_reject.pgy) | `must carry the exact admitted payload binding 'receipt'` | `typed intent terminal payload identity is invalid` |
| [single_step_missing_terminal_reject](../../tests/concept_semantics/intent/single_step_missing_terminal_reject.pgy) | `requires one labeled failure terminal for every step` | nonzero 종료, MIR/C artifact 없음. 이 입력은 현재 self 경로에서 진단 text가 비어 있으므로 진단 품질까지 통과했다고 주장하지 않는다. |

두 프로그램은 **step이 하나다.** predecessor 사이의 불변식은 없어도 step의
typed outcome을 목적 terminal에 귀속시키는 의무가 존재한다. 보상이 없는
단일 action이라는 이유로 Intent 후보에서 자동 탈락시키면 안 되는 직접 근거다.

유지 gate는 [terminal_substitution_smoke.sh](../../tests/concept_semantics/intent/terminal_substitution_smoke.sh)이다.
3개 positive의 native C/LLVM 출력과 2개 negative의 native/self 거절만 요구한다.
일반 함수의 합법적인 value reconstruction을 실패로 취급하지 않는다.

## 기존 gate 실행

모든 실행은 Git Bash에서 단독 scratch와 `timeout 300`을 사용했다.
compiler 재빌드는 하지 않았다. 처음 두 명령은 scratch 환경변수를
`D:/...`로 넘겨 relative MIR path 계약을 어겼고, `/d/...`로 교정한 뒤 실행했다.
그 초기 경로 오류는 compiler 의미론 실패로 세지 않는다.

| Gate | 실제 결과 | 검증 범위 |
|---|---|---|
| [intent_typed_outcome_execution_owner.sh](../../tests/self_hosted/parity/intent_typed_outcome_execution_owner.sh) | PASS | legacy Bool intent 안 한 action의 enum outcome binding, 정확 한 번 평가, native C/LLVM/self C, MIR binding negatives |
| [intent_typed_outcome_compensation_owner.sh](../../tests/self_hosted/parity/intent_typed_outcome_compensation_owner.sh) | PASS | typed 2-step success/first failure/second failure, predecessor 역보상, 복수·중복 보상, zero compensation, history, Zone identity와 MIR negatives |
| [terminal_substitution_smoke.sh](../../tests/concept_semantics/intent/terminal_substitution_smoke.sh) | PASS | 위 bounded source 대체 실험과 terminal 거절 |
| [intent_execution_fact_contract_owner.sh](../../tests/self_hosted/parity/intent_execution_fact_contract_owner.sh) | INCONCLUSIVE | probe를 실행하기 전 default self source-C가 `undefined_symbol`, `name: steps.transition_ids`로 compile을 거절했다. fact owner 자체의 positive/negative 실행 결과는 얻지 못했다. |

`intent_step_binding_contract_owner.sh`는 다른 lane이 실행했으므로 여기서는
중복 실행하지 않았다. 다른 lane의 결과도 probe compile에서
`ast_artifact_invalid / nominal_constructor_argument_type`으로 중단된
INCONCLUSIVE였으며, 이 감사의 authority PASS 근거에 포함하지 않는다.

## 발견한 self-host 생산 경계

새 `single_step_exact.pgy`에서 DRV-2의 `--emit-mir-json-verified`는 종료값 0으로
`pgy.mir.v1`을 만들었다. 하지만 output에는 `intent_execution` property가 없었다.
뒤이은 source `--emit-c-verified`는
`CODEGEN ERROR: self-host C emission rejected invalid machine-layer facts`로
거절했다. 최소 fixture에만 해당하는지 구분하려고 기존
`intent_typed_outcome_compensation.pgy`도 같은 self MIR 명령으로 확인했다.
그 output 역시 intent routine 1개를 포함했지만 `intent_execution`이 없었다.

반면 통과한 typed-compensation gate는 **native MIR oracle가 v3 plan을 생산하고
self MIR→C consumer가 그것을 실행**한다. 이 경로의 성공은 확실하지만
Pergyra source producer 전체의 closure와 동의어가 아니다. 이 감사는 두
관찰의 원인이 완전히 같다고 확정하지 않았고 source를 수정하지 않았다.

불완전한 self MIR 생산을 새로운 positive regression으로 고정하지 않았다.
새 runner는 위 native runtime 및 의미 거절로 범위를 명시한다. source plan
생산 경계와 누락 terminal의 빈 진단은 이후 owner 작업의 구체적인 반증 입력이다.

## 모델 정리와 구현의 구분

[IntentSpine.v](../semantics/proofs/IntentSpine.v)의 `checked_intent_guard_free`는
`participants_covered`, `deps_wf`, `comp_covered`와 `sched_ok`를 전제로 한다.
파일 자체가 interprocedural used-set 계산은 semantic pass의 의무이며 실제
emitter와 `sched_ok`의 일치는 gate 의무라고 명시한다. 이 감사는 해당 모델을
읽었지만 Rocq 전체 proof suite를 다시 실행하지 않았다.

따라서 정리가 존재한다는 사실을 현재 source compiler가 일반 INT-1~4를
모두 강제한다는 증거로 쓰지 않는다. Purpose/Trace label 변경이 검사를
바꾸지 않는다는 모델 결과도 trace를 독립 verifier primitive로 올리지 않는다.

## canon 충돌과 보강 방향

기준 `bf8b33d0`의 docs/01에는 새 판단 기준과 옛 조언이 함께 남아 있었다.
`보상 유무, action 수, step 수는 어느 쪽도 단독 판정 기준이 아니다`와
`Micro는 보상이 필요할 때만 Intent`, `step 10개/body 20줄이면 쪼개기`는
같은 규칙으로 적용할 수 없다. primary는
[docs/200](../200_object_to_action_boundary_patterns.md)에도 action 두 개 이상
기준과 action 개수 판정 금지가 함께 있음을 확인했다. 본 lane은 원문을
수정하지 않았고 통합 시 purpose-bound obligation 기준으로 정정하도록 넘겼다.

다음 보강은 새 키워드보다 기존 약속을 executable pair로 연결하는 편이 좋다.

1. 한-step 목적에서도 terminal/authority/boundary 귀속을 검사하는 positive와
   동일 데이터의 잘못된 귀속 negative를 유지한다. 이 감사가 terminal 사례를 추가했다.
2. 동일한 typed source가 생산한 plan을 자기 consumer가 실행하게 만드는
   producer 경계를 하나의 후속 rung으로 조사한다. native oracle의 성공을
   source 대체율로 더하지 않는다.
3. INT-1은 선언된 참여자와 helper를 통해 실제 사용한 참여자의 차이를
   source semantic receipt로 설명해야 한다. 단순 `who` alias 검사는 그 일부다.
4. INT-2는 효과 분류가 소유한 effectful step 집합과 보상 coverage를 결합해야
   한다. 보상이 실행됐다는 증거와 보상이 완전하다는 증거를 분리한다.
5. INT-3/4는 일반 데이터 dependency와 co-activity를 실제 분석 owner에서
   공급해야 한다. `priority` 하나를 대칭적인 충돌 부재 증거로 취급하지 않는다.

다른 게임·서버·데이터 pipeline의 실제 workload는 이번 lane에서 실행하지
않았다. 위 세 가지 source는 같은 작은 실험의 변형이지 독립 외부 workload
세 개가 아니다. Intent의 범용성이나 새 개념 도입 기준의 전체 통과를 주장하지 않는다.
