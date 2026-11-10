# 컴파일 경로 일관성과 자동정리의 남은 기능 결함

2026-11-06 KST. 사용자 요청에 따른 기능 감사다. SoT 레지스트리 정리와
전면 구조 개편은 작업 목록에 넣지 않는다. 기능을 고치는 데 필요한 타입·
소유권·수명 정보 연결은 해당 기능의 수정 의무에 포함한다.

핵심은 **일반 String과 일부 컨테이너의 실제 누수**, **같은 소스에 대한
native / 설치 C / 설치 LLVM의 서로 다른 판정·지원**이다. 수학적 모델이나
일부 Join 정리 성공을 이 범위의 완료 증거로 사용하지 않는다.

이 목록은 아래 고정 실행 범위의 확인된 결함과 전체 완료의 검증 공백을
모은 것이다. `tests/cases/**`와 `examples/**` 전체를 실행한 전수 언어
지원표는 아니다. 실행하지 않은 구문 조합까지 빠짐없이 파악했다고 주장하지 않는다.

## 고정한 실행 경계

- HEAD: `60fd1e26d8ea3987efbcbfc65f59b892b234701a` + 공유 dirty tree.
  메인이 동시에 구현 중이며 이 감사는 production 소스를 수정하지 않았다.
- 최신 감사 native/public launcher SHA-256:
  `b25581e17ae86f5634b12c971b62480621b5e6a2afd1d8cf971f825175aeec34`.
  `/root/pgy-v1-live-20261105-current/bin-v1-live/pgy`를 복사해 고정했다.
- 설치형 driver13 artifact SHA-256:
  `c931c8b5f5f6af386a399e9c90971fa2f3f90c6e330b33216baa80f800f64ead`.
  `.tmp/v001-driver13/pgy-self-driver`와 대응 manifest를 launcher 옆에 복사했다.
- Driver13 source-graph key:
  `824402e766eb05ddc40af7a860016a131b9a19380f71d2156a6f095a6eada8d8`.
  source-graph/build/output receipt도 같이 보존했다. native C build의 전체
  source manifest는 이 감사에서 재구성하지 않았으므로 clean HEAD 재빌드 증거가 아니다.
- 실행 prefix: `/root/pgy-functional-frontier-current-20261106`.
  원래 설치본·메인의 build/log·기존 staging은 변경하지 않았다.
- 19개 기존 허용 입력 + 3개 거절 입력 × native/installed × C/LLVM:
  88개 실행 경로, 25.93초. 64개 허용 실행 PASS, 12개 컴파일 실패,
  12개 거절. 입력과 고정 binary의 전후 hash가 모두 동일했다.
- 자동정리는 별도의 작은 7개 프로그램을 각 20회 호출했다. 생성 C/LLVM과
  canonical runtime을 ASan/LSan으로 링크한 28개 관측 중 22개 LEAK,
  3개 sanitizer 정상, 3개 installed LLVM emit 실패다.
- leak 종료는 stdout flush 전에 일어날 수 있으므로 같은 25개 실행 파일을
  leak reporting만 끈 값 관측으로 별도 실행했다. 25/25가 기대값과 일치했다.
  이 값 관측을 누수 해소로 세지 않는다.
- Int-only observer control은 네 경로 모두 정상. 명시적 ArrayDrop observer
  control은 native C/LLVM과 installed C에서 정상이며 installed LLVM은
  여전히 emit을 거절했다. 수동 drop은 관측기 대조군이며 수정안이 아니다.

이전 native `937be0...`로도 같은 조사를 수행했지만, 감사 중 새 native가
나와 위 세대를 다시 검증했다. 이전 빌드에서만 실패한 지역 Array literal
정리는 현재 native 결함 목록에서 제거했다.

## A. 실제 누수와 자동정리 누락

각 칸은 **프로그램 하나당 20회 호출 후 LSan이 확인한 잔존 bytes**다.
`정상`은 해당 관측에서 leak/error 없이 종료한 뜻이며 일반적인 완전성 증명이 아니다.

| 값/수명 | native C | native LLVM | installed C | installed LLVM |
|---|---:|---:|---:|---:|
| Concat String 임시값 | 220 B | 220 B | 220 B | 220 B |
| Concat String 함수 지역값 | 220 B | 220 B | 220 B | 220 B |
| String 필드를 가진 struct | 220 B | 220 B | 220 B | 220 B |
| 지역 `Array<Int>` literal | 정상 | 정상 | 320 B | emit 거절 |
| factory 반환 `Array<Int>`의 지역 소비 | 160 B | 320 B | 320 B | emit 거절 |
| 함수 지역 `List<Int>` | 1280 B | 1280 B | 1280 B | emit 거절 |
| 함수 지역 `HashMap<Int,Int>` | 2880 B | 2880 B | 2880 B | 정상 |

### F01 — 일반 String의 마지막 소비와 물리 해제가 연결되지 않음

- 재현: `string_temporary`, `string_local`, `string_record`.
  `Work`에서 String을 만들고 길이를 사용한 뒤 Int를 반환한다. drop은 없다.
- 네 경로 전부 각 20개 allocation/220 B가 남는다. stack은 `Work`에서 호출한
  `StringConcat`의 allocation을 가리킨다. 단순히 RSS가 줄지 않은 관측이 아니다.
- 수정 경계: existing String result/backing 사실 -> 소유 local/temporary/field ->
  마지막 소비/반환·분기 -> MIR 정리 의무 -> C/LLVM release 및 runtime.
  String의 출처 추론이 된다는 사실과 실제 release가 발급됐다는 사실을 구분해야 한다.
- 완료 반례: 같은 입력에서 수동 정리 없이 leak 0, 조기 release 시 ASan 실패,
  release 억제 시 LSan 실패. borrowed/static String을 해제해서 통과시키면 안 된다.

### F02 — Array literal의 설치본 정리·LLVM 지원 누락

- 최신 native는 함수 지역 `[n, n + 1]`을 정상 정리한다.
- installed C는 같은 값에서 20개 allocation/320 B가 남고, LLVM은 literal
  expression admission에서 거절한다.
- 수정 경계: native가 발급한 scalar Array retirement / exact binding /
  `ReleaseStorage` ABI를 설치 MIR·emitter 소비까지 연결한다.
  native만 통과한 raw/View retirement를 설치본 통과로 세면 안 된다.

### F03 — 반환받은 Array의 정리 책임 누락

- `Make(n) -> Array<Int>`가 fresh Array를 반환하고 `Work`가 읽은 뒤 끝난다.
- native C/LLVM, installed C에서 누수, installed LLVM에서 emit 거절이다.
- 수정 경계: callee의 fresh result 소유권 -> caller local -> 마지막 사용 ->
  cleanup. literal만 허용하는 정리 후보를 factory 반환값의 증거로 확장해야 한다.
  반환 loan을 fresh owner로 추정하거나 caller/callee 둘 다 해제하면 안 된다.

### F04 — List 지역값의 정리와 설치 LLVM 지원 누락

- `ListNew`, `ListPush`, `ListGet` 후 반환하는 작은 입력이다.
- native C/LLVM·installed C가 값은 맞게 계산하지만 backing을 정리하지 않는다.
  installed LLVM은 `stage=local_inventory`에서 거절한다.
- 수정 경계: List의 allocation/소유 수명/정리 ABI와 일반 local inventory·emitter.

### F05 — HashMap 정리의 backend/경로 불일치

- `MapNew`, `MapSet`, `MapGet`을 사용하는 같은 입력에서 native C/LLVM과
  installed C는 60개 allocation/2880 B가 남고 installed LLVM은 정상이다.
- 관련 기존 소비자: `direct_mir_scalar_program_c_hashmap_cleanup_owner.pgy`,
  LLVM counterpart와 shared HashMap lifetime/ABI owner.
- 해당 C source route가 실제로 어떤 cleanup을 소비했는지부터 추적해야 한다.
  LLVM 쪽 성공이나 두 파일의 비슷한 내용으로 C 경로까지 닫지 않는다.

## B. 같은 프로그램의 판정·지원 불일치

### F06 — 제네릭 메서드의 Array 반환 판정이 경로마다 다름

- 기존 입력: `generic_member_array_return_flow.pgy`,
  `generic_member_record_array_return_flow.pgy`.
- native C/LLVM: `non-own parameter 'value'; inferred transfer proof is unavailable`
  거절. installed C: 각각 `44`, `45` 실행. installed LLVM: 거절.
- native의 직접 소비자는 `type_checker_ownership_param_summary.c`의 Array
  result / default formal transfer·copy 판정이다.
- 실제 unsafe admission이라고 아직 단정할 수 없다. 예제의 concrete actual은
  Int 또는 Int 필드 Point이므로 generic declaration의 사실 부족에 의한 보수적
  거절 가능성과 actual별 독립 값 증명을 구분해야 한다.
- LLVM은 각각 `stage=literal ... AST_ARRAY_LITERAL`,
  `constructed record Array specialization is invalid`로 거절한다.
- 완료 조건: 같은 concrete ownership/ABI 사실로 두 frontends가 같은 판정을
  하고 두 backend가 같은 결과를 낸다. missing ABI/borrowed payload 반례는 유지한다.

### F07 — 실제 owned String Join을 설치 경로가 처리하지 못함

- 입력: `direct_mir_owned_array_string_real_join.pgy`.
- native C/LLVM: `actual-owner` 실행. installed C/LLVM: builtin-call
  node 3/global row 13 admission 거절.
- 앞선 고정 R8 분석에서 같은 지점의 누락 연산은 `SubContainsWithLen`이었다.
  새 driver13에서도 같은 failure tuple을 재현했다.
- 수정 경계: 해당 builtin의 타입·호출 ABI·expression projection을 일반
  emitter에서 지원하고 같은 실제 import-composed 입력을 실행한다.

### F08 — `unsafe(raw)` 표면이 설치 parser에 연결되지 않음

- 입력: `raw_view_ownership/branch_exit.pgy`.
- native C/LLVM은 `75`, `100`을 실행한다. installed C/LLVM은 3:11에서
  `{`를 기대한다며 parse 단계에서 거절한다.
- `parser/stmt_owner.pgy`는 unsafe 뒤 block을 바로 파싱한다.
  native가 허용하는 경계 표식을 기존 syntax identity로 운반해야 한다.
- 이것은 공개 raw pointer 타입을 새로 허용하라는 요구가 아니다.

### F09 — Slice indexed write의 설치 semantic type fact 누락

- 입력: `raw_view_ownership/scalar_arrays.pgy`.
- native C/LLVM의 Int/Long/Float/Double/Bool write-through 결과가 일치한다.
- installed C/LLVM은 첫 `view[0]` 대입에서 `assignment_type_unresolved`로 거절한다.
- 직접 소비자는 `semantic/ast_assignment_type_fact_owner.pgy`다. Slice base와
  element type을 기존 expression/scope 사실에서 가져와야 한다. backend에서
  타입을 추정하거나 OOB/수명 검사를 생략해서는 안 된다.

## C. 검증 완료라고 아직 말할 수 없는 부분

1. **전체 compiler 입력과 실행 파일의 대응.**
   과거 R8의 207개는 [별도 전수 분석](native_driver_207_errors_2026-11-06.md)의
   고정 결과다. 새 native의 read/lend focused 입력은 둘 다 통과했다.
   새 native × 동일 R8 전체 입력의 최신 결과는 아래 최종 관측란에 기록한다.
   이것과 실제 전체 compiler 실행·무수동 drop·물리 정리 완료는 별도 조건이다.
2. **전체 driver 고정점과 산출 compiler 실행.**
   기존 codegen-tool gen2==gen3와 driver C source equality는 범위가 한정돼 있다.
   이번 감사에서는 전체 driver gen2==gen3를 실행하지 않았다.
3. **전체 native-admitted corpus의 설치 C/LLVM 동등성.**
   이번 19개 허용 frontier를 `tests/cases/**`·`examples/**` 전체 성공으로
   바꾸어 말할 수 없다. 전체 corpus의 실패 집합은 별도 통합 경계에서 실행해야 한다.
4. **전체 수명/저장소 종류.**
   escaping/returned Slice, pool, nested aggregate, closure와 worker/foreign
   boundary까지 모든 정리 경로를 검증한 증거는 없다. 이 항목은 이번 실행에서
   확인한 누수와 구분되는 coverage 공백이다. 이번 사례를 수정했다고 자동으로 닫지 않는다.
5. **거절의 이유까지 확인하는 안전 회귀.**
   3개 negative는 네 경로 모두 거절됐다. live Slice 중 ArrayDrop은 양쪽
   frontend가 storage invalidation으로 거절했다. 반면 capture negative는
   installed에서 `initializer_type_unresolved`로 먼저 중단됐고, retaining Join은
   native MIR의 `invalid-empty-birth`가 먼저 나왔다. 이 둘은 의도한 최종
   safety rule을 실제로 실행했다는 증거가 아니다. valid sibling과 해당
   diagnostic/owner를 함께 확인하는 기존 안전 gate를 보강해야 한다.
6. **같은 공개 commit의 CI.**
   수정 소스·native·installed artifact가 일치하는 같은 commit에서 bootstrap과
   installed-driver CI가 실제 실행돼야 한다. 본 감사는 commit/push/CI를 수행하지 않았다.

### 최신 전체 canonical 관측

- 새 native `b25581e...`에서 안전한 default read/lend 입력은 오류 없이
  C를 생성했다. 이전 설치 launcher/native `937be0...`는 이 둘을 거절했다.
- 같은 고정 R8 전체 입력을 일반 native pipeline과 모든 원래 진단을 유지해
  실행했으나 300.17초의 focused budget 안에 완료하지 못했다. process group을
  종료하고 회수했다. C 산출물과 최종 structured diagnostic array는 없다.
- 따라서 최신 전체 오류 수는 **미확정**이다. 과거 207개나 오류 0개로
  대신 기록하지 않는다. 이 timeout은 의미 판정이나 안전성 결과가 아니다.
  입력을 줄이거나 허용 시간을 늘려 성공으로 바꾸지 않았다.

## D. 현재 결함으로 다시 세지 않을 항목

- generic scalar member, generic vessel member, generic Option 반환,
  nominal/ability/subject/vessel의 이번 frontier는 네 경로 모두 성공했다.
- `first_callee`, `late_callee`, 독립 Array 반환, Join replacement와 기존
  함수 지역 collection 입력도 값 실행은 네 경로 모두 성공했다. 이 값 성공만으로
  해당 프로그램의 모든 allocation을 추적해 해제했다고 주장하지 않는다.
- 새 native의 지역 scalar Array literal은 sanitizer 관측도 정상이다.
- raw pointer escape를 무조건 허용하지 않는 것은 안전 경계다.
- primitive capture만 callable-let에 허용하는 현재 closure 범위, Int32/Float32
  선택, 정의된 Int wrap, ListGet의 값 읽기를 새 memory-safety 버그로 세지 않는다.
  escaping/aggregate closure, 재귀 enum·함수값 container의 과거 이식 보고는
  최신 고정 반례를 확보해야 별도 지원 공백으로 판정할 수 있다.
- D1 경고 census나 정책 변경은 시작하지 않았다. 값 의미론을 바꾸어 검사기를
  통과시키는 것은 이 감사의 수정안이 아니다.

## 전달할 수정 순서와 완료 기준

1. F01–F05의 물리 정리를 **typed 소유권/수명 -> MIR 정리 의무 -> emitter ->
   runtime** 전체에서 연결한다. 일반 String·field와 함수 지역/반환 collection을
   loop-only Join 정리와 구분해 검증한다. scope마다 무조건 free하지 않는다.
2. F06–F09는 기존 공통 판정/일반 emitter에 기능을 채운다. C-only 특례나
   native fallback을 추가하지 않는다. 타입/ABI/수명 사실이 없으면 계속 거절한다.
3. 각 작은 입력과 안전 반례가 안정된 뒤 전체 canonical, 산출 compiler 실행,
   전체 driver 고정점, 설치 corpus, 동일 commit CI를 차례로 실행한다.
4. 같은 source/input/binary ledger와 실행 결과로 닫힌 항목만 완료 처리한다.
   SoT 레지스트리 표기·문서 수·fixture 수는 완료 근거로 사용하지 않는다.

원시 로그와 재현 도구: `D:\PergyraLang\.tmp\functional-gap-audit-20261106\`.
최종 데이터 목록/manifest는 같은 디렉터리의 `results/`에 보존한다.
[전체 116개 주 관측 목록](compiler_functional_gaps_2026-11-06.tsv)은
88개 기능 경로와 28개 물리 정리 관측을 포함한다. 숫자는 검증 범위이며
서로 다른 버그 수나 V1 진척률이 아니다.
이 문서와 전체 관측 목록을 페르기라 메인 채팅에 전달했다. Production 코드
수정·staging·commit·push는 이 lane에서 수행하지 않았다.
