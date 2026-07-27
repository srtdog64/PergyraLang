# Pergyra-Native Self-Host Dogfood Contract

Status: `BRIDGE`

이 문서는 Pergyra 컴파일러가 Pergyra 소스로 작성되었다는 사실만으로
개사료(dogfood)를 완료했다고 오판하지 않기 위한 실행 계약이다.

Pergyra의 self-host는 두 조건을 모두 만족해야 한다.

1. 컴파일러 소스가 Pergyra 문법으로 파싱되고 컴파일된다.
2. 실제 부트스트랩 실행 책임이 Pergyra의 의미 모델인
   `world -> zone -> subject/action -> intent`를 통해 조직된다.

`func`, `struct`, `if`, `while`가 많다는 사실은 결함이 아니다. 순수 계산과
값 표현에는 이들이 올바른 도구다. 결함은 권한, 자원 수명, 상태 전이,
단계 오케스트레이션 같은 Pergyra 고유 책임까지 일반 함수 호출 그래프로
우회하면서, 별도 파일의 `world`/`zone`/`intent` 선언을 개사료 증거로
세는 것이다.

## 현재 관측된 상태

2026-07-27 현재 실제 부트스트랩 진입점은
`src/self_hosted/compiler/driver_bootstrap_main.pgy`이다. 이 파일은
`driver_rung2_owner.pgy`와 `driver_rung2_execution_owner.pgy`를 import한다.
source/MIR 생성 및 기존 C 호환 mode는 아직 다음 일반 함수들로 분기한다.

- `CompileSourceToCVerified`;
- `CompileSourceToMirJsonFileVerified` 및 pressure-observed 변형;
- `CompileMirJsonToCVerified` 및 observed 변형.

반면 `--mir-json-backend=c|llvm` mode는 이제
`DriverRung2Execution.EmitDirectMir` action을 호출한다. 이 action이 request를
typed target projection으로 admit하고, 기존
`CompileMirJsonToDirectBackendVerified` owner를 정확히 한 번 소비하고,
artifact identity를 확인한 뒤 출력 파일을 쓴다. `Main`의 target-fact 생성,
direct backend 호출, direct-mode `WriteFile` 우회는 삭제됐다.

반면 `PgyCompilerWorld`와 `CompilePergyraProgram`을 선언한
`src/self_hosted/compiler/world.pgy`는 이 진입점에서 import되거나 호출되지
않는다. 따라서 `world.pgy`가 선언한 `world`/`zone`/`subject`/`action`/`intent`
표면은 아키텍처 계약과 파서/정적 게이트에는 쓰이지만, 아직 부트스트랩
실행의 루트는 아니다. 별도 direct-MIR execution subject/action은 아래처럼
실제 진입점에서 도달 가능하다.

재귀 import 감사에서 bootstrap closure는 396개 파일이고 missing import는
0이었다. fixture/generated/probe를 제외한 reachable source는 395개다. 그
reachable 집합의 선언은 `func` 2,643개, `struct` 175개, `enum` 3개,
`subject` 1개, `action` 1개다. `world`/`zone`/`intent`/`role`/`ability`/`effect`는
아직 0개다. 즉 첫 production subject/action은 도달 가능해졌지만 전체
실행 그래프의 대부분은 책임에 맞는 `func`/`struct` 계산 소유자다.

fixture/generated/probe를 제외하고 Pergyra-native 구성체를 선언한 production
파일은 현재 다음 네 개다.

- `compiler/driver_rung2_execution_owner.pgy`: subject 1, action 1;
  direct-MIR production mode에서 `REACHABLE`;

- `compiler/world.pgy`: world 1, zone 36, subject 32, action 16, intent 10;
- `compiler/stage_intents.pgy`: intent 4;
- `compiler/authority_owner.pgy`: role 4, ability 4.

뒤의 세 파일은 여전히 bootstrap에서 unreachable이다.

`world.pgy`의 16개 action은 19개의 `Compiler*Ready()` 호출/결합만 반환한다.
world/stage-intent closure에는 production `CompileSource*`, `CompileMir*`,
`ParseRootProgramArtifact`, semantic/MIR/backend artifact 호출이 없다. 따라서
이들 world/stage-intent action/intent의 bootstrap call site는 0개다. 별도
direct-MIR execution action의 call site는 1개다. world 전체를 성급하게
import하면 driver와 겹치지 않는 39개 파일과 약 5,919 LOC의 test-harness
inventory까지 추가되므로, import 수 자체를 늘리는 방식도 금지한다.

`bin/pgy.exe src/self_hosted/compiler/world.pgy --emit-c`도 현재 exit 1,
6 errors/5 warnings다. 여섯 오류는 `SemanticVerdictZone`, `EmissionZone`,
`ParityZone`을 쓰는 `Check`, `Emit`, `Prove`, `MiddleEnd`, `Backend`,
`SelfProof` step에 필요한 `authorized by` 주체가 없어서 발생한다. 그러므로
현재 AST-only compiler-world gate는 semantic compile이나 executable
reachability 증거가 아니다. root intent takeover 전에 이 파일의 C emission을
필수 green gate로 올려야 한다.

첫 execution owner의 ABI 전제는 별도 fixture로 확인했다.
`tests/self_hosted/parity/driver_execution_action_abi_parity.sh`는 subject action,
aggregate request, enum stage를 포함한 aggregate result,
`with caps io_read, io_write`, action 내부 `WriteFile`/`ReadFile`을 한 경로에서
C/LLVM으로 컴파일하고 실행한다. 양쪽 모두 `ok / artifact-written / 17`과
동일한 `driver-action-abi` 파일을 만들었다. 이것은 다음 rung을 열어 주는
ABI 증거다. production reachability 자체는 별도의
`driver_rung2_execution_action_gate.sh`와 실제 current-driver compile 및
one-MIR C/LLVM 실행 gate가 증명한다.

production action을 처음 연결했을 때 native C emitter가 hosted method body를
전역 함수 prototype보다 먼저 방출하는 일반 결함도 드러났다.
`transpiler.c::emit_program`의 early-eligible function/intent prototype pass를
nominal method body 방출 앞으로 옮겼고,
`subject_action_global_helper`가 nominal-return 전역 helper를 action에서
호출하는 C/LLVM 회귀 사례로 이를 고정한다. action 안에 C prototype이나
중복 helper를 써서 우회하지 않는다.

145개 언어 단어 레지스트리도 이 판정과 분리한다. 단어가 lexer, parser,
LSP, TextMate에서 일치하는 것은 필요한 SoT 증거지만, 그 단어가 실제
컴파일러 책임을 소유한다는 증거는 아니다. fixture, generated projection,
parser probe, readiness shell의 단어 수는 실행 개사료율로 세지 않는다.

## 세 단계의 증거

| 단계 | 의미 | 진척으로 세는 범위 |
| --- | --- | --- |
| `SURFACE` | 선언이 파싱/타입검사/코드생성되고 fixture가 통과한다 | 언어 표면 지원 증거만 |
| `REACHABLE` | 배포 대상 entrypoint의 import/call graph에서 실제 호출된다 | Pergyra-native 개사료 증거 |
| `SUBSTITUTING` | 도달 가능한 구현이 기존 C 소유 경로를 대체하고 oracle parity와 negative gate를 통과한다 | hard self-host 대체 진척 |

한 구성체가 `SURFACE`라고 해서 `REACHABLE` 또는 `SUBSTITUTING`으로
승격하지 않는다. 문서, LOC, 선언 수, readiness `Bool`도 승격 근거가
아니다.

## 구성체별 구현 규칙

### `world`

- 사용자에게 보이는 하나의 컴파일러 조합 루트다.
- resource zone과 root intent를 조합하지만 token, AST, MIR, ABI, target,
  artifact 사실을 복제하거나 재판단하지 않는다.
- 배포 entrypoint가 도달하지 않는 고립된 `world.pgy`는 target shape일
  뿐 실행 루트가 아니다.
- C emitter world와 LLVM emitter world를 따로 만들지 않는다. 하나의
  compiler world가 동일한 MIR/type/ABI/target fact에서 두 projection을
  선택한다.

### `zone`

- 자원, 권한, 수명 또는 관측 가능한 evidence의 실제 경계다.
- 폴더, pass, helper 묶음, 소스 파일 하나를 zone으로 번역하지 않는다.
- zone은 기존 SoT owner의 fact/view를 보유하거나 노출할 수 있지만 그
  사실을 문자열에서 다시 복원하거나 두 번째 authority가 될 수 없다.
- `lexer/`, `parser/`, `codegen/` 같은 물리 디렉터리 수와 zone 수는
  독립적이다.

### `subject`

- identity를 가진 의사결정자, 승인자 또는 오케스트레이터다.
- 큰 데이터 저장소나 모든 컴파일 단계의 god object가 아니다.
- 내부 계산은 기존 typed owner와 `func`에 위임하고, 자신은 누가 어떤
  자원 경계를 통과하는지를 소유한다.

### `action`

- subject의 공개 상태 전이, 권한 행사 또는 stage handoff를 나타낸다.
- 실제 entrypoint가 호출하고, typed fact/`Result`/artifact 같은 관측
  가능한 결과를 소비하거나 생산해야 한다.
- `return true` 또는 `*Ready()` 하나만 반환하는 action은 readiness shell로
  분류한다. 그것만으로 self-host 진척을 세지 않는다.
- 일반 함수 하나를 이름만 바꿔 감싼 action은 금지한다. action에는
  authority, target selection, resource transition, failure boundary 중
  최소 하나의 실제 책임이 있어야 한다.
- `requires`, `within`, `causes`, `authorized by`는 실제 계약이 있을 때만
  쓴다. 키워드 밀도를 높이기 위한 장식은 금지한다.

### `intent`

- 여러 action/zone을 하나의 목적과 성공/실패 계약으로 닫는 실행
  프로토콜이다.
- 한 함수 호출을 감싼 intent나 전체 컴파일을 설명만 하는 거대 call
  list는 금지한다.
- `step`, `pre`, `expect`/`post`, `failure`, `compensate`는 실제 실패 및
  복구 의미가 있을 때 owner fact에 결속한다.
- entrypoint가 호출하지 않는 intent는 아키텍처 표면이며 실행 개사료
  증거가 아니다.

### `ability`, `role`, `authority`

- `ability`는 행위 자격 계약, `role`은 그 계약의 구체적 이행,
  `authority`는 zone 경계에서 누가 결정을 내릴 수 있는지 소유한다.
- 이름표나 문서 분류를 위해 추가하지 않는다.
- 권한 누락과 잘못된 주체는 컴파일 또는 해당 경계에서 fail closed해야
  한다. 일반 함수 fallback으로 우회하지 않는다.

### `effect`, `relation`, `party`, `roster`, projection 구성체

- domain/state/evidence에 실제 해당 관계가 있을 때만 사용한다.
- compiler stage 이름을 Pergyra 단어로 바꾸기 위한 동의어가 아니다.
- 기존 MIR/AIR/ABI fact를 다시 serialize/scan하는 두 번째 그래프를 만들지
  않는다.

### `func`, `struct`, `object`, `tobject`, `vessel`

- 순수 계산, deterministic transform, 값 fact, passive state에는 계속
  사용한다.
- Pergyra-native self-host는 모든 함수를 action으로 바꾸는 작업이 아니다.
- `func`는 “어떻게”, `action`은 “누가 왜 어느 경계에서”, `intent`는
  “어떤 목적을 어떤 성공/실패 프로토콜로”를 소유한다.

## 금지 패턴

- keyword 사용 횟수나 파일 수를 개사료 점수로 사용;
- entrypoint가 import하지 않는 `world.pgy`를 load-bearing root라고 주장;
- 모든 action이 readiness `Bool`만 반환하는 compiler-world shell;
- C 폴더 하나마다 동일 이름의 zone/action 파일 생성;
- native C/LLVM 각자에게 별도 world/사실 그래프 생성;
- `Main -> CompileSourceTo*` 직접 우회와 Pergyra-native root를 동시에 유지;
- 새 root가 기존 owner fact를 복사하거나 source/MIR JSON을 재탐색;
- fixture/generated/probe의 Pergyra 키워드를 production reachability로 계산;
- `action`/`intent`를 단순 pass-through helper로 사용;
- 문서나 정적 텍스트 gate를 실행 parity보다 강한 증거로 사용.

## 목표 실행 척추

최종 방향은 다음과 같다.

```text
driver Main
  -> reachable Pergyra-native compiler subject/action
  -> compiler intent at a real multi-stage boundary
  -> existing typed source/MIR/type/ABI/target owners
  -> one target-neutral projection decision
  -> selected C or LLVM emitter
  -> artifact/parity evidence zone
```

Pergyra-native layer는 기존 owner를 호출하는 마지막 정당한 orchestration
consumer다. parser, MIR, ABI, target facts의 새로운 owner가 아니다.

## 마이그레이션 순서

1. **실제 reachability census**: production entrypoint의 import/call graph와
   language-word 구현 inventory를 생성한다. raw word count는 사용하지 않는다.
2. **action ABI probe — green**: subject action + aggregate 인자 + enum 포함 결과 +
   `with caps io_read, io_write` + action 내부 `WriteFile`을 한 fixture에서
   결합하고 C/LLVM runtime parity를 증명한다.
3. **첫 action 경계 — green**: direct MIR projection 한 경로의 target/authority/stage
   handoff를 실제 subject action이 소유하게 하고 `Main`의 직접 호출을
   삭제한다. 단일 함수 pass-through가 아니라 requested -> target-admitted
   -> artifact-written/rejected 전이를 명시해야 한다.
4. **zone/world 결속**: 그 action이 기존 target/artifact owner fact를 zone
   경계로 소비하게 한다. 사실 복제 없이 한 compiler world에 결속한다.
5. **intent takeover**: 둘 이상의 실제 action/zone과 성공/실패 계약이
   준비된 시점에 root intent를 entrypoint에서 호출한다.
6. **확장**: 같은 방법으로 source -> MIR -> backend 경로를 한 단계씩
   옮긴다. 매 단계에서 기존 direct bypass를 같은 변경으로 삭제한다.

## 완료된 direct-MIR action rung objective card

- Objective: direct MIR C/LLVM projection의 실제 target 선택 및 artifact
  handoff를 Pergyra subject action 하나로 옮기고 bootstrap `Main`의
  `CompileMirJsonToDirectBackendVerified` 직접 호출을 제거한다.
- Execution owner: `driver_rung2_execution_owner.pgy`가
  `DriverRung2ExecutionStage`의 `Requested`, `TargetAdmitted`,
  `ArtifactWritten`, `Rejected` 전이와 그 전이를 수행하는 subject/action을
  소유한다. 이름은 구현 시 기존 owner manifest와 충돌 여부를 검증한다.
- Action responsibility: CLI의 C/LLVM 요청을 owner projection으로 변환하고,
  `CompilerTargetProjectionFactFromOwner`로 target을 admit하고, 기존
  `CompileMirJsonToDirectBackendVerified` 결과를 한 번 소비해 파일을 쓴 뒤
  성공 stage를 남긴다. semantic/MIR/ABI fact는 재소유하지 않는다.
- Priority: 동일 MIR/target fact identity; 실제 entrypoint reachability;
  missing/unknown target fail-closed; direct bypass 삭제; C/LLVM/native parity;
  이후 world/zone 결속.
- Fact owner: MIR, ABI, target projection, certificate, plan은 현재 typed owner가
  계속 소유한다. 새 action은 orchestration과 경계 전이만 소유한다.
- Last legitimate consumer: 실제 artifact를 쓰기 직전의 bootstrap driver
  action.
- Forbidden fallback: `Main`의 직접 backend projection 호출, action 실패 시
  기존 함수 재호출, `Main`의 직접
  `CompilerTargetProjectionFactFromOwner` 호출, C/LLVM별 별도 world/action
  graph. Source mode의 `CompileSourceTo*` 직접 호출 금지는 source mode가
  실제 protocol로 이주하는 다음 rung에서 건다.
- Falsifying case: 알 수 없는 `--mir-json-backend=` 값이나 손상된 target fact가
  action을 우회해 artifact를 생성하는 경우.
- Gate: entrypoint import/call reachability, migrated mode의 direct-call 부재,
  고정 MIR identity, C/LLVM/native output parity, target/graph/certificate/plan
  negative가 artifact 전에 거부됨을 함께 검사한다.

이 objective는 현재 source에서 달성됐다. 증거 등급은 `REACHABLE`이다.
Pergyra action이 실제 direct-MIR orchestration을 소유하지만 새로운 C-owned
compiler path를 대체한 것은 아니므로 `SUBSTITUTING`으로 올리거나 hard
self-host 대체율을 변경하지 않는다.

## 다음 실행 rung objective card

- Objective: 현재 reachable direct-MIR action을 기존 target/artifact fact를
  소비하는 실제 zone 경계에 결속하고, 그 경계를 하나의 compiler world가
  조합할 수 있게 한다.
- Priority: 현재 action과 MIR/target/artifact identity 유지; zone의 실제
  자원·권한·수명 책임; 누락 authority fail-closed; direct-mode bypass 재도입
  금지; 그 뒤 root intent takeover.
- Fact owner: target, certificate, plan, artifact는 기존 typed owner가 계속
  소유한다. zone/world는 이를 복제하지 않고 마지막 orchestration 경계를
  소유한다.
- Forbidden fallback: world 전체를 무조건 import해 선언 수만 늘리기,
  C/LLVM별 별도 zone/world, readiness-only action, zone 내부 source/MIR 재탐색.
- Falsifying case: zone authority가 없거나 잘못됐는데 action이 실행되거나,
  rejected transition 뒤 artifact가 남는 경우.
- Blocker: 현재 `world.pgy --emit-c`의 여섯 intent step에 `authorized by`
  주체가 없어 6 errors/5 warnings로 실패한다. 이 권한 사실부터 실제 owner와
  결속해야 하며 임의 actor 이름으로 채우지 않는다.

`PgyCompilerWorld`는 계속 목표 아키텍처이고 아직 부트스트랩 실행 루트가
아니다. 전체 상태는 `BRIDGE`로 유지한다.

## 세션 메모리와 handoff 규칙

모든 후속 작업은 `docs/current_work_handoff.md`에 다음을 남긴다.

- 어떤 production entrypoint와 mode가 Pergyra-native 경계에 도달하는지;
- 그 경계가 `SURFACE`, `REACHABLE`, `SUBSTITUTING` 중 어디인지;
- 삭제된 direct bypass의 정확한 symbol;
- 마지막 green 실행/parity/negative gate;
- 다음 falsifying fixture와 blocker;
- fixture/generated/readiness 선언을 제외한 실제 진척.

이 문서와 handoff가 소스 또는 실행 gate와 충돌하면 현재 source/call graph와
실행 증거가 우선하며 문서를 바로 고친다.
