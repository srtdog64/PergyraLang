# 200. Object-to-Action 경계와 작성 패턴

Updated: 2026-07-27 (Asia/Seoul)

이 문서는 `struct`에서 `action`까지의 선택 기준을 한곳에 모은 authoring
contract다. 의미 사실의 최종 권위는 parser/semantic/MIR owner와 executable
gate다. 이 문서가 현재 source와 다르면 source를 근거로 문서를 고치며,
구현되지 않은 목표를 현재 동작으로 기록하지 않는다.

## 0. Objective card

- Objective: 값, 수동 도구, projection, 내부 상태 수용체, identity 주체,
  공적 전이를 서로 바꿔 쓰지 않게 하는 하나의 선택 규칙을 고정한다.
- Priority: nominal identity 보존; 전달과 receiver 의미 분리; mutation 권한;
  실제 boundary 의미; C/LLVM parity; 짧은 문법보다 정확한 진단.
- Fact owner: `NominalDeclKind`가 선언 identity를 소유하고
  `MIRDeclHeader.nominal_kind`가 이를 운반한다. pointer-self, projection
  immutability, boundary-transfer policy는 이 identity에서 파생한다.
- Last legitimate consumer: semantic boundary validator와 C/LLVM hosted-call,
  projection, action-contract emitter다.
- Forbidden fallback: `subject == class`, `tobject == struct`,
  `vessel == value-self`, 문서 이름표로 action을 승격하거나 backend가 AST
  spelling을 보고 nominal 의미를 재판단하는 경로.
- Verification: `tests/object_action_boundary_contract_smoke.sh`, semantic unit
  negatives, `subject_class_dispatch`, `subject_projection`, production
  direct-MIR action parity.
- Falsifying case: value/pass mode와 hosted receiver mode를 하나로 취급하거나,
  `object`/`tobject`를 in-place 갱신하거나, non-subject가 action을 소유하거나,
  action 실패 뒤 artifact/state가 성공으로 관측되는 경우.

## 1. 먼저 네 축을 분리한다

다음 네 질문은 서로 다른 질문이다.

1. 이 값에 복제하면 안 되는 **identity**가 있는가?
2. 함수 경계를 넘을 때 값 복사인가, identity 참조인가?
3. hosted `func`/`action`의 `self`가 원본 셀인가, 값 사본인가?
4. local view, 외부 transfer, authority, resource/stage transition 중 어떤
   경계를 표현하는가?

`value type`이라고 hosted receiver도 반드시 value-self인 것은 아니다.
`vessel`은 일반 함수 인자로는 값 전달되지만, hosted `func`는 소유 subject
안의 상태를 갱신할 수 있도록 pointer-self로 lowering된다. 반대로 `class`는
값 전달과 value-self를 함께 사용한다. 이 차이를 합치면 문서와 backend가
서로 다른 언어를 설명하게 된다.

## 2. 현재 구현과 canonical authoring 행렬

| 구성체 | 존재/identity | 함수 경계 전달 | hosted receiver | construction 이후 상태 | 허용 동사 | 경계 의미 |
| --- | --- | --- | --- | --- | --- | --- |
| `struct` | 작은 구조 값, identity 없음 | 값 복사 | 없음 | 바인딩/필드 규칙에 따른 값 갱신 | free `func`의 입력/출력. hosted `func`는 semantic에서 거부 | 값 fact, 좌표, 옵션, 계획 행 |
| `class` | 수동 nominal 값/도구, identity 없음 | 값 복사와 값 반환 | value-self | `let mut` 필드는 직접 바인딩을 통해 갱신 가능. hosted func 안의 갱신은 호출자 identity 전이가 아님 | hosted `func` | 계산이 붙은 도구, formatter, policy value |
| `object` | local projection 값, 능동 identity 없음 | 값 복사 | value-self | construction 이후 read-only. source를 갱신하고 `refresh`하거나 새 view 생성 | 관측/format/query `func`만 권장. `action` 금지 | 동일 실행 경계의 borrow-first read model |
| `tobject` | 외부 전달 값, identity/authority 없음 | materialized 값 복사 | 현재 passive helper는 value-self; canonical surface는 receiver 없음 | immutable. 새 snapshot을 `publish` | 현재 semantic은 passive `func`를 허용하지만 새 코드는 method-free. `action` 금지 | API/IPC/persistence/world 경계 transfer |
| `vessel` | subject-owned 수동 상태 값, 독립 actor identity 없음 | 일반 파라미터에서는 값 전달 | **pointer-self** | owner가 호출한 hosted `func`가 원본 내부 상태를 갱신 가능 | hosted `func`; `action` 금지 | subject 내부 상태·자원·규칙 수용체 |
| `subject` | identity-bearing active host | 기본 파라미터는 자동 참조; plain copy/rebind/value return 금지 | **pointer-self** | 자신의 `func`/`action`을 통한 원본 셀 전이 | private/local `func` + public/boundary `action` | 결정, 승인, stage/resource handoff |
| `action` | subject의 동사이며 독립 타입이 아님 | subject receiver 계약을 따름 | subject pointer-self | 관측 가능한 성공/실패와 전이를 소유 | `requires`/`within`/`authorized by`/`causes`/`with caps`/`with effects` | 공적 상태 전이, 권한 행사, artifact/resource handoff |

### 구현 근거

- parser는 여섯 nominal을 서로 다른 `NOMINAL_DECL_*`으로 만든다.
- MIR declaration header는 `subject`와 `vessel`만 `uses_pointer_self`로
  운반한다.
- semantic은 subject의 plain copy/rebind/value return을 거부하고 기본
  parameter를 reference semantics로 취급한다.
- semantic은 `object`와 `tobject` field assignment를 각각 refresh/publish
  위반으로 거부한다.
- `struct` hosted method는 semantic에서 거부한다.
- parser와 semantic은 `action`을 `subject` 안에서만 허용한다.
- top-level `public`/`export`는 native와 self-host parser 모두 같은 AST
  `[export]` fact를 만든다. `private`은 non-export다. 특히 다른 owner가
  참조하는 `public zone`의 visibility를 parser가 장식처럼 버리면 action의
  `within` 경계가 self-host import 그래프에서 사라진다.
- 현재 semantic unit은 `object`/`tobject` passive helper `func` 선언을
  허용한다. canonical contract는 object의 관측 helper만 남기고 tobject는
  method-free로 닫는 방향이며, 이 차이는 아래의 열린 부채다.
- LLVM은 `object`/`tobject`를 immutable projection storage로, `tobject`만
  boundary-transfer contract로 분류한다.

같은 C layout을 사용할 수 있다는 사실은 같은 언어 계약이라는 뜻이 아니다.
storage sharing은 구현 세부사항이고 nominal kind가 의미 identity다.

### 실제 corpus 판정

2026-07-27의 `driver_bootstrap_main.pgy` 재귀 import closure는 443개 파일이며
missing import는 0이다. 이 import-reachable 집합에는 `func` 3,495개,
`struct` 176개, `enum` 6개, `object` 18개, `tobject` 3개, `subject` 17개,
`action` 17개, `zone` 19개, `world` 1개, `intent` 14개, `role` 4개,
`ability` 4개가 선언돼 있다. `class`/`vessel`/`effect`/`relation`/`party`/
`roster` 선언은 0이다.

이 수치는 import 표면 census이지 모든 선언의 실행 call-site 증거가 아니다.
실제 direct-MIR production call chain은 `PgyCompilerWorld`의 `direct_mir` zone과
`DriverRung2Execution.EmitDirectMir` action 하나를 지난다. 이 action은
identity admission, target admission, atomic artifact commit을 실제로 소유한다.
commit은 `tobject SelfMirArtifactReceipt`가 있을 때만 `ArtifactCommitted`로
전이하며, 이 receipt는 atomic visibility만 주장하고 crash durability는
명시적으로 `false`다. `world.pgy`의 기존 action 16개는 import closure에는 들어왔지만
계속 `Compiler*Ready()`를 반환하는 readiness facade이며 현재 production
chain에서 호출되지 않는다. 따라서 선언 수나 Pergyra다운 이름은 구조
적합성의 참고 자료일 뿐, 실행 개사료나 C-path 대체 진척은 아니다.

| 구성체 | production dogfood 판정 | 근거 |
| --- | --- | --- |
| `struct` | `REACHABLE` supporting construct | production 계산에는 실제 사용되지만 구성체 자체가 C-owned path를 대체했다는 독립 증거는 아님 |
| `class` | `SURFACE` | bootstrap closure 선언 0, backend fixture만 존재 |
| `object` | `SURFACE` | world schema 18개, active direct-MIR call chain 소비 0 |
| `tobject` | `REACHABLE`, not `SUBSTITUTING` | active artifact commit의 immutable receipt/failure 전달 |
| `vessel` | `SURFACE` | bootstrap closure 선언 0 |
| `subject` / `action` | `REACHABLE`, not `SUBSTITUTING` | direct-MIR subject/action 1쌍만 production 호출, 나머지 16쌍은 readiness |
| `zone` / `world` | `REACHABLE`, not `SUBSTITUTING` | direct-MIR slice 하나만 호출; authority runtime 증거는 presence-only |
| `intent` | `SURFACE` | 14개가 import되지만 production intent call 없음 |

corpus에서 반복되는 유효 패턴도 이 행렬과 일치한다.

- `object`는 action과 mutation 없이 view/snapshot/data carrier로 사용된다.
- `class`는 거의 전부 passive value/tool이며 mutation 사례는 backend
  fixture/test에만 집중돼 있다.
- `subject`는 상태 전이와 vessel 소유가 필요할 때 쓰인다. 다만 `name` 필드만
  둔 ceremonial identity도 많으므로 field가 실제 승인/진단/전이에 쓰이는지
  별도로 확인해야 한다.
- `zone`이 subject slot의 vessel을 직접 수정하면 subject action의 mutation
  owner를 우회할 수 있다. zone은 admission/authority를 소유하되 subject
  내부 전이는 해당 subject action으로 요청하는 것이 기본이다.

### 실행 call-site 감사가 보여 준 경계

현재 production 호출 그래프에서 실제로 호출되는 Pergyra-native hosted action은
`DriverRung2Execution.EmitDirectMir` 하나다. `object` 18개는 schema/slot
surface이며 construction, `ToObject`, `refresh` production call이 없다.
`tobject SelfMirArtifactReceipt`와 `SelfMirArtifactFailure`는 transaction 결과로
실제 도달하지만, `ParityVerdict`는 surface다. `subject/action` 17쌍 중 나머지
16쌍도 호출되지 않는 readiness facade다.

따라서 모든 키워드를 한 경로에 억지로 쓰는 것은 개사료가 아니다. 필요한
경계만 다음 순서로 둔다.

```text
typed struct/enum request + existing owner facts
  -> one identity-bearing subject.action
  -> existing typed computation owners
  -> transaction/resource owner
  -> tobject receipt/failure
  -> one real zone
  -> one PgyCompilerWorld
  -> intent only after multiple real actions exist
```

현재 더 큰 결함은 action 선언의 **typed carriage**다. self-host typed arena는
`Action:`과 `Function:`을 같은 callable kind로 접고, parser가 읽은 caps/effects를
signature fact에 운반하지 않는다. self-host MIR declaration과 `pgy.mir.v1`도
action identity, `requires`, `within`, `causes`, `authorized by`, caps/effects를
하나의 fact로 보존하지 않는다. 그러므로 native action 실행 parity는
self-host source -> MIR action-contract 보존 증거가 아니다. 다음 실제 대체
rung은 이 필드를 `ActionContract` 하나로 운반하고 변조·누락을 fail closed로
거부해야 한다.

### MatchCase 패턴 그래프 통합

`case` 패턴은 한때 typed AST의 `Case:` atom과 별도
`match_pattern_graphs`에 동시에 존재했다. production parser artifact에는 두
표현이 모두 있었지만 compact/native AST bridge에는 후자만 비어 있어, 같은
source가 진입 경로에 따라 다른 semantic 결과를 냈다.

현재 pattern identity의 owner는 typed `MatchCase` AST atom을 받는
`AstMatchCasePatternFactFromArtifact` 하나다. parser는 syntax를 검증하고 atom을
만들되 별도 pattern expression graph를 생산하지 않는다. `AstTreeArtifact`는
payload schema v3에서 executable `expression_graphs`만 운반하며, 구 partition
owner와 ordinal join은 삭제됐다. 빈 패턴, `or`/guard 형태, 문자열 pattern,
중복 또는 비식별자 binding은 owner contract에서 fail closed다.

이 물리적 이중 그래프 제거는 완료됐지만 pattern fact family 전체는 아직
`BRIDGE`다. codegen의 option/tagged-enum condition/binding helper 네 곳은 전달된
pattern 문자열을 다시 구조화한다. 이후 semantic/MIR codegen view가
`AstMatchCasePatternFact`를 직접 운반할 때만 `CLOSED`로 올린다.

## 3. 구성체 선택 순서

### 3.1 값과 host 선택

아래에서 처음으로 `예`가 되는 항목을 고른다.

1. 필드 묶음이고 hosted behavior가 필요 없는가? `struct`.
2. 계산/query가 붙은 복사 가능한 도구나 policy value인가? `class`.
3. 같은 execution boundary에서 source의 읽기 모델이 필요한가? `object`.
4. API/IPC/persistence/world 경계로 source lifecycle과 분리해 보내는가?
   `tobject`.
5. identity 주체가 소유하며 호출될 때 원본 상태/자원을 갱신하는 내부
   수용체인가? `vessel`.
6. 누가 결정·승인·조율했는지가 의미이고 plain copy가 금지돼야 하는가?
   `subject`.

저장 위치는 이 선택의 기준이 아니다. heap/stack/`Box<T>`는 storage와
escape 문제이고, `Slot<T>`는 자원 규율이다. `subject != heap`,
`vessel != pointer type`이다.

### 3.2 `func`와 `action` 선택

순수 라이브러리나 단일 계산은 intent/world를 먼저 만들지 않고 필요한
`struct`/`class`와 `func`에서 시작한다. 도메인 전이가 생겼을 때 subject를,
여러 action/zone의 성공·실패·보상 프로토콜이 생겼을 때 intent를 추가한다.

`func`를 기본값으로 사용한다. `func`도 visibility, `with caps`,
`with effects`를 가질 수 있으므로 `func == pure`, `action == impure`로 나누지
않는다. 다음 중 하나를 실제로 소유할 때만 subject의 `action`으로 올린다.

- authority 승인/거부;
- resource 또는 zone 경계 통과;
- requested -> admitted -> committed/rejected 같은 stage transition;
- 외부에 관측되는 effect;
- artifact handoff와 실패 경계.

`action`은 긴 함수의 다른 이름이 아니다. action body의 계산은 typed owner와
`func`에 위임하고, action은 누가 어떤 fact를 가지고 어느 경계를 통과했는지
소유한다. 결과는 typed result, stage, diagnostic, artifact identity처럼
호출자가 성공과 거부를 구별할 수 있어야 한다.

## 4. Action contract best practice

1. **Subject-only**: action receiver에는 복제 불가능한 identity가 있어야 한다.
2. **One transition owner**: 같은 transition을 `Main`, helper, action이 함께
   판단하지 않는다.
3. **Explicit failure**: 실패가 old path 재진입이나 빈 artifact로 바뀌면 안
   된다.
4. **Clauses are facts**: `requires`, `within`, `authorized by`, `causes`는 실제
   ability/zone/authority/effect owner가 있을 때만 쓴다.
5. **Capabilities are effects, not decoration**: 파일/환경/clock 작업은 실제
   `with caps`와 body-derived effect가 일치해야 한다.
6. **Backend-neutral action**: C action과 LLVM action을 나누지 않는다. action은
   target fact를 소비하고 backend projection owner를 정확히 한 번 호출한다.
7. **Negative ratchet**: action을 추가할 때 기존 direct bypass 삭제와 rejected
   path의 zero-artifact 검사를 같은 rung에 둔다.
8. **Transaction mechanism stays below the action**: action은 admitted payload와
   typed commit outcome을 연결한다. begin/write/abort/commit 상태기계는 전용
   transaction owner 한 곳에 두고 action body에 복제하지 않는다.
9. **Contract survives lowering**: `Action:` spelling만 보존해서는 안 된다.
   callable identity, subject owner, `requires`/`within`/`causes`/authority,
   caps/effects를 한 typed `ActionContract`로 semantic과 MIR wire까지 운반한다.
   중간 단계가 action을 일반 function으로 접거나 clause를 버리면 fail closed다.

### 4.1 Artifact action의 commit 조건

파일 artifact를 만드는 action은 `FileOpen -> FileWrite* -> FileClose`를 성공
전이라고 부르면 안 된다. 이 raw `Int` handle 표면은 open mode, write failure,
flush/close failure, final publish를 서로 다른 호출로 흩뜨린 호환성 API다.
현재 일반 I/O 표면과 compiler artifact 표면은 다음처럼 분리한다.

- native runtime은 이제 `FileOpen` mode와 `FileRead`/`FileWrite`에서 실제
  `io_read`/`io_write` grant를 다시 검사하고 semantic도 literal mode를
  정밀 추론한다. 동적 mode는 양쪽 capability를 보수적으로 요구한다.
- `FileOpen`의 mode-derived capability는 아직 MIR/AIR call-site fact로 운반되지
  않는다. name-only AIR mapping으로 READ|WRITE 두 site를 추정하면 read-only
  program의 실제 manifest와 충돌하므로 이 부분은 `PARTIAL`이다.
- `FileWrite`/`FileClose`는 계속 일반 호환성 I/O다. 언어 표면에서 `Void`인 이
  API를 compiler artifact 성공 증거로 사용하지 않는다.
- compiler artifact는 `CompilerArtifactBegin/Write/Commit/Abort` 내부 ABI를
  사용한다. C-inline과 LLVM-linked runtime은 같은 native core를 include하며,
  self-host owner 한 곳이 scalar status를 typed receipt/failure로 즉시 바꾼다.
- production MIR writer, direct-MIR action, bootstrap 출력, rung-1 CLI 출력은
  final path raw writer를 사용하지 않는다. 기존 final은 publish 직전까지
  변경되지 않고, 실패한 temp cleanup도 별도 실패 상태로 관측된다.

Pergyra다운 해법은 `TryFileWrite`류를 계속 추가하는 것이 아니라 compiler
artifact 전용 transaction owner 하나다.

```text
Begin(final path)
  -> same-directory unique temp
  -> checked chunk writes
  -> checked flush + close
  -> atomic replace
  -> tobject ArtifactReceipt
```

transaction은 실패 시 기존 final의 byte/hash를 보존하고 temp를 제거하며 성공
receipt를 발행하지 않는다. 현재 `tobject SelfMirArtifactReceipt`는 schema,
final path identity, atomic-visibility와 crash-durability claim만 전달하는
immutable 결과다. byte count/fingerprint는 아직 receipt wire에 넣지 않았으므로
존재한다고 주장하지 않는다. write 권한과 내부 handle은 action-local resource이며
`object`/`tobject`가 소유하지 않는다. file/directory sync가 없으므로 현재
`crash_durable`은 항상 `false`다.

`causes DamageEffect` 같은 domain effect와 `with effects io, alloc, authority`
같은 compiler effect mask, `with caps io_read, clock` 같은 runtime capability는
서로 다른 축이다. 특히 `with effects authority`는 zone의 `authority`와
`authorized by`가 소유하는 승인 provenance를 대신하지 않는다.

현재 production 예인 `DriverRung2Execution.EmitDirectMir`는 target admission과
`Requested -> TargetAdmitted -> ArtifactCommitted`를 소유한다. wrong
identity/target과 commit failure는 반환된 `Rejected`로 구별하지만 하위
direct-MIR owner의 malformed-input 실패는 여전히 `Die` fatal boundary다.
backend artifact 생성 자체는 기존 typed owner에 남고, action-contract wire도
아직 열려 있다. 따라서 이 action은 atomic commit까지 `REACHABLE`이지만 아직
C-owned compiler path를 대체한 `SUBSTITUTING`은 아니다.

## 5. 권장 조합

```pergyra
struct TargetRequest {
    projection: String;
}

class DiagnosticFormatter {
    let prefix: String;

    func Format(self, message: String) -> String {
        return prefix + message;
    }
}

object ArtifactView {
    schema: String;
    fingerprint: String;
}

tobject ArtifactReceipt {
    schema: String;
    fingerprint: String;
}

vessel ExecutionState {
    stage: Int;

    func Advance(self, next: Int) -> Void {
        stage = next;
    }
}

subject CompilerExecution {
    vessel state: ExecutionState;

    func TargetKnown(self, request: TargetRequest) -> Bool {
        return StringLength(request.projection) > 0;
    }

    action Emit(self, request: TargetRequest) -> Result<ArtifactReceipt>
        with caps io_write {
        // admit -> emit -> verify -> write; 실패는 Result로 구별한다.
    }
}
```

이 예에서 `struct`는 요청 값, `class`는 복사 가능한 계산 도구, `object`는
local read model, `tobject`는 전달 receipt, `vessel`은 subject 내부 상태,
`subject/action`은 identity-bearing 전이를 소유한다. 실제 zone/authority가
없으므로 `within`이나 `authorized by`를 장식으로 추가하지 않는다.

## 6. 금지 패턴

- `struct`에 hosted `func`를 넣고 class처럼 사용하는 것;
- `class`를 identity actor로 사용하거나 action을 붙이는 것;
- `class` value-self mutator가 호출자 상태를 바꾼다고 가정하는 것;
- `object`를 source 대신 직접 수정하거나 mutable cache/store로 쓰는 것;
- `tobject`에 method, authority, slot, borrowed pointer를 싣는 것;
- `vessel`이 스스로 authority를 판단하거나 action을 시작하는 것;
- 모든 상태와 계산을 subject 하나에 모아 god subject를 만드는 것;
- `return true`, `*Ready()`, 단일 helper pass-through를 action 진척으로 세는
  것;
- 동일한 transition을 C/LLVM별 action이나 별도 world graph로 복제하는 것.

## 7. 현재 열린 구현 부채

아래는 목표가 아니라 현재 source에서 관측된 gap이다.

1. `tobject` body의 passive helper `func`는 parser뿐 아니라 현재 semantic
   unit에서도 명시적으로 허용된다. 하지만 immutable boundary DTO라는
   canonical contract는 method-free다. 기존 positive를 전용 semantic
   negative로 전환해 fail closed해야 한다.
2. `object` receiver field assignment은 거부되지만, hosted body의 bare field
   write와 `self.field` write가 같은 owner policy를 소비하는지는 완전히
   닫히지 않았다. object query/format func의 무효과 규칙을 한 gate로 묶어야
   한다.
3. subject/class/vessel의 bare field write와 `self.field` write가 동일한
   field-mutability fact를 소비하지 않는 오래된 경로가 남아 있다. 새 코드는
   변경 상태를 `let mut` 또는 vessel bare field로 명시하고 두 표기를 같은
   의미로 가정하지 않는다.
4. `class` hosted func는 value-self이므로 내부 mutation 결과가 호출자에 남지
   않는다. mutator처럼 보이는 이름은 피하고 새 class 값을 반환하는 형태를
   쓴다.
5. production self-host import graph에는 `PgyCompilerWorld`와 19개 zone을 포함한
   domain 표면이 들어와 있다. 그러나 실제 direct-MIR call chain이 통과하는
   Pergyra-native 경계는 world 1개, direct-MIR zone 1개, subject/action 1개다.
   import-reachable 선언 수나 fixture 수를 self-host substitution으로 세지 않는다.
6. `MakeSubject().Action()` 같은 temporary subject receiver는 C에서
   address-of-rvalue가 되고 LLVM에서는 임시 alloca로 보존될 가능성이 있다.
   두 backend가 다른 lifetime을 만들지 않도록 semantic에서 stable subject
   binding만 receiver로 허용하는 negative가 필요하다.
7. `MIRDeclMethod`는 현재 action identity, `within`, `causes`,
   `authorized by` 이름 목록을 운반한다. action `requires`, declared caps/effects,
   zone authority ability와 호출별 participant binding은 아직 하나의 소비 가능한
   method/call fact로 닫히지 않았다. backend가 AST를 재조회하거나 이름에서
   binding을 추정하지 않도록 각 carriage owner를 분리해 명시해야 한다.
8. action 후 projection/effect sync가 C와 LLVM에 별도로 구현돼 있다. 동일한
   검증 plan을 두 backend가 소비하도록 합쳐야 하며, 출력 parity만으로 중복
   정책 소유를 정당화하지 않는다.
9. semantic helper `type_is_class_object_type()`은 이름과 달리 현재 subject만
   판정한다. 새 consumer를 붙이지 말고 subject identity 이름으로 바꿔
   오분류 가능성을 제거해야 한다.
10. hosted method의 source declaration order는 사용자 ABI가 아니다. C는
    MIR-owned value layout schedule 뒤 domain type을 완성한 후 nominal method
    body를 방출하고, LLVM은 nominal/domain layout을 모두 등록한 뒤 method
    signature와 body를 방출한다. 이 순서를 source 재배치나 opaque type 추정으로
    우회하지 말고 declaration inventory와 later-declared value fixture로 고정한다.
11. self-host parser의 `decl_nominal_owner.pgy`는 nominal body의 `func`/`action`을
    공통 경로로 읽어 native의 subject-only action, struct-method 금지를 자체적으로
    재검증하지 않는다. self-host semantic negative owner가 생기기 전에는 grammar
    fixture 통과를 nominal semantic parity로 해석하지 않는다.
12. self-host function parser는 `requires`/`within`/`causes`/`authorized by`를
    AST text로 남기지만 `with caps`/`with effects` 이름은 소비 후 버린다.
    action을 generic signature로 낮춰 계약 fact를 잃는 경로는 full self-host
    action 지원이 아니다.
13. native in-memory `MIRDeclMethod`의 일부 action metadata와 달리 현재
    `pgy.mir.v1` declaration JSON은 method name/return/params 중심이며 action
    identity, `within`, `causes`, authority, requires, caps/effects, call binding을
    운반하지 않는다. source -> MIR -> direct backend가 action 계약을 보존했다고
    주장하기 전에 `ActionContract` 단일 fact owner와 wire carriage가 필요하다.
14. compiler artifact transaction은 shared C/LLVM runtime core, self-host typed
    receipt, generated runtime include, old-final 보존 fault gate까지 닫혔다.
    일반 raw file handle의 MIR/AIR mode fact와 checked close 표면은 별도 호환성
    부채다. source -> MIR의 완전한 action substitution은 item 12~13의 action
    contract carriage와 `Main -> CompileSourceTo*` 직접 경로 삭제에 계속 막혀 있다.
15. `Rejected` enum의 존재가 모든 실패의 반환을 뜻하지 않는다. wrong
    identity/target처럼 action이 직접 반환하는 실패와, 하위 `Die`/`Exit`가
    중단시키는 fatal 실패를 gate와 문서에서 분리한다.

다음 semantic closure의 첫 falsifying fixtures는 `tobject` hosted method,
`object` bare-field mutation, `class` mutator persistence 오해, subject의
bare/`self.` mutability 불일치, temporary subject action receiver다. 이 작업은
현재 zone/world executable rung을 우회하는 별도 진척으로 계산하지 않는다.

후속 gate는 다음 순서가 안전하다.

1. temporary subject receiver, tobject hosted method, object mutation을 semantic
   negative로 닫는다.
2. 서로 다른 두 subject의 direct field와 vessel field 중 한 인스턴스만
   변경되는지 C/LLVM parity로 확인한다.
3. reachable action은 production caller, 실제 subject fact 사용, 소비되는
   result를 가져야 하며 `return *Ready()` 단독 body를 금지한다.
4. 잘못된 execution identity/target은 returned `Rejected`, malformed MIR은
   fatal/nonzero로 분리하고 둘 다 output 부재를 실행 gate로 만든다. 현재는
   이 dynamic negative가 완전히 존재하지 않으므로 문서 주장보다 gate를 먼저
   추가한다.
5. zone이 authority 없이 subject-owned vessel을 직접 바꾸는 경로를 거부하고,
   동일 subject identity가 multi-zone handoff 뒤 유지되는지 실행으로 고정한다.

## 8. 문서 읽기 우선순위

1. 현재 작성 판단: 이 문서.
2. 문법 spelling: `docs/grammar/00_cheatsheet.md`와
   `docs/grammar/01_syntax.md`.
3. subject/vessel 철학: `docs/22_class_object_model.md`와
   `docs/26_vessel_action_model.md`.
4. self-host 증거 등급: `docs/self_hosted/17_pergyra_native_dogfood_contract.md`.
5. 과거 설계 문서가 이 행렬과 충돌하면 현재 source와 executable gate를
   먼저 따르고 과거 문서를 고친다.

## 9. Compiler dogfood의 canonical 경계 패턴

현재 direct-MIR rung에서 채택한 실행 패턴은 다음 하나다.

```text
struct/class/object/tobject value facts
  -> identity-bearing subject
  -> action (transition + explicit result)
  -> direct-MIR zone (subject slot + authority/lifetime boundary)
  -> the single PgyCompilerWorld declaration graph
  -> intent only after multiple real actions form one purpose
```

구현 규칙은 다음과 같다.

1. `action` 선언이 `requires`, `within`, `authorized by`, `causes`, caps/effects와
   transition 계약의 source surface를 소유한다. intent step은 유일하게 matching한
   action에서 `where`, `requires`, `causes`, `authorized by`를 상속한다. participant
   alias mapping이 모호하면 상속하지 않으며, 같은 clause를 명시적으로 반복하는
   현재 동작은 semantic error가 아니라 redundancy warning이다.
2. **현재 `DriverRung2DirectMirZone`에 한해** zone은 subject slot과 authority/
   lifetime 경계만 소유한다. 일반 zone은 projection의 실제 owner일 때
   object/tobject slot, layer, state, refresh/publish를 정당하게 소유할 수 있다.
   direct-MIR zone은 target, MIR, certificate, projection plan, artifact payload를
   다시 복사하지 않는다.
3. 현재 direct-MIR world method는 zone 내부 action으로 한 번 위임하는
   composition boundary다. target 판정, backend 호출, artifact commit을 world나
   `Main`으로 끌어올리지 않는다. 일반 world가 여러 실제 zone/action을 조정할
   수 있다는 언어 규칙까지 단일 호출로 제한하지 않는다.
4. compiler topology의 world 선언은 `PgyCompilerWorld` 하나다. 이는 C/LLVM별
   world나 현재 rung 전용 mini-world를 금지하는 **단일 사실 그래프** 규칙이지,
   프로세스 전체에 world aggregate가 하나만 존재한다는 runtime singleton
   계약은 아니다.
5. semantic은 이미 named live zone/subject binding의 world/zone 암묵 embedding을
   거부하고 `Clone(...)`만 명시적 detached copy로 허용한다. composition root의
   `World(Zone(Subject(...)))`는 surviving origin alias가 없는 inline
   materialization이다. C/LLVM layout은 계속 by-value aggregate이므로 이를
   physical no-copy, stable address, 또는 고유 runtime identity로 부르지 않는다.
   `DriverRung2Execution.identity`의 고정 문자열도 admission label이지 고유
   identity token은 아니다.
6. 일반 world/zone call ABI는 named argument를 아직 구현하지 않았다. 따라서
   positional constructor는 정확한 arity를 가져야 하고, 현재 world member는
   실제 실행되는 `direct_mir` 하나뿐이다. 선언만 된 target zone은 production
   bypass를 삭제하는 rung에서만 같은 world에 추가한다. 생략된 aggregate
   field의 zero 값, `__zone_active_*`, 가짜 schema/subject로 readiness를
   가장하지 않는다.
7. action contract는 semantic 검사에서 끝나지 않는다. declaration clause,
   호출별 authority binding, runtime evidence의 owner가 분리돼야 하며 C/LLVM은
   같은 binding fact를 소비해야 한다. inherited intent authority가 필요한
   경로는 RIR/AIR evidence까지 운반된 뒤에만 닫힌 것으로 센다.

현재 production 경로는 다음 모양이다.

```text
driver_bootstrap_main.Main
  -> EmitDirectMirThroughPgyCompilerWorld
  -> PgyCompilerWorld.EmitDirectMir
  -> PgyCompilerWorld.direct_mir
  -> DriverRung2DirectMirZone.execution
  -> DriverRung2Execution.EmitDirectMir
  -> existing target/projection/emission owners
```

이 연결은 기존 C-owned compiler 의미를 대체하지 않으므로 증거 등급은 계속
`REACHABLE`이며 `SUBSTITUTING`이 아니다. 하지만 선언만 존재하던 world와 실제
production action을 하나의 import/call graph로 합친 실행 경계다. 별도 world
재도입과 migrated direct-MIR mode의 `Main` 직행은 negative gate가 금지한다.

### 9.1 현재 action authority ABI 범위

현재 action authority는 다음 세 층으로 나눠 판정한다.

| 층 | 현재 owner와 증거 | 아직 닫히지 않은 것 |
| --- | --- | --- |
| declaration contract | `MIRDeclMethod`의 action/`within`/`causes`/`authorized_by_names`, `MIRDeclZoneAuthority`의 subject slot/required ability | action `requires`, caps/effects를 호출이 소비할 한 contract row로 결합 |
| call binding | C/LLVM hook이 direct world -> zone -> subject receiver와 정확한 zone authority slot을 구조적으로 resolve | named participant, 복수 authority, indirect receiver를 나타내는 호출별 binding fact |
| runtime evidence | 선택한 zone 주소와 participant 주소를 runtime에 전달하고 non-null presence snapshot 기록 | slot membership, subject identity/token, action/zone ability authorization |

현재 C/LLVM member-call hook이 check를 방출하는 범위는 world의 직접
zone/subject receiver와 `authorized by self` 단일 항목
(`authorized_by_count == 1`) 조합뿐이다. 런타임의
`pgy_zone_authority_check_export`/C macro는 현재 zone과 participant 주소의
non-null만 검증한다. 따라서 positive snapshot은 정확한 정적 slot 선택과 주소
전달 증거이지 identity/token/ability 권한 승인 증거가 아니다.

`tests/self_hosted/parity/driver_execution_action_abi_parity.sh`는 C/LLVM positive
snapshot/artifact parity와 missing/wrong zone authority의 **semantic compile
negative**, named/multiple/indirect world-action authority의 **backend
fail-close negative**를 고정한다. runtime identity/token mismatch와 generic
direct-subject receiver negative는 아직 없다.

semantic은 named participant authority와 복수 authority를 허용하지만 현재
C/LLVM world hook은 호출별 binding fact가 없는 named/multiple/indirect shape를
codegen boundary에서 명시적으로 거부한다. generic direct-subject receiver는
아직 이 gate의 admission 또는 fail-close 증거가 아니다. 지원 범위를 넓히기
전에는 해당 shape도 명시적 negative로 고정해야 한다. `self`로 추정하거나 첫
authority/zone을 고르는 fallback, check 없는 성공 lowering은 금지한다. 확장할
때 binding fact, 정확한 subject slot lookup, C/LLVM positive와 unsupported-shape/
runtime negative를 같은 변경에서 닫는다.

C emitter의 authority helper는 `bool + out` 계약을 사용한다. `out == null`인
"적용할 check 없음"과 check 문자열 materialization 실패가 호출자에게 같은 값으로
보이면 실패 시 unchecked action을 emit하는 silent fallback이 된다. 따라서
materialization 실패는 진단 할당의 성공 여부와 무관하게 `false`로 전파되어
호출 emission을 중단해야 한다.

### 9.2 Hosted-method declaration scheduling

source declaration order는 hosted method의 작성 제약이나 backend별 ABI가
아니다. 현재 C는 nominal forward typedef와 MIR-owned enum/nominal by-value
layout schedule을 먼저 실행하고, zone/world value type이 완성된 뒤 nominal
hosted method body를 방출한다. LLVM은 nominal layout, domain layout, nominal
method signature, method body 순서로 등록/방출한다. missing layout/type metadata는
source를 재배치하거나 `i32`/opaque type으로 추정하지 않고 fail closed한다.

`tests/cases/backend_compare/hosted_method_later_value_object/main.pgy`가 먼저
선언된 subject method가 뒤에 선언된 object를 by-value parameter로 받는 C/LLVM
회귀를 고정한다. 현재 증거는 이 nominal-host/later-object 범위다. zone/world
host와 later zone/world value까지 일반 보장으로 넓히려면 해당 C/LLVM parity
fixture를 먼저 추가하고 domain method body schedule도 같은 declaration inventory
owner 아래에서 검증해야 한다.
