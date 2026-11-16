# 2026-11-16: 두 프런트엔드가 MIR에 싣는 hand-off 행 (S2)

Claude root lane의 S2 트랙이 남긴 실행 기록이다. 대상은
[hand-off 규칙 연구](../../audits/unified_handoff_rule_study_2026-11-16.md)의
S2 조각(§3.2, §3.4 G2, §3.5)이고, 따르는 결정은
[하나의 hand-off 규칙](../decisions/2026-11-16_one_handoff_rule_for_both_front_ends.md)과
[원소 소유권 축](../decisions/2026-11-16_getter_loan_correction_and_element_ownership_axis.md)이다.
앞 조각의 기록은 [S0 불일치 원인](2026-11-16_handoff_disagreement_attribution.md)과
[S1 규칙 렌더링](2026-11-16_s1_one_handoff_rule_rendered.md)이다.
S2는 판정을 바꾸지 않는다. 행은 MIR 읽기 쪽이 검증만 하고, 판정에 쓰는 소비자는
S6이 더한다.

## MIR에 실리는 것

루틴마다 `collection_ownership_facts` 뒤에 키 하나 `call_handoff`가 붙는다.
어휘는 모두 `src/semantic/call_handoff_rule.def`의 이름이다. 렌더러가 C
(`pgy_call_handoff_*_name`)와 Pergyra(`SemanticCallHandoff*Name`) 철자표를
같이 만들고, S1 게이트의 두 프로브가 철자표를 덤프해 비교한다.

```
"call_handoff":{"complete":true,
 "class_count":1,"classes":[{"parameter_index":0,"members":["GROW"],
   "divergence":"NATIVE_ARRAY_FORMAL_DESCRIPTOR_ONLY"}],
 "action_count":1,"actions":[{"call_site":32,"callee":"Facts","parameter_index":0,
   "binding_syntax_id":20,"compiler_clone":false,"members":["KEEP"],"origin":"LOCAL",
   "later":"ONCE","context":"SYNC","elements":"AS_GIVEN","action":"HAND_OFF",
   "divergence":"","row":"handed-off"}],
 "clone_count":1,"clones":[{"site":52,"binding_syntax_id":42,"callee":"NodeSet",
   "argument_index":1}]}
```

- 클래스 행 (루틴, formal, 클래스 집합): hand-off 소유자가 호출 지점에서 읽은
  callee formal의 클래스다. 행은 callee 루틴에 붙는다. 모든 formal을 세지 않는
  까닭: native 요약은 요구될 때 계산되므로, 모든 formal을 세면 새 분석이
  시작되고 요구 순서가 바뀐다(native 판정이 요구 순서에 민감했던 기록이 있다).
  divergence는 클래스를 정한 projection 또는 input divergence다(native `Array`
  formal은 `NATIVE_ARRAY_FORMAL_DESCRIPTOR_ONLY`, self-host effect 4·5는
  `SELF_UNPROVED_EFFECT_NOT_MOVING`).
- 행동 행 (루틴, 바인딩, 호출 간선, 행동, divergence): 추적되는 지역
  `Array<String>` 행의 인자 지점마다 하나다. 기본 모드 formal과 생성자의
  `Array<String>` 필드가 대상이다. 소유자가 쓴 입력(members, origin, later,
  context, elements), 표의 답, 답을 바꾼 row/projection divergence, 그 지점에서
  바인딩 행이 handed-off가 되었는지(`row`)를 싣는다. input divergence(예:
  `SELF_LATER_USE_NOT_READ`)는 프런트엔드 전체의 입력 계산 방식이라 행마다
  싣지 않는다. `call_site`는 각 프런트엔드의 호출 식별자다(native AST id,
  self-host 식 그래프 노드). 두 프런트엔드의 id 공간이 달라서 비교는 이름으로
  한다.
- 복사 계획 (`clones`): 컴파일러가 둔 `Clone`마다 하나다. 복사되는 식의 id,
  이름이면 그 바인딩, 그 복사를 받는 호출의 이름과 인자 위치를 싣는다. 인자에
  선 `Clone`은 행동 행에도 `compiler_clone:true`, origin `FRESH`로 나온다.
- `complete`: native가 루틴 본문을 다 걷지 못하면(그 루틴의 행동·복사 행을
  싣지 않는다), 또는 인자 `Clone`의 callee 클래스가 아직 계산되지 않은 요약에
  있으면(그 지점만 뺀다) false다. 요약을 새로 요구하지 않는 까닭은 위와 같다.
  self-host는 항상 true다.

## 생산자와 읽는 쪽

- native: `collection_handoff_owner.c`의 기존 walk가 장부(`call_handoff_ledger.h`)를
  쓴다. 이제 추적 행이 없는 프로그램도 걷는다(복사 행 때문). 인자 `Clone`의
  클래스는 정해진 요약에서만 읽는다(`semantic_default_aggregate_formal_class_settled`).
  `SemanticResult`가 장부를 갖고, MIR은 AST처럼 빌려 쓰며, `mir_json_dump.c`가
  루틴마다 쓴다. MIR 루틴이 없는 행이 하나라도 있으면 `mir_lower`가 실패한다.
- self-host: 호출·생성자 hand-off 소유자가
  `ast_collection_call_handoff_ledger_owner.pgy`에 행을 적고, 소유권 판정이
  장부를 나른다. 복사 행은 선택된 `Clone` 노드를 surface root 구간으로 루틴에
  배정한다. `call_handoff_row_owner.pgy`가 두 JSON 작성기에 같은 객체를 준다.
- 읽는 쪽: `src/self_hosted/mir_lower/call_handoff_row_fact_owner.pgy`가 MIR 수용
  때(모든 경로가 지나는 `MirMachineLayerAdmitDocumentWithTopologyAndDeclarationsObserved`)
  모든 루틴을 검사한다: 필드와 개수, 규칙 어휘, 행동 행의 클래스와 입력이
  규칙으로 그 행동을 내는지(row divergence는 답을 실제로 바꿨는지), 컴파일러
  Clone 지점이 FRESH인지, handed-off 행이 HAND_OFF이고 바인딩마다 한 번인지,
  같은 행이 두 번 나오지 않는지. `--mir-json` 경로는 거부한 단계를 이름으로
  알린다(`MIR call hand-off rows are missing or invalid: <stage>`). 수집 소유권
  행과의 교차 검사는 S6 소비자의 일이다. 지금 넣으면 cleanup 게이트가 위조한
  MIR이 backend 대신 수용 단계에서 거부되어, 그 게이트가 확인하는 거부 지점이
  바뀐다.

## G2 측정

`tests/self_hosted/parity/ownership/call_handoff_differential_measure.sh`,
WSL `/root/ts2`(이 브랜치 `fe33c3235`의 바이너리), native
`--test-native-mir-json-oracle`과 self-host `--emit-mir-json-verified`.
입력은 lifetime 868개와 `tests/cases/function_param_flow_summary` 972개다.

| 입력 | native 승인 | self-host 승인 | 둘 다 |
|---|---|---|---|
| lifetime | 577 | 381 | 362 |
| cases | 626 | 585 | 427 |

### hand-off (S0 기준)

| 입력 | 비교 | 일치 | native만 | self-host만 | 다른 프로그램 |
|---|---|---|---|---|---|
| lifetime | 수집 행 `handed-off` | 121 | 38 | 29 | 47 |
| lifetime | 행동 행 `row=handed-off` | 121 | 38 | 29 | 47 |
| cases | 두 방식 모두 | 41 | 2 | 0 | 2 |

- 두 프런트엔드 모두 handed-off 수집 행과 `row=handed-off` 행동 행이 하나도
  어긋나지 않는다.
- S0의 119 / 38 / 29와 native만·self-host만은 같고 일치가 2 많다. S0이 쓴
  바이너리(`/root/tg-own`, `f1456722d`)를 같은 측정기로 돌려도 둘 다 362,
  121 / 38 / 29가 나온다. 차이는 바이너리가 아니라 S0 실행이 셌던 승인
  집합(360)에서 온다. S0 기록도 같은 바이너리에서 278과 360을 보고했다.

### 행동 수준 (간선마다 행동, compiler_clone, row)

| 입력 | native 행 | self-host 행 | 같음 | 다름 | native만 | self-host만 |
|---|---|---|---|---|---|---|
| lifetime | 312 | 314 | 215 | 97 | 0 | 2 |
| cases | 78 | 77 | 72 | 5 | 1 | 0 |

다른 짝(native → self-host)과 S0 원인:

| 행 | native | self-host | S0 원인 |
|---|---|---|---|
| 33 | HAND_OFF, handed-off | KEEP_ROW_STATE (`SELF_RETIRED_CONSTRUCTOR_INPUT_KEPT`) | 1 |
| 18 | MOVED, 인자 Clone | HAND_OFF, handed-off | 2a(15) + 3 생성자(3) |
| 9 | LEND_FOR_CALL | HAND_OFF, handed-off | 2b |
| 5 (+ cases 1) | HAND_OFF, handed-off | CLONE_ARGUMENT, kept | 4 |
| 2 | 행 없음 | HAND_OFF, handed-off | 3 호출 |
| cases 1 | HAND_OFF, handed-off | 행 없음 | 5 |
| 17 | MOVED, 인자 Clone | HAND_OFF, kept | 행 비교로는 안 보임 |
| 8 | HAND_OFF, kept | KEEP_ROW_STATE | 행 비교로는 안 보임 |
| 5 (+ cases 4) | MOVED, 인자 Clone | CLONE_ARGUMENT, kept | 행 비교로는 안 보임 |
| 1 | LEND_FOR_CALL | HAND_OFF, kept | 행 비교로는 안 보임 |
| 1 | MOVED, 인자 Clone | KEEP_ROW_STATE | 행 비교로는 안 보임 |

- S0의 native만 38 = 33(원인 1) + 5(원인 4), self-host만 29 = 18 + 9 + 2로
  원인별 수가 맞는다.
- "행 없음" 2행: native가 인자 `Clone(rows)`을 둔 호출의 callee 요약이
  계산되지 않아 그 지점을 뺀 두 루틴이다(`complete:false`, lifetime의 native
  incomplete 2개: `exclusive_copy_landing_call_positive`,
  `exclusive_borrowed_wrap_negative`).
- 아래 다섯 줄 32행은 바인딩 행의 disposition이 양쪽에서 같아(둘 다 handed-off가
  아니다) 행 비교로는 보이지 않던 간선 불일치다.

### 클래스 행

| 입력 | native | self-host | 멤버 같음 | kind만 같음 | kind 다름 | self-host만 |
|---|---|---|---|---|---|---|
| lifetime | 55 | 57 | 3 | 26 | 26 | 2 |
| cases | 12 | 12 | 4 | 4 | 4 | 0 |

- kind만 같음: native `{}` 대 self-host `{READ}`(둘 다 READ_ONLY).
- kind 다름: native `{GROW}` 또는 `{}` 대 self-host `{KEEP}`. native `Array`
  formal은 descriptor 비트만 보므로(`NATIVE_ARRAY_FORMAL_DESCRIPTOR_ONLY`, S4)
  반환·보관·성장을 가르지 못한다.
- self-host만 2행: 위의 두 incomplete 루틴이 부르는 `FactsMake`, `Wrap`의
  formal이다. native는 그 요약을 계산하지 않은 채 읽지 않았다.

### 복사 계획

| 입력 | native Clone (프로그램) | self-host Clone (프로그램) | 일치 | native만 | self-host만 |
|---|---|---|---|---|---|
| lifetime | 246 (128) | 102 (85) | 28 | 218 | 74 |
| cases | 217 (157) | 72 (62) | 12 | 205 | 60 |

- 비교 키는 (루틴, 받는 호출, 인자 위치, 바인딩 이름)이다.
- S0의 방법(MIR 문장 텍스트의 `Clone(` 수 - 소스의 수)으로 재면 native
  246 + 205 = 451개, 128 + 148 = 276개 프로그램이고 self-host는 0이다. S0의
  449개, 274개와 같은 측정이다. 둘 다 승인한 프로그램이 362 + 427로 S0의
  360 + 423보다 6개 많다.
- 새로 보이는 것: self-host도 174개의 컴파일러 Clone을 고른다. MIR 문장
  텍스트에 없을 뿐이다. 인자에 선 Clone의 불일치는 lifetime 186개
  (native만 149, self-host만 37), 그 밖의 자리(초기화, 반환 등)는 106개다.

이 숫자는 측정이다. S5까지 통과 기준이 아니고, 게이트에 개수 핀은 없다.

## 게이트

- `make call-handoff-rule-test-smoke`(S1, core shard): 두 프로브가 판정 칸과
  함께 MIR JSON 철자(멤버, 축 값, 행동, divergence 39개)를 덤프하고, C 헤더와
  Pergyra owner의 덤프가 같아야 한다.
- 새 음성 게이트 `make self-host-call-handoff-row-reader-test-smoke`
  (`tests/self_hosted/parity/ownership/call_handoff_row_reader_owner.sh`,
  `self-host-collection-ownership-semantic-test-smoke`의 선행이라 self-host
  shard에서 돈다): 생성자 hand-off와 callee formal이 있는 한 프로그램의
  native·self-host MIR을 설치된 driver가 받아들이고, 위조한 행 11종을 두
  생산자 × `--mir-json`·`--mir-json-backend=llvm`에서 그 단계 이름으로 모두
  거부한다(44회, 산출물 없음). 11종: 알 수 없는 행동, 알 수 없는 멤버, 그
  행동을 낼 수 없는 클래스, 같은 행동 행 두 번, 같은 클래스 행 두 번, 답을
  바꾸지 않은 row divergence, 클래스 행의 잘못된 divergence 형식, FRESH가 아닌
  컴파일러 Clone, HAND_OFF가 아닌 handed-off 행, 개수 불일치, 객체 누락.
- G2 측정: `tests/self_hosted/parity/ownership/call_handoff_differential_measure.sh`
  (+ `.py`). 수동 측정 행으로 `tests/repo/manual_gate_inventory.tsv`에 등록했다.
- `direct_mir_role_override_program_identity_owner.pgy`: 루틴 키 개수 핀 23/22를
  새 키 하나만큼 24/23으로 올렸다. route 소유자의 수정이고 거부 조건은 그대로다.
- `SemanticAstCollectionCallArgumentVerdict`를 직접 부르는 두 semantic 프로브
  (`collection_inout_effect_identity_probe.pgy`, `collection_storage_producer_probe.pgy`)는
  늘어난 인자로 빈 장부를 넘긴다. 확인하는 판정은 그대로다.
- `docs/semantics/sot_owner_spine_registry.md`: 새 읽는 쪽
  `call_handoff_row_fact_owner.pgy`에 derived 행을 더했다(primary term
  `MirCallHandoffRowsRefusal`, owner `semantic.hashmap_collection_ownership`,
  projection). `sot_authority_edge_smoke.sh`가 분류 없는 `*_fact_owner.pgy`를
  거부한다.
- MIR JSON 텍스트를 바이트로 비교하는 게이트 가운데 깨진 것은 없었다. native·
  self-host MIR 비교 게이트(`driver_rung2_mir_producer_parity_owner.sh`)는
  양쪽을 self-host 분석으로 다시 만든 canonical MIR끼리 비교한다.

## 수용 검사

WSL `Ubuntu-C-WSL`, 새 바이너리 `/root/ts2/bin`(`fe33c3235`), 기준
`/root/ts2-base/bin`(main `577f9b722`을 같은 트리에서 빌드), 둘 다
`/root/ts2`에서 실행했다.

| 입력 | leg | 파일 | 승인 기준/새 | 판정 차이 | 출력 바이트 차이 | 진단 차이 |
|---|---|---|---|---|---|---|
| lifetime 868 | native C | 868 | 574/574 | 0 | 0 | 0 |
| lifetime 868 | native LLVM | 868 | 573/573 | 0 | 0 | 0 |
| lifetime 868 | self-host C | 868 | 320/320 | 0 | 0 | 0 |
| lifetime 868 | self-host LLVM | 868 | 166/166 | 0 | 0 | 0 |
| `tests/cases` 전체 `.pgy` | native C | 2275 | 1767/1767 | 0 | 0 | 0 |

- 진단 텍스트는 출력 디렉터리와 바이너리 경로를 지운 뒤 비교했다.
- native oracle: main 소스(`/root/ts1`, `01f531e71`; `577f9b722`과는 `.def`
  주석과 Makefile만 다르다)의
  `driver_bootstrap_main.pgy --native-pipeline --emit-c`가 새 native와 기준
  native에서 바이트 단위로 같다(sha256 `e971cfe55dd4…`, 840553줄; 새 1027초·최대
  RSS 2.76 GB, 기준 1035초·2.75 GB; 호스트를 다른 트랙과 공유). 이 실행은
  `c3bae7a18`의 native로 했고, `fe33c3235`는 self-host 소스와 프로브만 바꿔
  native `bin/pgy`의 sha256(`dac93114410b…`)이 같다.
- G2가 만든 MIR 문서 2169개(native·self-host)를 새 driver의 `--mir-json`이
  읽어 행 거부 0개.
- `collection_ownership_fact_carrier_owner.sh` PASS(55 malformed-row 거부),
  `direct_mir_array_string_cleanup_fact_owner.sh` PASS(refused 14+4, admitted
  6x2 + native-mir 2x2), `native_unsafe_negative_refusal_smoke.sh` 39 / 17 / 8
  PASS, `call_handoff_row_reader_owner.sh` PASS(44), `call_handoff_rule_smoke.sh`
  PASS(800칸, 철자 39개).
- `codegen_bootstrap.sh`: `fixpoint ok: gen2 == gen3 (123126 lines)`, 1090초.
- push shard(`PGY_CI_SELF_HOST_MODE=prebuilt`, `PGY_ALLOW_MISSING_COQ=1`, PATH는
  `/root/nowsl-bin`만, `bin/pgy.exe` 없음, `TMPDIR=/root/ts2-tmp`): core
  `all 63 steps ok`(`fe33c3235`, 1305초), self-host `all 14 steps ok`
  (`48dfcc2dc`, 903초, `sot-authority-edge` 214 derived 행). 두 커밋의 차이는
  registry 한 행이고 core shard는 registry를 읽지 않는다.
- 같은 배포판의 다른 트리가 기본 `TMPDIR=/tmp`로 `ci-push-linux`를 동시에 돌리면
  `/tmp/pgy-ci-linux-build`를 함께 써서 object가 섞인다. 그렇게 오염된 첫 core
  실행(`pgy_call_handoff_ledger_incomplete` 링크 실패)은 버리고 TMPDIR을 나눠
  다시 돌렸다.
