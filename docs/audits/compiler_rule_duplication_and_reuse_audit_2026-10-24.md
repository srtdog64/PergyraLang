# 컴파일러 규칙 중복과 재사용 경계 감사 — 2026-10-24

상태: **AUDIT COMPLETE — 읽기 전용 조사**
기준: `fdc661ff9bbc97033cc2a9615ab4ab3e0d7f5f99`와 당시 작업 트리
범위: 테스트·fixture를 제외한 C/Pergyra 구현의 대표 중복 경로
비범위: 리팩터링, 의미 변경, 게이트 실행, CI 판정, V1 완료 판정

## 결론

문제는 함수 재사용의 절대량이 아니다. **같이 바뀌어야 하는 의미 규칙이나
안전 판단이 둘 이상의 owner에 남아 있는 경우**가 실제로 있다. 반대로 출력
형식이나 소비 목적이 다른 코드를 몸체가 비슷하다는 이유만으로 합치면 새로운
generic helper와 결합만 늘어난다.

입력 조사에서는 주석·공백을 제외하고 일정 크기 이상인 동일 함수 본문을 비교해
3,203개 C/Pergyra 파일에서 36묶음·95개 함수를 보고했다. 이 문서는 그 수치를
독립적으로 다시 산출하지 않았다. 대신 아래 다섯 대표 경로의 소스와 호출 경계를
직접 확인했다. 따라서 36/95는 현재 결함 개수나 완료 게이트가 아니라 **탐색
신호**다. 파일·함수 개수나 문자열 존재 여부를 CI 게이트로 만들지 않는다.

## 목적 카드

- **목적:** 동일 의미 규칙의 다중 owner와 기존 공통 구현 미사용을 구분하고,
  안전하게 한 owner로 수렴시킬 순서를 정한다.
- **우선순위:** 의미 identity와 한 SoT → owner가 발급한 사실 소비 → 우회 검색
  삭제 → 행동 기반 음성/동등성 게이트 → 코드량 감소.
- **금지:** 거대한 `helper` 파일, 백엔드끼리의 무리한 통합, program-shape별
  direct-MIR 경로의 재포장, 중복 개수 자체를 진척으로 세기.
- **현재 실행 경계:** 이 감사만으로 V1 작업을 넓히지 않는다. 현재 V1 실행
  사슬이 아래 경로를 실제로 통과할 때만 그 사슬 안에서 제거한다.

## 확인 결과

| 경로 | 실제 분류 | 현재 owner 문제 | 의도한 수렴 |
|---|---|---|---|
| 연산자 별칭·접미사 | 의미 규칙 중복 | 의미 검사기, C, LLVM이 각각 이름 별칭과 token 매핑을 소유 | 의미 분석이 해석한 callable identity를 MIR에 싣고 백엔드는 그 identity만 소비 |
| 문자열 포맷 할당 | 기존 공통 구현 미사용 | 공통 구현이 있는데 MIR·의미 분석이 같은 할당/포맷 절차를 재작성 | `pergyra_strdup_vprintf`/`pergyra_strdup_printf`로 계약을 하나로 유지 |
| 빈 `Array<Int>`/`Array<Bool>` direct-MIR | program-shape 우회 중복 | 타입별 route가 병렬 배열 분해·재조립을 반복 | V1의 일반 MIR emitter가 node kind와 ABI type row를 소비한 뒤 두 route를 삭제 |
| copy-edge 공유 관계 탐색 | 공통 안전 사실 후보 | 변경 전파와 backing-loan 충돌 검사가 같은 연결 관계를 따로 탐색 | immutable storage-sharing component 질의만 별도 owner로 만들 수 있는지 먼저 증명 |
| LLVM region 승인·runtime 조회 | 백엔드 내부 계약 후보 | Clone과 String concat이 같은 scope currency 검사를 각각 수행 | region 활성화 검증만 책임 이름이 있는 LLVM owner로 수렴하고 payload lowering은 분리 유지 |

### 1. 연산자 규칙은 단순 함수 중복이 아니라 다중 의미 authority다

- `src/semantic/type_checker_expr_ops.c:17,37`
- `src/codegen/transpiler_operator.c:16,36`
- `src/codegen/llvm_domain_method_helpers.c:13,33`

세 경로가 `+`부터 비교 연산까지의 접미사와 `Add`/`add`/
`OperatorAdd` 같은 별칭 목록을 각각 가진다. 이것은 C와 LLVM의 출력 차이가
아니라 source 이름을 같은 연산자로 인정하는 언어 규칙이다. 공통 C 배열 하나를
세 파일에서 호출하게 만드는 것만으로는 충분하지 않다. 백엔드가 source/AST
별칭을 다시 찾는 권한 자체가 남기 때문이다.

의도한 owner는 의미 분석의 operator target resolution이다. 그 owner가 안정된
callable identity를 발급하고 MIR이 운반하며 C/LLVM은 최종 target만 소비해야
한다. 금지할 fallback은 백엔드의 별칭 재검색이다. falsifier는 한 별칭으로 정의한
overload가 의미 검사, C 실행, LLVM 실행에서 같은 target과 결과를 내며, MIR target
사실을 제거하거나 바꾸면 두 백엔드가 fail closed 하는 입력이다.

### 2. 문자열 포맷은 기존 공통 구현으로 수렴 가능한 실제 재사용 누락이다

- 공통 구현: `src/common/string_compat.h:39`
- MIR 복제 예: `src/compiler/mir_base_helpers.c:68`
- 의미 분석 복제 예: `src/semantic/type_checker_resolution_graph_body.c:8`

`pergyra_strdup_vprintf`는 길이 계산 뒤 두 번째 `vsnprintf`의 음수 결과와 기대
길이 불일치를 검사하고 실패 시 버퍼를 해제한다. `resolution_body_strdup_fmt`는
두 번째 결과를 검사하지 않는다. 중복끼리 오류 계약이 이미 갈라진 사례다.

다만 일괄 치환 전에는 각 호출자가 `NULL`과 빈 문자열을 어떻게 해석하는지,
format 인자의 수명과 allocator가 같은지 확인해야 한다. 계약이 같다면 새 wrapper를
만들지 말고 기존 공통 함수를 직접 사용한다. 검증은 대표 성공 출력과 할당 실패
경로를 실행하는 기존 단위/통합 게이트로 하고, 함수 이름이 사라졌는지를 검사하는
text scan은 만들지 않는다.

### 3. 타입별 direct-MIR 중복은 추출보다 삭제가 목표다

- `src/self_hosted/compiler/direct_mir_scalar_program_array_int_empty_literal_admission_owner.pgy:12`
- `src/self_hosted/compiler/direct_mir_scalar_program_array_bool_empty_literal_admission_owner.pgy:12`

두 함수는 기대 ABI type과 expression kind만 다르고, `facts`의 병렬 배열을 모두
꺼내 노드를 추가한 뒤 다시 집계하는 절차는 같다. 그러나 V1 계약은
`direct_mir_*` 같은 program-shape admission route를 새 추상화로 보존하지 않고,
instruction/expression/ABI row별 일반 emitter가 해당 프로그램을 덮으면 삭제하라고
정한다.

따라서 두 함수를 공통 `DirectMir...EmptyLiteralHelper`로 묶는 것은 잘못된 종착점이다.
일반 expression node consumer가 Int/Bool empty literal을 모두 C/LLVM에서 실행하고
동일한 runtime 결과를 낸다는 게이트를 먼저 세운 뒤, 타입별 route와 import를
삭제해야 한다. 이 작업은 현재 self-host 실행 rung이 해당 경로를 통과할 때만 V1
진척으로 센다.

### 4. copy-edge 순회는 공통화 전에 같은 관계인지 증명해야 한다

- 변경 전파: `src/semantic/default_aggregate_copy_owner.c:164`
- 대입 중 backing loan 탐색: `src/semantic/default_aggregate_call_move_owner.c:56`

둘 다 `binding`/field path에서 시작해 source와 carrier 양방향으로 copy-edge를
따라간다. 하지만 첫 경로는 순회 중 `MoveState`에 새 변경 row를 추가하고 loan
부작용을 검사한다. 두 번째는 기존 state의 연결 성분 안에서 이미 잡힌 backing
loan을 찾는 질의다. 몸체가 닮았다고 함수 전체를 합치면 mutation과 query의
책임이 섞인다.

공통화 가능한 최소 사실은 **한 고정 snapshot에서 어떤 storage path들이 같은
저장소 연결 성분에 속하는가**다. 이 사실의 owner를 만들려면 다음 반례가 먼저
필요하다.

- source 방향과 carrier 방향 각각에서 같은 성분을 찾는다.
- 상위/하위 field path overlap을 보존한다.
- copy cycle에서 한 번씩만 방문한다.
- 변경 전파가 순회 중 추가한 row를 같은 세대로 볼지 다음 세대로 볼지 명시한다.
- backing loan이 성분 어디에 있어도 충돌을 거부하고, 무관한 형제 field는 거부하지
  않는다.

이 의미가 고정되기 전에는 새 generic graph helper를 만들지 않는다.

### 5. LLVM은 region 활성화 검증만 공유 후보이고 lowering은 별개다

- Clone: `src/codegen/llvm_expr_clone_value.c:32-57`
- String concat: `src/codegen/llvm_expr_scalar_core.c:428-462`

두 경로 모두 verified region plan 조회, active scope id, region alloca, 현재 function,
runtime symbol을 검사한다. 반면 Clone은 빈 문자열과 결합해 독립 String을 만들고,
concat은 두 피연산자를 String으로 coercion한 뒤 결합한다. 후자를 한 emission
함수로 합치면 payload 의미가 섞인다.

공유 후보는 `node identity -> 현재 함수에서 사용 가능한 verified region
activation` 판정까지다. 반환 사실에는 scope id, alloca, runtime entry의 currency가
들어가야 하며 missing runtime과 wrong scope는 서로 다른 기존 진단을 보존해야 한다.
검증은 올바른 scope의 Clone/concat 성공, scope 이탈과 runtime inventory 누락의
fail-closed, C/LLVM 결과 동등성을 포함한다.

## 실행 순서

1. **현재 V1 rung을 방해하지 않는다.** 이 문서는 코드 정리 작업을 새로 여는
   지시서가 아니다.
2. **direct-MIR는 기존 V1 self-host 일반화 사슬에서만 제거한다.** 공통 route를
   새로 만들지 않는다.
3. **연산자 identity는 별도 SoT 폐쇄로 다룬다.** 의미 owner 발급 → MIR carriage
   → C/LLVM consumer 이전 → 백엔드 별칭 검색 삭제 → 음성/동등성 게이트 순서다.
4. **포맷 함수는 계약이 같은 호출자부터 기존 공통 구현으로 옮긴다.** 의미 SoT
   변경이 아니므로 작은 독립 변경으로 끝낼 수 있다.
5. **copy-edge와 LLVM region은 반례로 공통 사실의 경계를 확인한 뒤에만 owner를
   추출한다.** 소비자별 mutation/lowering은 합치지 않는다.

## 남은 미확인 사항

- 36묶음·95개 전체 목록과 필터를 재현하는 고정 스크립트/receipt는 이 감사에
  포함되지 않았다.
- 각 포맷 함수 호출자의 `NULL` 계약과 allocator 동일성은 전수 확인하지 않았다.
- copy-edge 두 순회가 동일 snapshot 의미를 가질 수 있는지는 아직 증명되지 않았다.
- LLVM region 공통화가 실제 빌드 시간이나 결함률을 줄인다는 성능 측정은 없다.
- 이 감사 이후 코드·게이트·CI를 실행하지 않았으므로 어떤 V1 조건도 새로
  충족됐다고 주장하지 않는다.
