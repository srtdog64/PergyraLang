# 181. Parallel 표면 전체 설계 — join-형·reactive-형 완전 구현 청사진 (2026-07-11)

**결정 기록 (BDFL 2026-07-11):** 목적지 = 두 vision 표면의 **전체 구현**.
과도기 = **fail-close**(무음 수용을 명시 진단으로 — 이 문서와 같은 날 착지).
진행 = **rung 사다리, 돌다리 두드리듯** — rung마다 목격자 먼저(RED) → 구현
(GREEN), docs/180 §6 이행 신호 준수. 입력 census = TODO 2026-07-11 노트
(`f0b2f6f2`): 표면 4종 중 실행 표면은 `parallel { arms }` 하나, join-형·
reactive-형은 파싱-후-무음-폐기, task_group은 파서 생산 없는 AST 잔재.

원칙 계승: 증거 4종(docs/178) 위의 확장이지 신축 아님 · 선언이 곧 증거
(declare-not-decide, docs/19 §✦) · byte-equal compare가 못 무는 기능은
착지 금지(가상 시간이 그 답) · 취소는 협조적(cooperative)만.

---

## §1. 형 A — `parallel (x in xs) join with <mode>` (데이터 병렬)

### 1.1 표면 문법 (확정)

```text
parallel (x in xs) { body }                    // R0: 문 형태, all-join 기본
parallel (x in xs) join with all { body }      // 명시 all
parallel (x in xs) join with any { body }      // R3: any (취소 종속)
let rs = parallel (x in xs) join with all { .. } // R2: 식 형태 (결과 수집)
```

- **원소 바인딩 필수**: `x in xs` — for-in과 동형 어휘(신규 키워드 0).
  기존 sketch(바인딩 없음)는 body가 원소를 만질 방법이 없어 폐기.
- `join with` 생략 = `all`. mode는 예약 식별자 all/any만(그 외 파스 에러).

### 1.2 의미론 (확정 + R1 열린 옵션)

- **증거**: N-way Disjointness — "태스크 i가 원소 i를 소유"는 인덱스-서로소
  정리로 값-독립(rung 0의 2-way 분할과 동일한 선언-기반 증거). 원소 밖
  캡처는 기존 arm 규칙 그대로(증거 4종 + capture-disposition fact row).
- **컬렉션 본체**: body 안에서 `xs` 직접 참조 = 거절(base-in-arm reject
  재사용). 순회 길이는 진입 시 고정(스냅샷 길이) — 구조 변경 원천 차단.
- **R0 원소 = read-only 값 복사**(spawn-arg 복사와 동형; 복사-교리 유지).
- **R1 확정 (2026-07-11): 옵션 (ii) 인덱스형** — `parallel (i in lo..hi)`,
  range는 for-루프와 동형 어휘(표면 추가 0). 옵션 (i) 원소-view 바인딩은
  기각(1-원소 slice 특수 규칙이 늘 뿐, 인덱스형이 그 일반화를 이미 담는다).
  **[i]-정칙**: 캡처 배열의 body 내 모든 출현(읽기 포함)은 정확히
  `name[binding]`이어야 admit — 읽기까지 묶는 이유는 alias 견고성(두 캡처
  이름이 같은 backing을 alias해도 모든 접근이 task i의 원소 i에 착지 →
  값-독립 무중첩). 고정/계산 인덱스·whole-array 사용은 거절(스텐실류
  이웃 읽기는 alias 증거가 생기는 후속 rung). 원소 바인딩 모드(`x in xs`)는
  배열 캡처 전면 불허(값 바인딩엔 인덱스 정리가 성립 안 함) — 인덱스형으로
  유도하는 진단. 범위 밖 인덱스는 런타임 OOB fail-closed panic이 문다.

### 1.3 런타임 매핑

- N 태스크 fan-out → 기존 4-worker 풀 큐잉(추가 런타임 0). 부모는 join
  블로킹(현행 parallel과 동일). 원소 간 채널 의존은 증거 규칙이 이미 거절
  → 풀-고갈 데드락 클래스 원천 차단.
- 청킹/그레인은 **R2에서 측정 후**(amortized-cost 원칙: 수치 없이 최적화
  금지).

### 1.4 rung 사다리

| rung | 내용 | 게이트 |
|---|---|---|
| R0 ✅ | 문 형태 · all-join · read-only 원소 · Array<T> | 목격자 `parallel_join_collection`(compare) + 거절 4종(무바인딩/xs 직접 접근/원소 쓰기/외부 쓰기) |
| R1 ✅ | 인덱스형 `(i in lo..hi)` + `arr[i]` in-place 쓰기 | 목격자 `parallel_join_index`(compare, 164/328) + 거절 4종(비-바인딩 인덱스/whole-array/원소모드 배열 쓰기/읽기) |
| R2 ✅ | 식 형태 `let rs = parallel ... { give e; };` (결과 Array<R>, 인덱스 순서) | 목격자 `parallel_join_expr`(compare, 204/182/2/72 — 원소 프로브가 인덱스 순서를 고정) + 거절 3종(give 없음/give 비-최종/문장형 give). 청킹 측정은 잔여(게임 소음 없는 시점) |
| R3 | `join with any` | §2.4 취소 프로토콜 선행 |
| R4 ✅ | reduce 조합자 `join with sum\|product\|min\|max` (결과 스칼라, 인덱스-순서 고정 left fold) | 목격자 `parallel_join_reduce`(compare, 204/24/3/-2/0/1 — Float 행이 fold 순서를 고정) + empty-min 런타임 panic 목격자(양 백엔드 class=out-of-bounds) + 거절 3종(미지 조합자/Bool give/문장형 reduce) |

**R0 착지 (2026-07-11, WO-PARSURF-2)**: 파서(`parser_parallel.c`, 원소
바인딩 필수·all 기본·any는 R3 fail-close) → semantic
(`type_checker_flow_parallel_join.c`: Array<primitive> 요구, 원소 read-only,
collection-in-body 거절, **replicated-arm 규칙** = 외부 바인딩 쓰기 전면
거절 — N개 동시 인스턴스라 single-writer 증거가 성립 불가) → C 이미터
(`transpiler_parallel_join_emit.c`: per-element ctx 배열 + 단일 wrapper +
fan-out/join 루프) → LLVM 이미터(`llvm_stmt_parallel_join.c`: 동적 IR 루프,
alloca 유도변수, 핸들 non-null guard). rung-0 명시 경계: slot/callable
캡처는 양 백엔드 동일 fail-close("a later rung"). 검증: 204 양 백엔드 ·
compare 등록 · 스모크(`parallel_join_smoke.sh`) 3플랫폼 CI 배선 · 유닛
918/0·2794/0. 새 책임 = 새 파일 4개(550-line 규칙).

**R1 착지 (2026-07-11, WO-PARSURF-2 R1)**: 파서(range 끝점, for-루프와
같은 `..` 인라인 매칭) → AST(`join_range_end` + **index-disjointness fact
rows** `join_index_arrays` — join admission checker가 단일 생산자,
snapshot-row 패밀리와 reset 소유권 분리) → semantic([i]-정칙 walker
`ast_parallel_index_analysis.c` 신설: 알 수 없는 노드 종류는 free-ref
oracle로 fail-close = over-reject-never-over-admit; replicated-arm 루프는
fact-admitted 배열만 완화; arms 공유-컬렉션 거절에 index-fact 예외) →
C 이미터(인덱스 fan-out `lo+i`, 배열 캡처는 fact 게이트) → LLVM 이미터
(endpoint sext→i64·클램프, **wrapper 내 배열 레지스트리 재등록** — 레지스트리가
scope alloca 키라 경계 넘으면 재바인딩 필수, 캡처 시점에 elem 메타 값-스태시).
검증: 164/328 양 백엔드 byte-equal · compare `parallel_join_index` 등록 ·
거절 8종 스모크 · 유닛 918/0·2794/0·parser·concurrency. Int=i32/int32_t
parity 유지. 부수 수확: 원소 모드의 배열-읽기 캡처 C/LLVM 수용 비대칭이
semantic 단일 거절로 닫힘.

**R2 착지 (2026-07-12, WO-PARSURF-2 R2)**: BDFL (b)안 비준 — body의 값
산출은 **명시 `give <expr>;`**(구두점 register 원칙과 정합: code층 유지;
(a) 마지막-식-값 안은 무종결=세계층 의미와 충돌해 기각). `give`는
parallel body 안에서만 인식되는 문맥 키워드(밖에서는 평범한 식별자;
corpus 충돌 0 전수 확인). 규칙: 정확히 1회·body 최종 문장·primitive 결과.
결과는 **인덱스 순서**로 Array<R>에 수집(완료 순서 아님 — byte-equal
compare가 병렬 결과를 물 수 있는 이유; 목격자의 원소 프로브 2/72가 순서를
고정). fact 규율: checker가 give 결과 타입을 노드에 봉인
(`join_give_type_name`), 양 백엔드 infer(transpiler/LLVM/MIR
source-local-expr 3지점)가 소비 — body 재추론 금지. C는 GNU
statement-expr(`({ PgyArray_R _pj_res = {0}; ...; _pj_res; })`)로,
LLVM은 `pgy_array_new_R(n)` + per-task 결과 슬롯 push 루프로 물질화.
착지 위치는 let-초기화가 유일 검증 지점이나 admission은 표현식 일반
(parse는 이미 primary). 잔여: 청킹/그레인 측정(게임 소음 없는 시점),
non-primitive give, give-in-branch.

**R4 착지 (2026-07-12, WO-PARSURF-2 R4)**: `join with` 슬롯의 조합자
어휘를 닫힌 집합으로 확장 — `all`(수집)에 `sum`/`product`/`min`/`max`
(fold) 추가, `any`는 R3 유지. 결과는 Array<R>가 아니라 **스칼라 give
타입**(수치 전용 — Bool give는 semantic 거절), 문장형 reduce는 "결과를
버리는 모양"이라 fail-close. fold 의미론 3결정: **① 인덱스-순서 고정
left fold**(완료 순서 무관 → Float까지 양 백엔드 byte-equal; 미래 청킹은
이 fold 모양을 (n, 선언된 정책)의 함수로 보존해야 하며 스케줄-적응형
금지), **② Int/Long 합·곱은 surface `+`/`*`와 동일한 checked-arith
export**(`pgy_checked_add/mul_i32/i64_export`)를 fold 스텝으로 호출 —
reduce가 unchecked overflow를 몰래 재도입하는 것을 구조로 차단, **③
empty fan-out의 min/max는 런타임 fail-closed panic**(양 백엔드가 같은
`pgy_runtime_panic_out_of_bounds_export`에 같은 reason 문자열 —
"identity 극값 반환"은 가짜 도메인 값 누출이라 기각; sum/product는
항등원 0/1 반환). min/max의 NaN은 ordered-compare로 accumulator 유지
(C 삼항 ↔ LLVM fcmp-olt/select 쌍둥이). fact 규율은 R2 재사용:
`join_reduce_op`은 parse-time 표면 fact로 노드 봉인, give 타입 fact와
함께 3 infer 지점(transpiler/LLVM/MIR)이 스칼라로 소비. 검증:
204/24/3/-2/0/1 양 백엔드 byte-equal · empty-min panic 양 백엔드 ·
거절 14종 스모크 · concurrency 전부 · semantic 2794/0. 잔여:
non-primitive/사용자-정의 모노이드(ability/witness 필요 — 별개 rung),
`join with reduce(op, id)` 일반형은 필요 실증 전 보류.

## §2. 형 B — role reactive block (`parallel on (lane) { every/continuous }`)

### 2.1 정체 (확정)

이 표면은 고아가 아니라 **SEA lane 아키텍처의 표면 문법**이다(3층: SEA
계약 / ExecutionLaneFact IR / PgyLaneScheduler 런타임 — facade fill이 남은
그 자리). `on (aiThread)`는 스레드 지정이 **아니라 lane 선언-증거**다
(declare-not-decide): 이름은 사용자-선언 lane id로 ExecutionLaneFact에
lower되고 스케줄러가 소비한다. 수동 배치 해석은 SEA canon("lane은 증거로
결정")과 충돌하므로 금지 — 선언-증거 재해석이 유일한 합법 독해.

### 2.2 라이프사이클 (R2에서 확정, 후보 기록)

role은 정적 선언이라 "언제 돌기 시작하나"가 미정의. 후보: (i) world
enter/exit 자동 (ii) **명시 시작/정지 API**(권고 — 가장 관측 가능하고
fail-closed, 취소 프로토콜과 자연 결합) (iii) main 진입 자동. SEA facade
설계와 한 몸으로 확정한다.

### 2.3 시간 (확정 — compare가 물 수 있는 유일한 길)

- **duration 리터럴**: `1000ms` / `5s` — 렉서에서 숫자+단위 접미사를
  Duration 리터럴 토큰으로(내부 Int ns). 현행 "숫자 파싱 후 식별자 스킵"
  눙침 폐기. R1.
- **가상 시간 필수**: `PGY_VIRTUAL_CLOCK=1`에서 every는 벽시계 대신 틱 큐
  소비, 테스트 훅 `pgy_clock_advance(ms)`. **compare 게이트는 가상 모드만
  문다**(벽시계 경로는 비결정 — 스트레스 스모크 전용). 가상 클록 없이
  every 착지 금지 — byte-equal 문화가 못 무는 기능을 만들지 않는다.

### 2.4 취소 (R2, any-join과 공용)

협조적 프로토콜만: 태스크당 stop flag + 안전점(루프 백엣지, 채널 블로킹
진입 전) 검사. pthread 강제 종료 금지. R6 budget 강제의 주입점과 공유
가능성 검토(둘 다 "안전점에서 플래그 확인" 모양).

### 2.5 rung 사다리

| rung | 내용 | 선행 |
|---|---|---|
| R0 | 설계 확정(이 문서) + fail-close 유지 | — |
| R1 | duration 리터럴 + 가상 클록 런타임 | — |
| R2 | every/continuous 실행(명시 시작 API, 단일 lane) + 취소 | R1 |
| R3 | `on(lane)` → ExecutionLaneFact 연결 | SEA facade fill |
| R4 | role 라이프사이클 자동화(§2.2 확정안) | R3 |

게이트: 가상-클록 결정 목격자(3틱→로그 3, byte-equal) · 취소 목격자(stop
후 틱 0) · lane fact golden(기존 SEA golden 확장).

## §3. task_group — 삭제 판정

`parallel { }` = wait-all, any는 §1의 join mode가 흡수. 파서 생산이 없는
AST 노드+walker 잔재(~25파일)는 **구현이 아니라 제거**(§8 cosmetic
abstraction 금지). 별도 기계적 커밋(WO-PARSURF-4).

## §4. 과도기 fail-close (이 문서와 같은 날 착지)

두 형의 파서 인식 지점에 명시 진단: "declared vision surface (docs/181),
not yet executable". 폐기하던 의미(collection 식, on-target, 주기)는
진단과 함께 여전히 소비만 하고 버림(복구 매끄럽게). census 근거로 게이트
무손상(두 sketch 데모는 라벨 강제 + 컴파일 게이트 제외). 목격자 스모크가
진단 문구를 고정하고, rung이 실행을 여는 순간 그 스모크가 RED로 울려
**이 문서 갱신을 강제**한다.

*갱신 (2026-07-11, R0 개통)*: 예고대로 작동했다 — all-join이 실행으로
졸업하며 vision 스모크에서 join-form 거절을 빼고 any-join(R3)으로 교체.
남은 fail-close 표면 = any-join + reactive-형 전체.

## §5. 이행 프로토콜

rung마다: 목격자 먼저(RED) → 구현(GREEN) → compare 등록 → docs/178·181
갱신 → boundary_migration_manifest row(신규 fact 생기는 rung: R0의
element-disjoint fact, R3의 lane fact 연결). 무음 수용 재도입은 §4 스모크가
거부.

## Related

- docs/178(증거 4종) · docs/180 §6(이행 신호) · docs/168(Fortran 병렬
  evidence — §1의 배열-병렬 선행 사고) · SEA 3층(project 메모리/골든) ·
  TODO WO-PARSURF-1~4
