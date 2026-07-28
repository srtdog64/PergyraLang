# Pergyra — 현재 진행 상황

마지막 업데이트: 2026-07-29

## 2026-07-29 intent guard/post/compensate fail-closed checkpoint

- `tobject` 구현을 다시 추적한 결과, detached immutable success receipt/failure
  payload에는 맞지만 variant branch, step completion, predecessor, rollback graph의
  owner는 아니라는 기존 경계가 확인됐다. 이 사실은 intent transition SoT가
  소유해야 한다.
- Self parser는 `guard`/`post`/`compensate` row를 보존하지만 DIR/MIR/C path가
  소비하지 않아 이전에는 성공한 듯 보이면서 clause가 사라졌다. 이제 첫 executable
  DIR boundary에서 세 clause를 서로 다른 진단으로 거부하고 partial MIR/C artifact를
  만들지 않는다.
- 이 안전 ratchet은 `SUBSTITUTING` 진척이 아니다. 다음 executable rung은 source에
  선언된 exact success/failure variant identity, payload binding, stable predecessor,
  success-only completion과 compensation target을 같은 semantic/DIR/MIR 변경에서
  닫아야 한다. 일반 `expect` 실패의 기존 rollback 의미를 typed failure와 혼동해
  backend completion flag 위치만 옮기는 수정은 금지한다.

## 2026-07-28 fallible action tobject outcome checkpoint

- Production `DriverRung2Execution.EmitDirectMir`가 이제 success receipt,
  ordinary rejection, artifact failure를 서로 다른 typed variant로 반환한다.
  `PgyCompilerWorld`/composition은 이를 그대로 운반하고 bootstrap Main이 exact
  target/path/visibility receipt 또는 stage/status/recovery failure payload를 소비한다.
- 기존 `DriverRung2ExecutionResult { ok, stage, execution_identity, ... }`는 삭제했다.
  Detached payload에는 subject authority, source freshness, graph/topology identity를
  싣지 않는다. Unknown failure status와 known-but-wrong target은 fail closed한다.
- Native C 선언 scheduler가 action return enum을 payload tobject보다 먼저 출력하던
  결함을 닫았다. Hosted method의 by-value return/parameter는 schedule fact가 되고,
  implicit self, pointer-carried mutual subjects, direct host-self type은 false cycle을
  만들지 않는다.
- Focused gate에서 native C/LLVM/self C가 `ok=7`, `error=9`를 동일하게 실행했다.
  Fresh bootstrap Main은 real artifact-begin failure를 schema/path/begin-temp/status 1/
  prior-final-preserved/temp-removed로 정확히 구별했고 partial output은 없었다.
- Fresh build peak는 `pgy` 약 808 MiB + `cc1` 약 1.03 GiB, 합계 약 1.8 GiB였다.
  20 GiB whole-graph/process-tree 재발은 관측되지 않았다.
- 이 rung은 failure tobject를 `OUTCOME_CONSUMED`로 올리지만 전체 `tobject`,
  subject/action/zone/world는 계속 `REACHABLE`, compiler `intent`는 `SURFACE`다.
  다음 falsifier는 두 production action의 typed intent outcome binding, failure branch,
  compensation과 exact predecessor evidence다.

## 2026-07-28 binding production self-C substitution checkpoint

- `binding_slot_constructor_source_order`가 이제 production Pergyra-built
  `CompileSourceToCVerified`를 통과한다. Direct source C와 admitted-MIR C는
  byte-equal이며, self C/native C/native LLVM은 모두 `door=5`, `key=9`,
  `view=5`를 출력한다.
- Positive MIR의 stable field ID를 유지한 채 `door: binding_slot`을
  `object_slot`으로, `key: binding_slot`을 `tobject_slot`으로 바꾸면 두 경우
  모두 nominal constructor policy가 `expected: at_most_1`, `actual: 2`로
  거부한다. C preamble/body는 출력되지 않는다.
- 따라서 이 좁은 binding admission/runtime 입력 기능 slice만
  `SUBSTITUTING`으로 승격했다. `object`, `tobject`, `zone`, `world`, `intent`의
  compiler-organization 등급과 world/tobject query rung은 승격하지 않았다.
- `semantic.nominal_field_kind`, `semantic.domain_runtime_assignment`,
  `dir.domain_graph` registry row에는 실행·negative gate를 추가했지만, 남은
  vessel/lifecycle/epoch/shared-plan 범위 때문에 세 family 모두 `BRIDGE`를 유지한다.
- 이때 기록한 다음 falsifier는 최신 fallible action tobject outcome checkpoint에서
  완료됐다. Multi-action intent의 outcome binding/compensation은 계속 열린다.
- Hard self-host contract의 stale owner anchor도 현재 구조로 맞췄다. MIR fixture
  inventory는 `driver_rung2_mir_manifest_owner.pgy`, body call environment는 shared
  environment owner, graph traversal은 accessor가 소유한다. Collection mutation은
  old generic simple-statement lane에서 제거했고 ArraySet secondary graph attach는
  graph-owned lane 한 곳만 남겼다.
- 현재 source로 fresh driver를 다시 빌드한 binding gate는 PASS다. Indexed
  assignment direct/admitted C byte parity, runtime `2`, missing-target-graph negative도
  PASS다. 다만 넓은 filtered producer-parity runner는 그 검사 전에 기존 oracle MIR
  machine-layer admission 오류로 멈췄으므로 전체 runner GREEN은 주장하지 않는다.

## 2026-07-28 tobject publication + domain admission boundary checkpoint

- `object`/`tobject`는 zone/relation/effect constructor input이 아니라
  `refresh`/`publish`/`bind`가 materialize하는 projection destination으로
  고정했다. `object slot ... = ...`와 `tobject slot ... = ...`는 DIR/MIR/runtime
  owner가 없어 zero-filled storage로 조용히 사라지던 문법이므로 native semantic과
  self parser가 fail closed한다.
- Object-valued endpoint admission은 contextual `binding slot`으로 분리했다.
  Zone constructor는 declaration source order의 subject/binding만 받고,
  relation/effect constructor는 `for ...` header participant만 받는다. Layer/shared
  storage와 projection destination을 caller가 선주입하는 경로는 native C/LLVM과
  self semantic negative로 차단한다.
- `binding`은 145-row language-word registry에서 self lexer, parser selector, LSP
  completion/hover, TextMate grammar로 생성된다. `binding_slot` wire/AST identity도
  MIR declaration-field vocabulary가 소유한다.
- Self path에서 발견된 세 누락을 닫았다. Effect/relation participant label은 mutable
  string SSA carry 대신 immutable classifier 결과를 사용한다. DIR graph census는
  SubjectSlot/BindingSlot participant를 함께 세고, projection source lookup은
  Subject -> Binding -> Object exact field ID 순서를 사용한다.
- 관측 결과 self MIR은 intent positive 46,384 bytes, binding positive 10,394 bytes로
  생성됐고 네 self negative는 모두 거부됐다. Native C/LLVM은 binding fixture의
  `door=5`, `key=9`, `view=5`와 interleaved projection fixture의
  `alpha=7`, `beta=9`, `view=7`, `receipt=9`를 동일하게 실행했다.
- 이 변경의 self binding 경로 등급은 `REACHABLE`이다. Production general self C가
  binding positive를 직접 실행해 C-owned path를 대체한 증거는 아직 없으므로 새
  `SUBSTITUTING` 진척으로 세지 않는다. 다음 falsifier는 그 self C 실행과 valid-ID
  binding/projection kind mutation이다.
- 과거 20 GiB처럼 보인 현상은 compiler 고유 비용이 아니라 sealed whole graph를
  다시 검증한 lifetime 결함과 중복 process-tree build 합산이었다. 현재 focused
  driver build 관측은 `pgy` 약 674 MiB + `cc1` 약 791 MiB였고, 대응 절차는
  `docs/127_compiler_speed_engineering.md` §8.1에 고정돼 있다.
- 오래된 tracked 실행 transcript `testall_run.txt`는 삭제하고 ignore했다. 실제
  fixture와 owner gate만 실행 증거로 유지한다.

## 2026-07-28 intent execution + tobject boundary checkpoint

- Self DIR의 exact intent declaration/step facts가 이제 별도 typed MIR intent
  routine으로 운반된다. MIR consumer는 participant/zone/action/authorization/commit
  identity를 교차 검증하고, full source와 admitted MIR에서 byte-equal C를 만든다.
- `intent_callable_execution`은 `Checkout(payment, buyer)`를 실제 호출한다. Self C,
  native C, native LLVM이 모두 `buyer.total=3`, `payment.total=3`, 두 projection
  ready, `Mina`, world ready를 동일하게 출력했다. Action은 payment zone의 admitted
  buyer에 실행되고 projection sync 뒤 caller buyer로 value-result writeback된다.
- Intent kind, commit identity, participant type, zone alias, authorization
  cross-carrier, rollback identity 변조는 partial C 없이 실패한다. 이 successful
  path는 `REACHABLE`이다. Fallible `expect`, compensation/effect observability와 실제
  `PgyCompilerWorld` root intent takeover가 열려 있으므로 whole-intent
  `SUBSTITUTING`으로 세지 않는다.
- `tobject`는 detached immutable payload/receipt이며 graph owner가 아니다. Zone
  constructor는 source order의 subject/binding만 받고 object/tobject projection
  storage를 인수로 받지 않는다. Native C/LLVM positive source-order 실행과 native/
  self arity negative가 이 경계를 고정한다.
- Publish된 `tobject`는 새 projection source가 될 수 없다. Native source semantic,
  self DIR source producer, native/self MIR admission에서 거부하며, 실제 valid
  TObjectSlot ID로 바꾼 malformed self MIR도 C output 전에 실패한다.
- 다음 executable falsifier는 fallible `expect`의 typed success/failure 분기와
  compensation/effect outcome이다. 그 뒤 둘 이상의 실제 compiler action을
  `PgyCompilerWorld` root intent에서 호출하고 migrated direct bypass를 삭제해야
  compiler dogfood가 `SUBSTITUTING`으로 올라간다.

## 2026-07-28 exact intent DIR reachability checkpoint

- Self typed AST가 native AST의 intent mode/rollback/retry, ordered
  involves/value, step와 step child rows를 distinct kind로 보존한다. 명시적
  `using`/`where`는 native `IntentStep:` header와 child row가 일치할 때만
  수용하며 subintent/transfer header는 현재 bounded rung에서 fail closed한다.
- `SelfDirIntentFacts`가 participant alias/type/value class와 ordered step range를
  소유한다. `SelfDirIntentStepFromArtifact`는 raw source를 다시 읽지 않고 semantic
  action contract의 `within/requires/causes/authorized by`를 소비하며 `self`
  authority를 실제 receiver participant로 결속한다.
- `domain_graph_fact_owner.pgy`의 unconditional `IntentDecl` 거부는 삭제됐다.
  Intent declaration node와 participant type, step zone/who/requires/causes/
  authorized-by, ordered predecessor edge가 graph census와 anchor에 한 번 포함된다.
- Fresh Pergyra-built DRV-2는 full `intent_callable_reachability` source를
  `pgy.mir.v1`까지 생성한다. Single-step native/self graph는 정확히 14 nodes,
  30 edges, anchor `14937234969446610600`; explicit-using two-step variant도
  predecessor edge를 포함한 anchor `14937235081115760274`로 일치한다. Wrong-zone
  `using`은 partial MIR 없이 거부된다.
- SoT registry는 intent declaration signature를
  `selfhost.intent_declaration_rows` authority로, intent DIR fact/step owner를
  `dir.domain_graph` bridge로 분류한다. Coq authority projection과 live gate
  evidence를 정합화한 결과는 61 authorities, 62 derived carriers,
  `CLOSED=34 BRIDGE=27 ACTIVE=0`이다. 이 runner에는 Coq/Rocq가 없어 proof
  compile은 declared skip이며 live owner/consumer 및 negative mutation 검사는
  통과했다.
- 이 delta는 `REACHABLE`, not `SUBSTITUTING`이다. Source -> self C는 이제 DIR을
  지나지만 MIR JSON reconstruction에서 intent declaration/step 실행 fact가
  운반되지 않아 `SemanticAstExpressionSurfaceFacts`의 `ast_artifact_invalid`에서
  멈춘다. 다음 executable rung은 DIR owner를 재스캔하지 않는 MIR intent carrier와
  `Checkout` call execution이며, native AST/MIR graft나 count-only graph는 금지한다.
- `tobject`는 계속 `subject source -> tobject slot -> publish`의 immutable detached
  handoff만 소유한다. Intent step identity, authority, predecessor graph를 tobject에
  넣지 않는다.

## 2026-07-28 intent callable + observability SoT checkpoint

- Self semantic call resolution now derives source `intent` signatures from
  exact typed-AST declaration and ordered `involves`/`value` child identity.
  Intent remains a distinct purpose boundary; it is not inserted into the
  function/action signature owner.
- The 51 native `Intent*` observability calls now have one append-only owner,
  `src/common/intent_observability_abi.def`. Native C includes it directly and
  a checked generator projects stable ID, source/runtime name, arity, result,
  and parameter signatures into self-host semantics. The omitted
  `IntentHistoryCount` row is no longer hand-maintained drift.
- The original world/zone/tobject fixture and the focused variant now converge
  at `self-host DIR authority shape is unsupported`; the old
  `undefined_function Checkout` and incomplete call-target failures are gone.
  Wrong arity, wrong participant type, and renamed intent fail earlier with
  stable diagnostics and no partial MIR.
- Grade is `REACHABLE`, not `SUBSTITUTING`. The next rung must add a typed DIR
  intent fact owner for participant and step authority/topology edges before
  removing the current `IntentDecl` rejection. A count-only bypass is forbidden.
- The rebuilt native-current DRV-2 sampled about 674 MiB private in `pgy` and
  791 MiB private in `cc1`; no 20 GiB-class graph-revalidation regression was
  observed.

## 2026-07-28 world/tobject query reachability checkpoint

- Self semantic은 명목 생성자 호출을 일반 함수의 exact-arity 경로와 분리한다.
  `PaymentZone(Clone(buyer))`는 caller가 공급하는 subject slot prefix만 받고,
  topology-managed layer storage는 생성자 인자에서 제외된다.
- `HasProjection`/`HasZoneProjection` 인자는 일반 값 변수가 아니라 선언-scoped
  symbolic identity다. Self owner는 `PaymentWorld.payment -> PaymentZone ->
  buyerView:ObjectSlot | buyerPacket:TObjectSlot`을 exact field kind로 검증한 뒤,
  검증된 leaf node만 undefined-value 검사에서 제외한다.
- `refresh/publish ... by buyer`의 `by`는 tobject에 authority를 넣는 문법이 아니라
  projection 전이의 provenance다. 이를 제거한 축소 fixture가 무진단 실패한 것을
  첫 falsifier로 삼아 양성/음성 fixture 모두 명시적 participant를 유지한다.
- `world_tobject_projection_query_owner.sh`는 새 DRV-2가 source에서 typed MIR의
  tobject slot/publish/world query fact를 산출하는지 확인하고, 현재 native C/LLVM이
  모두 `true`를 출력하는지 비교한다. 존재하지 않는 `missingPacket`은 부분 MIR
  없이 `undefined_symbol`로 실패한다.
- 이 delta는 `REACHABLE`이다. Self query backend lowering/실행은 아직 없고 직접
  `ToTObject(Target, source)`의 self semantic/codegen도 열려 있다. 현재 실제
  tobject 실행 패턴은 `subject source -> tobject slot -> publish`다.
- 전체 `world_zone_projection_visibility`는 이제 initializer/query seam을 지나
  `undefined_function`, `func: Checkout`에서 멈춘다. 다음 executable falsifier는
  intent callable reachability이며 tobject/map/backend 우회로 풀지 않는다.
- 변경 소스를 포함한 fresh Pergyra-built DRV-2는 약 28분에 설치됐고, 단일
  `gen2.exe`의 sampled peak는 private 1,173.0 MiB / working set 1,071.1 MiB였다.
  20+ GiB 재발은 없다. Self fact builder는 growable record member를 직접
  `ArrayPush`하지 않고 local arrays를 완성한 뒤 immutable fact를 한 번 생성한다.

## 2026-07-28 explicit projection-map executable checkpoint

- Self parser가 `map { target <- source }`를 버리지 않고 refresh/publish
  directive의 typed `ProjectionMap:` child로 보존한다. DIR은 parent directive와
  entry를 exact node identity로 결속하고 target 중복을 거부한다.
- `semantic.domain_runtime_assignment`의 self producer가 explicit source spelling을
  exact declaration-field path로 해석한다. Assignability는 새 semantic owner
  `SemanticDomainProjectionTypeAssignable`가 native `type_is_assignable(from, to)`
  방향으로 결정하며 MIR은 이 정책을 복제하지 않는다.
- `zone_layer_projection_explicit_map_runtime`는 `life: Long <- hp: Int`와
  `label: String <- name: String`을 사용한다. 따라서 단순 type-string equality나
  same-name fallback으로는 통과할 수 없다. Self MIR과 native MIR의 semantic row는
  producer-local 숫자 ID를 제외하고 일치한다.
- Production direct-source self C와 explicit self-MIR C는 byte-equal이고 exact
  `life <- hp`, `label <- name` assignment를 방출한다. Self C, native C, native LLVM
  실행은 모두 `7`과 `dst`를 출력했다. no-map, type mismatch, missing source,
  duplicate target은 artifact 전에 실패한다.
- 이 좁은 explicit effect/relation eager map 경로는 실제 C-owned oracle 경로를
  대체하므로 `SUBSTITUTING`이다. 전체 family는 self semantic fact가 아직 MIR
  boundary에서 생산되고, declaration-level source ID, materialization/dirty/epoch/
  lifecycle과 하나의 native/self shared plan이 없으므로 `BRIDGE`다.
- `tobject`부터 `action`까지의 best practice는 nominal 승격이 아니라 경계별
  protocol이다: tobject는 detached transfer, object는 local observation, vessel은
  stable owned state, subject는 authority identity, action은 observable transition을
  소유한다. 모든 경계는 `semantic owner -> typed fact -> lossless carrier -> one-time
  admission -> last consumer -> negative gate`로 닫는다.
- 다음 falsifier는 `world_zone_projection_visibility`다. renamed explicit map 자체는
  이번 owner를 재사용하되, 먼저 world/intent source가 self semantic artifact를
  통과해 production consumer에 도달해야 한다. native MIR graft나 축소 fixture로
  reachability blocker를 숨기지 않는다.
- Focused runtime/component/object-action/build-MIR inventory/keyword/documentation
  gate와 targeted C/LLVM compare가 green이다. SoT edge 감사에서 기존
  `SFDomainRuntimeAssignment`의 Coq authority projection 누락도 닫아 최종
  `CLOSED=34 BRIDGE=25 ACTIVE=0`을 확인했다. `rocq`/`coqc`가 없어 proof compile은
  명시적 skip이며 live owner/consumer·negative mutation 검사는 통과했다.

## 2026-07-28 exact domain runtime assignment executable checkpoint

- `semantic.domain_runtime_assignment`가 effect bearer, relation source/target,
  projection target/source member path의 exact declaration·field ID/name/type을
  한 번 결정한다. HIR/DIR/MIR과 `pgy.mir.v1`은 이 사실을 lossless하게 운반하며,
  self source producer도 자기 identity epoch 안에서 같은 typed row를 만든다.
- Self machine admission은 topology와 runtime row를 한 번 exact join해
  target-neutral plan receipt를 만든다. 이후 owner lookup과 codegen view는 plan이나
  프로그램 graph 전체를 다시 검증하지 않는다. 3 GiB 부근의 과거 결함은 이미
  sealed graph를 writer/accessor가 반복 검증한 owner-lifetime 오류였으며, 병렬 full
  build의 process peak 합산과도 구분한다.
- Native C/LLVM은 runtime sync에서 same-name/nested-name 재탐색, missing-source
  zero-fill과 effect/relation 0/1 ordinal 결정을 제거하고 exact MIR fact를 소비한다.
  Self general C는 admitted plan으로 다섯 assignment를 방출한다:
  bearer, effect view, relation source/target, relation packet sync다.
- Production `CompileSourceToCVerified`의 direct source 경로와 explicit self MIR
  경로는 byte-equal C를 만들었고 native C, native LLVM, 두 self C 실행 모두
  `7`과 `dst`를 출력했다. Missing/duplicate/foreign role·member·directive,
  operation/topology mismatch와 identity-epoch drift는 partial C 없이 실패한다.
- 이 implicit-map eager method-entry bind/sync slice는 실제 C-owned 결정을
  대체하므로 `SUBSTITUTING`이다. 전체 object-to-action runtime은 explicit map,
  declaration-level source identity, pool/materialization, dirty/epoch/lifecycle,
  native/self shared-plan 부채 때문에 `BRIDGE`다. Self compiler 내부에서
  `effect`/`relation` action graph를 개사료로 썼다는 주장과도 구분한다.
- 다음 executable falsifier는 `world_zone_projection_visibility`의
  `label <- displayName`, `user <- displayName` explicit map이다. Self parser가
  map syntax identity를 보존하고 semantic assignability를 다시 추정하지 않은 채
  exact path row로 general C까지 운반해야 한다.
- 최종 소스에서 `make -j2 all`로 compiler/LSP를 함께 링크했고 focused self
  runtime gate, native C/LLVM `7`/`dst`, component cap, build/MIR inventory,
  performance contract, object/action contract, 144-row keyword registry와 SoT
  owner/adequacy gate가 모두 통과했다. Performance gate의 마지막 관측 compile은
  396ms였다. Coq compile만 `rocq`/`coqc` 부재로 선언된 skip이며 다른 성공과
  합치지 않는다.
- Gate 감사 중 기존 모순도 함께 닫았다. Full DRV-2 shell guard는 `/usr/bin/bash`
  하드코딩과 Bash 4 associative array를 버리고 actual `command -v bash` 및 Bash
  3.2 indexed-array lookup을 사용한다. LSP completion의 27-item registry projection
  process cache는 명시적 한 owner로만 allowlist되며, MIR/perf inventory는 삭제된
  name/ordinal/refresh-metadata helper를 다시 요구하지 않고 exact runtime fact
  view를 negative ratchet으로 고정한다.

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
  없다. artifact receipt/failure는 모두 payload까지 소비되며,
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
