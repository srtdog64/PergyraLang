# 182. 병렬 표면 잔여 뼈 — 실행 주문서 (2026-07-12)

Status: `work-orders`, 실행 대기 (잠금 해제 조건부)

docs/181의 형 A(join 표면)는 R0~R5 + R3 슬라이스 1로 골격 완결(게임
잣대 5/5). 이 문서는 **남은 뼈 전부**를 실행 가능한 주문서로 고정한다.
작성 시점에 동시 스트림이 capture-fact MIR 이주 + join 이미터 분할을
staged 상태로 진행 중이라(ast.h/채널 런타임/join 이미터/Makefile/TODO
잠김), 주문서마다 **잠금 의존성**을 명시한다. 잠금과 무관한 뼈 하나
(§0 클록 기반)는 이 문서와 같은 날 착지했다.

## §0. 착지 완료 — 클록 기반 (docs/181 §2.3의 주춧돌)

`pgy_clock_now_ns_export` / `pgy_clock_advance_ns_export` /
`pgy_clock_is_virtual_export` — 실모드는 호스트 단조시계, 가상모드
(`PGY_VIRTUAL_CLOCK=1`, 첫 사용에 latch)는 advance로만 움직이는
프로세스 단일 카운터. 상태는 export-측 TU 단일 인스턴스
(g_pgy_budget/cap_granted와 같은 규칙 — 생성-C와 bc가 한 상태 공유).
실시계 advance/음수 advance = lifecycle panic(무음 no-op 금지 — 테스트
훅이 조용히 삼키면 그 훅이 잡으려는 drift를 숨긴다). LLVM 빌트인 선언
3종 등록. 목격자 3/3(실모드 단조·가상 0→5ms 정확·misuse panic),
`tests/virtual_clock_smoke.sh`(독립 실행형 — **test-all 배선은 Makefile
해제 후**; 로컬 주의: 게임 중 bash→gcc 무음 사망 채널 함정, CI에는
해당 없음).

## §1. WO-PARSURF-3a — Duration 리터럴 + Duration 타입 (반응형 R1 잔여)

**잠금**: ast.h/ast_domain_data.h/ast_destroy.c (경로 A), 없음(경로 B).

- 렉서: `<digits>ms|s` 닫힌 집합(us/ns는 필요 실증 후). 숫자 직후
  공백-없는 접미사만; `5 ms`는 리터럴 아님. 현행 "숫자 후 식별자 스킵"
  눙침 폐기(§2.3 명시).
- 내부 표현: **Long(i64) ns** — 문서의 "Int ns"는 i32로 2.1초가 한계라
  기각, ns 해상도 유지가 미래 정밀 타이머와 호환.
- **경로 A (정공, ast.h 해제 후)**: `AST_DURATION_LITERAL{int64 ns}`
  신설 + walker 8지점(destroy/identity/print/…) + `Duration` primitive
  타입 등록 + 양 백엔드 i64 상수 lowering.
- **경로 B (잠금 지속 시, ast.h-free)**: 파서가 리터럴을
  `DurationNs(<Long literal>)` 빌트인 **call로 desugar**(기존 노드
  종류만 사용) + semantic에 Duration 타입/빌트인 시그니처 등록. 경로
  A가 열리면 B의 desugar를 리터럴 노드로 치환(표면 불변).
- 거절: 미지 단위(`5m`) parse error / Duration↔Int 암시 혼용 type
  error / 음수 duration.
- 목격자: compare 픽스처(리터럴 2종 → ns Long 출력), 거절 3종.

## §2. WO-RT-2 — 취소 슬라이스 2 (any의 관절 완성 + §2.4 선언 잔여)

**잠금**: 채널 런타임(신설 `pgy_runtime_channel_lifecycle_inline.h`에
맞춰 착지 — wait 루프의 새 주소에 종속), join 이미터.

1. **any await-구멍 수리 (설계 결함, 정직 기록)**: 슬라이스 1의
   순서-await는 승자보다 **앞 인덱스의 블로킹 패자**가 join을 영구
   정지시킨다(승자 존재를 관측 못 함). 수리: n>0일 때 call-site가
   ①decided를 yield-spin으로 대기 → ②전 핸들 cancel → ③전 핸들
   await. n==0은 spin 진입 전 기존 empty-panic 경로. spin은 최초
   give까지 바운드(게임 워크로드에선 짧음), true waitany는 후속.
2. **취소가능 채널 블로킹 대기**: 모든 블로킹 wait 루프(send/recv/
   timeout/spsc-spin)의 wake 조건에 `pgy_task_is_cancelled()` 편입.
   cancelled 반환은 **계약 결과**(false/None — panic 아님): any 패자는
   이후 give 도달 → CAS 패배 → 은퇴로 자연 수렴. 채널 자체 의미론
   불변(취소 없는 프로그램은 경로 불변). 트윈 lockstep + `.bc` 재생성
   + parity 필수(어제 lifecycle 승격과 같은 드릴).
3. **루프 백엣지 안전점**: any-wrapper 안 while/for 백엣지에 decided
   검사 → 은퇴. C = `in_pjoin_any` 플래그로 루프 이미터 분기, LLVM =
   `pjoin_any_state_ptr` 비-NULL 시 백엣지에 load-acquire+cond-ret.
   달리는 패자의 중도 은퇴가 열리며 §2.4 선언(백엣지+채널 진입)이
   완성된다.
- 목격자: 블로킹-패자 픽스처(승자 give 후 join이 유한 시간 내 복귀 —
  슬라이스 1이면 hang), 백엣지 은퇴 픽스처(패자 루프가 decided 후
  중단), 기존 17거절+panic 3종 무회귀.

## §3. WO-PARSURF-3b — every/continuous 실행 (반응형 R2)

**잠금**: parser_parallel.c는 열려 있으나 **§2(취소) 선행** + BDFL
판정 3건이 남음. 판정 없이 착수 금지:
1. 시작 의미론 — 블록 도달=암시 시작 vs 명시 시작 API.
2. 취소 표면 — 핸들 반환(`let t = parallel every…`) vs 스코프 종속.
3. 단일 lane 규약 — worker-pool 고정(§2.5 R2)인지, role 소유(R4)까지
   보류인지.
- 실행 모양(판정 후): every-블록 = spawn된 루프 태스크, 가상모드는
  `now >= next_tick`까지 advance-브로드캐스트 대기(클록 기반에 waiter
  condvar 추가), 실모드는 nanosleep — **compare 게이트는 가상모드만
  문다**(§2.3). 목격자: 3틱 advance → 로그 3(byte-equal), stop 후 틱 0.

## §4. WO-PARSURF-4 — task_group 삭제

**잠금**: ast.h + walker 파일들. census 완료(29파일, AIR 테스트
.cases.h 2 포함). 파서 생산 0 확인됨 — 구현이 아니라 제거(§8 cosmetic
abstraction 금지). ast.h 해제 직후 기계적 단일 커밋.

## §5. WO-PARSURF-5 잔여 — 분할 위생

동시 스트림이 llvm_stmt_parallel_join capture 분할 + llvm_stmt_block
분할을 진행 중 — **그 커밋이 이 주문의 절반을 흡수**. 잔여:
`transpiler_parallel_join_emit.c`(561) — reduce/any 물질화를
`transpiler_parallel_join_reduce_emit.c`로 분리(그들 착지 후 충돌
없이). 게이트 filename-pin 이주 확인 필수(negative 검사 공허 통과
함정).

## §6. 교리-게이트 뼈 (의도적 미착수 — 게이트가 지키는 갭)

- **청킹/그레인**: measure-first(docs/127 §1) — 게임 소음 없는 시점의
  측정이 선행. 설계 제약만 선고정: fold 모양 = (n, 선언된 정책)의
  함수, 스케줄-적응형 금지(R4 착지 노트).
- **일반 DP 사다리(docs/168 DP-2~6)**: 지금까지는 전부 join-표면의
  선언된 인스턴스. 임의 루프 read/write-set 유도(DP-2/3),
  ReductionFact 일반화(DP-4), 벡터화(DP-5), 비-CPU projection(DP-6)은
  별도 트랙 — claim gate("general DP-3 전 포트란급 주장 금지") 유지.
- **타입 폭**: non-primitive give / 사용자 모노이드(ability/witness
  필요) / 2D·타일 stencil — 각각 필요 실증 후 rung.

## 실행 순서 (잠금 해제 시)

§2(취소 — any의 정직성 구멍이 걸려 있어 최우선) → §1 경로 A →
§4(기계적) → §3(BDFL 판정 후) → §5 잔여. §6은 교리 조건 충족 시.
