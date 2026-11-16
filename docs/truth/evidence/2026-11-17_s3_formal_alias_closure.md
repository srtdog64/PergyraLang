# 2026-11-17: self-host formal의 alias closure (S3)

Claude root lane의 S3 트랙 실행 기록이다. 대상은
[hand-off 규칙 연구](../../audits/unified_handoff_rule_study_2026-11-16.md)
§2.1(저장소 closure)과 §3.5의 S3 조각이다. 따르는 결정은
[11-12 복사 결정](../decisions/2026-11-12_unwritten_field_copy_is_a_copy.md)(closure는
착지한 `Clone`에서 멈춘다)과
[원소 소유권 축](../decisions/2026-11-16_getter_loan_correction_and_element_ownership_axis.md)이다.
앞 조각의 기록은 [S0 불일치 원인](2026-11-16_handoff_disagreement_attribution.md)과
[S2 hand-off 행](2026-11-16_s2_call_handoff_rows.md)이다.

## 목표 카드

- 목표: `var grown = values; ArrayPush(grown, "b")`처럼 기본 모드
  `Array<String>` formal의 지역 alias를 거친 성장을 그 formal의 `Grow`로
  분류한다. 그러면 `GrowCount(names)`(`grown_handoff`)에서 두 front end가 같은
  hand-off를 낸다.
- 사실 소유자: 새 `src/self_hosted/semantic/ast_collection/ast_collection_formal_alias_closure_owner.pgy`.
  formal effect 소유자(`ast_collection_formal_use_owner.pgy`,
  `ast_collection_formal_statement_effect_build_owner.pgy`)가 그 답을 읽는다.
  native는 바꾸지 않았다(`bin/pgy` sha256 `8d91dbf81564…` 그대로).
- 마지막 소비자: 호출 hand-off 소유자(`ast_collection_call_handoff_owner.pgy`)가
  내는 binding 행, 그리고 direct MIR storage-sharing 소유자.
- 금지한 것: backend의 alias 예외, fixture 목록, 거부 약화, 새 annotation.
- 게이트: `direct_mir_array_string_cleanup_fact_owner.sh`(아래), G2 측정, 868 × 4
  leg와 `tests/cases` sweep.

## closure 규칙

alias는 그 formal의 저장소를 한 번에 한 이름만 가리킬 때만 closure에 든다.
그래야 다른 member의 성장이 바꾼 descriptor를 어느 member도 들고 있지 않다.

- formal의 함수에 있는 단일 binding `Array<String>` 지역이고, 초기화식이
  정확히 그 formal 또는 이미 든 alias다. 그 초기화식에 착지한 `Clone`이 없다.
- 선언이 호출마다 한 번 실행된다(루프, `defer`, `async`, `parallel`, 중첩
  callable 밖). 어떤 대입도 그 alias를 다시 묶거나 원소를 쓰지 않는다.
- alias가 나온 member는 그 초기화식 말고 다른 사용이 없고, 그 member에서 다른
  alias가 나오지 않는다.
- alias를 거친 성장은 String literal을 넣는 parser `ArrayPush`/`ArraySet`뿐이다.
  self-host는 formal의 원소 소유권을 계산하지 않는다(11-16 원소 소유권 축).
  그래서 pop, 원소 대입, 호출 형태의 변경(`ArrayPushOwnedString` 등), literal이
  아닌 원소는 closure를 끝낸다.
- 하나라도 어기면 그 formal은 alias를 하나도 갖지 않는다. alias 선언은 S3 전처럼
  formal의 보통 사용으로 남아 formal을 막는다.

closure에 든 alias의 사용은 formal의 사용으로 읽힌다. alias 선언의 초기화식은
formal을 막는 사용이 아니다.

## 호출 수용 쪽 변경

`ast_collection_call_argument_effect_admission_owner.pgy`의 shallow mutation
진입 검사는 inout/own formal의 write-back 증명이다. S3 전에는 기본 모드 formal이
effect 6(shallow mutation)을 가질 길이 없었다. 기본 모드 formal 직접 변경은
`value_param_collection_mutation`으로 거부된다. 그래서 기본 모드 callee에는 이
검사를 적용하지 않는다. 이동은 호출 hand-off 소유자가 정하고, callee가 키울 수
있는 저장소에 live 행을 넘기는 호출은 direct MIR storage-sharing 소유자가 계속
거부한다.

이 변경 전 첫 빌드에서 확인한 두 회귀가 이 조건을 정했다.
- `tests/cases/backend_compare/empty_literal_typed_site/main.pgy`(sc 승인 → 거부):
  `FirstName([])`의 새 빈 literal이 `unproved_formal_shallow_mutation_entry`로
  거부됐다.
- `tests/cases/function_param_flow_summary/default_owned_string_push_borrowed_actual_invalid.pgy`
  (sc/sl 거부 → 승인): borrowed literal 배열에 alias로 `Concat` 결과를 넣는 음성
  fixture다. native는 의미 분석에서 거부한다("A moving default Array<String>
  actual requires current owned elements"). self-host 의미 분석은 S3 전에도 이
  프로그램을 받아들였고, backend가 공유 저장소로 거부했을 뿐이다. hand-off가
  backend 거부를 없애자 그 구멍이 드러났다. literal 성장만 closure에 넣어 S3 전
  상태로 되돌렸다.

## 게이트

`tests/self_hosted/parity/ownership/direct_mir_array_string_cleanup_fact_owner.sh`:
- `grown_handoff`: `refuse ... $SHARED`(C, LLVM)에서 두 leg 실행으로 바꿨다.
  출력 `5`, 종료 0, glibc heap 검사 아래에서. 이유: `values`의 closure에 `grown`이
  들어 `GrowCount.values`가 `GROW`가 되고, `Main.names`가 native처럼 handed-off가
  된다. 호출자는 성장이 재할당한 저장소를 해제하지 않는다.
- 새 실행 이웃 `alias_other_formal`: closure는 formal마다다. `second`의 alias만
  자라므로 `others`는 handed-off, 읽기만 하는 `first`의 `names`는 호출자가 한 번
  해제한다. 출력 `6`, `a`.
- 거부로 남는 이웃(C, LLVM 모두):
  - `alias_read_after`: alias가 자란 뒤 formal을 다시 읽는다. formal 사용이 둘이라
    closure가 없다. native는 alias 선언에 `Clone`을 착지시킨다(11-12). self-host는
    S3 전과 같이 `$SHARED`로 거부한다.
  - `alias_loop_rebind`: 루프마다 alias를 다시 묶는다. 의미 분석이
    `boundary: unproved_exclusive_member_mutation`으로 거부한다(S3 전과 같다).
  - `alias_owned_element`: borrowed literal 배열의 alias에 `Concat` 결과를 넣는다.
    closure에 들지 않아 `$SHARED`로 거부된다. native는 의미 분석에서 거부한다.
  - `grown_loop`: S3 전과 같이 C는 `$SHARED`. self-host는 반복 호출 인자에
    `Clone`을 고른다(`origin=FRESH`, 행동 `MOVED`).
- 끝 줄 개수: `refused=18+4 admitted=8x2+native-mir-2x2`(전 `14+4`, `6x2`).

`tests/self_hosted/parity/ownership/collection_formal_key_consumption_owner.sh`
(self-host shard): `borrow_formal_identity_input`은 네 모드의 formal을 각각 지역에
묶는다. 기본 모드 alias는 이제 그 formal의 closure에 들어 `Modes:0:0`이 `-1`(막힘)에서
`3`(Read)이 된다. inout, own, ref alias는 여전히 formal을 막고(`-1`), 본문은 ref
alias 때문에 그대로 `borrow_boundary_escape`로 거부된다. 기대값을 그 소유자에서
고쳤다. 이 프로브의 관찰 프로그램
`tests/self_hosted/fixtures/collection_formal_key_consumption_probe.pgy`는 statement
effect 소유자를 직접 부르므로 늘어난 인자에 빈 closure를 넘긴다.

## G2

`call_handoff_differential_measure.sh`, 868 lifetime fixture + `tests/cases/function_param_flow_summary`.
기준 바이너리(`a1cb26a93`, `/root/ts2-base/s3/bin`)와 S3 바이너리의
`summary.json`과 `differences.tsv`가 바이트 단위로 같다.

| 입력 | 둘 다 승인 | 일치 | native만 | self-host만 |
|---|---|---|---|---|
| lifetime | 362 | 121 | 38 | 29 |
| cases | 427 | 41 | 2 | 0 |

- self-host만 29행은 줄지 않았다. S0의 29 = 2a 15 + 3(생성자) 3 + 2b 9 + 3(호출)
  2이고, alias를 거친 성장은 이 중 0행이다. S0 기록도 "alias를 거친 성장은 868개와
  cases에서 행이 0"이라고 적었다. 그래서 이 입력에서 줄 몫이 없다.
- alias 성장이 나오는 입력에서 측정했다. S3 프로브 프로그램 22개(WSL
  `/root/ts2/.tmp/s3probe*`의 임시 입력, 같은 측정기)에서. 게이트 인라인
  프로그램 여섯 개(`grown_handoff`, `grown_loop`, `alias_read_after`,
  `alias_loop_rebind`, `alias_other_formal`, `alias_owned_element`)가 같은 소스로
  들어 있다:

| | 둘 다 승인 | hand-off 일치 | native만 | 행동 행 같음 / 다름 |
|---|---|---|---|---|
| 기준 | 14 | 0 | 10 | 2 / 10 |
| S3 | 14 | 9 | 1 | 11 / 1 |

  일치로 바뀐 9행: `grown_handoff`, `alias_chain`, `alias_forward_grow`(2행),
  `alias_grow_read`, `alias_other_formal`, 기본 모드 formal을 그대로 넘기는
  `v_formal_forward`, literal을 넣는 `v_shallow`, owned 원소 실인자에 literal을 넣는
  `w4_alias_literal_owned_actual`. 남은 1행은
  `w2_alias_concat_read_owned_actual`이다. owned 원소 배열의 alias에 `Concat`
  결과를 넣는다. native는 hand-off, self-host는 closure 밖이라 lend다. 원소
  소유권 축(S5 이후)의 몫이다.

## 수용 검사

WSL `Ubuntu-C-WSL`, S3 바이너리 `/root/ts2/bin`(`348833c2f`), 기준
`/root/ts2-base/s3/bin`(`a1cb26a93`를 같은 트리에서 빌드), 둘 다 `/root/ts2`에서
실행했다. CI와 sweep의 임시 디렉터리는 `TMPDIR=/root/ts2-tmp`다.

| 입력 | leg | 파일 | 승인 기준/S3 | 판정 차이 | 출력 바이트 차이 | 진단 차이 |
|---|---|---|---|---|---|---|
| lifetime 868 | native C / LLVM | 868 × 2 | 574/574, 573/573 | 0 | 0 | 0 |
| lifetime 868 | self-host C / LLVM | 868 × 2 | 321/321, 175/175 | 0 | 0 | 0 |
| `tests/cases` 전체 | native C | 2275 | 1767/1767 | 0 | 0 | 0 |
| `tests/cases` 전체 | self-host C / LLVM | 2275 × 2 | 1365/1365, 618/618 | 0 | 0 | 0 |

- `tests/cases`의 self-host leg는 brief의 수용 목록 밖이지만 S3가 바꾸는 쪽이라
  돌렸다. 위의 두 회귀를 여기서 찾았다.
- `grown_handoff`: native·self-host 모두 `GrowCount.values` = `GROW`,
  `Main.names` = `HAND_OFF`/handed-off. self-host C와 LLVM이 승인하고, `-O0` ASan
  (C는 gcc, LLVM은 `sanitize_address`를 붙인 IR을 clang으로)과 glibc
  `MALLOC_CHECK_=3` + `libc_malloc_debug` 아래에서 깨끗하다. 출력은 native와 같다.
- 새로 승인되는 프로브(`alias_forward_grow`, `alias_other_formal`,
  `v_formal_forward`, `v_shallow`, `w4_alias_literal_owned_actual`)도 네 leg 모두
  같은 출력, ASan·heap 검사 깨끗.
- native 바이너리는 바뀌지 않았다(sha256 `8d91dbf81564…`, 기준과 같음).
- 게이트(S3 바이너리): `direct_mir_array_string_cleanup_fact_owner.sh` PASS
  (`refused=18+4 admitted=8x2+native-mir-2x2`), `call_handoff_row_reader_owner.sh`
  PASS(44), `call_handoff_rule_smoke.sh` PASS, `collection_ownership_fact_carrier_owner.sh`
  PASS(55), `native_unsafe_negative_refusal_smoke.sh` 39 / 17 / 8 PASS.
- `codegen_bootstrap.sh`: `fixpoint ok: gen2 == gen3 (123440 lines)`, 949초.
- push shard(`PGY_CI_SELF_HOST_MODE=prebuilt`, `PGY_ALLOW_MISSING_COQ=1`, PATH는
  `/root/nowsl-bin`만, `bin/pgy.exe` 없음, `TMPDIR=/root/ts2-tmp`): core
  `all 64 steps ok`(`348833c2f`, 1036초). self-host는 첫 실행에서 step 4
  (`collection_formal_key_consumption_owner.sh`)가 위의 `Modes:0:0` 차이로 실패했고,
  기대값을 고친 뒤 다시 돌렸다: `all 14 steps ok`(`f81d4c800`, 850초, 바이너리
  sha256 `170eece5eb56…` 그대로). 두 커밋의 차이는 self-host shard만 돌리는 그
  게이트 스크립트다.

## 남은 것

- callee가 alias 행을 해제한다. `grown_handoff`의 self-host C에서 `GrowCount`는
  끝에서 `pgy_as_drop_storage(&grown)`을 부른다(행 상태 Unknown → 저장소만
  해제). `Main`은 `names`를 해제하지 않으므로 해제는 한 번이고 누수도 없다. native는
  11-15 결정대로 아무도 해제하지 않는다. 출력은 같다. 위험은 lend 쪽이다.
  `alias_read_only`(alias를 읽기만 하는 callee에 `names`를 빌려줌)에서는 호출자도
  해제하고 callee도 alias를 해제한다. 지금은 direct MIR storage-sharing 소유자가
  호출자를 거부해서(`$SHARED`, callee formal이 다른 binding에 넘겨짐) 안전하다.
  S6이 그 backend 분기를 지우면 이 프로그램은 이중 해제가 된다. 그래서 S6에는
  "기본 모드 formal의 alias 행은 해제하지 않는다"가 먼저 필요하다.
  Falsifier: `alias_read_only`를 S6 빌드에서 두 leg, glibc heap 검사로 돌리기.
- alias를 거친 보관: alias를 반환하거나 생성자에 넣는 것은 formal의 보통 사용처럼
  formal을 막는다(S3 전과 같다). descriptor retention(`Keep`)은 alias를 따라가지
  않는다. corpus에 이 경우의 행은 없다.
- 착지한 `Clone`: closure는 formal effect 계산 때 아는 착지(owned String store)만
  본다. exclusive 복사 착지는 그 뒤 ownership 판정에서 정해진다. closure에 든 alias의
  출발 member는 사용이 하나뿐이라 10-22/11-12 복사 규칙이 거기에 착지할 이유가
  없다. 프로브에서도 착지하지 않았다. 다만 판정 시점의 교차 검사는 없다. 착지하면
  closure가 넓게 잡혀 hand-off(누수 방향)가 되고, 이중 해제 방향은 아니다.
- 원소 소유권: alias에 literal이 아닌 원소를 넣는 성장은 closure 밖이다.
  `w2_alias_concat_read_owned_actual`이 G2에 남는다.
- member path alias: self-host formal effect 목록은 String 시퀀스와
  `HashMap<String, Int>` formal만 담는다. 레코드 formal과 그 member path는 목록에
  없어서 S3는 `Array<String>` formal 전체의 alias만 다룬다.
- `grown_loop`: self-host 의미 분석은 반복 호출 인자에 `Clone`을 고르지만, direct MIR
  emitter는 컴파일러 `Clone`을 싣지 못해 거부한다(S5/S6).
