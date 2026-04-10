# Pergyra 개발 현황

마지막 업데이트: 2026-04-10

## 요약

- 컴파일러의 실제 driver 경로는 이제 `Lexer → Parser → Semantic → HIR → DIR → RIR → MIR → Backend dispatch`로 고정되며, `driver_run_pipeline()`은 backend 진입 전에 `DIR/RIR/MIR` lowering과 structural validation을 항상 수행한다.
- `DIR`, `RIR`, `MIR` 코드 계층은 각각 `--dir`, `--rir`, `--mir`로 declaration/resource/execution dump를 제공하고, backend runner는 `CompilerIRBundle`을 입력으로 받는다. 현재 실제 codegen은 `MIR` 주도 + `HIR` 보조 하이브리드로 움직이며, C/LLVM 두 backend 모두 MIR entrypoint를 받는다. 다만 LLVM backend에는 아직 domain/intent/main-wrapper 쪽 HIR fallback이 남아 있다.
- `HIR/DIR/RIR/MIR`, resource lattice, intent compensation, projection sync, authority/capability의 고정 계약은 `docs/37_compiler_contracts.md`에 정리함.
- 최근 ABI/성능/AlphaDev식 invariant 최적화 진행 상태는 `docs/49_invariant_optimization_progress.md`에 따로 추적한다.
- 아직 부분 구현 상태인 핵심 언어 축(effect lattice, capability security, MIR->LLVM, debugger/formatter/LSP, stack-slot escape analysis, generic where validation)은 `docs/50_language_completion_board.md`에서 추적한다.
- C backend의 공식 역할 재정의는 `docs/51_c_backend_reference_policy.md`에 정리한다.
- LLVM/native-first 전환 단계는 `docs/52_llvm_native_first_roadmap.md`에서 추적한다.
- LLVM backend의 현재 검증 범위와 실제 debt는 `docs/62_llvm_backend_debt_ledger.md`에 따로 정리한다.
- `parallel`을 코어 실행 primitive로 재정의한 정책은 `docs/53_parallel_core_policy.md`에 정리한다.
- `spawn/select/async`를 `parallel` 아래 실행 family로 재배치하는 작업 보드는 `docs/54_parallel_execution_relayout_board.md`에서 추적한다.
- 전체 언어 키워드의 현재 완성도와 공백은 `docs/55_keyword_progress_board.md`에서 추적한다.
- runtime 파일 I/O 보안 정책은 이제 기본적으로 `상대경로만 허용 + parent traversal 금지 + 절대경로 기본 거부`로 잠겨 있다. `PGY_IO_ROOT`가 있으면 runtime `ReadFile/WriteFile`는 해당 root 아래로 고정되고, non-Windows에서는 canonical path 검사로 symlink escape도 차단한다. `PGY_IO_ALLOW_ABSOLUTE=1`일 때만 절대경로를 허용한다.
- whole-file read 경로는 compiler/runtime 공통으로 `64 MiB` 상한, `fseek/ftell/fread` 실패 방어, short read 거부를 가진다.
- hardware fingerprint는 이제 probe 실패 시 즉시 붕괴하지 않고 stable fallback identity를 채운다. Linux는 `machine-id/hostname/uname/non-loopback MAC`, Windows는 `computer name` 기반 fallback을 사용한다.
- 보안 회귀는 `make test-security`에서 runtime file I/O policy, fingerprint consistency, secure slot/token 경로를 함께 검증한다.
- `examples/logistics_intent_probe/`는 현재 가장 직접적인 4-layer probe 예제로, `DIR` role/ability edge, `RIR` handle/flow fact, `MIR` phi/cleanup graph, runtime intent history를 한 번에 밟고 `tests/ir_pipeline_probe.sh`로 회귀시킴.
- `examples/resource_scheduler_async_probe/`는 현재 가장 직접적인 async/parallel resource probe 예제로, `Channel<Int>`, `parallel`, `Slot<subject>`, helper-based `ref Slot<subject>` mutation, `DeviceSlot<Int>`, `RemoteFuture<Int>`를 한 시나리오에서 동시에 밟고 exact stdout/results + module smoke로 회귀시킴.
- HIR는 아직 expression-level deep IR은 아니지만, 더 이상 순수 top-level bucket classifier만은 아니며 `decl index` / `routine summary` / `signature_type_refs` / direct-call snapshot / routine call-edge / conservative entry reachability / `hir_run_routine_pass(...)` / `hir_run_block_pass(...)`와 function CFG v0(predecessor/reachability/dead-block-count/immediate-dominator/dominance-frontier/dominator-tree/natural-loop-depth/local-def/phi-candidate/phi-node-skeleton 포함)를 가진 indexed backend/pass view로 올라와 있음
- RIR는 scope-based explicit resource op/fact 계층으로 시작됐고, tracked resource/projection마다 `initial_state` / `final_state` / `last_op` / `transition error`를 가진 normalized state summary를 materialize함. 최근 패스로 HIR CFG를 받아 `flow-block[...]` 단위의 branch/join lattice propagation, join conflict, loop widening 정보를 함께 덤프하며, `relation/effect/zone/world` nominal handle도 function param / intent participant / `using` / `transfer` 경로에서 explicit fact/op로 정규화함. 추가로 resource fact가 없는 CFG block도 `authority/projection/world-handoff/invalidation/authority-loss/projection-invalidation` conservative semantic flag를 유지해서 scope-level `semantics=`와 block-level `sem-entry/sem-exit` 표면에 남기고, authority/capability의 `Authorized/AuthorityLost`, projection의 `Synced/Dirty/Stale/Published`, world handoff의 `HandoffPending/HandedOff`, lifecycle rollback의 `Compensated` 상태를 보수 요약에 포함함
- RIR는 이제 `object slot`도 `ObjectSlot` resource fact/state로 materialize하며, `rir_validate_against_dir()`를 통해 `DIR`의 `zone-slot / projection-slot / authority-slot` 계약이 matching `RIR` scope/fact/state/capability로 실제 내려갔는지 driver 단계에서 교차 검증한다
- DIR는 이제 intent participant/type edge, step zone/ability/authority/effect edge, step predecessor dependency뿐 아니라 `party-slot / zone-slot / projection-slot / authority-slot`를 owner-qualified node로 materialize하는 slot-contract graph까지 직접 가진다
- MIR는 HIR CFG와 RIR op를 묶는 실행 skeleton으로 시작됐고, routine/block/instruction/cleanup-block을 가지며 intent compensation/abort 경로를 cleanup instruction으로 분리함. 최근 패스로 `phi` materialization, instruction-level `def/use`, block entry/exit SSA version map, cleanup convergence root, rollback/invalidation exceptional CFG, routine-level value summary(`def_block`, `def_inst`, `use_count`, `live_in/out block count`, `reaches_cleanup`, `slot_anchor`)까지 들어와 `--mir`가 더 이상 순수 block 껍데기만 덤프하지 않음. resource/cleanup instruction은 matching `RIR` op의 `slot_anchor`를 그대로 보존하고, `def/phi`와 value summary는 base local 이름을 slot anchor로 유지한다. validator도 이제 slot anchor 누락이나 `RIR`와의 slot mismatch를 실패로 본다. 추가로 lowering 경로에서 실제 `liveness` 재계산과 dead `def/phi` 제거 DCE pass가 돌고, C backend에는 branch/return top-level function subset, MIR block 안의 non-SSA statement fallback, intent cleanup/rollback/invalidation CFG subset을 MIR block/terminator에서 직접 emit하는 첫 codegen vertical slice가 들어갔음. intent exceptional CFG에서 MIR cleanup/resource op는 이제 no-op runtime hook 호출로 직접 emit된다.
- LLVM이 기본 백엔드이며, C 백엔드는 reference/bootstrap/debug 경로로 유지됨.
- `driver_run_pipeline_timed(...)`와 `test-abi-perf`가 들어가 phase별 compile timing(`module_load`, `semantic`, `HIR/DIR/RIR/MIR`, `backend`, `total`)을 직접 계측한다. backend는 다시 `codegen / native_compile / link`로 쪼개진다. CI에서는 hard upper bound를, 로컬 benchmark에서는 comparative metrics를 분리한다.
- async/await는 coroutine runtime을 통해 동작하며, channel/select/parallel이 동작함.
- 최종 목표 계층은 `ability -> role -> party -> relation -> effect -> zone -> world`로 문서화됨.
- 최종 존재론은 `struct`와 `subject`를 분리하며, 현재 surface는 `subject`와 `class`를 별도 nominal declaration flavor로 기록하고 semantic/codegen도 점진적으로 분기함.
- `vessel` declaration이 parser/semantic/transpile에 반영됐고, subject는 `vessel name: Type;` 형태의 피동 수용체 필드를 가질 수 있음.
- `subject`는 `action` declaration을 직접 가질 수 있고, `requires` / `within` / `causes` / `authorized by` 최소 clause가 parser/semantic에 연결됨.
- `action` clause는 이제 존재 확인을 넘어서 `authorized by` subject-host 검증, `within` zone subject/authority 적합성 검증, `causes` effect target/zone layer 적합성 검증까지 포함함.
- `intent` declaration이 parser/AST/semantic/HIR/codegen에 반영되어 `intent Name(args...)`, legacy `involves`, `step`, `exclusive`/`concurrent`, `priority`, `where`, `who`, repeated `on`, `pre`, `post`, `requires`, `authorized by`, `causes`, `expect`, `success`, `failure`를 검증하고 executable generated function으로 lowering함
- `intent` clause는 이제 suspension/concurrency control을 직접 담지 않는 것으로 잠갔고, semantic은 `spawn` 같은 async/fiber 제어를 step clause에서 거부한다. `await`는 원래 async context 바깥 parser surface에서 막히며, intent는 orchestration 계약에 머문다.
- `intent` runtime은 이제 same-subject conflict registry를 가져서 `exclusive` 차단, `concurrent` 병행, higher-`priority` nested override까지 수행함. step-level `guard` / `invariant`도 실행되며, reverse-order `compensate` rollback과 `IntentLastTrace()` / `IntentLastFailure()` / `IntentLastName()` / `IntentLastHandle()` / `IntentLastStepCount()` / `IntentLastFailed()` history도 동작하고, `IntentHistoryCount()` / `IntentHistoryStep*()`로 마지막 completed intent의 typed step history도 읽을 수 있으며, `using: zoneAlias;`와 `transfer: source -> target;`를 통해 live zone instance sync, participant-to-zone-slot materialization, cross-zone handoff materialization도 수행함
- hosted `func` / `action` body 안의 bare field access와 bare helper call은 이제 subject/class/relation/effect/zone/world 전반에서 implicit `self`로 해석되며, `self.`는 선택적 표기로 남음
- `object`는 현재 `struct` 호환 passive state-target declaration alias로 동작하며 helper `func`와 국소 상태를 가질 수 있고, `tobject`는 더 좁은 transfer/projection declaration alias로 동작함.
- `ToObject(TargetObject, subjectBinding)` 최소 passive projection surface가 semantic/C/LLVM backend에 반영됨.
- `ToTObject(TargetDto, subjectBinding)` 최소 tobject projection surface가 semantic/C/LLVM backend에 반영됨.
- relation/effect/zone/world 바깥의 direct `ToObject` / `ToTObject`는 여전히 허용되지만, `ToObject`는 internal projection warning, `ToTObject`는 boundary projection warning으로 분리됐다. 권장 경로는 domain-local projection wiring이다.
- `bind`는 target slot kind를 따른다. object slot이면 internal `ProjectRefresh/Synced`, tobject slot이면 boundary `ProjectPublish/Published`로 RIR에 고정된다.
- `entity`는 코어 언어 존재론에 넣지 않고, 필요하면 프레임워크/도메인 용어로만 취급함.
- 상위 레이어로 갈수록 더 덜 구속적인 문맥 계층이라는 원칙을 채택함.

## 구현된 컴포넌트

### 렉서 / 파서
- 파서는 파일 분할 구조: `parser.c`, `parser_expr.c`, `parser_stmt.c`, `parser_decl.c`, `parser_domain.c`, `parser_async.c`
- 문법 표면: `let`, `func`, `async`, `spawn/await`, `if/for/while/match/select`, `slot/view/move`, `subject/class`, `struct/object/tobject`, `ability/role/party/relation/effect/zone/roster/world`, `event`, `subject`, `import/export/namespace`
- `world`, `roster`, `relation`, `effect`, `zone`은 declaration position에서만 키워드처럼 동작하고, local variable / expression position에서는 식별자로 그대로 쓸 수 있음
- `subject`, `class`, `struct`, `object`, `tobject` declaration은 parser AST에서 서로 다른 nominal flavor로 보존됨
- 현재 domain 표면은 `ability/role/party/roster/world`에 더해 `relation/effect/zone`의 최소 body surface까지 parser/semantic에 연결됨
- `relation`, `effect`, `zone`은 `subject slot` / `object slot` / `tobject slot` / `shared` / `func`까지의 최소 표면이 parser/semantic에 연결됨
- `relation` / `effect`도 `refresh objectSlot from subjectSlot`, `publish dtoSlot from subjectSlot`, `bind slotName from sourceSlot` projection sync를 declaration body에서 직접 가질 수 있음
- `relation`, `effect`, `zone`의 domain slot은 optional initializer를 받아 `object slot view: PlayerView = ToObject(PlayerView, player)` 같은 local projection wiring을 직접 표현할 수 있음
- `relation`, `effect`는 optional `for ...` header로 subject endpoint/target을 declaration header에 고정할 수 있음
- `relation`, `effect`는 optional `for object ...` header로 object endpoint/target도 declaration header에 고정할 수 있음
- `relation` / `effect`는 positional nominal constructor call을 받아 local runtime instance를 만들 수 있고, constructor argument arity/type을 semantic에서 검사함
- `zone`은 `relation slot` / `effect slot` / fixed-capacity `effect pool`으로 overlay type을 참조할 수 있고, `world`는 `zone` slot으로 하위 지역 규칙을 참조할 수 있음
- `zone`은 `apply effectSlot to targetSlot`, `detach effectSlot from targetSlot`으로 local effect attachment/detachment를 최소 surface로 표현할 수 있음
- `zone`은 `link relationSlot between left, right`, `unlink relationSlot between left, right`로 local relation wiring을 최소 surface로 표현할 수 있음
- `zone`은 `refresh objectSlot from subjectSlot`으로 subject -> object projection 갱신을 명시할 수 있음
- `zone`은 `publish dtoSlot from subjectSlot`으로 subject -> tobject projection 갱신을 명시할 수 있음
- `zone`은 `bind slotName from sourceSlot`으로 object/tobject target kind를 slot declaration에서 추론하는 projection sync surface를 가짐
- `HasProjection(slotName)` builtin은 relation/effect/zone declaration / method 안에서 선언된 object/tobject projection slot의 sync-ready 여부를 Bool query로 읽을 수 있음
- `zone`은 `maintain effectSlot on targetSlot`, `maintain relationSlot between left, right`로 지속 lifecycle rule을 선언할 수 있음
- `zone`은 `authority subjectSlot`으로 mutation/projection 승인 주체를 선언할 수 있음
- `zone` authority는 `authority subjectSlot requires Ability[, Ability]`로 승인 주체가 수행 가능한 ability 계약까지 명시할 수 있음
- `zone`은 `state name: effect ... on ...` / `state name: relation ... between ..., ...`로 lifecycle state alias를 선언할 수 있음
- `zone`의 `apply/link/detach/unlink/refresh/publish/bind/maintain`은 optional `by subjectSlot`을 받아 authority와 연결됨
- `zone`은 `apply stateName`, `link stateName`, `detach stateName`, `unlink stateName`, `maintain stateName` shorthand를 지원함
- `HasLayer(layerSlot)` builtin은 zone declaration / zone method 안에서 선언된 `relation slot` / `effect slot` / `effect pool` 활성 여부를 Bool query로 읽을 수 있음
- `HasState(stateName)` builtin은 zone declaration / zone method 안에서 선언된 state alias를 Bool query로 읽을 수 있음
- `HasState(effectState, targetSlot)` / `HasState(relationState, leftSlot, rightSlot)`로 state가 어떤 slot 조합에 붙는지까지 명시적으로 질의할 수 있음
- `world`는 `state name: zone zoneSlot`, `state name: zone zoneSlot projection projectionSlot`, `state name: zone zoneSlot layer layerSlot`, `state name: zone zoneSlot state zoneStateName`, `state name: all zoneOrState[, ...]`, `state name: any zoneOrState[, ...]`, `activate/deactivate/maintain zoneOrState` 최소 lifecycle surface를 가짐
- `HasZone(zoneOrState)` builtin은 world declaration / world method 안에서 zone slot 또는 world state alias를 Bool query로 읽을 수 있음
- `HasZoneProjection(zoneSlot, projectionSlot)` / `HasZoneLayer(zoneSlot, layerSlot)` / `HasZoneState(zoneSlot, stateName)` builtin은 world declaration / world method 안에서 embedded zone의 projection/layer/state flag를 직접 Bool query로 읽을 수 있음
- 파생 `world state`는 zone active flag와 embedded zone projection/layer/state flag를 자동 조합해 계산되며, 조합 state는 `all`/`any`로 앞선 zone/state alias를 다시 합성할 수 있음
- 조합 `world state`는 duplicate input과 direct zone slot + plain zone alias 중복을 semantic warning으로 보고함
- 조합 `world state`는 raw zone slot을 직접 입력으로 받으면 warning을 내고, plain world state alias를 통한 파생층 입력을 권장함
- direct `activate/deactivate/maintain` 대상은 plain `state name: zone zoneSlot` alias만 허용함
- `world` lifecycle도 이제 duplicate `activate` / `deactivate`, conflicting `activate` + `deactivate`, redundant `activate` + `maintain`를 semantic warning으로 보고함
- `activate/deactivate/maintain battle`처럼 direct zone slot 이름을 쓰는 표면도 C/LLVM world sync 경로에서 semantic과 동일하게 해석됨
- C/LLVM world sync는 이제 `command pass(reset/directives) -> zone sync pass -> derived pass` 순서로 고정됨
- world runtime은 `__zone_dirty_<slot>`와 `__world_derived_dirty`를 유지해 active flag가 바뀐 zone만 다시 sync하고, derived state는 dirty일 때만 다시 계산함
- world constructor는 embedded zone dirty와 derived dirty를 `true`로 시작시켜 첫 world sync에서 zone projection/layer/state를 빠뜨리지 않음
- world method는 post-sync 전에 embedded zone dirty를 보수적으로 다시 올려 world-owned zone 교체/갱신이 derived layer까지 전파되게 함
- world value에 embed된 zone은 바깥의 옛 zone 바인딩을 통해 다시 assignment/hosted `func`/`action`으로 mutate할 수 없고, semantic error로 차단됨
- C backend는 zone layer slot을 `bool __layer_active_<slot>` 필드로, zone state alias를 `bool __state_<name>` 필드로, world zone lifecycle을 `bool __zone_active_<slot>` / `bool __zone_state_<name>` 필드로 낮춤
- C backend는 relation/effect/zone/world마다 `<Type>_sync(self)` helper를 생성하고, method 실행 전후에 호출해 `refresh`/`publish` projection과 state/lifecycle flag를 incremental하게 동기화함
- C backend는 relation/effect/zone projection slot에 `__projection_ready_*` flag를 두고 `HasProjection(...)`를 현재 domain self field query로 lowering함
- C backend는 `relation` / `effect` constructor를 compound literal runtime instance로 lowering하고, instance method call도 pointer-self로 호출함
- LLVM backend도 relation/effect/zone/world declaration에 대해 `<Type>_sync(self)` helper와 method 전후 sync 호출 parity를 가지며, contextual `HasProjection(...)` / `HasLayer(...)` / `HasState(...)` / `HasZone(...)` lowering과 constructor/runtime instance path까지 연결됨
- `HasProjection(...)`는 현재 relation/effect/zone 문맥에서 semantic/C/LLVM runtime parity까지 닫혀 있음
- `zone` layer slot은 이제 C/LLVM 양쪽에서 `void*` placeholder가 아니라 typed `relation` / `effect` runtime instance로 유지되며, zone sync가 subject slot 값을 overlay endpoint/target에 바인딩한 뒤 `<Layer>_sync(&self->layer)`를 호출함
- LLVM backend의 `effect pool`도 이제 실제 `{items, active, count, cap}` storage로 lowering되어 `apply poolName to slotName`와 `HasLayer(poolName)`이 semantic-only가 아니라 concrete runtime path를 가진다
- direct `apply/link/detach/unlink`와 `maintain effect/relation/state` 모두 C/LLVM zone sync에서 실제 layer active/state/projection 전파로 연결됨
- zone method 안에서 `self.poison.view.hp`, `self.trust.packet.name` 같은 embedded overlay projection read가 LLVM smoke까지 닫혀 있음
- C backend에서 `HasLayer(...)`는 zone rdlock + generation stale-warning을 감싼 generated helper로 lowering되고, `HasState(...)` / `HasZone(...)`는 zone/world method 문맥 안에서 실제 `self->__state_*` / `self->__zone_*` 필드 질의로 lowering됨
- `zone`의 `apply/detach`는 `effect` declaration의 bindable target 수와 타입을 검사하며 object target도 허용함
- `zone`의 `link/unlink`는 `relation` declaration의 bindable endpoint 수와 타입을 검사하며 object endpoint도 허용함
- `zone` / `relation` / `effect`의 `refresh`/`publish`/`bind`는 object/tobject slot kind와 projection field 정합성을 검사하고, source는 subject/object를 허용하되 tobject source는 금지함
- `zone` subject slot은 이제 bare `class`가 아니라 subject host (`subject`, `subject`)만 허용함
- `ToObject` / `ToTObject` source projection은 이제 bare `class`가 아니라 subject host binding만 허용함
- `role`은 이제 non-subject nominal declaration에 바인딩되면 semantic error를 냄
- `party` role slot은 이제 subject-bound role impl이 실제로 존재하는 ability만 협력 슬롯으로 받을 수 있음
- `zone`의 `maintain`은 `effect/relation` contract를 재사용하고 duplicate/conflicting lifecycle rule에 warning을 냄
- `zone` authority는 선언된 subject slot만 받을 수 있고, authority가 있을 때 mutable rule이 `by`를 생략하면 warning을 냄
- `zone` state shorthand는 effect/relation kind mismatch를 semantic error로 보고함
- `zone`은 현재 subject가 0개이거나 4개를 크게 넘는 형태에 대해 운영 lint를 냄
- `relation`, `effect` declaration은 C backend에서 struct + method wrapper로 codegen됨
- `relation/effect/zone`은 여전히 계층 간 구조적 의미론이 더 필요함
- `subject`는 코어 identity-bearing host로 고정되며, role binding, subject slot, `ToObject` / `ToTObject` source, subject copy restriction에 참여함
- `object`는 intent를 시작하지 않는 passive state target으로 정리되며, 상태/반응/helper `func`를 가질 수 있음
- `tobject`는 `object`보다 더 좁은 boundary transfer/publish 형식으로 유지됨
- `object`와 `tobject`는 struct-style declaration syntax를 공유하지만 같은 계약은 아니다. `object`는 local/internal projection contract이고, `tobject`는 boundary projection contract다.
- backend lowering은 이제 `object borrow-first / tobject materialize-first` 첫 단계를 가진다. non-escaping `ToObject(...)` local binding은 C/LLVM 양쪽에서 borrowed projection alias로 낮추고, whole-value가 필요할 때만 object literal을 재구성한다. `ToTObject(...)`는 materialized transfer value를 유지한다.
- relation/effect/zone projection slot runtime은 `__projection_ready_*`뿐 아니라 `__projection_dirty_*`도 유지한다. sync helper는 dirty target만 다시 rebuild하고, source slot assignment는 matching projection target을 invalidate한다.
- `subject`는 이제 일반 `func`와 공적 `action`을 모두 가질 수 있음
- `func`는 계산/보조 판단/국소 상태 갱신용 hosted func이고, `action`은 zone/authority/effect와 연결되는 공적 오케스트레이션 동사임
- example smoke는 backend-aware exact stdout goldens와 backend-aware exact `expected_results` goldens를 함께 지원함
- enum/result 패턴 shorthand: `Some(x)`와 `.Some(x)` 둘 다 파싱됨. `case .Ok(v):`, `return .None;` 같은 문서 표기도 현재 파서 기준으로 허용됨
- `Option<T>` 표면: `Some/None`, `IsSome/IsNone`, `UnwrapOption`, `match` destructuring이 semantic/C/LLVM 경로에 연결됨
- `match` 시맨틱: `Option/Result/tagged enum` destructuring 바인딩과 제한된 exhaustiveness check가 동작함
- `match` 품질 진단: duplicate variant case와 redundant default를 warning으로 보고함
- `with effects ...` / `/// @effects ...` 계약: 선언이 있으면 body inferred effect와 mismatch를 semantic error로 보고함
- effect contract는 이제 최소 closure/subsumption과 join API를 가진다. 현재 `collapse`는 `nondeterministic`를 포함하는 것으로 취급되며, disjoint branch effect도 계약으로 합쳐진다.
- `use module;` duplicate import는 semantic warning으로 보고하며, `use datetime;` exported stdlib surface 회귀가 존재한다.
- ability `require` 필드는 이제 선언 검증만이 아니라 role impl 시 bound subject host가 실제로 요구 필드를 만족하는지도 검사한다.
- `ability`는 이제 기본 공개 계약이다. cross-module에서 숨겨진 ability는 explicit `private ability`일 때만 `role impl ability ...`와 `action ... requires Ability` 양쪽에서 semantic error로 차단된다. `export ability`는 허용되더라도 권장 표기는 아니다.
- `secure` capability는 이제 `SecureSlot`뿐 아니라 `Token<T>` 시그니처도 secure effect를 유발하며, paired token 이름/타입 정합성까지 정적으로 검사한다.
- `Token<T>`와 secure capability는 이제 channel transport도 금지되어 capability-bearing 값이 병렬/원격 payload로 새는 경로를 semantic에서 차단한다.
- authority가 선언된 `zone`에서 boundary projection(`publish` / tobject-target `bind`)은 이제 explicit `by <subjectSlot>` 없이 허용되지 않는다.
- explicit `private` nominal member는 same host 내부에서만 허용되고, explicit visibility가 붙은 nominal field/method/constructor는 cross-module 경계에서 실제로 검사된다.
- foreign non-exported nominal constructor는 이제 cross-module 호출이 차단되며, exported nominal constructor만 외부 모듈에서 생성할 수 있다.
- ability `require` field는 duplicate declaration을 semantic error로 보고한다.
- `Box<T>` explicit handle surface: `Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`
- `Box<class>`는 현재 object handle 경로로 허용되며, plain class value parameter/return 제한을 우회하는 명시적 저장/전달 표면으로 사용 가능
- `subject`는 plain copy / plain value parameter / plain value return이 금지되고, `class`는 값 복사/값 parameter/값 return을 허용함
- C backend와 LLVM backend 모두에서 `subject` method는 `self` pointer, `class` method는 `self` value로 lowering됨
- plain `Slot<subject>`와 `Slot<subject>`는 이제 local object-cell anchor로 허용됨
- 현재 회귀 수치: `make test-transpile` 통과, `make llvm-test-smoke` 통과, `make test-abi` 통과, `make test-abi-perf` 통과
- `SecureSlot<subject>`와 `SecureSlot<subject>`도 이제 local secure object-cell anchor로 허용됨
- `own/ref Slot<subject-host>` / `own/ref SecureSlot<subject-host>` 함수 경계 전달이 semantic + C/LLVM backend에 반영됨
- secure boundary slot은 paired token symbol을 함수 바디 안에 자동 노출해 `Write(s, ..., s_token)` / `Release(s, s_token)` 형태를 유지함
- secure boundary slot은 helper를 한 번 더 거치는 forwarding call에서도 paired token이 함께 전달됨
- LLVM backend는 `self.battle.player.hp = hp` 같은 deeper nested member assignment도 world/zone/object-cell 경로에서 실제로 갱신함
- 남은 공백은 richer handle/object-cell propagation semantics의 더 복잡한 경계 조합임
- 채널 convenience surface: `TryRecv -> Option<T>`, `RecvTimeout -> Option<T>`, `TrySend/SendTimeout -> Bool`, `TrySendStatus/SendTimeoutStatus -> Option<Bool>`이 C/LLVM 경로에 연결됨
- 채널 backpressure observation surface: `ChannelLength/ChannelCapacity/ChannelSpace -> Int`, `ChannelFull/ChannelClosed -> Bool`이 C/LLVM/runtime에 연결됨
- 현재 `TryRecv/RecvTimeout/TrySend/SendTimeout`은 plain-value channel 중심이며 movable resource channel은 의도적으로 제외됨

### 시맨틱
- 타입 시스템 + 슬롯 규칙 + move/consume 추적
- anchored handle(`Slot/SecureSlot/DeviceSlot`)와 movable handle(`QubitSlot`)의 경계 규칙 반영
- `RemoteFuture<T>`의 `await`가 `Result<T>`를 반환하도록 강제

### 백엔드
- C 백엔드: reference/fallback, 스모크와 트랜스파일 테스트로 유지
- LLVM 백엔드: 기본 실행 경로, backend-compare 및 llvm-smoke로 회귀 체크
- 두 backend 모두 `object borrow / tobject materialize` 첫 단계를 공유한다. 다만 LLVM은 여전히 HIR fallback debt가 남아 있고, transpiler의 ABI metadata 실사용도 더 강화해야 한다.

### 런타임
- slot/secure slot/device slot/qubit slot 런타임
- coroutine runtime (POSIX ucontext + Windows Fiber)
- channel/parallel/select 지원
- runtime `ReadFile/WriteFile`는 경로 정책과 크기 상한을 강제한다
- channel non-blocking/timeout helper 지원
- helper body를 따라가는 parallel slot conflict detection이 들어가서, top-level helper가 `ref Slot<subject>` / `own Slot<subject>`를 받는 경우도 동일 슬롯 병렬 mutation/release 충돌로 거부됨
- `select`는 round-robin 시작 인덱스로 readiness를 검사해 단순 고정 순서 starvation을 줄임
- task cancellation surface: `Cancel(task)` / `IsCancelled()`가 C/LLVM/runtime에 연결됨
- spawned descendant는 부모 task의 cancellation chain을 상속함
- 현재 cancellation은 cooperative/best-effort이며, preemptive interruption은 아직 아님

### 도구
- debugger는 breakpoint set/clear/list와 single-frame backtrace를 지원함
- formatter는 `--write` 외에 `--check`, parse-guard, idempotent roundtrip guard를 가짐
- LSP는 diagnostics/hover에 더해 completion/documentSymbol/definition/references/rename을 제공함

### 구현 메모

- 저장소의 `.inc` 파일은 Pergyra 언어 문법이 아니라, 큰 C 구현 파일을 안전하게 분할하기 위한 **textual include 조각**이다.
- 예: `type_checker.c`, `transpiler.c`, `llvm_expr.c`, 테스트 대형 파일은 일부 로직을 `.inc`로 나눠 `#include`한다.
- 목적은 빌드 구조를 바꾸지 않고 파일 길이와 책임 범위를 줄이는 것이다.

## 테스트 현황

2026-04-10 현재 직접 확인한 기준:

| 스위트 | 결과 |
|---|---|
| security | 52 passed |
| semantic | 695 passed |
| transpile | 461 passed |
| llvm smoke | 통과 (`zone_action_effect_runtime`, `cancel_propagation`, `channel_pressure`, `string_io` 포함) |
| abi | 56 passed |

추가 회귀:
- `make test-semantic` 통과
- `make test-parser` 통과
- `make test-transpile` 통과
- `make llvm-test-smoke` 통과 (async, select, tagged-union, RemoteFuture, device slot, generics, channel pressure 등)
- `make test-security` 통과
- `make test-abi` 통과
- zone method 안의 subject `action` call은 현재 C/LLVM 모두에서 matching `effect slot` runtime activation과 embedded layer sync까지 연결됨
- `self.player.Attack()` 같은 nested nominal host method call도 이제 C/LLVM 모두에서 실제 method dispatch로 lowering됨
- semantic `slot_analyzer`는 이제 `return/call/channel-send` 기반 slot escape를 분류하고 conservative warning을 낸다
- LLVM AST emission path는 non-escaping local slot에 대해 entry-hoist 대신 current-block alloca sinking을 시작했다
- effect lattice는 이제 `closure/join/compare`뿐 아니라 `meet/conflict` API와 if/match branch-local effect join/conflict 경고도 가진다

## 남은 주요 작업

### 단기
- orchestration 고도화 (`select` 공정성, timeout, cancellation, backpressure)
- partial 완료: channel timeout/non-blocking built-in surface 추가
- partial 완료: channel backpressure observation surface (`ChannelLength`, `ChannelCapacity`, `ChannelSpace`, `ChannelFull`, `ChannelClosed`)
- partial 완료: `select` round-robin fairness
- partial 완료: cooperative cancellation surface (`Cancel`, `IsCancelled`)
- partial 완료: spawned descendant cancellation propagation
- 완료: semantic O2 crash root-cause 정리 및 회귀 고정 (`Channel*` diagnostic format bug)
- partial 완료: source-level `with effects ...` signature surface
- partial 완료: `Box<class>` explicit handle surface (`BoxGet/BoxSet/BoxDrop/BoxIsValid`)
- partial 완료: `subject` vs `class` lowering/runtime split의 첫 단계 (`subject=self-cell`, `class=value self`)
- partial 완료: `subject`를 코어 host로 semantic 정렬 (`role`, `subject slot`, projection source, copy restriction)
- partial 완료: `subject Name { ... }` subject-first surface
- partial 완료: standalone `subject Name { ... }`를 transitional syntax로 경고
- partial 완료: plain `Slot<subject-host>` / secure `SecureSlot<subject-host>` local object-cell anchor
- partial 완료: `own/ref Slot<subject-host>` / `SecureSlot<subject-host>` 함수 경계 전달 (semantic + C backend)
- effect system 2단계 (더 정교한 effect lattice, call-site contract)
- relation/effect/zone declaration 이후의 구조적 의미론 고도화
- 안정화 문서 갱신 및 표면 문법 정리
- 보안 2단계:
  - `PGY_IO_ROOT`의 Windows canonical enforcement 강화
  - runtime file I/O의 symlink 정책을 플랫폼별로 더 일관화
  - secure slot/capability 문서를 현재 구현 수준과 더 정렬

### 중기
- stable stdlib surface 고정
- toolchain 강화 (formatter, LSP 진단 품질, debugger)

### 장기
- JavaScript backend policy 초안 정리 (`docs/23_js_backend_policy.md`)
- WebAssembly 타겟
- 패키지 매니저
- 성능 최적화 (LLVM 쪽 집중)
