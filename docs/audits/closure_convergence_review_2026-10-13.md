# SoT 폐쇄 정체와 기존 핵심 수렴 검토

Date: 2026-10-13 KST. Status: dated review of preserved shared WIP.
Base: `0994eb9b8867e2cef4feaecc45ba43fc2c42d02d`.

**9월 말 이후 등록표의 순폐쇄가 정체됐다. 현재 작업은 전체 SoT 폐쇄보다
DRV-2 소유권 admission 복구와 그 검증에 묶여 있다.** 국소 검증 통과와
구현량 증가는 마지막 소비자 이전·옛 경로 삭제·설치본·CI를 대신하지 않는다.
사용자는 더 많은 핵심 개념을 도입하기 어렵다고 판단했고, 기존 계약의 구현과
검증에 수렴하도록 요청했다. 채택한 방향은
[VISION의 기존 핵심 수렴과 완료 기준](../00_vision.md#기존-핵심-수렴과-완료-기준)에 둔다.
이 검토는 새 의미 소유자나 구현 rung을 만들지 않으며 registry 상태를 바꾸지 않는다.

## 소유권과 SoT와 DRV-2의 관계

| 구분 | 책임 | 완료로 대체할 수 없는 것 |
| --- | --- | --- |
| 소유권과 자동 정리 | 복사·이전·대여·변경·정리의 값 수명 계약 | Rocq 모델 통과를 실제 compiler/runtime 정제로 간주하기 |
| SoT 폐쇄 | 판단의 단일 owner, 마지막 소비자 이전, missing-fact 거부, 옛 경로 삭제 | owner 파일·문서·fixture 수 또는 선언된 CLOSED 개수 |
| DRV-2 | 의미 검사된 소스를 MIR/C로 잇는 driver 통합 경로 | 소유권 알고리즘 전체, 모든 backend, 언어 전체의 메모리 안전성 증명 |

`DRV`는 Driver 사다리의 약어다. DRV-0은 단계 조립, DRV-1은 CLI 표면,
DRV-2는 artifact-body semantic source 변환, DRV-3는 외부 native compile까지
소유하는 단계로 [doc150](../150_selfhost_driver_lsp_wiring.md#1-driver-사다리-drv)에 정의됐다.
현재 소유권 작업에 DRV-2 이름이 붙는 것은 그 실제 경로가 컴파일러 자신의
소스를 처리하면서 막히기 때문이다. `DRV-2 = 소유권 완성`이라는 동일시는 부정확하다.

SoT 문서 작업을 전부 마친 다음 소유권을 추가하는 두 단계가 아니다.
같은 producer → typed fact → consumer → runtime/backend 사슬을 닫으며
도달한 SoT seam을 함께 닫고 DRV-2로 통합한다. 이것은
[hard self-host contract](../self_hosted/10_hard_self_host_contract.md#sot-rule)의
기존 조건이며 새 폐쇄 트랙이 아니다.

## 등록표의 날짜별 변화

아래는 `docs/semantics/sot_owner_spine_registry.md`의 지정 registry block을
각 Git tree와 현재 파일에서 읽어 `owner_id`별로 집계한 **선언 상태**다.
gate를 전수 재실행한 결과나 전체 언어의 완료율이 아니다.

| 기준 | Revision | 전체 row | CLOSED | BRIDGE | ACTIVE | 미폐쇄 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 2026-09-01 마지막 조상 snapshot | `0c6e15139893328bce99899d0f715941bcf514ee` | 88 | 55 | 32 | 1 | 33 |
| 2026-09-30 마지막 조상 snapshot | `6d3eb90313f190f1cf5091f4cd9c687218c388bf` | 95 | 69 | 24 | 2 | 26 |
| 현재 공개 HEAD | `0994eb9b8867e2cef4feaecc45ba43fc2c42d02d` | 95 | 70 | 23 | 2 | 25 |
| 현재 보존된 dirty 작업 트리 | 같은 HEAD + WIP | 95 | 69 | 23 | 3 | 26 |

현재 파일 SHA-256은
`7FA4C7F9672140C28BF812FF59AE2DBE3E77B5831E25B347F996B760A6EE5562`다.
공개 HEAD와 작업 트리는 같은 상태가 아니다. 현재 26개를 공개 HEAD의 수치로
표현하거나, 표의 CLOSED 표시를 지금의 compiler CI green으로 표현하지 않는다.

9월 1일과 현재의 공통 identity는 88개다. 그중 기존 미폐쇄에서 CLOSED로
바뀐 row는 8개이고, 새 row는 7개이며 그중 6개가 CLOSED다. 따라서 9월 전체에
폐쇄가 전혀 없었다는 결론은 맞지 않는다. 신규 row를 기존 row 폐쇄와 섞으면
같은 작업 집합의 속도를 판단할 수 없다.

반면 9월 말과 현재는 같은 95개 identity다. `abi.mir_array_string_layout_projection`은
BRIDGE에서 CLOSED로 바뀌었고, `semantic.function_param_flow_summary`는 CLOSED에서
ACTIVE로 돌아갔다. **선언상 승격 1개와 재개방 1개, 순변화 0개**다. 최근 정체라는
사용자의 관찰은 이 비교에서 지지된다. 재개방은 기존 증거를 숨기기 위한 숫자 조정이
아니라 현재 소비자·음성 검증이 다시 필요한 상태로 읽어야 한다.

재집계는 각 revision에 `git show <revision>:docs/semantics/sot_owner_spine_registry.md`를
적용하고 `BEGIN sot-owner-spine-registry`와 `END sot-owner-spine-registry` 사이의
owner identity와 상태를 비교한다. 상태 문자열 전체 검색이나 historical prose의
OPEN/CLOSED 단어 수를 세지 않는다. 실질 폐쇄 속도를 평가하려면 각 전이의
마지막 소비자·삭제된 old path·음성 gate·실행 revision과 CI를 별도로 확인해야 한다.

## DRV-2 복구가 길어진 원인

자동 정리 전환은 해제 호출을 추가하는 작업이 아니다. 실제 backing 소유자,
값 복사와 서술자 공유, 반환값의 출처, call 이후의 책임, 모든 continuing exit의
복원을 실제 producer와 consumer에 연결해야 한다. 기존 계약과 구현의 빈틈,
새 분석기의 판정 부족, 수정 중 들어온 실제 회귀가 함께 드러났다.

확인된 한 경로는 `MirIntentExecutionCaptureCompensations`다. 구조체의 배열
서술자를 local로 꺼내 성장시킨 뒤, 후속 JSON 오류에서 부모 필드에 현재 서술자를
돌려놓기 전에 반환할 수 있었다. caller는 은퇴한 backing을 가리킬 수 있다.
이 거부는 단순히 정상 코드를 과도하게 막는 것으로 판정되지 않았다.
[실제 focus/exit 사슬](../agent_work_directives/drv2_field_focus_exit_recovery_2026-10-13.md#whole-affected-chain)이
normal·early-return·handled-error·nested call 복원을 함께 요구한다.

반대로 읽기 전용 임시값, 독립 scalar snapshot, input-only 호출과 실제 retention을
구분하지 못한 판정도 복구 대상이었다. 또 S/T 변경 `8c5cc3fe`에서 HashMap 서술자
복사본을 변경하여 재해시가 원본 backing을 은퇴시키는 실제 버그가 들어갔다.
[Valen 비교 감사](valen_group_borrowing_comparison_2026-10-11.md#counterexample-this-comparison-exposed)는
그 반례와 당시 map 성장 요약 누락을 기록한다. 안전 검사를 약하게 만드는 것으로
이 둘을 함께 해결할 수 없다.

작업 기록의 [ownership-entry frontier](../agent_work_directives/drv2_ownership_entry_frontier_2026-10-11.md#how-the-frontier-formed)는
이전 CI가 앞단에서 막힌 동안 뒤의 판정 규칙이 자체 컴파일로 검증되지 않은 채
누적됐다고 지적한다. Git tree 비교에서도 `aed8a9049`부터 현재 HEAD까지 semantic
디렉터리의 216개 파일, 추가 22,868행·삭제 3,471행이 확인된다. 이는 변경 범위의
크기이지 진척 수치나 각 회귀의 인과 증명은 아니다. 원인 책임별 좁은 테스트와
whole-chain 통합 착지가 분리된 채 큰 WIP로 이어진 것이 지연을 키운 운영 문제다.

## 현재 검증 경계

새 let/var 표기의 기본 C/LLVM 실행과, 고정 focus 후보의 생성-C 실행·거부·zone
정리 검증은 통과했다. 마지막 후보는 기존 13개와 var 5개 실행, 거부 7개,
poison 19개와 zone harness 3개를 원래 예산에서 확인했다. 같은 후보의 독립
리뷰도 해당 gate 차이에서 새 결함을 찾지 않았다. 이 범위는 native target LLVM/MIR
focus recovery, full canonical, 설치본이나 전체 compiler CI를 포함하지 않는다.

마지막 정확한 HEAD CI 기록 `38374181905`는 bootstrap에서
`unproved_exclusive_member_mutation`으로 거부돼 RED이며, 종속 compiler job 6개는
SKIP이다. 일부 플랫폼·formal job의 PASS를 전체 compiler green으로 세지 않는다.
진단 node ID는 parser 세대마다 다시 발급되므로 이전 숫자를 현재 identity로 쓰지 않는다.

private canonical 관측은 원래 300초 예산에서 미완료다. 마지막 관측에서는
statement 단계 89.515초, 환경 35,248쌍의 구간 합 20.945초가 기록됐고 generic
단계에서 deadline에 도달했다. 로그·flush·host capture가 포함된 wall 관측이므로
환경 재생성을 주된 CPU 병목으로 확정하지 않는다. 이는 compiler 분석 비용이며
사용자 프로그램의 자동 cleanup 비용을 측정한 결과가 아니다.

최종 body verdict와 최신 전체 실패 센서스도 미관측이다. structural error는 이전
row prefix를 버릴 수 있고, expression/ownership verdict는 첫 실패만 남기며,
후속 owner는 아예 실행되지 않을 수 있다. empty/default Ok, 첫 진단 하나와
35,248개 관측 쌍을 전체 통과나 전체 남은 오류 수로 바꾸지 않는다. 현재 세대의
출처·실행 파일·receipt·미해결 항목은 [active handoff](../current_work_handoff.md)에 둔다.

## 수렴을 위한 실행 경계

현재 public 의미와 지원 약속을 유지한다. 새 핵심 개념으로 구현 거부를 덮지
않으며, 이미 정한 안전 계약에서 필요한 내부 fact를 생산하고 마지막 소비자를
이전한다. 실제 반례를 고치는 일과 가상의 정밀화·새 연구 트랙을 분리한다.
필요한 내부 사실 보강이나 책임 분리는 금지하지 않지만, 기존 owner로 해결하지
못하는 책임과 실제 비용을 먼저 밝혀야 한다. 새 public 개념은 별도 승인 대상이다.

기본 소유권의 종료 조건은 [기존 고정 끝선](../agent_work_directives/drv2_canonical_recovery_2026-10-12.md#fixed-ownership-baseline-finish-line)을
그대로 따른다. 고정 입력의 거부 0, unchanged full canonical 컴파일, 기존 음성
검증 유지, 같은 공개 commit의 필수 compiler CI와 실제 bootstrap·설치본 검증을
마치면 이 기준선을 끝낸다. hypothetical 반례나 owner/model 수로 끝선을 늘리지 않는다.

진척 보고는 같은 owner 집합에서 새 폐쇄·재개방·남은 의무를 분리하고, 실제
C 경로 대체와 최종 소비자의 실행 증거를 연결한다. native C 선행 수정, 좁은
회귀 검증, 문서화와 성능 진단은 지원 작업으로 따로 표시한다. 이전 97/115 등
다른 input/revision의 오류 수를 새 고정 센서스 없이 현재 남은 개수로 재사용하지 않는다.

언어의 첫 안정 버전과 전체 자체 compiler 완성은 구분한다. 지원 범위의
소유권·정리·ABI·실패 경계를 backend/runtime까지 연결하고 설치본으로 외부 실제
프로그램을 작성·빌드·실행하는 것이 사용 가능한 언어의 기준이다. 전체 자체
호스팅이 일반적인 첫 사용의 필수 전제는 아니지만, 현재 사용자가 선택한 compiler
완성 목표와 active DRV-2를 이 구분으로 취소하지 않는다. Alrescha는 독립
framework이며 compiler 의미 소유자나 완료를 증명하는 내부 테스트로 흡수하지 않는다.

완료 날짜는 최신 전체 실패 집합과 같은 입력의 순폐쇄 추세가 있어야 산정할 수 있다.
대화에서 언급한 수주·수개월은 위험 범위에 대한 판단이지 관측된 속도에서 계산한
ETA나 작업 약속이 아니다. 문서 추가는 이 정체를 명료하게 기록하는 것이며,
SoT 폐쇄·자동화 구현·언어 완성을 대신하지 않는다.
