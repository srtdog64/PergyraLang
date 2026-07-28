# Pergyra — 현재 진행 상황

마지막 업데이트: 2026-07-28

## 2026-07-28 callable receiver carriage executable checkpoint

- `CallableReceiverCarriage`를 `none | value | mutable-identity`의 필수
  callable fact로 고정했다. Native MIR와 self MIR producer가 routine
  `source_syntax_id` 및 exact declaration owner에 결속해 JSON으로 운반하고,
  self MIR admission은 누락·unknown·중복 ID·foreign owner·nominal/carriage
  불일치를 모두 output 생성 전에 거부한다.
- `tobject`/`object`/`class` method는 value receiver, `subject`/`vessel`/`party`/
  `roster`/`zone`/`world`/`effect`/`relation`/`role` method는 mutable identity
  receiver, free function과 intent는 `none`이다. `ability`는 compile-time
  contract이므로 runtime method receiver로 승격하지 않는다.
- Production `zone_layer_projection_runtime`의 canonical row는
  `27 | method | BattleZone | Show | mutable-identity`와
  `35 | function | Main | none`이다. General self C는 이를
  `BattleZone_Show(BattleZone *self)` 및 `BattleZone_Show(&(battle))`로
  방출한다. `value` 변조와 semantic place fact가 addressable하지 않은 임시
  mutable receiver는 C output 전에 실패한다.
- 이 slice는 self MIR -> general C의 실제 by-value zone receiver 경로를
  대체하므로 `SUBSTITUTING`이다. 다만 native C/LLVM의 일반 parameter ABI는
  여전히 넓은 `uses_pointer_self` compatibility policy를 재사용하므로
  `semantic.callable_receiver_carriage` 전체는 `BRIDGE`로 남긴다.
- Role-erased local ABI도 concrete mutable target을 `_raw_self`에서 `T *self`로
  보존하고 stable direct receiver 주소만 받도록 고정했다. 다만 production
  direct role call은 native semantic method resolution 부재와 role method
  canonical ID `13` 대 `6` 불일치로 unreachable이다. Role body의
  `return self.health`도 self semantic statement-type fact가 unresolved라
  fail closed하므로 이 local gate를 `SUBSTITUTING`으로 세지 않는다.
- Runtime `7`/`dst`는 아직 RED다. 다음 활성 owner seam은 projection member
  assignment와 effect/relation destination role이며, layer materialization과
  refresh/publish sync까지 같은 exact identity epoch에서 닫혀야 한다.

## 2026-07-28 domain runtime assignment boundary audit

- `tobject -> object -> vessel -> subject -> action`은 nominal 승격 계층이
  아니라 detached transfer, local observation, subject-owned state, stable
  identity, observable transition이라는 서로 다른 경계 프로토콜이다.
  `effect`/`relation`/`zone`까지 내려가면 같은 규칙이 explicit destination
  role, exact member assignment, callable receiver carriage, lifecycle operation,
  materialization/sync fact로 반복된다.
- Native semantic은 implicit same-name projection을 합법적인 편의로 선택하지만
  exact member ID/type/path를 저장하지 않는다. Native MIR header도 explicit map을
  문자열 pair로만 들고 JSON wire에는 내보내지 않는다. C/LLVM은 이를 다시
  same-name으로 해석하고, C는 missing source를 `.field = 0`으로 숨기는 반면
  LLVM은 NULL/error로 실패해 두 backend의 실패 의미도 다르다.
- Effect declaration에는 explicit bearer destination role이 없고 relation의
  source/target destination도 ordered generic slot뿐이다. Native C/LLVM binder는
  각각 첫 slot과 0/1번째 slot을 선택한다. `by participant`는 transition
  initiator/provenance이며 이 destination role을 대신하지 않는다.
- Receiver carriage도 별도 owner가 아니다. Native in-memory MIR의
  `uses_pointer_self`는 JSON wire에서 사라지고 receiver와 일반 parameter ABI를
  섞는다. Self general C는 zone method를 by-value로 방출할 수 있다. 따라서
  receiver는 DIR topology가 아니라 callable ABI가 callable ID별로 소유해야 한다.
- 다음 owner family는 `DomainParticipantRoleFact`,
  `DomainProjectionMemberAssignment`, `DomainLifecycleOperation`,
  `DomainLayerMaterialization`, `CallableReceiverCarriage`로 분리한다.
  `VerifiedDomainRuntimePlan`은 이들을 exact join한 admission receipt일 뿐 원본
  의미의 새 owner가 아니다.
- Public compact AST에 `ProjectionMap:` 행만 추가하는 임시 patch는 채택하지
  않았다. Native AST parity의 source identity를 바꾸고 MIR consumer에서 다시
  유실되기 때문이다. Explicit/implicit mapping은 semantic fact에서 시작해
  canonical identity epoch과 함께 MIR JSON을 lossless하게 지나야 한다.
- 다음 executable falsifier는 그대로 self MIR -> C의 `7`/`dst`다. Member ID/type,
  bearer role, relation destination role, receiver mode, materialization/sync op 중
  하나를 바꾼 artifact가 admission 전에 실패하고 같은 target-neutral plan을
  C/LLVM이 소비해야 한다. 이번 감사·문서 갱신은 supporting slice이며 새
  `SUBSTITUTING` 진척으로 세지 않는다.

## 2026-07-28 self-host non-empty topology executable checkpoint

- `zone_layer_projection_runtime`의 production DRV-2 source 경로가 이제 native
  topology JSON을 빌리지 않고 self source -> typed AST/DIR -> MIR로 non-empty
  topology를 생산한다. Exact graph ID는 `14937235025281185444`이고 row는
  `Poisoned.refresh`, `TrustedLink.publish`, `BattleZone.apply-effect`,
  `BattleZone.link-relation` 네 개다. Apply는 maintain과 별도 lifecycle identity며
  dependency graph에는 edge를 추가하지 않는다.
- Native `apply stateAlias`도 semantic owner가 exact effect/target slot로
  정규화한 뒤 같은 `apply-effect` row를 생산하며, unresolved apply는 DIR에서
  누락되지 않고 실패한다. Production self source parser의 state-alias carriage는
  아직 열려 있어 직접형만 현재 self substitution 범위에 포함한다.
- 각 row는 같은 canonical identity epoch에서 declaration의
  `(owner, field name, source_syntax_id, field_kind)`에 exact join한다. 과거 raw
  field ID, `player` 이름 + canonical `enemy` ID, refresh의 tobject slot,
  publish의 object slot, caller가 layer storage를 세 번째 zone constructor
  인자로 넘기는 경우는 모두 fail closed다.
- 이 좁은 non-empty DIR/MIR producer는 기존 C-owned producer 결정을 실제로
  대체하므로 `SUBSTITUTING`이다. Machine admission은 같은 MIR에서 ID-keyed
  target-neutral plan을 한 번 만들고 한 번만 전체 검증한다. 이후 C/LLVM은
  graph identity/digest/cardinality receipt만 소비하며 plan 전체를 재검증하지
  않는다. 이 plan 소비 단계는 현재 `REACHABLE`이다.
- `BattleZone` plan은 exact `nodes=3`, `edges=2`, `depth=2`, `pass_limit=2`와
  `trust <- player`, `trust <- enemy`를 C/LLVM 모두에 투영한다. Forged edge와
  gate-only digest mutation은 artifact 생성 전에 거부된다.
- 실제 zone runtime은 아직 RED다. Apply row는 운반되지만 effect bearer/relation
  source·target destination role, projection member map, receiver carriage,
  `.poison`/`.trust` storage materialization 및 refresh/publish value sync owner가
  없다. 따라서 same-name/ordinal 추론, generic zero-fill이나 native graft로
  `7`/`dst`를 꾸미지 않는다. 이 owner chain과 실행 결과가 다음 substitution
  rung이다.
- Fresh pressure-owned self-host compiler build는 2,138,300 ms에 설치/smoke까지
  green이었다. Peak working set 1,038.0 MiB, private 1,132.4 MiB,
  top process `gen2.exe` 1,119.4 MiB로 3,072 MiB cap 이하다. 20GB 재발은 없지만
  35분 fresh bootstrap 시간은 별도 최적화 부채다.

## 2026-07-28 declaration field exact-identity checkpoint

- Native와 self-host `pgy.mir.v1`의 topology-addressable
  `decls[].fields[]` row가 이제 nonzero `source_syntax_id`와 `field_kind`를 함께
  운반한다. Native MIR validator와
  self-host declaration index는 topology reference를 같은 owner의
  `(field name, source_syntax_id, field_kind)`에 exact join한다.
- Self-host는 `declarations[]`를 한 번만 index하고 field identity를 그 index의
  하위 fact로 소유한다. Topology edge마다 declaration JSON을 재탐색하던 name-only
  lookup은 삭제됐다. Missing/zero/duplicate field ID, duplicate owner/name,
  `player` 이름에 유효한 `enemy` ID를 붙인 row, field-kind drift가 fail closed다.
- Native parser ID와 self-host compact typed-arena ID는 서로 다른 producer/revision
  epoch이므로 raw 숫자 equality를 요구하지 않는다. Offset 보정도 금지한다.
  Canonicalization은 새 declaration/topology ID를 함께 remap해야 하며, 이 owner가
  생기기 전 non-empty topology는 계속 명시적으로 거부한다.
- 관측된 focused hard DRV-2 gate는 `function_clause_order_minimal` self-produced MIR,
  canonical reconstruction, emitted C까지 green이다. Native MIR unit은 exact
  field-kind mutation까지 거부하도록 강화됐다. 이 변경은 다음 executable
  non-empty graph rung의 supporting seam이며 독립 `SUBSTITUTING` 진척은 아니다.
- 다음 falsifier는 `zone_layer_projection_runtime` canonical 문서의 declaration과
  topology ID를 같은 epoch으로 재발급한 뒤, row 하나만 과거 raw ID로 되돌린
  mutation과 `player` 이름 + canonical `enemy` ID mutation을 거부하는 것이다.
  그다음 한 ID-keyed graph plan이 C/LLVM의 exact 3-node/2-edge 실행을 소유한다.

## 2026-07-28 self-host empty DIR graph executable checkpoint

- Self-host production MIR가 `function_clause_order_minimal`의 DIR graph
  census를 직접 소유한다. Typed `Authority`와 declaration/role/ability/slot
  facts를 한 번 join해 native와 같은 `nodes=9`, `edges=16`,
  `domain_graph_id=14937235029576152731`을 계산하고 empty topology row를
  방출한다.
- `Refresh`/`Publish`/projection `Bind`/`Maintain`/`Link`/`Apply`/`Detach`/
  `Unlink`/`State`는 서로 다른 typed kind다. 현재 bounded owner는 이 중
  하나라도 있으면 empty로 낮추지 않고 fail closed한다.
- Focused hard DRV-2 gate에서 self-produced MIR, canonical native/self MIR,
  MIR consumer, emitted C compile/run이 green이며 결과는
  `clause-order-minimal`이다. 이 empty-topology producer slice만
  `SUBSTITUTING`; non-empty graph plan/runtime과 전체 `dir.domain_graph`는
  계속 `BRIDGE`다.
- Canonical bridge는 MIR 문서를 한 번만 admit하고 이미 admit된 empty
  topology를 운반한다. authority가 빠지는 MIR-to-AST projection에서 graph를
  재계산하거나 같은 문서 graph를 두 번 검증하지 않는다.
- 다음 rung은 declaration/field `source_syntax_id` exact join과 non-empty
  typed directive row, ID-keyed target-neutral graph plan이다. `player` 이름에
  `enemy` ID를 붙인 row가 첫 falsifier다.

## 2026-07-28 object-to-action boundary audit

- Canonical 구현 단위는 keyword 하나가 아니라 `NominalKind`, `FieldRole`,
  `ReceiverCarriage`, `ParameterCarriage`, `CallableKind`, `ActionContract`,
  `CallAuthorityBinding`, `RuntimeAuthorityEvidence`의 직교 fact다.
- Production import closure 450개에서 object 18개는 import만 되고 실제 생성/소비가
  없다. artifact receipt는 payload까지 소비되지만 failure payload는 버려지며,
  subject/action 17쌍 중 production 호출은 direct-MIR 한 쌍뿐이다.
- C/LLVM은 vessel hosted receiver의 `uses_pointer_self`를 일반 vessel 파라미터에도
  재사용해 canonical value carriage와 달리 caller 원본을 바꾼다. object/tobject
  bare-field write와 class/subject immutable bare-field write도 shallow semantic
  검사 밖으로 빠진다. 이것들은 언어 규칙이 아니라 executable negative가 필요한
  현재 결함이다.
- 세부 authoring 규칙, 실제 반례, 폐쇄 순서는
  `docs/200_object_to_action_boundary_patterns.md`가 소유한다. 선언/import 수는
  `IMPORTED -> MATERIALIZED -> INVOKED -> OUTCOME_CONSUMED -> SUBSTITUTING`
  사다리와 분리하며 마지막 단계만 hard self-host 진척으로 센다.

## 2026-07-28 MIR JSON topology admission checkpoint

- Native `pgy.mir.v1`은 이제 `relation` declaration과 optional
  `domain_topology` object를 운반한다. Domain row는 graph, owner, directive,
  participant/layer/endpoint slot의 stable identity를 flat 18-field wire로
  보존하며, domain declaration이 없는 scalar 문서의 기존 5-field root는
  그대로 유지된다.
- Self-host `mir_lower`는 이 object를 한 번 index한 뒤 typed
  `MirDomainTopologyFacts`로 admit한다. name/ID null pair, known row kind,
  directive identity uniqueness, declaration field-kind join, relation의 정확한
  두 subject endpoint를 fail closed로 검사한다. AST/source 복구는 없다. 다만
  declaration field JSON에 `source_syntax_id`가 없어 field name과 ID가 실제 같은
  field인지 증명하는 join은 아직 없다.
- `TrustedLink`도 self-host typed declaration과 canonical AST text로 복원된다.
  topology 누락, relation owner 누락, unknown kind, duplicate directive,
  missing/stray slot identity, kind drift negative가 backend output 전에 실패한다.
- 이 checkpoint의 새 증거는 `REACHABLE`이다. Native C/LLVM zone frontier의
  기존 `SUBSTITUTING` 경계는 유지되지만, self-host graph plan/runtime consumer는
  아직 이 carrier를 실행하지 않는다. 따라서 전체 `DomainRuntimeTopology`는
  계속 `BRIDGE`다.
- 관측된 gate는 MIR 155/0, `domain_runtime_topology_smoke.sh`,
  `domain_topology_admission_owner.sh`, `object_action_boundary_contract_smoke.sh`,
  self-host `mir_lower` source compile 및 positive relation reconstruction이다.
- 기존 self-host MIR producer는 domain declaration을 만들면서 아직
  `domain_topology` 부재/empty를 증명해 emit하지 못한다. focused
  `function_clause_order_minimal` DRV-2 producer gate는 새 admission 경계에서
  의도적으로 RED다. 이를 optional fallback으로 숨기지 않고 다음 executable
  rung에서 producer-side typed topology owner와 graph plan으로 닫는다.
- 다음 rung은 정확히 `BLOCKED`다. 빠진 사실은 declaration-field name/ID join,
  self-host producer-owned typed topology, Pergyra `MirDomainTopologyGraphPlan`,
  그리고 fixture의 apply/state-count/hidden-layout/sync-operation fact다. 먼저
  `player` 이름에 `enemy` ID를 붙인 forged row를 거부하고, 그 다음
  `zone_layer_projection_runtime`의 exact 3-node/2-edge trace와 mutation 결과를
  일반 DRV-2 C production path가 한 plan에서 소비해야 한다. 이 다음에는 다른
  SoT-only commit을 두지 않는다.

## 2026-07-28 DIR-owned zone frontier topology executable checkpoint

- 실행 경계 `c66e22ca6dd34b50ff2a7a3a8e183852943d3a9a`에서
  `dir.domain_graph`가 projection refresh/publish/bind, maintained effect,
  relation link row를 stable owner/directive/slot `SyntaxNodeId`와 함께 소유한다.
  MIR은 이 사실을 복사해 운반할 뿐 새 owner가 아니다.
- Production MIR lowering은 DIR을 명시적으로 bind하며 HIR과 다른 source-program
  identity의 DIR, DIR 누락, 손상된 slot identity, 존재하지 않는 topology owner를
  backend 전에 거부한다. 같은 검증 graph를 backend마다 다시 만들지 않는다.
- C와 LLVM의 zone frontier pass-limit 경로는 이제 MIR carrier만 소비한다. 기존
  `propagation_graph_build_from_zone(ASTNode *)`와
  `pgy_codegen_zone_frontier_graph_pass_limit(ASTNode *)` entrypoint는 삭제됐다.
  따라서 이 좁은 native frontier slice는 실제 C-owned AST read를 대체한
  `SUBSTITUTING` 진척이다.
- `zone_layer_projection_runtime`의 양 backend trace는 정확히
  `nodes=3, edges=2, depth=2, graph pass_limit=2`와
  `trust <- player`, `trust <- enemy`다. 생성 loop limit은 count floor 때문에 3이며,
  trace gate가 없으면 빈 graph도 stdout parity 뒤에 숨을 수 있다.
- 관측된 gate는 isolated LLVM build, DIR 15/0, MIR 155/0,
  `domain_runtime_topology_smoke.sh`, focused C/LLVM backend compare가 green이다.
  현재 broad `test-transpile`은 이 domain test에 도달하기 전 기존 expression
  `identifier -> same name`에서 null 결과를 `strcmp`해 SIGSEGV가 나는 RED이며,
  이 checkpoint의 green으로 기록하지 않는다.
- 전체 `DomainRuntimeTopology`는 계속 `BRIDGE`다. Apply/detach/unlink, pool capacity,
  authority/state/lifecycle/action transition과 self-host graph/runtime consumer가
  남아 있다. MIR JSON relation/topology carriage와 typed admission은 위의 최신
  checkpoint에서 `REACHABLE`로 닫혔다.

## 2026-07-28 nominal field-kind bridge checkpoint

- `AST_EFFECT_DECL -> pgy.mir.v1 -> self-host mir_lower -> C`가 explicit
  `effect/effect` identity로 연결됐고, `causes Damage`는 실제 effect declaration을
  요구한다. `function_clause_order_minimal` focused C shard는 native/self MIR,
  canonical reconstruction, emitted C compile/run까지 green이다.
- `mir_decl_field_kind_vocabulary.def`가 일반/shared field와 domain/zone/world/roster
  slot 14개의 wire spelling/AST label을 소유하며 self-host projection은 생성된다.
  `Damage.bearer=subject_slot`, `BattleZone.damage=effect_slot`과 effect participant
  cardinality의 누락/평탄화 변조는 backend output 전에 실패한다.
- 이 상태는 `BRIDGE`/`SURFACE`다. Stable field identity, pool capacity,
  vessel/binding slot, relation declaration, zone refresh/authority/state/lifecycle,
  runtime C/LLVM topology가 열려 있다. 다음 executable fixture는
  `zone_layer_projection_runtime`이며 production call graph는 아직 바뀌지 않았다.
- Hard-substitution accounting은 `BLOCKED`로 기록한다. 정확한 missing fact는
  `dir.domain_graph`가 소유해야 할 typed `DomainRuntimeTopology`(stable field/layer
  identity, relation endpoints, pool capacity, refresh/authority/state/lifecycle,
  action transition binding)다. 현재 native carrier는 `MIRDeclHeader`이고 마지막
  합법 consumer는 target-neutral topology plan을 거쳐야 할 self-host C/LLVM
  runtime emitter다. 금지된 직접 우회는 backend의 AST/source topology 재조회이며,
  다음 falsifying fixture는 `zone_layer_projection_runtime`이다. 이 사실이 없어서
  현재 commit은 supporting SoT seam이지 executable C-path substitution이 아니다.
- zero-explicit-parameter role impl도 implicit `self` C ABI를 보존한다. focused
  emitted-C gate가 receiver-free duplicate signature 재도입을 거부한다.

## 2026-07-27 self-host closure checkpoint

- Production direct-MIR entrypoint reaches one real
  `PgyCompilerWorld -> zone -> subject.action` slice. This is `REACHABLE`, not
  yet `SUBSTITUTING`; source-mode `Main -> CompileSourceTo*` still bypasses it.
- ActionContract declaration carriage is `CLOSED`: callable identity and
  requires/within/causes/authorized/caps/effects survive typed AST, semantic,
  native/self MIR, `mir_lower`, and C/LLVM validation.
- The same focused source now preserves two `impl ability` partitions instead
  of dropping every declaration when a role owns more than one impl. Zone
  `effect slot` and `relation slot` rows also enter the nominal field fact.
  This was the historical predecessor of the 2026-07-28 explicit effect and
  field-kind bridge above; use the newer checkpoint for continuation.
- `semantic.callable_contract_vocabulary` owns the 9 capability and 9 effect
  closed values. Native, self-host, MIR, diagnostic, manifest, and runtime
  grant consumers use one direct/generated projection. Duplicate,
  noncanonical, unknown, and `local + nonlocal` contracts fail closed.
- The prior array-only DRV-2 emitted-C header defect is fixed at the
  runtime-header owner: `uses_array` selects `<string.h>` and the narrow panic
  contract. Full unfiltered DRV-2 remains an integration-boundary rerun.
- The historical multi-GiB incident was repeated whole-graph readiness inside
  per-local loops. The hot loop now consumes a once-validated artifact. The
  3 GiB cap remains mandatory; later compiler-scale stages still carry
  measurable optimization debt.

Exact revision, dirty state, last green gate, and next falsifier live in
`docs/current_work_handoff.md`. The sections below are a broad capability
inventory and older test snapshot, not the resume authority.

## 컴파일러 파이프라인

```text
.pgy → Lexer → Parser → Semantic → HIR → DIR → RIR → MIR → Backend
                                                        ├→ LLVM → Object → Binary
                                                        └→ C    → C → GCC/Clang
```

- LLVM이 기본 백엔드
- C 백엔드는 폴백/reference 경로

## 현재 구현 요약

### 문법/시맨틱
- `let`, `func`, `async`, `spawn/await`, `if/for/while/match/select`
- `slot/view/move`, `SecureSlot`, `DeviceSlot`, `QubitSlot`
- `ability/role/party/relation/effect/zone/roster/world`, `event`, `subject`
- 장기 의미론은 `struct` / `class` / `subject` 분리를 채택했고, 현재 surface도 parser/semantic/codegen에서 이 nominal flavor를 구분한다
- `import/export/namespace`, `extern "C"`
- `RemoteFuture<T>`의 `await` 결과는 `Result<T>`
- enum/result shorthand `.Some(x)`, `.None`, `.Ok(v)`, `.Err(e)` 파싱 지원

### 백엔드/런타임
- C/LLVM 백엔드 둘 다 동작
- coroutine runtime (POSIX ucontext + Windows Fiber)
- channel/parallel/select 동작
- Result/enum/array/string built-in 경로 동작

### 테스트
최근 직접 확인 기준:
- `make test-transpile` 통과 (`464 passed`)
- `make test-abi` 통과 (`56 passed`)

추가 회귀:
- `llvm-test-backend-compare` 통과
- `example-test-smoke` 통과
- `ir-pipeline-test-smoke` 통과
- `fmt-test-smoke` 통과

## 미완성 / 다음 단계

- orchestration 고도화 (select 공정성, timeout, cancellation)
- effect system 2단계 (선언적 effect + mismatch 진단)
- stable stdlib surface 고정
- 패키지 매니저 / WebAssembly / product-grade debugger/LSP
