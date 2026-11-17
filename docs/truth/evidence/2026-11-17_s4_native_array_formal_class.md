# 2026-11-17: native Array formal의 Keep/Return 분류 (S4)와 alias 행 해제 금지

Claude root lane의 S4 트랙 실행 기록이다. 대상은
[hand-off 규칙 연구](../../audits/unified_handoff_rule_study_2026-11-16.md) §3.5의
S4 조각과, [S3 기록](2026-11-17_s3_formal_alias_closure.md)의 "남은 것"에 적은 S6
선행 조건이다. 앞 기록은 [S0 불일치 원인](2026-11-16_handoff_disagreement_attribution.md),
[S2 hand-off 행](2026-11-16_s2_call_handoff_rows.md)이다.

## 범위 결정

- 연구 §3.5는 S4의 인수 조건으로 `exclusive_branch_rebind_positive`의 `seed` 일치를
  적었다. 그런데 S0 기록("연구 가정과 어긋나는 것" 2)이 이미 보였듯이, native가
  `seed`를 빌려주는 이유는 `RowsFill` 안의 `Rows(true, Clone(seed))`에 둔
  owned-element `Clone`이다. `Rows.names`는 원소를 소유해야 한다(`RowsAdd`가
  `ArrayPushOwnedString(Clone(name))`). 그 `Clone` 때문에 `RowsFill.seed`는 읽히기만
  한다. 11-16 원소 소유권 축 결정은 이 native 동작을 규칙으로 정했다. 그러므로
  맞추는 쪽은 self-host다(생성자 입력 owned-element `Clone`, S0 원인 2a/2b).
- 조정자 결정(2026-11-17): `seed` 일치는 S4에서 빼고 S5로 옮긴다. 선행 조건은
  self-host direct MIR이 컴파일러 `Clone`을 MIR 문장으로 싣고 내는 것이다(DM 레인).
  S4는 native class 행만 맞추고, self-host 생성자 `Clone`은 바꾸지 않는다.

## 1. S6 선행 조건: callee는 by-value formal의 alias 행을 해제하지 않는다

- 바뀐 곳: `direct_mir_scalar_program_array_string_storage_sharing_owner.pgy`의
  `DirectMirScalarProgramArrayStringByValueFormalStorage`(옛 이름
  `...ByValueFormalMember`)와 그 소비자 `...LocalCleanupRequired`(cleanup policy).
  전에는 by-value formal의 member path로 정의된 지역만 해제하지 않았다(11-15).
  이제 formal 자체, member path, 그리고 그런 지역에서 다시 정의된 지역(alias 사슬)도
  해제하지 않는다. 같은 지역에 다른 정의가 있어도 해제하지 않는다(누수 방향이
  fail-closed다).
- MIR 행에서 다시 계산하는 게 아니다. 같은 소유자가 이미 member path에 쓰던
  판정(MIR 정의 연산의 근원이 by-value `Parameter`인가)을 alias 사슬로 넓혔다.
  native MIR(`--mir-json`)과 self-host MIR 모두에 같은 답을 준다.
- guard를 끈 스크래치 빌드: `direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy`의
  공유 저장소 `Die`만 지웠고, 커밋하지 않았다. 트리는 `/root/ts2-tmp/guard-*`이고,
  self-host C와 LLVM leg를 `-O0` ASan과 glibc heap 검사 아래에서 돌렸다.

| 프로그램 | 변경 전 `f2a13fd61` + guard 끔 | 변경 후 `38e713ccb` + guard 끔 |
|---|---|---|
| `alias_read_only`(alias를 읽기만 하는 callee에 lend) | 거부: callee alias `seen`에 "requires an admitted ownership row" | 승인. 두 leg 모두 ASan·heap 깨끗, 출력은 native와 같음 |
| `grown_handoff` | 승인, 깨끗 | 승인, 깨끗(callee도 `grown`을 해제하지 않음, hand-off 누수) |
| `alias_read_after`(alias가 자란 뒤 formal을 다시 읽음, closure 밖) | 승인, ASan `attempting double-free` | 승인, ASan `attempting double-free`(그대로) |

- `alias_read_only`는 이 변경으로 guard 없이도 한 번만 해제된다. S3 기록에 적은 S6
  위험은 이것으로 닫혔다.
- `alias_read_after`는 이 변경과 무관한 S6 위험이다. callee가 alias로 저장소를
  키우는데(재할당) closure 밖이라 class가 `{}`(READ_ONLY)이고, 호출자는 lend해서
  재할당 전 버퍼를 다시 해제한다. 지금은 guard가 호출자를 거부한다(`$SHARED`).
  S6이 guard를 지우고 class 행 검증으로 바꾸면 이 경우를 놓친다. 그래서 S6에는
  "self-host formal effect가 막힘(effect 0)이면 READ_ONLY가 아니라 UNPROVED(이동)로
  본다" 또는 거부가 먼저 필요하다. Falsifier: `alias_read_after`를 S6 빌드에서 두 leg,
  ASan으로 돌리기.
- guard를 켠 정상 빌드에서는 `tests/self_hosted/parity/fixture/collection_field_lifetime/exclusive_nested_path_focus_positive.pgy`의
  self-host LLVM leg가 거부에서 승인으로 바뀌었다. `SeqAppend`의 `kinds`가 by-value
  formal `seq`의 member를 지역 사슬(`arena` → `topo` → `kinds`)로 따라간 alias여서,
  전에는 해제 대상이라 공유 저장소 guard에 걸렸다. 이제 해제하지 않는다. 네 leg 모두
  종료 코드 5, ASan·heap 깨끗.

## 2. S4: native Array formal의 class

`src/semantic/default_aggregate_move_rule.c` `formal_class_of_entry`:
- 이동 여부는 그대로 descriptor 비트(`array_storage_unproved`)와 release가 정한다.
- class에 summary의 Read를 더한다. descriptor가 움직였으면 Keep(호출·채널 탈출)과
  Return도 더한다.
- Keep이 없는 이동은 전처럼 GROW로 센다. 그런 class에만
  `NATIVE_ARRAY_FORMAL_DESCRIPTOR_ONLY` divergence를 단다
  (`collection_handoff_owner.c`, `.def` 주석 갱신, 렌더러 `--check` 통과).
- kind가 이동 계열(KEEP_ONLY, MOVING) 안에서만 바뀌고, 결정표에서 그 kind들의 행동이
  같다. 그래서 `semantic_default_aggregate_formal_moves`와 hand-off 선택이 그대로이고,
  바뀌는 것은 ledger의 class 행뿐이다. 아래 바이트 비교로 확인했다.

G2 class 행(lifetime; cases는 수만 같고 아래처럼 행이 바뀜):

| | 같은 멤버 | 같은 kind | 다른 kind | self-host만 |
|---|---|---|---|---|
| 기준 `f2a13fd61` | 3 | 26 | 26 | 2 |
| S4 | 8 | 21 | 26 | 2 |

- 다른 kind 26행은 하나도 줄지 않았다.
  - 16행: native는 `{GROW}` 9행, `{RETURN, GROW}` 7행이 되었고, self-host는
    `{KEEP}`이다. 이 formal들은 record에 담긴다. 소스를 확인한 예에서는 callee가
    그 record의 member를 키우거나(`exclusive_other_branch_positive`의 `Pick`,
    `exclusive_alias_rewrite_positive`의 `Build`: `Grow(facts)`) record가
    반환된다(`FactsMake`). 연구 §2.1로는 `{KEEP, GROW}` 또는
    `{KEEP, GROW, RETURN}`이 맞다.
    native summary는 생성자 결과를 formal의 path(alias)로 따라가고 탈출로 세지 않아서
    Keep이 생기지 않는다. self-host의 descriptor retention은 Keep만 세고, record를
    거친 성장과 반환을 보지 않는다. 그래서 kind가 맞으려면 양쪽 모두 바뀌어야 한다.
    native Keep은 array storage walk가 생성자 입력을 따로 알려야 하는 변경이다.
    그것만으로는 이 16행의 kind가 맞지 않아서 이번에 넣지 않았다.
  - 10행: native `{}` 대 self-host `{KEEP}`. native가 생성자 입력에 owned-element
    `Clone`을 둔 formal(`RowsFill.seed` 등)이다. S5 몫이다.
- 같은 멤버 3 → 8: Read를 더하자 native `{}` 대 self-host `{READ}` 7행이 맞았다.
  새로 2행(lifetime), 4행(cases)이 native `{READ}` 대 self-host `{}`로 갈렸다.
  self-host는 `ArrayLength`만 하는 formal을 effect 1(멤버 없음)로, unproved 원소
  사용을 `SELF_UNPROVED_EFFECT_NOT_MOVING`(멤버 없음)으로 둔다. 연구 §2.1의 Read는
  "descriptor, 길이, 원소를 읽음"이므로 native 쪽이 규칙대로다.
- hand-off 행과 행동 행은 기준과 같다(lifetime 121 / 38 / 29, cases 41 / 2 / 0;
  행동 같음 215 / 다름 97). S4는 행동을 바꾸지 않는다. S0의 원인별 native만·self-host만
  수도 그대로다.

## 3. seed는 S5, 선행 조건은 direct MIR의 컴파일러 Clone 운반

- `exclusive_branch_rebind_positive`의 `Main.seed`: native `LEND_FOR_CALL`(class `{}`),
  self-host `HAND_OFF`(class `{KEEP}`). 기준과 S4 모두 같다.
- 맞추려면 self-host가 생성자 입력에 owned-element `Clone`을 두어야 한다(S5, 원인
  2a 15행 + 2b 9행). 그 전에 self-host direct MIR이 컴파일러 `Clone`을 MIR 문장으로
  싣고 내야 한다. 지금은 싣지 못해서 그런 프로그램을 거부한다("direct MIR emission
  cannot carry the N compiler Clone landing(s)").
- 그 선행 조건의 측정값(G2, 기준 `f2a13fd61`, S2 기록과 같음): self-host가 고른
  컴파일러 `Clone`은 lifetime 102개(85 프로그램), cases 72개(62 프로그램), 합계
  174개다. MIR 문장 텍스트에 소스보다 많은 `Clone(`이 있는 self-host 프로그램은 0개다
  (S0 방법, `clone_text_s0_method.self_excess` = 0). native는 lifetime 246개, cases
  205개가 텍스트에 있다.

## 4. 수용 검사

WSL `Ubuntu-C-WSL`. S4 바이너리 `/root/ts2/bin`(`b9513961f`: alias 행 커밋
`38e713ccb` + native class 커밋)이고, 기준은 `/root/ts2-base/s4/bin`(`f2a13fd61`을
같은 트리에서 빌드)이다. 둘 다 `/root/ts2`에서 실행했고 `TMPDIR=/root/ts2-tmp`다.

| 입력 | leg | 파일 | 승인 기준/S4 | 판정 차이 | 출력 바이트 차이 | 진단 차이 |
|---|---|---|---|---|---|---|
| lifetime 868 | native C / LLVM | 868 × 2 | 574/574, 573/573 | 0 | 0 | 0 |
| lifetime 868 | self-host C / LLVM | 868 × 2 | 321/321, 175/176 | 1 | 0 | 0 |
| `tests/cases` 전체 | native C / LLVM | 2275 × 2 | 1767/1767, 1764/1764 | 0 | 0 | 0 |
| `tests/cases` 전체 | self-host C / LLVM | 2275 × 2 | 1365/1365, 618/618 | 0 | 0 | 0 |

- 판정 차이 1개는 위의 `exclusive_nested_path_focus_positive` sl이다(alias 행 커밋의
  의도 안).
- native C/LLVM 바이트는 868 × 2와 `tests/cases` × 2에서 그대로다. canonical 입력
  `driver_bootstrap_main.pgy --native-pipeline --emit-c`도 기준 native와 S4 native가
  바이트 단위로 같다(sha256 `2c5802fbb030…`, 845911줄. 각각 27분 45초, 최대 RSS
  2.77 GB, 동시 실행).
- 게이트(S4 바이너리): `direct_mir_array_string_cleanup_fact_owner.sh` PASS
  (`refused=18+4 admitted=8x2+native-mir-2x2`), `call_handoff_row_reader_owner.sh`
  PASS(44), `call_handoff_rule_smoke.sh` PASS(800칸, 렌더러 drift 없음),
  `collection_ownership_fact_carrier_owner.sh` PASS(55),
  `native_unsafe_negative_refusal_smoke.sh` 39 / 17 / 8 PASS.
- `codegen_bootstrap.sh`: `fixpoint ok: gen2 == gen3 (123440 lines)`, 1933초(다른
  빌드와 동시 실행).
- push shard(`PGY_CI_SELF_HOST_MODE=prebuilt`, `PGY_ALLOW_MISSING_COQ=1`, PATH는
  `/root/nowsl-bin`만, `bin/pgy.exe` 없음, `TMPDIR=/root/ts2-tmp`): core
  `all 64 steps ok`(1191초), self-host `all 14 steps ok`(1333초), 둘 다 `b9513961f`.
- 새 sanitizer 보고: 없음. 판정이 바뀐 프로그램(`exclusive_nested_path_focus_positive`)과
  §1의 guard 끈 확인 외에는 출력 바이트가 같다.
