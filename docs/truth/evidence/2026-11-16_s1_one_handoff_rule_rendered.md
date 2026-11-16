# 2026-11-16: 하나의 hand-off 규칙을 C와 Pergyra로 렌더링 (S1)

Claude root lane의 S1 트랙이 남긴 실행 기록이다. 대상은
[hand-off 규칙 연구](../../audits/unified_handoff_rule_study_2026-11-16.md)의
S1 조각(§3.5)이고, 따르는 결정은
[하나의 hand-off 규칙](../decisions/2026-11-16_one_handoff_rule_for_both_front_ends.md)과
[원소 소유권 축](../decisions/2026-11-16_getter_loan_correction_and_element_ownership_axis.md)이다.
S1은 판정을 바꾸지 않는다.

## 바뀐 것

- 규칙 한 곳: `src/semantic/call_handoff_rule.def`. 효과 멤버 6개와 join(kind
  5개), 축 4개(origin 4, later 2, context 2, elements 2), action 10개, 규칙
  행 11개, divergence 13개(행 3개, 투영 1개, 입력 9개), native summary 비트
  투영 6개, self-host effect 코드 투영 7개.
- 렌더러: `scripts/render_call_handoff_rule.py`가
  `src/semantic/call_handoff_rule.h`와
  `src/self_hosted/semantic/call_handoff_rule_projection_owner.pgy`를 만든다.
  렌더러는 표가 모든 칸(5×4×2×2×2 = 160)에 규칙 action을 하나씩 주는지,
  가려진 행, 아무 칸도 바꾸지 않는 divergence 행, LOCAL·ONCE가 아닌
  HAND_OFF 행을 거부한다.
- 게이트: `make call-handoff-rule-test-smoke`
  (`tests/registry/call_handoff_rule_smoke.sh`), ci-push-linux core shard에
  넣었다.
- 호출자 (기존 분류로 생성된 action을 부른다):
  - native: `default_aggregate_move_rule.c`(moves, lends, borrow_handoff),
    `default_aggregate_call_move_owner.c`(다시 쓸 때의 copy/refuse),
    `collection_handoff_owner.c`(호출·생성자 hand-off),
    `collection_owned_element_member_requirement_owner.c`(원소 소유권 축 Clone);
  - self-host: `ast_collection_call_handoff_owner.pgy`,
    `ast_collection_constructor_handoff_owner.pgy`,
    `ast_collection_exclusive_argument_boundary_owner.pgy`(이동 인자 Clone).

## 측정

- 트리와 바이너리: WSL `Ubuntu-C-WSL`의 `/root/ts1`(브랜치
  `worktree-agent-abb65b8860b08f67b`, 측정 때 `01f531e71`; 그 뒤 커밋은
  주석만 바꿨고 렌더링 출력은 같다). 기준은 `/root/mg-main/bin`(main
  `7cf0c6be1`, 코드는 `93c233de9`와 같다), 그 트리 안에서 실행했다.
- 판정과 출력 바이트(exit code, 출력 파일 sha256). 진단 텍스트는 트리
  경로와 출력 디렉터리 이름을 지운 뒤 모두 같았다.

| 입력 | leg | 파일 | 승인 기준/새 | 판정 차이 | 출력 바이트 차이 |
|---|---|---|---|---|---|
| lifetime 868 | native C | 868 | 574/574 | 0 | 0 |
| lifetime 868 | native LLVM | 868 | 573/573 | 0 | 0 |
| lifetime 868 | self-host C | 868 | 320/320 | 0 | 0 |
| lifetime 868 | self-host LLVM | 868 | 166/166 | 0 | 0 |
| `tests/cases` 전체 `.pgy` | native C | 2275 | 1767/1767 | 0 | 0 |

- native oracle: main `93c233de9` 소스의
  `driver_bootstrap_main.pgy --native-pipeline --emit-c`를 새 native로
  컴파일한 C가 main 바이너리의 C와 바이트 단위로 같다(sha256
  `2e617af1c9d6…`, 새 14분 4초·최대 RSS 2.76 GB, 기준 17분 15초·2.75 GB;
  호스트를 다른 트랙과 공유).
- `codegen_bootstrap.sh`: `fixpoint ok: gen2 == gen3 (122065 lines)`.
- `collection_ownership_fact_carrier_owner.sh` PASS(55 malformed-row 거부),
  `direct_mir_array_string_cleanup_fact_owner.sh` PASS(refused 14+4,
  admitted 6x2 + native-mir 2x2), `native_unsafe_negative_refusal_smoke.sh`
  39 / 17 / 8 PASS.
- `PGY_CI_SELF_HOST_MODE=prebuilt PGY_CI_PUSH_LINUX_SHARD=core
  PGY_ALLOW_MISSING_COQ=1 make ci-push-linux`(PATH는 `/root/nowsl-bin`만,
  `bin/pgy.exe` 없음): `all 63 steps ok`, 1089초. 새 step 42
  `call-handoff-rule-test-smoke`가 그 안에서 돌았다. 바뀐 C 파일에서 나온
  컴파일 경고는 0개. 같은 조건의 self-host shard: `all 14 steps ok`,
  1145초(`self-host-component-contract`의 owner line cap과 generator drift
  포함).
- `make call-handoff-rule-test-smoke`: 렌더링 drift 없음, 손으로 고친
  `.def`·`.h`·`.pgy` 각각 `--check` 실패(exit 1), 잘못된 규칙 9종 거부,
  C 헤더와 Pergyra owner가 800칸과 kind 64개에서 같은 답.
