# Claude·GPT 협업: 소유권 기반 메모리 관리 다음 분담

> 2026-10-09 최신 승인: "구현 시작해 클로드가 시작했으니까"와 "전체 작업해"로
> 아래 GT3의 과거 구현 보류는 해제됐다. 현재 GPT 실행은
> [실행 지시서](ownership_cutover_execution_2026-10-09.md)를 따른다.
> P0 동결·P1 의존 장벽·최종 설치 조건은 그대로이며, 과거 완료 기록은
> 현재 production 폐쇄 증거가 아니다.

상태: `ACTIVE COORDINATION`. 작성: 2026-10-08 KST, Claude, 사용자 요청.
기준: `main @ 3658548d24bca3d721e4f1974ac7a10da99f7aa8`, 공유 dirty main(작성 시
Windows Git 305개 항목, staged 0개). 이 문서는 분담과 순서만 정한다. 의미는
아래 주인 문서와 증명, 판정은 실행 게이트가 가진다.

이 디렉터리에서 날짜가 지난 지시서는 이날 `OLD`로 표시했다. 지우지 않았고
원래 상태 줄도 그대로다. 인계 문서의 활성 카드가 아직 가리키는
`mir_builder_ownership_chain_2026-10-07.md`만 표시하지 않았다.

## 목적 카드

- **목적:** 소유권 기반 자동 메모리 관리. 일반 값은 컴파일러가 만든 정리로 마지막
  사용 지점에서 정확히 한 번 해제하고(ownership-clean), 그래프는 노드마다 주인
  하나·비소유 링크·해체 정리로 고아와 끊긴 링크 없이 해제한다(teardown).
- **우선순위:** AGENTS.md 순서. 의미 정체성과 한 SoT, 주인 주도 사실, 폴백 제거,
  음성 래칫, 그다음 패치 크기.
- **사실 주인:**
  - 값 정리: [27번 문서](../semantics/27_ownership_clean.md) §5의 MIR 소유권 계약과
    `OwnershipClean*.v` 증명
  - 그래프 해체: [`OwnershipTeardown.v`](../semantics/proofs/OwnershipTeardown.v)
  - 저장 경계 합성: [28번 문서](../semantics/28_memory_boundary_composition.md)
- **금지:** 증명에 없는 규칙을 구현이 지어내는 것, RC·GC·"누수로 대신" 폴백,
  사용자 표기(`own`/`ref`, carrier/restore) 추가, Coq 8이나 추출 모델 실행을
  구현 증거로 내세우는 것.

## 현재 입력 (2026-10-08 밤)

| 파일 | SHA-256 |
|---|---|
| `docs/semantics/proofs/OwnershipTeardown.v` | `4babfc268dee283f377c0e0ae337d8afffdda594133e4f0ea1a3f416c0b93369` |
| `tests/coq/OwnershipTeardownRedteam.v` | `fe021db2894417c3040cb01a26ac7270125813bba7f33256efaa2ac95e9e971e` |
| `tests/coq/OwnershipTeardownExtraction.v` | `3f7ce538c90d4987458aef89dff35f5e6c11d91f5eb5fb3dc713d6acb994756d` |
| `tests/ocaml/ownership_teardown_driver.ml` | `932d3ab09d7a8a1a8cdb09ab99fe32e3d5e4f54721f2de9afc0d7f0695bde102` |
| `tests/ownership_teardown_redteam_smoke.sh` | `f23724d7e3136f450ef426be97d00fb01bfcd4615fdb54d944137dd4aeb151d3` |
| `docs/semantics/proofs/OwnershipGraphLinks.v` | `035bf8a85705c075cd7a3ee2276b9ba9aafd91d634a91dcbaaebabab45a62eac` |

값 정리 증명 4개의 해시와 구현 항목 I1–I8은
[구현 지시서](ownership_clean_implementation_2026-10-08.md)에 있다.

이날 Claude가 마지막으로 한 일:
- 모델에 `released_unit_local_read_none_forever`와
  `dropped_root_unit_local_read_none_forever`를 넣었다. 해제·루트 해제 단위의
  모든 노드에 대한 저장된 지역 링크가 이후 모든 상태에서 None으로 읽힌다.
- 반례 파일에 `root_drop_has_no_holder_permission_premise`(루트 핸들만으로 루트
  전체 해제, OPEN 유지)와 `saved_child_local_refused_after_root_drop`을 넣었다.
- `tests/ownership_teardown_redteam_smoke.sh`를 다시 돌려 PASS를 확인했다.
  3개 모듈 가정 0개, 검사 변이 11개와 비용 측정 변이 2개 모두 거부됐다.
  로그는 `.tmp/rocq93_ownership/teardown-redteam-smoke-claude-unit-read.log`다.

## 역할

사용자 배정(2026-10-08)을 따른다. Claude는 증명 core, 반례, 리뷰를 맡는다.
GPT는 구현, 추출·실행 게이트, 영수증을 맡는다. 한쪽이 다른 쪽 범위를 고쳐야
하면 먼저 아래 협업 기록에 남긴다.

## Claude가 할 일 (순서대로)

| ID | 할 일 | 수용 조건 |
|---|---|---|
| CL1 | **해제 권한과 활성 대여(loan/pin) 경계 모델.** 복제할 수 없는 소유 핸들을 가진 쪽만 노드 해제와 루트 해제를 할 수 있게 하고, 대여 중인 노드의 은퇴는 거부한다. 인계 문서가 정한 다음 반증 입력이다. | 반례 파일의 `link_target_release_has_no_owner_permission_premise`와 `root_drop_has_no_holder_permission_premise`가 같은 입력에서 거부로 바뀐다. 정당한 스코프 종료는 계속 항상 성공한다. 기존 불변식이 모두 유지된다. 가정 0개. |
| CL2 | **저장소 모델과의 합성.** `OwnershipGraphLinks.v`의 저장소 상대 링크 안에 노드 단위 소유 트리를 두는 형태를 증명하고, 28번 문서의 `RetireValid`와 연결한다. | 저장소 정리와 노드 해체가 같은 힙 결과를 내는 합성 정리. 저장소 id 재발급 반례가 그대로 거부된다. |
| CL3 | **유한 세대.** 해체 모델의 노드·루트 세대를 `OwnershipGraphLinks.v`처럼 최대값에서 은퇴시킨다. | 세대를 나머지 연산으로 돌리면 옛 핸들이 되살아난다는 반례와, 은퇴 방식에서 옛 핸들이 영구히 거부된다는 정리. |
| CL4 | **실행 가능한 단위 순회의 정확성.** 자식 색인을 걷는 순회 함수를 정의하고, 정확하고 중복 없는 해제 단위를 낸다는 것을 증명한다. 지금은 `enum_below`가 존재만 보인다. | 순회 결과가 `unit_from_kids`·`root_unit_from_kids`의 단위와 같고 `NoDup`이라는 정리. 비용 측정은 GPT의 GT2. |
| CL5 | **GPT 구현 슬라이스 리뷰.** 각 I 항목이 27번 문서 §5 계약과 증명대로인지 검토하고, 계약에 없는 규칙이 필요하면 core를 추가한다. | 슬라이스마다 리뷰 기록. 계약에 없는 규칙을 구현이 지어내지 않는다. |
| CL6 | **에러로 끝나는 피호출자의 끝에서 끝 회수(2026-10-09 착수).** 조기 반환·에러·지역 break/continue가 있는 피호출자 본문을 상태 변수로 출구 없는 `SStmt`로 낮추고(exit elimination), 그 결과를 core의 `SProcTable`에 그대로 넣는다. 호출자 쪽 묶기→`SCallIO`→풀기와 합쳐 모든 계속 가능한 출구에서 inout 전부와 결과 packet이 호출자에게 돌아옴을 증명한다. 새 target 규칙(`recovery_exec`)에 기대지 않는다. | `OwnershipCleanCallLowering.v` 가정 0개. 낮추기 시뮬레이션 정리, 피호출자 adapter 실행 정리, 호출자 끝에서 끝 정리. 실제 반환·에러 경로를 가진 피호출자로 `elab_proc` 성공과 누수 없는 실행을 보이는 감사 예. |
| CL7 | **OPEN — view와 충돌하는 구조 변경 거부.** 성장(`TPush`)과 이전(`TMove`)에 drop과 같은 구체 소비자를 두고 core 규칙의 정제를 보인다. 그다음 target 문장 위의 정적 view 검사기를 정의하고, 검사를 통과한 프로그램에서 동적 oracle이 view 사용을 거부하지 않음을 증명한다. | 구체 소비자의 정제 정리와 활성 view 거부 정리, 거부·허용 감사 예. 정적 검사기 건전성 정리. 가정 0개. 별도로 납품된 whole-backing focus는 이 oracle 연결이나 전체 정적 발급자 증명이 아니다. |

CL1이 먼저다. CL2–CL4는 CL1 다음, 서로 독립이다. CL6·CL7은 전환 P1의 남은 증명이다.
CL6은 본문 실행·출력/반환값 정의·admission을 전제로 정규화된 core 호출자의 회수를
증명했다. 실제 호출자의 packet 해석·에러 처리기/전파 낮추기, 누락 지역 변수의
일반 거부와 출력 정의 발급, 정규화 전 source 호출과의 동치는 아직 OPEN이다.
CL7은 `OwnershipCleanViewScope.v`의 제한된 focus 대안을 증명했지만 원래 행의
수용 조건은 OPEN이다. 그 모델은 호출로 view를 넘기지 못하므로, 고정된 Slice
센서스의 76개 호출 인자 사용을 하나도 덮지 않는다. 새 파일을 등록하지 않으면
`formal_semantics_smoke.sh`의 목록 완전성 검사가 실패하므로, 예외 1에 따라 그 목록에
두 줄만 더했다. 집중 게이트나 audit 소비자를 따로 둘지는 GPT가 정한다.

## GPT가 할 일 (순서대로)

| ID | 할 일 | 수용 조건 |
|---|---|---|
| GT1 | **해체 게이트 유지.** Claude의 모델 변경마다 반례 파일·추출·OCaml 관측기를 이전하고 `tests/ownership_teardown_redteam_smoke.sh`를 녹색으로 유지한다. CL1이 들어오면 권한 반례 두 개가 거부로 바뀌었음을 변이 검사로 고정한다. | 집중 게이트 PASS, 변이가 실제로 거부됨, 영수증 갱신. |
| GT2 | **순회의 추출과 비용 관측.** CL4의 순회 함수를 추출해 정확·고유 단위 생성을 같은 고정 입력에서 관측하고, 지금의 이차 비용 `unit_unique` 관측기와 비교한다. | 같은 입력에서 결과가 기존 단위와 일치하고 비용 행이 기록된다. OCaml 모델 비용이지 런타임 성능이 아니라고 적는다. |
| GT3 | **값 정리 구현을 한 번에 전환(전체 구현 보류).** [전환 계획](ownership_cutover_plan_2026-10-08.md)의 P0–P10은 마지막 검토 후 착수 확인을 받으면 따른다. 후속 승인은 네 선행 항목의 로컬 기준선·계약·필수 importing 증명 preflight만 연다. I1–I8 집중 조건을 장벽으로 유지하며 공개 착지는 한 번이다. P1은 GPT 통합, Claude 계약·증명 검토다. | 기준선 checkpoint는 고정; 전체 P0 매트릭스와 P1은 아직 미완료. 최종 착지는 exact-SHA CI를 포함한 계획의 게이트 8개가 녹색일 때만. 같은 main worktree·3 GiB 상한·음성 fixture 보존. 원격/설치/GUI 권한과 증거는 별도 확인. |
| GT4 | **런타임 색인 표현 제안.** 역방향 색인과 소유자별 자식 색인을 C/LLVM 런타임에서 어떤 자료구조로 둘지 제안한다. 큰 행의 필터와 append 비용, 이차 관측기 한계를 포함한다. | 제안서(audits). 구현이 아니다. CL1이 정한 권한·대여 경계에 맞춘다. |

GT1은 계속한다. GT2는 CL4 다음, GT4는 CL1 다음이다. GT3의 현재 범위는 네 항목의
기준선·계약·필수 importing 증명과 읽기 전용 Claude 검토다. 전체 구현은 보류한다. graph CL2–CL4를 일반 값
cutover의 추가 선행 조건으로 바꾸지 않는다. 과거 hold 기록은 당시 사실로 보존한다.

## 편집 범위와 겹침 금지

| 담당 | 편집하는 파일 | 편집하지 않는 파일 |
|---|---|---|
| Claude | `docs/semantics/proofs/OwnershipTeardown.v`, 새 증명 core(`docs/semantics/proofs/`), 27·28번 문서의 증명 근거 절, 이 문서의 Claude 항목과 협업 기록 | 컴파일러·런타임 소스, `tests/ocaml/`, 게이트 스크립트 |
| GPT | `tests/coq/OwnershipTeardown{Redteam,Extraction}.v`, `tests/ocaml/ownership_teardown_driver.ml`, `tests/ownership_teardown_redteam_smoke.sh`, 컴파일러·런타임 구현(GT3), 영수증(`docs/audits/`), 이 문서의 GPT 항목과 협업 기록 | `docs/semantics/proofs/*.v`의 정의와 정리 |

- 예외 1: Claude의 모델 변경이 반례 파일이나 추출 계약을 깨면, Claude가 같은
  변경 안에서 최소한으로 이전하고 기록한다. 이후 정리는 GPT 몫이다.
- 예외 2: 사용자가 직접 지시하면 범위를 넘을 수 있다. 이날 Claude의 반례 파일
  추가가 그렇다.
- 공유 WSL에서 이름으로 프로세스를 죽이지 않는다(`pkill` 금지). 꼭 멈춰야 하면
  자기가 띄운 정확한 PID만 멈추고 기록한다.

## 명령과 시간 예산

- Rocq 9.3.0만 쓴다. 래퍼는
  `OPAMROOT=/home/c/.local/share/pergyra-rocq/opam bash scripts/run_rocq_toolchain.sh`다.
  Coq 8 결과는 증거가 아니다.
- 시간 예산은 AGENTS.md를 따른다. 정적 검사 60초, 집중 게이트 5분, 통합 30분.
- 커밋·push·설치본 교체·GUI 메시지는 사용자 지시가 있을 때만 한다.

## 통합 주인과 게이트

- 해체 쪽 통합 주인은 GPT, 게이트는 `tests/ownership_teardown_redteam_smoke.sh`다.
  증명 파일을 바꾸면 `tests/coq_kernel_check.sh`와 `tests/formal_semantics_smoke.sh`도
  돌린다.
- 값 정리 쪽 통합 주인과 게이트는 구현 지시서의 I8을 따른다.
- 산출물의 성격: Claude의 증명은 설계 확인이다. 구현 증거나 executable CLOSED가
  아니다. GPT의 추출·관측은 함수형 모델 관측이다. GT3만 구현 후보다.

## 협업 기록

- 2026-10-09 GPT, 소유권 생애 리뷰 보완: 사용자의 "전부 보완해서 문서 보강"
  요청으로 예외 2를 적용한다. `OwnershipGraphActionScope.v`의 단계 결과·saga
  실패 정보와 독립 소비자/게이트만 함께 이전한다. graph/tree/value core와
  다른 레인의 production P1은 건드리지 않는다. 범위·반증·경계는
  [리뷰 보완 계획](ownership_lifecycle_review_hardening_2026-10-09.md)에 고정했다.
  보상 코드 실행 완료를 실제 효과 복구로 부르지 않으며 물리적 원자성은 OPEN이다.
- 2026-10-09 GPT, 사용자 요청으로 문서 주장만 정정: 현재 비교 기준은
  `aa7f0d65f88ff319253e45d3d8a51d0129eaf873`와 읽은 증명 정의/정리문이다.
  예외 2에 따라 27번·semantics README·102·207·협업/계획/preflight의 관련 표현을
  맞췄다. CL6의 caller dispatch와 출력/지역 변수 발급, CL7의 원래 oracle 연결과
  파생·별칭·반환·호출 view 발급, 다중 inout source 정규화 동치를 OPEN으로 남긴다.
  GC 비교는 read coverage·추상 비용식으로 한정하고, 예제·구문 정리를 일반 실행
  정리로 쓰지 않는다. 현재 Views의 `ViewSchedule`은 발급과 종료를 모두 포함하며
  ended-ticket 정리는 종료 전 `views_wf`를 요구한다. 이 부분은 전달된 감사의
  issuance-only 지적과 현재 소스가 다르다. 증명·컴파일러·게이트·부모 작업 지시서는
  수정하지 않았고, 기존 커널/CI 영수증을 새 문서 입력의 결과로 재표기하지 않는다.
  상세 대조와 확인 범위는 [preflight 영수증의 문서 정정 절](../audits/ownership_cutover_preflight_2026-10-09.md#documentation-claim-alignment-2026-10-09)에 있다.
- 2026-10-09 Claude, CL6·CL7 증명: `OwnershipCleanCallLowering.v`(SHA-256
  `8db20314…572026`)와 `OwnershipCleanViewScope.v`(`f7c7a5c5…8fd04`)를 더했다. 기존
  core·teardown·preflight 파일은 바꾸지 않았다. 두 파일 포함 5개 모듈 커널 검사 PASS,
  가정 0(`.tmp/rocq93_ownership/cl6-cl7-kernel-claude.log`).
  - CL6: 에러로 끝나는 피호출자를 상태 변수로 낮춰 core의 보통 호출 표에 넣었다.
    반환·에러 두 실행 모두 inout 전부와 packet이 돌아오고, 호출자 전체가 블록을 전부
    푼다. `recovery_exec`에 기대지 않는다.
  - CL7: 쓰기 view를 backing 전체 focus로 두었다. 범위 안에서 backing의 성장·이전·해제는
    core가 거부하고, admission이 view의 모양 변경·복사·탈출을 거부한다. backing 길이
    보존을 증명했다. 위 정정처럼 이는 대안 모델의 결과이지 원래 CL7 수용 조건의
    완료나 production Slice 발급자 증명이 아니다.
  - GPT에 넘길 결정: (1) 상태 변수 경로는 활성 범위를 늘린다. production은 바로
    epilogue로 뛰는 방식을 쓰고 그 정제를 따로 둘지. (2) 쓰기 Slice가 살아 있는 동안
    원본 배열을 부를 수 없다(DX). 지금 소스에 그런 사용이 몇 곳인지 세어야 한다.
    (3) 27 §5.10.3의 lease 어휘 재사용 문장은 compiler-local view에는 필요 없다.
  - 문서: 27 §5.10.5, semantics README, docs/102.
- 2026-10-09 Claude, preflight 검토(읽기 전용): checkpoint `a75da804`는 456개
  파일, 바이너리·비밀값·충돌 표시·Claude co-author 없음. 새 증명 3개와 게이트
  해시가 영수증과 같고, `tests/ownership_cutover_preflight_smoke.sh`를 따로 돌려
  PASS(7개 모듈, 가정 0)를 확인했다(`.tmp/rocq93_ownership/preflight-claude-review.log`).
  주장과 다른 증명 결함은 찾지 못했다. 남은 것: (1) 다중 inout은 호출자 쪽
  pack/call/unpack과 피호출자 쪽 `recovery_exec`가 따로 증명됐고, `recovery_exec`는
  core의 `SCallIO`/`TCall`에 연결되지 않은 새 target 규칙이다. 끝에서 끝 회수가 P1의
  가장 큰 잔여다. (2) Views는 drop만 구체 소비자가 있고 성장(`TPush`)·이전(`TMove`)은
  호출자가 넘기는 영향 목록에만 기댄다. backing을 마지막 view 사용까지 살리는 정적
  issuer도 없다(동적 oracle). (3) `LeaseKind`는 두 모델 모두 판정에 쓰이지 않아,
  §5.10.3의 "admission 규칙 재사용"은 아직 이름 공유뿐이다. (4) `a75da804`와 추적
  diff 0이던 구간에는 증명·문서 게이트만 돌았다. 전체 P0와 메모리 판정을 그 기준선으로
  기록하려면 preflight 변경을 두 번째 로컬 checkpoint로 고정하고 동결한 채 돌려야 한다.
- 2026-10-09 GPT, baseline 이후: 사용자 확인으로 모든 다른 편집자가 멈췄고
  456개 입력의 로컬 checkpoint `a75da804`를 동결 상태에서 검증했다. 예외 2로
  `OwnershipCleanCallRecovery.v`와 `OwnershipCleanViews.v`의 importing preflight와
  독립 gate를 이 채팅에서 맡는다. 기존 core/teardown 주인은 바꾸지 않는다.
  Claude는 읽기 전용 검토. production consumer 이전·전체 P0/메모리·push/설치는
  이 승인에 포함되지 않는다. 최신 범위는 preflight directive가 기록한다.
- 2026-10-09 GPT, 최신 승인: 사용자가 ABI 주인·다중 inout 정규화·Slice 수명
  증거·검토된 로컬 기준선 커밋 네 항목에 착수하도록 확인했다. 예외 2로 필요한
  importing 모델과 생산/소비 연결을 이 채팅에서 맡는다. 별도 병렬 구현 레인은
  열지 않는다. 첫 소스 편집 전 기준선 고정; 전체 GT3 전환·push·공식 설치는
  아직 보류다. 범위와 반증 경계는
  `ownership_cutover_preflight_2026-10-09.md`에 기록한다.
- 2026-10-09 Claude: 전환 계획과 `audits/ownership_cutover_contradictions_2026-10-09.md`를
  읽기 전용으로 소스와 대조했다. 확인한 9건이 일치한다: HashMap String 값 경로가
  값도 해제함, `CompilerRuntimeValueCallAbiFact`는 Allocator/TextBuilder join,
  Slice descriptor는 `{data, length}`이고 세대 없음, `SCallIO`는 inout 하나,
  core 해시 4개 불변, 27 §5.10 OPEN, `ci.yml` push는 main 전용이고 PR·dispatch는
  있음, change-scope는 base가 없으면 full, Makefile 경로 변수 주입. 내 메모리
  감사의 HashMap 서술 오류는 정정했다. 착수 전에 남은 것은 셋이다.
  (1) P0 공식 기준선이 BLOCKED이고 같은 worktree를 쓰므로, 첫 소스 편집 전에
  기준선 방법을 사용자가 정해야 한다. 편집이 시작되면 같은 트리에서 옛 기준선을
  다시 만들 수 없다. (2) 착지 게이트 5에 고정 명령·입력·지표가 없다. 기록된 세
  최고치는 서로 다른 작업이고 기준선이 이미 상한을 넘으므로, P0 메모리 측정을
  P2 전 go/no-go로 둔다. (3) P1 담당 문구가 어긋난다(계획 표는 "GPT 통합, Claude
  검토", F1은 "Claude와 core", 27 §5 편집자는 미지정). 분담을 정할 때 함께 고친다.
- 2026-10-09 GPT, 최신 범위: 사용자가 모순 정리만 먼저 하고 마지막 체크 후
  구현하도록 지시했다. 현재 바뀐 것은 문서뿐이며 GT3 구현은 보류한다.
- 2026-10-09 GPT: 사용자 "전부 고쳐라 그 다음 작업하고 클로드랑 체크" 지시로
  예외 2를 적용해 GT3의 계약 문서를 보완했다. 이후 최신 지시로 실행 착수는
  보류했다. 읽기 전용 검토에서
  드러난 여섯 연결 의무를 전환 계획·구현 지시서에 반영했다. Claude에는 실제
  소스/모델과 계약 연결의 교차 검토를 요청한다. 과거 CL1 완료나 구조 gate
  PASS를 현재 compiler cutover 완료로 사용하지 않는다.

- 2026-10-08 GPT: 위 직접 지시의 CL1 범위를 실행 모델로 구현하고 통합했다.
  기존 숲/해체 알고리즘은 재사용했다. 설치된 Claude와 읽기 전용 검토 2회를
  진행해 무권한 pin 발급 High와 증명·검사 공백을 고쳤다. 최종 집중 게이트와
  전체 커널/형식 의미론, 문서/수명 게이트 PASS를 확인했다. 권한 모델 해시
  `ca8aa6f1`, 영수증은 `docs/audits/ownership_teardown_authority_2026-10-08.md`.
  CL2 실제 접근/변경·저장소 합성, CL3 유한 식별자, CL4 색인 순회와
  호출자/수신자 바인딩·보호된 상태 및 네이티브 구현은 여전히 OPEN이다.
  GT3/I1–I8 보류, 커밋·push·설치본 교체·GUI 메시지 없음.
- 2026-10-08 GPT: 사용자가 CL1의 참조/정리권 분리, 전체 단위 대여·pin 검사,
  발급·이전·소비 구현을 직접 지시하고 구현 후 Claude 공동 검증을 요청했다.
  예외 2로 이 채팅에서 CL1의 가져오기 기반 모델과 통합 소비자를 수정한다.
  기존 해체 기계를 복제하지 않는다. GT3/I1–I8 보류는 해제하지 않는다.
  작업 경계는 `teardown_authority_implementation_2026-10-08.md`에 기록한다.
- 2026-10-08 Claude: 이 문서를 만들었다. 정리 2개와 반례 2개를 넣고 집중 게이트
  PASS를 확인했다. 날짜가 지난 지시서 132개를 `OLD`로 표시했다(삭제 없음).
- 2026-10-08 Claude: 사용자 판단(자동 소유권은 섞인 상태가 결함)에 따라 GT3을
  [한 번에 전환하는 계획](ownership_cutover_plan_2026-10-08.md)으로 바꿨다. 읽기 전용
  조사 4갈래의 당시 실측과 착지 게이트, 당시 사용자 결정 5개(현재 계획의
  "현재 보류와 사전 확인")를 그 문서에 적었다. 코드는
  바꾸지 않았다. P1 계약 수정은 Claude가 보류 해제 뒤 맡는다.
- 2026-10-08 Claude: 3 GiB 상한을 읽기 전용으로 감사했다
  ([`compiler_memory_pressure_2026-10-08.md`](../audits/compiler_memory_pressure_2026-10-08.md)).
  09-28 `gen2.exe`는 C 방출 전, front end만으로 상한에 걸렸다. C oracle도 같은 일에
  2.365 GiB를 쓴다. GPT의 P0에 live bytes·누적 할당 측정과 측정 출처 기록을 더했다.
  코드는 바꾸지 않았다.
