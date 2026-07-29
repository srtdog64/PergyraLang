# 200. Object-to-Action 경계와 Domain Composition 작성 패턴

Updated: 2026-07-29 (Asia/Seoul)

이 문서는 `struct`에서 `action`까지의 값·identity 선택과, 그 action이
`effect`/`relation`/`zone`/`intent`/`world`로 합성되는 기준을 한곳에 모은
authoring contract다. 의미 사실의 최종 권위는 parser/semantic/MIR owner와 executable
gate다. 이 문서가 현재 source와 다르면 source를 근거로 문서를 고치며,
구현되지 않은 목표를 현재 동작으로 기록하지 않는다.

## 0. Objective card

- Objective: 값, 수동 도구, projection, 내부 상태 수용체, identity 주체,
  공적 전이를 서로 바꿔 쓰지 않게 하는 하나의 선택 규칙을 고정한다.
- Priority: nominal identity 보존; 전달과 receiver 의미 분리; mutation 권한;
  실제 boundary 의미; C/LLVM parity; 짧은 문법보다 정확한 진단.
- Fact owner: `NominalDeclKind`가 선언 identity를 소유하고
  `MIRDeclHeader.nominal_kind`가 이를 운반한다. receiver carriage, parameter
  carriage, field role/mutability, callable/action contract, call authority,
  runtime authority evidence는 아래의 서로 다른 fact family가 소유해야 한다.
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
canonical `vessel`은 일반 함수 인자로는 값 전달되고, hosted `func` receiver만
소유 subject 안의 상태를 갱신하도록 pointer-self여야 한다. 반대로 `class`는
값 전달과 value-self를 함께 사용한다. 현재 C/LLVM은 이 두 축을 아직
`uses_pointer_self` 하나로 합쳐서 **일반 vessel 파라미터도 포인터로 전달한다**.
이것은 canonical 계약이 아니라 아래에 고정한 구현 결함이다. receiver mode와
parameter carriage를 합치면 문서와 backend가 서로 다른 언어를 설명하게 된다.

구현에서는 하나의 “타입 종류” predicate가 아래 결정을 모두 대신하면 안 된다.
최소 fact family를 직교하게 나눈다.

| Fact family | 소유하는 질문 |
| --- | --- |
| `NominalKind` | struct/class/object/tobject/vessel/subject 중 무엇인가 |
| `FieldRole` | 일반 field, mutable field, subject-owned vessel, domain slot 중 무엇인가 |
| `ReceiverCarriage` | hosted self가 value-self인가 pointer-self인가 |
| `ParameterCarriage` | default/ref/inout 파라미터가 value, readonly indirect, identity reference, value-result 중 무엇인가 |
| `CallableKind` | free/hosted func인가 subject action인가 |
| `ActionContract` | requires/within/causes/authorized/caps/effects 선언 fact는 무엇인가 |
| `CallAuthorityBinding` | 이 호출에서 어떤 participant/zone authority가 결속됐는가 |
| `RuntimeAuthorityEvidence` | runtime이 어떤 token/identity/provenance를 실제 검증했는가 |

각 consumer는 필요한 fact 하나를 받고, nominal 이름이나 첫 authority row에서 다른
fact를 재추론하지 않는다. 특히 `ReceiverCarriage`를 일반 parameter ABI로 쓰거나
`ActionContract` 존재를 runtime 승인 증거로 쓰는 경로를 금지한다.

## 2. 현재 구현과 canonical authoring 행렬

| 구성체 | 존재/identity | 함수 경계 전달 | hosted receiver | construction 이후 상태 | 허용 동사 | 경계 의미 |
| --- | --- | --- | --- | --- | --- | --- |
| `struct` | 작은 구조 값, identity 없음 | 값 복사 | 없음 | 바인딩/필드 규칙에 따른 값 갱신 | free `func`의 입력/출력. hosted `func`는 semantic에서 거부 | 값 fact, 좌표, 옵션, 계획 행 |
| `class` | 수동 nominal 값/도구, identity 없음 | 값 복사와 값 반환 | value-self | `let mut` 필드는 직접 바인딩을 통해 갱신 가능. hosted func 안의 갱신은 호출자 identity 전이가 아님 | hosted `func` | 계산이 붙은 도구, formatter, policy value |
| `object` | local projection 값, 능동 identity 없음 | 값 복사 | value-self | construction 이후 read-only. source를 갱신하고 `refresh`하거나 새 view 생성 | 관측/format/query `func`만 권장. `action` 금지 | 동일 실행 경계의 refreshable read projection; 현재 ABI는 value projection |
| `tobject` | 외부 전달 값, identity/authority 없음 | materialized 값 복사 | 현재 passive helper는 value-self; canonical surface는 receiver 없음 | immutable. 새 snapshot을 `publish` | 현재 semantic은 passive `func`를 허용하지만 새 코드는 method-free. `action` 금지 | API/IPC/persistence/world 경계 transfer |
| `vessel` | subject-owned 수동 상태 값, 독립 actor identity 없음 | canonical은 값 전달. **현재 C/LLVM은 잘못 자동 간접 전달** | **pointer-self** | owner가 호출한 hosted `func`가 원본 내부 상태를 갱신 가능 | hosted `func`; `action` 금지 | subject 내부 상태·자원·규칙 수용체 |
| `subject` | identity-bearing active host | 기본 파라미터는 자동 참조; plain copy/rebind/value return 금지 | **pointer-self** | 자신의 `func`/`action`을 통한 원본 셀 전이 | private/local `func` + 명시적 public/boundary `action` 권장 | 결정, 승인, stage/resource handoff |
| `action` | subject의 동사이며 독립 타입이 아님 | subject receiver 계약을 따름 | subject pointer-self | 관측 가능한 성공/실패와 전이를 소유 | `requires`/`within`/`authorized by`/`causes`/`with caps`/`with effects` | 공적 상태 전이, 권한 행사, artifact/resource handoff |

### 2.1 이 구성체들은 상속 계층이 아니라 경계 프로토콜이다

`tobject -> object -> vessel -> subject -> action`을 “점점 강한 class”로 읽으면
안 된다. 각 구성체는 서로 다른 질문의 답이며 자동 승격 관계가 없다. Domain
구성체까지 포함한 canonical 책임 사다리는 다음과 같다.

| 구성체 | 한 문장 책임 | 생성·갱신 동사 | 만들지 말아야 할 때 |
| --- | --- | --- | --- |
| `struct` | identity 없는 계산 fact | construct/return | hosted behavior나 원본 identity가 필요할 때 |
| `class` | 복사 가능한 수동 계산 도구 | construct/새 값 return | 호출자 원본의 전이를 기대할 때 |
| `object` | 같은 실행 경계의 source-bound read projection | `refresh` 또는 새 view | 외부 전달 snapshot이나 mutable store가 필요할 때 |
| `tobject` | source lifecycle에서 분리된 immutable transfer value | `publish` | borrow, authority, method, live resource를 싣고 싶을 때 |
| `vessel` | subject가 소유하는 pointer-self 상태·자원 수용체 | subject가 hosted `func` 호출 | 독립 승인 주체나 public transition이 필요할 때 |
| `subject` | 복제 불가능한 결정·승인 identity | stable binding으로 construct | 단순 namespace나 상태 없는 helper만 필요할 때 |
| `action` | subject identity가 소유하는 관측 가능한 전이 | admit/commit/reject/handoff | 순수 계산, query, `*Ready()` facade일 때 |
| `effect` | participant에 적용·유지되는 시간적 상태 layer | apply/maintain/detach | 로그나 generic compiler effect mask의 다른 이름일 때 |
| `relation` | 두 participant identity 사이의 materialized edge | link/unlink/publish | 두 값을 잠시 함께 계산하기만 할 때 |
| `zone` | slot, authority, lifecycle, frontier의 실제 경계 | admit/refresh/publish/maintain/link | 함수 묶음이나 이름공간만 필요할 때 |
| `intent` | 현실의 목적을 닫고 coordination/authority/effect/boundary/compensation/trace fact를 한 선언 identity에 귀속하는 source-level binder | 목적에 필요한 step protocol과 검증 fact elaboration | action 호출을 묶기만 하고 독립적인 "왜"와 번들 의무가 없을 때 |
| `world` | 여러 실제 zone을 한 사실 그래프로 합성하는 root | zone/action composition | backend별 mini-world나 readiness dashboard가 필요할 때 |

이 사다리의 best practice는 키워드를 많이 쓰는 것이 아니라 **새 의미 경계가
생길 때만 다음 구성체를 추가하는 것**이다.

1. 값 계산은 `struct`/`class`/`func`에 남긴다.
2. local read model과 외부 transfer를 각각 `object.refresh`와
   `tobject.publish`로 분리한다. 하나의 mutable DTO가 둘을 겸하지 않는다.
3. 원본 상태는 subject-owned `vessel`에 두되, vessel은 authority를 판정하거나
   action을 시작하지 않는다.
4. `action`은 계산을 복제하지 않고 typed owner 결과를 받아 transition과 명시적
   실패만 소유한다.
5. `effect`와 `relation`은 action의 장식이 아니라 zone frontier의 독립 typed
   layer다. `causes`는 effect identity에, link는 relation identity에 결속한다.
6. `zone`은 admission/authority/resource lifetime과 topology를 소유한다. subject
   내부 상태 전이는 해당 subject action을 우회해 직접 쓰지 않는다.
7. production에서 호출되는 action이 둘 이상이고 하나의 성공·실패·보상 목적이
   생겼을 때만 `intent`를 추가한다. `world`는 그 실제 zone들을 한 선언·호출
   그래프로 묶으며 C/LLVM별 복제본을 만들지 않는다.

Self-host compiler도 이 규칙의 예외가 아니다. 모든 stage를 ceremonial
subject/action/intent로 감싸는 것은 개사료가 아니다. 실제 C-owned 결정을 대신하는
production call, 삭제된 bypass, 소비되는 result, negative gate가 있을 때만 해당
구성체가 `SUBSTITUTING`이다. 그 전에는 정확히 `SURFACE` 또는 `REACHABLE`로
기록한다.

### 2.2 재사용 가능한 다섯 경계 패턴

`tobject`에서 `action`까지를 하나의 승격 사다리로 구현하지 않는다. 실제 코드는
아래 패턴 가운데 필요한 행만 조합한다. 한 행의 출력이 다음 행의 입력이 될 수는
있지만, 타입이 자동으로 다른 구성체가 되거나 앞 구성체의 owner가 뒤 의미까지
소유하지는 않는다.

| 패턴 | canonical 흐름 | 최종 fact owner | 금지되는 축약 |
| --- | --- | --- | --- |
| local observation | `subject source -> object slot -> refresh -> query` | source identity는 subject, projection freshness/topology는 effect/relation/zone의 DIR row | object 직접 수정, object를 외부 DTO로 재사용, backend의 refresh 문자열 재탐색 |
| boundary publication | `subject source -> tobject slot -> publish -> transfer/receipt` | tobject는 materialized payload만 소유한다. source identity/freshness/edge는 enclosing directive와 `dir.domain_graph`, 전송 성공은 action/transaction result가 소유한다 | borrowed pointer·live slot·authority를 tobject에 싣기, mutable DTO, publish 없는 암묵 복사, receipt를 새 projection source로 재사용 |
| encapsulated state | `subject -> owned vessel -> hosted func` | actor identity/승인은 subject, 내부 state/resource는 vessel | vessel이 action/authority를 소유, free-function parameter ABI를 hosted receiver ABI에서 추정 |
| observable transition | `typed input fact -> subject.action -> Result/stage/tobject receipt` | action contract와 transition result, 하위 계산은 기존 typed owner | `*Ready()`/`return true` wrapper, action body의 semantic 재계산, 실패 시 old path 재진입 |
| intent-first domain purpose | 현실 목적을 `intent`로 먼저 닫고 participant/coordination/authority/effect/boundary/compensation/trace fact로 elaborate한 뒤 필요한 action/zone/world 경계를 유도 | intent는 binder와 attribution, 각 subfact의 최종 owner는 participant/zone/ability/authority/effect/coordination/compensation/trace 축에 유지 | action 개수로 intent 필요성을 판정, clause를 장식으로 추가, intent가 축별 사실을 재소유, backend별 world |

이 표에서 `object`와 `tobject`는 subject의 약한 버전이 아니고, `vessel`은 subject로
승격되기 전 단계가 아니다. `action`도 effect/relation/zone을 포괄하는 상위 타입이
아니다. 각 구성체는 identity, freshness, transfer, internal state, authority,
transition, orchestration 중 정확히 한 경계 질문에 답한다.

`object`와 `tobject`는 typed value 또는 graph endpoint일 수 있지만 edge identity,
source provenance, freshness, topology, authority를 소유하지 않는다. 그 사실은
enclosing effect/relation/zone의 `refresh`/`publish`/`bind` directive와
`dir.domain_graph`가 소유한다. 특히 publish가 끝난 `tobject`는 source lifecycle에서
분리된 receipt이므로 다른 projection의 fresh source로 재사용할 수 없다.

Self-host 구현도 같은 판정법을 사용한다. 올바른 Pergyra 컴파일러 파일 대부분이
`func`와 `struct`로 남는 것은 정상이다. 실제 source lifecycle 분리에는
`tobject`, identity-bearing 승인에는 `subject/action`, resource/lifetime frontier에는
`zone`, 현실 목적과 그 목적의 bundle-level 정적 의무에는 `intent`를 쓴다. action
개수는 intent 여부의 semantic 기준이 아니다. 단어가
import되거나 parser fixture에 나온다는 사실은 이 패턴이 실행된 증거가 아니다.

각 패턴의 구현 순서는 다음으로 고정한다.

1. 언어-word registry는 spelling/class/stable word identity만 선택한다.
2. owning parser는 해당 문맥에서 distinct typed node와 source identity를 만든다.
3. semantic/DIR owner는 field kind, action contract, projection/topology처럼 소비자가
   필요한 fact를 한 번 결정한다.
4. MIR은 owner fact와 그 identity를 lossless하게 운반하고, 새 identity epoch을
   만들면 declaration과 모든 dependent reference를 원자적으로 재발급한다.
5. target-neutral plan이 C/LLVM의 마지막 소비자 앞에서 한 번 만들어진다.
6. production entrypoint를 연결하고 옛 bypass를 삭제한 뒤, stale ID·name-only
   join·missing fact·zero-artifact failure를 negative gate로 고정한다.

이 여섯 단계 중 parser/문서/fixture까지만 있으면 `SURFACE`, production caller가
실제로 호출하고 결과를 소비하면 `REACHABLE`, C-owned 결정을 대신하고 우회가
삭제됐을 때만 `SUBSTITUTING`이다.

### 2.3 경계 레코드로 구현한다

`object`부터 `action`까지의 best practice는 타입을 단계적으로 승격하는 것이
아니라, 필요한 경계마다 아래 여섯 사실을 하나의 typed record로 끝까지 운반하는
것이다.

| 사실 | 답해야 하는 질문 | 대표 owner |
| --- | --- | --- |
| source identity | 어느 원본의 사실인가? | subject/declaration field identity |
| boundary kind | local observation, detached transfer, internal state, public transition 중 무엇인가? | nominal kind + field role |
| materialization | borrow/view인가, 복사 snapshot인가, stable owned cell인가? | projection/constructor/ABI fact |
| operation | `refresh`, `publish`, hosted `func`, `action` 중 무엇이 경계를 움직이는가? | topology row 또는 callable contract |
| authority/lifecycle | 누가 허용하고 언제 유효한가? | subject/action, zone, transaction owner |
| observable outcome | freshness, receipt, stage, failure 중 무엇을 caller가 소비하는가? | typed result/runtime state fact |

이 레코드의 일부를 nominal 이름이나 backend layout에서 역산하지 않는다. 예를
들어 `tobject`라는 이유만으로 전송 성공을 주장할 수 없고, `subject`가 C pointer로
내려갔다는 이유만으로 runtime authority가 증명되지 않는다. `action`이 존재해도
그 결과가 caller의 결정을 바꾸지 않으면 transition dogfood가 아니다.

Self-host compiler에 적용할 때는 다음 조합을 기본으로 한다.

```text
typed computation facts (struct/class/func)
  -> local read model이 필요할 때만 object + refresh
  -> lifecycle이 분리된 handoff가 필요할 때만 tobject + publish/receipt
  -> 장기 내부 상태가 필요할 때만 subject-owned vessel + hosted func
  -> identity-bearing 승인/commit/reject가 필요할 때만 subject.action
  -> resource/lifetime frontier가 실제로 있을 때만 zone
  -> 현실 목적과 cross-axis 번들 의무가 있을 때 intent
```

따라서 Pergyra다운 구현은 키워드를 많이 쓰는 구현이 아니라, 실제 경계마다
필요한 키워드 하나를 쓰고 그 typed fact가 production consumer까지 살아남는
구현이다.

Self-host fact builder도 이 규칙을 따른다. `struct` 결과의 growable array member를
`ArrayPush(result.rows, ...)`처럼 live mutable builder로 사용하지 않는다. 배열은
함수-local owner로 수집하고, 모든 cardinality가 맞은 뒤 immutable fact record를
한 번 생성해 반환한다. 이 패턴은 C식 out-parameter 묶음이 아니라 “검증 전
builder storage”와 “검증 뒤 fact identity”의 lifetime을 분리한다. 중간 실패는
부분 record를 내보내지 않고 명시적 invalid fact를 반환한다.

### 2.4 Runtime 경계의 반복 패턴과 owner 분리

Native C/LLVM과 self-host 경로를 함께 감사하면, `object`부터 `zone`까지는
겉모양이 달라도 런타임에서 같은 다섯 질문을 반복한다.

1. 어느 선언·필드·callable identity의 사실인가?
2. 값인가, immutable snapshot인가, stable mutable identity인가?
3. source를 destination의 어느 **역할과 member**에 결속하는가?
4. 어떤 operation이 materialize/mutate/synchronize하는가?
5. 성공·실패·freshness·receipt 중 무엇을 caller가 관측하는가?

이 질문들은 하나의 nullable mega-row가 아니라 수명이 다른 owner fact로 나눈다.

| Fact family | semantic owner가 결정할 것 | 마지막 소비자 | 금지 fallback |
| --- | --- | --- | --- |
| `NominalBoundaryFact` | object/tobject/vessel/subject/effect/relation/zone identity와 field role | layout·constructor admission | C layout 또는 이름으로 nominal kind 복원 |
| `CallableReceiverCarriage` | callable ID별 value-self, readonly-reference, mutable-identity self | C/LLVM callable ABI | nominal kind 하나로 일반 parameter ABI까지 결정 |
| `DomainParticipantRoleFact` | effect bearer, relation source destination, relation target destination의 exact field ID/type | lifecycle bind operation | 첫 bindable field, 0/1 ordinal, `by participant` 대용 |
| `DomainProjectionMemberAssignment` | directive/slot identity와 target/source member path의 각 field ID/type | refresh/publish renderer | backend same-name 탐색, field order, missing source zero-fill |
| `DomainLifecycleOperation` | apply/maintain/detach/link/unlink와 필수 zone/layer/endpoint/state identity | target-neutral runtime scheduler | backend AST lifecycle 재순회, unknown operation 무시 |
| `DomainLayerMaterialization` | storage identity/type/capacity, initial state, required bind/sync operation | constructor와 sync emitter | aggregate zero를 materialization 성공으로 간주 |

`VerifiedDomainRuntimePlan`은 위 fact를 exact join한 **admission receipt**일 수는
있지만 원본 의미의 새 owner가 아니다. receiver carriage는 callable ABI owner에,
participant role과 member assignment는 semantic/DIR owner에, materialization과
lifecycle operation은 domain runtime owner에 남긴다. C/LLVM은 같은 admitted
operation 배열을 렌더링할 뿐 다시 선택하지 않는다.

구성체별 best practice와 현재 falsifier는 다음과 같다.

| 경계 | canonical runtime 규칙 | 현재 닫아야 할 falsifier |
| --- | --- | --- |
| `object` | local source-bound value projection. semantic이 refresh member assignment를 exact ID/type/path로 확정 | 같은 이름의 다른 field ID, missing source, 직접 mutation |
| `tobject` | source lifecycle과 분리된 immutable publish snapshot. routine receiver를 두지 않고 graph/source/authority를 payload에서 복구하지 않음 | borrowed/live state 포함, passive hosted method, publish 없는 전달, zone constructor 선주입, valid-ID receipt를 projection source로 재사용 |
| `vessel` | subject-owned stable cell. hosted receiver만 pointer-self이고 일반 parameter carriage와 분리 | default/ref parameter가 의도 없이 caller state를 바꾸는 경우 |
| `subject` | stable identity가 func/action transition을 소유. temporary receiver 금지 | by-value self, address-of-rvalue, copy/rebind/value return |
| `action` | subject-owned callable contract와 호출별 authority binding, 명시적 outcome을 소유 | `*Ready()` facade, first-authority 선택, 실패 후 old path 재진입 |
| `effect` | zone target을 declaration의 explicit bearer destination에 bind하고 apply/maintain/detach 뒤 sync | 첫 bindable slot 사용, missing role에서 silent return |
| `relation` | zone left/right를 explicit source/target destination에 bind하고 link/unlink 뒤 sync | declaration field 0/1을 endpoint role로 간주 |
| `zone` | slot/layer storage, membership, lifecycle, frontier와 pointer-self sync를 소유 | layer zero-fill 성공, by-value self, backend별 lifecycle 재구축 |

`by participant`는 transition initiator/provenance이지 effect bearer나 relation
destination role이 아니다. implicit same-name mapping은 Pergyra의 좋은 기본값으로
유지할 수 있지만 **semantic owner가 한 번 exact assignment로 확정**해야 한다.
backend에서 다시 same-name을 선택하는 것은 편의가 아니라 이중 권위다.

현재 runtime 경로의 폐쇄 상태는 다음과 같다.

- Native semantic은 effect bearer와 relation source/target role 및 projection
  member/path를 exact declaration·field ID/type로 한 번 결정한다. HIR/DIR/MIR과
  `pgy.mir.v1`은 이 사실을 lossless하게 운반한다.
- Native C의 domain sync는 더 이상 `emit_projection_literal_by_zone_refresh_metadata`
  또는 missing-source `.field = 0`을 쓰지 않는다. LLVM domain sync도 same-name/
  nested-name value 탐색과 participant ordinal을 삭제하고 같은 MIR fact family를
  소비한다.
- Self source producer도 같은 typed row를 만들고, machine admission이 topology,
  role, member/path를 한 번 exact join해 target-neutral plan을 만든다. 이후 lookup과
  codegen view는 whole-plan/whole-owner validation을 반복하지 않는다.
- Domain constructor input은 declaration source order를 보존한 identity/admission
  binding뿐이다. Zone은 `subject slot`/`binding slot`, relation/effect는 `for ...`
  header participant를 사용한다. `object`/`tobject` projection destination과
  effect/relation layer/shared storage는 topology/runtime owner가 construction 이후
  materialize하며 caller가 값을 선주입할 수 없다. Native C/LLVM과 self semantic이
  같은 정책을 소비한다.
- `binding slot`은 object-valued endpoint를 zone에 들이는 명시적 admission 역할이다.
  `object slot`을 admission과 projection에 동시에 쓰지 않으므로 constructor ABI가
  projection directive 유무에 따라 암묵적으로 바뀌지 않는다.
- 이 좁은 binding admission/runtime slice는 production
  `CompileSourceToCVerified`에서 direct-source C와 admitted-MIR C가 byte-equal하고,
  self/native C/LLVM이 `door=5`, `key=9`, `view=5`를 동일하게 실행한다. 유효한 field
  ID를 유지한 `binding_slot -> object_slot|tobject_slot` 변조도 constructor policy가
  partial C 전에 거부하므로 이 slice만 `SUBSTITUTING`이다. 이것은 `tobject`나
  compiler root 전체의 승격 근거가 아니다.
- `object slot ... = ...`와 `tobject slot ... = ...`도 projection materialization
  경로가 아니다. 이 syntax는 한때 parser/semantic에서 받아들였지만 DIR/MIR과 두
  backend가 initializer를 소유하지 않아 실제 실행에서는 zero-filled storage로
  사라졌다. Semantic은 이제 이를 fail-closed하고 `object -> refresh`,
  `tobject -> publish`, target kind 선택이 필요하면 `bind`만 허용한다.
- Native semantic, native MIR verifier, self DIR producer, self MIR admission 모두
  `tobject` source를 거부한다. `tobject` field ID를 유효한 다른 source ID로 바꾼
  malformed MIR도 C artifact 전에 실패한다.
- Production DRV-2 직접 source entrypoint는 source -> self MIR -> admission -> general
  C를 통과한다. `zone_layer_projection_runtime`의 명시적 MIR 경로와 byte-equal C를
  만들며 둘 다 `7`과 `dst`를 실행한다.
- 아직 남은 금지 경로는 일반 명시적 projection builtin의 이름 기반 변환,
  LLVM world-effect의 first-subject helper, constructor/dirty scheduling의 옛
  refresh metadata, native C/LLVM별 runtime-fact validation 중복이다. 또한 MIR
  declaration row에 declaration-level `source_syntax_id`가 없어 단발성 foreign
  positive declaration ID를 inventory와 완전 exact join하지 못한다.

첫 executable falsifier였던 self MIR -> C `7`/`dst`는 GREEN이다. Explicit self
`map { ... }`도 parser child -> DIR parent association -> semantic exact path/
assignability -> MIR carrier -> admitted plan -> general C를 통과한다. `life: Long <-
hp: Int`가 문자열 equality를 falsify하며 no-map, type mismatch, missing source,
duplicate target은 output 전에 실패한다.

다음 closure는 declaration-level identity carriage와
materialize/bind/sync/dirty/epoch/lifecycle operation을 한 native/self
target-neutral plan으로 통합하고, 같은 map owner를 실제 world/intent production
path에서 재사용하는 것이다. 현재 exact eager method-entry bind/sync와 explicit
effect/relation map은 해당 fixture를 대체하지만 전체 lifecycle scheduler나 compiler
자체의 subject/action orchestration 완료 증거는 아니다.

### 구현 근거

- parser는 여섯 nominal을 서로 다른 `NOMINAL_DECL_*`으로 만든다.
- MIR declaration header의 hosted self policy에서 callable별
  `receiver_carriage`가 파생되어 native/self JSON과 machine admission을 통과한다.
  현재 C/LLVM 일반 parameter ABI가 더 넓은 `uses_pointer_self` fact를
  재사용하는 것은 열린 결함이며 별도 parameter-carriage owner로 분리해야 한다.
- semantic은 subject의 plain copy/rebind/value return을 거부하고 기본
  parameter를 reference semantics로 취급한다.
- semantic은 `object`와 `tobject` field assignment를 각각 refresh/publish
  위반으로 거부한다.
- semantic은 zone constructor에서 projection/layer storage 인수를 거부하고,
  detached `tobject`를 다른 projection의 source로 쓰는 것도 거부한다.
- `struct` hosted method는 semantic에서 거부한다.
- parser와 semantic은 `action`을 `subject` 안에서만 허용한다.
- top-level `public`/`export`는 native와 self-host parser 모두 같은 AST
  `[export]` fact를 만든다. `private`은 non-export다. 특히 다른 owner가
  참조하는 `public zone`의 visibility를 parser가 장식처럼 버리면 action의
  `within` 경계가 self-host import 그래프에서 사라진다.
- 현재 semantic unit은 `object`/`tobject` passive helper `func` 선언을
  허용한다. canonical contract는 object의 관측 helper만 남기고 tobject는
  method-free로 닫는 방향이며, 이 차이는 아래의 열린 부채다.
- LLVM type metadata는 `object`/`tobject`를 immutable projection storage로,
  `tobject`만 boundary-transfer contract로 분류한다. 그러나 현재 소비자는
  `ToObject` borrow 등록에 치우쳐 있고 store 거부까지 보장하지 않는다. 이
  metadata 이름을 backend 불변성 폐쇄 증거로 세지 않는다.

`action`의 기본 visibility는 현재 private다. “공적 action”은 의미 역할과 권장
authoring이지 자동 export 규칙이 아니다. import 경계를 넘겨야 하는 action만
`public`을 명시하고, local orchestration action은 private로 남길 수 있다.

같은 C layout을 사용할 수 있다는 사실은 같은 언어 계약이라는 뜻이 아니다.
storage sharing은 구현 세부사항이고 nominal kind가 의미 identity다.

### 실제 corpus 판정

2026-07-29의 `driver_bootstrap_main.pgy` 재귀 import closure에는 world 1개와
concrete zone 20개가 있다. 전체 declaration census는 gate가 생성하는 관측값이며,
새 실행 경계의 진척은 선언 수가 아니라 call-site와 outcome 소비로 판정한다.

이 수치는 import 표면 census이지 모든 선언의 실행 call-site 증거가 아니다.
실제 production call chain은 `PgyCompilerWorld`의 `direct_mir`와 `source_mir`
zone을 지난다. `DriverRung2Execution.EmitDirectMir`는
identity admission, target admission, atomic artifact commit을 실제로 소유한다.
commit은 `tobject SelfMirArtifactReceipt`가 있을 때만 typed success variant로
전이하며 이 receipt의 payload는 실제 소비된다. `SelfMirArtifactFailure`도 typed
failure variant로 Main까지 전달되고 exact stage/status/recovery payload가 진단과
실패 판정을 바꾼다.
receipt는 atomic visibility만 주장하고 crash durability는 명시적으로 `false`다.
`DriverSourceMirExecution.EmitSourceMir`는 subject/topology identity, full-driver
pressure mode, 정확히 한 source-to-MIR payload producer, 한 atomic commit을
소유한다. Main은 이 action의 typed receipt/rejection/failure outcome을 소비한다.
`world.pgy`의 기존 action 16개는 import closure에는 들어왔지만
계속 `Compiler*Ready()`를 반환하는 readiness facade이며 현재 production
chain에서 호출되지 않는다. 따라서 선언 수나 Pergyra다운 이름은 구조
적합성의 참고 자료일 뿐, 실행 개사료나 C-path 대체 진척은 아니다.

| 구성체 | production dogfood 판정 | 근거 |
| --- | --- | --- |
| `struct` | `REACHABLE` supporting construct | production 계산에는 실제 사용되지만 구성체 자체가 C-owned path를 대체했다는 독립 증거는 아님 |
| `class` | `SURFACE` | bootstrap closure 선언 0, backend fixture만 존재 |
| `object` | `SURFACE` | active action call chain의 domain-object 소비 없음 |
| `tobject` | `REACHABLE`, not `SUBSTITUTING` | receipt/failure가 production caller까지 typed variant로 전달되고 양쪽 payload가 소비됨 |
| `vessel` | `SURFACE` | bootstrap closure 선언 0 |
| `subject` / `action` | `REACHABLE`, not `SUBSTITUTING` | direct-MIR과 source-to-MIR subject/action 두 쌍이 production에서 호출되고 outcome이 소비됨 |
| `zone` / `world` | `REACHABLE`, not `SUBSTITUTING` | 한 world의 ordered executable zone 두 개가 호출됨; authority runtime 증거는 아직 bounded admission |
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

구성체 등급과 실행 증거를 섞지 않기 위해 각 선언은 다음 사다리의 마지막으로
관측된 칸까지 별도로 기록한다.

```text
IMPORTED -> MATERIALIZED -> INVOKED -> OUTCOME_CONSUMED -> SUBSTITUTING
```

- `IMPORTED`는 parser/import graph에 들어왔다는 뜻뿐이다.
- `MATERIALIZED`는 production path가 실제 값을 생성했다는 뜻이다.
- `INVOKED`는 receiver/action이 production caller에서 호출됐다는 뜻이다.
- `OUTCOME_CONSUMED`는 tag 존재가 아니라 payload나 전이 결과가 후속 결정을
  바꿨다는 뜻이다.
- `SUBSTITUTING`은 여기에 더해 실제 C-owned 경로를 대체하고 우회가 삭제됐다는
  뜻이다.

현재 domain object는 `IMPORTED`에서 멈춘다. Artifact receipt와 failure는
production direct-MIR 및 source-to-MIR action에서 caller까지 typed variant로
전달되고 `OUTCOME_CONSUMED`에 도달한다.
`DriverRung2Execution.EmitDirectMir`와
`DriverSourceMirExecution.EmitSourceMir` 두 쌍이 `INVOKED`와 result consumption에
도달한다. 나머지 readiness/intent 선언과
문법·lowering fixture는 canonical authoring 예가 아니라 surface 회귀 자료다.

### 실행 call-site 감사가 보여 준 경계

현재 production 호출 그래프에서 실제로 호출되는 Pergyra-native hosted action은
`DriverRung2Execution.EmitDirectMir`와
`DriverSourceMirExecution.EmitSourceMir`다. domain `object`는 schema/slot
surface이며 construction, `ToObject`, `refresh` production call이 없다.
`tobject SelfMirArtifactReceipt`와 `SelfMirArtifactFailure`는 transaction 결과의
모든 payload field가 bootstrap caller의 성공 판정 또는 실패 진단을 바꾼다.
`ParityVerdict`는 surface다.
나머지 compiler action은 호출되지 않는 readiness facade다.

이 경계의 terminal result는 `ok + stage` 중복 tag struct가 아니다.
`DriverRung2ExecutionOutcome`이 성공 receipt, 일반 rejection, artifact failure를
서로 다른 payload variant로 소유한다. 성공 receipt는 schema, exact target/path,
atomic visibility와 durability를 검증하고, artifact failure는 stage/status,
prior-final 보존과 temp 제거 여부를 transaction owner가 검증·render한다. Unknown
status와 known-but-wrong target도 fail closed한다. Detached payload에는 subject
authority, source identity/freshness 또는 topology identity를 넣지 않는다.
Source path는 별도 `DriverSourceMirExecutionOutcome`이 success receipt, admission
rejection, artifact failure를 구분하고 source path, request mode, final path를
검증한다. Subject/topology identity는 action admission에만 쓰며 detached receipt가
권한을 재구성하게 하지 않는다.

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
  -> intent when a real-world purpose and its bundle obligations exist
```

이 순서는 컴파일러 구현의 하향식 call graph가 아니라 경계 구현 체크리스트다.
언어 설계·문서 읽기 순서는 `intent -> world -> zone -> subject`가 정본이다.
단 하나의 action만 호출하더라도 참여자·권한·경계·효과·보상·추적을 한 목적
identity에 귀속하고 bundle-level 검증 의무를 방출한다면 intent일 수 있다. 반대로
action이 여러 개라는 이유만으로 intent를 만들면 단순 orchestration wrapper다.

action 선언의 **typed carriage**는 이제 구현돼 있다. self-host typed arena는
`Action:`과 `Function:`을 서로 다른 callable kind로 보존하고,
`SemanticAstActionContractFacts`가 callable `SyntaxNodeId`에 subject-only identity,
`requires`/`within`/`causes`/`authorized by`, caps/effects, body identity를 결속한다.
codegen은 이 owner가 지정한 clause node만 정확한 순서로 소비하며 `Body:`를
찾기 위해 중간 행을 건너뛰지 않는다. Native/self MIR declaration은 같은
`callable_kind + contract` wire를 방출하고 `mir_lower`는 이를 한 번 읽어 action
AST를 복원한다.

`semantic.callable_contract_vocabulary`는 capability 9개와 effect 9개의 spelling,
stable identity, mask symbol 연결, canonical rank, capability manifest 이름과
`local` zero-exclusive 정책을 하나로 소유한다. Native parser·AST/MIR renderer·
structured-comment effect·runtime grant와 self-host parser·semantic·MIR verifier는
모두 이 owner 또는 생성 projection을 소비한다. `all`/`none`은 runtime grant
alias일 뿐 source `with caps` 어휘가 아니다.

`function_clause_order_minimal` focused gate는 C와 LLVM driver 각각에서 native/self
MIR의 contract byte row를 확인한다. 기존 여섯 field 변조에 더해 unknown,
duplicate, noncanonical order, `local + nonlocal` 양방향 변조가 backend output 전에
fail closed한다. 따라서 declaration carriage와 vocabulary fact family는 `CLOSED`다.
다만 이 closure는 C-owned compiler path를 대체하지 않는 supporting semantic
seam이다. Production source-to-MIR action의 직접 file-helper 우회는 삭제됐지만
그 변화도 Pergyra 내부 orchestration 교체다. Dogfood 상태는 계속 `REACHABLE`,
not `SUBSTITUTING`이다.

이 fixture가 드러낸 인접 경계도 같은 규칙을 따른다.

- role이 여러 `impl ability`를 소유할 때 선언 전체를 비우는 임시 제한은 허용하지
  않는다. 각 impl의 method span은 이름 추측이 아니라 ability semantic owner의
  method count로 분할하고, 전체 role method span을 정확히 덮지 못하면 실패한다.
- zone의 `subject/object/tobject/effect/relation slot`은 문자열 장식이나 일반
  field가 아니라 semantic `field_kind`를 가진 typed edge다. `field_kind`는 field
  이름이나 참조 타입에서 복원하지 않으며, 선언 종류별 허용 행렬을 한 owner가
  판정한다.
- 현재 `mir_decl_field_kind_vocabulary.def`가 14개 wire identity의 spelling과 AST
  projection label을 소유한다. Native C와 self-host는 직접/생성 projection을
  소비하며 `Damage.bearer=subject_slot`, `BattleZone.damage=effect_slot`을 MIR
  wire와 canonical AST까지 보존한다. 누락, unknown, 선언-host 불일치, effect의
  required participant cardinality 손실은 backend output 전에 실패한다.
- 이 증거는 effect/relation declaration kind와 slot-role carriage의 부분
  closure다. Stable declaration-field identity와 non-empty canonical remap도 이후
  닫혔다. Owner declaration identity join, pool capacity, vessel/binding slot,
  projection member map, layer destination role, zone state/lifecycle operation과
  C/LLVM runtime ABI는 열린 층이다. 따라서 전체 domain declaration 또는 runtime
  ABI를 `CLOSED`로 기록하지 않는다.

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

순수 라이브러리나 단일 계산에는 목적 binder를 의식적으로 만들지 않고 필요한
`struct`/`class`와 `func`만 둔다. 반대로 도메인을 설계할 때는 intent가 닫을 현실
목적을 먼저 판정하고 그 목적에서 world/zone/subject/action 경계를 유도한다.
Action 수가 아니라 participant·coordination·authority·effect·boundary·
compensation·trace의 bundle-level 귀속과 검증 의무가 intent 추가 기준이다.

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

### 4.1 ActionContract SoT 패턴

권장 파이프라인은 다음 하나다.

```text
parser clause nodes
  -> SemanticAstActionContractFacts(callable SyntaxNodeId)
  -> MIRDeclMethod callable_kind + contract projection
  -> pgy.mir.v1 method contract wire
  -> MirDeclarationMethodContractFact
  -> exact Action AST reconstruction
  -> target-neutral C/LLVM consumers
```

- parser는 순서와 표면 syntax를 소유하지만 action 의미의 최종 owner가 아니다.
- semantic owner는 callable 하나당 contract 하나를 만들고 subject-only,
  clause 순서/중복, body identity를 판정한다. consumer가 AST text를 다시 읽어
  이 판단을 복제하지 않는다.
- MIR method는 `callable_kind`와 `contract`를 함께 검증한다. `action`을 기본
  `function`으로 추정하거나 contract가 없을 때 빈 contract를 합성하지 않는다.
- `caps_present`/`effects_present`는 값 mask와 별도다. 생략과 명시된 빈 목록을
  같은 상태로 접지 않으며, 현재 명시된 빈 목록은 malformed wire로 거부한다.
- `within`은 문자열을 출력하는 것으로 끝나지 않는다. declaration index의
  실제 `nominal_kind=zone` identity에 결속돼야 한다.
- declaration carriage, call-site authority binding, runtime identity/ability
  authorization은 서로 다른 증거 층이다. 첫 층이 닫혀도 뒤의 두 층을 완료로
  기록하지 않는다.
- role impl partition, zone slot carriage, effect runtime ABI 역시 서로 다른 owner
  층이다. 앞 층의 결함이 뒤 층 증상을 만들더라도 빈 선언이나 unknown-type
  fallback으로 덮지 말고 최초로 사라진 fact에서 닫는다.
- `io_read` 같은 capability와 `secure` 같은 effect는 lexer 예약어 목록이 아니라
  `semantic.callable_contract_vocabulary`의 closed values다. lexer는 `with`, `caps`,
  `effects` 같은 grammar selector를 소유하고, semantic registry가 목록 membership,
  mask, canonical wire 순서와 `local` 배타성을 소유한다. 이 둘을 합치면 문법과
  의미 ABI가 다시 한 테이블에 뒤섞인다.

이 패턴의 핵심은 clause를 많이 쓰는 것이 아니라, action의 권한·전이 계약을
한 identity로 끝까지 운반하는 것이다. 계산-only 함수에 빈 action contract를
붙이거나 모든 compiler stage를 subject/action으로 감싸는 것은 이 패턴에
해당하지 않는다.

### 4.2 Nominal declaration과 field-kind SoT 패턴

권장 파이프라인은 다음 하나다.

```text
parser field node + SyntaxNodeId
  -> SemanticAstNominalConstructorFacts
       (nominal_kind, field_kind, source_syntax_id, referenced type identity)
  -> SelfMirDeclarationRows
  -> pgy.mir.v1 declarations[].fields[]
  -> exact SubjectSlot/ObjectSlot/TObjectSlot/EffectSlot/RelationSlot reconstruction
  -> DIR domain edge와 ABI/runtime projection
```

`field_kind`는 출력 label이 아니라 semantic discriminant다. 일반/shared storage,
subject/object/tobject participant slot, effect/relation slot과 pool, world/roster
slot을 구별한다. Consumer는 declaration name, field name, type spelling 또는
대문자 여부로 이를 다시 판정하지 않는다.

`subject_slot`은 subject identity를, `object_slot`은 local projection을,
`tobject_slot`은 detached transfer를 참조한다. `effect_slot/effect_pool`은 effect
declaration만, `relation_slot/relation_pool`은 relation declaration만 참조해야
한다. `action`은 field kind가 아니며 subject-owned method contract로 남는다.
`within`과 `causes`는 각각 zone/effect declaration identity에 결속한다.

Field reference의 최소 join key는 `(owner declaration, field name,
source_syntax_id, field_kind)`다. 이름은 진단과 authoring identity이고 ID는 같은
revision 안의 구조 identity이며 kind는 그 field가 맡을 수 있는 역할을 제한한다.
셋 중 하나라도 생략하면 `player` 이름에 `enemy`의 유효한 ID를 붙이거나,
subject slot을 object slot로 바꾼 forged row를 구별할 수 없다. Declaration field
index는 문서당 한 번 만들고 topology/action consumer가 공유한다. Edge마다
`declarations[]`를 다시 순회하거나 name-only lookup을 복원하지 않는다.

누락·unknown kind, host-kind 불일치, effect/relation 교환, required participant
cardinality 손실, pool capacity 누락, 이름-only declaration join은 모두 fail
closed한다. Identity-insensitive payload parity와 canonicalized same-epoch MIR
parity는 필요하지만 raw producer ID 숫자 equality는 요구하지 않는다. Exact AST
reconstruction과 C/LLVM runtime layout/operation parity가 없으면 domain runtime
closure가 아니다.

현재 focused `function_clause_order_minimal` C shard는 effect/zone field kind와
effect participant shape를 포함한 native/self MIR parity, canonical reconstruction,
생성 C compile/run을 증명한다. 이는 `BRIDGE`/`SURFACE` 증거이며 production
call graph를 바꾸지 않았으므로 hard substitution 진척으로 세지 않는다.

### 4.2.1 Domain runtime topology의 첫 executable slice

field kind가 “이 field가 어떤 종류인가”를 소유한다면 domain topology는 “어떤
slot의 변화가 어느 slot을 다시 계산하게 하는가”를 소유한다. 둘을 한 문자열
목록으로 합치지 않는다. 현재 canonical 경로는 다음 하나다.

```text
AST domain directive
  -> DIR lowering boundary
  -> DIRDomainTopologyRow              # semantic owner
  -> MIRDomainTopologyRow              # owned copy/carrier
  -> target-neutral PropagationGraph
  -> C/LLVM zone frontier pass limit
```

현재 executable row의 의미는 다음과 같다.

| row | graph contribution | 작성 의미 |
| --- | --- | --- |
| projection refresh/publish/bind | source slot -> object/tobject projection slot | source identity가 바뀐 뒤 projection을 갱신하거나 전달한다 |
| apply effect | 없음; runtime lifecycle plan이 소비 | inactive layer를 target에 결속하고 `APPLY` transition을 시작한다 |
| maintain effect | effect layer slot -> target subject slot | 유지되는 effect layer가 target frontier에 영향을 준다 |
| link relation | left/right subject slot -> relation layer slot | 양 endpoint identity가 relation materialization의 선행 사실이다 |

`apply-effect`와 `maintain-effect`는 같은 layer/target을 가리킬 수 있어도 같은 row가
아니다. 전자는 새 binding과 `PGY_PROP_CAUSE_APPLY`를 만드는 one-shot lifecycle
operation이고 dependency schedule에 edge를 추가하지 않는다. 후자는 기존 binding의
유지와 `PGY_PROP_CAUSE_MAINTAIN`을 뜻하며 frontier edge를 만든다. Graph adjacency를
runtime transition identity로 사용하거나 둘을 한 enum 값으로 접지 않는다.

각 row는 owner declaration, directive, participant/layer/endpoint slot의 stable
`SyntaxNodeId`를 함께 운반한다. Backend는 zone AST를 다시 열어 `refresh`,
`maintain`, `link` 문자열을 찾지 않는다. MIR lowering도 아무 DIR이나 받지 않고
HIR과 같은 source-program identity인지 확인한다. DIR 누락, row 누락, slot identity
손상, unknown owner는 fail closed한다.

`zone_layer_projection_runtime`에서 relation `trust`는 `player`와 `enemy` 두
endpoint에 의존하므로 native C/LLVM graph는 모두 3 node, 2 edge, depth 2다.
다만 runtime loop의 count floor는 3이라 graph가 비어도 최종 limit/output이 같을
수 있다. 따라서 best practice는 stdout parity만 보는 것이 아니라 exact graph
trace와 옛 AST entrypoint 부재를 함께 gate하는 것이다.

이 slice는 기존 C-owned zone frontier AST graph builder를 삭제했으므로 native
C/LLVM 경계에서 `SUBSTITUTING`이다. Native `pgy.mir.v1`도 이제 relation declaration과
optional `domain_topology` object에 stable-ID 값을 운반한다. Self-host `mir_lower`는
이를 typed `MirDomainTopologyFacts`로 한 번 admit하고 declaration field kind,
relation participant cardinality, null/name-ID shape, directive uniqueness를 fail
closed로 검사한다. `TrustedLink` relation도 typed declaration으로 복원된다.

Declaration JSON field는 이제 nonzero `source_syntax_id`와 `field_kind`를 함께
운반한다. Native validator와 self-host `MirProgramDeclarationFieldIdentityIndex`는
owner 안에서 `(name, ID, kind)`를 exact join하고, 문서 전체 duplicate field ID,
owner 내부 duplicate name, missing/zero ID를 거부한다. 따라서 이름은 `player`로
둔 채 `enemy`의 유효한 ID를 붙인 row와, 같은 name/ID를 잘못된 field kind로
바꾼 row가 backend 전에 실패한다. 이 admission delta는 production C path를
대체하지 않으므로 증거 등급은 `REACHABLE` supporting이다.

Raw ID 숫자의 native/self equality는 계약이 아니다. Native parser의 preorder
`SyntaxNodeId`와 현재 self-host compact typed-arena identity는 서로 다른 producer와
revision의 identity epoch다. 유효한 join은 한 MIR 문서와 그 문서에서 파생된
consumer 안에서만 한다. MIR-to-AST canonicalization처럼 새 프로그램을 만들면
declaration field와 이를 참조하는 topology row의 ID를 한 번에 재발급해야 한다.
숫자 offset, AST text row ordinal, name hash로 native ID를 흉내 내지 않는다. 현재
canonical identity epoch owner는 declaration field와 dependent topology ID를
원자적으로 재발급한다. 과거에는 이 owner가 없어 non-empty topology를 fail closed로
거부했으며, 그 이전 동작을 compatibility fallback으로 되살리지 않는다.

이전 JSON/admission slice 자체의 판정은 `REACHABLE`이었다. 그다음 bounded
executable rung에서는 self-host source producer가 “topology row는 없지만 DIR graph는
존재하는” 문서를 직접 소유하게 되었다. `function_clause_order_minimal`에서 typed
`Authority`와 declaration/role/ability/slot facts를 join해 native와 같은
`nodes=9`, `edges=16`, `domain_graph_id=14937235029576152731`을 계산하고,
self-produced MIR을 다시 소비한 C가 `clause-order-minimal`을 출력한다. 이 좁은
empty-topology producer만 `SUBSTITUTING`이다.

그 checkpoint에서 empty는 declaration 수나 topology row 수를 뜻하지 않았다. DIR census를
완성한 결과 row가 0개임을 뜻한다. `Refresh`/`Publish`/projection `Bind`/
`Maintain`/`Link`/`Apply`/`Detach`/`Unlink`/`State`는 서로 다른 typed kind로
보존됐고, 당시 owner는 하나라도 발견하면 빈 graph로 낮추지 않고 fail closed했다.
MIR canonical bridge도 이미 admit된 empty topology를 그대로 운반한다. authority가
빠지는 MIR-to-AST projection에서 graph를 재계산하거나 native oracle 값을 self
source producer에 붙이지 않는다.

그 당시 Pergyra graph plan과 production runtime consumer는 없었으므로 전체
domain/action runtime은 `BRIDGE`였다. 다음 rung의 목표는 self-host가 non-empty
directive row를 생산하고, canonicalization이 새 identity epoch에서 declaration/
topology ID를 함께 remap한 뒤, admitted row로 하나의 ID-keyed target-neutral graph
plan을 만드는 것이었다. 일반 DRV-2 C/LLVM production path가
`zone_layer_projection_runtime`의 exact 3-node/2-edge trace를 소비해야 한다.
canonical topology ID 하나만 과거 raw native ID로 되돌린 row와 `player` 이름에
canonical `enemy` ID를 붙인 row가 첫 negative다. 이 fixture의 native runtime
전체를 대체하려면 graph 외에도 `apply poison to player`, zone state count, hidden
layer layout와 sync operation fact가 필요하다. 그때도 `MIR if present, otherwise
AST` fallback은 두지 않는다.

2026-07-28 producer checkpoint에서 `zone_layer_projection_runtime`은 production
DRV-2의 self source -> typed AST/DIR -> MIR 경로로 non-empty topology를 직접
생산한다. `domain_graph_id=14937235025281185444`와 exact 4 rows
`[refresh, publish, apply-effect, link-relation]`가 declaration의 `(owner, field
name, source_syntax_id, field_kind)`에 같은 identity epoch 안에서 join된다. Apply는
maintain과 별도 typed identity이며 `poison` effect slot과 `player` subject slot을
exact ID로 결속한다. 이 producer 경로와 exact runtime-assignment producer가
`SUBSTITUTING`이다. Graph plan은 여전히 topology receipt이고, 별도 runtime plan이
role/member/path를 join해 self general C의 마지막 소비자를 구동한다.

Native의 `apply stateAlias`는 semantic owner가 state declaration을 정확히
effect/target slot로 해석하고 AST carrier에 결속한 뒤 같은 `apply-effect` row를
생산한다. DIR은 unresolved apply를 조용히 건너뛰지 않는다. 현재 production
self source parser는 state declaration/apply alias를 아직 typed artifact로 보존하지
않으므로 직접형만 admit하고 alias form은 fail closed한다. Parser가 alias를 이름으로
다시 찾는 임시 fallback을 넣지 말고, state declaration identity와 resolved slot
identity를 함께 운반하는 별도 self parser/DIR closure로 닫아야 한다.

현재 runtime owner의 닫힌 범위와 남은 blocker는 다음처럼 기록한다.

- `apply poison to player`는 exact `effect-bearer` role로 `Poisoned.bearer`를
  `BattleZone.player`에 결속한다. `by participant`를 destination으로 쓰거나 첫
  bindable field를 고르는 경로는 C/LLVM/self fixture 경로에서 삭제됐다.
- Relation은 exact `relation-source`/`relation-target` role로
  `TrustedLink.source <- player`, `TrustedLink.target <- enemy`를 만든다. 0/1 ordinal
  선택은 같은 경로에서 삭제됐다.
- `view.hp <- bearer.hp`와 `packet.name <- target.name`은 field ID/name/type 및 source
  path segment ID/name/type를 MIR wire까지 운반한다. Self implicit same-name과
  explicit `map { ... }`가 같은 exact assignment family로 수렴한다. Explicit
  `life: Long <- hp: Int`는 self semantic assignability owner를 통과하므로 MIR이나
  backend가 target/source 문자열 equality를 새 정책으로 소유하지 않는다.
- Self runtime plan은 admission에서 한 번 검증되고 eager method-entry bind/sync를
  materialize한다. 이 실행 모델은 현재 fixture의 실제 결과를 대체하지만
  transition cause, ready/dirty/epoch, detach/unlink, pass-limit scheduler와 crash/
  re-entry lifecycle까지 소유하지 않는다.
- callable receiver carriage는 이제 self MIR에서 `none | value |
  mutable-identity`로 필수 운반되고, machine admission이 exact callable 및
  declaration identity와 함께 검증한다. General self C의 zone method는
  `BattleZone *self`, named stable binding 호출은 `&(battle)`로 방출된다.
  Missing/unknown/value-zone/foreign-owner/temporary-receiver 변조는 output 전에
  거부된다. 다만 일반 parameter carriage와 native C/LLVM receiver 소비자는
  아직 더 넓은 `uses_pointer_self` compatibility policy를 공유한다.
- Domain constructor caller는 declaration source order의 subject/binding input만
  넘긴다. object/tobject projection destination과 layer/shared storage는 topology/
  runtime owner가 materialize한다. 현재 self
  exact prologue가 hidden layer를 bind/sync하므로 generic zero-fill만으로 성공을
  주장하지 않는다. Native MIR/JSON graft도 금지한다.

Self source -> MIR -> C의 `7`/`dst` 요구는 implicit/explicit map 모두 GREEN이며 이
좁은 runtime assignment/eager materialization 경로는 `SUBSTITUTING`이다. 전체
family는 self semantic fact가 아직 MIR producer boundary에서 만들어지는 구조,
declaration identity, lifecycle/epoch, native shared-plan gap 때문에 `BRIDGE`다.

### 4.3 Artifact action의 commit 조건

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
- production MIR writer, direct-MIR action, source-to-MIR action, bootstrap 출력,
  rung-1 CLI 출력은
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
artifact commit을 실제로 소유한다. Terminal protocol은
`DriverRung2Executed(receipt)`, `DriverRung2Rejected(rejection)`,
`DriverRung2ArtifactRejected(failure)` 세 variant다. `ok + stage` 중복 tag와
읽히지 않는 intermediate stage local은 삭제됐다. Wrong identity/target은 typed
rejection으로, transaction failure는 원래 failure payload로 구별하지만 하위
direct-MIR owner의 malformed-input 실패는 여전히 `Die` fatal boundary다.
`DriverSourceMirExecution.EmitSourceMir`는 별도 typed outcome으로 pressure,
subject identity, topology identity rejection을 구분하고 기존 source-to-MIR
payload owner를 정확히 한 번 소비한 뒤 같은 transaction owner를 commit한다.
Main의 직접 payload compile/file-helper/commit 우회는 없다.
backend artifact 생성 자체는 기존 typed owner에 남고, action-contract wire도
끝까지 운반된다. Contract vocabulary SoT는 shared registry로 닫혔지만 실행 대체는
아직 열려 있다.
따라서 이 action은 atomic commit까지 `REACHABLE`이지만 아직
C-owned compiler path를 대체한 `SUBSTITUTING`은 아니다.

## 5. 권장 조합은 필요한 경계만 쓴다

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

tobject ArtifactReceipt {
    schema: String;
    fingerprint: String;
}

subject CompilerExecution {
    func TargetKnown(self, request: TargetRequest) -> Bool {
        return StringLength(request.projection) > 0;
    }

    action Emit(self, request: TargetRequest) -> Result<ArtifactReceipt>
        with caps io_write {
        // admit -> emit -> verify -> write; 실패는 Result로 구별한다.
    }
}
```

이 최소 예에서 `struct`는 요청 값, `class`는 복사 가능한 계산 도구,
`tobject`는 전달 receipt, `subject/action`은 identity-bearing 전이를 소유한다.
실제 zone/authority가 없으므로 `within`이나 `authorized by`를 장식으로 추가하지
않는다. production caller는 `Emit`의 `Result`를 match하고 성공 receipt payload나
실패 payload를 다음 결정에 사용해야 한다.

`object`는 실제 source-bound read projection과 downstream read가 생길 때,
`vessel`은 subject가 소유해야 할 장기 상태와 pointer-self 갱신이 생길 때만
별도로 추가한다. 모든 구성체를 한 예제나 compiler stage에 한 번씩 배치하는
것은 조합이 아니라 ceremonial inventory다.

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
   write는 실제로 그 검사를 우회한다. `object`/`tobject`의 bare `hp = hp + 1`
   probe는 컴파일되고 C 실행에서 value-self 복사본만 바꿔 caller의 7은 그대로
   남았지만, `self.hp = ...`는 semantic에서 거부됐다. nested
   `holder.view.hp`와 indexed member도 shallow identifier 검사 밖이다. query/format
   func의 무효과 규칙을 target-shape와 무관한 한 field policy owner로 묶어야 한다.
3. subject/class/vessel의 bare field write와 `self.field` write가 동일한
   field-mutability fact를 소비하지 않는다. 실제 probe에서 `class`와 `subject`의
   `let count`도 bare assignment가 통과했고, class caller는 7로 남은 반면 subject
   원본은 8로 바뀌었다. `MIRDeclField`에는 현재 mutability fact도 없다. 새 코드는
   hosted field를 항상 `self.field`로 쓰고 변경 상태를 `let mut`로 명시하며,
   bare 표기를 같은 의미로 가정하지 않는다.
4. `class` hosted func는 value-self이므로 내부 mutation 결과가 호출자에 남지
   않는다. mutator처럼 보이는 이름은 피하고 새 class 값을 반환하는 형태를
   쓴다.
5. self MIR -> general C의 callable receiver carriage는 별도 fact로 분리됐지만,
   native C/LLVM의 receiver와 일반 parameter carriage는 아직 한
   `uses_pointer_self` predicate를 공유한다. `func Touch(state: HealthState) {
   state.Advance(); }` probe는 initial 7을 C와 LLVM 모두 8로 바꿨고 `ref vessel`도
   C에서 const 경계보다 pointer-self 분기가 먼저다. 따라서 callable closure를
   일반 parameter ABI closure로 오인하지 않으며, parameter-carriage owner와
   C/LLVM 소비자 migration 전에는 vessel을 free-function default/ref 파라미터로
   넘기지 않고 subject owner 안의 stable storage에서만 호출한다.
6. production self-host import graph에는 `PgyCompilerWorld`와 20개 zone을 포함한
   domain 표면이 들어와 있다. 실제 call chain이 통과하는 Pergyra-native 경계는
   world 1개와 ordered direct/source-MIR zone 2개, subject/action 2개다.
   import-reachable 선언 수나 fixture 수를 self-host substitution으로 세지 않는다.
7. `MakeSubject().Action()` 같은 temporary subject receiver는 backend마다 다른
   lifetime을 만들 수 있다. Self general C는 이제 node-kind를 재추론하지 않고
   semantic place fact를 소비해 임시 member receiver를 C output 전에 거부한다.
   Native C/LLVM도 같은 stable-binding owner를 직접 소비하게 만들기 전까지
   전체 backend closure로 세지 않는다.
8. `MIRDeclMethod`와 native/self `pgy.mir.v1`은 action identity, `requires`,
   `within`, `causes`, `authorized by`, declared caps/effects를 한 method contract로
   운반한다. 그러나 zone authority ability와 호출별 participant binding은
   declaration contract가 아니라 별도 call/runtime fact다. backend가 AST를
   재조회하거나 이름에서 binding을 추정하지 않도록 이 owner 구분을 유지해야 한다.
9. action 후 zone frontier pass-limit graph는 이제 DIR owner의 MIR carrier를
   C와 LLVM이 함께 소비한다. 그러나 실제 projection/effect/relation operation
   emission과 action transition은 여전히 별도 backend/AST 경로가 남아 있다.
   동일한 검증 plan으로 계속 이동시키며 출력 parity만으로 중복 정책 소유를
   정당화하지 않는다.
10. semantic helper `type_is_class_object_type()`은 이름과 달리 현재 subject만
   판정한다. 새 consumer를 붙이지 말고 subject identity 이름으로 바꿔
   오분류 가능성을 제거해야 한다.
11. hosted method의 source declaration order는 사용자 ABI가 아니다. C는
    MIR-owned value layout schedule 뒤 domain type을 완성한 후 nominal method
    body를 방출하고, LLVM은 nominal/domain layout을 모두 등록한 뒤 method
    signature와 body를 방출한다. 이 순서를 source 재배치나 opaque type 추정으로
    우회하지 말고 declaration inventory와 later-declared value fixture로 고정한다.
12. self-host parser의 `decl_nominal_owner.pgy`는 non-subject action을 거부한다.
    native의 struct hosted-`func` 금지는 아직 같은 self-host semantic negative로
    닫히지 않았다. grammar fixture 통과를 이 nominal semantic parity의 대체로
    해석하지 않는다.
13. self-host parser는 native와 달리 non-subject nominal body에서도 `vessel`
    field prefix를 소비할 수 있고 그 field-role을 AST/MIR JSON에 보존하지 않는다.
    `class X { vessel y: Y; }` native/self mismatch를 거부하고
    `field_role=subject_owned_vessel`을 wire까지 운반해야 한다.
14. callable contract vocabulary는 shared registry와 projection으로 닫혔다. 현재
    domain declaration의 `field_kind` spelling도 compiler-owned registry와 생성
    self-host projection으로 단일화됐다. DIR zone-frontier slice는 stable
    directive/slot identity를 운반한다. MIR JSON relation/topology carriage와
    self-host typed admission은 declaration field `(owner, name, ID, kind)` exact
    join까지 `REACHABLE`로 닫혔다. Non-empty canonical identity remap, self-host
    producer emission과 one-shot graph plan도 이후 닫혔다. Exact participant role과
    implicit/explicit projection member/path assignment은 native/self MIR을 지나 self
    runtime plan과 C/LLVM consumers에 도달했고 `7`/`dst`가 green이다. Native/self raw
    ID 숫자 수렴은 계약이 아니다. Declaration-level source ID carriage, pool
    capacity, vessel/binding slot, dirty/epoch/lifecycle shared plan은 열려 있다.
15. native/self `pgy.mir.v1` declaration JSON은 action identity와 전체 contract를
    운반하고 `mir_lower`가 이를 fail closed로 소비한다. 이것은 declaration
    carriage 증거이며 호출별 authority binding 또는 runtime identity/token 승인
    증거는 아니다.
16. compiler artifact transaction은 shared C/LLVM runtime core, self-host typed
    receipt, generated runtime include, old-final 보존 fault gate까지 닫혔다.
    일반 raw file handle의 MIR/AIR mode fact와 checked close 표면은 별도 호환성
    부채다. Source-to-MIR production action reachability와 직접 file-helper 삭제는
    닫혔지만 이는 Pergyra 내부 orchestration 교체다. Source-to-C/general MIR-to-C
    직접 경로와 compiler-root purpose bundle은 계속 열려 있다.
17. `Rejected` enum의 존재가 모든 실패의 반환을 뜻하지 않는다. wrong
    identity/target처럼 action이 직접 반환하는 실패와, 하위 `Die`/`Exit`가
    중단시키는 fatal 실패를 gate와 문서에서 분리한다.

18. 일반 zone routine prologue는 복수 authority 중 첫 row를 C/LLVM 모두 암묵
    선택한다. 호출별 binding fact가 없으면 `authority_count != 1`을 fail closed하고,
    source order를 권한 선택 정책으로 만들지 않는다.
19. strongest dynamic action gate인
    `driver_execution_action_abi_parity.sh`는 아직 Makefile/aggregate gate graph에
    연결되지 않았다. 수동 PASS를 표준 회귀 보호로 과장하지 않는다.
20. generic subject/vessel class의 LLVM on-demand method specialization은
    pointer-self flag를 self parameter type에 적용하지 않는 C/LLVM ABI gap이 있다.
21. `uses_pointer_self` MIR validator는 subject/vessel/domain이 `false`인 한 방향만
    거부한다. class/struct/object/tobject에 잘못 `true`가 들어온 반대 drift도
    fail closed하고 양방향 negative를 둬야 한다.
22. default subject argument의 non-identifier temporary는 semantic에서 통과한 뒤
    C/LLVM backend의 addressable-storage 경계에서 늦게 실패한다. receiver뿐 아니라
    identity-reference parameter도 semantic stable-binding owner가 먼저 거부해야 한다.
23. C의 `ToTObject` emitter는 semantic을 우회한 입력에서 exact `tobject`가 아니라
    struct/vessel/object/tobject shape를 함께 허용한다. `ToObject`/`ToTObject`의
    exact target kind를 MIR/backend에서도 검증하는 direct-MIR negative가 필요하다.
24. `ToObject`/`ToTObject`는 현재 둘 다 value projection ABI이고 tobject 전용
    channel/API/IPC transport 정책은 아직 없다. API/IPC/persistence는 canonical
    사용 의도이지 구현된 별도 wire/transport 보장으로 과장하지 않는다.
25. Native AST printer는 domain vessel field를 `VesselSlot:`으로 출력할 수 있지만
    self-host typed-AST inventory는 아직 이 label을 field row로 분류하지 않는다.
    현재 MIR field vocabulary에도 vessel 전용 wire kind가 없어 object slot로 접힐
    수 있다. `subject-owned vessel`은 storage spelling이 아니라 별도 field-role
    fact여야 하며, native/self negative와 carriage fixture가 생기기 전까지 이
    경계를 구현 완료로 세지 않는다.
26. Role-erased method ABI는 concrete target이 mutable identity이면 `_raw_self`를
    값으로 복사하지 않고 `T *self`로 유지해야 하며 direct call도 안정된
    receiver의 주소를 전달해야 한다. 이 local ABI owner는 별도 gate로 고정하지만,
    production direct role call은 native semantic의 `Player.TakeDamage` 해석 부재와
    role method canonical source ID `13` 대 `6` 불일치 때문에 아직 unreachable이다.
    Role body의 `return self.health`도 self semantic statement-type fact가
    unresolved라 C emit 전에 실패한다. Synthetic call이나 local emitter gate를
    `SUBSTITUTING`으로 세지 말고 role callable identity epoch, production
    call-target resolution, role-body field type fact를 먼저 닫아야 한다.
27. Legacy intent의 `guard`/`expect`/`post`와 ordered `compensate`는 self
    DIR/MIR/general C까지 무손실로 실행된다. v2 transition carrier는 stable
    expression/graph occurrence identity로 compensation을 결속하며, executable gate는
    동일한 compensate expression의 여러 occurrence도 순서를 보존해 실행한다.
    Expression text 전역 equality를 join authority로 재도입하지 않는다.
28. 합법적인 empty domain topology가 현재 self machine admission에서 거부된다.
    Intent runtime fixture의 최소 `object`/`tobject` refresh/publish scaffold는 이
    admission 제약을 통과시키는 실제 domain plan이지, tobject가 rollback owner라는
    증거가 아니다. Empty topology admission을 별도 falsifier로 닫아야 한다.
29. Intent의 explicit `success`/`failure`, `concurrent`/`retry` surface carriage는
    이번 legacy predicate rung의 증거가 아니다. Parser가 보존한 clause가 DIR/MIR에서
    default로 접히거나 사라지지 않도록 각 의미 owner와 negative를 따로 닫아야 한다.

다음 semantic closure의 첫 falsifying fixtures는 `tobject` hosted method,
`object`/`tobject` bare·nested-field mutation, class/subject의 bare/`self.` mutability
불일치, vessel default/ref parameter mutation, temporary pointer-self receiver,
temporary subject parameter, exact projection target, non-subject vessel field다. 이 작업은
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
  -> intent when a real-world purpose binds the required cross-axis facts
```

구현 규칙은 다음과 같다.

이 흐름은 구현 경계의 소비 순서를 보인 것이며 intent를 마지막에 발명한다는
뜻이 아니다. 설계에서는 목적을 먼저 정하고 필요한 fact와 구성체를 유도한다.
Action 수가 하나인지 여럿인지는 intent의 필요조건도 충분조건도 아니다.

1. `action` 선언이 `requires`, `within`, `authorized by`, `causes`, caps/effects와
   transition 계약의 source surface를 소유한다. intent step은 유일하게 matching한
   action에서 `where`, `requires`, `causes`, `authorized by`를 상속한다. participant
   alias mapping이 모호하면 상속하지 않으며, 같은 clause를 명시적으로 반복하는
   현재 동작은 semantic error가 아니라 redundancy warning이다.
2. **현재 `DriverRung2DirectMirZone`과 `DriverSourceMirZone`에 한해** zone은
   subject slot과 authority/lifetime 경계만 소유한다. 일반 zone은 projection의 실제 owner일 때
   object/tobject slot, layer, state, refresh/publish를 정당하게 소유할 수 있다.
   두 execution zone은 target/source/MIR/certificate/projection/artifact payload를
   다시 복사하지 않는다.
3. 현재 direct-MIR와 source-to-MIR world method는 각 zone 내부 action으로 한 번
   위임하는 composition boundary다. target/pressure 판정, backend/payload 호출,
   artifact commit을 world나
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
   실제 실행되는 `direct_mir`, `source_mir` 둘이다. 선언만 된 target zone은 production
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

driver_bootstrap_main.Main
  -> EmitSourceMirThroughPgyCompilerWorld
  -> PgyCompilerWorld.EmitSourceMir
  -> PgyCompilerWorld.source_mir
  -> DriverSourceMirZone.execution
  -> DriverSourceMirExecution.EmitSourceMir
  -> existing typed source/MIR owners + artifact transaction
```

이 두 연결은 기존 C-owned compiler 의미를 대체하지 않으므로 증거 등급은 계속
`REACHABLE`이며 `SUBSTITUTING`이 아니다. 하지만 선언만 존재하던 world와 실제
production action을 하나의 import/call graph로 합친 실행 경계다. 별도 world
재도입과 migrated direct/source-MIR mode의 `Main` 직행은 negative gate가 금지한다.

### 9.1 현재 action authority ABI 범위

현재 action authority는 다음 세 층으로 나눠 판정한다.

| 층 | 현재 owner와 증거 | 아직 닫히지 않은 것 |
| --- | --- | --- |
| declaration contract | `MIRDeclMethod`와 native/self MIR wire의 action/`requires`/`within`/`causes`/`authorized_by_names`/caps/effects, `MIRDeclZoneAuthority`의 subject slot/required ability; shared caps/effects vocabulary는 CLOSED | declaration fact와 별도인 call binding |
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

## 10. Typed action outcome을 소비하는 intent 경계

단일 step의 exact action 결과 binding과 legacy `guard`/`expect`/`post` predicate,
ordered `compensate` 실행은 2026-07-29에 닫혔다. Typed variant branch,
success-only completion과 failure payload-driven predecessor compensation도 v2
execution plan과 executable owner gate까지 구현돼 있다. 두 action은 predecessor
rollback을 드러내는 fixture shape일 뿐 intent의 정의가 아니다. Production compiler
`intent`의 최종 등급 판정은 이 구현 사실과 별개다.

### 10.1 완료된 binding 및 legacy predicate/compensation rung

- Source spelling은 `on outcome: worker.Run(...);`이다. `on:`은 결과를 버리는
  legacy form으로 남는다. 새 예약어를 추가하지 않는다.
- semantic owner가 receiver의 exact `subject.action` signature를 한 번 resolve해
  return type을 binding type으로 고정한다. `Void`, unknown action, 두 개 이상의
  `on`, participant/value 이름 충돌, rollback lifetime 전체의 outcome 이름 중복은
  거부한다. 언어 scope상 binding은 같은 step의 `expect`/`post`/`compensate`에서만
  보이며 `pre`와 뒤 step에서는 보이지 않는다.
- DIR은 binding name/type과 action stable syntax identity를 운반한다. MIR의
  `IntentOutcomeBinding`은 `result=slot_anchor=binding`, `arg0=action syntax id`,
  `arg1=step`, `abi_type_name=exact return type`,
  `source_type=AST_INTENT_STEP`을 소유하고, `IntentEval(on)`은 같은 result/type을
  결속한다. 타입을 `runtime_call_abi`나 `uses` 문자열에 숨기지 않는다.
- Native C와 LLVM은 action을 정확히 한 번 평가해 immutable typed local에 저장하고
  동적 `expect`가 그 값을 소비한다. Self source -> MIR -> admitted general C와 direct
  source C는 byte-equal이며 같은 local을 소비한다. `IntentCheck` call expression은
  일반 statement-call allowlist보다 semantic carrier 정책이 먼저 소유한다.
- `intent_typed_outcome_execution_owner.sh`는 enum의 success/failure variant가 각각
  detached `tobject` payload를 갖는 action을 두 번 호출한다. Self C, native C,
  native LLVM이 모두 `accepted=true/calls=1/rejected=false/calls=2`를 출력한다.
  호출 횟수와 Bool 결과가 action exact-once 및 저장된 outcome 소비를 함께
  falsify한다.
- 같은 gate는 missing binding, result/type/action identity drift, duplicate binding,
  eval result drift를 각각 valid MIR에 주입하고 partial C 전에 거부한다.

이 rung은 variant 이름으로 성공을 추정하지 않는다. `IntentRunAccepted`라는 typed
helper가 enum payload를 match해 Bool을 만들고 intent는 그 결과를 `expect`한다.
`tobject`는 receipt/failure payload일 뿐 step identity, 완료, predecessor, rollback,
authority 또는 projection freshness를 소유하지 않는다.

Self executable 경계는 이제 step별 `guard`/`expect`/`post` singleton과 ordered
`compensate` expression graph를 DIR→MIR→마지막 C consumer까지 운반한다. 실행 순서는
action completion 뒤 `guard -> expect -> post`이며, 실패하면 완료된 step을 역순으로,
각 step의 compensate row도 역순으로 실행한다. 첫 step 실패는 미래 step을 실행하지
않고 성공은 compensation 0회를 보장한다.

`intent_guard_post_compensation_execution_owner.sh`는 direct/admitted self C byte
parity와 self/native C/native LLVM runtime parity를 고정한다.
`intent_phase_carrier_negative_owner.sh`는 phase/step/slot/graph 및 result/type
cross-wire를 partial C 전에 거부한다. 이 legacy predicate 의미는 typed action failure
분기와 다르다. 후자는 현재 step을 completed로 만들지 않고 completed predecessor만
보상해야 하므로 10.2의 별도 transition owner에서 닫혔다. `tobject`는 여전히
receipt/failure payload일 뿐 completion/rollback owner가 아니다.

### 10.2 구현된 v2 typed-transition objective card

- **Objective:** `subject.action` 결과를 intent step에서 typed binding하고, source가
  선언한 enum variant branch에서 success/failure payload를 소비한다. Predecessor가
  있는 후속 action이 실패하면 완료된 predecessor의 compensation만 실행한다.
- **Priority:** exact step/outcome identity, payload type/definition, branch successor,
  success-only completion evidence, predecessor edge, compensation target, runtime
  payload consumption, direct bypass 삭제 순서다.
- **Fact owner:** semantic은 action return type과 variant/payload binding을 소유하고,
  DIR은 step stable identity와 dependency edge를 소유한다. MIR은 그 identity를
  outcome binding/case, success completion, compensation expression graph에 결속한다.
  C/LLVM intent emitter는 마지막 소비자다. `tobject` receipt/failure는 payload이며
  step topology, predecessor, completion 또는 authority owner가 아니다.
- **Forbidden fallback:** action outcome을 `Bool`/literal `expect`로 축소, variant
  spelling으로 success/failure 추정, diagnostic 문자열에서 payload 복원, subject나
  global field를 outcome side channel로 사용, payload type 재추론, predecessor를
  `step_index - 1` 또는 source order에서 재생성, action 호출 직후 무조건 completed,
  모든 이전 step을 역순 보상, compensate AST/source 재스캔, native MIR graft,
  missing carrier 성공, 기존 `Main -> action` 직행 orchestration 병존이다.
- **Executable gate:**
  `tests/self_hosted/parity/intent_typed_outcome_compensation_owner.sh`는
  `intent_typed_outcome_compensation.pgy`를 MIR로 내리고
  `pgy.selfhost.mir-intent-execution-plan.v2`를 admission한 뒤 general self C를
  실행한다. success, first-step failure, predecessor-only compensation과
  multiple/duplicate/zero compensation shape를 결과와 호출 횟수로 관측한다.
- **Falsifying fixture:** action A가 typed success payload를 반환하고 그 payload가 action
  B의 입력이나 후속 결정을 바꾼다. B의 typed failure payload는 exact failure
  diagnostic 또는 관측 상태를 바꾼다. B는 completed가 아니므로 B compensation은
  0회, predecessor A compensation은 정확히 1회이며 최종 상태가 복구돼야 한다.
  여기서 두 action은 predecessor 관계를 falsify하기 위한 테스트 모양이지 intent
  사용 조건이 아니다.

### 10.3 compensation 최소 negative ratchet

양성 fixture가 만드는 유효 MIR에서 다음 mutation만 독립적으로 적용한다. 단순히
schema/hash를 깨뜨리지 않도록 다른 유효 step/type/definition/block identity를
cross-wire하고, 모두 C artifact 첫 줄 전에 거부한다.

1. outcome binding의 result definition 또는 declared outcome type을 다른 유효
   identity로 바꾸고 admission이 해당 결속을 거부하는지 확인한다.
2. success/failure variant 또는 successor block identity를 서로 바꾼다:
   branch와 successor의 exact identity 불일치로 거부해야 한다.
3. B의 predecessor를 B 자신이나 future step으로 바꾸거나 predecessor name/identity
   쌍을 교차시킨다. Row order 추정 없이 predecessor identity 불일치로 거부해야 한다.
4. A의 compensation row는 남기고 success-only completion evidence를 제거한다:
   compensation 존재를 completion으로 추정하지 않고 거부해야 한다.

이 ratchet은 v2 admission에서 variant/payload, predecessor, completion,
compensation identity의 cross-wire를 거부해야 한다는 의미 계약이다. 세부 문구를
별도 stable diagnostic owner에 등록하기 전에는 우연히 발생한 내부 diagnostic
문자열을 공개 exact 계약이나 이 rung의 완료 증거로 세지 않는다.

### 10.4 등급 조건

- 별도 test subject의 source -> DIR -> MIR -> admission -> general C 실행과 native
  C/LLVM parity가 성공하고 같은 bounded feature의 old path가 없으면 그 **입력 언어
  기능 slice**만 `SUBSTITUTING`으로 올릴 수 있다.
- 실제 compiler production entrypoint가 독립적인 compiler 목적과 bundle-level
  의무를 소유한 intent를 호출하고 그 intent가 production action transition을
  실행하면 compiler `intent`는 `REACHABLE`이다. Action 수는 판정 기준이 아니다.
  Readiness facade 또는 순수 계산 `func`를 action으로 이름만 바꾼 경로는 이 조건을
  만족하지 않는다.
- 기존 `Main -> action(s)` 직행 우회가 삭제되고 success/failure payload와 실제
  compensation을 production caller가 관측하며 위 negative가 fail closed한 뒤에만
  compiler `intent`를 `SUBSTITUTING`으로 올릴 수 있다.
