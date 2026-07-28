# Pergyra-Native Self-Host Dogfood Contract

Status: `BRIDGE`

이 문서는 Pergyra 컴파일러가 Pergyra 소스로 작성되었다는 사실만으로
개사료(dogfood)를 완료했다고 오판하지 않기 위한 실행 계약이다.

Pergyra의 self-host는 두 조건을 모두 만족해야 한다.

1. 컴파일러 소스가 Pergyra 문법으로 파싱되고 컴파일된다.
2. 완성된 부트스트랩 실행 책임이 Pergyra의 의미 모델인
   `world -> zone -> subject/action -> intent`를 통해 조직된다. 부분 실행 rung은
   intent 이전에도 `REACHABLE`로 기록할 수 있지만 전체 self-host 완료는 아니다.

`func`, `struct`, `if`, `while`가 많다는 사실은 결함이 아니다. 순수 계산과
값 표현에는 이들이 올바른 도구다. 결함은 권한, 자원 수명, 상태 전이,
단계 오케스트레이션 같은 Pergyra 고유 책임까지 일반 함수 호출 그래프로
우회하면서, 별도 파일의 `world`/`zone`/`intent` 선언을 개사료 증거로
세는 것이다.

구성체 자체의 선택 기준은
[`../200_object_to_action_boundary_patterns.md`](../200_object_to_action_boundary_patterns.md)가
소유한다. 이 문서는 같은 패턴을 self-host 실행 도달성과 대체 증거에 적용한다.

## 현재 관측된 상태

2026-07-28 현재 실제 부트스트랩 진입점은
`src/self_hosted/compiler/driver_bootstrap_main.pgy`이다. 이 파일은
`driver_rung2_owner.pgy`와 `compiler_world_direct_mir_owner.pgy`를 import한다.
source/MIR 생성 및 기존 C 호환 mode는 아직 다음 일반 함수들로 분기한다.

- `CompileSourceToCVerified`;
- `CompileSourceToMirJsonFileVerified` 및 pressure-observed 변형;
- `CompileMirJsonToCVerified` 및 observed 변형.

`--mir-json-backend=c|llvm` mode는 다음 한 경로를 호출한다.

```text
driver_bootstrap_main.Main
  -> EmitDirectMirThroughPgyCompilerWorld
  -> PgyCompilerWorld.EmitDirectMir
  -> PgyCompilerWorld.direct_mir
  -> DriverRung2DirectMirZone.execution
  -> DriverRung2Execution.EmitDirectMir
  -> existing target/projection/emission owners
```

action이 request를 typed target projection으로 admit하고, 기존
`CompileMirJsonToDirectBackendVerified` owner를 정확히 한 번 소비하고,
artifact identity를 확인한 뒤 shared compiler-artifact transaction을 commit한다.
`Main`의 target-fact 생성, direct backend 호출, direct-mode raw writer 우회는
삭제됐다.

`ArtifactCommitted`는 `tobject SelfMirArtifactReceipt`가 반환되고
`atomic_visibility=true`, `crash_durable=false`가 확인된 경우에만 기록한다.
wrong identity/target과 commit 실패는 action이 returned `Rejected`로 구별하지만
malformed MIR의 하위 `Die`는 fatal boundary다. 두 실패 종류를 하나의 Rejected
계약으로 과장하지 않는다.

`src/self_hosted/compiler/world.pgy`는 이제 composition owner를 통해 bootstrap
import/call graph에 들어오며 `PgyCompilerWorld`는 direct-MIR slice의 실제
composition boundary다. 다만 현재 호출되는 world/zone/subject/action은 위
경로의 각 하나뿐이다. `world.pgy`의 기존 readiness action과 intent, 나머지
18개 zone은 import-reachable surface이지만 production call site가 없다.

재귀 import 감사에서 bootstrap closure는 477개 파일이고 missing import는
0이다. 그 import-reachable 집합의 선언은 `func` 3,800개, `struct` 204개,
`enum` 6개, `object` 18개, `tobject` 3개, `subject` 17개, `action` 17개,
`zone` 19개, `world` 1개, `intent` 14개, `role` 4개, `ability` 4개다.
`class`/`vessel`/`effect`/`relation`/`party`/`roster`는 0개다. 이 수치는
import surface census이며 모든 선언이 실행된다는 뜻이 아니다.

| 구성체 | 현재 grade | production 근거 |
| --- | --- | --- |
| `struct` | `REACHABLE` supporting construct | production typed 계산에 사용되지만 독립 C-path 대체 등급은 아님 |
| `class`, `object`, `vessel` | `SURFACE` | active direct-MIR call chain의 소비 없음 |
| `tobject` | `REACHABLE`, not `SUBSTITUTING` | receipt는 생성되고 payload까지 소비된다. failure는 생성되지만 caller는 tag만 소비하고 payload는 버린다 |
| `subject`, `action`, `zone`, `world` | `REACHABLE`, not `SUBSTITUTING` | direct-MIR slice 각 1개 |
| `intent` | `SURFACE` | 14개 import, production call 0 |
| `effect`, `relation` | compiler 조직은 `SURFACE`; 입력 기능의 좁은 runtime slice는 `SUBSTITUTING` | bootstrap closure 안의 compiler declaration/call은 0이다. 별개로 self source -> MIR -> general C가 exact role/member facts를 소비해 `zone_layer_projection_runtime`을 실행한다 |

이 표의 grade는 **self-host 컴파일러를 어떤 Pergyra 구성체로 조직했는가**를
평가한다. 컴파일러가 입력 프로그램의 `effect`/`relation`을 정확히 구현했다는
기능 대체 증거와 섞지 않는다. 현재 general C bridge는 Pergyra `func` owner이며
production source 경로에서 실제 C-owned 결정을 대체하지만, 그 사실만으로
self-host compiler 내부에 호출되는 `effect`/`relation` action graph가 생기지는
않는다.

production declaration과 composition은 다음처럼 나뉜다.

- `compiler/driver_rung2_execution_owner.pgy`: 실제 subject/action 1개와
  direct-MIR zone 1개;
- `compiler/world.pgy`: world 1개, target zone 18개, subject/action 16개,
  object 18개, tobject 1개, intent 10개를 선언하고, world member로는 실제
  실행되는 direct-MIR zone 하나만 binding;
- `compiler/stage_intents.pgy`: intent 4개;
- `compiler/authority_owner.pgy`: role 4개, ability 4개;
- `compiler/compiler_world_direct_mir_owner.pgy`: 새 world를 선언하지 않고
  정확한 arity의 `PgyCompilerWorld` direct-MIR root를 inline materialize하는
  유일한 composition function. 미실행 zone의 aggregate zero-fill은 금지된다.

`world.pgy`의 기존 action 16개는 계속 `Compiler*Ready()` 호출/결합만 반환하고
현재 chain에서 호출되지 않는다. 실제 direct-MIR action 하나만 target admission,
artifact 검증, write/rejected transition을 수행한다. 따라서 world가 import됐다는
사실이나 domain 선언 수를 C-owned compiler substitution으로 세지 않는다.
`bin/pgy.exe src/self_hosted/compiler/world.pgy --emit-c`는 현재 exit 0이지만,
semantic compile 성공 역시 production call-site나 대체 증거를 대신하지 않는다.

### 현재 action authority 증거의 세 층

1. **Declaration contract**: `MIRDeclMethod`가 action identity, `requires`,
   `within`, `causes`, `authorized_by_names`, caps/effects를 하나의 method
   contract로 운반하고 `MIRDeclZoneAuthority`가 subject slot과 required
   ability를 운반한다. Native/self `pgy.mir.v1`과 `mir_lower`는 같은 contract
   wire를 소비한다. `semantic.callable_contract_vocabulary`의 18행과 생성
   projection이 native/self/runtime의 membership, mask, canonical order와
   `local` 배타성을 함께 고정하고 field/unknown/duplicate/noncanonical/
   local-mix 변조를 fail closed로 거부한다. 이 declaration 층은 `CLOSED`다.
   다중 `impl ability` method partition, explicit `Damage` effect nominal과
   zone effect/relation slot의 `field_kind`도 canonical native/self MIR과 exact
   AST reconstruction까지 운반된다. Shared field는 일반 field와 구별되며
   field-kind registry projection drift와 required effect participant 손실은
   fail closed한다. Stable declaration-field identity, relation declaration,
   non-empty canonical topology remap, self producer와 graph plan도 이후 좁게
   닫혔다. Exact effect bearer/relation source·target destination role,
   projection member/path와 callable receiver carriage도 이제 semantic ->
   HIR/DIR/MIR -> self admission -> general C의 좁은 실행 slice에서 닫혔다.
   그러나 explicit projection map, declaration-level identity join, pool capacity,
   zone state/dirty/epoch/lifecycle operation, 그리고 native C/LLVM과 self가 함께
   소비하는 단일 runtime plan은 열려 있다. 따라서 이 성공을 전체 runtime
   action 폐쇄로 올려 기록하지 않는다.
2. **Call binding**: 현재 C/LLVM hook은 direct world -> zone -> subject
   receiver와 `authorized by self` 단일 항목만 exact zone authority slot에
   결속한다. named participant, 복수 authority, indirect/direct-subject receiver의
   호출별 binding fact는 없다. named/multiple/indirect world-action shape는 현재
   backend boundary에서 명시적으로 거부되며, generic direct-subject receiver의
   fail-close negative는 아직 없다.
3. **Runtime evidence**: 선택한 zone/participant 주소를 전달하지만 현재 runtime
   check는 두 주소의 non-null presence만 검증한다. slot membership, subject
   identity/token, action/zone ability authorization은 증명하지 않는다.

`tests/self_hosted/parity/driver_execution_action_abi_parity.sh`는 C/LLVM positive
snapshot과 artifact parity, missing/wrong zone authority의 semantic compile
negative와 named/multiple/indirect world-action authority의 backend fail-close
negative를 고정한다. runtime identity/token mismatch와 generic direct-subject
receiver negative는 아직 없다. 호출별 binding fact가 생기기 전에는 지원 범위
밖 shape를 계속 semantic/codegen boundary에서 명시적으로 거부해야 한다.

### Source-to-MIR action의 선행 blocker

source-to-MIR을 lexer/parser/semantic/MIR action 네 개로 쪼개지 않는다. 그
계산은 기존 typed `func` owner가 계속 소유하고 하나의 compiler-run action만
request admission과 verified MIR artifact commit을 소유한다. 이 commit 선행
조건은 이제 닫혔다. `Begin(final) -> checked chunk writes -> checked flush/close ->
atomic replace -> tobject receipt`는 C-inline/LLVM-linked가 한 runtime core를
소비하며, open/write/flush/close/publish fault에서 기존 final과 zero-temp를
검증한다. production source-to-MIR은 facts를 한 번 검증한 뒤 verified writer를
호출하므로 writer가 whole graph를 다시 검증하지 않는다.

ActionContract carriage는 parser, typed AST, semantic owner, native/self MIR
declaration wire와 `mir_lower` 소비 경계까지 연결됐다. focused C/LLVM gate는
native/self contract parity와 missing `within`, unknown zone, non-subject owner,
action-as-function, empty explicit caps/effects를 backend output 전에 거부한다.
clause를 건너뛰어 `Body:`를 찾는 fallback은 없다.

ActionContract carriage 자체는 기존 production action을 정확히 운반하는 supporting
semantic seam이며 C-owned compiler path를 새로 대체하지 않는다. Callable contract
vocabulary는 이미 닫혔고, domain runtime assignment/receiver의 첫 실행 slice도
별도 `func` owner 경로에서 닫혔다. 그러나 production source mode의
`Main -> CompileSourceTo*` 직접 호출은 아직 새 subject action으로 이주하지 않았다.
따라서 direct-MIR world/zone/subject/action은 계속 `REACHABLE`, not
`SUBSTITUTING`이다. Source-mode action을 `SUBSTITUTING`으로 올리려면 production
entrypoint가 새 action을 실제 호출하는 변경과 같은 rung에서 그 직접 우회를
삭제하고 실행/parity/negative gate를 통과해야 한다.

`struct` hosted-func negative, 호출별 authority binding, runtime identity/token과
ability authorization은 declaration carriage와 별도인 열린 fact family다.

### Hosted-method declaration schedule

hosted method의 source declaration order를 사용자 제약으로 만들지 않는다.
C는 nominal forward와 MIR-owned enum/nominal by-value layout schedule을 먼저
실행하고 domain value type이 완성된 뒤 nominal hosted method body를 방출한다.
LLVM은 nominal layout, domain layout, nominal method signature, method body
순서로 등록/방출한다. missing type/layout metadata를 source 재배치, 중복 helper,
opaque/`i32` 추정으로 우회하지 않는다.

`subject_action_global_helper`는 action이 later file-scope helper를 호출하는
prototype 순서를 고정하고,
`tests/cases/backend_compare/hosted_method_later_value_object/main.pgy`는 먼저
선언된 subject method가 뒤에 선언된 object를 by-value로 받는 C/LLVM 회귀를
고정한다. 후자는 현재 nominal-host/later-object 범위의 증거다. zone/world host
또는 later zone/world value까지 보장하려면 대응 parity fixture와 domain method
schedule gate를 먼저 추가한다.

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

세 단계 grade 안에서는 call-site의 실제 깊이를 다음 보조 사다리로 기록한다.

```text
IMPORTED -> MATERIALIZED -> INVOKED -> OUTCOME_CONSUMED -> SUBSTITUTING
```

`REACHABLE`을 주장하려면 최소한 production caller의 `INVOKED` 증거가 필요하다.
결과 enum tag를 읽은 것만으로 payload가 `OUTCOME_CONSUMED`됐다고 세지 않는다.
`SUBSTITUTING`은 기존 C-owned bypass 삭제와 parity/negative gate까지 요구한다.

## 구성체별 구현 규칙

### `world`

- 사용자에게 보이는 하나의 컴파일러 **사실 그래프와 선언 topology**의 조합
  루트다. composition function이 호출마다 새 aggregate를 만들 수 있으므로
  runtime singleton이나 stable-address identity를 뜻하지 않는다.
- resource zone과 root intent를 조합하지만 token, AST, MIR, ABI, target,
  artifact 사실을 복제하거나 재판단하지 않는다.
- 현재 `PgyCompilerWorld`는 direct-MIR mode에서 실제 도달하지만 한 zone/action
  slice만 조합한다. import된 다른 zone/intent를 실행 진척으로 세지 않는다.
- C emitter world와 LLVM emitter world를 따로 만들지 않는다. 하나의
  compiler world가 동일한 MIR/type/ABI/target fact에서 두 projection을
  선택한다.
- `World(Zone(Subject(...)))` inline materialization은 surviving origin binding과
  암묵 identity fork를 막는 owner handoff다. by-value layout의 physical no-copy나
  전역 고유 instance를 증명하지 않는다. `DriverRung2Execution.identity`의 고정
  문자열도 admission label이지 고유 identity token은 아니다.

### `zone`

- 자원, 권한, 수명 또는 관측 가능한 evidence의 실제 경계다.
- 폴더, pass, helper 묶음, 소스 파일 하나를 zone으로 번역하지 않는다.
- zone은 기존 SoT owner의 fact/view를 보유하거나 노출할 수 있지만 그
  사실을 문자열에서 다시 복원하거나 두 번째 authority가 될 수 없다.
- 현재 `DriverRung2DirectMirZone`은 subject slot과 authority/lifetime 경계만
  소유한다. 이 최소 shape를 object/tobject projection, layer, state를 실제로
  소유하는 모든 zone의 보편 규칙으로 확대하지 않는다.
- `lexer/`, `parser/`, `codegen/` 같은 물리 디렉터리 수와 zone 수는
  독립적이다.

### `subject`

- identity를 가진 의사결정자, 승인자 또는 오케스트레이터다.
- 큰 데이터 저장소나 모든 컴파일 단계의 god object가 아니다.
- 내부 계산은 기존 typed owner와 `func`에 위임하고, 자신은 누가 어떤
  자원 경계를 통과하는지를 소유한다.

### `action`

- subject의 관측 가능한 공적 의미의 상태 전이, 권한 행사 또는 stage handoff를
  나타낸다. 이는 export visibility와 별도이며 action은 기본적으로 private다.
- 실제 entrypoint가 호출하고, typed fact/`Result`/artifact 같은 관측
  가능한 결과를 소비하거나 생산해야 한다.
- `return true` 또는 `*Ready()` 하나만 반환하는 action은 readiness shell로
  분류한다. 그것만으로 self-host 진척을 세지 않는다.
- 일반 함수 하나를 이름만 바꿔 감싼 action은 금지한다. action에는
  authority, target selection, resource transition, failure boundary 중
  최소 하나의 실제 책임이 있어야 한다.
- `requires`, `within`, `causes`, `authorized by`는 실제 계약이 있을 때만
  쓴다. 키워드 밀도를 높이기 위한 장식은 금지한다.
- declaration clause가 유효하다는 사실과 호출별 participant가 결속됐다는
  사실, runtime이 identity/ability를 승인했다는 사실을 하나로 취급하지 않는다.
  지원하지 않는 binding shape는 authority check를 생략하지 말고 fail closed한다.

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

- `effect`는 action이 `causes`로 참조하는 typed domain state/evidence이고, zone이
  effect slot/pool로 materialize한다. Callable `with effects` mask와 다른 fact
  family다.
- `relation`은 typed participant endpoint 사이의 지속 관계이며 effect의 별칭이
  아니다. Endpoint identity와 순서, layer kind를 별도 fact로 보존한다.
- zone은 participant slot과 effect/relation layer의 membership/lifetime을
  소유하지만 participant nominal identity나 action transition을 다시 판단하지
  않는다.
- declaration name/type spelling만 같다는 이유로 slot kind, layer kind 또는 ABI
  layout을 추론하지 않는다.
- compiler stage 이름을 Pergyra 단어로 바꾸기 위한 동의어가 아니다.
- 기존 MIR/AIR/ABI fact를 다시 serialize/scan하는 두 번째 그래프를 만들지
  않는다.

### `func`, `struct`, `object`, `tobject`, `vessel`

- 순수 계산, deterministic transform, 값 fact, passive state에는 계속
  사용한다.
- Pergyra-native self-host는 모든 함수를 action으로 바꾸는 작업이 아니다.
- `func`는 “어떻게”, `action`은 “누가 왜 어느 경계에서”, `intent`는
  “어떤 목적을 어떤 성공/실패 프로토콜로”를 소유한다.
- `struct`는 hosted behavior 없는 값 fact, `class`는 value-self 도구,
  `object`는 local read model, `tobject`는 immutable transfer, `vessel`은
  subject-owned pointer-self 상태다. 이 경계를 이름표로만 바꾸지 않는다.

## 금지 패턴

- keyword 사용 횟수나 파일 수를 개사료 점수로 사용;
- import만 되고 production call site가 없는 world/zone/intent를 load-bearing
  실행 진척이라고 주장;
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
   -> artifact-committed/rejected 전이를 명시해야 한다.
4. **zone/world 결속 — green**: 그 action을 subject slot/authority를 소유한
   direct-MIR zone과 기존 `PgyCompilerWorld`에 사실 복제 없이 결속하고
   `Main`이 그 composition boundary를 호출하게 한다.
5. **다음 실제 action 확장**: 같은 방법으로 source -> MIR 경계의 실제
   orchestration을 옮긴다. 매 단계에서 migrated mode의 direct bypass를 같은
   변경으로 삭제한다.
6. **intent takeover**: 둘 이상의 실제 action/zone과 성공/실패 계약이
   준비된 시점에만 root intent를 entrypoint에서 호출한다.

## 완료된 direct-MIR action reachability objective card

- Objective: direct MIR C/LLVM projection의 실제 target 선택 및 artifact
  handoff를 Pergyra subject action 하나로 옮기고 bootstrap `Main`의
  `CompileMirJsonToDirectBackendVerified` 직접 호출을 제거한다.
- Execution owner: `driver_rung2_execution_owner.pgy`가
  `DriverRung2ExecutionStage`의 `Requested`, `TargetAdmitted`,
  `ArtifactCommitted`, `Rejected` 전이와 그 전이를 수행하는 subject/action을
  선언한다. commit receipt는 atomic visibility만 증명하고 crash durability는
  증명하지 않는다.
- Action responsibility: CLI의 C/LLVM 요청을 owner projection으로 변환하고,
  `CompilerTargetProjectionFactFromOwner`로 target을 admit하고, 기존
  `CompileMirJsonToDirectBackendVerified` 결과를 한 번 소비해 compiler artifact
  transaction을 commit한다. semantic/MIR/ABI fact는 재소유하지 않는다.
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

이 objective의 **target admission, orchestration reachability, direct bypass 삭제,
typed atomic commit/rejected outcome 범위**는 현재 source에서 달성됐다.
증거 등급은 `REACHABLE`이다. Pergyra action이 실제 direct-MIR orchestration을
소유하지만 새로운 C-owned compiler path를 대체한 것은 아니므로
`SUBSTITUTING`으로 올리거나 hard self-host 대체율을 변경하지 않는다.

## 완료된 direct-MIR zone/world 결속 objective card

- Objective: reachable direct-MIR action을 authority/lifetime 경계인
  `DriverRung2DirectMirZone`에 넣고 기존 `PgyCompilerWorld`가 조합하게 한다.
- Priority: action과 MIR/target/artifact identity 유지; exact zone/subject
  authority lookup; migrated direct-mode bypass 재도입 금지; 별도 mini-world
  금지.
- Fact owner: target, certificate, plan, artifact는 기존 typed owner가 계속
  소유한다. zone/world는 이를 복제하지 않고 마지막 orchestration 경계만
  소유한다.
- Last legitimate consumer: `PgyCompilerWorld.EmitDirectMir`이
  `direct_mir.execution.EmitDirectMir`에 한 번 위임하는 call site.
- Forbidden fallback: C/LLVM별 별도 zone/world, readiness-only action,
  composition owner의 source/MIR 재탐색, `Main`의 direct action/backend 호출.
- Falsifying case: compiler graph에 두 번째 world가 생기거나, exact authority
  slot 없이 action이 실행되거나, rejected transition 뒤 artifact가 남는 경우.
- Gate: `driver_rung2_execution_action_gate.sh`, action ABI C/LLVM parity,
  current-driver compile과 one-MIR C/LLVM 실행 negative가 이 경계를 고정한다.

이 objective도 현재 source에서 달성됐지만 증거 등급은 `REACHABLE`이다.
`PgyCompilerWorld`는 direct-MIR slice의 실제 bootstrap composition root이지
runtime singleton이나 C-owned compiler path의 대체 구현이 아니다.

## 완료된 domain runtime assignment 실행 rung objective card

- Objective: self-produced `zone_layer_projection_runtime` MIR에서 한 exact
  target-neutral runtime assignment plan을 만들고 self C 실행이 `7`/`dst`를
  출력하게 한다.
- Priority: projection member path와 type; effect bearer/relation source·target
  destination role; receiver carriage; layer materialization/sync; missing-fact
  fail-closed; 그 뒤에 source -> MIR의 두 번째 실제 action 확장.
- Fact owner: semantic `domain_runtime_assignments`가 directive,
  zone/layer/endpoint와 destination/source field path의 exact ID/type를 소유하고
  HIR/DIR/MIR은 lossless하게 운반한다. Self machine admission은 topology와 이
  fact를 한 번 join한 plan receipt를 만들며 의미를 재소유하지 않는다.
- Forbidden fallback: source/AST 재탐색, same-name member join, bindable slot 0/1
  ordinal, native MIR graft, zero-filled layer storage, by-value zone receiver,
  출력 문자열만 맞춘 fixture special case.
- Falsifying case: member ID/type 또는 relation destination role 하나를 바꿔도
  artifact가 생성되거나, self C가 `.poison`/`.trust` zero storage를 읽거나,
  zone method가 by-value receiver로 방출되는 경우.
- Gate: `tests/self_hosted/parity/domain_runtime_assignment_execution_owner.sh`가
  direct source와 explicit MIR의 byte-equal C, exact 5개 prologue assignment,
  `7`/`dst` 실행과 missing/duplicate/foreign role·member·directive·epoch 변조의
  output-before-failure를 함께 고정한다.
- Observed result: native C, native LLVM, self explicit-MIR C와 production self
  direct-source C가 모두 `7`과 `dst`를 출력했다. Self plan은 admission에서 한
  번만 전체 검증되고 owner lookup은 local bounds/shape만 검사한다.

이 좁은 implicit-map eager method-entry bind/sync 경로는 실제 C-owned 결정을
대체하므로 `SUBSTITUTING`이다. 전체 상태는 `BRIDGE`로 유지한다. Native C/LLVM은
같은 semantic fact family를 소비하지만 self와 하나의 shared runtime plan을
소비하지 않고, declaration row에는 declaration-level `source_syntax_id`가 없다.
Dirty/epoch와 detach/unlink/state lifecycle도 아직 scheduler fact가 아니다.

## 완료된 explicit projection-map 실행 rung objective card

- Objective: `map { target <- source }`를 self source artifact와 MIR에 보존하고,
  renamed member와 semantic assignability를 exact path fact로 admit해 production
  self C가 native C/LLVM과 같은 결과를 실행하게 한다.
- Priority: explicit-map parent/entry identity; target/source path와 assignability;
  canonical identity epoch; 기존 implicit-map owner 재사용; direct-source 실행과
  negative ratchet; 그 뒤 lifecycle/dirty/epoch 통합.
- Fact owner: parser는 map spelling과 parent structure를 보존한다. Semantic
  `DomainProjectionMemberAssignment`가 implicit/explicit 모두의 resolved exact
  assignment를 소유하며 self
  `SemanticDomainProjectionTypeAssignable`가 source-to-target compatibility를
  결정한다. DIR/HIR/MIR과 self admission은 carrier/receipt다.
- Forbidden fallback: explicit map을 implicit same-name으로 접기, member 이름만
  운반하기, target/source type 문자열 equality로 assignability를 판정하기,
  native MIR graft, backend AST/source 재탐색, map child를 버린 뒤 callable ID를
  숫자 offset으로 복구하기.
- Observed gate: `domain_runtime_explicit_map_execution_owner.sh`는
  `life: Long <- hp: Int`, `label <- name`의 exact rows와 emitted C, direct-source /
  explicit-MIR byte equality, C/LLVM/self의 `7`/`dst`를 고정한다. no-map, type
  mismatch, missing source, duplicate target은 partial artifact 전에 실패한다.

이 explicit effect/relation eager-map slice도 `SUBSTITUTING`이다. 다만 self의
resolved semantic family는 아직 MIR producer boundary에서 만들어지므로 전체
owner family는 `BRIDGE`다. 이것은 self compiler의 내부 orchestration이
`subject/action/effect/relation`로 이주했다는 증거가 아니다.

## world/tobject query reachability rung objective card

- Objective: `world_zone_projection_visibility`를 self semantic artifact와
  production world/zone call graph에 도달시켜 `label <- displayName`과
  `user <- displayName`이 같은 explicit-map owner를 재사용하도록 한다.
- Priority: 첫 semantic artifact blocker; world/intent reachability; exact renamed
  assignment reuse; observable consumer; direct bypass 삭제; 그 뒤
  lifecycle/dirty/epoch 통합.
- Fact owner: world/intent parser와 semantic artifact owner가 reachability를
  소유한다. 이미 닫힌 projection-map/parser/DIR/runtime owner는 재사용하며 새
  world 전용 map table을 만들지 않는다.
- Forbidden fallback: native MIR graft, fixture-specific 축소 world, map 전용
  codegen branch, `Main -> old function` 병행, readiness-only action을 실행 증거로
  세기.
- Falsifying case: self source가 semantic initializer artifact에서 계속 중단되거나,
  world/intent를 제거해도 같은 실행 결과가 나오거나, renamed source path를
  foreign valid field identity로 바꿔도 artifact가 생기는 경우.
- Current reached seam: nominal constructor calls now consume constructor-field
  facts rather than the ordinary exact-arity function path. Zone constructors
  accept the caller-supplied positional prefix while topology-managed layer
  storage is excluded. `Buyer`, `PaymentZone(Clone(buyer))`, and
  `PaymentWorld(Clone(payment))` therefore pass the initializer artifact.
- `HasProjection`/`HasZoneProjection` arguments are declaration-scoped symbolic
  names, not ordinary value reads. The self semantic query owner now validates
  `payment -> PaymentZone -> buyerView:ObjectSlot` and
  `buyerPacket:TObjectSlot` from the nominal field graph and suppresses the
  undefined-value check only for those exact validated leaf node identities.
  It does not put `buyerPacket` into the runtime variable environment.
- Closed semantic blocker: the same full fixture now resolves `Checkout` from
  an exact parser-artifact intent signature and resolves `IntentHistoryCount`
  from the generated projection of the canonical native observability ABI.
  It reaches `self-host DIR authority shape is unsupported`.
- A fresh Pergyra-built DRV-2 now emits typed MIR for the bounded
  `world_tobject_projection_query` fixture. The focused gate observes the exact
  tobject/world/query facts, compares native C/LLVM output `true`, and rejects
  `missingPacket` with `undefined_symbol` before a partial MIR artifact. The
  projection directives retain explicit `by buyer`: this is transition
  provenance, not authority stored in the tobject.
- Grade this constructor/query delta `REACHABLE`, not `SUBSTITUTING`. Self query
  backend lowering has not executed the world result yet. The existing
  `subject source -> tobject slot -> publish` runtime assignment remains the
  actual tobject execution path; direct self semantic/codegen support for
  `ToTObject(Target, source)` is still open and must not be claimed from parser
  fixtures alone.
- Gate: `tests/self_hosted/parity/world_tobject_projection_query_owner.sh` owns
  self source -> semantic/DIR -> typed MIR reachability, current native C/LLVM
  execution parity, and the missing declaration-identity negative. The full
  fixture is now also pinned by
  `tests/self_hosted/parity/intent_callable_reachability_owner.sh`: arity, type,
  and renamed-intent negatives fail before MIR, while the positive reaches the
  next honest DIR boundary. This remains `REACHABLE`, not `SUBSTITUTING`.

### Next intent DIR objective card

- Objective: carry exact intent participant and step authority/topology facts
  into self DIR, then remove the current unconditional `IntentDecl` rejection.
- Fact owner: a dedicated typed DIR intent owner, sourced from parser artifact
  identity and declaration rows. `tobject` owns immutable detached handoff data;
  it does not own intent step identity or authority.
- Required edges: participant type, step zone, who, requires, authorized-by,
  causes, and predecessor dependency, matching the native DIR fact families.
- Forbidden fallback: incrementing only node/edge counts, accepting unknown
  participant types, rescanning raw source in MIR/backend, importing native MIR,
  or lowering `Checkout` to a constant `Bool` stub.
- Falsifier: mutate the focused `Checkout` participant type, action receiver,
  required ability, authorization source, caused effect, or step order. The
  typed DIR owner must reject the exact changed fact before any MIR artifact.

Direct-MIR action 경계는 계속 `REACHABLE`이며 `SUBSTITUTING`이 아니다. 위 다음
rung은 runtime feature substitution을 넓히는 작업이고, source-mode orchestration을
subject/action으로 이주하는 작업은 그 뒤 별도 executable rung으로 유지한다.

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
