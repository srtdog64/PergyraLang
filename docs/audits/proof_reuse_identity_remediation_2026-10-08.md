# 재사용 식별자 레인 수정 영수증 — 2026-10-08

상태: **집중 검증된 구현 후보; main 독립 통합 전**. 기준 HEAD는
`3658548d24bca3d721e4f1974ac7a10da99f7aa8`, 공유 dirty `main`이다.
이 문서는 관찰 영수증이며 의미 소유자, SoT CLOSED 또는 CI 증거가 아니다.
전체 작업 경계는 [공유 지시문](../agent_work_directives/proof_redteam_reuse_remediation_2026-10-08.md)에 있다.

## 전체 경로와 판단

실제 intent 경로는 C-inline/링크형 진입 → TLS 현재 스택 → 잠긴 active
handle index → parent ancestry 충돌 면제 → exit/index 퇴역 → 공개 trace
투영이다. 공개 ABI가 `int32` 값만 받기 때문에 내부 generation만 추가하면
오래된 exit/trace 호출은 여전히 재사용된 공개 handle을 구별하지 못한다.
따라서 공개 handle은 다시 발급하지 않고 `INT32_MAX` 이후 명시적으로
거부한다. 물리 registry 칸은 계속 재사용하며 trace ID의 관찰용 순환은
별도다. 발급 정책의 유일한 소유자는
`src/runtime/pgy_runtime_intent_identity.h`이고 두 runtime 경로가 이를 읽는다.

우선순위는 식별자 보존 → 퇴역 권한의 재생 금지 → 명시적 실패 → 반례다.
마지막 소비자는 실제 conflict/exit/trace와 영구 typed probe다.
금지 경로는 handle counter의 재순환, lexical nesting을 runtime ancestry로
대체하기, generation 없는 옛 holder의 사용, frozen pre-state 해제 집계다.

## 항목별 실제 변경

| ID | 처분 후보 | 소유자와 관찰 |
| --- | --- | --- |
| C1 | REPAIRED | 공통 nonrecycling 발급, absorbing exhaustion. 양쪽 ancestry walk는 live 조회를 먼저 하고 nondecreasing parent를 거부한다. 모델의 `issue_handle_implements_identity_enter`, `old_public_identity_never_reissued`, `ancestor_waiver_requires_live_identity`가 실제 발급/퇴역 인터페이스를 연결한다. |
| C2 | CLAIM CORRECTED | `IntentSpine.no_dep_cycle`은 한 intent 안의 step dependency만 보장한다. runtime parent cycle/재사용 배제라는 헤더·주석 주장을 제거했다. |
| M2 | REPAIRED, bounded model | `SlotLifecycleCore` 연산은 `(slot,generation)`으로 식별한다. reclaim은 새 세대를 발급하고 `retired_identity`를 임의의 `rsteps`에서 보존한다. `released_identity_forever_stale`는 재claim 이후에도 옛 use/release를 거부한다. acquire 전제 정리의 이름은 `acquire_requires_vacancy`다. |
| M3 | REPAIRED, bounded model | 자기 move는 원본을 보존한다. missing source/occupied destination/반복 retire의 결과를 구분한다. consuming ledger의 `unique_arbitrary_trace_retirement`가 임의 길이의 transfer/drop trace를 다룬다. 이전 2-binding 정리는 제한된 대비 증거로만 남겼다. |
| M4 | REPAIRED, bounded model | `StringValue`가 복사한 값과 raw 주소 설계를 구분한다. 실제 write/free/reuse 모델에서 raw 주소는 새 데이터를 읽지만 boundary copy는 원래 데이터를 읽는다. missing storage와 반복 free는 명시적 결과를 반환한다. |

owner/gate table의 존재 정리는 인터페이스 inventory 증거일 뿐이다.
`transfer_run`과 `foreign_run`은 각각 소유 상태/메모리 상태 투영이며,
실제 단일 단계 결과는 `transfer_exec`/`foreign_exec`에 있다. 이 투영을
전체 프로그램 오류 처리나 allocator refinement로 주장하지 않는다.

## 관찰한 검증

- `tests/proof_reuse_identity_smoke.sh`: Rocq 9.3.0 / Stdlib 9.2.0의 fresh
  snapshot으로 5개 owner + `tests/coq/ReuseIdentityAudit.v`를 컴파일한 뒤
  trusted kernel로 검사했다. 6개 module PASS, axiom/admit/unsafe feature 없음.
  SlotLifecycleCore의 기존 masking/deprecated 경고는 남는다.
- `tests/runtime_intent_identity_reuse_smoke.sh`: 실제 runtime source의
  inline/linked × trace 0/1 조합을 빌드하고 실행했다. 정상 nesting,
  non-LIFO leave, 동일 registry 저장소 재사용, 다른 thread와의 충돌 거부,
  stale exit/trace, MAX 직후와 빈 registry에서도 지속되는 exhaustion을 검증한다.
- `tests/runtime_intent_observability_contract_smoke.sh`: PASS.
- 세 shell의 `bash -n`, 변경 범위 `git diff --check`: PASS.
- `.tmp/intent-identity-wrap-mutation-c3591cab99594ae6903294c4baed37af`에서
  공통 발급의 MAX→0을 MAX→1로 바꾼 mutation은 runtime probe의 absorbing
  state assertion에서 실패했다. 정상 workspace runtime은 바꾸지 않았다.

## 소스 checkpoint

| 소스 | SHA256 |
| --- | --- |
| IntentConflict.v | `51a75c3b06a700520aee17ad7c3b8041931b2b78897e4404bf92fd5cd8477356` |
| IntentSpine.v | `c0a44f93aaa12f3dadcee5dfe4969dcc22d10c6837919a1cd6b768ad69d9eda6` |
| SlotLifecycleCore.v | `671c158fbd1e818cea4489ab1ad932b43d1b1771254ea0b98a303da94aac95bf` |
| CollectionOwnershipTransfer.v | `46a6a0a830107ead029e34162081a093ff081bd8bd5c26561006f5e326e13dd9` |
| ForeignStringOwnership.v | `98305ab3fb2ee67052e414fc3c9838445eba680947bb534dd868ae10770d7192` |
| ReuseIdentityAudit.v | `8bf5201cc0bb20bd177cd8ed7ca0cffe99cc8754aad98e90e33de7de694d869c` |
| pgy_runtime_intent_identity.h | `97320a40cd257a2374757971e0c6013a084f400f3dd87349e4c5d59f546c7303` |
| pgy_runtime_lib_set_intent_trace_exports.c | `8788dd3365bc3a49968e47fd7dfcff4b96dd29b506eaed2e2878bee20d2ba9a1` |
| pgy_runtime_intent_trace_inline.h | `1beaa876b17fd7e1ab81c5334d4adc497f93533f75c3ba98120a40c66dc33cb5` |

## 열린 refinement 경계

이 레인은 whole-corpus/CI나 installed-driver를 검증하지 않았다. main이
독립 통합한다. 모델은 unbounded slot generation, abstract byte 값,
fresh clone storage를 전제로 한 조각이다. 일반 allocator, 실제 foreign
callee, 토큰·pin 모델과의 전면 simulation은 별도다. INT32 handle exhaustion의
비용/발생률은 측정하지 않았다. 새 GC, 사용자 annotation, compiler pass,
worktree, 설치, commit/push, shared process 종료는 추가하지 않았다.

공유 인용 수정 대상은 `docs/semantics/README.md`의 SlotLifecycle 문단,
`docs/semantics/19_theoretical_foundations.md`의 slot proof 문단,
`docs/206_minimal_unit_decisions.md`의 해제/foreign copy 인용 및 docs/173의
runtime parent cycle 인용이다. 이 파일들은 main에 전달했고 레인에서 수정하지 않았다.
