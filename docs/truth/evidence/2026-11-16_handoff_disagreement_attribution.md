# 2026-11-16: 두 프런트엔드의 hand-off 불일치 원인 (S0)

Claude root lane의 S0 트랙이 남긴 측정 기록이다. 대상은
[hand-off 규칙 연구](../../audits/unified_handoff_rule_study_2026-11-16.md)의
S0 조각(§3.5, §5.5)이고, 따르는 결정은
[하나의 hand-off 규칙](../decisions/2026-11-16_one_handoff_rule_for_both_front_ends.md),
[generic 매개변수](../decisions/2026-11-16_generic_parameter_follows_the_move_rule.md),
[getter loan](../decisions/2026-11-16_getter_loan_extends_to_reused_locals.md),
[hand-off는 해제하지 않음](../decisions/2026-11-15_handed_off_array_storage_is_not_released.md)이다.
코드는 바뀌지 않았다. 계측은 스크래치 트리에만 있었고 커밋하지 않았다.

## 입력과 바이너리

- 트리: WSL `Ubuntu-C-WSL`의 `/root/ts0`, 내용은 `cd33a0031`(origin/main).
- 비교에 쓴 바이너리: `/root/tg-own/bin/pgy`, `/root/tg-own/bin/pgy-self-driver`
  (`f1456722d` 내용). `/root/ts0`에서 상대 경로로 실행했다. self-host
  driver는 절대 경로 입력과 작업 트리 밖 출력을 거부한다.
- 계측 native: `/root/ts0/bin/pgy`. `cd33a0031`에 stderr 추적(`S0_TRACE`)만
  더했다. 손댄 곳은 `collection_handoff_owner.c`(호출 간선, 생성자 입력,
  선택), `default_aggregate_call_move_owner.c`(이동 간선과 formal 요약),
  `default_aggregate_move_owner.c`(`refuse_use`, `lent_use_admitted`),
  `loan_copy_landing_owner.{c,h}`(Clone을 고른 소유자 함수)다. 1825개
  입력 모두에서 종료 코드와 MIR JSON이 tg-own native와 바이트 단위로 같았다.
- 입력:
  - `tests/self_hosted/parity/fixture/{collection_field_lifetime,aggregate_storage_transfer}`
    868개(아래 "lifetime");
  - `tests/cases/function_param_flow_summary` 957개(아래 "cases");
  - `tests/self_hosted/parity/ownership/direct_mir_array_string_cleanup_fact_owner.sh`의
    인라인 프로그램 6개.
- 명령: native `pgy --native-pipeline --mir-json <f>`, self-host
  `pgy-self-driver --emit-mir-json-verified <f> -o <rel>`.
- 비교 키: MIR `collection_ownership_facts`에서 disposition이 `handed-off`인
  행, (프로그램, 루틴, 로컬 이름). MIR 행에는 호출 간선이 없다. 간선은 native
  추적(`S0HCALL`, `S0HCTOR`, `S0HSEL`)과 MIR 문장 텍스트에서 찾았다.
  불일치 행마다 hand-off 후보 간선은 하나였다(원인 4는 생성자 입력 두 개 중
  하나에 Clone이 선다).

## 집계

| 입력 | native 승인 | self-host 승인 | 둘 다 | 일치 | native만 | self-host만 | 불일치 프로그램 |
|---|---|---|---|---|---|---|---|
| lifetime 868 | 572 | 395 | 360 | 119 | 38 | 29 | 47 |
| cases 957 | 620 | 578 | 423 | 41 | 2 | 0 | 2 |

- 숫자는 프로그램 수가 아니라 hand-off 행 수다. lifetime에서 native는
  157행, self-host는 148행을 낸다. `f1456722d` 기록의 119 / 38 / 29와 같다.
- 그 기록은 둘 다 승인한 프로그램을 278개로 적었다. 같은 바이너리로 다시
  재면 360개다. 278이 어떤 실행 집합에서 나왔는지는 확인하지 못했다.

## 원인별 귀속 (69행)

칸은 연구 §2.2 표(S5 기준)와 §5.3 표(S5b 기준)를 가리킨다.

| # | 원인 | 행 (native만 / self만) | 프로그램 | 예 | 결정된 규칙의 칸 | 바뀔 쪽 |
|---|---|---|---|---|---|---|
| 1 | self-host가 aggregate release plan으로 예약한 owned-element 생성자 입력을 `retired`로 둔다(`SemanticAstCollectionAggregateReleaseReserveAtCompletion`). 생성자 hand-off 소유자는 live 행만 보므로 건너뛴다. native는 `handed-off` | 33 / 0 | 13 | `aggregate_release_error_path_positive`: `OwnedTables`의 `names`, `params`, `returns` → `SemanticAstExpressionFunctionTableFacts(...)` | 이동 멤버(생성자=`Keep`), 정의 하나인 로컬, 나중 사용 없음, 한 번 → Hand off. 레코드가 한 번 해제(11-15) | self-host (S5) |
| 2a | native가 owned-element 요구 때문에 호출자의 생성자 입력에 `Clone`을 둔다(`aggregate_constructor_proves`). self-host는 borrowed-literal 저장소를 레코드에 넘긴다 | 0 / 15 | 15 | `exclusive_readonly_sibling_call_positive`: native `NodeSet(true, Clone(types))`, self-host `NodeSet(true, types)` hand-off | 표에 칸 없음 (원소 소유권 축) | Unknown |
| 2b | 같은 `Clone`이 callee의 생성자 입력에 서서, 다음 분석에서 callee formal이 Return에서 Read로 바뀐다. 호출자 로컬은 live로 남는다 | 0 / 9 | 9 | `exclusive_branch_rebind_positive`: `RowsFill` 안의 `Rows(true, Clone(seed))` | 표에 칸 없음 | Unknown |
| 3 | self-host가 나중에 읽히는 로컬을 hand-off한다. native는 10-22대로 `Clone(argument)` | 0 / 5 | 5 | `field_ctor_readonly_positive`: `Carrier(values)` 뒤 `ArrayLength(values)` | 이동 멤버, 로컬, 다시 사용 → whole이면 Clone at argument | self-host (S5) |
| 4 | 한 바인딩이 두 입력이나 한 값의 두 부분으로 간다. native는 앞 입력에 whole `Clone`을 두고 마지막 입력을 hand-off한다. self-host는 Clone 없이 hand-off를 거부해 행이 live다 | 6 / 0 | 6 | `copy_field_disjoint_sibling_positive`: `CopyPair(pair.left, Clone(spare))`, `CopyPair(pair.right, spare)` | 앞 입력: 다시 사용 → Clone at argument. 마지막: 한 번 → Hand off | self-host (S5) |
| 5 | 이름 붙은 필드 struct literal `Facts { names: names, ... }`을 self-host가 생성자 입력으로 세지 않는다. 생성자 입력은 호출 인자 노드에서만 모은다(`ast_collection_constructor_field_input_owner.pgy`) | 1 / 0 | 1 | `string_param_field_opaque_call_valid` | 생성자=`Keep`, 로컬, 한 번 → Hand off | self-host |

합계 40 / 29 (lifetime 38 / 29, cases 2 / 0), 프로그램 49개.

원인별 프로그램:
1. `aggregate_readonly_call_chain_positive`, `aggregate_release_{alternative_returns,error_path,for_exit,outer_restore,sink_default_formal,sink_disjoint_field,sink_distinct_invocation,sink_nested,while_exit}_positive`,
   `aggregate_terminal_owned_return_positive`, `callable_table_owned_release_positive`,
   `nested_owned_string_move_positive`. 레코드를 반환하는 곳이 9개(25행),
   루틴 안에 두는 곳이 4개(8행)다.
2a. `exclusive_{alias_after_use,alias_after_use_loop,focus_update,holder_rebind_restore,inout_argument_exit_stale,inout_argument_exit_writeback,inout_exit_before_growth,inout_scoped_writeback,loop_rebind,loop_rebind_branch,nested_path_focus,readonly_ref_sibling_call,readonly_sibling_call,return_before_growth,sibling_value_reads}_*`.
2b. `exclusive_branch_rebind_{continue_negative,missing_else_negative,positive,reads_old_negative}`,
   `exclusive_loop_old_alias_{reread_negative,unread_positive}`,
   `exclusive_rebind_after_{earlier_break_positive,later_break_negative}`,
   `exclusive_grown_field_ref_holder_positive`.
3. 생성자: `field_ctor_identity_input`, `field_ctor_indexed_readonly_positive`,
   `field_ctor_readonly_positive`. 호출: `exclusive_copy_landing_call_positive`,
   `exclusive_borrowed_wrap_negative`. 뒤의 둘은 callee formal이 Return만 하는
   `Array`라서 S5b(§5.3)에서는 loan 칸으로 옮겨 가고, 그때는 native도 바뀐다.
4. `copy_field_{disjoint_sibling_positive,overlapping_sibling_negative}`와
   `nonowner_risk_local_nominal_carrier_return_landed_valid`(10-22 이동 후 사용),
   `exclusive_array_copy_negative`(다른 쪽이 바뀐 로컬 복사),
   `member_completed_read_shared_backing_positive`(10-23 한 저장소가 두 부분),
   `empty_capture_two_fields_positive`(`aggregate_constructor_proves`).

연구가 예로 든 원인 가운데 868개와 cases에서 행이 0인 것: alias를 거친 성장,
`Unproved` 효과, generic formal, 반복 술어. alias 성장은 게이트 인라인
프로그램에서만 보인다(아래). 반복 술어는 코드에서 갈린다. self-host 호출
hand-off의 '한 번' 판정(`SemanticAstCollectionCallRetirementSiteForOrigin`)은
`function_id` 0으로 불려 defer와 루프만 반복으로 보고 lambda, async,
parallel 본문은 보지 않는다. native는 넷 다 반복으로 본다. 이 차이를 밟는
fixture는 없다.

### 게이트 인라인 프로그램

| 프로그램 | native | self-host | 원인 | 바뀔 쪽 |
|---|---|---|---|---|
| `grown_handoff` | `names` handed-off | live | self-host가 `var grown = values` 뒤의 성장을 못 본다 | self-host (S3) |
| `grown_loop` | live, 반복 호출에 `Clone(names)` | live, Clone 없음 | 반복 → whole이면 Clone. 행은 같고 Clone에서만 갈린다 | self-host |
| `grown_record` | live, `NodeSet(true, Clone(types))` | handed-off | 원인 2a | Unknown |
| `constructor_twice` | 앞 입력 Clone, 뒤 입력 handed-off | live | 원인 4 | self-host |
| `constructor_loop`, `constructor_handoff` | 일치 | 일치 | - | - |

## 연구 가정과 어긋나는 것

1. **원인 2(24행, 불일치의 35%)는 표에 칸이 없다.** native의
   `aggregate_constructor_proves`
   (`collection_owned_element_member_requirement_owner.c`)는 레코드 필드에
   owned String이 필요할 때 borrowed-literal `Array<String>` 입력을 깊게
   복사한다. 표 2.1의 효과 집합에는 원소 소유권 축이 없다. 어느 쪽이 바뀔지는
   결정이 필요하다.
   ```json
   {"status": "Unknown",
    "reason": "owned-element 생성자 Clone이 하나의 hand-off 규칙에 속하는지 정해지지 않음",
    "missing": ["self-host가 이 Clone을 들이고 저장소 closure를 거기서 끊을지(11-12)",
                "native가 borrowed-literal 입력을 owned 필드로 hand-off하고 원소 소유권을 따로 나를지"]}
   ```
2. **2b는 'native가 레코드에 담긴 Array 반환을 loan으로 본다'가 아니다.**
   `exclusive_branch_rebind_positive`의 첫 분석에서 `RowsFill.seed` 요약은
   `mask=0x8`(RETURN), `array_storage_unproved=1`, moves=1이고, `Main.seed`는
   hand-off 후보였다(`S0HSEL admitted=1`). `RowsFill` 안에 owned-element
   `Clone`이 선 다음 분석에서 `mask=0`, moves=0이 되어 `seed`가 live로 남는다.
   그래서 S4(레코드 안 `Keep`/`Return`)는 이 9행을 바꾸지 않는다(연구 §2.4
   2번, §3.5 S4).
3. **원인 1은 반환되는 생성자에만 있지 않다.** 4개 프로그램(8행)은 레코드가
   루틴 안에 남는다(연구 §2.4 3번은 반환 생성자만 적었다).
4. **hand-off 행 비교는 Clone 행동을 보지 못한다.** 둘 다 승인한 783개
   프로그램(lifetime 360, cases 423)에서 MIR 문장 텍스트의 `Clone(` 수가
   소스보다 많은 프로그램은 native 274개(초과분 합 449), self-host 0개다. self-host의
   direct MIR은 복사를 싣지 못하고(`shared_storage_copy_landing_owner.sh`),
   복사가 필요한 곳에서는 프로그램을 거부하거나 복사 없이 둔다. native가
   승인하고 self-host가 거부한 lifetime 212개 중 139개, cases 197개 중 91개에
   native `Clone`이 있다. 그래서 S2가 기대한 "행동 수준에서 119 / 38 / 29
   재현"은 성립하지 않는다. 행동 수준 G2는 `Clone` 착지를 같이 비교해야 하고,
   self-host MIR에는 그것을 실을 자리가 먼저 필요하다.
5. **native는 이미 로컬도 빌려준다.**
   [2026-10-19 로컬 getter 결정](../decisions/2026-10-19_local_getter_lends_until_handed_on.md)은
   README에서 "in force"이고, `by_value_getter_lend_local_valid`는 native에서
   `Clone` 없이 loan으로 승인된다. canonical 입력에서도 getter-on-local
   105곳 중 50곳이 복사 없는 loan이다. 11-16 getter 결정의 "10-19는 formal만
   빌려줬다"와 연구 §5.3의 "native는 `Clone(argument)`를 둔다"는 `Array`
   formal(native가 빌려주지 않음)과 loan 증명이 실패한 곳에만 맞는다.

## getter-on-local

정의: callee formal이 Return만 하고(native `lends=1`, 또는 `Array` formal의
요약이 READ|RETURN뿐), 인자가 멤버 경로가 아닌 로컬 전체이며, 호출 뒤에 그
로컬을 다시 쓰는(`S0USE`가 그 호출을 가리키는) 간선. 결과는 native가 고른
것이다. `Array` formal은 native 요약으로는 반환과 성장을 가를 수 없어서 따로
센다.

### fixture (native 추적)

| 입력 | 지점 | loan (복사 없음) | `Clone(call)` | `Clone(argument)` | 프로그램 거부 | 기타 |
|---|---|---|---|---|---|---|
| lifetime, 비`Array` | 33 | 18 | 15 | 0 | 0 | 0 |
| lifetime, `Array` 반환 | 7 | 0 | 0 | 7 | 0 | 0 |
| cases, 비`Array` | 63 | 16 | 26 | 2 | 17 | 2 (복사·loan 기록 없이 승인) |
| cases, `Array` 반환 | 7 | 0 | 0 | 5 | 2 | 0 |

- 오늘 이 지점에 서는 복사: lifetime 22개, cases 33개. 비`Array`의 복사는 거의
  모두 `Clone(call)`, 곧 loan 증명이 실패한 곳이다(10-20, 10-24).
- 같은 지점에서 self-host: lifetime은 40개 중 33개가 프로그램째 거부, 7개는
  복사 없는 호출. cases는 70개 중 31개 거부, 39개 복사 없음. native가
  프로그램째 거부하는 cases 지점 19개 중 16개를 self-host MIR 경로는 복사
  없이 승인한다.

### canonical 입력 (native 추적, 한 번 실행)

`/root/ts0/bin/pgy src/self_hosted/compiler/driver/driver_bootstrap_main.pgy
--native-pipeline --emit-c`: 0 error, 27 warning, 벽시계 27분 51초(다른
트랙과 호스트 공유, 추적 출력 포함), 최대 RSS 2.75 GB. 나온 C는
`/root/tg-oracle2/new.c`(Track G의 `f1456722d` oracle)와 바이트 단위로 같다.

| 구분 | 지점 |
|---|---|
| 로컬 전체를 받는 lending 간선 (다시 쓰든 안 쓰든) | 508 |
| getter-on-local (다시 씀) | 105 |
| loan, 복사 없음 | 50 |
| `Clone(call)`만 | 31 |
| `Clone(argument)`만 (비`Array` 19, `Array` 반환 1) | 20 |
| 둘 다 | 1 |
| 복사·loan 기록 없음 | 3 |

- 오늘 이 지점에 서는 복사는 52곳이다. `Clone(argument)` 21곳 가운데 13곳은
  `refuse_use`가 결과를 복사할 수 없어 lent 인자를 복사한 곳(10-24)이고, 4곳은
  String 원소 소유자(`semantic_collection_owned_string_results_prepare`,
  `definition_select_protected_copies`)가 골랐다. 4곳은 고른 소유자를 추적에서
  짝짓지 못했다. 예: `CompileMachineAdmittedMirJsonToCForTargetVerifiedObserved`의
  `SemanticAstArtifactAnalyzeWithExpressionGraph(Clone(artifact), ...)`.
- 2026-10-19 결정이 적은 "로컬 getter 이동 거부 647건"(`bc2aefb30`)은 지금
  없다. 105곳 모두 승인되고, 절반은 이미 복사 없이 빌려준다. S5b가 native에서
  줄일 수 있는 복사는 이 52곳 가운데 §5.3이 충돌로 보지 않는 곳뿐이고, 몇
  곳인지는 S5b 구현 전에는 모른다.

## 재현

- 각 입력을 native와 self-host로 MIR JSON까지 돌리고, 위 키로 `handed-off`
  행을 비교한다.
- 원인은 `S0_TRACE=1`로 계측 native를 돌려 얻었다. 불일치 행마다 그 로컬의
  `S0LAND`(`by=`가 `Clone`을 고른 함수), `S0HCALL`/`S0HCTOR`(callee formal의
  `mask`, `outward`, `array_storage_unproved`, moves), `S0HSEL`(정의 수, 반복,
  루프 깊이)을 읽었다.
- 스크립트와 계측 패치는 커밋하지 않았다.
