# 자기 컴파일 메모리 해결 계획 (2026-11-17)

- 상태: PROPOSAL. 사용자가 직접 진행한다. 구현 후보이지 착지 승인이 아니다.
- 기준 리비전: `integrate/track-a` `757f02f76` (S4까지). 줄 번호와 수치는 각 근거
  문서의 기준 리비전 것이다. 손대기 전에 현재 소스로 다시 확인한다.
- 이 문서는 조정용이다. 결정은 [임시 값 해제 결정](../truth/decisions/2026-11-16_compiler_temporaries_are_released_at_last_use.md)이,
  설계는 [설계 조사](../audits/compiler_temporaries_release_design_2026-11-16.md)가,
  측정은 [메모리 조사](../audits/selfhost_source_c_memory_study_2026-11-16.md)와
  [DM0 센서스](../truth/evidence/2026-11-16_direct_mir_drv2_refusal_census.md)가 가진다.

## 목표 카드

- 목표: 설치된 self-host 드라이버가 DRV-2(`driver_rung2_main.pgy`)를 한
  프로세스에서 source→C로 16 GB 안에 컴파일한다. gen2 == gen3는 유지한다.
- 우선순위: 메모리 안전(이중 해제·해제 후 사용 0) > fail-closed > 출력 동일 >
  peak RSS > 시간.
- 사실 소유자: MIR 수준 value lifetime plan 하나(결정 1). emitter는 해제를
  결정하지 않고 retire 행만 출력한다.
- 금지: 작업·게이트 시간 한도나 메모리 한도 올리기, 입력 줄이기, 참조 계수나
  tracing GC, 프로그램 모양별 `direct_mir_*` 경로, 증명 못 한 값의 해제.

## 1. 문제 (측정값)

- 한 프로세스 DRV-2 source→C: 212 s에 16.6 GB, 증가 중. 외삽 약 23 GB.
- 증가 법칙: MIR JSON 바이트의 55~63배, 루틴당 2.2~2.6 MiB. 입력보다 빨리
  자라지는 않는다(선형). 상수가 크다.
- `semantic/main.pgy` 최고점의 살아 있는 힙 중 87~92%가 도달 불가능한 쓰레기다.
  - String 임시(Concat, Substring, ToString): 42%
  - Array·record 버퍼: 57%. 그중 BFS 배열 하나(`MirRoutineGraphDistancesAvoiding`)가 190 MiB.
- `mir_lower` 누계: 할당 9.75 GB, 해제 2.48 GB. 할당한 것의 75%가 끝까지 남는다.

## 2. 원인

1. **설치 드라이버를 만드는 emitter가 거의 해제하지 않는다.** `bin/pgy-self-driver`는
   self-host AST 소비 C emitter(`src/self_hosted/codegen/emission/`)가 만든다.
   이 emitter의 scope cleanup은 zone lock, defer, focus 복구뿐이다. 드라이버 C에는
   `pgy_concat` 17,358곳에 drop 약 300곳이 있다. 그래서 RSS가 살아 있는 사실이
   아니라 누적 할당량을 따라간다.
2. **source→C 한 프로세스 경로만 MIR JSON을 String 조각으로 통째로 만든다.**
   `CompileSourceToCVerified` → `DriverRung2MirProjectionJson` → `SelfMirProgramJson`.
   단계마다 `Array<String>` 조각을 위 단계로 복사하고 아래 조각은 버리지 않는다.
   결과 JSON 1바이트당 약 17.8바이트가 남는다(피크의 34%). `--emit-mir-json-verified`는
   같은 사실을 파일 writer로 내보내 25 MiB만 남긴다.
3. **소비자의 phi predecessor binding.** `MirPhiPredecessorBindingFactFromOwners`가
   안쪽 루프마다 이름 비교 문자열과 BFS 배열을 새로 만든다(semantic 소비자의 25%).
   gen2/gen3 `--mir-json` 경로에도 들어 있다.

## 3. 해결책

두 갈래다. 갈래 A는 결정과 무관하게 바로 줄이는 것이고, 갈래 B가 뿌리다.
A만으로는 16 GB에 닿지 않는다(B 없이 약 16 GB, 경계선).

### 갈래 A: 국소 감소 (드라이버 소스 수정, 소유자 한두 곳)

| 항목 | 바꾸는 곳 | 예상 효과 | 게이트 |
|---|---|---|---|
| A1. JSON String 투영 제거 | source→C 경로가 `SelfMirProgramJson` 조각 대신 artifact writer(`SelfMirProgramJsonWriteArtifactVerified`)를 쓰게 한다. 방법 1: 출력 옆 임시 파일에 쓰고 다시 읽는다(`io_write` 권한이 있는 `DriverSourceCExecution.Compile`만). 방법 2: writer의 row renderer가 TextBuilder 싱크를 받게 하고 옛 조각 구현을 지운다(writer 파일 5개) | MIR 바이트의 약 18배 감소. semantic `c` 2,392 → 약 1,650 MiB, mir_lower 약 9 → 6.2 GB, DRV-2 약 23 → 16 GB | source 경로 C와 MIR 경로 C가 `cmp`로 같다. `tools/mir_json_instruction_writer_probe` 통과 |
| A2. phi binding | `MirSsaNameMatches`를 할당 없는 문자 비교로 바꾼다. 한 phi를 판정하는 호출 안에서만 `(dominator, predecessor)` BFS 결과를 재사용한다 | semantic 소비자 1,019 → 약 760 MiB | canonical MIR을 `--mir-json`으로 내린 C가 수정 전후 같다 |
| A3. 게이트만 두 프로세스로 | weekly driver shard 첫 단계를 `--emit-mir-json-verified`와 `--mir-json`으로 나눈다 | 약 10 GB(16 GB 러너 안, 여유 6 GB 미만) | 컴파일러는 그대로이고 스크립트만 바뀐다. `--emit-c-artifact-verified` 자체는 여전히 죽는다 |

- `JsonEmitCollection`에서 `items`를 바로 버리는 수정은 하지 않는다. 호출자가 빌린
  문자열과 리터럴을 섞어 넣는다(`json_emit.pgy` 주석). 잘못된 해제가 된다.

### 갈래 B: 마지막 사용에서 해제 (결정 2026-11-16, V1)

MIR에 두 가지를 싣는다.
- 루틴 단위 `owned_value_facts`: `{value, origin(fresh|call-result|moved-in|lent|static), recipe, disposition(retired|handed-off|returned|lent|unproved)}`.
  지금의 `collection_ownership_facts`를 String과 record로 넓힌 것이다.
- `value-retire` instruction: 지금의 `local-collection-retire`를 일반화한다.
  `{value, recipe, cause(last-use|redefinition|exit)}`.

검증(MIR 소비 직전, 두 front end 공통, Swift OSSA 방식):
- 소유 값마다 모든 경로에서 수명 종료(retire, 넘김, return)가 정확히 하나다.
- 종료 뒤 사용이 없다.
- `lent` 값은 종료가 없고 owner 종료보다 먼저 끝난다.
- `unproved`는 retire가 없다(누수는 안전하다).
- 없거나 위조된 행은 거부하고, 진단에 값·경로·규칙을 적는다.

조각(예상치는 추정이지 측정이 아니다):

| 조각 | 해제하는 것 | DRV-2 한 프로세스 |
|---|---|---|
| S0 | 센서스 (완료: DM0) | 약 23 GB |
| S1 | 신선한 String 임시, `x = Concat(x, …)`의 이전 값, 마지막 사용 뒤 String 지역 | 약 15~17 GB |
| S2 | 루틴 안에서 죽는 Array<스칼라>·Array<String>, 반복마다 태어나는 배열 | 약 9~11 GB (목표 도달) |
| S3 | record recipe 중첩, Array<record>, 사용자 함수의 fresh 결과 행, 단계가 끝난 사실 묶음, handed-off 저장소의 callee 해제 | 약 3~5 GB |
| S4 | 제자리 재사용(누적 Concat 3,906줄), native region과 `private_uses` 삭제, 수동 drop 삭제 | 할당 횟수와 시간 |

결정 4: 한 바인딩이 한 경로에서는 신선한 값, 다른 경로에서는 빌린 값을 받으면
빌린 정의 자리에 컴파일러 복사를 넣어 항상 소유로 만든다. 실행 중 소유 플래그는 두지 않는다.

## 4. 갈래 B의 선행 조건: direct MIR이 컴파일러를 받아야 한다

결정 3에 따라 retire를 처음 소비하는 것은 direct MIR emitter다. 그런데 설치
드라이버는 아직 AST emitter가 만든다. 그래서 B의 효과는 direct MIR이 DRV-2를
받아 드라이버를 그 경로로 만들 때 나타난다.

현재 거리(main `ea0783975`, DM1·DM2 반영):
- 거부 루틴: semantic 31, mir_lower 52, codegen 59.
- DRV-2는 route 단계 `role-declaration`에서 프로그램 전체가 거부된다. role 수신자
  `void *self` ABI 행과 override 식별이 `direct_mir_role_override_*`에만 있다.
  게이트 `role_override_mir_replacement.sh`가 override 경로 고유 철자(C
  `OverrideTarget_Name(&target)`, LLVM `%pgy.subject`)를 핀으로 고정한다. 일반
  emitter는 지역을 `pgy_local_N`으로 부르므로, 경로를 지우려면 그 핀을 이름과
  무관한 행동 검사로 바꿔야 한다.
- 그다음 큰 덩어리: 컬렉션 소유권 전이 행(DRV-2 45루틴, 앞 58% 기준). 이것이
  `owned_value_facts`·`value-retire`로 대체될 사실이다. 따로 고치지 말고 S1·S2와
  함께 다룬다. receipt-plan reader(전이 소유자 192행) 하나만 먼저 고치면
  15~18루틴이 풀린다.
- 나머지 coverage: `ArrayStringPushOwned` member 수신자, inout 인덱스 대입 사실,
  `ArrayMap`/`ArrayFilter`(builtin 인자에 기대 타입 없음), `let` 값 범위 경계,
  `Slot<T>` 값, 진입 블록 밖 `MapNew`, `ArrayReverse` on `Array<String>`.

**닭과 달걀.** direct MIR emitter도 지금은 새는 드라이버 안에서 돈다. DM0에서 DRV-2
루틴 승인만 5,486/9,470 지점에서 9.45 GB였다(전체 외삽 약 16 GB). 그래서 direct
MIR로 만드는 첫 세대 드라이버는 여전히 크게 든다. 그 다음 세대부터 retire가 효과를
낸다. 첫 세대를 어떻게 넣을지 미리 정한다.
- 갈래 A1·A2를 먼저 넣어 첫 세대의 바닥을 낮춘다.
- 첫 세대만 두 프로세스(`--emit-mir-json-verified` → `--mir-json-backend=c`)로 만든다.
- 결정 3의 임시안: 거리가 길면 AST emitter가 String 문장 끝 retire만 먼저 소비한다.
  V1이 지울 경로에 대한 투자이므로 String 문장 끝으로만 한정한다.

## 5. 권고 순서와 확인 지점

1. A2(phi binding) → 측정: `semantic/main.pgy --mir-json` peak RSS 1,019 → 800 MiB 이하.
2. A1(JSON 투영 제거) → 측정: `mir_lower/main.pgy --emit-c-artifact-verified`가 8 GiB
   안에서 완주하고, C가 `--mir-json` 경로 결과와 같다.
3. role 수신자 ABI를 일반 emitter로 옮기고 override 경로를 지운다 → DRV-2 센서스가
   route 단계를 넘는다. 다음 멈춤 지점을 기록한다.
4. 갈래 B S1(String) → 검증자, 위조 retire 증인 4개, -O0 ASan. 측정: semantic live
   768 → 490~560 MiB.
5. S2(배열). 컬렉션 전이 행을 여기서 대체한다 → 측정: `mir_lower` `c`가 약 4 GiB 이하.
   DRV-2 direct MIR 승인 완주.
6. 드라이버를 direct MIR로 만든다(첫 세대는 4절의 방법). gen2 == gen3.
7. S3, S4. AST emitter 삭제(V1).

조각마다 같은 입력, 같은 경로, 같은 실행 파일로 before/after를 기록한다.

## 6. 게이트

- ASan: 868 lifetime fixture와 컴파일러 구성요소, C(`-fsanitize=address`)와 LLVM IR
  **`-O0`**. `-O1`은 두 번째 free를 지워서 이중 해제를 놓친다(IR 증인으로 확인됨).
- 위조 retire 증인 4개: retire 복제, `handed-off` 값에 retire 추가, `lent` 값에
  retire 추가, owner retire를 lent 사용 앞으로 이동. 검증자가 모두 거부해야 한다.
  검증을 끈 scratch 빌드에서는 ASan이 잡아야 한다.
- legacy 기본값 삭제 뒤: 전이 행 없는 `Array<String>`은 drop 없이 통과하고
  census에 `unproved`로 잡힌다.
- 출력 동일: gen2 == gen3, 구성요소 출력 `cmp`.
- peak RSS 상한을 CI 실행 게이트로 둔다(결정 7). 고정 중간 크기 입력, 조각마다 낮춘다.
  예: `semantic/main.pgy --mir-json`.

## 7. 열린 질문과 함정

- **Clone 운반 기록이 서로 다르다.** DM0 센서스는 네 입력 모두에서 "MIR 운반자가
  없는 컴파일러 Clone 착지 0"이라고 적었다. S2·S4 근거는 "self-host가 고른 Clone 174개
  (lifetime 102 + cases 72)가 MIR 문장 텍스트에 0"이라고 적었다. 운반자가 문장
  텍스트가 아닌 별도 행일 수 있다. S5c와 결정 4의 컴파일러 복사가 이것에 기대므로
  먼저 하나의 측정으로 정리한다.
- **S6 전 이중 해제 위험.** backend 공유 guard를 끄면 `alias_read_after`가 이중
  해제한다(closure 밖 alias가 READ_ONLY class 아래에서 저장소를 키움). guard를
  지우기 전에 그 effect를 이동으로 보거나 거부해야 한다. 미착지 브랜치
  `s5a-one-later-use`에 시도가 있다(검증 안 됨).
- **handed-off 저장소.** 2026-11-15에는 "해제하지 않음"이었고, 임시 값 결정 5가 S3에서
  callee 해제로 다시 열었다. S4가 by-value formal의 alias 사슬은 해제하지 않게
  바꿔 두었다(`757f02f76`).
- **측정 환경.** WSL `Ubuntu-C-WSL`, 도구는 `/root/mem-tools/`(`measure.sh`,
  `prof2.sh`, `heapsamp.c`, `memD/reach.c`). 무거운 측정은 한 번에 하나만, RSS
  감시 루프로 PID를 지정해 죽인다. 바이너리는 자기 트리 루트를 품으므로 자기
  트리에서만 실행한다. 병렬로 `make ci-push-linux`를 돌리면 `TMPDIR`를 분리한다.
- 벽시계 시간은 다른 빌드와 겹치면 수 배 흔들린다. 시간은 참고로만 쓴다.

## 8. 참고 파일

- 설치 드라이버 빌드: `tests/self_hosted/parity/driver/self_host_compiler_build.sh`,
  `driver_bootstrap.sh`(CI 고정점).
- source→C 경로: `src/self_hosted/compiler/driver/driver_rung2_owner.pgy`
  (`CompileSourceToCVerified`), `src/self_hosted/mir/json_projection_owner.pgy`,
  `src/self_hosted/mir/program_json_artifact_writer_owner.pgy`, `src/self_hosted/lib/json_emit.pgy`.
- phi binding: `src/self_hosted/mir_lower/phi_predecessor_binding_fact_owner.pgy`,
  `ssa_identity_owner.pgy`, `routine_definition_dominance_fact_owner.pgy`, `mir_cfg_graph_owner.pgy`.
- direct MIR 해제: `src/self_hosted/compiler/owned_storage_release_plan_owner.pgy`,
  `owned_local_lifetime_owner.pgy`, `owned_storage_result_recipe_owner.pgy`,
  `owned_expression_temporary_owner.pgy`, `.../array_string_cleanup_policy_owner.pgy`.
- 검증자 시작점: `collection_cleanup_fact_owner.pgy`(`local-collection-retire`를 이미 읽음).
- hand-off 규칙: `src/semantic/call_handoff_rule.def`(렌더러 `scripts/render_call_handoff_rule.py`).
- 최근 coverage 근거: [DM1](../truth/evidence/2026-11-16_direct_mir_dm1_coverage_families.md),
  [DM2](../truth/evidence/2026-11-17_direct_mir_dm2_driver_and_fuzz_failure_set.md).
