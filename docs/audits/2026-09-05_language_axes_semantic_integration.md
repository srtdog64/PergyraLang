# 언어 축 삭제·보강 의미 테스트 — 통합 결과

Status: AUDIT COMPLETE. 기준 revision:
`bf8b33d078b27c41cc6cdb7ffed2e8fa5c62ef22`.

사용자 요청으로 세 subagent가 Intent, Authority/Effect, Domain axes를
독립 조사했고 primary가 nominal 대체 실험과 통합 검증을 맡았다.
이 문서는 관찰 기록이며 언어 의미, SoT 상태, 후속 구현 rung의 owner가 아니다.
[작업지시서](../agent_work_directives/intent_and_language_axes_semantic_audit_2026-09-05.md)의
범위 안에서 source fixture와 검사 runner를 추가했으며 compiler source는 바꾸지 않았다.

## 결론

**Intent를 단순 sugar로 취급할 수 없는 실행 근거가 있다.** 한 action의 결과값과
호출 횟수는 `func + match`로 재현할 수 있었지만, 승인된 receipt를 같은 값의 새
receipt로 바꾸는 경우 typed Intent만 terminal 귀속 위반으로 거절했다. 따라서
action이 둘 이상이거나 보상이 있어야만 Intent가 필요하다는 기준은 틀리다.

동시에 모든 개념의 현재 표면이 최소라는 결론도 나오지 않았다. `using`에서 Zone이
정해지는 step의 중복 `where`는 생략해도 같은 fact가 남았다. 별도 domain 계약이
없는 state action은 hosted func로, 단순 immutable object read는 `struct`의 `let`
field로 같은 관찰을 만들었다. 삭제할 대상은 fact 자체인지 반복 표기인지 먼저
나누어야 한다.

가장 큰 미완료는 **이미 있는 의미 검사를 self-host source→MIR 경계까지 보존하는
일**이다. 새 통합 admission gate의 9개 항목이 실패했다. 이는 9개의 독립 버그라는
뜻이 아니다. immutable object/struct 두 사례처럼 같은 누락을 관찰하는 항목도 있다.
잘못된 수용을 성공 기대값으로 만들지 않았다.

## 증거별 판정

| 축 | 실제 대체·구별 실험 | 판정과 한계 |
| --- | --- | --- |
| Intent | 한-step Intent / exact 함수 / receipt를 새로 만든 함수는 C/LLVM에서 값·호출 수가 같다. 같은 재구성을 typed terminal에 적용하면 native/self 모두 거절 | CONDITIONAL: 목적에 귀속되는 checked bundle이 있으면 유지. 모든 기존 구문에 대한 비표현성 정리는 아님 |
| Capability | caps assertion을 지워도 CLOCK 추론은 남지만 random-only 상한을 두면 Now 사용을 거절 | KEEP-CORE. 선택적 선언 상한을 생략하는 것과 operation-grant fact 삭제는 다름 |
| Authority | 같은 Player 타입·cap mask에서 승인 owner 대신 observer를 사용하면 native 거절. authority 집합을 지우면 거절 의무가 사라짐 | KEEP-FACT. 특정 instance/slot의 승인은 operation mask와 다름. delegation history는 모델 증거만 있음 |
| coarse effects / domain effect | CLOCK이 허용돼도 local effect 약속은 Now를 거절. effect slot에 class를 결속하면 거절 | 분류 fact와 domain layer identity는 구별해서 유지. class 치환의 명목 거절만으로 모든 수동 인코딩을 배제하지 않음 |
| Ability / Role | 두 role의 explicit rebind와 직접 함수 호출은 `10, 77`이 같다. 후자는 열린 구현 계약과 문맥 binding을 보존하지 않음 | Ability KEEP-CORE, Role KEEP-FACT. 폐쇄된 호출 두 개의 출력 동치는 전체 치환이 아님 |
| World | free Zone의 intent argument는 수용, world-owned Zone의 live escape는 native 거절 | 소유 경계 유지. World-equivalent root fact 없이 Zone graph로 완전히 대체하지는 못했음 |
| Action | 단순 counter action과 hosted func는 `2, 2`가 같다. action-only within 계약은 func에 그대로 붙일 수 없음 | CONDITIONAL. mutation 자체는 고유 의미가 아님. 후자의 문법 거절을 새 정적 정리로 세지 않음 |
| within / intent where | optional within과 authorized_by를 따로 운반. explicit where를 지워도 using의 동일 Zone fact가 남고, 충돌은 거절 | KEEP-FACT, 반복 표기는 추론 가능. 이 실험은 Zone type 일치이며 모든 instance authority를 증명하지 않음 |
| generic where | 실제 인자 타입의 Sortable bound 위반을 native가 거절 | intent where와 별도 fact. 같은 spelling이 같은 semantic owner라는 뜻이 아님 |
| object / struct | immutable object와 struct let field 모두 C/LLVM에서 7. 양쪽 field write는 native에서 동일 typed diagnostic으로 거절 | 읽기 전용 값이라는 좁은 성질은 치환 가능. refresh/source freshness/projection/transfer까지 지운 실험은 아님 |

상세 근거와 owner/consumer는
[Intent 감사](2026-09-05_intent_graph_semantic_audit.md),
[Authority/Effect 감사](2026-09-05_authority_effect_deletion_audit.md),
[Domain axes 감사](2026-09-05_domain_axes_deletion_audit.md)에 있다.
`subject`, `class`, `vessel`, `tobject`는 각 fixture의 조합과 canonical 계약을
검토했지만, 각 경계의 완전한 삭제 encoding은 만들지 않았다. 기존 Slot/Zone/Scope
감사는 [앞선 matrix](2026-09-05_concept_deletion_stress_matrix.md)의 증거이며 이번에
전부 재실행한 것은 아니다. 146개 언어 단어의 전수 의미 검증도 아니다.

## Primary nominal 실험

[nominal/run.sh](../../tests/concept_semantics/nominal/run.sh)는 네 source를 유지한다.
`object View { value: Int; }`와 `struct View { let value: Int; }`를 생성한 뒤 읽으면
native C/LLVM 네 실행 모두 정확히 `7`이다. construction 이후 `view.value = 9`를
추가한 두 source는 `PGY_SEM_IMMUTABLE_FIELD_WRITE`로 거절되고 MIR stdout가 비어 있다.

설치 self-host는 두 write source를 모두 MIR로 내보냈다. 이것은 object를 삭제해도
된다는 뜻이 아니라 field mutability를 source admission이 보존하지 못한다는 관찰이다.
잘못된 MIR는 실행하지 않았다. projection의 refresh 관계, detached publication,
vessel parameter ABI, subject identity copy를 이 read/write 쌍으로 증명하지 않는다.

## 열린 source-admission 의무 — 실제 RED

재현 명령:

```sh
timeout 300 bash tests/concept_semantics/source_admission_parity.sh
```

Primary 실행 결과는 **exit 1, 9 claims / 9 failures**, 약 3.3초다.
작업 로그는 `.tmp/self_hosted/concept_semantics_20260905/source_admission/run.pxTyMT/`에
있다. 각 negative는 먼저 native의 지정된 의미 진단과 artifact 부재를 확인한다.
잘못된 source의 MIR를 코드 생성하거나 실행하지 않는다.

| 검증 항목 | 설치 self-host 관찰 | 다음에 필요한 증거 |
| --- | --- | --- |
| clock capability 상한 위반 | exit 0, MIR 발행 | 기존 admitted capability verdict가 production publication을 막는 소비 경계 |
| coarse effect 상한 위반 | exit 0, MIR 발행 | body-derived effect와 선언 상한의 실제 검사 |
| same-type observer의 미승인 mutation | exit 0, MIR 발행 | 정확한 subject slot identity와 승인 관계의 call/mutation 결합 |
| class를 effect slot에 결속 | exit 0, MIR 발행 | nominal effect/participant identity의 admission |
| object / struct immutable field write — 2개 항목 | 둘 다 exit 0, MIR 발행 | field mutability fact를 assignment consumer까지 보존 |
| generic actual의 Ability bound 위반 | exit 0, MIR 발행 | specialization/call consumer의 bound 검사 |
| world-owned Zone의 live argument escape | exit 0, MIR 발행 | world resource provenance의 argument-boundary 검사 |
| 정상 typed Intent의 source plan | exit 0, MIR는 있으나 intent_execution 없음 | source producer가 v3 step/terminal/Zone/payload identity를 실제 생산 |

마지막 항목의 native control은 v3 plan을 생산한다. self output의 JSON 파싱은
성공했고 `source MIR omitted intent_execution` assertion에서 실패했다. 파일 형식
오류나 실행 환경 실패를 의미 실패로 세지 않았다. 이 schema/carriage 검사는 full
plan readiness나 실행 parity의 대체물이 아니다.

기존 typed compensation green 경로는 **native v3 MIR producer → self MIR consumer**다.
이를 self source producer까지 끝났다고 읽지 않는다. 별도 owner probe 두 개는
compile 단계에서 막혔으므로 INCONCLUSIVE이고, generic 정상 source의 LLVM
specialization도 argument metadata 부족으로 실패했다. 구체 진단은 각 lane 보고서에
보존했다. Rocq/Lean 정리를 새로 만들거나 이번 감사에서 재실행하지 않았다.

## 검증과 해석

유지된 정상 계약은 다음 명령으로 실행한다.

```sh
bash tests/concept_semantics/run.sh
```

Primary는 네 lane을 각각 실행해 모두 PASS를 확인한 뒤, 위 통합 명령도 실행해
4/4 PASS를 확인했다. 각 lane은 5분 제한이다. documentation quality와 146-row
keyword registry gate도 PASS이며, 새 보고서/README의 로컬 링크 81개와 신규 파일
43개의 UTF-8/공백, shell syntax, scoped diff를 확인했다. Keyword occurrence
inventory는 정식 generator로 갱신했고 registry 의미나 lexer/editor projection은
바뀌지 않았다.

이 green 묶음과 위 red admission gate는 서로 다른 주장이다. 현재 Makefile/CI에
새 job을 연결하지 않았고 전체 CI를 다시 실행하지 않았다. 기준 commit의 원격 run
`33922587191` 30/30 green은 새 uncommitted 테스트까지 실행했다는 증거가 아니다.
이 감사의 변경은 아직 stage/commit/push하지 않았고 다른 세션의 dirty 파일은
보존했다.

실행 binary SHA-256은 시작과 primary 통합 때 동일했다.

- Native local: `0F9F4F30255D6850B5A773E21D5815F776B305E5C01A7A2C3DF6D373BB15A29E`
- Installed DRV-2: `FB37EA36D92E9C28B6BB7162F87BA00E733255AD5E46B24A166578713DF75847`

Native는 다른 세션의 rebuild 결과다. base commit의 CI binary와 같다고 주장하지
않는다. `CLOSED=55 BRIDGE=32 ACTIVE=1`, 88/185, 통합 83%, hard replacement 75%를
이번 테스트 수나 문서 수로 올리지 않는다.

## 문서 정정과 다음 보강 후보

[docs/01](../01_intent_first_design.md)의 compensation-only Micro 규칙과
[docs/200](../200_object_to_action_boundary_patterns.md)의 action 두 개 이상 규칙을
기존 purpose-bound checked-bundle 원칙에 맞췄다. step/body 개수는 검토 신호이며
자동 분할 기준이 아니라고 정정했다. 앞선 matrix의 cross-step 필요조건도 후속
실행 증거에 따른 정정임을 표시했다. 새 언어 의미나 키워드는 도입하지 않았다.

후속 source 작업의 가장 작은 조사 후보는 Capability다. Primary가 읽은
[manifest owner](../../src/self_hosted/compiler/capability_manifest_owner.pgy)는
`SemanticAstCapabilityFactsFromAdmittedBody`와 readiness/contract verdict를 소비한다.
반면 확인한 [rung-2 admission 함수](../../src/self_hosted/compiler/driver_rung2_owner.pgy)는
analysis/body receipt를 검사한 뒤 projection으로 넘어가며 이 capability verdict를
직접 소비하지 않는다. 이는 root-cause 조사 지점이지 전체 원인 확정이나 구현 완료가
아니다. 새 정책을 복제하거나 native를 재호출하지 말고 기존 owner와 마지막
[source-MIR publication consumer](../../src/self_hosted/compiler/driver_source_mir_execution_owner.pgy)를
연결하는 하나의 실행 경계를 먼저 검증해야 한다.

다른 red 항목을 동시에 구현하는 track은 열지 않았다. 단일 enum 실행 rung의
발견 상태는 handoff에 보존했다. 다음 구현을 선택할 때는 여기의 red source 하나를
그 rung의 명시적 falsifier로 사용하고, 문서나 owner 수만으로 진척을 세지 않는다.
