# Pergyra — 현재 진행 상황

마지막 업데이트: 2026-08-02

## 활성 우선순위 — vessel generic-member 실행 경로 치환

- 실행 체크포인트 `e24d5652`에서 두 번째이자 마지막 연속 SoT 승격을 닫았다.
  Constructed `Array<Int>`와 `Array<Point>`는 이제 하나의 target-neutral
  `DirectMirAggregateValueFlowFact`와 선택된 target projection을 소비한다.
  두 family plan/emitter가 representation, storage/index, call receipt,
  allocator/lifetime/carriage를 다시 결정하는 경로는 negative-gated다.
- ABI provenance를 합치지 않았다. `Array<Int>`는 실제 captured physical ABI
  receipt를 사용한다. `Array<Point>`는 새 absence owner가 admitted MIR의
  `layout_id=0`, `required=false`, `layout=null`을 검증하고 typed result capture와
  결합한 digest를 `aggregate_flow.abi_evidence_id`로 전달한다. `point_abi.digest`는
  Point 원소 ABI일 뿐 Array 부재 증거가 아니다.
- 두 lane의 C/LLVM artifact는 승격 전과 byte-equal이다. Focused gate는 exact
  `44`/`45`, 여섯/일곱 invariants, 각 세 variants, 27/35 C negatives와 7/10 LLVM
  sentinels를 통과했다. Hard contract, full component, installed public C와
  runtime-free LLVM compile/run도 green이다.
- 현재 설치 드라이버는 4,077,599 bytes, SHA-256
  `A8F0F563A79CB87CDEB2052742FDA0472980CA67A4700FCF7B13C2F73E5140C6`다.
  Current-source parse/codegen과 동일 release-flag host compile/source smoke는
  성공했다. Codex runner의 nested build shell은 GCC temp를 보호된 `C:\Windows`로
  해석해 wrapper 마지막 단계가 red였으며, repo temp를 지정한 동일 C 직접
  compile은 66.9초였다. Pressure는 재지 않았다. Full CI, Coq/Rocq adequacy,
  bootstrap fixpoint와 current-source gen2==gen3도 재실행하지 않았다.
- 첨부 8월 2일 architecture review의 topology-specific mini-compiler 경고는 이번
  공통 value-flow 승격으로 받아들였다. Pair/Array<Point> 미구현과 3.65MB driver
  평가는 관찰 HEAD `bac9b3f1` 기준이라 현재 사실에는 그대로 적용하지 않는다.
  portable external call ABI와 general aggregate legalization은 여전히 열린 문제다.
- 다음 objective는 실제 C-owned 실행 경로 치환이다.
  `generic_vessel_member_inferred_flow.pgy`의 source-to-MIR은 성공하며 결과는
  6,527 bytes, SHA-256
  `367AA5B544912E9735B42F9C16222A6424432D88FD308CAC6258E823D0229DD5`다.
  현재 direct C와 LLVM은 모두 artifact 전에
  `direct MIR two-routine classification is invalid`로 fail-closed한다.
- 기존 inferred generic-member owner를 passive `vessel Cell`에도 재사용하되
  class/vessel host identity는 명시적으로 보존한다. Fixture/type-name dispatch,
  vessel 전용 plan/emitter, 실패 후 다른 planner retry, native C fallback은 금지한다.
  같은 MIR이 C/LLVM의 실제 generic-member definition과 nested calls를 거쳐 exact
  `42`를 실행하고 host-kind/target/use negatives가 artifact 전에 거부되는 것이
  다음 falsifier다. 다음 커밋은 반드시 executable replacement delta여야 한다.

## 비활성 진행 기록 archive

### 이전 첫 multi-routine Array parameter carriage

- 실행 체크포인트는 `f8e91764`다. Installed public C/LLVM artifact·compile·run은
  Pergyra-built sibling driver가 소유하는 target-specific `SUBSTITUTING` 경로다.
  전체 compiler 치환 완료를 뜻하지는 않는다.
- `array_return_literal.pgy`의 두 routine은 이제 source-to-MIR 한 번과 같은
  6,267-byte MIR의 C/LLVM projection을 거쳐 정확히 `4\n3\n`을 출력한다.
  Producer는 caller-owned fixed storage만 채우며 LLVM runtime reference는 0이다.
- Program identity owner는 exact-one `Main`, unique header field, zero-parameter
  signature, typed direct callee와 syntax identity를 row 순서와 무관하게 봉인한다.
  Plan owner는 producer literal, caller definition/use, reachable terminal block,
  blank Log scalar fact, target capability, ABI와 lifetime을 한 번 결합한다.
- Local/returned Array plan은 하나의 canonical captured `Array<Int>` ABI owner를
  공유한다. 모든 field offset/size/align을 확인하며, layout ID까지 올바르게
  재계산한 잘못된 field shape도 artifact 전에 거부한다.
- Multi-routine 분류는 어떤 row-zero shape read보다 먼저 일어나며 hello, scalar,
  local Array, Option, CFG planner로 재시도하지 않는다. Routine-order swap은
  C/LLVM artifact 모두 byte-equal이다.
- Focused gate는 entrypoint, graph-valid unresolved callee, producer kind/return
  누락·중복·변경, caller definition/use, ABI offset, repaired-ID field shape,
  unreachable/nonterminal block, forged Log result의 13개 변조를 거부한다. 이
  실행 gate는 LLVM self-host preparation parity aggregate에 연결됐다.
- 최종 installed driver는 3,560,729 bytes, SHA-256
  `350A39D1DA6800657B24A5423B104057B4CFE33787AEDFE0F0442131ABC03EF3`다.
  Current-source DRV-2 rebuild는 93.9초였고 메모리는 계측하지 않았으므로 이전
  peak를 이 binary에 붙이지 않는다.
- 최신 green은 Array-return C/LLVM parity와 13 negatives, 기존 local Array,
  installed LLVM, hard contract, full component ratchet이다. Full CI, Coq adequacy,
  current-source gen2==gen3 fixed point는 이번 checkpoint에서 실행하지 않았다.
- 다음 활성 fixture는
  `src/self_hosted/mir_lower/fixture/array_literal_call_argument.pgy`다. `Double`,
  `SumPair`, `Main` 세 routine이 fixed `Array<Int>` literal을 typed parameter로
  전달하고 nested scalar call을 수행해 정확히 `11`을 출력해야 한다. 아직 완료
  증거로 올리지 않는다.
- 다음 owner는 strict routine/signature, typed call target, parameter carriage,
  Array ABI, argument/result/use와 nested expression graph를 한 target-neutral
  plan으로 결합해야 한다. Name/row special case, call flattening, unowned raw
  pointer, C-only reconstruction, native re-entry, 이전 2-routine plan retry는
  금지한다.
- 다음 falsifier는 source-to-MIR 한 번, 동일 MIR의 C/LLVM projection, exact `11`,
  routine permutation과 parameter type/carriage·call target·argument use·result·ABI
  negative다. General query engine이나 dynamic Array로 범위를 넓히지 않는다.
- 메모리는 semantic target당 마지막 maximum만 기록한다. 2.4 GiB attention과
  3 GiB hard stop을 유지하되 threshold 아래 실행을 최적화 이유로 삼지 않는다.

### 이전 첫 multi-routine Array-return checkpoint

- 실행 체크포인트는 `76867abd`다. Public C artifact/compile/run과 sealed
  runtime-free Option 및 local `Array<Int>` LLVM compile/run은 installed
  `pgy-self-driver`가 소유한다. Native semantic/AIR/codegen/libLLVM fallback은
  닫혀 있다.
- `array_literal_assignment.pgy`는 source-to-MIR 한 번, typed expression graph,
  target-neutral Array plan, 선택된 ABI projection과 C/LLVM emitter를 거쳐 두
  backend 모두 정확히 `3\n10\n`을 출력한다. LLVM artifact의 `@pgy_` runtime
  reference는 0이다.
- Array owner는 literal spine, assignment target, `ArrayLength`, index/add graph와
  local/result identity, element vector, latest SSA use, canonical layout, target
  capability를 한 plan으로 봉인한다. Instruction kind/source type은
  `MirProgramRoutineIndex`가 소유하며 display용 blank scalar field를 권위로 쓰지
  않는다.
- Element kind, index kind, length target, stale SSA use, ABI offset, source type,
  unsupported static index의 일곱 변조는 artifact 전에 실패한다. 같은 MIR을 C와
  LLVM이 각각 한 번 소비하고 focused hash는
  `9D056A3A9D9063207B9CD3A871E81E60684C0637A3CC4AA870E06952499C618F`다.
- Public LLVM installed gate도 Array program으로 승격했고 정확히 `3\n10\n`을
  실행한다. Exactly-once, stale-output, missing/malformed/failure negative와
  no-native-fallback은 유지한다. `clang -x ir`만 마지막 host boundary다.
- MSYS bootstrap의 import-composed source 입력은 repo-relative로 고쳤다. 기존
  absolute spelling은 native compiler의 source authority check에서 거부됐으며,
  output path와 cache identity는 별도 owner로 계속 명시한다. Structural ratchet이
  absolute source invocation의 재도입을 막는다.
- Refreshed codegen seed-only는 410.451초, peak working/private 2.705/2.841 GiB로
  완주했다. 2.4 GiB attention은 넘지만 3 GiB hard stop 아래다. Intermediate
  driver build는 98.359초, 1.579/1.684 GiB였고 이후 작은 rebuild는 계측하지
  않았으므로 그 수치를 최종 binary peak로 확장하지 않는다.
- Installed driver는 3,528,807 bytes, SHA-256
  `D3CDA2D90E2018F453DCA8ACE7B374F21E5B62EF7F4DFCB281282D1F86D2BE52`다.
  Refreshed gen2 seed는 2,257,728 bytes,
  `BD6D3E074885CCA4C8308F873A212A04DDF4DD22E1C7244E22963B041ADCF28D`다.
  현재 seed capability는 확인했지만 이번 checkpoint에서 gen2==gen3는 다시
  실행하지 않았으므로 이전 fixed-point 기록을 최신 결과로 재사용하지 않는다.
- 다음 단일 rung은 `array_return_literal.pgy`다. `Build() -> Array<Int>`와 이를
  호출하는 `Main` 두 routine이 있으며 기대 출력은 `4\n3\n`이다. Installed
  source-to-MIR는 한 번 성공해 6,267-byte MIR
  `8AFFE11FE23F78554980FCCAA62E1DE8F024F679EC496702736FC0C47669D6DD`를 만들었지만
  direct LLVM은 artifact 전에 fail closed한다.
- 실제 blocker는 Array emission이 아니라 `admitted.routines[0]`와 첫 routine의
  shape를 기본값으로 쓰는 dispatch다. `MirProgramRoutineIndex`의 entrypoint,
  routine identity, declaration, typed call/return fact를 한 multi-routine plan이
  소비하고, `Build` return을 row-order 복원 없이 `Main`에 전달해야 한다.
- Source-name guess, first-routine default, C-only call reconstruction의 LLVM 복제,
  scalar/hello retry, native re-entry, broad query engine은 금지한다. Query engine,
  Insere/Zeno provenance, unrelated SoT 정리는 현재 blocker가 아니다.

### 이전 source-to-MIR checkpoint — 비활성 역사 자료

- 활성 실행 경로는 driver_bootstrap_main.pgy, PgyCompilerWorld.source_mir,
  DriverSourceMirExecution, DriverRung2MirProjectionFromAdmittedAnalysisObserved,
  SelfMirProgramFactsFromReadyArtifactObserved 하나다.
- SelfMirProgramFacts가 immutable semantic expression graph를 한 번 소유하고,
  instruction row는 root와 bounded range handle만 보유한다. Program instruction
  index는 routing/text/graph의 borrowed bounds를 소유하며 graph target을 다시
  문자열로 복원하지 않는다.
- 누적 expression graph append는 이전 call-return type vector를 그대로 이어받아
  새 node 한 개만 추가한다. append/target projection에서 전체 arena를 다시 만들거나
  Ready를 반복하는 경로는 ratchet이 거부하고, 최종 fact owner만 한 번 검증한다.
- artifact 모드는 전체 MIR JSON 문자열을 만들지 않고 검증된 program facts를
  SelfMirProgramJsonWriteArtifactVerified로 원자적 스트리밍한다. stdout 모드만
  실제 payload 경계이므로 문자열 materialization을 유지한다.
- 고정 90,304,012-byte MIR consumer에서 수정 전 r54는 graph row 12,288 이후
  311.431초에 private 3.009 GiB hard stop에 도달했다. 누적 graph 재구성을 제거한
  r55는 900초 timeout 동안 private 0.965 GiB, working set 0.904 GiB였고
  row 28,672까지 진행했다. 메모리 결함은 닫혔지만 완주 증거는 아니다.
- r56은 같은 결과를 더 오래 기다리는 것이 구현 진척을 막는다고 판단해
  row 40,960 이후 중단했다. timeout 연장이나 반복 실행을 진척으로 세지 않는다.
- 길이가 이미 봉인된 문자열 구간은 새 SubstringWithLen builtin이 한 번 복사한다.
  C/LLVM runtime, native type/codegen, self-host signature가 같은 surface를 소유하며,
  bounded JSON의 unescaped string과 number token은 문자별 heap string을 만들지 않는다.
- 일반 self-host emitted-C 빌드는 release가 기본이다. 플래그는
  -O3 -fwrapv -fno-strict-aliasing이다. PGY_SELFHOST_CC_PROFILE=test만
  명시적으로 -O0을 선택하며 고정점과 전수 테스트 시간은 일반 빌드 시간과
  분리해 기록한다.
- SubstringWithLen을 포함한 현재 codegen seed-only 생성은 400.6초에 exit 0이었다.
  새 Pergyra-built gen2와 self parser가 생성됐다. 첫 bounded driver 실행은
  builtin registry row 수를 별도 숫자 124로 복제한 readiness에서 fail-closed했다.
  숫자 mirror를 제거하고 owner projection을 유일한 row-count 사실로 유지했으며,
  새 C/LLVM readiness parity gate가 숫자 mirror 재도입과 SubstringWithLen row
  누락/중복을 거부한다.
- 수정 후 bounded production driver는 534.4초에 exit 0이었다. Pergyra-built
  driver와 native oracle의 sample C, bounded MIR JSON, MIR-to-C artifact가 모두
  byte-identical하다. 관찰된 약 0.967 GiB RSS와 긴 구간은 native oracle
  재컴파일 비용이며, 3 GiB 누적 graph 재검증 결함의 재발이 아니다.
- O0 Windows 경로는 ApplyPostfixFact의 중첩 lowering에서 큰 생성 C stack frame
  때문에 routine 397 부근에서 stack overflow가 난다. Release 경로는 완주하며,
  stack 상향은 test-profile 결함의 해법으로 인정하지 않는다.
- 현재 증거 등급은 REACHABLE이다. bounded codegen gen2와 gen3 고정점은 있지만
  기본 배포 compiler의 전체 Pergyra 치환율은 아직 0퍼센트다.
- 다음 falsifier는 현재 green seed를 다시 만들지 않고 동일 full source-to-MIR
  target을 pressure owner 아래 정확히 한 번 실행하는 것이다. 완주하면 native
  oracle byte parity를 확인하고 current-source gen2==gen3로 진행한다.
- 메모리는 semantic target마다 한 번 실행한 뒤 최종 peak_private_gib와
  attention_required만 읽는다. 3 GiB hard stop과 2.4 GiB attention을 유지하며
  미완주 target을 timeout만 늘려 반복하지 않는다.
- 최신 focused green은 fresh Pergyra gen2/parser seed, bounded production-driver
  sample/MIR producer/MIR consumer parity, builtin-signature readiness C/LLVM parity,
  native pgy 증분 build, SubstringWithLen C/LLVM parity,
  bounded JSON C/LLVM exact-bound parity, runtime-call ABI C/LLVM artifact parity,
  expression-graph projection/persisted-read, MIR routine index, self-parser
  acceptance다. Structural component contract는 graph/JSON slice까지 green이며
  마지막 ABI 추가 후에는 shell syntax, line cap, owner acceptance만 확인했다.
  Filtered self-host codegen은 tool build가 300초 안에 끝나지 않아 PASS가
  아니지만 fresh bounded seed가 실제 mapping을 소비했다. Full matrix나
  current-source fixed point PASS는 추론하지 않는다.
- Insere와 Zeno 등 외부 프로젝트는 비활성 provenance다. 사용자가 명시적으로
  다시 열기 전에는 현재 self-host TODO나 진척으로 취급하지 않는다.

### 더 이전 진행 기록 archive

이 절 아래의 모든 날짜별 checkpoint는 과거 증거와 회귀 falsifier를
보존한다. 활성 TODO 또는 재개 후보가 아니며, 다음 작업은 위의
`multi-routine LLVM 프로그램 graph 합성`에서만 선택한다.

## Historical — 2026-07-30 exhaustive self-host CI and executable-rung closure checkpoint

- GitHub run `30535237959` separated five failures that had previously been
  hidden behind focused gates: imported enum variants were missing from the
  lightweight semantic callable SoT, standalone sources relied on transitive
  imports, the generated language-word inventory was stale, two platform gates
  still read pre-zone-sync owners, and full bootstrap exposed an action-only
  DIR intent-step contract.
- Canonical enum callables now carry qualified identity, payload signature and
  enum return type. The scanner admits comma-separated and newline-separated
  variants, strips labels from payload types, excludes enum methods, and emits
  no partial rows for malformed declarations. The semantic fixture frontier is
  114 and includes a fail-closed qualified missing-variant case. Delimited
  comma scanning no longer treats a spaced comparison `<` as a generic opener;
  nominal constructors consume `let mut`; direct consumers name their imports;
  one intentional recursive semantic cluster maps to its checker root instead
  of adding a circular import.
- The current semantic-target manifest contains 684 real self-host sources;
  all 684 were accepted by the C semantic checker. The production-header
  census is 717 and its self-host C/LLVM checker is artifact-equal; the
  C-focused memory-concurrency model follows the canonical zone-sync ABI owner.
  These results prove source acceptance, not full bootstrap.
- The documented nested-intent seam now carries an expression-graph call spine
  into DIR instead of reparsing the `on` text. A fresh self-host driver emitted
  and compiled through C, then matched native domain graph identities for the
  single-step, `FrontendPipeline -> IntakeSource -> SourceUnit.Read`, and
  two-step fixtures. Missing, wrong-arity and ambiguous nested targets fail
  before a partial MIR artifact. This is `REACHABLE`, not `SUBSTITUTING`.
- Exact semantic authority rows now carry authority, owning zone, subject slot,
  and required-ability node/name identities into DIR. The old DIR
  `TypedAstKindZoneAuthorityTag` rescan is forbidden; missing or duplicate
  identity mutations fail closed. This also remains a `BRIDGE` while the
  production MIR authority transition and shared zone-sync runtime plan are
  open.
- Native and self-host parsers now defer intent parameter role classification
  until the complete declaration/import graph exists. Neutral `IntentBinding`
  rows are resolved once to `IntentInvolves` or `IntentValue`; source-order
  imports and suffix guesses are not authorities. The self resolver only
  recognizes indentation-anchored AST labels so its own contract strings are
  not mistaken for unresolved parameters. The native cross-module positive and
  unresolved negative gate passes, and the actual bootstrap driver AST resolves
  `SourceIntakeZone` as involved and `StagePathManifest` as a value. The focused
  self resolver reproduction passes, but a whole-driver self-parser run was
  policy-stopped after 1,532.042 seconds with no artifact; last observed private
  memory was 717,144,064 bytes. It is CPU/incomplete evidence, not a full-parser
  PASS or a memory blow-up.
- The attached architecture review's memory conclusion is supported, but its
  CPU warning is not superseded. The latest full integration attempt reached
  `mir-facts:start` and was stopped after 2,534,272 ms with 2,284.8 MB peak
  private memory. No 20 GB compiler process was reproduced; the run is an
  incomplete CPU timeout, not a semantic pass or memory regression. The next
  performance rung must profile that current owner path and define stable query
  keys before adding a cache, opaque artifact, or general query engine.

## 2026-07-30 installed source-to-MIR one-graph checkpoint

- 실제 사용자 경로를 다시 추적해 `bin/pgy --self-driver`가
  `driver_bootstrap_main.pgy`가 아니라 설치된 `pgy-self-driver`의
  `driver_rung2_main.pgy -> driver_rung2_cli_owner.pgy`를 실행한다는 숨은 두 번째
  source-to-MIR orchestration을 발견했다. 설치 CLI의 직접
  `CompileSourceToMirJsonVerified` 호출을 삭제했다.
- 설치 CLI와 full bootstrap artifact root는 이제 모두
  `PgyCompilerWorld.source_mir -> DriverSourceMirExecution` 한 owner를 지난다.
  Installed stdout은 `io_read` 전용 `ProduceSourceMir`, bootstrap artifact는
  `io_read, io_write`의 `PublishSourceMirArtifact`를 호출한다. 둘은 같은 payload
  admission owner를 소비하므로 compile/identity/pressure 결정은 복제되지 않는다.
  빈 경로 sentinel, 임시 파일 왕복, caller의 direct compile/commit은 금지된다.
- 물리적 stage 폴더는 서로 다른 lexer/parser/semantic/MIR fact lifetime을
  보존하므로 합쳐서 한 파일로 만들지 않는다. “프로그램 그래프 하나”의 의미는
  설치 root와 bootstrap root가 같은 world/action owner를 소비하고 독립 결정을
  갖지 않는다는 뜻이다.
- Static installed-link/action no-bypass gate, compiler-world contract와 두 entrypoint
  AST parse는 PASS했다. Full staged-array build는 5,101,206ms 뒤 explicit
  `initializer_type_unresolved`로 설치 전 실패했지만 peak private 1,469.2MB,
  working set 1,301.8MB로 3GiB/20GiB memory regression은 재현하지 않았다.
  불필요한 `Clone(admitted.intent_execution_plan)`은 제거했다. 중간의 typed local은
  old gen2 inference 오류를 없앴지만 current compiler의 borrowed-member escape
  규칙에 어긋났으므로, 최종 source는 admitted member를 typed value parameter로
  projection한다. `own` 권한 확대, detached local, Clone과 consumer 재검증을
  ratchet이 거부한다. Existing seed를 쓴 intermediate install-only rerun에서는
  이전 initializer 오류가 재현되지 않았지만
  4,605,377ms 뒤 peak private 3,072.0MB, working set 2,820.5MB에서 고정 pressure
  owner가 중단했다. `driver.c`는 0바이트이고 설치 바이너리는 갱신되지 않았으므로
  install/launcher parity는 PASS가 아니다. 다음 executable falsifier는 admitted
  semantic-analysis receipt를 emission이 소비해 whole-artifact fact 재구성 횟수를
  0으로 만드는 것이며, 제한 상향은 해법으로 인정하지 않는다. 최종 typed-value
  source의 focused driver rebuild는 성공했고, broader machine-layer gate는 별도
  기존 producer/consumer fact mismatch에서 RED다.

## 비활성 provenance archive — 2026-07-30 Insere/Zeno

이 절은 완료된 설계 출처 기록이다. 활성 self-host rung이나 재개 큐가 아니며,
사용자가 명시적으로 다시 열지 않는 한 여기서 후속 작업을 고르지 않는다.

- 사용자 소유 `F:/insere`와 `F:/zeno`의 채택 계약을
  `docs/201_insere_zeno_lineage_and_library_adoption.md`에 revision까지 고정했다.
  외부 source는 provenance/falsifier이고 Pergyra semantic SoT는 아니다. Zeno의
  기존 dirty package/`llms.txt` 작업은 수정하지 않았다.
- Insere의 latest-only start admission은 기존 `HostTaskSlot` 위의 typed
  `spawn`/`restart`/`skip` policy로 구현했다. Active skip/spawn은 generation을
  보존하고 restart만 증가시키며, vacant 세 정책은 같은 next generation을
  시작한다. Generation 0/negative/상한과 unknown phase는 fail closed하고,
  `Phase`가 malformed fact를 `vacant`로 위장하지 않는다.
- Zeno-derived `SnapshotTicket`/`BinaryProjectionPreflight`는 existing MIR layout
  identity와 explicit endian을 소비하는 internal tooling `REACHABLE` slice다.
  Snapshot ticket 값은 아직 caller-provided이므로 live `SlotHandle` authenticity나
  production consumer 증거로 세지 않는다.
- 다음 후보는 start admission과 분리된 bounded post-failure supervision,
  `MirAbiLayoutRowCapture` 단일 owner에서 파생하는 normalized manifest diff,
  기존 diagnostic/loss-contract owner에 붙는 resolution/loss evidence다. 새
  scheduler, Zeno hash, backend별 layout table, 새 error taxonomy는 금지된다.
- 이 작업들은 공식 library/tooling 채택이며 C-owned compiler path를 삭제하지
  않았으므로 hard self-host `SUBSTITUTING` 진척으로 세지 않는다.

## 2026-07-29 source-to-MIR compiler-world action checkpoint

- Production artifact `--emit-mir-json-verified`는 이제 `Main ->
  PublishSourceMirArtifactThroughPgyCompilerWorld -> PgyCompilerWorld.source_mir ->
  DriverSourceMirExecution.PublishSourceMirArtifact` 한 경로를 지난다. World는 기존
  `direct_mir` 다음에 `source_mir`를 두며 한 composition owner가 두 zone을
  정확한 positional arity로 한 번 materialize한다.
- 새 action은 subject/topology identity와 pressure mode를 admit하고, 기존 typed
  source-to-MIR producer 중 하나를 한 번 호출한 뒤 shared artifact transaction을
  한 번 commit한다. Full driver만 pressure-observed 요청을 허용하고 일반 fixture의
  pressure 요청은 fail closed한다.
- 옛 `CompileSourceToMirJsonFileVerified` 및 pressure variant 정의/호출과 Main의
  source-MIR 직접 compile/commit은 삭제됐다. Static action/no-bypass,
  build-pressure, topology, compiler-world, Pergyra-likeness, component, hard
  self-host, progress-metric contract는 PASS했다.
- Grade는 `REACHABLE`, not `SUBSTITUTING`이다. Production caller가 action과 typed
  outcome을 실제 소비하지만 새 C-owned semantic path가 아니라 Pergyra 내부
  file-helper orchestration을 교체했다. Root intent는 계속 `SURFACE`다.
- `function_clause_order_minimal` production 실행은 현재 120초 C driver build
  ceiling에서 산출물 없이 timeout되어 아직 PASS를 주장하지 않는다. 같은 시도 뒤
  잔류 compiler worker는 없었고 LLVM leg는 C prerequisite가 없어 시작하지 않았다.

- The same checkpoint repaired CI contract drift without restoring old owners:
  MIR/region link inventories are complete, doc-link census parity is current,
  intent diagnostics follow their split owners, secure pin evidence requires
  the typed init ABI, and Windows vocabulary preparation receives the exact
  configured `PGY_BIN`. Focused gates, `make -j2 test-mir` (157/0), the default
  50,000,000-iteration evidence benchmark, and the full perf contract pass.
  The next pushed platform matrix remains the authority for cross-host closure.

## 2026-07-29 typed intent execution plan v2 admission checkpoint

- Intent 의미는 action 개수로 판정하지 않는다. `docs/01`·`docs/173` 정본대로
  현실 목적을 닫고 participant/coordination/authority/effect/boundary/
  compensation/trace fact를 검증 평면에 귀속하는 source-level binder다. 이번
  plan은 그중 coordination/boundary/compensation 실행 subfact의 bounded
  projection이며 universal intent owner가 아니다.
- Native semantic→AST→DIR→MIR→JSON과 self admission 전 구간이 step success/
  failure 및 terminal source/result의 payload `tobject` declaration syntax ID를
  운반한다. Schema는 `pgy.selfhost.mir-intent-execution-plan.v2`이며 v1 name-only
  payload join은 dual-read fallback 없이 제거했다.
- Canonical v2 plan은 2 steps/3 terminals, digest `1268084794`; multi-routine
  plan은 digest `1173492658`이다. Fresh native build와 MIR 157/157, native C/LLVM
  typed transition execution, self in-memory fact gate, canonical/multi JSON
  admission이 PASS했고 41개 schema/identity/topology mutation이 partial C 전에
  fail-closed했다.
- `MirIntentExecutionPlanReady` full validation은 machine admission에서 한 번만
  실행된다. Codegen/compiler consumer의 Ready/Digest/step/terminal 재검증과
  recursive expression-graph 재구성은 static gate가 거부한다. Fresh self-driver
  rebuild의 관측 동시 private 표본은 `pgy` 약 791MiB + `cc1` 약 739MiB +
  `gcc` 약 1MiB, 합계 약 1,531MiB였다. 두 fresh 반복 모두 같은 범위였으며
  20GiB 회귀는 없었다.
- Admitted general self C는 production driver의 단일 admission receipt에서 v2
  plan을 소비한다. Persisted plan graph는 `{root,digest,nodes}` exact seal을
  요구하고, action/enum/tobject/instruction은 syntax ID와 nominal kind까지
  exact-join한다. 동명 foreign subject action, digest 누락/zero/extra, missing
  target, reachable zero-compensation scaffold를 모두 partial C 전에 거부했다.
- Fresh Pergyra-built driver에서 success, failure A/B, predecessor-only reverse
  compensation, multiple/duplicate expression, zero compensation의 direct/admitted
  self C parity가 PASS했다. 이 bounded input-language MIR→self C 경로는 실제
  옛 typed direct/rollback 우회를 제거했으므로 `SUBSTITUTING`이다. 그러나 실제
  compiler 목적을 닫는 production root intent는 아직 entrypoint에서 호출되지
  않으므로 compiler 조직으로서의 `intent`는 계속 `SURFACE`다.

## 2026-07-29 native typed intent plan 실행 checkpoint

- `tobject`는 이번 경계의 해법이 맞지만 역할은 명확히 제한했다. Action은
  immutable detached `Receipt`/`Problem` payload를 `enum<tobject>`로 반환하고,
  MIR transition plan은 step identity, predecessor, success-only completion,
  compensation 순서와 terminal control flow를 소유한다. `tobject`가 권한이나
  실행 그래프를 소유하는 우회는 금지한다.
- Native semantic/DIR에서 확정한 exact enum/variant/payload와 intent return type이
  이제 `MIRIntentExecutionPlan`으로 내려간다. Declaration stable syntax ID,
  predecessor transition ID, branch-local payload definition, completion,
  ordered compensation, terminal result identity를 한 번 검증하고
  `pgy.selfhost.mir-intent-execution-plan.v1` JSON으로 투영한다.
- Native C와 LLVM은 typed plan이 있으면 legacy Bool intent emitter로 떨어지지
  않고 그 plan을 직접 소비한다. Success, failure A, failure B가 서로 다른 exact
  payload를 반환하고, B failure는 완료된 A만 보상하며, 복수 보상은 양 backend
  모두 역순으로 실행된다.
- 통합 LLVM compiler rebuild, native typed execution gate, MIR 157/157,
  self frontend/fact/result-signature gate가 모두 PASS다. 후속 MIR smoke에서 찾은
  self JSON admission 회귀도 닫았다. 현재 wire는 participant/projection fact가
  모두 없으면 `domain_runtime_assignments` namespace 자체를 생략한다. Non-empty
  namespace는 `program_syntax_id`, `participant_roles`, `projection_members`의 exact
  3-key shape만 허용하며 top-level epoch와 각 row epoch를 교차 봉인한다. 임의 epoch를
  가진 explicit empty namespace와 non-empty stray row는 partial C 없이 fail-close한다.
- Self frontend/DIR/MIR은 `legacy_bool`과 typed result signature를 구분해 운반하고
  target-neutral execution fact owner도 schema/digest/fact 책임으로 분리했다.
  하지만 self top-level MIR JSON admission은 아직 routine result와 plan을 한 번
  index/cross-seal하지 못한다. 따라서 이 checkpoint는 native executable
  `REACHABLE`이며 hard self-host `SUBSTITUTING`은 아니다.
- 다음 executable falsifier는 self JSON reader가 native projection을 source/name/
  row-order 복원 없이 admit하고, admitted self C가 같은 success/failure/multiple-
  compensation gate를 통과한 뒤 production bootstrap의 C direct bypass를 지우는
  것이다.

## 2026-07-29 typed intent transition frontend + tobject boundary audit checkpoint

- `tobject`의 현재 핵심 방향은 맞다. action 경계를 넘는 immutable detached
  receipt/problem payload이며, step identity, predecessor, completion, compensation,
  authority, topology 또는 projection freshness를 소유하지 않는다. 새 native
  compiler로 `action_tobject_outcome_probe.pgy`를 C와 LLVM에서 실행해 두 backend
  모두 `ok=7`, `error=9`를 관측했고, `tobject_boundary_execution_owner.sh`도
  detached publication, constructor-input 차단, projection-source 재사용 차단,
  self MIR admission을 포함해 PASS했다.
- Typed intent frontend는 `intent ... -> Outcome`, `step B after A`, step-local
  success/failure variant payload, labeled success/failure terminal을 native AST/DIR와
  self parser/semantic/DIR까지 lossless하게 운반한다. `after`는 146-row language
  keyword registry에서 파생되며 lexer/self/LSP/VS Code projection gate가 PASS했다.
- `mir.intent_step_transition`과 `mir.intent_terminal_transition`의 self in-memory
  fact owner 및 mutation gate가 착지했다. 이 fact는 exact enum/variant/payload,
  explicit predecessor, success-only completion, compensation identity와 terminal
  coverage를 묶는다. 현재 증거 등급은 `REACHABLE` supporting fact이며
  `SUBSTITUTING`이 아니다.
- 실제 실행 frontier는 아직 OPEN이다. Native MIR JSON에는
  `intent_step_transition`, `intent_terminal_transition`, `depends-on` row가 0개이며,
  intent routine return signature도 아직 없다. HIR CFG는 outcome tag success/failure
  successor를 만들지 않고 native C emitter는 `RunWorkflow`를 `Bool`로 고정한다.
  따라서 다단계 fixture의 native C compile은 `_Bool` 대 `WorkflowOutcome` 불일치와
  branch payload `receipt_a` 미정의로 fail한다. Codegen이 AST를 다시 읽어 이를
  복원하는 우회는 금지한다.
- 다음 executable rung의 순서는 intent MIR routine return signature -> typed
  transition producer/JSON projection -> one admission read -> outcome-tag HIR CFG ->
  C/LLVM consumer -> success/failure A/B runtime parity와 malformed-MIR negative다.
- Canonical contract와 현재 implementation 사이의 별도 tobject debt도 남아 있다.
  Semantic은 아직 tobject 안의 passive `func`를 허용하고, immutable field-write
  검사는 단순 member target 중심이라 bare/nested/indexed mutation을 모두 닫았다고
  주장할 수 없다. 이 debt는 typed intent control-flow owner를 tobject에 떠넘기지
  않고 별도 fail-closed semantic closure에서 해결한다.
- 관측 gate: full LLVM-enabled native compiler build PASS, complete parser test PASS,
  typed frontend PASS, typed transition fact negatives PASS, existing single-step
  enum<tobject> self/native C/LLVM gate PASS, object/action boundary PASS, self-host
  component contract PASS. 이번 순차 gate 실행에서도 20 GiB 재검증 회귀는
  관측되지 않았다.

## 2026-07-29 intent predicate와 ordered compensation 실행 checkpoint

- 직전 fail-closed 경계가 실제 실행 경계로 대체됐다. Parser는 step별
  `guard`/`expect`/`post` singleton과 ordered `compensate` expression graph를
  보존하고 중복 singleton을 거부한다. DIR은 exact step/node/range를 소유하며 MIR은
  `IntentCheck(guard|expect|post)`와 `IntentEval(compensate)`를 순서대로 운반한다.
- Admitted general self C는 action 완료 뒤 `guard -> expect -> post`를 실행한다.
  실패하면 완료된 step을 역순으로, 같은 step의 compensate row도 역순으로 실행한다.
  성공 시 compensation은 0회이고, 첫 step guard 실패 시 미래 step과 그 보상은
  실행되지 않는다. B의 guard/expect/post 실패는 A와 B의 forward mutation을 모두
  복구한다.
- `intent_guard_post_compensation_execution_owner.sh`가 direct/admitted self C의
  byte parity와 self/native C/native LLVM runtime parity를 고정한다.
  `intent_phase_carrier_negative_owner.sh`는 unknown/orphan/wrong-step-or-slot phase,
  singleton 중복, check/compensate result-type 오염, on result/type 비대칭, missing
  expression graph의 9개 mutation을 exact diagnostic으로 거부하고 partial C를 남기지
  않는다. Parser frontend, 기존 enum<tobject> exact-once gate와 component gate도
  함께 green이다.
- `tobject` 결론은 바뀌지 않는다. detached immutable success/failure payload에는
  맞지만 step identity, phase order, completion, predecessor, compensation graph의
  owner가 아니다. 이번 실행 의미는 intent carrier와 emitter가 소유한다.
- 이 bounded input-language slice는 `REACHABLE`이다. Production bootstrap이 compiler
  intent를 호출하지 않으므로 compiler `intent`는 계속 `SURFACE`이며 hard self-host
  substitution으로 세지 않는다. 현재 self machine admission은 non-empty admitted
  domain runtime plan을 요구하므로 합법적인 empty topology는 별도 blocker다. 서로
  다른 step의 동일 action expression text를 전역 문자열 equality로 join하는 기존
  seam도 다음 negative debt로 남는다.
- 다음 executable rung은 typed variant branch, success-only completion, DIR-owned
  predecessor와 failure payload-driven compensation을 `mir.intent_step_transition`에서
  한 번에 닫는다. 일반 predicate failure의 current-step rollback과 action typed
  failure의 predecessor-only rollback을 합치지 않는다.

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
  완료됐다. Typed intent transition의 outcome binding/compensation은 계속 열린다.
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
