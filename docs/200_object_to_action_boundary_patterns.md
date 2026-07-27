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
- 현재 semantic unit은 `object`/`tobject` passive helper `func` 선언을
  허용한다. canonical contract는 object의 관측 helper만 남기고 tobject는
  method-free로 닫는 방향이며, 이 차이는 아래의 열린 부채다.
- LLVM은 `object`/`tobject`를 immutable projection storage로, `tobject`만
  boundary-transfer contract로 분류한다.

같은 C layout을 사용할 수 있다는 사실은 같은 언어 계약이라는 뜻이 아니다.
storage sharing은 구현 세부사항이고 nominal kind가 의미 identity다.

### 실제 corpus 판정

2026-07-27의 `driver_bootstrap_main.pgy` 재귀 import closure는 403개 파일이며
missing import는 0이다. 그 실행 closure에서 이 문서의 domain 구성체는
`subject` 1개와 `action` 1개만 도달한다. `object`/`tobject`/`vessel`/`world`/
`zone`/`intent`는 0이다. 반대로 미도달 `compiler/world.pgy` 하나에는
`object` 18개, `tobject` 1개, `subject` 16개, `zone` 18개, `world` 1개,
`action` 16개가 선언돼 있다.

도달한 `DriverRung2Execution.EmitDirectMir`는 identity와 target admission,
artifact 검증, write/rejected transition을 실제로 소유한다. 미도달 action
16개는 모두 `Compiler*Ready()`를 반환하고 subject field를 읽지 않는
readiness facade다. 따라서 선언 수나 Pergyra다운 이름은 구조 적합성의
참고 자료일 뿐, 실행 개사료 진척은 아니다.

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
- requested -> admitted -> written/rejected 같은 stage transition;
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

`causes DamageEffect` 같은 domain effect와 `with effects io, alloc, authority`
같은 compiler effect mask, `with caps io_read, clock` 같은 runtime capability는
서로 다른 축이다. 특히 `with effects authority`는 zone의 `authority`와
`authorized by`가 소유하는 승인 provenance를 대신하지 않는다.

현재 production 예인 `DriverRung2Execution.EmitDirectMir`는 target admission,
`Requested -> TargetAdmitted -> ArtifactWritten/Rejected`, artifact identity
검증, 최종 write를 소유한다. backend artifact 생성 자체는 기존 typed owner에
남긴다. 따라서 이 action은 `REACHABLE`이지만 아직 C-owned compiler path를
대체한 `SUBSTITUTING`은 아니다.

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
5. production self-host import graph에서 이 계층 중 실제로 도달한
   Pergyra-native 경계는 현재 subject/action 하나뿐이다. 다른 구성체의 fixture
   수를 self-host substitution으로 세지 않는다.
6. `MakeSubject().Action()` 같은 temporary subject receiver는 C에서
   address-of-rvalue가 되고 LLVM에서는 임시 alloca로 보존될 가능성이 있다.
   두 backend가 다른 lifetime을 만들지 않도록 semantic에서 stable subject
   binding만 receiver로 허용하는 negative가 필요하다.
7. MIR method metadata는 현재 action identity와 `within`을 운반하지만
   `requires`/`authorized by`/capability/effect 계약 전체를 한 method fact로
   닫지는 않는다. backend가 AST를 재조회하지 않도록 action contract carriage
   owner를 명확히 해야 한다.
8. action 후 projection/effect sync가 C와 LLVM에 별도로 구현돼 있다. 동일한
   검증 plan을 두 backend가 소비하도록 합쳐야 하며, 출력 parity만으로 중복
   정책 소유를 정당화하지 않는다.
9. semantic helper `type_is_class_object_type()`은 이름과 달리 현재 subject만
   판정한다. 새 consumer를 붙이지 말고 subject identity 이름으로 바꿔
   오분류 가능성을 제거해야 한다.

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
4. 잘못된 execution identity가 `Rejected`이고 output을 만들지 않는 현재
   direct-MIR dynamic negative를 유지한다.
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
