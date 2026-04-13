# Pergyra TODO (배포 준비)

마지막 업데이트: 2026-04-14

## 현재 상태 냉정 평가 (2026-04-12 재정렬)

### 종합 판단: Late-Stage Alpha

- 베타 진행률 추정: 약 `83%`
- 현재 표현: `late-stage alpha / beta-closure sprint`

### 최근 closure 진행 (2026-04-14)

- declaration-side MIR-only intent inventory를 더 밀었다
  - MIR가 `IntentParticipant(alias,type)` metadata를 직접 운반
  - C/LLVM intent declaration emission이 participant alias/type를 AST 재해석 없이 MIR metadata로 우선 소비
- step-level MIR-only validation을 AST field 존재 검사에서 metadata 존재 검사로 옮겼다
  - `IntentCheck`
  - `IntentEval`
  - `IntentZoneWhere/IntentZoneAlias/IntentZoneFrom`
  - `IntentWho/IntentDispatch`
  - `compensate` 존재 판정
- intent emission cleanup/rollback 경로의 metadata gate를 C/LLVM 둘 다 정렬했다
- 관련 회귀:
  - `test-mir` green
  - `test-transpile` green

즉, intent declaration/step emission은 아직 완전 MIR-only 선언이 끝난 것은 아니지만,
`participant/step contract inventory`를 AST presence에 기대던 가장 거친 fallback는 한 단계 더 제거됐다.

실행 가능한 연구용 컴파일러 단계는 넘겼지만, 아직 베타라고 부를 수는 없다.

판정 기준:
- 베타 원칙인 `부분 구현 상태를 남기지 않는다`를 아직 충족하지 못함
- 키워드 부족이 아니라 `구현 depth 불균형`이 문제임
- parser가 받는 surface 중 일부가 semantic/C/LLVM/runtime/test/documentation까지 완전히 닫히지 않음

### 이미 닫힌 축과 더 이상 베타 차단이 아닌 것

- `public/private/export` module boundary
  - top-level nominal/domain/callable visibility 정렬 완료
  - private `func/intent/event` cross-module call 차단 완료
  - private `zone/effect` action-contract leakage 차단 완료
- nominal token split
  - `subject/class/struct/object/tobject`는 lexer token 레벨에서 이미 분리됨
- ability field surface
  - legacy `require` alias 제거, `fields` canonical surface 고정
- generic ability baseline
  - `ability<T>`, `requires Ability<T>`, `impl ability Ability<T>`, zone authority generic ref, mismatch diagnostics baseline 존재
- 양자 surface
  - 베타 대상에서 제외
  - `v2 / experimental`로만 추적

### 현재 베타를 막는 실제 B0 갭

#### 1. Intent / Zone / World closure

현재:
- intent orchestration, inherited/derived contract, rollback/cleanup carrier, zone/world declaration과 기본 lowering은 존재
- zone/world projection/layer/state query도 존재
- intent runtime observability baseline도 존재
  - `IntentLast*`
  - `IntentHistoryStep*`
  - `IntentActive*`
  - `IntentRecent*`
  - active/recent handle + active-step field query builtin의 semantic/transpiler/runtime/LLVM baseline 연결 완료
  - runtime 내부 recent ring + active registry + typed step history storage 연결 완료
  - ABI regression: `IntentRecent*` trace/failure baseline, failed-intent provenance, world zone query, relation/effect zone state parity 고정
  - backend parity: embedded world -> zone projection visibility regression 고정

남은 것:
- embedding ownership / handoff policy를 surface trust 수준까지 명확히 고정
- richer multi-instance timeline query와 failure provenance 정교화
- cross-layer propagation policy의 더 깊은 closure
- C/LLVM parity를 declaration/runtime/diagnostic까지 같은 품질로 정렬

#### 2. relation / effect / projection closure

현재:
- declaration, lifecycle shorthand, `refresh/publish/bind`, layer/state query, overlay sync baseline 존재
- effect join/meet/conflict API와 basic closure 존재
- projection contract diagnostics는 target/source/mode/fix를 포함하는 structured error 쪽으로 보강됨
- backend parity:
  - embedded world -> zone projection visibility regression 고정
  - relation/effect layer + state propagation parity regression 고정

남은 것:
- authority/resource와 effect partial order의 더 완전한 통합
- projection propagation policy 심화
- runtime contract와 deeper propagation failure provenance를 더 설명 가능하게 정리
- C/LLVM parity에서 helper-heavy edge path 감소

#### 3. generic contract closure

현재:
- generic ability declaration/reference baseline 존재
- action / intent step / zone authority / party role slot generic mismatch diagnostics baseline 존재
- hidden/default-export generic ability visibility는 action/role impl뿐 아니라 zone authority/party role slot consumer path까지 회귀로 고정
- `ability<T> where ...` bound는 `requires` / `impl ability` / party role slot ref에서 다시 검증됨
- default type argument는 semantic + transpiler + backend compare까지 baseline closure 완료
  - user-defined `class/ability<T = ...>`가 omitted arg 경로에서도 effective specialization으로 정렬됨
  - non-deduced trailing generic parameter default도 function call `where` validation 경로에서 회귀로 고정
  - cross-module omitted default generic ability consumer(`party role slot` / `zone authority`)도 회귀로 고정
- multi-bound `where T: A + B` baseline은 현재 동작함
- hidden/default-export와 generic ability ref 규칙 정렬 완료

남은 것:
- broader type-family generalization을 beta 범위 밖으로 명시
- richer generic constraint validation의 beta contract 범위를 문서/board에 일치시켜 고정
- import/use surface와 diagnostics/tooling 표현을 module contract 기준으로 더 일관되게 정리

#### 4. own/ref closure

현재:
- anchored subset은 닫혀 있음
  - `ref Slot<subject-host>`
  - `own SecureSlot<subject-host>`
- 관련 진단/예제/문서는 현재 구현 기준으로 정렬됨

판정:
- 베타 기준 own/ref는 anchored subset만 stable surface로 본다
- 일반 ownership system은 이번 베타 범위에서 제외한다
- 따라서 own/ref의 B0 클로저는 `확장`이 아니라 `surface trust 고정`으로 본다

### 레이어별 현재 진실

#### 시맨틱

- 강한 부분:
  - nominal family
  - subject/action
  - async/channel/select
  - generic ability baseline
  - visibility/export boundary
- 아직 얕은 부분:
  - richer generic constraint validation
  - general own/ref
  - event closure의 잔여 negative path
  - collection semantic depth

#### 코드 생성

- C backend:
  - 코어 surface는 가장 성숙
  - method owner metadata가 HIR->MIR로 내려와 declaration-side zone/relation/effect/world context 복원 시 이름 추정보다 MIR metadata를 우선 사용
- LLVM backend:
  - MIR-led / HIR-assisted hybrid
  - ordinary routine은 MIR 중심이지만 domain declaration과 일부 bootstrap/helper path에 HIR/AST 의존 잔존
  - pure MIR-only라고 부르기에는 아직 이름이 과함

#### 런타임

- 강한 부분:
  - slot / secure baseline
  - async/channel basic runtime
  - basic intent execution/rollback
  - intent observability baseline (`last` / `history` / `active` / `recent`)
- 아직 얕은 부분:
  - richer multi-instance timeline / failure provenance
  - channel backpressure protocol
  - party edge-path completeness
  - richer zone/world runtime policy

### 컬렉션 / 표면 신뢰

- `Map<K, V>`는 현재 `String | Int` key만 stable surface
- 이것은 버그가 아니라 현재 contract
- 나머지 key type을 지원하지 않으면 surface trust 문서에 명시적으로 남겨야 함

### 툴링

- LSP / formatter는 베타 차단 핵심이 아님
- debugger / package manager / WASM도 베타 차단 핵심이 아님
- 이들은 B0 closure 이후에 다루는 것이 맞음

### 베타 직전 정리 원칙

1. 새 키워드/새 축을 더 추가하지 않는다
2. 남은 미완성 surface를 `완성`하거나 `experimental`로 내린다
3. `양자`, `WASM`, `패키지 매니저`, `고급 디버거`는 베타 대상에서 제외한다
4. B0 4개를 닫기 전에는 베타라고 부르지 않는다

---

## 완료 (P0 — Pain Point 수정, 2026-04-12)

- [x] **P0-1: Array for-in `.count` → `.length`** — `transpiler.c`에서 Array는 `.length`, List는 `.count` 사용
- [x] **P0-2: `StringSplit`/`StringJoin` 런타임 구현** — `pgy_runtime.h`에 실제 구현 추가, 시맨틱/C 백엔드 일치
- [x] **P0-3: `None` 심볼 정의** — `type_checker.c`에서 AST_IDENTIFIER 처리, `type_system.c`에서 `Option<unknown>` → `Option<T>` 할당 허용, 코드젠에서 `expected_type` 기반 타입 해결
- [x] **P0-6: defer 변수 스코프 버그 수정** — `type_checker_flow.c`에서 defer body 처리 전/후 slot 상태 저장/복원
- [x] **P1-7: struct/subject Slot 매크로 warning 억제** — `transpiler.c`에서 `#pragma GCC diagnostic push/pop`으로 `-Wunused-function` 억제
- [x] **P1-emit_call 갭 메우기** — `BUILTIN_BOX_ARRAY`, `BUILTIN_PARALLEL` 케이스 추가
- [x] **P0-4: enum match OR 패턴 수정** — `type_checker_flow.c`에서 named variant OR 패턴 허용 + coverage 체크 수정
- [x] **P2-13: match 기반 함수 default return 자동 생성** — `transpiler_emitters_base_b.inc`에서 non-void 함수 끝 fallback return 추가
- [x] **Pain Point 보고서** — `docs/68_pain_point_report.md`에 수정 내역 기록

## 완료 (최근)

- [x] **nested vessel-source projection ambiguity closure**
  - zone `refresh/publish/bind` projection contract 경로에서 ambiguous source path가 `missing`으로 오진되던 분기 순서를 수정
  - builtin `ToObject` / `ToTObject`도 동일한 structured `Reason/Fix` ambiguity diagnostic으로 정렬
  - nested vessel ambiguity semantic regressions 추가
- [x] **generic consumer provenance diagnostics 보강**
  - `action requires` / `zone authority` / `party role slot` / `intent step requires`에서 generic ability mismatch가 `actual type argument` / `actual implementation` provenance를 함께 보고하도록 정렬
  - 관련 semantic 회귀 추가
- [x] **anchored own/ref provenance diagnostics 보강**
  - closed-subset / local-only / missing `own/ref` / `ref` escape 진단에 `Reason/Fix`와 borrowed-here provenance를 추가
  - 관련 semantic 회귀 추가
- [x] **world embedding structured diagnostics 회귀 고정**
  - embedded zone old-binding mutation이 assignment / hosted func-action call 모두에서 `Reason/Fix`와 world-owned-copy provenance를 남기도록 semantic 회귀 강화
- [x] **Windows shell smoke portability 보강**
  - `abi_pipeline_smoke.sh`, `compare_backends.sh`가 `cmp`/`diff` 부재 환경에서도 `git` 또는 Python fallback으로 비교/차이 출력을 수행하도록 정리
- [x] **surface trust docs 정렬 — collection/result/struct baseline**
  - `Array<T>`는 `[]`, `List<T>`는 `ListNew()`, `HashMap<K,V>`는 `MapNew()`를 canonical 생성 surface로 고정
  - `Result<T>` 추출 API는 `Unwrap` / `UnwrapOr` / postfix `?`로 고정, `UnwrapResult()` 표면은 비채택
  - `struct` field의 legacy `let`은 불변 표식이 아니라 declaration introducer임을 문서화하고, 읽기 전용 계약은 `object/tobject`에만 둔다
- [x] **generic default-arg closure 1차 복구** — declaration acceptance만이 아니라 user-defined generic class omission, generic ability impl-reference omission, arity diagnostics range화, semantic/backend parity까지 다시 녹색으로 정렬
- [x] **ABI Unification Infrastructure** — `pgy_abi_spec.h`, `test_abi_spec.c` (28 PASS), `MIRTypeLayout`, `mir_abi_lookup()`, `rir_dump_json()`, dumb emitter Visitor
- [x] **Windows CI Fix** — `TOKEN_TYPE` → `PGY_TOKEN_TYPE`, `TokenType` → `PgyTokenType` (~20개 파일)
- [x] **v2 Quantum Planning** — 양자 연산 미지원 명시, v2 계획 문서화
- [x] **Documentation Index** — `docs/INDEX.md` 생성, 전체 문서 체계화
- [x] **`HashMap<Int, V>` surface trust 정렬** — semantic annotation/builtins/runtime comment/test를 `String | Int` key 지원으로 일치시킴
- [x] **mixed `ability + zone` module export 충돌 수정** — default-export `ability`가 sibling zone visibility를 깨뜨리던 정규화 버그 제거, module smoke 회귀 추가
- [x] **nominal host receiver type 오염 수정** — C backend member-call emit 중 static type-name overwrite를 제거해 `Int_Advance`류 오발행 복구
- [x] **MIR cleanup exceptional topology 회귀 복구** — cleanup/rollback/invalidation block edge materialization과 test expectation 정렬
- [x] **`order_analytics` example 실전화** — sketch 수준 surface를 정리하고 compile-smoke covered example로 승격
- [x] **declaration name surface tightening** — declaration name을 일반 식별자로만 제한하고 reserved keyword 재사용 surface 제거
- [x] **anchored-handle diagnostics/test 정렬** — `own/ref` closed-subset 진단 문구와 `DeviceSlot`/anchored-handle semantic test expectation을 현재 구현 기준으로 일치시킴
- [x] **계층형 stdlib/domain kit v0 고정** — `money`, `datetime(Duration/Instant)`, `timer`, `versioning`, `ledger`, `obligation`, `device_adapter` 모듈과 probe 예제 추가, 코어 추가 금지 원칙 문서화

## 베타 클로저 보드

베타 전 원칙:
- `부분 구현` 상태를 남기지 않는다
- 완료시키지 못하는 surface는 내리거나 experimental로 격리한다
- parser가 받는 표면은 semantic/C/LLVM/runtime/test/documentation까지 닫는다

### B0 — 의미론 클로저 필수

- [ ] **Intent/Zone/World semantics 완전 closure**
  - contract reuse/derivation / authority / lifecycle / embedding ownership / runtime observability / C/LLVM parity / regression
- [ ] **relation/effect/projection semantics 완전 closure**
  - effect lattice, authority-resource partial order 통합, refresh/publish/bind/causes 일관화, diagnostics, C/LLVM parity
- [ ] **generic contract 완전 closure**
  - strict beta-quality 기준으로 stable subset closure에서 재개방
  - `default type arg` actual resolution, `where T: A + B` 전경로 enforcement, `ability<T>` mismatch provenance, instantiation-path parity까지 닫는다
  - 완료: default type arg declaration acceptance / omitted trailing default resolution / generic ability impl-reference omission / arity diagnostics provenance
  - 남음: multi-bound 전경로 enforcement, module-contract propagation, richer mismatch provenance, wider C/LLVM regression 확대
- [ ] **own/ref 완전 closure**
  - strict beta-quality 기준으로 anchored subset closure에서 재개방
  - 일반 movable type ownership, move/borrow/escape/rebind/channel/return provenance, diagnostics/test parity까지 닫는다

### B1 — 베타 신뢰도 필수

- [ ] **surface trust 문서 재분류**
  - alpha-complete / experimental / removed 기준으로 전면 재정리
- [ ] **stable example / smoke source of truth 확대**
  - canonical examples와 closure examples를 smoke에 직접 연결
  - explicit surface vs compressed surface를 같은 의미로 보여주는 pair example 최소 4쌍 고정
  - 대상: app/web orchestration, game/simulation, async/worker/device, world-handoff/domain propagation
- [ ] **experimental surface 제거 또는 격리**
  - 닫지 못한 parser surface는 명시 거부 또는 문법 제거

## Pain point freeze board

원칙:
- 기능을 더 넓히기 전에 반복해서 다시 깨지는 작성/진단 pain point를 먼저 고정한다
- 각 pain point는 `stable contract + regression + docs wording`까지 같이 잠근다
- recoverable failure와 invariant break를 같은 방식으로 처리하지 않는다

### Failure handling policy freeze

분류:
- `recoverable failure`
  - 사용자 코드가 예상 가능한 실패
  - 예: intent failure, authority/boundary rejection, timeout, remote failure, empty/closed operational state
  - 원칙:
    - 프로세스를 죽이지 않는다
    - `Bool` / `Result<T>` / queryable runtime state로 드러낸다
    - reason / boundary / authority / step provenance를 조회 가능하게 남긴다
- `contract violation`
  - 원칙적으로 semantic 단계에서 차단
  - 런타임까지 오면 structured panic
  - 예: released slot access, invalid secure token, ownership boundary 위반
- `internal compiler/runtime bug`
  - 즉시 중단
  - internal error / panic로 명확히 분리
  - 사용자 코드 실패처럼 위장하지 않는다

현재 고정:
- intent/zone/world 쪽 실패는 장기적으로 `recoverable failure`로 수렴시킨다
- slot/token/invariant 계열은 계속 hard fail로 둔다
- `Unwrap(...)`는 panic 성격의 sharp tool로 유지하고, recoverable path의 기본 계약으로 쓰지 않는다

- [ ] **large canonical pair 예제 추가**
  - 큰 예제에서 `explicit`와 `compressed`를 둘 다 stable source of truth로 유지한다
  - 최소 4개 파일 기준으로 관리한다
    - `calendar manage-event`: explicit/compressed
    - `composite intent orchestration`: explicit/compressed
  - 목적:
    - 큰 예제의 전체 계약을 명시형으로 읽을 수 있게 유지
    - 같은 의미를 축약형으로도 바로 복사해 시작할 수 있게 유지
    - smoke에서 두 예제가 모두 실행 가능하도록 고정
- 이 보드는 sugar backlog가 아니라 beta surface trust를 지키기 위한 고정판이다
- P0 pain point가 잠기기 전에는 declaration-side MIR-only debt를 국소 복구 외에는 넓게 건드리지 않는다
- backend 내부 정리는 pain point 기준선과 회귀가 먼저 고정된 뒤에만 다시 확장한다

### P0 — 작성/계약 pain point

- [ ] **contract clause density 고정**
  - 대상: `requires / within / authorized by / causes / refresh / publish / bind`
  - 문제: 같은 의미를 action / intent step / zone에서 중복 기술하게 되어 작성 피로가 커짐
  - 고정 기준:
    - 어디까지 inherited/derived 되는지 vocabulary를 고정
    - 길게 쓰는 버전과 압축 버전의 의미 차이가 문서/진단/예제에서 같아야 함
    - canonical pair와 minimal subset example의 역할을 분리해 source-of-truth를 고정
  - 회귀 기준:
    - semantic regression: inherited/derived contract source가 진단에 노출
    - example smoke: long-form vs compressed-form 예제 둘 다 유지

현재 source-of-truth:
- canonical pair
  - `examples/intent_contract_pair_minimal.pgy`
  - `examples/authority_contract_pair_minimal.pgy`
  - `examples/transfer_contract_pair_minimal.pgy`
- stable minimal subset
  - `examples/action_contract_inheritance_minimal.pgy`
  - `examples/intent_contract_derivation_minimal.pgy`
  - `examples/transfer_move_minimal.pgy`
  - `examples/transfer_move_typed_minimal.pgy`
  - `examples/zone_context_minimal.pgy`

- [ ] **contract provenance vocabulary 고정**
  - 대상: `inferred_*` 잔여 표현, contract source wording, docs/example terminology
  - 문제: compiler type/effect inference와 domain contract 상속/파생이 같은 단어로 섞이면 설명력이 무너짐
  - 고정 기준:
    - domain contract는 `상속 / 파생`과 `inherited / derived`로만 부른다
    - 일반 compiler 의미는 type/effect `inference`에만 남긴다
  - 회귀 기준:
    - parser/semantic diagnostics 기대 문자열 고정

### P0.5 — recoverable failure 분류/고정

- [ ] **failure class inventory 정리**
  - intent/zone/world/runtime API를 `recoverable failure / contract violation / internal bug`로 분류
  - 현재 panic인 경로 중 recoverable이어야 하는 것을 표로 정리
- 현재 inventory baseline:
  - recoverable 유지:
    - `Result<T>` / `?`
    - `RemoteFuture<T> -> Result<T>`
    - channel timeout / non-blocking / closed state
    - world roster timeout
    - `IntentLast* / History* / Active* / Recent*`
  - hard-fail 유지:
    - released slot / invalid token / token permission mismatch
    - `Unwrap(...)` on `Err`, option unwrap on `None`
    - allocator / box / rc / weak invariant break
    - array / slice bounds violation
    - current runtime zone authority null-guard
      - 참고: 이건 아직 real authority rejection이 아니라 invariant check라서 hard-fail 유지 쪽이 맞다
  - first-wave conversion targets:
    - future real runtime authority rejection
    - intent boundary/authority mismatch provenance at runtime
- [ ] **intent/zone/world recoverable failure baseline**
  - intent failure, authority rejection, boundary mismatch는 process abort 대신 queryable reason/state로 노출
  - runtime observability와 diagnostics wording을 같은 provenance vocabulary로 정렬
- [ ] **runtime authority guard downshift**
  - 현재 `pgy_zone_authority_check_export(...)`는 null self/null participant invariant guard다
  - 이 guard 자체는 hard-fail 유지
  - 별도 real authority rejection runtime path가 생기면 그쪽을 `recoverable authority failure` 경로로 설계
- [ ] **hard-fail boundary 명시**
  - released slot, invalid token, invariant corruption은 계속 panic이라는 점을 문서와 테스트에 명시
    - docs wording search 기준 고정

- [ ] **projection contract diagnostics 고정**
  - 대상: `refresh/publish/bind` source/target/path/field-map 실패
  - 문제: projection은 언어 강점인데 실패 이유가 약하면 가장 먼저 피로를 줌
  - 고정 기준:
    - target slot / source slot / projection kind / field path / fix가 모두 진단에 들어감
    - structured `Reason:` / `Fix:` formatting을 source-of-truth로 고정
  - 회귀 기준:
    - semantic regression: missing source field / ambiguous path / wrong projection kind / duplicate field map

현재 source-of-truth:
- stable example
  - `examples/projection_bind_group_minimal.pgy`
  - `examples/projection_refresh_publish_group_minimal.pgy`
- semantic regression
  - `src/test_semantic.c:test_projection_contract_diagnostics`

- [ ] **surface trust subset 분류 고정**
  - 대상: generics, own/ref, collections, runtime observability
  - 문제: 되는 것처럼 보이는데 실제로는 subset만 되는 surface가 가장 큰 신뢰 손상 지점
  - 고정 기준:
    - `stable subset / explicit reject / beta-out-of-scope`를 TODO/docs/diagnostic에서 같은 말로 쓴다
  - 회귀 기준:
    - semantic tests와 depth docs가 같은 subset을 가리킴

현재 고정하려는 baseline:
- generics
  - stable subset: exact/ability/multi-bound baseline
  - stable subset extension: default type argument actual resolution on implemented declaration/call/module-consumer paths
  - beta-out-of-scope: broader generic generalization
- own/ref
  - stable subset: anchored slot-handle boundary subset
  - explicit reject: general own/ref on non-anchored/general value types
  - beta-out-of-scope: general ownership system
- collections
  - stable subset: `List<T>`, `Set<T>`, `HashMap<String, T>`, `HashMap<Int, T>`
  - explicit reject: unsupported map key kinds
  - beta-out-of-scope: arbitrary key-universal collection contracts
- runtime observability
  - stable subset: `last / history / active / recent`
  - explicit reject: 없음
  - beta-out-of-scope: richer multi-instance timeline query와 deeper failure provenance query

### P1 — 내부 구조 pain point

- [ ] **declaration-side MIR-only debt 고정**
  - 대상: declaration inventory / metadata helper / duplicated named-decl lookup
  - 문제: routine body는 MIR로 정리돼도 decl-side helper debt가 남으면 parity bug가 반복됨
  - 고정 기준:
    - backend lookup은 공통 inventory helper를 사용
    - 남은 debt는 “기능 미구현”이 아니라 “AST-carried decl metadata 구조 debt”로 분리해서 기록
  - 회귀 기준:
    - LLVM/C backend helper duplication 감소
    - debt ledger와 TODO 표현 정렬

- [x] **runtime observability baseline vs richer query 구분 고정**
  - 대상: `IntentLast* / IntentHistory* / IntentActive* / IntentRecent*`, zone/world inspection
  - 문제: baseline이 이미 있는데 문서가 thin이라고 쓰면 반대로 surface trust를 깎음
  - 고정 기준:
    - baseline observability는 complete로, richer timeline/provenance는 open debt로 분리
  - 회귀 기준:
    - docs/board/status 문구 일치
    - observability regression이 baseline API를 계속 고정

## 완료 (P0 — 즉시 수정)

- [x] **`system()` 명령 주입 제거** — `_spawnvp`/`execvp`로 교체, 경로 검증 추가 (`pgy_path_is_safe`)
- [x] **AES-256 실구현** — XOR 가짜 암호를 FIPS 197 AES-256-CTR + HMAC-SHA256 인증으로 교체 (외부 의존성 없음)
- [x] **`auto __tmp` 제거** — `PGY_RESULT_TRY` 매크로에서 GCC 확장 `auto` 제거, C11 호환 (명시적 타입 파라미터)
- [x] **REPL 고정 파일명** — `_pgy_repl_tmp.*` → `TMPDIR/pgy_repl_{pid}.*` (PID 기반 유니크 경로)
- [x] **`type alias` vertical slice** — `type UserId = Int;` parser/semantic/C/LLVM lowering 연결, 실전 annotation/typedef 경로 확보

## P1 — 다음 단계

- [ ] **CI 하드닝** — Ubuntu + Windows 빌드 매트릭스 유지, AddressSanitizer/UBSan, 더 촘촘한 smoke coverage
- [ ] **CodeQL + secret scanning 활성화** — C/C++ 분석 모드, push protection
- [ ] **CHANGELOG.md + 버전 정책 수립** — SemVer, 릴리스 태깅 규칙
- [ ] **SECURITY.md** — 보안 취약점 제보 채널, 책임 있는 공개 정책

## P1.5 — 언어/컴파일러 보강

- [ ] **MIR DCE statement-level 확장**
  - 현재는 dead SSA/PHI 제거 + `HasState`/`ChannelLength`류 pure-query stmt 제거까지는 동작함
  - 남은 단계: pure expression stmt / dead call / dead resource-op / carrier stmt를 더 세분화하고, side-effect lattice 기준으로 제거 정책을 정교화
  - 목표: MIR-only emitter가 기대하는 metadata carrier를 잃지 않으면서도 불필요한 stmt 제거 범위를 넓힘

- [x] **IR 계층 설계 검토** — HIR/DIR/RIR/MIR 분리 타당성 평가
  - **DIR 유지 결정**: intent domain structure 검증에 필수 (step dependency, zone binding, post-condition)
  - **RIR 유지 결정**: resource state lattice (20-state)는 slot/projection/authority lifecycle 검증에 필요
  - **MIR 유지 결정**: SSA/CFG/cleanup edge는 intent compensation execution path에 필수
  - ~~남은 과제~~: Backend를 HIR 기반 → MIR 기반으로 전환해야 IR 투자 ROI 실현 → **완료**
  - 참고: Rust도 AST→THIR→MIR→LLVM 4단계, Pergyra는 AST→HIR→DIR→RIR→MIR→Backend 6단계
  - DIR은 domain graph로 HIR와 구조가 달라 별도 IR로 유지하는 것이 타당
  - RIR 20-state lattice는 단순화 가능성 검토 (현재: Owned/Borrowed/Synced/Dirty/Stale/Published/Authorized 등)
- [ ] **ability 기반 연산자 dispatch 고도화** — 현재는 `role/impl ability` 메서드에서 `operator_<suffix>_<Type>` alias를 합성해 C/LLVM이 정적으로 호출하는 방식. 장기적으로는 ability/vtable 기반의 직접 dispatch와 더 정교한 overload 우선순위 규칙이 필요
- [ ] **LLVM 연산자 오버로드 회귀 테스트 확장** — 현재 스모크는 `role IntMath for Int` 1건 중심. 비교 연산, 포함된 role, enum/custom type, namespace 경로까지 자동 테스트 확대

## P1.58 — 표준 라이브러리 인프라

- [x] **`use datetime;` 실제 stdlib module화**
- [x] **`use http;` v0.1**
  - `HttpRequest`, `HttpResponse`, `RouteSpec`
  - `OkResponse`, `ErrorResponse`, `JsonResponse`
  - intent adapter handler 예제와 연결
- [x] **`use storage;` v0.1**
  - `SnapshotMeta`, `SnapshotRecord`
  - `StorageSave`, `StorageLoad`, `StorageAppendLog`
  - world/session snapshot 예제와 연결
- [x] **`use page;` v0.1**
  - `PageRoute`, `PageAction`, `PageMessage`
  - `MountPage`, `BindAction`, `RenderSection`
  - projection surface / action binder 예제와 연결
- [x] **쇼핑몰 예제를 stdlib 인프라 사용 버전으로 리프트**
  - `pages/` -> `use page;`
  - `api/` -> `use http;`
  - `report/storage` -> `use storage;`

- [ ] **`pgy scaffold project`에 app-infra starter 추가**
  - intent-first layout + `intents/ subjects/ zones/ world.pgy main.pgy`
  - optional `pages/ api/ report/` app adapter starter

## P1.58 — 표준 라이브러리 개선 (2026-04-06 분석)

- [ ] **stdlib page.pgy 실제 렌더링/컴포넌트 시스템으로 확장**
  - 현재: 단순 데이터 구조 + 렌더링 문자열 함수만
  - 목표: 페이지 라이프사이클(마운트/언마운트/업데이트), 컴포넌트 트리, 상태 관리
  - 제안: `Component` abstract base, `mount()`, `render()`, `update()`, `unmount()` 라이프사이클 훅
- [ ] **stdlib storage.pgy WriteFile 추상화**
  - 현재: `WriteFile` 내장 함수 직접 호출 → 플랫폼 의존성
  - 목표: Slot/Device 인터페이스로 분리 (`StorageDevice` ability)
  - 제안: `ability StorageDevice { Write(path, data) -> Result<Void, Error>; Read(path) -> Result<String, Error> }`
- [ ] **stdlib 전반 Result<T, Error> 패턴 활용**
  - 현재: `WriteFile`, `ReadFile` 실패 시 크래시 가능성
  - 목표: 모든 I/O 연산이 `Result<T, Error>` 반환
  - 제안: `?` 연산자와 조합해 에러 전파 자동화
- [ ] **datetime.pgy 메서드 일관성 개선**
  - 현재: `export class LocalDate` + `export func SameDate()` 혼재
  - 제안: 메서드 일관성 (`a.SameDate(b)` vs `SameDate(a, b)`) — 하나만 남기거나 둘 다 문서화

## IR 파이프라인

- [x] **DIR code layer 시작**
  - declaration graph
  - intent participant/step edge
  - role/ability completeness edge
- [x] **RIR code layer 시작**
  - explicit resource/projection/authority/capability/intent-policy fact
  - explicit resource op
  - scope-level normalized state summary
  - HIR-enriched branch/join `flow-block[...]` lattice summary
- [x] **MIR code layer 시작**
  - block/instruction skeleton
  - phi materialization
  - block-local SSA rename
  - instruction-level `def/use` 시작
  - rollback/invalidation exceptional CFG 시작
- [ ] **RIR lattice propagation 심화**
  - relation/effect/zone/world handle merge는 시작됨, conditional handle invalidation과 world-handoff lattice를 더 밀기
  - conditional authority/projection invalidation fact 확장
- [ ] **MIR full SSA / flow merge**
  - block-level version map은 시작됨, rename을 full def-use chain/liveness 수준으로 확장
  - cleanup convergence root는 시작됨, MIR-level `RIR-flow` merge와 cleanup convergence policy를 더 고도화
- [ ] **MIR DCE 확장 (statement-level)**
  - dead DEF/PHI 제거를 넘어 side-effect-free STMT/unused call 제거
  - 현재는 pure query builtin (`Has*`, `ChannelLength/Capacity/Space/Full/Closed`)만 안전 제거 시작
  - `unused pure let initializer` 제거는 source-local/runtime-backed storage와 충돌해 다시 보류
  - dead identifier-assign 제거는 loop/phi/live-out 오판이 남아 있어 계속 보수 보류
  - 다음 reopen 조건: value summary의 block-boundary / phi provenance를 이용해 loop-carried DEF와 진짜 dead local DEF를 분리
  - user call purity는 아직 보수적으로 side-effect 있다고 간주
  - RESOURCE_OP/CLEANUP_EDGE/abort/IO 등 side-effect 보존 규칙 명시
  - RPO 기반 liveness와 결합해 제거 정확도 개선
## P2.0 — Backend MIR 기반 전환 ✅ 완료

- [x] **emit_program()을 HIR 기반 → MIR 기반으로 전환**
  - **완료**: `emit_func_decl_from_mir_named()` 완전 구현
  - **결과**: MIR routine → SSA locals + CFG → C 코드 생성
  - **지원 기능**:
    - Intent compensation (cleanup blocks)
    - SSA versioned locals (`_pgy_ssa_name_N`)
    - PHI 노드 복사 (join block 진입)
    - BRANCH → if/else gotos
    - RESOURCE_OP → 런타임 함수 호출
  - **테스트**: 428 passed, 0 failed (기존 403 passed, 5 failed)
  - **아키텍처**:
    ```
    Domain IR:   Intent Recover → policy exclusive → step Heal → zone main → participant unit
    Resource IR: IntentBegin I1 → ConflictCheck exclusive → BindZone main → CallAction Recover
    MIR:         bb0: conflict_check(unit) → br !r0, bb_fail, bb1
                 bb1: call recover(unit) → call sync_projection(main, unit)
                 bb_commit: intent_commit(I1) → ret true
                 bb_fail: intent_abort(I1) → ret false
    ```

## P2.1 — LLVM 백엔드 MIR 기반 전환 ✅ 완료

- [x] **LLVM 백엔드 MIR 기반 전환 완료**
  - `src/codegen/llvm_pipeline.c`: MIR routine → LLVM IR 직접 생성
  - `src/codegen/llvm_mir_emit.c`: `llvm_emit_func_from_mir()` 완전 구현
  - SSA locals, PHI nodes, branch terminators, intent compensation 모두 지원
  - 기대 효과 달성: LLVM 최적화 패스 완전 활용, C/LLVM 백엔드 아키텍처 통일
  - C/LLVM 둘 다 MIR 기반으로 통일 → IR 투자 ROI 실현

## P1.55 — 언어 기능 확장

### 기반 타입 시스템
- [x] **태그드 유니언 (enum with data)** — `enum Shape { Circle(Int), Rect(Int, Int) }` 데이터를 가진 enum
  - 완료: variant payload 파싱, variant 생성자 타입 추론, C tagged union / LLVM discriminated struct, LLVM tagged-union regression 및 예제 실행
- [x] **Option<T> / None** — "상자가 비어있을 수 있다"를 타입으로 표현. `-1` sentinel 제거
  - 완료: `Option<T>` constructed type, `Some/None`, `IsSome/IsNone/UnwrapOption`, C/LLVM lowering
  - 완료: `match opt { case Some(v): ... case None: ... }` destructuring
- [ ] **디스트럭처링** — `let (slot, token) = ClaimSecureSlot<Int>()` 등 패턴 기반 바인딩 확장
- [ ] **sealed ability** — 구현 가능한 role을 제한 (`sealed ability Combatable` → 같은 모듈 내 role만 impl 가능)
- [x] **문자열 보간** — `f"값은 {x}"` → `StringConcat(...)` series로 lowering
  - 완료: lexer에서 `f"..."` → `TOKEN_INTERPOLATED_STRING`
  - 완료: parser에서 `{expr}` 파싱, `ToString(expr)` + `+` concatenation으로 분해
  - 완료: 기존 `"${expr}"` 레거시 문법도 호환 유지

### 에러 처리
- [x] **`?` 연산자** — `Result<T>` 에러 자동 전파. `let val = riskyFunc()?;` → 에러 시 즉시 반환
  - 완료: 시맨틱 검증, C early-return lowering, LLVM `Result<T>` 레이아웃/unwrap/early-return lowering, `pipe_and_try.pgy` C/LLVM 실행 검증

### 편의 문법
- [x] **파이프 연산자** — `data |> Transform |> Validate |> Persist` 단방향 데이터 흐름
- [x] **defer** — `defer Release(s)` 스코프 종료 시 자동 실행
- [x] **`let` 타입 추론** — initializer 기반 기본 추론은 현재 구현됨
  - 완료: annotation이 없을 때 initializer 타입으로 추론
  - 남음: 문서/표면 예시를 더 공격적으로 타입 추론 중심으로 정리할지 결정

### 제네릭 클래스
- [x] **제네릭 클래스** — `class Pair<T>` 문법 + 시맨틱 + C 코드젠 (단형화). 예제: `examples/generic_class.pgy`

### Slot 소유권 모델
- [x] **`own`/`ref` 소유권 모델 확정 및 구현** — move 기본, 함수 시그니처에 명시
  - 완료: `own`/`ref` 키워드 (렉서/파서/AST), Slot 대입 시 move 시맨틱, Clone() 명시적 복사
  - `func Upload(own tex: Slot<Texture>)` → 소유권 이전, 원본 무효
  - `func Render(ref tex: Slot<Texture>)` → 빌림, 원본 유효
  - 문서화: `docs/22_ownership_model.md`

### Slot 표면 문법 개선 (P0 우선순위)
- [x] **암묵적 Read + 대입 기반 Write** — Slot의 기본 사용 표면을 일반 변수처럼
  - 완료: 읽기 문맥에서 `Slot<T>` auto-read
  - 완료: `slot = expr` → `Write(slot, expr)` lowering
  - 유지: `Release(slot)`는 계속 명시적

### Slot 최적화 (P0 우선순위)
- [x] **스택 할당 최적화** — 스코프를 벗어나지 않는 Slot은 malloc 대신 alloca
  - 완료: `slot_analyze_escape_flags()` (slot_analyzer.c)
  - 완료: LLVM 백엔드에서 `slot_escapes == false` 시 alloca 생성 (llvm_stmt.c:145-146)
  - 완료: escape analysis로 non-escaping slot 자동 스택 할당

### View 범위 부여 (리뷰 필요 — 미결정)
- [ ] **View에 바이트/인덱스 범위 부여** — 실제 사용 사례 만들어보고 결정
  - 안 A: Slice 기반 — `SliceOf(buf, 0, 1024)` → Slot의 "창문"
  - 안 B: View에 범위 부여 — `ViewRead(buf, offset, length)`
  - **미결정 — 파일 I/O, 네트워크 버퍼, GPU 텍스처 사례를 만들어보고 결정**

### 병렬/채널
- [x] **select 실체화** — 여러 채널 중 먼저 준비된 것을 처리

### 언어 완성도 Tier 1 — 범용 필수
- [x] **for-in 컬렉션 루프** — `for item in array { }` 배열/컬렉션 순회
  - 완료: Array<T>/Slice<T> 특수화 (index loop lowering), 시맨틱 element type 추론
  - 남음: ability 기반 Iterable<T> 프로토콜 (Tier 2)
- [x] **StringSplit / StringJoin** — 문자열 분리/결합 빌트인 실체화
  - 완료: `Split(s, delim) → Array<String>`, `Join(arr, sep) → String`
- [x] **ToInt / ToFloat** — 문자열→숫자 변환 빌트인
- [x] **기본 Math 빌트인** — Sqrt, Pow, Floor, Ceil, Random 추가 (기존 Abs/Min/Max + 신규 5개)
- [x] **ArraySort / ArrayMap / ArrayFilter / ArrayReverse** — 고차 함수 기반 컬렉션 연산
  - 완료: ArraySort(arr) → qsort, ArrayMap(arr, fn) → 새 배열, ArrayFilter(arr, fn) → 조건 필터, ArrayReverse(arr) → 뒤집기
  - fn은 함수 이름 또는 람다 (C 함수 포인터로 lowering)
- [x] **디스트럭처링** — `let (a, b, c) = expr` 배열/컬렉션 positional 바인딩
  - 완료: Array<T> → 인덱스 기반 추출 (`result.data[0]`, `result.data[1]`, ...)

### 메타프로그래밍 입장 (결정 완료)
- [x] **TMP 비채택** — 제네릭 monomorphization + ability dispatch로 95% 커버. 문서: `docs/23_metaprogramming_position.md`
- [ ] **향후 코드 생성 필요 시** — 컴파일 타임 플러그인 (proc_macro 모델) 또는 소스 생성기 검토

### 언어 완성도 Tier 2 — 실사용 편의
- [ ] **innate ability** — 같은 모듈 내 role만 impl 허용 (sealed 대신 innate 채택. 문서: `docs/24_visibility_model.md`)
  - 파서 완료, 시맨틱에서 `innate` 키워드 인식 (type_checker_decls.inc 참조)
  - 남음: 모듈 경계 검증 로직 완성
- [x] **제네릭 constraint 시맨틱** — `where T: Comparable` 시맨틱 검증
  - 완료: 파서 + 시맨틱 검증 (type_checker_helpers.inc:1847)
  - 완료: Generic function where-clause constraint validation
- [x] **OR 패턴** — `case 1 | 2 | 3:` match에서
  - 완료: lexer `TOKEN_PATTERN_OR`, parser 파싱, 시맨틱 검증
  - 완료: 리터럴 OR 패턴 지원 (`case 1 | 2 | 3:`)
  - 제한: variant destructuring OR 패턴은 아직 미지원 (`case .Some(v) | .None:`)
- [ ] **enum 메서드** — `enum Direction { ... func Name(self) -> String }`
- [ ] **labeled break/continue** — `outer: while { ... break outer; }`
- [ ] **Custom error 타입** — `Result<T, E>` where E is user type (현재 String만)

### ability 차별화
- [ ] **ability ≠ interface 문서화** — ability는 "협업 프로토콜의 자격 조건"이며 슬롯에 부착됨

## P1.6 — 자원/오케스트레이션 방향 고정

### 분산 설계 결정 (2026-04-03 확정)
- [x] **RemoteFuture `await` → `Result<T>` 강제** — 원격 자원의 지연/실패를 타입 시스템에서 강제 노출
  - `Future<T>` (로컬) → await → `T` (실패 없음)
  - `RemoteFuture<T>` (원격) → await → `Result<T>` (실패 가능)
  - 시맨틱 체커 + C 코드젠 + 런타임 매크로 구현 완료
  - 테스트: 205 semantic + 141 transpile 통과
- [x] **RemoteFuture에 Claim/Read/Write/Release 차단** — 원격 자원의 동사는 Submit/Await만
  - Read/Write/Release 호출 시 친절한 에러 메시지 출력
  - "RemoteFuture does not support Read(); use 'await' to obtain Result<T>"
- [ ] **원격 Slot은 Claim 없이 Channel 기반 메시지 패싱만** — 분산 락 회피
  - 크로스 World 통신은 `Channel<T>`만 허용
  - 원격 자원에 Claim 동사를 사용하면 컴파일 에러
- [x] **World 경계 = 실패 도메인 경계** — 크로스 World 통신은 Channel만
  - 완료: World 시맨틱 체커 (`type_check_world_decl`, type_checker_decls.inc)
  - 완료: World 코드젠 (C 백엔드, transpiler_helpers.inc)
  - 완료: `HasZoneProjection`, `HasZoneLayer`, `HasZoneState` builtin

### Projection / Domain Query
- [x] **Projection query surface** — `HasProjection(slotName)`으로 relation/effect/zone 문맥에서 object/tobject projection slot의 sync-ready 여부를 질의
  - 완료: semantic + C/LLVM lowering
  - World 내부의 Slot은 로컬 (zero-cost), World 간은 Channel (명시적 비용)

### 스케일링 대응 (레드팀 피드백 기반)
- [ ] **백엔드 역할 컷오프 고정** — C = reference/fallback, LLVM = optimization/mainline
  - 같은 의미론을 두 백엔드에 유지하되, 공격적 최적화와 type-erased fast path는 LLVM에만 집중
  - C 백엔드는 MVP 호환성, 디버깅, 폴백, 부트스트래핑 역할로 제한
  - 새 기능 추가 시 "C에서도 반드시 최적화 경로까지 구현해야 하는가?"를 기본적으로 `아니오`로 둠
- [ ] **매크로 조합 폭발 대응** — C 매크로 monomorphization의 장기 대안
  - 현재: `PGY_SLOT_DEFINE`, `PGY_CHANNEL_DEFINE` 등 타입별 전개 (부트스트래핑 전략)
  - 대안: LLVM 백엔드에서 type-erased 경로 (opaque ptr + vtable) 추가
  - LTO + dead code elimination으로 바이너리 비대화 억제
- [ ] **코드젠 이중화 억제 규칙** — bifurcation trap 방지
  - 동일 기능의 C/LLVM lowering이 영원히 쌍으로 비대해지지 않게 공통 의미론 테스트 우선
  - backend compare / smoke를 계약으로 유지하고, backend-specific fast path는 명시적으로 분리
- [ ] **Async 힙 할당 오버헤드 감소** — 고성능 분산 I/O를 위한 런타임 최적화
  - 현재: `pgy_spawn` + `malloc` per task
  - 대안: Arena allocator 기반 task pool, io_uring/IOCP zero-copy I/O
  - 코루틴 스택은 이미 fiber 기반 (pgy_parallel.h)
  - 단, 언어 코어와 OS 전용 스케줄러를 강결합하지 말 것
- [ ] **BYOS (Bring Your Own Scheduler) 경로 설계** — async 의미론과 스케줄러/I/O 모델 분리
  - 언어는 task/future/channel 의미만 고정
  - 실제 polling/runtime은 플랫폼별 주입 가능 계층으로 분리
- [ ] **ABI 다형성 전략** — 크기가 다른 슬롯 타입의 제네릭 처리
  - 의도적 설계: `Slot<T>` ≠ `SecureSlot<T>` (보안 차원 분리)
  - 다형성 필요 시: `ability` vtable dispatch (Party 시스템에 이미 구현)
  - Boxing 필요 시: `Rc<T>` + ability 조합
  - `Rc<T> + dyn ability`는 explicit high-cost path로 문서화
  - 값 경로(struct), 객체 경로(class), 동적 경로(Rc + dyn ability)를 성능 계약으로 구분

### 기존 항목
- [x] **Slot Protocol 고정** — Claim/Access/Mutate/Transfer/Release 불변 계약
- [x] **Slot/View 계층 마감** — ReadView/WriteView/MoveToken 권한 축소/이전 계층
- [ ] **슬롯을 추상 자원 핸들로 일반화** — 장기적으로 MemorySlot, DeviceSlot, SessionSlot 등 자원 클래스 확장
- [ ] **채널 의미론 강화** — 비동기 제출/대기/수거/후처리 흐름 보강
- [x] **`Future<T>`를 transfer boundary로 고정** — await/recv와 같은 ownership 경계
- [ ] **effect/resource capability 표기 도입** — `local cpu`, `secure device`, `remote` 등 타입/효과 시스템
  - 현재: inferred effect mask + spawn/await/channel에서 remote 추론
  - 현재: `/// @effects ...` 선언이 있으면 body inferred effect와 mismatch 진단
  - 다음: 시그니처 문법 차원의 선언적 annotation 표면
- [ ] **성능 목표를 orchestration overhead 중심으로 재정의**

## P1.7 — 의미 통일 언어로서의 다음 단계

### 비용 모델 / effect
- [ ] **비용 모델 표면화** — "semantic unity, visible cost" 원칙
  - `local / secure / remote / device` 자원군의 비용 차이를 표면에 드러내기
- [ ] **effect system 2단계** — 선언적 effect 표기, mismatch 진단
  - 부분 완료: structured comment `@effects` 기반 mismatch 진단
  - 부분 완료: source-level `with effects ...` 시그니처 surface
  - 남음: 더 정교한 effect lattice, call-site contract surface

### 상위 계층 모델
- [x] **최종 문맥 계층 고정** — `ability -> role -> party -> relation -> effect -> zone -> world`
  - 완료: `world`를 최상위 실행/신뢰/실패 경계라는 목표 정의로 문서화
  - 완료: 상위 레이어로 갈수록 덜 구속적이라는 설계 원칙 문서화
  - 완료: `relation`, `effect`, `zone` declaration keyword와 최소 `subject slot` / `object slot` surface를 parser/semantic 표면에 연결
  - 완료: `zone -> relation/effect`, `world -> zone` 최소 조립 slot surface를 parser/semantic에 연결
  - 완료: `relation`, `effect`의 optional `for ...` header로 subject endpoint/target 최소 surface를 연결
  - 완료: `zone`의 `apply effectSlot to targetSlot` 최소 attachment surface를 parser/semantic에 연결
  - 완료: `zone`의 `link relationSlot between left, right` 최소 relation wiring surface를 parser/semantic에 연결
  - 완료: `zone`의 `detach effectSlot from targetSlot`, `unlink relationSlot between left, right` 최소 release surface를 parser/semantic에 연결
  - 완료: `zone`의 `apply/detach`, `link/unlink`를 `effect/relation` declaration contract와 기본 타입/arity 수준으로 연결
  - 완료: `zone` subject shape에 대한 권장 lint 추가
  - 완료: `tobject` keyword를 `struct` 호환 projection alias로 추가
  - 완료: `ToObject(TargetStruct, subjectBinding)` 최소 passive projection surface를 semantic/C backend에 연결
  - 완료: `ToTObject(TargetDto, subjectBinding)` 최소 projection surface를 semantic/C backend에 연결
  - 완료: `relation/effect/zone`에 `tobject slot` surface를 연결
  - 완료: `relation/effect/zone`의 domain slot에 optional initializer를 연결해 `object slot view: View = ToObject(View, subject)` 같은 projection wiring을 직접 표현 가능하게 함
  - 완료: `zone`의 `refresh objectSlot from subjectSlot` surface로 projection 갱신 흐름을 parser/semantic에 연결
  - 완료: `zone`의 `publish dtoSlot from subjectSlot` surface로 tobject projection 갱신 흐름을 parser/semantic에 연결
  - 완료: `zone`의 `maintain effectSlot on targetSlot`, `maintain relationSlot between left, right` surface로 지속 lifecycle rule을 parser/semantic에 연결
  - 완료: `maintain` duplicate/conflict warning (`maintain` + `detach/unlink`) 추가
  - 완료: `zone`의 `authority subjectSlot` surface와 optional `by subjectSlot` authority annotation을 parser/semantic에 연결
  - 완료: `authority subjectSlot requires Ability[, Ability]` ability-gated authority surface를 parser/semantic에 연결
  - 완료: `zone`의 `state name: effect ... on ...` / `state name: relation ... between ..., ...` lifecycle alias surface를 parser/semantic에 연결
  - 완료: `zone`의 `apply/link/detach/unlink/maintain stateName` shorthand를 parser/semantic에 연결
  - 완료: `HasState(stateName)` zone query builtin을 parser/semantic에 연결하고 C backend에서 zone state field query로 lowering
  - 완료: `HasLayer(layerSlot)` zone query builtin을 parser/semantic에 연결하고 C/LLVM backend에서 zone layer field query로 lowering
  - 완료: `HasState(effectState, targetSlot)` / `HasState(relationState, leftSlot, rightSlot)` slot-aware state query를 semantic에 연결
  - 완료: `world`의 `state name: zone zoneSlot`, `activate/deactivate/maintain zoneOrState` lifecycle surface를 parser/semantic에 연결
  - 완료: `HasZone(zoneOrState)` world query builtin을 parser/semantic에 연결하고 C backend에서 world zone-state/active field query로 lowering
  - 완료: C backend가 zone/world마다 sync helper를 생성하고 method 전후에 `refresh`/`publish` projection과 lifecycle flag를 incremental하게 동기화
  - 완료: `relation`, `effect` declaration이 C/LLVM backend에서 struct + method wrapper로 codegen되고 runtime instance constructor/method path가 연결됨
  - 완료: `zone` layer slot이 C/LLVM에서 typed overlay runtime instance로 유지되고 sync가 subject slot을 layer endpoint/target에 바인딩한 뒤 projection sync까지 수행
  - 완료: direct `apply/link/detach/unlink`와 `maintain effect/relation/state`가 C/LLVM zone sync에서 실제 layer/state propagation으로 연결됨
  - 완료: zone embedded overlay projection read (`self.poison.view.hp`, `self.trust.packet.name`)가 LLVM runtime smoke로 검증됨
  - 완료: `world`가 `HasZoneProjection(zoneSlot, projectionSlot)` / `HasZoneLayer(zoneSlot, layerSlot)` / `HasZoneState(zoneSlot, stateName)`로 embedded zone runtime flag를 직접 질의할 수 있음
  - 완료: `ability/role/party/relation/effect/zone/roster/world` 전체 구현
  - 완료: `world`가 `state name: all zoneOrState[, ...]` / `state name: any zoneOrState[, ...]`로 앞서 선언된 zone/state alias를 최소 조합 contract로 합성
  - 남음: richer world-level runtime semantics, 더 깊은 cross-layer propagation policy

### 존재론 모델
- [x] **subject-first 존재론 고정** — `struct` vs `subject`
  - 완료: `subject = 상태와 identity를 가진 주체 타입`으로 문서화
  - 완료: `subject`와 `class`를 서로 다른 nominal flavor로 분리하고 의미론도 1차 분기
  - 완료: legacy host-profile surface를 제거하고 `subject`/`object`/`intent` 중심으로 정리
  - 완료: `entity`는 코어 언어 존재론에 넣지 않고 프레임워크/도메인 용어로 남긴다고 문서화
  - 완료: `object`는 intent를 시작하지 않는 passive state target이라고 문서화
  - 완료: `tobject`는 object의 외부 경계용 축약 투영이라고 문서화
  - 완료: `subject`, `class`, `struct`, `object`, `tobject` declaration flavor를 parser AST에 분리 기록
  - 완료: `subject slot`과 `ToObject` / `ToTObject` source가 `subject` host만 받도록 semantic 분기
  - 완료: `object` keyword alias를 parser/LSP surface에 반영
  - 완료: `object`를 passive state/value 형식으로, `tobject`를 더 좁은 projection/value 형식으로 정리하고 helper method를 허용
  - 완료: `vessel` declaration과 `subject` 내부 `vessel` field surface 추가
  - 완료: `subject` 전용 `action` declaration과 최소 clause (`requires/within/causes/authorized by`) parser/semantic 연결
  - 완료: `subject` 안의 legacy `func` 제거, `action` only 정책으로 승격
  - 완료: `role`/`party`/`authority`를 subject-first로 더 강하게 제한
  - 완료: C/LLVM method lowering에서 `subject=self-cell`, `class=value self` 1차 분기
  - 완료: legacy host-profile surface를 제거하고 관련 규칙을 `subject`에 통합
  - 완료: `subject` 단일 host surface로 통일
  - 완료: standalone host-profile surface 삭제
  - 완료: object를 effect/relation target으로 semantic/C/LLVM에 연결
  - 완료: domain-local `refresh` / `publish` source를 subject/object까지 확장하고 tobject source는 금지
  - 완료: relation/projection 중심 surface 고정

### slot 권한 / 자원군 확장
- [ ] **slot 권한 모델 고도화** — 공유 읽기 vs 독점 쓰기, capability narrowing
- [ ] **실제 자원군 확장** — SessionSlot, ChannelSlot, RemoteJob 고도화
- [x] **subject/class/object model 구현 정렬**
  - 완료: subject direct copy/plain value parameter/return 금지, positional constructor
  - 완료: C/LLVM lowering 1차 분기 (`subject=self-cell`, `class=value self`)
  - 완료: legacy host-profile을 `subject` 규칙으로 통합
  - 완료: `subject` 단일 host surface로 통일
  - 완료: plain/secure `Slot<subject>` local object-cell anchor 지원
  - 완료: `own/ref Slot<subject-host>` / `SecureSlot<subject-host>` 함수 경계 전달을 semantic + C/LLVM backend에 반영
  - 완료: `Box<class>` explicit handle surface (`Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`)
  - 완료: richer object-handle cell propagation

### orchestration 완성도
- [ ] **오케스트레이션 모델 강화** — select 공정성, timeout, cancellation, backpressure
  - 부분 완료: `TryRecv/RecvTimeout -> Option<T>`, `TrySend/SendTimeout -> Bool`
  - 부분 완료: `TrySendStatus/SendTimeoutStatus -> Option<Bool>`로 full/timeout vs closed를 값으로 구분
  - 부분 완료: `ChannelLength/ChannelCapacity/ChannelSpace -> Int`, `ChannelFull/ChannelClosed -> Bool`
  - 부분 완료: `select` round-robin 시작 인덱스 fairness
  - 부분 완료: `Cancel(task)` / `IsCancelled()` cooperative cancellation
  - 부분 완료: spawned descendant cancellation propagation
  - 현재 제한: movable resource channel의 non-blocking/timeout transfer는 미지원
  - 현재 제한: pressure observation은 가능하지만 bounded policy/backpressure protocol은 아직 미구현
  - 현재 제한: preemptive cancellation, blocked thread task interruption, structured cancellation scope/lattice는 미지원
- [x] **async/await runtime 고도화** — POSIX ucontext + Windows Fiber 기반 coroutine
- [ ] **Windows coroutine 검증/고정**

### 툴링 / 표준면
- [ ] **stable stdlib surface 재고정**
- [ ] **툴링 단계 진입** — formatter, LSP 진단 품질
- [x] **ontology-first scaffold 정렬**
  - 완료: `pgy scaffold` help를 `subject/class/object/tobject` 우선 분기로 정렬
  - 완료: `class` scaffold kind 추가
  - 완료: `project/simulator` scaffold가 `subject`가 `class`를 소유하고 `object/tobject`로 투영하는 starter shape를 생성
  - 완료: `project` scaffold가 intent-first layout(`intents/`, `subjects/`, `zones/`, `world.pgy`, `main.pgy`)을 실제로 생성
  - 완료: `pgy new`가 `subject-first` / `class-first` / `projection-first` starter를 선택하게 할지 검토
  - 완료: `pgy new` / scaffold output에 ontology decision guide file 별도 생성 검토
  - 완료: intent-first project guide 문서도 scaffold output에 같이 생성할지 검토
    - `intents/`를 프로젝트 table-of-contents로 설명하는 guide 포함
    - intent declaration이 필요한 subject/zone/ability/effect TODO를 역산하는 workflow 예시 포함
  - 완료: intent runtime follow-up
    - rollback policy를 current reverse-order `compensate` beyond v1로 확장하기
    - intent의 cross-world transfer / identity handoff semantics 설계 및 구현
    - current last-intent typed history를 trace id / stream / multi-instance observability로 확장하기

### 대표 프로그램
- [ ] **대표 애플리케이션 3종** — 이종 자원 파이프라인, secure+device+channel, slot/orchestration 철학 증명

## P1.85 — 게임 프레임워크 계층

- [ ] **게임 프레임워크 라이브러리 경계 고정**
  - 원칙: `entity/object pool`은 언어 코어 기능이 아니라 `use pool;` 같은 게임/앱 라이브러리 계층으로 둔다
  - 원칙: `encounter/turn/state machine`, `strategy/AI`, `content tables`도 동일하게 코어 문법이 아니라 프레임워크 surface로 쌓는다
  - 원칙: 이 계층은 “도메인 라이브러리”보다 “generic pattern library + domain injection”으로 정의한다
  - 이유: 코어 언어는 `subject / vessel / object / tobject / relation / effect / zone / world / Slot<T>` 의미론을 유지하고, 대규모 게임 설계는 그 위의 library/DSL 계층으로 올리는 편이 확장성과 설명력이 더 좋다
  - 목표: “게임을 만들 수 있는 코어 언어”와 “게임을 실제로 만드는 프레임워크”를 분리
- [ ] **게임 stdlib/use surface 초안**
  - 후보: `use pool;`, `use fsm;`, `use encounter;`, `use strategy;`, `use tables;`
  - 방향: pool/fsm/strategy/table은 `.pgy` 또는 stdlib 모듈로 제공하고, 언어 키워드로 승격하지 않는다
  - 방향: `Pool<T>`, `StateMachine<TState, TEvent>`, `StrategyTable<TContext, TChoice>`, `WeightedTable<T>`처럼 generic-first naming을 우선한다
  - 방향: GOF 기초 패턴도 inheritance/object graph가 아니라 Pergyra host 기준으로 번역한다
    - `singleton` -> contextual runtime registry / host-local shared state
    - `factory` -> staged template/spec builder
    - `strategy` -> policy card / policy table + function injection
    - `state` -> explicit FSM / transition rule + context application
    - `observer` -> relay bundle / sink spec / report sink / event bus
  - 방향: generic pattern library는 static spec/table만이 아니라 function-typed picker/resolver 주입도 기본 표면으로 포함한다
    - 예: `Picker<TInput, TChoice>`
    - 예: `Resolver<TContext, TResult>`
    - 예: `StrategyApply(context, AggressivePolicy)`
  - 현재 상태: `data/card/table` 경로는 안정, custom function injection도 V1 표면이 올라옴
  - 현재 전략 패턴의 안정 단계:
    - `StrategyCard`
    - `StrategyContext`
    - `ApplyStrategy(card, context)`
  - 이번 예제 기준 라이브러리화 후보:
    - `use strategy;`
      - `WeaponCard` / `CombatStrategyCard`
      - `WeaponFactory<TClass>` 또는 `LoadoutTable<TArchetype>`
      - `StrategyTable<TContext, TChoice>`
      - `ActionTextFactory<TContext>` / `EffectTextFactory<TContext>`
    - `use tables;`
      - `SceneChoiceCard`
      - `CompanionEventCard`
      - `BossPhaseCard`
      - `WeightedTable<T>`
      - `ChoiceTable<TState, TOption>`
    - `use encounter;`
      - `EncounterStateMachine<TState, TEvent>`
      - `TurnLoop<TActor, TAction>`
      - `BossPhaseMachine<TPhase>`
      - `ResolutionLedger<TSnapshot>`
    - `use report;`
      - transcript accumulator
      - exact report writer
      - stdout/results dual sink
    - `use campaign;`
      - scripted / random / player mode runner
      - input script playback
      - seeded choice resolver
- [ ] **GOF 기초 패턴을 Pergyra식 pattern catalog로 정리**
  - 기준 문서: `docs/31_gof_pattern_catalog.md`
  - 기준 예제: `examples/pattern_library_basics/`
  - 목표: 전통 OOP 패턴 이름을 유지하더라도 실제 구현 shape는 `subject / vessel / shared / spec / card / relay`로 재정의
  - 비목표: inheritance / `super` / hidden callback graph를 패턴 구현의 기본값으로 채택하지 않음
- [ ] **DND/campaign 시나리오를 게임 프레임워크 검증장으로 사용**
  - `dnd_tavern_campaign`를 기준으로 pool/fsm/strategy/table이 실제로 충분한지 검증
  - language core 부족이 아니라 framework layer 부족인지 계속 분리해서 기록
  - 지금까지 뽑힌 실제 패턴:
    - 장소/장면 진입 팩토리 (`OpenTavernCampaign`)
    - 게임 상태 머신 (`tavern -> floor1 -> floor2 -> floor3 -> dragon -> epilogue`)
    - 선택 해석기 (`scripted` / `random` / `player`)
    - 장면 카드 / 동료 반응 카드 / 보스 페이즈 카드
    - 전투 loadout/strategy 카드
    - transcript-first report writer
  - 다음 목표:
    - 위 패턴들을 `examples/` 전용 코드가 아니라 `use` 라이브러리 후보로 재구성
    - `world.pgy`의 orchestration 양을 줄이고 encounter/strategy/report 계층으로 분리

## P1.8 — 멀티 타겟

- [ ] **공통 UI IR 고정** — Kotlin/Android 개별 백엔드보다 먼저, 모든 플랫폼이 공유하는 scene/projection UI IR을 정의
  - 목적: native / web / mobile이 같은 UI 의미론과 projection 흐름을 공유하게 함
  - 원칙: 기술 기반은 Qt 방향(native shell / render loop), 선언 철학은 WPF식 projection/binding, 최종 정체성은 Pergyra scene/projection UI
  - 범위: `Window`, `Scene`, `Node`, `Layout`, `DrawCommand`, `InputEvent`, `ProjectionBinding`, `DirtyScope`
  - 원칙: `subject`를 직접 화면에 그리지 않고 `object` / `tobject` / projection surface를 UI 소비 표면으로 사용
  - 원칙: `zone` / `world` state와 projection dirty sync가 UI IR의 갱신 계약이 됨
  - 순서: UI IR 고정 → native backend 1개 → JS/web backend 1개 → 그 뒤 mobile shell / Kotlin 필요성 재평가
  - 비목표: 플랫폼별 UI 의미론(Qt widget tree, WPF object model, Android View/Compose semantics)을 코어 언어에 직접 들이지 않음
- [~] **JavaScript 백엔드** — `.pgy → JS` 변환으로 브라우저/Node.js 실행 지원
  - 완료: 코어 의미론은 inheritance/super 없이 유지하고, JS lowering은 delegation/composition 중심으로 간다는 정책 초안 문서화
  - 완료: Kotlin backend보다 공통 UI IR이 우선이라는 멀티플랫폼 정책 문서화
  - 남음: JS IR/lowering shape, runtime shim, interop surface (`extern js`) 설계
- [ ] **mobile shell 전략** — Android/iOS는 우선 공통 UI IR consumer로 접근
  - 원칙: 초기 mobile 대응은 JS/web-compatible UI backend 또는 native shell bridge를 우선 검토
  - 남음: Android 전용 Kotlin backend는 공통 UI IR + web/native backend 검증 뒤 필요성을 재평가
- [ ] **WebAssembly 타겟** — LLVM wasm32 backend 활용

## P2 — 배포 시작 시

- [ ] **문서-구현 동기화** — 테스트 수/기능 범위 일치
- [ ] **SBOM (SPDX) + provenance (SLSA)** — 공급망 투명성
- [ ] **릴리스 아티팩트** — 서명된 바이너리, 체크섬, 설치 스크립트
- [ ] **3rd-party NOTICE** — OpenSSL/LLVM/pthread 라이선스 정리

## IR 파이프라인 재구성

- [x] **컴파일러 계약 고정** — `HIR/DIR/RIR/MIR`, resource lattice, intent compensation, projection sync, authority/capability를 `docs/37_compiler_contracts.md`에 고정

- [~] **DIR (Domain IR)** — declaration graph / intent step graph 시작
  - 완료: `src/compiler/dir.h`, `src/compiler/dir.c`, `pgy --dir`, `test-dir`
  - 완료: intent participant/type edge, step zone/ability/authority/effect edge, step predecessor dependency
  - 완료: role/ability completeness edge, missing-ability-method edge
  - 남음: richer zone/world membership graph
- [~] **RIR (Resource IR)** — slot/resource/authority/lifecycle 의미론 전용 계층
  - 범위: `Slot`, `SecureSlot`, `DeviceSlot`, projection validity, authority, effect/relation lifecycle, intent compensation resource edge
  - 완료: `src/compiler/rir.h`, `src/compiler/rir.c`, `pgy --rir`, `test-rir`
  - 완료: scope별 normalized state summary (`initial_state`, `final_state`, `last_op`, `transition error`)
  - 완료: relation/effect layer slot와 world zone slot도 resource fact로 materialize
  - 출력: 단순 map이 아니라 `Resource Graph + Transfer Ops + Static Ownership Facts`
  - explicit op 정규화:
    - `Claim/Read/Write/Release`
    - `Move/BorrowRead/BorrowWrite`
    - `ProjectRefresh/ProjectPublish`
    - `AttachEffect/DetachEffect`
    - `LinkRelation/UnlinkRelation`
    - `Authorize/AwaitRemote`
    - `CommitIntent/AbortIntent/CompensateIntentStep`
  - state lattice 초안:
    - `Uninit`
    - `Owned`
    - `BorrowedRead`
    - `BorrowedWrite`
    - `Moved`
    - `Released`
    - `Invalid`
    - `Measured`
    - `RemotePending`
  - CFG 의존 branch/join/loop/phi merge는 MIR로 이월
- [~] **MIR (Machine / Execution IR)** — CFG/SSA/liveness/optimization 계층
  - 범위: basic block, explicit instruction, phi, liveness, CFG-dependent resource merge, dead code elimination
  - 완료: `src/compiler/mir.h`, `src/compiler/mir.c`, `pgy --mir`, `test-mir`
  - 완료: HIR CFG -> MIR block bridge
  - 완료: RIR op -> MIR instruction bridge
  - 완료: intent cleanup block skeleton
  - 완료: phi materialization + incoming predecessor value list
  - 완료: block-local SSA rename skeleton
  - 완료: intent cleanup successor edge skeleton
  - 필요: `RIR-flow` merge 정책
  - 필요: richer phi merge policy
  - 필요: cleanup / rollback / detach-invalidation edge 고도화
