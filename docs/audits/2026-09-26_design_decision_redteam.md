# 설계 결정 레드팀 — 2026-09-26

기준 HEAD: `463406e9` (origin/main). 이 문서는 읽기 전용 감사 기록이다. 컴파일러
의미론, 진행도, 레지스트리 상태를 소유하지 않는다. 결정권은 사람에게 있다.

## 목적과 방법

language-word 레지스트리를 닫으면서 "self-host 파서가 단어 식별자로 읽지만 기본
경로는 지원하지 않는" 표면이 여럿 드러났다. 이 문서는 그 표면과 주변 결정을 하나씩
명시화한다. 항목마다 다섯 가지를 적는다.

1. **명시된 규칙**: 문서나 레지스트리에 적힌 것.
2. **실제 동작**: 직접 재현한 것. 재현 소스는 부록에 있다.
3. **드러난 암시**: 적혀 있지 않은데 동작이 따르고 있는 규칙.
4. **선택지와 대가**: 각 선택이 강제하는 코드, 게이트, 단어 변화.
5. **권고와 그 전제**: 내 권고와, 그 권고가 기대고 있는 가정.

다른 AI에게 같은 항목을 주고 독립적으로 판단하게 하면, 결론이 갈리는 항목이 곧
사람이 결정해야 할 지점이다.

### 이 문서를 쓴 AI 쪽 전제 (편향 공개)

- fail-closed를 기본값으로 둔다. 확실하지 않으면 거부를 권한다.
- 단어와 표면을 줄이는 쪽을 선호한다. docs/42의 "단어는 fact owner와 의무를 가져야
  한다"는 규칙을 권위로 취급했다.
- 네이티브 컴파일러를 오라클로 본다. 오라클 자체가 틀렸을 가능성은 R1과 R9에서만
  다뤘다.
- 재현은 Windows에서 네이티브 C 백엔드로만 했다. LLVM 레그와 Linux는 확인하지
  않았다.

## 독립 검증 정정 — 2026-09-26

이 절은 아래 원문의 관측과 권고를 보존하되, 현재 소스와 추가 실행으로 확인한
사실관계를 우선한다. 원문 작성 기준은 `463406e9`이고, 독립 검증 시점의 실제
`HEAD == origin/main`은 `df097da5`다. 최초 독립 검증 시 checkout에는 compiler 작업
249건이 남아 있었고, 후속 재검증 시에는 282건으로 늘어났다. 따라서 이 검증은 clean
exact-HEAD나 CI 증거가 아니다. 네이티브 바이너리는 그
현재 트리에서 다시 빌드했지만 설치된 `pgy-self-driver.exe`는 9월 21일 산출물이고
현재 소스와 동기화됐다는 receipt가 없다. 따라서 설치 드라이버 관측은 회귀
반례이지 현재 self-host 구현의 완료 증거가 아니다.

또한 "모든 항목을 native 컴파일러로 직접 실행했다"는 표현은 정확하지 않다. R3,
R6, R8, R9 및 R10의 일부 근거는 source/document/gate 정적 검사이고, default-route
관측은 설치된 self-host driver를 사용했다. 아래 표가 현재 검증 범위를 소유한다.

| 항목 | 현재 판정 | 독립 검증과 정정 |
|---|---|---|
| R1 | `PARTIAL` | 현재 native C/LLVM은 `Duration -> Long`을 같은 진단으로 거부한다. 설치 self-host C는 컴파일해 `2`를 출력했지만 stale binary다. `ast_print_expr.c`가 duration kind/unit을 출력하지 않는 손실은 source로 확인됐다. 현재 self-host source로 재빌드한 driver 증거는 아직 없다. |
| R2 | `VERIFIED` | native C/LLVM 모두 `secure` 함수 안의 `join with any`를 오류와 경고 없이 받았다. 같은 두 실행 파일을 반복 실행했을 때 두 백엔드 모두 둘 이상의 승자를 실제로 출력했다. 실행마다 빈도는 달라지므로 특정 분포를 계약으로 기록하지 않는다. 효과 누락과 관찰 가능한 비결정성이 모두 재현됐다. |
| R3 | `PARTIAL` | backend의 AST 직접 조회는 실제 결함이다. 다만 "semantic과 MIR에 정책 코드가 없다"는 원문은 틀렸다. RIR은 `RIR_FACT_INTENT_POLICY`의 `rollback` fact를 만들고, `mir_cleanup.c`는 이를 `RollbackPolicy` cleanup instruction으로 운반한다. 문제는 이 fact가 없다는 것이 아니라 C/LLVM backend가 여전히 AST accessor를 직접 읽어 완전한 MIR-only 단일 소유자가 되지 못했다는 점이다. |
| R4 | 동작 `VERIFIED`, 정책 `DECISION` | 현재 native C/LLVM은 각각 20회 모두 `1 3 2 4`를 출력한다. 그러나 legacy source emitters인 `transpiler_select.c`와 `llvm_stmt_select.c`에는 atomic round-robin 구현이 있고, 실제 production MIR 출력은 source-order readiness chain이다. legacy emitter는 현재 production 의미 소유자가 아니므로 두 구현의 존재만으로 사용자-visible backend divergence라고 부르면 과장이다. 정확한 결함은 production 정책과 beta 약속을 고정하는 순서 oracle이 없고, 퇴역 경로의 다른 정책도 음성 게이트로 제거되지 않았다는 점이다. `llvm_smoke.sh`는 네 값을 순서 없이 grep하므로 이 차이를 잡지 못한다. |
| R5 | `PARTIAL`, divergence 추가 | 현재 native C/LLVM은 `pin = 3`을 같은 parse 오류로 거부한다. stale 설치 self-host C는 이를 받아 `3`을 출력했다. native 문법 함정뿐 아니라 parser divergence 반례이지만, 현재 self-host source 재빌드 전에는 현행 default-route 판정으로 승격하지 않는다. |
| R6 | `VERIFIED` | `every`/`continuous`는 keyword registry에 있으나 `unavailable` 집합으로 gate되며 native parser가 vision surface로 명시적으로 거부한다. 유지/삭제는 여전히 설계 결정이다. |
| R7 | `PARTIAL` | generic `is`는 현재 native C/LLVM에서 `true`, `false`를 출력하고 String target은 scalar-only 진단으로 거부된다. 다만 이것이 `reflect`와 동일 계약이라는 결론은 증명되지 않았고, 현재 `reflect`는 `projection` 결과를 내는 별도 질의다. 현재 증거는 `is`의 scalar compile-time predicate 동작까지만 고정한다. |
| R8 | `NOT A NEW DEFECT` | 단어 재사용 자체는 사실이지만 `docs/reflect_operator_design.md`가 domain projection과 reflect 결과의 같은 이름 사용을 "vocabulary economy" 결정으로 명시한다. 레지스트리 TYPE 문맥 누락 여부는 별도 정합성 문제일 수 있으나, 단순 rename 권고는 현재 설계 SoT를 다시 여는 제안이지 발견된 우발적 누수가 아니다. |
| R9 | `VERIFIED` | native `AST_LET_DECL` printer는 `is_mutable`을 출력하지 않고 self-host printer는 `mut `를 출력한다. duration도 숫자 텍스트만 남긴다. 현재 parser parity는 이 손실 텍스트를 비교하므로 의미 동등성 오라클이 될 수 없다. |
| R10 | `VERIFIED`, 범위 제한 | `PGY_CAP_GRANT=io_read`에서 native C의 `Print("print ok")`는 첫 바이트 전에 `capability-denied`로 멈추지만 설치 self-host C는 `print ok`를 출력한다. 현재 source에는 `pgy_host_now_ms`도 존재하며, `StringRuntimeCPrintBlock`과 `HostIORuntimeCClockBlock`을 포함한 private runtime 방출에는 `pgy_cap_require_export`가 없다. 다만 설치 driver는 9월 21일 산출물이므로 이 실행 divergence와 현재 dirty source 사이의 artifact receipt는 없다. shared native runtime에 검사가 있다는 사실은 source-generated private runtime의 정적 우회를 닫지 않는다. |
| R11 | 사실 `VERIFIED`, 정책 `DECISION` | native runtime과 self-host private runtime source 모두 Windows에서 `GetTickCount64()`, POSIX에서 `CLOCK_REALTIME`을 사용하고 결과를 Pergyra `Int` 폭으로 줄인다. 따라서 플랫폼별 clock domain과 약 24.8일 이후 signed wrap 지적은 맞다. 다만 `Now()`를 monotonic으로 유지할지 wall clock으로 바꿀지는 버그 수정이 아니라 언어/API 결정이다. |
| R12 | `VERIFIED`, 낮은 우선순위 | native parser는 첫 오류만 `parser->error_msg`에 보관하고, 복구 뒤 오류는 즉시 stderr에 출력한다. driver가 보관된 첫 오류를 나중에 출력하므로 두 번째 오류가 먼저 보일 수 있다는 지적은 source contract와 일치한다. 의미ㆍ보안 결함은 아니며 진단 수집 순서의 UX 문제다. |
| R13 | 동작 `VERIFIED`, 새 결함 아님 | `Int`의 wrapping i32 의미는 `docs/semantics/11_arithmetic_ub_model.md`와 C `-fwrapv`/LLVM non-`nsw` gate가 이미 명시적으로 소유한다. `2147483647 + 1`의 `-2147483648` 결과도 고정 fixture가 있다. 누적 예산에 이 타입을 쓰는 위험은 타당하지만, 현재 동작 자체는 암묵적 누수가 아니라 의도된 계약이다. 검사 산술의 `Result` API 추가는 별도 표면 결정이다. |
| PP-003 | `PARTIAL / PROPOSAL` | `TryMakeDirs`와 listen/accept가 현재 stdlib에 없다는 점은 확인했다. 외부 하네스의 114--287ms 수치는 이 저장소에서 재측정하지 않았고, 전달 문서 간 범위도 일치하지 않는다. stdlib 채택과 성능 효과는 별도 owner/gate가 필요한 제안이다. |

이 정정으로 release/security triage에서 P0 후보로 둘 항목은 R1의 손실 오라클과
default-route 타입 보존, R2의 effect 누락, R4의 production 정책 oracle 부재와 legacy
경로 잔존, R9의 parser parity 오라클, R10의 source-generated runtime capability
우회다. 이것은 위험도 분류이며 현재 활성 self-host executable rung을 대체하거나
닫는 증거가 아니다. 각 항목은 구현에 들어가기 전에 owner, production entrypoint,
현재 source로 만든 실행 artifact, falsifying gate, stop/release criterion을 별도 objective
card로 고정해야 한다. R3은 같은 우선순위 후보인 MIR-only 소유권 결함이지만 이미
존재하는 RIR/MIR fact를 확장ㆍ소비해야 하며 새 fact 체계를 병렬로 만들면 안 된다.
R6--R8, R11--R13과 PP-003도 현재 활성 self-host executable rung을 선점하지 않는
별도 결정/후속 범위다.

## 현재 HEAD 재검토 — 2026-09-27

이 절은 위 독립 검증의 실행 결과를 지우지 않고, 그것이 현재 실행 receipt인 것처럼
읽히는 것을 막는다. 재검토 기준은 `68a82abfe37e7c93c879baa0a6b0baae0cf73c45 ==
origin/main`이다. checkout에는 이 문서와 진행 중 compiler 작업을 포함해 390개 dirty
항목이 있으므로, 아래 판정 역시 clean CI 증거가 아니다. `df097da5..68a82abf`의 변경은
주로 self-host escape, directory walk, zone sync에 집중되어 있다.

| 항목 | 현재 HEAD 분류 | 현재 재검토 범위 |
|---|---|---|
| R1 | `BLOCKED` | native Duration fact와 손실 있는 AST text는 정적으로 남아 있다. 현재 source로 재빌드한 self-host artifact receipt가 없어 default-route 타입 보존의 현행 실행 판정은 열어 둔다. |
| R2 | 정적 누락 `VERIFIED`, 현행 반복 실행 `BLOCKED` | any-join에서 nondeterministic effect를 주입하는 owner 연결은 보이지 않는다. 위 반복 실행은 역사적 증거이며 현재 HEAD에서 재실행하지 않았다. |
| R3 | 정적 우회 `VERIFIED`, migration `BLOCKED` | RIR/MIR rollback fact와 backend AST 직접 읽기가 함께 남는다. 새 fact 체계가 아니라 기존 fact의 마지막 consumer 이관이 필요하다. |
| R4 | `DECISION`, oracle `BLOCKED` | 현재 broad gate는 `1`--`4`의 존재만 확인하고 순서를 고정하지 않는다. production 순서 계약을 먼저 정한 뒤 backend별 literal oracle과 legacy-path negative gate가 필요하다. |
| R5 | `BLOCKED`, 낮음 | native의 문법 함정은 남아 있지만 current self-host divergence는 fresh artifact로 확인되지 않았다. |
| R6 | 정적 상태 `VERIFIED`, `DECISION` | `every`/`continuous`는 no-parser-selector surface로 fail closed한다. 유지/삭제는 별도 vocabulary 결정이다. |
| R7 | `DECISION` | 확인된 범위는 scalar compile-time predicate다. `reflect`와의 통합 계약은 증명되지 않았다. |
| R8 | `DECISION` | `projection` 재사용은 기존 decision record의 의도다. TYPE 문맥 정합성만 별도 검증할 수 있다. |
| R9 | 정적 손실 `VERIFIED`, oracle 확대 `BLOCKED` | native AST text가 `mut`와 Duration kind를 잃는 사실은 남는다. text parity가 의미 parity를 소유한다는 더 강한 주장은 consumer와 structured fact gate가 필요하다. |
| R10 | Print/Now 정적 우회 `VERIFIED`, 실행 `BLOCKED` | private Print/Now helper에는 capability check가 없지만 DirWalk는 현재 canonical `pgy_dir_walk`를 호출한다. 따라서 "private runtime 전체 우회"가 아니라 남은 helper별 범위로 축소한다. |
| R11 | 정적 사실 `VERIFIED`, `DECISION` | platform clock domain과 Int narrowing은 남는다. API 의미 변경은 별도 결정이다. |
| R12 | `BLOCKED`, 낮음 | source의 진단 보관/출력 순서는 지적과 맞지만 현재 HEAD 실행 receipt는 없다. |
| R13 | 의미 계약 `VERIFIED`, consumer 위험 `DECISION` | wrapping i32는 명시된 SoT다. 예산ㆍ카운터 위험은 해당 consumer의 별도 proof가 필요하다. |
| PP-003 | `DECISION` | stdlib 범위 제안이며 현재 executable rung의 결함 증거가 아니다. |

현재 release/security triage 후보로 보존할 것은 R1, R2, R9, R10이다. 그러나 어느
항목도 현재 활성 array-member mutable-place P0나 collection-ownership self-host rung을
대체하지 않는다. R3과 R4도 중요하지만 각각 owner migration과 사용자-visible 순서
정책을 먼저 고정해야 하므로, 이 문서만으로 P0 구현을 시작하지 않는다. 현행 판정을
닫으려면 fresh self-host artifact hash, backend별 독립 oracle, falsifying negative gate를
가진 항목별 objective card가 필요하다.

## 요약

| # | 항목 | 종류 | 심각도 |
|---|---|---|---|
| R1 | 손실 AST 투영이 Duration을 Long 텍스트로 만듦 | 버그 (fail-open 위험) | 높음 |
| R2 | `join with any`의 비결정성이 효과로 선언되지 않음 | 설계 결정 | 높음 |
| R3 | intent rollback fact는 있으나 backend AST 우회가 남음 | 설계 결정 + 구조 부채 | 중간 |
| R4 | `select`: production 순서 oracle이 없고 퇴역 emitter 정책이 남음 | 설계 결정 + 퇴역 경로 잔존 | 중간 |
| R5 | `pin`이라는 이름은 선언은 되는데 대입하면 파싱이 안 됨 | 문법 함정 | 낮음 |
| R6 | `every`/`continuous`를 유지할지 삭제할지 | 설계 결정 | 낮음 |
| R7 | `is`가 scalar compile-time 타입 술어로 동작하지만 계약이 불명확함 | 설계 결정 | 낮음 |
| R8 | `projection`의 의도된 어휘 재사용을 다시 열지 결정 | 설계 재검토 | 낮음 |
| R9 | 파서 동등성 오라클이 손실 있는 텍스트임 | 검증 구조 | 높음 |
| R10 | source-generated private runtime이 capability를 우회함 | 버그 (fail-open) | 높음 |
| R11 | `Now()`가 Windows는 단조 시계, POSIX는 int32로 자른 벽시계 | 설계 결정 + 버그 | 중간 |
| R12 | native가 두 번째 파스 오류를 첫 오류보다 먼저 출력 | 진단 UX | 낮음 |
| R13 | 기본 `Int`가 32비트이고 넘치면 조용히 감김 | 설계 결정 | 중간 |

---

## R1. 기본 경로에서 Duration 타입이 지워진다

- **명시된 규칙**: 네이티브 타입 검사기는 `Duration`을 별도 타입으로 취급한다.
  `let a: Int = 5ms`, `let b: Long = 2min`, `5ms + 3`을 모두 거부한다.
- **실제 동작**:
  - `let b: Long = 2min; Log(b);`
    - `--native-pipeline`: `cannot assign 'Duration' to 'Long'`로 거부.
    - 기본 경로(`--backend=c`, self-host 프런트엔드): 그대로 컴파일된다.
  - 원인: 네이티브 `--ast`가 duration 리터럴을 `Let: d : Duration = 5000000L`처럼
    Long 리터럴 텍스트로 출력한다. self-host 파서도 같은 텍스트를 만들고(b186c7da),
    self-host 의미 단계는 `5000000L`을 Long으로 타입 매긴다.
  - b186c7da 이전에는 `2min`을 `2`로 읽어서, 값까지 틀린 채로 역시 받아들였다.
- **드러난 암시**: "AST 텍스트가 같으면 의미도 같다." 파서 패리티 게이트는 이
  가정 위에서 duration 픽스처를 통과시켰다.
- **선택지**
  - A. self-host가 duration 리터럴을 `surface_not_covered`로 거부한다. 즉시
    fail-closed가 되지만, `duration_literals` 파서 픽스처는 거부 행으로 옮겨야 한다.
  - B. 표현식 그래프와 AST 텍스트에 Duration 리터럴 노드를 따로 둔다. 네이티브
    출력기와, duration이 나오는 기준선이 바뀐다.
  - C. 언어 결정으로 `Duration`을 `Long` 별칭으로 만든다. 네이티브 타입 검사기와
    런타임 API 계약을 바꾸는 일이다.
- **권고**: A를 지금 적용하고, B를 대체 rung으로 진행한다. C는 사람이 결정할 일이다.
  전제: Duration을 별도 타입으로 둔 것은 의도된 설계다.

## R2. `join with any`의 비결정성이 효과로 선언되지 않는다

- **명시된 규칙**
  - docs/181 R3: "최초 give 승리". 비결정 승자임을 문서가 인정한다. 목격자
    픽스처는 같은 give 값을 쓰거나 n=1로 두어 출력을 고정한다.
  - 효과 어휘에 `nondeterministic`이 있다.
  - `type_effect_mask_conflicts`(src/semantic/type_effects.c)는 `secure`와
    `remote`/`collapse`/`nondeterministic`의 충돌을 거부한다.
- **실제 동작**: `func Pick(xs) -> Int with effects secure`의 본문에서 `join with any`를
  쓰면 경고 없이 컴파일된다. 6회 실행 결과는 `1`이 4번, `3`과 `4`가 한 번씩이었다.
- **드러난 암시**: "비결정성은 런타임 성질이지 효과가 아니다." 그래서 `secure`
  함수가 비결정적인 결과를 조용히 반환한다.
- **선택지**
  - A. any-join을 쓰려면 둘러싼 callable이 `nondeterministic`을 선언해야 한다.
    `secure`와는 기존 규칙대로 충돌한다.
  - B. any를 결정적으로 만든다. 예를 들어 완료된 것 중 가장 작은 인덱스를 고른다.
    지연 시간 이점이 줄고, `any`라는 이름과 의미가 달라진다.
  - C. `nondeterministic`은 외부 출처에만 쓰는 효과라고 문서화하고, any-join을
    예외로 둔다.
- **권고**: A. 전제: 효과 어휘는 관찰 가능한 비결정성을 빠짐없이 표시하려는
  목적이다. 이 전제가 틀렸다면 C가 맞다.

## R3. intent rollback fact가 있지만 backend AST 우회가 남아 있다

- **명시된 규칙**
  - `rollback: full | current | none`이 문법이다. 생략하면 `full`이다
    (src/parser/ast_intent_constructors.c:29).
  - docs/193: 백엔드는 MIR만 소비한다.
- **실제 동작**
  - RIR(`rir_builder_intent.c:71`)과 두 백엔드(`transpiler_intent_emit.c:58`,
    `llvm_intent.c:73`, cleanup 방출기)가 `ast_intent_decl_rollback_policy(node)`를
    AST에서 직접 읽는다.
  - RIR은 `RIR_FACT_INTENT_POLICY`의 `rollback` fact를 만들고 MIR cleanup은 이를
    `RollbackPolicy` instruction으로 운반한다. 따라서 "MIR에 없다"는 최초 grep
    결론은 잘못이었다.
  - 그럼에도 두 backend와 cleanup emitter가 AST accessor를 직접 읽으므로 이 fact가
    마지막 codegen consumer를 소유하지 못한다.
  - self-host는 `current`/`none`을 거부한다.
  - 사용처:
    - `none`: `intent_observability_rollback` 테스트 케이스
    - `current`: `abi_pipeline/intent_failure_abi`, `examples/bsd_packet_server`
- **드러난 암시**: "rollback fact가 있어도 backend가 AST를 다시 읽어도 된다."
  실제로는 어떤 보상(compensate)이 실행되는지를 바꾸는 관찰 가능한 의미이므로,
  기존 fact owner가 마지막 consumer까지 소유해야 한다.
- **선택지**
  - A. 정책 셋을 모두 유지한다. 의미 소유자와 MIR fact, self-host 지원을 추가하고,
    백엔드는 AST 읽기를 삭제한다.
  - B. `full`만 남긴다. `current`/`none` 단어를 삭제하고 테스트 1개와 예제 1개를
    고친다.
  - C. 소유자가 생길 때까지 네이티브에서도 `current`/`none`을 거부한다.
- **권고**: 사람이 결정한다. `current`/`none`에 실제 사용 사례가 있으면 A, 없으면 B가
  싸다. 어느 쪽이든 백엔드가 AST를 읽는 경로는 없어져야 한다. 전제: MIR-only 규칙은
  예외 없이 적용된다.

## R4. `select`: 베타 표면인데 기본 경로가 지원하지 않고, 공정성 규칙이 암시적이다

- **명시된 규칙**
  - docs/05와 docs/107은 `select`를 베타 안정 표면으로 나열한다.
  - docs/05: "현재 FIFO/런타임 동작을 넘는 공정성은, 이름 붙은 backend-compare
    픽스처가 덮지 않는 한 베타 약속이 아니다."
- **실제 동작**
  - 기본 경로는 `select`를 `surface_not_covered`로 거부한다.
  - `select_fairness` 케이스(채널 a에 1과 3, b에 2와 4)는 추가 검증에서 두 backend
    각각 20회 모두
    `1 3 2 4`를 출력했다. 즉 소스 순서 우선이고, a가 비워진 뒤에야 b가 읽힌다.
  - 이 케이스에는 `expected.stdout`이 없다. C와 LLVM이 서로 같다는 것만 확인한다.
  - legacy source emitters에는 atomic round-robin 코드가 있지만 production MIR
    lowering은 source-order readiness chain을 방출한다. 현재 gate는 이 정책 분기를
    잡지 못한다.
- **드러난 암시**
  - "현재 런타임 순서가 곧 정책이다."
  - 이름은 fairness인데, 고정하는 동작은 소스 순서 우선순위다. 소스 순서상 뒤에 있는
    case는 앞 case에 데이터가 계속 있으면 굶을 수 있다.
- **선택지**
  - A. 규칙을 명시한다. 예: "준비된 case 중 소스 순서 첫 번째, 없으면 default".
    `expected.stdout`으로 고정하고 픽스처 이름도 바꾼다.
  - B. 선택 순서를 명시적으로 "미지정"이라고 두고, 픽스처가 순서에 기대지 않게
    바꾼다.
  - C. 진짜 공정성(라운드로빈 등)을 구현한다.
- **추가 결정**: `select`가 베타 표면이면 기본 경로 지원이 베타 차단 요소다. 아니면
  베타 목록에서 내려야 한다.
- **권고**: A와 "베타 차단 요소로 명시". 전제: 결정적인 순서가 디버깅에 유리하다.

## R5. `pin`이라는 이름은 선언되는데 대입이 파싱되지 않는다

- **명시된 규칙**: 레지스트리에서 `pin`은 contextual 단어이고 문맥은 STATEMENT다.
  네이티브는 `pin` 뒤에 `(`, `;`, EOF가 아닌 것이 오면 pin 블록으로 읽는다.
- **실제 동작**: `let mut pin: Int = 1; Log(pin);`은 컴파일된다.
  `let mut pin: Int = 1; pin = 3;`은 `Unexpected token in expression`으로 실패한다.
- **드러난 암시**: "contextual 단어는 이름으로 써도 된다." 이 약속이 대입문에서만
  깨진다.
- **선택지**
  - A. pin 블록의 판정을 `pin NAME as`로 좁힌다. 두 파서 모두에 적용하면 `pin`은
    완전한 이름이 된다.
  - B. `pin`을 reserved로 바꾼다. 이러면 `let pin` 자체가 금지된다.
  - C. 현상 유지하고 문서화한다.
- **권고**: A. 전제: 단어를 예약하는 비용(사용자에게서 이름을 빼앗음)이 문법을
  좁히는 비용보다 크다.

## R6. `every`/`continuous`를 유지할지 삭제할지

- **명시된 규칙**
  - docs/181 §2: role reactive parallel은 비전 표면이다.
  - 레지스트리는 이제 두 단어에 구현이 없다고 선언한다(b9e89bea).
- **실제 동작**: 두 파서 모두 `parallel`에서 거부한다. 두 단어를 읽는 파서는 없다.
- **선택지**
  - A. 유지한다. docs/181의 rung이 열릴 때 쓴다. 비용은 레지스트리 행 2개다.
  - B. rung이 열릴 때까지 레지스트리에서 삭제한다.
- **권고**: 사람이 결정한다. docs/42를 문자 그대로 읽으면 B다. 하지만 비전 표면
  거부 진단도 의무의 하나로 볼 수 있어서 A도 방어할 수 있다.

## R7. `is`와 `reflect`가 컴파일 타임 타입 질의로 겹친다

- **실제 동작**
  - `func Probe<T>(v: T) -> Bool { return v is Int; }`에서 `Probe(3)`은 `true`,
    `Probe(3.5)`는 `false`다. 단형화 때 상수로 접힌다.
  - `s is String`은 "Int, Long, Float, Bool 대상만 지원"이라는 이유로 거부된다.
- **드러난 암시**: `is`는 런타임 검사가 아니라 정적 타입 술어다. 그런데 문서는 이
  점을 말하지 않는다.
- **선택지**
  - A. `is`를 "컴파일 타임 술어"로 문서화하고 스칼라 전용으로 둔다.
  - B. `reflect` 질의로 합친다.
  - C. 명목 타입까지 확장한다.
- **권고**: A. 전제: 지금 필요한 건 확장이 아니라 정확한 이름표다.

## R8. `projection`의 의도된 어휘 재사용을 다시 열지 결정한다

- **실제 동작**
  - world/zone에서: `state v: zone battle projection scoreboard`.
  - reflect 결과 타입으로: `let p: projection = reflect Account;`.
  - 레지스트리 행의 문맥은 `CLAUSE | ZONE_BODY`이고, 타입 위치 사용은 선언되어 있지
    않다.
- **기존 결정**: `docs/reflect_operator_design.md`는 두 표면이 같은 이름을 쓰는 것을
  이미 인식하고 vocabulary economy를 이유로 선택했다. 따라서 재사용 자체는 새
  결함이 아니다. 다만 타입 위치가 keyword registry 문맥과 정합적인지는 별도 검증
  대상이다.
- **선택지**
  - A. reflect 결과 타입의 이름을 바꾼다(예: `TypeProjection`).
  - B. 레지스트리에 TYPE 문맥을 추가하고, 두 의미를 명시적으로 문서화한다.
- **권고**: 현 상태 유지. 이름을 바꾸려면 기존 decision record를 명시적으로 다시
  열고, 사용자 코드ㆍ타입 시스템ㆍ레지스트리ㆍ진단을 함께 이관한다.

## R9. 파서 동등성 오라클이 손실 있는 텍스트다

- **실제 동작**
  - R1: 네이티브 `--ast`는 Duration 리터럴을 Long 리터럴과 같은 텍스트로 출력한다.
  - `let mut`: 네이티브는 `let mut xs`를 `Let: xs`로 출력하고(mut 소실), self-host는
    `Let: mut xs`로 출력한다. 파서 패리티 픽스처 197개는 `let mut`을 피해서 이 차이를
    보지 못한다.
- **드러난 암시**: "파서 패리티 그린 = 의미 동등." 실제로 게이트가 증명하는 건
  "출력 텍스트 동등"뿐이다.
- **선택지**
  - A. 네이티브 출력기가 모든 의미 fact를 찍게 한다. mut, 리터럴 종류, 타입 인자
    등이다. 기준선이 대량으로 바뀐다.
  - B. 텍스트 대신 구조화된 fact(표현식 그래프, 노드 종류)를 비교하는 게이트를 둔다.
  - C. 텍스트 게이트는 유지하고, 손실되는 fact마다 별도의 의미 패리티 행을 둔다.
- **권고**: B. 전제: 오라클과 피검사자가 같은 손실을 공유하면 게이트가 그 손실을
  볼 수 없다.

## R10. 기본 경로 런타임에는 capability 검사가 없다

source-generated private runtime과 shared native runtime을 대조하다가 발견했다.

- **명시된 규칙**: native 의미 단계는 빌트인이 요구하는 capability를 기록한다
  (`Now`는 clock). native 런타임도 실행 시점에 다시 검사한다.
  `src/runtime/*.h`에 `pgy_cap_require_export` 호출이 40곳 있다.
  `pgy_now_ms`는 `PGY_CAP_CLOCK`을, 파일 IO는 read/write를 요구한다.
- **실제 동작**: self-host가 방출하는 host IO 헬퍼(`host_io_runtime_owner.pgy`)에는
  capability 검사가 없다. self-host도 의미 단계의 capability 행
  (`builtin_capability_projection_owner.pgy`)은 native와 같다. 현재
  `HostIORuntimeCClockBlock`은 `pgy_host_now_ms`를 방출하고,
  `StringRuntimeCPrintBlock`은 private `pgy_print`를 방출하지만 둘 다
  `pgy_cap_require_export`를 호출하지 않는다. 설치 driver 실행 반례는 현재 dirty
  source와 동기화된 artifact receipt가 없으므로 현재 source의 실행 증거가 아니라
  default-route 회귀 증거로만 취급한다.
- **드러난 암시**: "capability는 컴파일 타임 사실이면 충분하다." 반면 native는
  런타임 샌드박스까지 둔다. 같은 프로그램이 capability를 막은 실행에서 native는
  멈추고 기본 경로는 계속 돈다.
- **선택지**
  - A. self-host 헬퍼에 같은 런타임 검사를 넣는다. capability 런타임을
    self-host 출력에 포함해야 한다.
  - B. capability를 컴파일 타임 전용으로 정하고 native 런타임 검사를 걷어낸다.
  - C. 차이를 유지하되 문서와 게이트로 고정한다.
- **권고**: 사람이 결정한다. 샌드박스가 실제 보안 경계라면 A, 진단용이라면 B가
  싸다. 어느 쪽이든 두 경로가 다르게 동작하는 지금 상태를 약속으로 둘 수는 없다.
  현재 native capability negative gate가 실제 실행 경계로 사용되므로 A를 기본
  복구 방향으로 본다.

## R11. `Now()`의 뜻이 플랫폼마다 다르다

하네스 세션이 `df097da5` 이후 알려 온 관찰을 코드로 확인했다.

- **명시된 규칙**: `Now`는 `Int`를 반환하는 빌트인이다(native
  `type_checker_builtins_stdlib_body.c`, self-host `Now^Int^none`). 무엇을 재는지는
  문서에 없다.
- **실제 동작**: native `pgy_now_ms`(`src/runtime/pgy_runtime_io_qubit_inline.h`,
  `pgy_runtime_lib_core_exports.h`)는 플랫폼마다 다른 시계를 쓴다.
  - Windows: `GetTickCount64()`, 부팅 후 단조 시계다.
  - POSIX: `CLOCK_REALTIME`, epoch 기준 벽시계다.
  - 둘 다 `int32_t`로 자른다. epoch 밀리초(약 1.8e12)는 int32에 들어가지 않으므로
    POSIX 값은 49.7일 주기로 감기고 음수가 될 수 있다. Windows 값도 부팅 후
    24.8일이 지나면 음수가 된다.
  - 기본 C 경로의 `pgy_host_now_ms`(`df097da5`)는 native와 같은 의미를 목표로
    했으므로 같은 차이를 그대로 가진다.
  - 하네스는 Windows에서 `344011609` 같은 값을 받아서 세션 ID(벽시계)용으로 쓰지
    못하고 C shim을 유지한다.
- **드러난 암시**: "`Now()`는 무언가의 밀리초다." 경과 시간 측정(단조)인지 날짜
  시각(벽시계)인지가 정해지지 않았고, 그 결과 플랫폼이 뜻을 정한다.
- **선택지**
  - A. `Now()`를 단조 시계로 확정한다(POSIX는 `CLOCK_MONOTONIC`). 벽시계는 별도
    빌트인(예: `Long`을 돌려주는 `WallClockMs()`)으로 둔다.
  - B. `Now()`를 벽시계로 확정하고 `Long`으로 바꾼다. 경과 시간용 단조 시계는 따로
    둔다.
  - C. 현 상태를 유지하고 플랫폼 의존이라고 문서화한다.
- **권고**: A. 전제: 기존 사용처(재시도 백오프, 프레임 시간)는 경과 시간을 잰다.
  반환 폭도 함께 정해야 한다. int32 밀리초는 24.8일에서 감긴다.

## R12. native가 두 번째 파스 오류를 첫 오류보다 먼저 출력한다

- **실제 동작**: `reserved_remote.pgy`에서 native는 3행 오류를 먼저, 2행 오류를
  나중에 출력한다. 복구 뒤에 찾은 오류는 파싱 중에 곧바로 stderr로 나간다. 첫
  오류는 `parser->error_msg`에 담겼다가, 드라이버가 파싱이 끝난 뒤
  `parse error in '<file>'`로 출력한다.
- **선택지**: 첫 오류도 발견 시점에 출력하거나, 복구 오류를 모아 두었다가 첫 오류
  뒤에 출력한다. 첫 오류를 `error_msg` 한 칸으로 읽는 소비자(driver_diag, LSP)는
  그대로 둔다.
- **권고**: 뒤쪽 방식. 급하지 않다(하네스 판단도 같다).

## R13. 기본 `Int`는 32비트이고 넘치면 조용히 감긴다

하네스 PP-039(비용 장부 작업)에서 제기됐다.

- **명시된 규칙**: `docs/semantics/11_arithmetic_ub_model.md`는 `+ - *` 넘침을
  정의된 감김으로 정했다. 의미 도메인 이름은 `pergyra.int.wrapping.i32.v1`이다.
  C는 `-fwrapv`, LLVM은 `nsw` 없는 `add`를 쓴다. UB는 아니다.
- **실제 동작**: `2147483647 + 1`은 native와 기본 경로 모두 `-2147483648`이다.
  `CheckedAdd(big, 1)`은 두 경로 모두 `arithmetic-overflow` panic으로 멈춘다.
  `Long`은 64비트다.
- **드러난 암시**: "넘침을 신경 쓰는 코드는 스스로 `Long`이나 `CheckedAdd`를
  고른다." 문서는 감김을 정의했지만, 기본 정수가 조용히 감기는 것이 누적
  카운터(토큰·바이트·비용)에 맞는 기본값인지는 결정한 적이 없다. 하네스는 감긴
  음수 값이 예산 검사를 통과할 수 있는 경로를 찾았다.
- **선택지**
  - A. 현 의미를 유지한다. 대신 `Result`를 돌려주는 검사 산술(예:
    `TryAdd(a, b) -> Result<Int, Overflow>`)을 추가하고, 예산·카운터 문서에서
    `Long`과 검사 산술을 권한다.
  - B. 기본 `+ - *`를 검사 산술로 바꾼다. 넘치면 panic이고, 감김은 명시적인 함수로
    뺀다. 성능 비용과 기존 코드 영향이 생긴다.
  - C. 기본 `Int`를 64비트로 바꾼다. ABI와 모든 i32 레인, 증명 도메인 이름이
    바뀐다.
- **권고**: A. 전제: 감김 의미는 증명(`proofs/CheckedArith.v`)과 두 백엔드
  패리티까지 닫혀 있어서, 바꾸는 비용이 크다. 부족한 것은 panic 대신 `Result`로
  넘침을 다룰 수 있는 경로다. R11의 int32 시계도 같은 뿌리다.

## 외부 요청: Pergyra Agents 하네스 (PP-003 stdlib 범위)

하네스 세션(F:\pergyraAgents, `ec4a756`)에서 전달받았다. stdlib에 추가할지는 사람이
결정한다.

- **디렉터리 생성**: `TryReadFile`/`TryWriteFile`처럼 `Result`를 돌려주는
  `TryMakeDirs(path)`가 필요하다. `PGY_IO_ROOT` 격리 규칙도 같이 받아야 한다.
  근거: 하네스 시작 시간의 대부분(114~287ms)이 세션 폴더를 만들려고 Git Bash를
  띄우는 데 쓰였고, C shim(`pa_fs_mkdirs`)으로 바꾸자 38ms가 됐다.
- **서버 소켓**: listen/accept가 없어서 가짜 서버를 Pergyra로 옮기지 못하고 Python으로
  남겼다.
- **벽시계 시각**: 세션 ID처럼 epoch 시각이 필요한 곳에 쓸 빌트인이 없다. `Now()`는
  Windows에서 단조 시계다(R11). 하네스는 `pa_now_ms_text` shim을 유지한다.

## 공통 패턴

1. **소유자 밖의 결정.** R2(비결정성), R3(rollback), R4(select 순서)에서 관찰 가능한
   의미가 효과, fact owner, 적힌 규칙이 아니라 런타임이나 백엔드 동작으로 정해진다.
2. **손실 있는 투영을 권위로 사용.** R1과 R9에서는 텍스트 투영이 같다는 이유로
   의미가 같다고 받아들였다.
3. **단어 하나에 계약이 둘.** R5(이름이자 블록 머리)와 R8(두 개념).

## 부록: 재현 소스

Windows 네이티브 C 백엔드로 실행했다: `bin/pgy.exe FILE --native-pipeline --backend=c -o OUT`.
기본 경로는 `--native-pipeline`을 빼고 `PGY_SELF_DRIVER_BIN=bin/pgy-self-driver.exe`로
실행했다.

```pergyra
// R1: native refuses, default route compiles
func Main() -> Void {
    let b: Long = 2min;
    Log(b);
}
```

```pergyra
// R2: nondeterministic result from a `secure` callable
func Pick(xs: Array<Int>) -> Int with effects secure {
    let r: Int = parallel (x in xs) join with any { give x; };
    return r;
}
func Main() -> Void {
    let xs: Array<Int> = [1, 2, 3, 4];
    Log(Pick(xs));
}
```

```pergyra
// R5: `pin` is a legal binding name, but assigning to it does not parse
func Main() -> Void {
    let mut pin: Int = 1;
    pin = 3;
    Log(pin);
}
```

```pergyra
// R7: `is` folds per monomorphization
func Probe<T>(v: T) -> Bool {
    return v is Int;
}
func Main() -> Void {
    Log(Probe(3));
    Log(Probe(3.5));
}
```

R4는 `tests/cases/backend_compare/select_fairness/main.pgy`를 그대로 5번 실행했다.
