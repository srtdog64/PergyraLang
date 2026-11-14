# 소유권 검증 보완: 메인 통합 인계

기준: `main @ cefd474e0`. 이 문서는 관측과 통합 경계이며 의미 owner가 아니다.
사용자 결정: 이 레인은 작은 커밋·검증을 맡고, 전체 push와 원격 CI는 메인 작업자가 맡는다.

## 이미 착지한 범위

- `dca7fd0ce`: 오래된 aggregate 계약 안내와 키워드 검증 주장을 바로잡음.
  [`let`/`var` 인계](2026-10-23_let_var_semantics_main_handoff.md)는 문법 변경 제안이
  아니라 현재 구현·Swift/Rust 비교·필요한 음성 검사를 구분한다. 새 키워드나
  대량 소스 치환을 적용하지 않았다.
- `8fb4b7998`: cleanup proof spine을 실제 Rocq 정리 타입에 연결하고 누락·약화 반례를 추가.
- `cefd474e0`: 인증된 임시 String 복사본의 native C/LLVM 실제 해제 연결.
  [재현·검증·범위 기록](../truth/evidence/2026-10-23_string_copy_physical_release.md).

수동 drop이나 새 소유권 주석을 사용자의 기본 작업으로 추가하지 않았다.
일반 값의 자동 정리 전체, self-host 대체, canonical admission, 원격 CI 완료를
이 세 커밋에서 주장하지 않는다.

## nested Clone oracle: 단순 기대값 보정은 착지하지 않음

- 목적: 현재 값 의미론을 검사하면서 명시적 깊은 Clone의 독립성 검사를 유지한다.
- 우선순위: 계약상 독립 출력, 실제 원본 변경의 관측, 복사본 보존, 변경량.
- 의미 owner: [local copy 결정](../truth/decisions/2026-10-21_local_copies_refuse_the_other_side_after_a_change.md)
  및 `docs/semantics/27_ownership_clean.md` §2.1.
- 마지막 소비자: `tests/runtime/nested_array_copy_smoke.sh`의 C/LLVM 독립 출력 oracle.
- 변경 범위: fixture에 변경된 원본도 출력하고, outer를 별도로 다시 변경한다.
  oracle은 원본·outer의 변경 전후·명시적 Clone을 각각 검증한다.
  allocator/ownership 코드와 기존 OOM·ABI 음성 검사는 그대로 둔다.
- 금지: 실제 출력을 새 정답으로 복사하기, 실패 비교 생략, 원본 변경을 제거하기.
- 반례: 원본 변경이 outer에 전파되거나 명시적 Clone에 전파되면 실패해야 한다.
  변경이 아예 실행되지 않아도 원본 출력에서 실패해야 한다.

기대값만 현재 값 의미론에 맞추면 기존 게이트는 통과하지만, 자동 복사가 먼저
outer를 분리하므로 그 테스트가 더는 명시적 Clone의 독립성을 판별하지 못한다.
따라서 그 수정은 착지하지 않았다. 다음 강화 입력으로 확인했다.

```pergyra
let row: Array<Int> = [7, 8];
let source: Array<Array<Int>> = [row];
let copied: Array<Array<Int>> = Clone(source);
source[0][0] = 55;
Log(copied[0][0]);
```

현재 native 경로는 마지막 사용을 공유 저장소 변경 뒤의 사용으로 거절한다.
runtime의 깊은 복사 커널 검사 통과와 이 source-level 독립성 admission은 다르다.
`nested_copy_post_mutation.pgy`를 로컬 증거 폴더에 보존했다. 이 사실을 숨기기 위해
본문을 생략하거나 거절을 성공으로 바꾸지 않았고 기존 게이트도 그대로 남겼다.
메인은 checked Clone result relation과 실제 깊은 복사 지원의 경계를 맞춘 뒤
위의 강화된 출력 oracle을 적용해야 한다.

## 새로 확인한 실제 반례: 형제 필드의 공유 저장소

아래 입력은 수정 전 native snapshot `dca7fd0ce`와 수정 후 후보 모두에서
C·LLVM 각각 exit 0, 출력 `99\n99\n`다. 값 독립성 계약의 기대는 `99\n1\n`이다.
즉 기존 aggregate 게이트의 실패를 전부 오래된 기대값 탓으로 치부할 수 없다.
이 반례는 String 임시값 해제 변경 전에도 존재한다.

```pergyra
struct Pair
{
    first: Array<Int>;
    second: Array<Int>;
}

func Main() -> Void
{
    let rows: Array<Int> = [1];
    let pair: Pair = Pair(rows, rows);
    ArraySet(pair.first, 0, 99);
    Log(pair.first[0]);
    Log(pair.second[0]);
}
```

생성 C는 동일한 descriptor를 두 필드에 넣고, 중간 Clone 없이 첫 필드를 수정한다.
이 입력은 재할당·해제가 없는 정수 원소 갱신으로 값 오염을 관측한 것이다.
여기서 UAF나 double-free까지 실행했다고 주장하지 않는다.
재현 소스·생성 C·네 실행 출력은
`.tmp/ownership_cleanup_followup_20261023/pair_field_*`에 보존했다.

다음 조사 경계는 기존 `default_aggregate_copy_owner.c`의 `move_walk_copy_record`
및 `move_walk_copy_changed`, 그 입력인 checked result relation,
`default_aggregate_move_owner.c`의 나중 사용 판정과 `loan_copy_landing_owner.c`다.
이는 source에서 확인한 관련 owner 연결이며, 어느 producer가 사실을 잃는지는
아직 추적하지 않았다. 새 별칭 분석기나 필드별 예외를 만들지 말고 기존 owner가
형제 필드의 공유 관계를 끝까지 전달하는지 먼저 확인해야 한다.

기존 `aggregate_storage_transfer_smoke.sh`의 12개 불일치는 그대로 남겼다.
새 자동 복사 때문에 합법적으로 바뀐 사례, 위와 같은 미검출 별칭, 진단 위치 변화는
각각 분리해야 한다. 게이트를 삭제하거나 모두 성공으로 바꾸지 않았다.

## 통합 경계

다른 작업자의 변경을 보존했다: `docs/language/01_intent_first_design.md`,
`src/self_hosted/dir/intent_fact_owner.pgy`, `src/self_hosted/mir/artifact_lower_owner.pgy`,
미추적 indexed-String ownership directive. 새 worktree/branch를 만들지 않았다.
이 레인은 해당 MIR/DRV-2 작업을 병렬로 재설계하지 않는다.

메인은 기존 활성 ownership rung에서 위 반례와 일반 값의 반환·필드·복사본 정리를
통합해야 한다. 이후 변경 없는 canonical 입력, bootstrap/gen2==gen3,
installed C/LLVM driver를 같은 공개 SHA에서 실제 실행하고 CI를 확인한다.
그 전에는 전체 소유권 자동화나 완전 self-hosted 완료가 아니다.
