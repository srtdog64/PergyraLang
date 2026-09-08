# Pergyra — 현재 진행 상황

마지막 업데이트: 2026-09-07. 이 문서는 진행 지표의 해석과 우선순위만 소유한다.
현재 후보·실행 결과·다음 반례는 [단일 활성 handoff](current_work_handoff.md)에 둔다.
실행 기록을 여기에 덧붙이지 않는다.

## 현재 판단

핵심은 **공개 컴파일러와 검사기가 같은 정전 언어를 구현하는 것**이다.
부트스트랩의 byte equality, 파서 지원 수, owner 수는 이 조건을 대신하지 않는다.
잘못된 소스를 거절하는 것뿐 아니라 올바른 소스의 수용과 관측 가능한 실행
의미도 같아야 한다. 네이티브에서 발견한 버그까지 복제하는 것은 parity가 아니다.

기존 설치 바이너리의 104개 실험 중 34개 불일치는 역사적 관측이다. 현재 소스
전체에 대한 재측정으로 표시하지 않는다. 2026-09-07에 source-matched 후보로
다시 실행한 최소 admission gate도 간극을 확인했다. 정확한 후보와 후속 수정
결과는 handoff 및 해당 gate의 보존 로그에서 확인한다.

## 지표와 해석

| 지표 | 표시 | 무엇을 뜻하는가 |
| --- | --- | --- |
| strict beta readiness | 기존 공식선 83% 유지 | [beta checklist](100_beta_readiness_checklist.md)의 기존 평가. 이번 작업의 테스트 통과율이 아니다. |
| 전체 작업 예측 | 기존 추정 83% / 81–85%, 재산정 보류 | admission 간극의 남은 작업량을 재측정하기 전에는 상승이나 완료 시점의 근거로 쓰지 않는다. |
| hard self-host replacement | 기존 추정 75%, 재산정 보류 | 실제 C 경로 대체와 의미 보존이 함께 필요하다. 새 파일·fixture·문서 수는 진척이 아니다. |
| SoT 폐쇄 | [owner registry](semantics/sot_owner_spine_registry.md)에서 확인 | CLOSED/BRIDGE/ACTIVE 수를 여러 문서에 복제하지 않는다. 소비자 이전·누락 거절·옛 경로 삭제·negative gate가 폐쇄 조건이다. |
| 현재 소스 bootstrap | 미완료 | 이전 리비전의 fixed point는 남아 있지만 현재 dirty 소스 전체의 비교 및 gen2/gen3 완료 증거로 전용하지 않는다. |
| 현재 로컬 CI 통합 | 미확인 | 게시 HEAD의 과거 green은 이후 로컬 변경을 포함하지 않는다. 실제 실행한 좁은 gate와 전체 CI를 구분한다. |

## 작업 순서

1. 공개/native admission 및 실행 의미를 맞춘다. 현재 좁은 범위와 다음 반례는
   handoff에서 확인한다. 정전의 규칙을 선언 fact에서 최종 검사까지 운반한다.
2. 새 표면 도입을 멈추고 기존 receiver, 평가 순서, 미구현 정책 절,
   guard/expect/post의 의미를 각 owner에서 닫는다.
3. 선행 조건을 확인한 뒤 [동시성 방향](204_concurrency_direction_pscc_review.md)의
   기존 증명 목표 하나를 실제 runtime 경로에 연결한다. 동시에 새 구현 축을 열지 않는다.
4. current-source bootstrap과 설치/통합 경계까지 다시 검증한다.

Module Build 구현은 self-host closure 이후다. owner 단위 문서 정리는 이 순서를
지원해야 하며, 의미 수정 대신 문서만 늘리는 독립 작업 축이 되어서는 안 된다.

## 재현과 증거 규칙

- 최소 통합 반례: `tests/concept_semantics/source_admission_parity.sh`.
  열린 주장은 계속 RED로 둔다. 무관한 syntax 오류나 시간 초과는 올바른 거절이 아니다.
- 과거 삭제 실험: [실행 매트릭스 감사](audits/2026-09-06_language_word_deletion_execution_matrix.md).
  관측 수집기와 acceptance gate를 혼동하지 않는다. 잘못된 프로그램은 실행하지 않는다.
- 활성 실행: [handoff](current_work_handoff.md). 편집권은
  [짧은 협업 문서](current_work_collaboration.md)에만 둔다.
- full matrix는 통합/예정 경계에서 실행한다. 매번 전체 테스트를 돌리거나
  실패를 감추기 위해 gate를 줄이는 것은 동일화의 근거가 아니다.

## 과거 기록

이전 3,845줄은 [원본 보관본](audits/archive/progress_before_admission_priority_2026-09-07.txt)에
byte-for-byte 보존했다. SHA-256:
`C80E29480F63B86795D37D976B6AE0F89711B9513E5D3AF4AA2480A90399D288`.
보관본의 과거 ACTIVE/100% 표시는 현재 작업 큐나 현재 소스의 완료 판정이 아니다.
