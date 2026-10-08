# 소유권 자동 정리: 한 번에 전환하는 계획

상태: `REVIEW — 모순 정리, 구현 보류`. 작성: 2026-10-08 KST, Claude, 사용자 요청.
2026-10-09 후속 승인: 네 선행 의무와 로컬 기준선 checkpoint만
[별도 preflight](ownership_cutover_preflight_2026-10-09.md)에서 착수한다.
전체 P2–P10 전환·공식 설치·push 보류는 유지한다. 아래 모순 정리 당시의
보류 기록을 전체 착수 승인으로 해석하지 않는다.
기준: `main @ 3658548d24bca3d721e4f1974ac7a10da99f7aa8`(origin/main `b9a75ba2`보다
커밋 3개 앞, 미push), 공유 dirty main(조사 시 439개 항목).
이 문서는 계획이다. 의미는 [27번 문서](../semantics/27_ownership_clean.md)와 증명이,
판정은 실행 게이트가 가진다. 2026-10-09 KST 사용자의 최신 지시 "우선 모순쪽만
정리해놔 바로 구현은 마지막 체크한번 더 하고"가 앞선 착수 지시의 실행 범위를
줄였다. **지금은 문서 모순 정리와 읽기 전용 재검토만 한다.** P0 실행 기준선,
P1 모델 변경, P2–P10 구현은 마지막 체크 후 별도 착수 확인까지 보류한다.
옛 수동 own/carrier 수정 사슬은 재개하지 않는다. 완료·설치·원격 CI는 별도
실행 증거가 있을 때만 기록한다. 정리 진입 시 HEAD는 위와 같고 dirty 442개,
staged 0개였다. compiler/runtime/proof 변경은 없다.

이 계획은 [구현 지시서](ownership_clean_implementation_2026-10-08.md)의 I1–I8을
**착지 방식만 바꾼다.** 항목별 수용 조건은 그대로 쓰되, 항목마다 main에 착지하지
않고 전환 트리에서 모두 끝낸 뒤 한 번에 착지한다. 중간 트리가 전체 매트릭스에서
빨간 것과, 다음 단계가 의존하는 계약의 집중 게이트가 빨간 것은 다르다.
후자는 다음 소비자 이전을 막는다. 타입의 깊은 복사·변경·해제 검증 전에는 그
타입의 자동 drop을 실행하지 않고, 대체 소비자와 음성 게이트 전에는 옛 주인을
삭제하지 않는다. 문서 수정이나 항목별 PASS는 부분 착지·구현 CLOSED가 아니다.

## 왜 한 번에 하는가

사용자 판단(2026-10-08): 자동 소유권은 섞인 상태가 그 자체로 결함이다.

- **복사 의미가 섞인다.** 어떤 타입은 주인 하나·깊은 복사로, 어떤 타입은 얕은
  공유로 남으면, 둘을 함께 담은 구조체는 복사 한 번에 두 규칙이 섞인다. 이중
  해제를 막는 전제가 타입 경계에서 깨진다.
- **공존 장치가 누수를 숨긴다.** 수동 호출을 `reset`으로 강등해 공존시키면
  backing만 비워져, 힙을 가진 원소가 샌다(27 §5.5). 컴파일러가 3 GiB 상한을
  넘길 수 있다.
- **추론이 프로그램 전체 단위다.** 모드 추론은 호출 그래프 고정점이다. 일부만
  자동이면 경계마다 요약이 없어 거부되거나, 금지된 "전부 빌림" 폴백이 필요하다.
- **부트스트랩은 한 규칙이어야 한다.** gen2 == gen3은 컴파일러가 한 소유권
  규칙으로 컴파일될 때만 의미가 있다.

## 목적 카드

- **목적:** 모든 힙 값이 마지막 사용 지점에서 컴파일러가 만든 코드로 정확히 한
  번 해제되고, 프로그램 출력은 바뀌지 않는다. 수동 해제 호출 없이 3 GiB 상한을
  지킨다.
- **우선순위:** 모델대로 정확히 → fail-closed admission → C/LLVM 동일 출력과 음성
  증거 → 옛 경로 삭제 → 패치 크기.
- **사실 주인:** 새 ownership-clean 패스(27 §5.1). 입력은 기존 안전 검사와 사실
  생산자(아래 "유지")다.
- **마지막 소비자:** C emitter, LLVM emitter, self-host emitter, 관측 플래그.
- **금지:** 27 §5.1의 금지 목록 전부, 그리고 착지된 기록에서 옛 규칙과 새 규칙의
  공존. RC·GC·"누수로 대신" 폴백, 사실이 없을 때 drop을 건너뛰는 폴백, 새
  `own`/`ref`·carrier/restore 표기.

## 전체 사슬 실측 (2026-10-08)

출처: 이날의 읽기 전용 조사 4갈래. 아래 수치와 소스 줄 번호는 그 snapshot의
잠정 inventory다. 문서 27/옛 지시서의 더 이른 수치와 현재 값으로 섞지 않는다.
P0에서 exact-input/source manifest로 다시 고정하기 전에는 최신 실측이라 하지 않는다.

### A. 저장 모델과 런타임 (I1)

| 항목 | 현재 | 전환에서 바꿀 것 |
|---|---|---|
| MIR 명령 | copy·move·drop 명령이 없다(`src/compiler/mir_types.h:83-92`). 정리 명령은 `MIR_INST_CLEANUP_EDGE`뿐 | 패스가 내는 copy·move·drop·focus 사실을 표현할 명령 또는 사실 행을 추가(27 §5.3) |
| 복사 방식 | 두 백엔드 모두 descriptor를 값으로 복사. C 약 31곳, LLVM 약 21곳(대입, phi, 반환, 필드 저장, 생성자, for-in, inout 복사 들어가기·되쓰기). 중첩 배열 리터럴은 내부 `data`를 공유(`llvm_expr_aggregate.c:242-253`) | 사실이 copy라고 하는 곳만 깊은 복사, 나머지는 move. 얕은 경로 제거(27 §5.4) |
| drop 함수 | Array는 C inline과 LLVM raw export가 있다. String·List·Set·Queue의 일반 타입별 glue는 없다. HashMap은 String 값 전용 inline/export 경로에서 값도 해제한다(`pgy_runtime_map_string_inline.h`, `pgy_runtime_lib_raw_map_exports.h`); 일반 nominal/nested 값과 emitter 도달은 별도 확인 대상 | 타입별 drop glue와 마지막 소비자 이전. 기존 String 값 해제 경로를 보존·합성하고 일반 값으로 확장 |
| Clone | C는 Array·Slot만, LLVM은 Array만. 중첩·nominal 원소는 얕다. inline clone이 할당자를 버림 | 타입별 깊은 복사 glue, 할당자 보존 |
| 할당자 버그 | export `pgy_array_drop_owned_String`이 `pgy_free` 대신 `free` 호출(`pgy_runtime_lib_array_map_exports.h:235`). export Array 계열 `new`/`push`가 할당자를 무시 | 수정 |
| 백엔드 불일치 | String set-values가 C는 복사, LLVM은 저장 공간 별칭 | 한 규칙으로 |
| 타입별 표 | C(inline 매크로, `transpiler_specialization_registry.c:433-477`)와 LLVM(export 본문, `pgy_runtime_lib_*_exports.h`)이 따로. 공유하는 건 레이아웃 표뿐(`mir_abi_layout.c`) | 한 표가 두 백엔드 함수를 이름 붙이게(선례: `MIRTextBuilderRuntimeRow`, `mir_abi.h:109-118`) |
| 누수 검사 | 실행된 프로그램이나 LLVM 백엔드를 leak 검사로 돌리는 테스트가 없다. emitted-C sanitizer는 leak 검사를 일부러 끔. MinGW는 ASan 불가 | 착지 게이트에 Linux에서 C·LLVM 양쪽 실행 + LSan 켜기 |

### B. 런타임 호출 ABI (I2)

- native 행 `MIRResourceRuntimeRow`(`mir_abi.h:50-63`), self-host 행
  `CompilerRuntimeCallAbiFact`(`runtime_call_abi_structured_fact_owner.pgy`)는 기존
  자원 호출 범위의 출발점이다. `CompilerRuntimeValueCallAbiFact`
  (`runtime_value_call_abi_owner.pgy`)는 **Allocator/TextBuilder join**이며 지금의
  인자 schema는 최대 2개다. 둘을 ArrayPush/getter/Map 변경까지 이미 덮는
  일반 값 수명 주인이라고 하지 않는다.
- 컬렉션·String 호출의 lend/consume/reset, 반환 출처, 변경 후조건 **주인은
  미지정(OPEN)**이다. P1에서 기존 ABI identity를 확장할지 필요한 owner family를
  선언할지 docs/180·docs/192·SoT registry에 따라 확정한다. 합리적인 새 주인을
  금지하는 것이 아니라 근거 없는 두 번째 판정표·기본 borrow 폴백을 금지한다.
  지정·검증 전에는 새 자동 정리 경로에서 해당 호출을 거부한다.
- 결과의 **저장 분류**(static/region/heap)와 **소유 출처**(fresh owned,
  argument/part view, transferred, no result)를 구별한다. String getter의 heap
  주소나 Slice descriptor는 새로운 소유권이 아니다. view에는 backing owner,
  part/place와 유효 세대가 필요하다. 오류 시 결과 미발급과 입력 보존/소비도
  ABI 후조건이다. 빠진 열은 borrow/fresh로 추측하지 않고 거부한다.
- native String 결과 분류와 P1에서 지정할 collection/String ABI 주인의 연결은
  P1에서 확정한다. 기존 일반 값 전체 ABI 행이 있다는 전제는 두지 않는다.
  새 독립 판정표를 기본 선택으로 삼지 않는다.
- 레지스트리의 기존 행/closure 절은 CLOSED이고 옛 bridge 설명은 역사 기록임을
  명시해 상태 중복을 정리했다. 이 편집으로 행 상태를 바꾸거나 새 게이트 통과를
  주장하지 않는다. 새 ownership 열·출처·변경 후조건은 기존 ABI CLOSED와 별개로
  OPEN이며, 소비자 이전·음성 게이트 전에는 그 상태를 물려받지 않는다.

### C. 패스 위치와 관측

- **native:** ownership-clean 주인은 기존 `mir_lower` DCE 뒤에 진입한다. 자체
  source normalization/copy propagation 이후 최종 세대의 분석을 소비한다.
  정규화 후 DCE 재실행 여부는 P1에서 명시하며, 실행한다면 그 변환 이후 다시
  분석을 발급한다. copy propagation과 place/expression
  정규화로 def/use가 바뀌면 같은 최종 MIR 세대의 분석을 다시 생산한 뒤 그
  certificate를 검사한다. 정규화 전 liveness를 재사용하지 않는다. 테스트 하네스도 `mir_lower` →
  `mir_validate` 순서라 같은 경로를 탄다. **27 §5.1 수정이 필요하다.**
- **self-host:** liveness가 아예 없다(live_in/live_out 없음, JSON 블록 행에도 없음).
  패스가 직접 계산한다. 위치 후보는 `driver_rung2_owner.pgy:139-141`,
  `artifact_lower_owner.pgy:500-502`, `canonical_mir_execution_owner.pgy:31-32`이고,
  검증기는 `program_verify_owner.pgy:390-482`다.
- **관측 플래그:** native에는 `--observe-*`가 없다(`pgy_driver.c:45-79`). self-host는
  `--observe-mir-consumer-stages` 배선이 있다. 새 플래그는 launcher
  `self_host_driver.c:62-99`의 고정 argv에도 더해야 한다.

### D. MIR JSON (I3)

- **생산자:** native `mir_json_dump.c`(필드 `collection_ownership_receipt` :351)와
  self-host 4개(`json_projection_owner`, `program_json_artifact_writer_owner`,
  `instruction_json_artifact_writer_owner`, `domain_runtime_assignment_json_owner`).
  최상위 키 순서가 두 생산자 사이에 다르다. 지금 바이트가 같은지는 UNKNOWN.
- **소비자:** self-host `mir_lower/*`와 `direct_mir_*`(1,008개 파일). 미지 필드
  거부는 하나의 검사가 아니라 **48개 파일의 정확한 키 개수 검사**다(루트 5, 명령
  26, 루틴 23/22, 블록 3 등). 새 필드 하나가 이걸 모두 깬다. 6곳은 receipt가
  `null`이기를 요구한다.
- docs/192 행 `pergyra.mir-json.v1`(:40), 주인 `mir.execution_graph`(BRIDGE).
  게이트: `self-host-mir-json-parity-test-smoke` 등.
- 바이트 동일성은 같은 admission snapshot과 canonical identity/직렬화 순서에
  묶는다. 독립 생산자의 raw syntax ID 숫자 동일성은 docs/192 계약이 아니다.
  비교를 위해 ID를 지워 stale/cross-snapshot ownership 사실을 허용하지 않는다.

### E. 지울 것 (I7)

| 묶음 | 위치와 크기 |
|---|---|
| 해제 증명 분석기 | `src/self_hosted/semantic/ast_collection_aggregate_*` 8개, 2,329줄. native 짝 `collection_owned_element_requirement_owner.c` 391줄 |
| receipt 상태 기계 | native `mir_branch_source_facts.c`(965줄, 상태 :507-514, 검증 :716-882), self-host semantic `ast_collection_ownership_*` 13개 2,068줄, self-host MIR receipt 3개 216줄 |
| 수동 해제 허가 | `semantic_collection_admit_owned_string_drop`(`collection_ownership_fact.c`, 당시 753–891), `array_storage_release_owner.c` `semantic_array_storage_admit_drop`, `compiler_retire_array_storage_context_ready`, `array_storage_deferred_preservation_owner.c` |
| direct-MIR 정리 결정 | 정리 정책·HashMap 정리 348줄, receipt 기계 8개 1,030줄, 이동-은퇴 8개 827줄, 수동 해제 재허가 2개 440줄 |
| 수동 해제 호출 | 컴파일러 소스 436개(`CompilerRetireArrayStorage` 212, `ArrayDrop` 141, `ArrayDropOwnedStrings` 83), 테스트 460개(399개 파일) |
| 수동 프로토콜 | 꺼냈다 복원하기: `SelfMirCfgAttachLastDestructure`, `SelfMirAppendCfg`, `Rows(` 재구성 다수, 필드 복원 265곳, `own` 매개변수 100개. 27 §4 단계 4의 "own-threading 경로" 범위는 착수 전에 확정한다 |
| builtin 표면 | `ArrayDrop`·`ArrayDropOwnedStrings`·`CompilerRetireArrayStorage`의 operational/effect/caller 행과 builtin 목록 삭제. `builtin_name_reservation.def` tombstone 및 정확한 retired-call 진단은 별도 P1 결정; 이름 예약을 자동으로 함께 삭제하지 않음 |

### F. 보존할 불변식과 변경할 선행 admission

타입·callee·place identity, 배타성, 세대·대여·자원 경계는 보존한다. 그러나
현 주인의 이름을 유지하는 것과 옛 허가 정책을 그대로 유지하는 것은 다르다.
선행 semantic admission에서 거부된 코드는 MIR 정리 패스에 도달하지 않는다.

- inout과 별칭: `ast_inout_argument_alias_verdict_owner`,
  `type_checker_helpers_late.c:170-214`, `type_checker_ownership_call.c`
- 할당과 세대: `ast_collection_definition_storage_authority_owner` 등
- 대여와 보유: `formal_effect_*`, `ast_string_formal_borrow_owner`,
  `indexed_string_borrow_owner.c`
- 해제 의무 사실: `array_storage_element_lifetime_owner`
- 출구와 자원: `routine_defer_owner`, `mir_cleanup.c`, intent cleanup emitter, Slot 자동 해제

P1에서 아래를 **유지 / 사실 생산으로 변경 / 대체 후 삭제**로 분류한다.

- native `type_checker_helpers_late.c`와 self-host
  `ast_inout_argument_alias_verdict_owner`의 변수만 허용하는 정책은 변경 대상이다.
  멤버/원소 actual을 한 번 평가한 place, 루트 배타성, 동적 index 충돌 사실로
  바꾼다. 같은 place 두 번, root와 field 동시 전달은 허용으로 뒤집지 않는다.
- `type_checker_ownership_call.c`의 **일반 값** readonly 임시값 거부만 compiler
  temporary 발급·수명 사실으로 대체한다. 같은 함수의 movable/anchored Slot,
  subject 등 affine·authority named-boundary 검사는 보존한다. 일반 값의
  불필요한 named-local 의례를 제거한다고 실제 resource 경계를 제거하지 않는다.
- `ast_collection_definition_storage_authority_owner`의 `formal.mode != 2`를
  borrowed 권위로 쓰는 판정은 추론 summary의 소비자로 이전한다. source `own`
  표기를 새 소유권 증거로 재사용하지 않는다.
- `Slice`는 현 runtime의 write-through borrowed view다. backing owner와 place
  의존성을 최종 liveness에 포함하고 독립 owning drop/clone glue를 붙이지 않는다.
  descriptor에 generation은 없다. 유효 세대/현재 수명 certificate의 생산자와
  loan/refinement는 OPEN이며 source ID를 runtime 세대라고 쓰지 않는다.
  indexed String의 출처와 변경 무효화 사실도 새 패스의 입력으로 이전한다.
- 사용자 resource·worker 경계와 defer/finalizer 순서는 그대로 보존한다.

`semantic_indexed_string_borrow_invalidate_deep_drop`
(`stdlib_array.c:327`)은 지금 수동 drop에서 발동한다. 패스의 drop 사실에서
발동하게 바꾼다.

### F1. P1에서 닫을 실행 연결 의무

| 의무 | 사실 주인 → 마지막 소비자 | 반증 입력 / 수용 조건 |
|---|---|---|
| 선행 admission | native/self-host semantic place·temporary facts → MIR normalizer | member inout와 readonly temporary 통과; 중복 place/root overlap은 거부 |
| 호출 정규화 | admitted call/argument order → core statement sequence → 양 backend | 두 개 이상 inout + 별도 반환값, 중첩 호출, short-circuit, 조기/error 출구. 모든 회수값을 정확히 한 번 복원; 현 단일 SCallIO 증명으로 다중 출력까지 증명됐다고 하지 않음 |
| 표현식 순서 | 기존 typed expression graph와 source identity → MIR temporary def/use | 인자·index 한 번 평가, 실행되지 않은 arm은 할당/정리하지 않음, 부분 실패는 발급된 값만 정리 |
| 반환 출처·view | P1에서 지정할 collection/String ABI owner + 기존 범위의 resource/value facts → liveness·ownership → emitters | borrowed String을 독립 heap owner로 오인하면 거부; Slice backing 마지막 사용까지 보존. live view 중 grow/inout/sink와 실제 lifetime certificate 생산자는 OPEN |
| 컬렉션 변경 | 타입 glue + runtime operation ABI → C inline/LLVM exports | Set/replace의 old payload, Pop/discard/remove/clear, 중복 삽입, grow relocation, 복구 가능한 실패 시 부분 생성. panic/abort는 계속되는 실패와 구분; 최종 backing drop만으로 잃어버린 원소를 대신하지 않음 |
| 분석 세대 | 정규화/DCE 후 MIR analysis owner → ownership checker | 변환 전 certificate, stale/다른 snapshot ID, 누락 summary·glue·origin 거부 |

새 모델 규칙이 필요한 행은 Claude와 가져오기 기반 core/정규화 증거를 만든
뒤 게이트를 실행한다. 이 표는 의무 목록이며 아직 증명 또는 구현 완료가 아니다.
일반 값 전환의 선행 조건에 별도 Qt graph/CL2–CL4 폐쇄를 끼워 넣지 않는다.

### G. 게이트, 목록, 레지스트리

- **뒤집혀야 하는 게이트:**
  - `collection_ownership_semantic_owner.sh`: 각 음성 성질을 P1에서 분류한다.
    수동 호출 없는 positive successor / 금지 호출 manifest negative / 증명된
    중복 제거를 구분하며 17개 전체를 통과로 뒤집거나 삭제하지 않는다.
    receipt 개수와 "명시적 drop 뒤 자동 정리 추가 금지"의 낡은 정책만 이전한다.
  - `public_array_drop*.sh`: ArrayDrop 표면 전체
  - 분석기 게이트들
  - direct-MIR 정리 게이트들
  - `CompilerRetireArrayStorage` 관련 약 10개 게이트
  - builtin 표면·이름 게이트: operational/effect/caller 행 삭제와 이름 tombstone
    정책은 다른 결정이다. P1에서 예약 유지 여부와 기존 진단 주인에 등록할
    정확한 retired-call 진단을 정한다. 결정 전 예약까지 일괄 삭제하지 않는다.
  - native 단위 테스트 `test_mir_collection_ownership_validation.cases.h`
- **고칠 개수와 목록:**
  - `self_hosted_component_contract_smoke.sh` 85줄
  - `self_hosted_responsibility_caps.tsv`
  - `ownership_clean_retired_owners.txt`: 정규식이 `src/self_hosted/semantic/`만 받아서 mir/compiler/codegen 경로와 C 주인을 못 담는다. 확장 필요
  - `manual_gate_inventory.tsv` 12행
  - CI 프로필 개수 핀
  - likeness 하한(`RESULT_USE_MIN=5473`, 삭제분 약 63개)
  - Makefile 소스 목록
  - `src/self_hosted/OWNERS.md` 23줄
- **레지스트리 행:**
  - `semantic.hashmap_collection_ownership`(ACTIVE): 새 사실 계열의 전신
  - `projection.direct_mir_scalar_cfg_program_extension`(BRIDGE)과 `abi.mir_array_string_layout_projection`(CLOSED): 마지막 소비자로 지울 정리 주인을 이름으로 담고 있어서, 지우면 `require_path`가 실패한다
  - 증거 스크립트를 공유하는 `selfhost.match_case_pattern`과 `selfhost.semantic_artifact_admission`
  - 새 레지스트리 행을 만들면 상태 개수 문자열도 바뀐다

### H. 메모리 상한

- 상한 `PGY_BUILD_PRESSURE_LIMIT_MB ?= 3072`(Makefile:5438). 측정은
  `scripts/measure_build_pressure.ps1`.
- 기록된 최고치 2.974 GiB(08-09). **가장 최근 측정(09-28)은 3.006 GiB로 이미
  상한을 넘었고** 실행이 실패했다. 오늘 트리의 전체 고정점 최고치는 UNKNOWN.
- 09-28 실행(`gen2.exe`)은 semantic 판정이 끝난 직후, C 방출을 시작하기 전에
  상한에 걸렸다. front end만으로 상한을 채운다. `source` 단계가 1,221 MB,
  `statement` 단계가 695 MB를 더한다. 900초 동안 메모리가 20 MB 넘게 내려간 적이
  없다. 같은 일을 하는 C oracle도 2.365 GiB를 쓰므로 대부분은 파이프라인 구조
  (프로그램 전체를 한 프로세스에, 단계 사이는 JSON 문자열) 몫이다. 근거와 다른 언어
  비교: [`compiler_memory_pressure_2026-10-08.md`](../audits/compiler_memory_pressure_2026-10-08.md).
- 수동 은퇴 호출은 이 상한을 맞추려고 있다(27 §1). 자동 drop이 같은 시점이나
  더 이르게 해제하지 못하면 전환이 실패한다. **가장 큰 위험이다.** 최고치를 올리는
  길은 둘이다. 정리가 함수 끝으로 밀리는 것(self-host liveness 없음)과, 얕은 복사
  지점에서 이동 대신 깊은 복사를 고르는 것이다.

## 전환 트리 안의 순서

착지는 마지막 한 번뿐이다. 단계별 입력 계약·성공/거부 집중 게이트는 다음
단계의 장벽이다. 전체 매트릭스의 일시적인 빨강은 원인·대체 행·재실행 명령을
기록하고 설치/배포하지 않는다.

| 단계 | 내용 | 담당 |
|---|---|---|
| P0 | 승인된 고정 snapshot의 기준선. dirty 트리 실행은 source/hash manifest가 붙은 진단 관측일 뿐 SHA/CI 증거가 아니다. 공식 비교 기준선과 candidate는 모든 gate 입력이 승인 SHA와 diff 0인 같은 worktree에서 재검증한다. 기준선·candidate 간 차이는 계획된 변경 집합과 고정 fixture 관계로 기록한다. 10-06 원격 DRV-2 실패는 역사 기록이며 현재 CI 관측이 아니다. named-value 실패는 F의 일반 값 admission 변경과 관련된다. gate reachability의 등록 실패는 알고리즘과 별개지만 필수 gate이므로 고친다. 3 GiB 측정은 peak live/누적 bytes, 명령·입력·source/executable hash를 구분한다 | GPT |
| P1 | F/F1의 연결 계약·필요 core. ownership-clean 주인이 ordered source expression/place/call 정규화 → copy propagation → 모드 고정점 → focus/unpack·최종 certificate → elaboration → 남은 copy의 D1 판정을 소유한다. 결과 출처의 실제 ABI 주인, 다중 inout source/회수 보존 증거, Slice loan/현재 수명 증거·DX 영향, 변경/복구 실패/abort 구분, tombstone 진단, JSON identity와 삭제 범위 확정 | GPT 통합, Claude 계약·증명 검토 |
| P2 | 저장 모델과 런타임 glue(A): 한 타입 표, 모든 타입의 drop·깊은 복사·변경 계약, 할당자 보존, 백엔드 불일치 | GPT |
| P3 | ABI 열(B) | GPT |
| P4 | native 패스와 관측 플래그(C), MIR 사실 | GPT |
| P5 | 두 백엔드가 사실만 소비(A의 복사 지점 전부) | GPT |
| P6 | MIR JSON 계열과 모든 생산자·소비자(D) | GPT |
| P7 | self-host 패스(liveness 포함)와 emitter | GPT |
| P8 | 삭제(E)와 게이트·목록·레지스트리 전환(G), 재등장 금지 래칫 | GPT |
| P9 | 부트스트랩: native 패스가 수동 해제 없는 self-host 소스를 컴파일해 gen1 → gen2 → gen3, gen2 == gen3. 메모리 측정 | GPT |
| P10 | 착지 게이트 전체 실행. 리뷰 후 한 번에 착지 | GPT 실행, Claude 리뷰 |

P2–P7은 같은 사실 형식을 공유하므로 P1이 끝나기 전에 시작하지 않는다.
Claude는 P2–P9 동안 슬라이스마다 계약 대비 리뷰를 하고, 막히는 규칙에 core를 낸다.

## 착지 게이트 (전부 한 번에 녹색일 때만 착지)

1. 27 §5.8 픽스처: C·LLVM 출력이 수동 해제 없는 빌드와 같고, Linux에서 ASan·LSan
   누수 0, UAF 0. 음성 픽스처는 지정 코드로 거부.
2. native·self-host 관측 출력이 같은 픽스처에서 같다. 같은 snapshot/canonical
   identity에 묶인 MIR JSON 바이트 동일; unknown/stale/cross-snapshot 사실 거부.
3. DRV-2 전체 고정점 gen2 == gen3. 공식 설치 절차를 격리 prefix에서 실행한
   candidate driver/launcher 경로도 검증한다. 단독 candidate exe 실행을 설치
   gate로 대신하지 않는다. prefix/경로 주입의 현 지원은 **착수 전 읽기 전용
   확인** 항목이다. 미지원이면 사용자 결정 전까지 설치 의무는 BLOCKED이며,
   P1이라는 이름으로 설치 도구를 암묵적으로 고치지 않는다. 공식
   bin 교체와 그 경로의 재검증은 최종 확인 후의 별도 설치 단계다.
4. 정상 허용 프로그램의 일반 값 수동 해제 호출 0개(컴파일러·테스트), 옛 주인
   0개, 재등장 금지 래칫. 금지 표면을 거부시키는 명시적 negative-fixture
   manifest만 예외다. 그 fixture는 지정 진단과 산출물 미발급도 검사한다.
5. 컴파일러 빌드 최고 메모리가 3 GiB 이내(측정 스크립트).
6. SoT 레지스트리 게이트, 컴포넌트 계약, 책임 상한, likeness, CI 프로필, 게이트
   도달성.
7. 형식 증명 전체 커널 검사와 모델/추출 집중 gate. 최종 integration에서는
   모델 파일 변경 여부와 무관하게 실행한다; 이번 문서 전용 검토는 그 실행이 아니다.
8. P0 기준선 대비 새로 빨개진 게이트 0개와 동일 candidate SHA의 실제 CI PASS.
   1–7의 필수 게이트와 DRV-2는 기존 빨강이어도 예외 불가. 무관한 기존 실패의
   exact-input/code/revision allowlist는 **원인 분류**일 뿐 CI 면제 목록이 아니다.
   실제 필요한 CI workflow/job가 실패·누락되면 착지하지 않는다. 요구 profile은
   기존 CI profile 주인에서 읽으며 별도 문서 목록으로 축소하지 않는다.

위 필수 착지 조건 하나라도 빨가거나 미실행이면 착지하지 않는다. 부분 착지는 없다.

**원격 검증 순서:** 로컬 1–7(격리 설치 포함)과 baseline 대조, Claude 리뷰 →
사용자 commit/push 권한 확인 → 검증된 명시적 변경 집합의 candidate commit →
`codex/` candidate branch push → 승인된 CI 실행 요청 → 같은 SHA 실제 CI →
최종 확인/동일 SHA 착지 → 공식
설치/공식 installed-driver 경로 재검증. candidate commit은 모든 gate 입력을
포함해야 한다. 검증에 쓰인 source를 dirty로 남긴 채 commit SHA만 같다고
하지 않는다. 같은 worktree의 모든 gate 입력이 승인 candidate SHA와 diff 0인
상태를 기록하고 로컬 필수 게이트를 재실행한다. 타 레인의 WIP가 필요하면 그
변경의 포함/공개 승인을 받기 전까지 원격 단계는 멈춘다. 기록된 미push 선행
커밋 3개도 branch push에 포함될 수 있으므로 최신 ancestry와 그 공개 권한을
확인한다. main 착지는 검증한 SHA의 fast-forward여야 하며 SHA가 달라지면
로컬 증거와 CI를 다시 묶어 실행한다. destructive reset/새 worktree로 WIP를
치워 diff 0을 만드는 것은 금지한다.

현재 `.github/workflows/ci.yml`의 push trigger는 main 전용이다. candidate branch
push만으로 CI가 실행됐다고 하지 않는다. 기존 workflow_dispatch(또는 별도
승인된 PR)의 ref/SHA와 필요한 full profile을 확인하고 해당 외부 실행 권한을
받는다. PR의 `github.sha`는 merge commit일 수 있으므로 candidate HEAD와
같다고 가정하지 않는다. 승인된 dispatch ref 또는 실제 검사된 merge SHA에
gate 입력과 로컬 증거를 묶는다. merge 결과가 검증된 candidate와 다르면
동일 SHA 착지 조건을 충족하지 않으며 다시 검증한다. 현 change-scope 주인은
base가 없으면 full/non-markdown 실행으로 분류하지만, 이는 소스 관측이지
원격 CI 실행 증거가 아니다. 격리 prefix가 미지원이면 사전 사용자 결정 없이 구현/공식 bin 교체로
우회하지 않는다.
candidate commit/push는 공개 착지가 아니다. 원격 권한이 없으면 그 동작만
보류하고 로컬 작업은 진행한다. I8의 원격 CI를 요구하면서 I8 전 모든 push를
금지하는 순환 조건은 사용하지 않는다. main 강제 push/부분 설치/GUI 준비
메시지는 금지한다. 설치 경로 검증과 GUI 전달은 CI PASS와 최종 확인 이후다.

공식 설치 전 기존 유효 artifact bundle의 해시와 복구 권한·절차를 정한다.
공식 경로 재검증 실패 시 설치/준비 완료를 선언하지 않고 GUI 전달을 막는다.
승인된 기존 복구 절차만 사용하거나 사용자에게 복구 지시를 요청한다. 소스
WIP를 reset하거나 낡은 native fallback으로 성공을 꾸미지 않는다.

## 반증 케이스 (전환 트리에서 먼저 실패해야 하는 것)

- 얕은 복사 위에 drop만 넣으면 이중 해제(`alias_copy_double_free`)
- 요약이 없거나 길이가 틀린 호출을 빌림으로 읽기
- 부분 초기화, 조기 반환, 루프 출구, 분기 한쪽만의 move
- 중첩 `Array<String>`, 구조체 안 컬렉션, HashMap 값의 String
- inout 멤버 경로(`Push(holder.items)`)와 임시값(`Count(MakeItems())`):
  지금 거부되는 두 형태가 통과로 바뀌는지
- 두 개 이상 inout와 별도 반환값; 실패·조기 출구에서 모든 inout의 소유권 회수
- root/field overlap, 중복 inout place, 동적 index alias와 index 한 번 평가
- backing의 마지막 직접 사용 뒤에도 Slice/borrowed getter 결과를 읽는 경우
- Slice가 live인 상태의 push/grow/reset/inout/sink; owner 이동 또는 정리로 view를
  무효화하지 않아야 한다. backing 수명이 늘어 실제 독립 copy가 필요해지면
  observer/진단에 view 의존 원인을 표시한다. 복사로 write-through 의미를 숨기지 않는다
- owned String/nested 원소 교체·Pop/discard·중복 삽입·clear·성장·부분 실패
- 수동 해제 호출이 다시 나타나면 래칫이 거부

## 위험과 대응

| 위험 | 대응 |
|---|---|
| 메모리 상한(이미 초과) | P0에서 오늘 최고치를 잰다. P9에서 수동 은퇴 없는 최고치가 상한을 넘으면 착지하지 않고, 원인 연산을 찾는다. 상한을 올리는 건 사용자 결정 |
| 수정 범위가 크다(테스트 399개 파일, JSON 소비자 48개, 복사 지점 52곳) | 기계적 변경은 목록 기반으로 하고, 목록 자체를 P0에서 고정한다 |
| 다른 레인이 같은 파일을 동시에 고침 | 아래 "현재 보류와 사전 확인" 1; 승인 snapshot 재고정 |
| 기준선이 이미 빨감 | P0 목록으로 구별. 필수 착지/DRV-2 실패는 고친다. 무관한 실패의 명시적 allowlist는 실패 기록이지 PASS 대체가 아니다 |
| 부트스트랩 실패 | 착지하지 않는다. 전환 트리에서 원인을 고친다 |
| 설치본 교체 | `bin/pgy.exe`, `bin/pgy-self-driver.exe`는 I8 전에 덮어쓰지 않는다 |

## 현재 보류와 사전 확인

1. **위치:** 기존 사용자 지시대로 같은 main worktree. 기존 dirty 변경을 보존하며
   구현 재개 시 이 전환 파일의 겹치는 쓰기를 동결한다. snapshot/hash가 바뀌면
   재검증한다. 지금의 dirty 진단을 clean-SHA 검증으로 바꾸어 기록하지 않는다.
2. **보류:** 2026-10-09 최신 지시대로 문서 모순 정리만 실행한다. 읽기 전용
   마지막 체크 후 착수 확인 전까지 구현은 보류한다. 옛 manual chain은 계속
   SUPERSEDED다. user의 최종 확인을 구현/배포 완료로 앞당겨 쓰지 않는다.
3. **기준선:** 게이트 8의 공식 P0 기준선은 모든 gate 입력이 승인 baseline SHA와
   diff 0인 상태를 확보하기 전까지 **BLOCKED**다. 현재 dirty 트리 결과는 진단
   기록일 뿐 공식 기준선이 아니다. 승인된 타 레인/사용자 변경의 local checkpoint
   commit 등 diff 0을 만들 수단과 그 포함 범위를 사용자가 결정한다. stash는
   별도 허용 없이 쓰지 않는다. 필수 게이트/DRV-2 빨강은 착지 예외가 아니다.
4. **테스트:** 수동 호출 수가 아니라 검증 성질별로 이전한다. 허용 프로그램에서
   수동 정리를 제거하고, 금지 호출 negative fixture는 manifest로 유지한다.
5. **상한:** 3 GiB 유지. 실패하면 원인 연산을 찾아 기존 정책 안에서 수정한다.
   더 큰 상한은 별도 사용자 결정 없이는 사용하지 않는다. peak live bytes와
   누적 할당은 다른 값이며, 오래 살아 있는 저장소를 누적 할당처럼 해석하지 않는다.

commit/push·설치본 교체·GUI 메시지는 각각 해당 권한과 위 검증 순서를 확인한다.
이 지시서는 외부 쓰기 권한을 새로 발급하지 않는다.

## 미확인 (착수 전에 확인)

- native와 self-host MIR JSON이 지금 바이트 동일한지
- 공식 P0 baseline snapshot과 gate-input diff 0 확보 수단/포함 권한(현재 BLOCKED).
  candidate뿐 아니라 baseline에도 적용; 임의 stash/reset/새 worktree 금지
- 오늘 트리의 DRV-2 전체 고정점 메모리 최고치
- 최고치 순간의 live bytes(자동 정리가 줄일 수 있는 최대치). 09-28 측정 summary에는
  입력 경로가 없어서 그 입력도 미확인이다
- 155줄과 컴파일러 전체 사이의 메모리 증가가 선형인지
- collection/String argument/result/mutation ABI의 실제 주인과 registry identity
- Slice의 loan·현재 수명/generation certificate 생산자(현 descriptor에는 세대 없음)
- retired builtin tombstone/정확한 거부 코드와 negative-fixture manifest
- 착수 전 격리 prefix/installer route 지원; 미지원 시 사전 사용자 결정
- 승인 candidate snapshot에 타 레인 WIP·미push 선행 커밋을 포함/공개할 권한
- candidate CI trigger/ref/full profile와 동일 SHA main 착지 조건
- `abi.hashmap_runtime_release`, `resource.region_allocation_plan`에 새 소비자가
  생기는지
- 일부 native 해시맵 게이트가 옛 주인에 얼마나 기대는지
- 현재 전체 component gate의 결과와 실제 build pressure; 앞선 읽기 전용 검토의
  pressure 구조 gate PASS는 실제 peak 측정이 아니다. component 진단 재실행의
  60초 timeout도 semantic verdict가 아니다

## 협업 기록

- 2026-10-09 GPT, 최신 범위: 사용자가 "우선 모순쪽만"으로 범위를 축소했다.
  그 시점까지 문서만 수정했으며 compiler/runtime/proof 변경은 없다. 문서 모순
  정리와 읽기 전용 Claude 재검토까지만 진행하고 구현은 다시 보류한다.
- 2026-10-09 GPT: 사용자 직접 수정·착수·Claude 교차 검토 지시에 따라 계획을
  보완했다. 선행 admission, 다중 inout/표현식 정규화, 결과 출처/Slice 수명,
  컬렉션 변경 수명, 단계별 안전 장벽, canonical JSON과 CI/baseline/negative
  fixture 조건을 전체 사슬에 추가했다. P0/P1 실행 전 문서 보완이며 구현 완료가
  아니다. 이후 최신 범위 축소로 구현은 보류한다.

- 2026-10-08 Claude: 읽기 전용 조사 4갈래로 이 계획을 썼다. 코드는 바꾸지 않았다.
  협업 문서의 GPT GT3을 이 계획의 P0–P10으로 대체한다.
- 2026-10-08 Claude: 메모리 감사(`docs/audits/compiler_memory_pressure_2026-10-08.md`)를
  쓰고 H절, P0, 당시 "사용자 결정" 5(현재 "현재 보류와 사전 확인" 5),
  미확인에 반영했다. 코드는 바꾸지 않았다.
