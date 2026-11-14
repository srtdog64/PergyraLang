# Native driver의 207개 오류 전수 분석과 수정안

2026-11-06 KST. 읽기 전용 감사와 수정 제안이다. 이 감사는 production
compiler를 수정하거나 메인의 진행 상태를 소유하지 않는다.

207개는 독립적인 소유권 결함 207개가 아니다. 최종 진단 전부를
21개 모듈·35개 함수·207개 syntax identity에 연결했다. 앞선 잘못된
대여 거절 3개가 생성자 결과의 발급 실패를 만들고, 이후 정상 String
식의 정보 발급을 막는 사슬을 소스와 실행 상태에서 확인했다.
이 사슬을 수정한 실행 파일에서 207개가 모두 없어지는지는 별도
canonical 실행으로 확인해야 한다. 이 감사 자체가 그 성공 증거는 아니다.

## Objective card

- 목적: 고정 canonical 입력의 모든 진단을 원인과 소비자에 연결하고,
  하나의 소유권 계약으로 고칠 범위·순서·반례를 고정한다.
- 우선순위: 안전과 의미 identity, 기존 fact owner, 전체 오류 연결,
  금지 fallback 제거, 재현 가능한 검증 순서.
- Fact owner: `semantic.function_param_flow_summary`가 최종 formal의 효과를,
  `function_param_publications`가 typed call/value/nominal receipt를,
  `indexed_string_borrow_owner`가 String 저장의 출처·수명을 소유한다.
- 마지막 소비자: borrowed call admission, nominal result publication,
  shallow String store admission. 이후 MIR와 C/LLVM은 아직 도달하지 못했다.
- 금지: 오류 억제·warning 전환, canonical getter에 `ref` 추가, 얕은 복사,
  import/입력 축소, 미완료 summary 허용, 프로그램 이름별 허용 특례.
- 검증: 같은 전체 입력에서 오류 0개와 C 산출물을 확인하고, 기존
  허용·거절 및 실제 해제 검증을 유지한 뒤 전체 driver 고정점을 실행한다.
- 반증: 한 진단이라도 다른 원인·stale identity·실제 retention을 가리키면
  아래 수정 경계를 다시 조사한다. 같은 메시지만으로 같은 원인이라 보지 않는다.

## 고정한 입력과 실행 파일

| 항목 | 값 |
|---|---|
| 공유 checkout HEAD | `60fd1e26d8ea3987efbcbfc65f59b892b234701a` + 기존 dirty tree |
| 실제 분석 입력 | `/root/pgy-join-driver-20261105/r8`의 고정 전체 import graph |
| canonical entry | `src/self_hosted/compiler/driver/driver_bootstrap_main.pgy` |
| entry SHA-256 | `8e5e115bbe25063f57b03a58f44cbcf16e8d86e104de99a6efc708a3fa565a12` |
| 12,908개 입력 ledger SHA-256 | `2f9b650f6c55c8810c14a798c4d4b0d706ca049892aa4bbea70a0e0dfa4cf246` |
| 원본 native SHA-256 | `c0e0a6d89e44580a80e09c688bba316d124589babd1b9ddc7aa05c339cfeacab` |
| 관찰만 추가한 native SHA-256 | `70136def4b9e8afd69475e074d59e54c1cbc46e2ca88d8c28f9ea2ddfa612f65` |
| 관찰 실행 결과 | 173.140초, exit 1, 오류 207개·warning 23개, C 산출물 없음 |

원본 C source/object/binary는 덮어쓰지 않았다. 두 C owner의 임시 복사본에
기존 실패 분기의 상태 출력만 추가했다. 판정·반환값·진단 severity는
그대로 유지했다. 두 추적 실행의 전체 로그가 byte-equal이며, 마지막
진단 JSON도 기존 원본 로그와 byte-equal이다. 입력 ledger 재검사도
12,908개 전부 통과했다.

실행 명령의 의미는 다음과 같다.

```text
pgy --native-pipeline --error-format=json --emit-c \
  src/self_hosted/compiler/driver/driver_bootstrap_main.pgy -o driver.c
```

이는 고정 R8 입력과 식별된 native의 결과다. 현재 공유 소스나 새로
설치한 메인 실행 파일도 여전히 207개라고 일반화하지 않는다.

## 모든 오류의 연결

| 최종 진단 | 수 | 직접 발생 지점 | 확인한 상태 |
|---|---:|---|---|
| borrowed default 호출 거절 | 3 | `DriverRung2MirCPayloadRequestIdentitySnapshot` 97·98·99행 | 동일한 `machine_request`가 읽기/필드 getter의 default formal로 들어가면 즉시 거절 |
| nominal result publication 실패 | 1 | 같은 함수 95행 | 두 번째 actual의 row는 있으나 `value_present=0`, `call_present=0` |
| shallow String source receipt 실패 | 203 | 20개 후속 모듈·34개 함수 | 전부 row는 존재하지만 `value_present=0`, `indexed_string_source_present=0` |

[207개 전체 목록](native_driver_207_errors_2026-11-06.inventory.tsv)은
진단 순번, 원인군, node/routine identity, 모듈, 행·열, 실제 source line,
메시지를 포함한다. 각 위치는 그 파일의 실제 함수 범위와 다시 대조했다.

후속 String 진단의 분포는 아래와 같다. 개별 모듈을 수정할 목록이 아니다.

| 모듈 | 수 |
|---|---:|
| `mir_lower/declaration_method_contract_fact_owner.pgy` | 1 |
| `mir_lower/routine_inventory_owner.pgy` | 5 |
| `mir_lower/decl_lower.pgy` | 5 |
| `mir_lower/program_lower.pgy` | 2 |
| `mir/intent_execution_json_projection_owner.pgy` | 6 |
| `mir/match_json_projection_owner.pgy` | 3 |
| `mir/destructure_json_projection_owner.pgy` | 1 |
| `mir/destructure_type_json_projection_owner.pgy` | 1 |
| `mir/generic_specialization_json_projection_owner.pgy` | 4 |
| `mir/abi_layout_json_projection_owner.pgy` | 88 |
| `mir/nominal_abi_layout_json_projection_owner.pgy` | 1 |
| `mir/declaration_json_projection_owner.pgy` | 26 |
| `mir/runtime_call_abi_json_projection_owner.pgy` | 1 |
| `mir/expression_identity_json_projection_owner.pgy` | 2 |
| `mir/domain_runtime_assignment_json_owner.pgy` | 3 |
| `mir/local_ref_json_projection_owner.pgy` | 1 |
| `mir/expression_graph_json_projection_owner.pgy` | 7 |
| `mir/json_projection_owner.pgy` | 44 |
| `compiler/direct_mir_routine_parameter_set_admission_owner.pgy` | 1 |
| `compiler/direct_mir_composite_intent_program_graph_fact_owner.pgy` | 1 |
| 합계 | 203 |

최종 round에서는 204개 syntax node의 해당 실패 분기가 276회 실행됐다.
최종 JSON에는 203개로 나타난다. 진단 dedup은 module/node identity 없이
level·행·열·메시지를 비교하므로, 진단 수와 실패 node 수를 같게 세면 안 된다.
추가 node `627327`도 같은 미발급 상태이고 `SelfMirJsonInstruction`에
속하지만, 기존 trace에는 dedup된 그 node의 행·열이 없다. 추정 위치를
207개 목록에 끼워 넣지 않았다. 이 차이는 새로운 소유권 원인의 증거가 아니다.

## 원인 사슬

```text
driver_app -> semantic_analyze_landing_fixed_point
  -> semantic_analyze_landed -> type_check_program
    -> ref machine_request를 default getter로 전달
       [고정 native: final formal 효과를 보지 않고 즉시 거절 3개]
    -> 해당 call/value receipt 미발급
    -> nominal snapshot의 actual #1 검증 실패
       [node 577970; actual node 577977의 value/call 정보 없음]
    -> 일반 publication 오류가 ctx의 전역 오류 상태에 남음
    -> 이후 정상 식도 checked value/call 정보를 발급하지 못함
    -> shallow String store가 없는 receipt를 요구하며 203개 진단
    -> semantic 단계 종료; MIR/C/LLVM에 아직 도달하지 않음
```

**CONFIRMED — 첫 3개는 default 모드의 일괄 거절이다.**
`type_checker_ownership_call.c`의
`semantic_validate_borrowed_boundary_call_argument`가 소비자다.
기존 `default_aggregate_move_rule.c`는 이미 final parameter summary에서
move/lend/handoff를 구분한다. 반면 고정 native의 호출 경계는 그 사실을
소비하지 않고 borrowed actual + non-ref formal을 바로 거절한다.
상수를 반환하는 getter에도 같은 거절이 발생하므로 실제 escape만을
가리키는 진단은 아니다.

**CONFIRMED — 생성자 실패는 같은 actual의 미발급을 소비한 결과다.**
`function_param_publication_owner.c`의
`semantic_param_publication_record_nominal_result`에서 두 번째 actual
검증이 실패했다. 그 actual은 `DriverRung2MirCMachineMode(machine_request)`다.
row 자체의 할당 실패나 String 필드 타입 불일치가 아니었다.

**CONFIRMED — 203개는 미발급된 정보의 후속 실패다.**
`type_checker_expr.c`는 현재 식의 checked publication을
`semantic_param_publication_local_fact_after_borrow_refusals`로 제한한다.
이 함수는 앞선 오류 중 일반 publication 오류가 하나라도 있으면 false다.
따라서 nominal publication 오류 이후, 서로 다른 함수의 정상 String 식도
value/source receipt가 없어진다. `indexed_string_borrow_owner.c`는 이를
fail-closed로 거절한다. 이 마지막 안전 검사를 삭제할 이유는 없다.

독립적인 작은 대조 입력도 확인했다. `Snapshot`의 parameter에서 `ref`
한 곳만 바꾸었을 때 default 버전은 오류 0개와 C 산출물을 냈다. borrowed
버전은 3개 대여 거절, 1개 nominal publication 실패, 관계없는
`Unrelated` 함수의 `ArrayPush(values, Fresh())` 출처 오류를 냈다.
이 시험은 전역 전파를 반증 가능한 형태로 확인한 것이며 canonical 입력의
성공을 대신하지 않는다. 처음의 잘못된 `Array<T>()` 시험은 제외했고,
유효한 `var values: Array<String> = []` 입력만 이 비교에 사용했다.

전체 실행에서 같은 첫 호출의 거절은 9차례의 분석 방문에 다시 나타났다.
전체 trace의 1,896개 오류 발급을 서로 다른 결함 수로 세지 않았다.
이는 copy-landing 재분석의 관측치이며, 별도의 cache/최적화 작업을
시작하거나 분석 범위를 축소할 근거로 사용하지 않는다.

## 하나의 계약으로 수정할 순서

1. **호출 경계의 결정을 올바른 시점으로 옮긴다.**
   기존 publication owner가 caller/callee/actual/formal의 정확한 identity,
   모드·타입·generation을 가진 pending obligation을 보관한다. typed input을
   seal하고 final parameter summary가 COMPLETE가 된 뒤 기존 move/lend/handoff
   규칙을 소비한다. typing 중에 아직 없는 final summary를 요구하거나 AST를
   다시 훑는 새 summary owner를 만들지 않는다. 즉시 일괄 거절을 같은
   경로에 fallback으로 남기지 않는다.
2. **읽기와 반환 대여를 분리해 승인한다.**
   읽기만 하는 default formal은 허용한다. String 필드 getter는 반환값이
   독립 소유값인 척하지 않고 기존 String backing/loan owner에 연결한다.
   실제 필드 반환 -> `Concat`의 독립 결과 -> nominal field -> 이후 정리까지
   같은 출처를 따라간다. aggregate 반환, write/release/retention/handoff,
   body 없는 외부 호출, missing/stale/incomplete summary는 계속 거절한다.
3. **결과 정보의 생산부터 마지막 저장까지 함께 확인한다.**
   첫 호출의 checked call/value, constructor actual/nominal, 이후 String source
   receipt를 동일 generation에서 검증한다. 이 연결이 회복되면 203개 모듈별
   패치는 필요 없다. 앞선 타입 오류를 무시하거나 receipt를 추정해 채우지 않는다.
   첫 호출을 정상화한 뒤에도 실제로 미발급이 남는 경우에만 publication owner의
   locally-valid typed fact 계약을 수정한다. 전역 `has_error`를 무조건 무시하는
   변경이나 진단만 숨기는 변경은 금지한다.
4. **같은 전체 입력을 한 번 검증하고 전체 driver 사슬로 이어간다.**
   안전/허용 회귀가 안정된 뒤 source·binary SHA를 고정해 canonical 입력의
   오류 0개와 산출물을 확인한다. 그 다음 전체 driver의 gen2 == gen3,
   산출 compiler 실행, 설치 C/LLVM의 실제 해제와 required CI를 같은 소스에서
   검증한다. 최종 required CI는 같은 공개 commit에서 bootstrap/installed job이
   실제 실행된 결과여야 한다. 기존 codegen-tool 고정점 PASS를 전체 driver
   PASS로 대체하지 않는다.

이 순서는 공통 경계를 한 번 수정하는 계획이다. 알려진 203개 위치를 고치거나
오류가 하나 줄 때마다 전체 bootstrap을 다시 돌리는 계획이 아니다.

## 함께 유지할 실행 반례

| 주장 | 허용 또는 관측 | 반드시 남길 거절/반증 |
|---|---|---|
| default read boundary | 상수 getter, enum/struct 읽기, 안전한 지역 대여 | own 전달, mutation/release, 외부 retention/channel |
| String result provenance | 필드 -> local -> getter -> 독립 Concat -> constructor/ArrayPush | source owner 정리 후 사용, 분기·반복문에서 helper 정리 후 사용 |
| final facts가 권위 | sealed final summary의 효과 사용 | missing/incomplete/stale identity, 실제 retaining getter |
| 정보 발급의 정확성 | 모든 constructor actual과 String store receipt 확인 | 실제 타입 오류 또는 불완전 actual의 nominal 승인 |
| 자동 정리 | C·LLVM의 alloc/free/live와 출력 일치 | drop 억제 시 leak, 이른 drop 시 use-after-free/순서 위반 |
| 완성 경계 | 전체 driver gen2 == gen3와 산출 compiler 실행 | codegen-tool만 같거나 bootstrap/installed/CI가 skip된 성공 주장 |

기존 `function_param_flow_summary_smoke.sh`, semantic ownership boundary suite,
`call_argument_evaluation_order_smoke.sh`, loop cleanup 및 installed-driver C/LLVM
회귀가 출발점이다. 이 감사에서는 생산 코드를 수정하지 않았으므로 해당 전체
수정 후 suite를 실행했다고 보고하지 않는다.

## 현재 메인과의 관계와 전달

공유 소스에는 감사 도중 이미 `PendingBorrowedDefaultCall`의 기록·currentness
검증·final 소비와 read/lending/moving 회귀가 추가된 것을 확인했다. 이는
위 1번 방향의 실제 메인 작업이다. 파일 존재나 코드 열람만으로 실행 성공을
추정하지 않았으며, 이 감사에서 같은 구현을 별도로 덧붙이지 않았다.

메인은 현재 다른 self-host generic/ABI 소비 경계도 조사 중이다. 207개라는
고정 과거 결과와 그 새 작업의 오류 수를 섞지 않는다. 다음 통합 증거는
수정된 메인 native를 식별해 같은 canonical 입력에서 얻은 전체 결과다.

전수 목록과 이 수정안은 페르기라 메인 채팅에 전달했다. 이 lane이 바꾼 것은
감사 문서·목록과 임시 관찰 도구뿐이며, production 코드의 staging/commit/push는 없다.

상세 재현 파일은 `D:\PergyraLang\.tmp\native-207-audit-20261106\`에 있다.
`results/manifest.json`은 결과 파일 hash, 원본 입력 검사와 관찰 실행 상태를
기록한다. 첫 수집기의 JSON 후처리 실패는 별도 parser로 바로잡았고,
성공한 별도 runner에서 compiler exit와 byte-equal 결과를 다시 확인했다.
