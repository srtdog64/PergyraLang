# Pergyra — 현재 진행 상황

마지막 업데이트: 2026-08-27

2026-08-27 Markdown-only push matrix isolation 로컬 구현: exact Git diff를
`scripts/ci_change_scope_owner.sh` 하나가 분류하고 `build-linux`는 모든 push/PR에서
계속 실행한다. 따라서 Markdown 계약도 기존 compiler build, `test-all`, self-host
preparation contract를 그대로 통과해야 한다. 나머지 28개 platform/sanitizer/
bootstrap/proof/backend job은 모든 변경 경로가 case-insensitive `.md`일 때만
생략된다. Missing base, empty diff, mixed change, non-Markdown rename/copy source는
full matrix로 fail closed한다. Synthetic Git history, CI profile, shell syntax,
`actionlint`, build-source inventory, documentation/progress/SoT gate, 실제
`c0632e4f..132a29d4` 6-path Markdown 분류는 local green이고 exact-head push가
다음 falsifier다. 이는 검증 피드백 개선이지 hard substitution 분자가 아니므로
SoT와 진행률은 변하지 않는다.

2026-08-27 source-C compiler-purpose intent 구현 `fb4acef4`: public installed
`pgy SOURCE --emit-c -o OUTPUT`가 이제 한 `CompilePergyraCArtifact` intent를
실행한다. `DriverSourceCExecution`은 기존 compile/transaction 권위를 유지하면서
typed outcome을 저장하고, intent는 Bool completion만 관측한다. World는 outcome 부재나
Bool/outcome 불일치를 거부하고 installed consumer는 정확히 한 성공 intent history를
요구한다. 기존 `PublishSourceCArtifactThroughPgyCompilerWorld` 및 world direct publish
경로는 삭제되고 정적 negative로 고정됐다. Compile/commit 복제나 native retry는 없다.

격리된 current-source Pergyra-built DRV-2가 installed/public C byte parity, C compile/run,
manifest 검증, missing-parent no-artifact transaction rejection을 통과했다. Native world
AST, topology, source scan, likeness (`Result`/`Option` 4393/4393, intent 15/15,
zone-bound 37/37), atomic transaction, shell syntax, hard contract도 green이다. 첫 격리
self-build는 trace block이 다음 source-MIR 함수로 잘못 들어간 scope 결함을 찾았고,
수리 뒤 gate가 source-C consumer 본문 안의 trace locality를 직접 검증한다. Full
component inventory는 실행하지 않아 green으로 세지 않는다. 이는 이미 Pergyra-backed인
source-C 내부 orchestration을 intent로 닫은 `REACHABLE` dogfood이며 새 hard replacement
분자는 아니다. SoT `50/35/1`, hard closure 58.1%, migration 78.8%, 통합 83%
(81~85%), strict beta 83%, hard replacement 75%는 유지한다. 이 로컬 구현 시점의
다음 falsifier는 publication과 exact-head CI였다.

Publication checkpoint `20e7da6e`의 exact-head run `33068411554`는 28/29로
끝났다. Linux fast build를 포함한 나머지 28개 job은 모두 green이고, 유일한 실패는
full self-host native oracle이 다른 모듈의 private intent를 호출한 가시성 위반이었다.
Intent를 `public`으로 바꾸자 MIR-only C inference가 action 안의 두 enum variant
constructor를 implicit self-method로 오인하는 다음 fail-closed 경계가 드러났다. Repair
`cb53b879`는 성공/transaction failure outcome 생성을 protocol owner 함수로
명시하고, world/likeness gate가 `[export] Intent`와 `public intent`를 올바르게 세도록
고쳤다. 동일 native oracle C emission과 C compile이 통과했고, 생성 oracle이 발행한
`hello.pgy` C도 compile/run해 정확히 `Hello, Pergyra!`를 출력했다. Focused source-C,
topology, native world, source scan, likeness 15/15, hard contract는 local green이다.
Full component inventory는 실행하지 않았고 replacement exact-head CI가 다음
falsifier다. 진행률과 SoT 수치는 그대로다.

Repair와 문서 checkpoint `c0632e4f`의 exact-head run `33071044311`은
34분 32초에 29/29 GREEN으로 끝났다. `build-linux`, full self-host fixed point와
policy corpus, codegen bootstrap, backend 20/20, Windows/macOS, sanitizer, TSan,
Rocq가 모두 통과했으므로 source-C intent takeover와 bootstrap repair는 같은
published revision에서 폐쇄됐다. 공개 launcher, REPL compiler call, package compiler
path에는 암묵적 `driver_run_pipeline` 호출이 남지 않았다. 남은 호출은 explicit
native oracle/opt-out이고, package manifest parsing과 unsupported RIR/AIR/HIR에는
완전한 Pergyra owner가 아직 없다. 따라서 source-MIR를 모양만 intent로 바꾸거나
query/cache, O(n^2) epoch rewrite, 별도 SoT 정리를 후속 rung으로 만들지 않는다.
fresh production compiler bypass, 기존 complete Pergyra owner, last consumer,
executable falsifier가 함께 관측될 때만 다음 hard-substitution rung을 연다. 수치는
SoT `50/35/1`, hard closure 58.1%, migration 78.8%, 통합 83% (81~85%), strict
beta 83%, hard replacement 75%로 유지한다.

2026-08-27 native formal/intent callable identity 구현 `e1ad082f`: native
`FuncParam`은 이제 AST node와 별개의 parser-owned stable declaration ID를 갖고,
semantic parameter symbol, MIR routine `source_syntax_id`, persisted expression
leaf의 `binding_syntax_id/kind/ordinal`까지 같은 ID를 운반한다. 기존 AST node
번호는 유지하기 위해 parameter ID는 한 identity epoch의 node walk 뒤에 배정한다.
Native serializer는 이름 기반 observability ABI 재탐색 대신 admitted semantic
callee/runtime ABI ID를 기록한다.

같은 실행 rung에서 self semantic final resolver의 function-only 조회를
function+intent exact declaration join으로 바꾸고, C global environment도 admitted
intent node ID를 `@declared_callable_syntax:<SyntaxNodeId>` 키로 실제 C symbol에
연결했다. `TypeId` 대용, name-based MIR repair, call-target-only 수용, native retry는
없다. Fresh v23 oracle은 12개 positive/unique formal ID와 39개 exact formal leaf를
검증했고, 설치 self-driver가 낸 C는 typed compensation/history의 32줄 출력을
정확히 실행했다. Formal/function/intent ID 및 digest 12개 mutation은 partial C 전에
모두 실패한다. Stable-identity와 source-scan은 green이다.

로컬 전체 v3 gate는 LLVM-disabled v23의 native LLVM leg에서만 중지됐고, 그 전
self runtime과 native C compile은 통과했다. 그러나 exact-head run `33053920579`는
네 개 공통 self-host job에서 native gen0가 self source의 예약어 지역 변수
`let intent`를 거부했다. 구형 self codegen seed가 이 문법 차이를 받아들여 로컬
installed-driver 증거만으로는 잡히지 않았던 결함이다. Repair `2c052d42`는 변수명을
`intent_index`로 바꾸고, 새 complete routine parameter ID schema에서 실제 routine
ID 하나를 제거하도록 bootstrap negative를 갱신했다. Repair source는 native gen0
parse `0 error(s), 0 warning(s)`와 gen2==gen3 73,161-line fixpoint를 통과했다. 갱신된
partial-ID artifact는 self/oracle MIR lower 양쪽에서 exit 1과 동일한 fail-closed
진단을 냈고, full codegen bootstrap은 lexer/parser/semantic/MIR lower/tool/fuzz
oracle parity를 모두 마쳐 `SELF-HOSTING OK`로 종료했다. Replacement exact-head CI
전이므로 원격 green은 아직 주장하지 않는다. Published base `b2f9a5ca`의 run
`33045433992`가 마지막 29/29
GREEN이다. Complete component inventory는 60초 예산을 넘어 green으로 세지 않는다.
Replacement run `33055970238`은 backend 20/20, Windows/macOS, sanitizer, TSan,
Rocq, toolchain, codegen bootstrap을 포함한 27개 job이 green이다. `build-linux`는
실행 테스트를 모두 통과한 뒤 hard-contract가 routine-aware expression graph emission
이전의 정확한 C 호출 문자열 세 건을 요구해 실패했고, full self-host job은 그 시점에도
진행 중이었다가 최종 green으로 끝났다. 따라서 이 run은 28/29이며 build-linux만
실패했다. Repair `5c722a6f`는 호출/graph builder ratchet을 현재 routine identity
carrier에 맞춘다. Focused hard-contract 전체는 local exit 0이다. Replacement CI
전에는 29/29를 주장하지 않는다.

다음 exact-head run `33058636093`도 28/29였고 full self-host, codegen bootstrap,
backend 20/20과 나머지 platform/proof/sanitizer job은 모두 green이었다. 유일한
`build-linux` 실패는 function+intent lookup 통합 뒤 likeness `result_use`가
4374에서 4372로 줄어든 것이었다. 중복 Option scan은 제거됐지만 새 lookup이
invalid/missing/found를 raw Int `-1/0/양수`로 구분해 errors-as-data ratchet을 우회했다.
Repair `9454f9fe`는 이를 `Result<Int>`의 `Err`/`Ok(0)`/`Ok(SyntaxNodeId)`로 바꾸고,
runtime ABI fallback을 합법적 `Ok(0)`에서만 허용한다. Likeness는 sentinel 23과
result-use 4385/4385로 green이며 최소치를 함께 올렸다. Native gen0 parse 0/0,
gen2==gen3 73,172 lines, full codegen bootstrap `SELF-HOSTING OK`도 통과했다. Complete
component inventory는 다시 60초 예산을 넘어 중단했으므로 green으로 세지 않는다.
Implementation/checkpoint `9454f9fe`/`4a1261ec`의 exact-head run
`33061911002`는 29/29 GREEN으로 완료됐다. `build-linux`, full self-host fixed point,
codegen bootstrap, backend 20/20, Windows/macOS, sanitizer, TSan, Rocq가 모두
통과했으므로 gen0 예약어, routine-aware hard-contract, typed lookup/likeness 수리는
정확한 published revision에서 폐쇄됐다. 이 원격 폐쇄는 이미 계산된 실행 rung의
증거 완성이며 새 hard substitution 분자가 아니므로 수치는 올리지 않는다.

리뷰가 제안한 query/cache와 O(n^2) epoch 교체는 이 실행 rung의 blocker가 아니므로
열지 않는다. SoT `50/35/1`, hard closure 58.1%, migration 78.8%, 통합 83%
(81~85%), strict beta 83%, hard replacement 75%는 유지한다.

2026-08-27 intent-phase declared-callee identity 로컬 구현: `d437e9e8`은
semantic expression identity resolver가 function뿐 아니라 admitted intent surface의
declared leaf에도 정확한 declaration SyntaxNodeId를 기록하게 한다. Formal parameter
ordinal은 기존대로 function owner에서만 해석한다. MIR lower의 이름 복원,
call-target-only 수용, native fallback은 추가하지 않았다. Mixed intent/generic
fixture는 정확히 `accepted=true`, `calls=1`, `rejected=false`, `calls=2`를 실행하고,
invalid generic ordinal과 missing/crossed callee binding은 C artifact 전에 실패한다.
Fresh v21 codegen fixpoint, integrated driver, callable/namespace/canonical/typed-intent/
phase-carrier 회귀, source scan, likeness, Bash syntax와 `100/100` gate cap은 local
green이다. Complete component inventory는 60초 local 예산을 넘어 green으로 세지
않으며 exact-head CI가 다음 falsifier다. Registry와 진행률은 `50/35/1`, hard
closure 58.1%, migration 78.8%, 통합 83% (81~85%), strict beta 83%, hard
replacement 75%로 유지한다.

2026-08-27 callable-parameter public substitution 로컬 갱신: canonical recursive
`func(T...) -> R`와 callable value의 target/binding SyntaxNodeId가 parser,
semantic admission, MIR carriage를 지나 설치형 public backend까지 유지된다. C는
source-MIR 뒤 semantic re-entry/self-C identity-bound emitter를 소비하고, LLVM은
같은 source-MIR 뒤 direct-MIR GraphPlan을 소비한다. 둘 다 implicit native retry가
없으며 explicit native는 runtime-output oracle로만 남는다.

`compose_two_functions`는 C/LLVM 모두 정확히 `16\n13\n6`, builtin 이름과 같은
formal을 둔 falsifier는 정확히 `6`을 출력한다. 20개 missing/forged/cross-wired
mutation은 artifact publication 전에 실패한다. Fresh release DRV-2 설치, focused
gate, 별도 installed-public gate, full component/source-MIR inventory는 local
green이고 구현 checkpoint는 `30b84f80`이다. 첫 published run `33000341546`은 새
prototype helper가 `TextBuilder`를 함수 경계로 운반한 오류를 fresh build에서
발견해 다섯 job이 실패했다. Repair `f6d6fb4b`는 helper가 완성된
`Option<String>`만 반환하게 하고 해당 `TextBuilder` 경계를 금지하는 ratchet을
추가했다. Fresh isolated gen2 seed, DRV-2, focused/public callable, full
component/source-MIR는 repair 뒤 local green이다. 이 repair는 published였지만
replacement green 전이므로 원격 폐쇄는 주장하지 않는다. Registry census는 그대로
`50 CLOSED / 35 BRIDGE / 1 ACTIVE`; hard closure와 migration 산술만 현재
census에 맞춰 각각 58.1%, 78.8%로 바로잡고 통합 83% (81~85%), strict beta
83%, hard replacement 75%는 유지한다.

Replacement run `33002949085`는 첫 수명 repair를 통과한 뒤 두 integration seam을
더 발견했다. Native MIR가 `fp->type->stable_id`를 parameter identity로 잘못
직렬화해 role `Add(self, rhs)`가 partial identity가 되었고, self-host 전용
builtin-shadow fixture를 native backend inventory 아래 둬 20 shard가 동일한
registration 오류로 멈췄다. Repair `024d1ba7`은 가짜 native field를 삭제하고
MIR-to-AST breadth에서 complete/unique 또는 wholly absent identity만 허용하며 partial
변조를 거부한다. Full codegen bootstrap은 `role_operator_dispatch`까지 local
green이다. Checkpoint `dc7be82f`는 shadow fixture를 self-host fixtures로 옮겼고,
backend inventory와 focused/public callable gate가 green이다. 새 replacement run
전이므로 원격 폐쇄나 수치 상승은 주장하지 않는다.

Run `33005863688`에서는 fixture 이동이 20개 backend shard 모두를 통과했다. 다만
새 partial-identity 부정 테스트가 Linux JSON의 첫 declaration-shaped 행만 바꾸고
routine parameter 행은 wholly absent로 남겨, 컴파일러의 정상 수용을 실패로
오판했다. Repair `5f739701`은 모든 일치 행을 바꾼다. 같은 global mutation은
self-built/oracle mir_lower 양쪽에서 `identity carriage is partial`로 거부되고 shell
syntax도 green이다. 이전 run은 이 실패 뒤 새 push로 supersede되었고 replacement
`33006827756`도 후속 문서 push로 supersede되었다. Final-head run `33007078796`은
20개 backend shard, Windows/macOS, sanitizer, TSAN, Rocq, toolchain, codegen
bootstrap을 포함해 27/29를 통과했다. `build-linux`와 full
`self-host-bootstrap-linux`는 producer AST의 callable SyntaxNodeId를 reconstructed
canonical AST ID와 직접 비교하던 MIR-to-C semantic re-entry에서 멈췄다.

Repair `1d459036`은 `(kind, module, owner, routine name, parameter ordinal/name)`
조인을 소유하는 `MirExpressionIdentityEpoch`을 추가해 declared/formal target과
callee binding을 semantic admission 전에 canonical epoch로 원자적으로 바꾼다.
숫자 offset, source/canonical dual read, name-only semantic admission은 없다.
MIR v1이 의도적으로 저장하지 않는 collection receiver atom은 별도
producer-only lane으로 고정해 carried identity가 비어 있어야 하고 canonical
semantic owner만 채운다. Enum을 함수 사이에 둬 두 epoch가 실제로 달라지는 callable
fixture, 20개 negative, canonical topology/method epoch gate, full `mir_lower` C
emission, component inventory가 fresh v7 driver에서 local green이다. 새 replacement
CI 전이므로 원격 폐쇄와 수치 상승은 주장하지 않는다. Gate identity
ratchet `e070fcec`는 기존 stable pass marker를 복구했고 SoT edge는
`CLOSED=50 BRIDGE=35 ACTIVE=1`로 다시 green이다. 수치와 registry 상태는
그대로다.

Replacement run `33016014561`도 27/29를 통과해 이전 callable 단절은
넘었지만 두 후속 가정을 반례로 드러냈다. `build-linux`는 routine
배열이 source SyntaxNodeId 오름차순이라고 가정해 grammar 04의
`9,22,44,35`를 거부했고 generated language-word inventory drift도 잡았다.
Full bootstrap은 intent participant를 MIR routine formal row와 같은 fact family로
잘못 보아 멈췄다. Repair `dfbe9b0a`는 source ID 기준 exact pair를
정렬 삽입하고 source/canonical 중복을 거부한다. Intent participant는 기존
intent execution plan이 계속 소유하며 routine formal은 0개여야 한다. Fresh v9에서
grammar 17/17, callable C/LLVM과 20 negatives, canonical epoch gate, full
`mir_lower` C emission, language-word generator gate, SoT edge가 green이다. Full
component rerun은 5분 focused 예산을 넘어 중단했으며 green으로 세지 않는다.
새 원격 CI 전이므로 수치와 registry 상태는 그대로다.

Exact-head run `33019529720`은 `5d23fdda`에서 27/29였다. 20개 backend shard와
Windows/macOS, sanitizer, TSan, Rocq, toolchain, codegen bootstrap은 모두 green이었다.
`build-linux`는 identity-preserving expression graph constructor의 낡은 정적 문구 한
건을 잡았고, full bootstrap은 namespace 내부의 짧은 `PathCharAt()` 표기와 canonical
`__imp0_SelfHostPath_PathCharAt` target을 같은 display text라고 가정한 지점에서
멈췄다. Implementation checkpoint `9ab03311`은 callable index의 canonical name 및
exact call/callee SyntaxNodeId를 declared-call identity로 고정한다. Direct leaf는 declared binding ID,
namespace call은 persisted member-access topology와 target ID가 정확해야 한다. Formal
callable spelling 검사는 유지하지만 declared leaf text는 더 이상 authority가 아니다.

Fresh v16 driver(6,456,445 bytes, seed C 10,721,396 bytes)는 namespace 내부 C/LLVM
실행과 target/binding 4개 음성, callable C/LLVM과 20개 음성, canonical epoch,
program-graph gate를 통과했다. 같은 원격 실패를 겨냥한 full driver MIR은
270,050,952 bytes까지 생성됐고 repaired consumer는 11,180,254-byte C를 exit 0으로
방출했다. Owner split 뒤 v16 재컴파일도 끝났지만 full component/bootstrap은 다시
green으로 세지 않는다. 원격 29/29가 다음 falsifier이며, SoT `50/35/1`, hard
58.1%, migration 78.8%, 통합 83% (81~85%), strict beta 83%, hard replacement 75%는
그대로다.

Exact-head run `33025012263`은 `c31da1d2`에서 다시 27/29였다. 나머지 27개 job은
모두 green이고 `build-linux`는 namespace mutation 파일이 기존 750줄 상한을 28줄
넘은 것만 실패했다. Split `6fa362c5`는 책임 이름을 가진 mutation owner로 해당
범위를 옮겨 741/750으로 복구했으며 상한은 올리지 않았다. Full bootstrap은
gen2==gen3 169,347줄, installed DRV-2, legacy/composite intent LLVM까지 통과한 뒤
callable parameter에 `source_syntax_id`가 추가되기 전의 9-field implicit receiver
가정을 가진 nested-intent owner에서만 멈췄다.

Repair `af91687d`는 receiver object를 정확한 10-field schema로 받고, 양수 canonical
decimal receiver SyntaxNodeId와 indexed routine owner를 함께 검증한다. Zero receiver
ID는 artifact publication 전에 같은 owner에서 실패한다. Fresh v17은 nested-intent
public/native C/LLVM, namespace C/LLVM과 identity negatives, callable C/LLVM 20
negatives, canonical epoch, program-graph unification을 모두 통과했다. Full component는
정적 owner 예산을 넘겨 중단했으므로 local green으로 세지 않으며, exact-head 원격
29/29가 다음 falsifier다. Architecture review의 typed identity algebra와 display-text
감사는 이 실행 rung 뒤의 유효한 후보지만 query/cache, O(n^2) epoch 교체, 270-MB
profiling은 현재 rung을 닫기 전에는 열지 않는다. SoT `50/35/1`, hard 58.1%,
migration 78.8%, 통합 83% (81~85%), strict beta 83%, hard replacement 75%는 그대로다.

Replacement run `33027933374`은 `3e8a3567`에서 28/29였다. Full
`self-host-bootstrap-linux`는 green이라 fixed point와 installed driver가 이전의
nested-intent 실패 지점을 실제로 넘었다. `build-linux`도 750줄 cap과 full component
inventory를 통과한 뒤 semantic lifetime gate의 낡은 exact caller 목록 한 건만
실패했다. Driver는 `30b84f80`부터 carried-expression identity를 강제하는
`...ObservedWithIdentityPolicy`를 소비하지만 목록은 legacy observed wrapper를 계속
기대하고 있었다. Ratchet `a5ecff34`는 legacy caller set에서 driver만 제거하고 새
policy boundary를 driver/body owner 두 파일의 exact set으로 추가하며 production body도
그 호출을 해야 한다고 고정한다. Focused lifetime gate는 local green이고 다음
exact-head 29/29가 마지막 원격 falsifier다. 수치와 registry 상태는 바뀌지 않는다.

후속 run `33029460672`는 `9bf511d5`에서 28/29였다. Full self-host fixed point는
다시 green이고 lifetime ratchet도 통과했지만, `build-linux`가 callable identity
경로에서 늘어난 실제 `return/compare -1` 여섯 건을 likeness gate로 잡았다.
Repair `b80bc803`은 binding ordinal presence를 scalar fact로 소유하고 마지막
semantic/C 소비자에 `Option<Int>`로 제공한다. Cap이나 예외 목록은 올리지 않았고
likeness는 sentinel `23/23`, Result/Option `4375/4375`로 강화됐다.

Closure checkpoint `ae8b1341`의 exact-head run `33032356735`는 29/29 GREEN이다.
`build-linux`, codegen bootstrap, full self-host fixed point, 20 backend shard,
Windows/macOS, sanitizers, TSan, Rocq가 모두 성공했다. 이 결과로 callable-parameter
installed-path substitution lease는 닫힌다. 다음 singular rung은 이미 resolved된
formal callable을 C 환경에서 다시 source spelling으로 찾는 마지막 display-text
권위를 canonical parameter SyntaxNodeId lookup으로 치환하는 것이다. Query/cache,
O(n^2) epoch 교체, 270-MB profiling은 여전히 이 실행선의 blocker가 아니므로 열지
않는다. SoT `50/35/1`, hard closure 58.1%, migration 78.8%, 통합 83%
(81~85%), strict beta 83%, hard replacement 75%는 그대로다.

다음 local implementation `e4a14e7b`은 이 display-text seam을 좁게 닫는다.
Function-local callable `call` row는 source name 대신 canonical parameter
SyntaxNodeId로 만든 내부 키를 쓰고, final C emitter는 callee node text를 읽지 않는다.
Fresh v19 Pergyra seed/oracle가 컴파일됐고 callable C/LLVM 20 negatives,
namespace C/LLVM negatives, canonical epoch와 likeness `23/23`, Result/Option
`4374/4374`가 green이다. Complete component와 remote exact-head CI 전이므로
퍼센트와 registry 상태는 올리지 않는다.

Exact-head run `33035298360`에서는 20/20 backend shard를 포함한 27개 잡이
성공했지만 `build-linux`가 새 callable binding row owner의 직접 `Die` 호출에
필수 `text_owner.pgy` import가 없음을 complete component gate로 잡았다. Local
repair는 import를 명시했고 direct-consumer/source-scan/likeness/shell syntax가
green이다. Complete component는 60초 local budget을 소진해 green으로 주장하지
않으며 replacement CI를 기다린다. 진행 수치와 registry 상태는 변하지 않는다.

Replacement run `33037083062`는 import check를 통과한 뒤 namespace parity gate가
165줄로 기존 160줄 cap을 넘은 다음 정적 ratchet을 찾았다. Repair `8e8cd8cb`은
SyntaxNodeId lookup과 display-text 음성 검증을 유지한 채 layout 다섯 줄만 줄여
정확히 `160/160`으로 복구했고, 해당 C/LLVM parity와 negatives가 local green이다.
Cap은 올리지 않았으며 다음 exact-head CI 전까지 수치는 올리지 않는다.

Exact-head run `33038171342` at `e7c27b68`은 29/29 GREEN이다. `build-linux`,
full self-host fixed point, codegen bootstrap, 20 backend shard,
Windows/macOS, sanitizers, TSan, Rocq가 모두 성공했다. Formal callable의 마지막
C display-text binding lease는 닫혔다. 다음 singular executable rung은 이미
resolved된 declared callable alias를 `source_name` string key로 다시 찾는 direct C
consumer를 declaration SyntaxNodeId key로 치환한다. Query/cache, O(n^2) epoch,
performance track은 열지 않는다. SoT `50/35/1`, hard closure 58.1%, migration
78.8%, 통합 83% (81~85%), strict beta 83%, hard replacement 75%는 그대로다.

Local implementation `c72ba209`은 실제 마지막 소비자를 다시 추적해 바로잡았다.
Declared call은 일반 `RewriteSemanticCall`이 아니라
`RewriteSemanticIdentityBoundCall`로 먼저 분기했고, 그 안에서 `call_symbol`과
`binding_key`를 `source_name`으로 초기화하던 것이 실제 우회였다. 이제 non-generic
function-global row는 `@declared_callable_syntax:<declaration SyntaxNodeId>`로 C alias를
운반한다. Final emitter는 formal binding SyntaxNodeId, generic call-node specialization,
또는 declared SyntaxNodeId key만 읽고, 누락 시 fail closed한다. `source_name` 인자와
name fallback은 삭제됐다.

Fresh v20은 codegen gen2==gen3 73,145줄, integrated driver seed/oracle, bounded MIR/C,
callable C/LLVM 20 mutations, namespace C/LLVM 4 mutations, canonical epoch gate를
통과했다. Native manifest owner가 isolated v20 sibling을 생성한 뒤 public launcher
C/LLVM도 정확히 `16\n13\n6`과 `namespace:internal-ready`를 실행했다. Generic-default
source-to-C는 `save=9\nbox=7`이다. Source scan, likeness `23/23`와 Result/Option
`4374/4374`, shell/diff check는 green이다. Full component inventory는 60초 예산을
넘겨 green으로 세지 않는다. Generic-specialization epoch gate의
`IntentRunAccepted` admission 실패는 v19에서도 같은 진단이므로 baseline으로
분리했다. Exact-head 원격 CI 전까지 SoT `50/35/1`, hard closure 58.1%, migration
78.8%, 통합 83% (81~85%), strict beta 83%, hard replacement 75%는 그대로다.

Exact-head run `33041466890` at `5be3a3ee`은 29/29 GREEN이다. `build-linux`, full
self-host fixed point, codegen bootstrap, 20 backend shard, Windows/macOS,
sanitizers, TSan, Rocq가 모두 성공했다. 따라서 declared callable C alias lease는
닫힌다. 이 closure는 identity owner와 consumer를 바로잡은 것이며 registry census나
진행률 분자를 올리지는 않는다.

다음 singular executable rung은 같은 검증 중 실제로 관측된 intent-phase callee
binding gap이다. `intent_typed_outcome_execution.pgy`의 self MIR은
`IntentRunAccepted` call target ID `56`을 저장하지만 exact callee leaf에는
`binding_syntax_id:0`, `binding_kind:none`을 저장한다. 현재 semantic identity
resolver가 declared leaf resolution을 `IsSome(function_node)` 안에 묶어 intent-owned
surface를 제외한 것이 원인이다. MIR lower의 name 복원이나 target-only 수용 없이
producer semantic graph가 exact declared binding을 운반하고, mixed intent/generic
gate의 runtime과 missing/crossed identity negative로 닫는다. 이 RED는 v19, v20,
현재 installed driver에서 같고 29/29 matrix에는 포함되지 않았다. SoT `50/35/1`,
hard closure 58.1%, migration 78.8%, 통합 83% (81~85%), strict beta 83%, hard
replacement 75%는 그대로다.

2026-08-26 structured MatchCase carrier 로컬 폐쇄: HIR owner가 typed
`MatchCase` atom을 `SemanticAstStatementFacts` admission에서 한 번만 해석하고,
기존 SyntaxNodeId 행이 canonical pattern/variant와 평탄 binding range/pool,
mutation digest를 운반한다. Semantic use-site, MIR, self-C Option/tagged emission은
같은 구조 fact를 읽는다. MIR의 `SelfMirMatchCaseFactFromText`와 codegen String
accessor는 삭제됐고, 정적 ratchet은 HIR owner 밖 text parse 및 statement admission
밖 ready-artifact read의 복귀를 거부한다.

변조된 variant/binding/range 음성 contract, semantic/MIR lifetime, self-C 세
fixture 실행, 설치형 source-MIR 네 fixture canonical parity, SoT edge가 local
green이다. Full component inventory는 local focused budget을 넘겨 local 완료로
주장하지 않지만, replacement `build-linux`가 그 component와 전체 fast push
target을 green으로 실행했다. Implementation `aafcadbd`와 CI-ratchet repair
`5ce4b384`는 published이며 run `32969362909`는 정확한 repair HEAD에서 약
30분 26초에 29/29 green이다. `selfhost.match_case_pattern`은 `CLOSED`가 되어
SoT census가 `50 CLOSED / 35 BRIDGE / 1 ACTIVE`로 바뀌지만, 통합 **83%
(81~85%)**, strict beta 83%, hard replacement 75%는 유지한다.

2026-08-26 installed DeviceSlot 선언 운반 원격 폐쇄: public `pgy SOURCE
--emit-c`의 C adapter가 설치된 sibling `.machine-layer-manifest.json` 경로를
typed `SourceCManifestVerified` 요청으로 운반한다. 이전에는 artifact child가
`SourceCDefault`를 선택해 `ClaimDeviceSlot` instruction 0에서 machine-layer
projection이 fail closed했다. 이제 기존 Pergyra source-C world/action과 MIR
machine projection owner가 같은 companion을 소비하며, C나 Pergyra에 grant fact를
복제하지 않는다.

선언 확장으로 발견된 기존 startup 불일치도 닫았다. Machine operation이 없는
source/MIR은 mapping block/call을 내지 않고, DeviceSlot source/MIR만 둘 다 낸다.
Missing/corrupt companion은 artifact와 native timing 없이 nonzero다. Implementation
`10055d0b`, SoT-gate repair `464a907a`가 published이고 replacement run
`32949495441`은 1시간 10초에 29/29 green이다. 첫 run은 CLOSED registry row의 새
fallback 이름이 primary gate에 literal로 없음을 잡았고 repair 뒤 Linux structural/
SoT edge, full self-host, Windows, sanitizer, Rocq, bootstrap, macOS, TSan, 20개
backend shard가 모두 green이다. 이는 bounded `SUBSTITUTING`이지만 새 top-level row를
닫지는 않으므로 통합 **83% (81~85%)**, strict beta 83%, hard replacement 75%, SoT
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`는 유지한다.

37개 미폐쇄 SoT 행의 dependency census도 완료했다. 번호 없는
`docs/agent_work_directives/sot_closure_dependency_map_2026-08-26.md` 아래 세
보고서의 owner-ID 합집합은 registry와 정확히 37/37이고 누락·추가·중복은 0이다.
현재 artifact와 교차검증한 분류는 `READY_NEXT=2`, `DEPENDENCY_BLOCKED=24`,
`EVIDENCE_GAP=11`이며 product만 남은 행은 아직 0이다. 최초에 선택했던 48,531,749-
byte routine-1197 RED는 historical이었다. 현재 canonical MIR은 236,684,385 bytes이고
10,464,651-byte gen2/gen3 C를 byte-equal로 내며, exact-revision remote full
self-host도 green이다. 따라서 그 seam은 구현 전에 철회했고 감사 자체도 폐쇄 수로
세지 않는다.

이번 실행 rung `abi.intent_observability_rows`의 네이티브 consumer migration은 로컬
green이다. Semantic admission이 owner row의 RuntimeCallAbiId를 AST call에 한 번
stamp하고, explicit native C/LLVM emitter는 이제 그 ID를 source identity와
cross-seal해 소비한다. 두 backend의 source-spelling row 재조회는 삭제됐고 정적
ratchet이 복귀를 거부한다. ID 0, unknown ID, source-ID mismatch 음성과 installed/
native C/LLVM 실행 패리티가 통과했다. Wider compiler-purpose root intent는 여전히
열려 있으므로 이 bounded consumer 치환만으로 row를 닫거나 퍼센테이지를 올리지
않는다. 통합 83%, strict beta 83%, hard replacement 75%, SoT 49/36/1은 그대로다.

2026-08-26 증거 분모 재대조: 실행 가능한 readiness scorecard는 capability 4를
이미 `READY`로 판정하고, scorecard 본문도 allocator/TextBuilder Phase 1을 hard
self-host substrate closure로 기록한다. 따라서 오래 남아 있던 `9/10` 표기는
`10/10 READY`로 정정한다. 같은 source revision의 installed fixed-point 및
installed-driver gate와 remote run `32938125698`의 29/29 green이 bootstrap과
CI/release의 마지막 로컬 전용 칸을 닫았으므로 두 축은 각각 `4/4`다. 이 증거를
고정 가중치에 다시 넣은 현재 통합 작업 예측은 **83% (81~85%)**다. Strict beta
83%와 SoT `49 CLOSED / 36 BRIDGE / 1 ACTIVE`는 변하지 않는다. Native product
shell이나 unsupported RIR/AIR/HIR producer를 Pergyra-owned로 세지 않았고,
hard-self-host replacement 예측도 새 분모 없이 올리지 않았다.

2026-08-26 로컬 REPL compiler-bypass 갱신: public `pgy --repl`의 C session
UI와 선언 누적은 그대로 두되, 실행 입력마다 호출하던 implicit native
`driver_run_pipeline`을 기존 installed Pergyra C compile/run owner로 바꿨다.
존재하지 않는 `PGY_SELF_DRIVER_BIN`을 줘도 이전 구현이
`repl-native-bypass`를 native로 실행한 RED를 관측했고, 이제 missing/failed
driver는 native retry 없이 해당 입력만 실패한다.

Focused gate는 실제 installed 실행, counting sibling 정확히 1회, missing driver,
invalid source, native timing 부재, 임시 source/binary 회수, old-call static ban을
8초에 통과했다. 증분 build와 기존 installed-driver integration target도 green이며
새 Make target/CI job/timeout/두 번째 compiler build는 없다. 이는 REPL 내부
compiler 경계만 bounded `SUBSTITUTING`이며 전체 REPL 제품은 여전히 native다.
이 REPL lease 자체는 strict beta나 SoT를 바꾸지 않았고, 통합 예측의 83% 정정은
위의 별도 증거 분모 재대조에서만 나온다.
Directive/audit checkpoint `36af9496`과 implementation checkpoint `48aeccca`는
published다. Run `32938125698`은 30분 31초에 29/29 green이며 Linux aggregate
15분 39초, full self-host 30분 27초, Windows/sanitizers 8분 50초, codegen
bootstrap 7분 46초, backend toolchain 9분 27초, 20개 shard 41~58초였다.

2026-08-26 로컬 fallback 폐쇄 갱신: public `--rir*`, `--air*`, `--hir*`의
마지막 implicit `driver_run_pipeline` dispatch를 삭제했다. 완전한 Pergyra producer가
없는 bare 요청은 이제 launcher에서 mode별 missing-owner 진단과 함께 nonzero/empty
stdout으로 fail closed하며, 기존 native diagnostics는 명시적 `--native-pipeline`에서만
실행된다. RIR/AIR/HIR 읽기 전용 감사가 각각 ordered RIR program,
general AIR graph issuance, post-semantic HIR routine/CFG carrier의 부재를 확인했기
때문에 partial MIR/AST reconstruction, native dump parsing, 가짜 request/producer는
추가하지 않았다.

Current-source 강제 build와 installed-driver parent CLI, public MIR,
8-mode negative/opt-in, AIR graph JSON parity/schema/MIR binding, IR probe,
machine-neutral/machine-layer, RIR flow, SEA lane, proof envelope, C/LLVM
observability gate는 local green이다. AIR fixture의 유일한 MIR binding fingerprint
drift는 세 번 동일하게 재현된 current owner 값으로 갱신했다.
`mir_machine_layer_smoke.sh`는 `driver_rung2_main.pgy` 재컴파일 중 focused 5분
예산을 넘겨 중단했으며 green으로 주장하거나 timeout을 늘리지 않는다. 이 entry를
포함하는 implementation checkpoint `4eef51ad`는 published이고 remote closure도 완료됐다.
이는 implicit fallback 폐쇄일 뿐 Pergyra RIR/AIR/HIR implementation 대체가 아니므로
`SUBSTITUTING` 진척, 전체 78%, strict beta 83%, SoT
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`는 모두 그대로다.

Implementation checkpoint `4eef51ad`의 첫 run `32932076025`은
`src/pgy_driver.c`가 359줄로 기존 340-line component cap을 넘은 한 건을 잡았다.
Cap은 올리지 않았다. Mode identity는 `driver_self_host_selection_owner`, missing-owner
진단은 `driver_diag`로 옮기고 launcher는 한 줄 rejection call만 남겨 정확히
340/140줄을 회복했다. Repair checkpoint `45a2cfae`의 replacement run
`32933640461`은 30분 9초에 29/29 green이다. `build-linux` 14분 41초,
full self-host 29분 49초, sanitizers 12분 35초, Windows 8분 37초,
codegen bootstrap 7분 48초, backend toolchain 9분 13초였고 20개 shard는
40~59초에 모두 끝났다.

2026-08-26 원격 폐쇄 갱신: implementation checkpoint `c2ff6548`, closure
checkpoint `b3da55a3`은 public installed
`pgy --mir SOURCE`의 기본 native `driver_run_pipeline -> mir_dump` 우회를
삭제했다. 명시적 `--native-pipeline --mir`만 기존 lifecycle/liveness/source
오라클을 유지하며, 기본 요청은 sibling Pergyra-built driver의
`--emit-mir-diagnostic-verified`로 들어간다. Child는 기존
`ProduceSourceMirThroughPgyCompilerWorld`와 `DriverSourceMirPayloadReceipt`를
재사용하고 canonical `pgy.mir.v1` payload를 full borrowed-text admission에 한 번
통과시킨 뒤 typed routine/block/instruction view만 안정 진단문으로 투영한다. 새
world/zone/protocol, JSON/source/AST rescan, native retry, 임시 artifact는 없다.

Native relay는 stdout 128 MiB/300초 경계를 소유한다. Windows Job Object와 process
polling, POSIX process group과 nonblocking poll로 child/descendant를 묶고, child
failure의 partial stdout은 공개하지 않는다. Missing driver, invalid source,
malformed schema, unsupported option, zero-byte success, descendant-held stdout,
stdout-close-before-exit는 모두 payload 없이 실패한다. Timeout/overflow/crash와 일반
child exit도 서로 다른 receipt로 남는다.

최종 current-source Pergyra-built DRV-2가 설치됐고 full installed CLI gate,
four-block/phi diagnostic, public MIR-JSON, source-MIR world/action, explicit native IR,
SoT edge, likeness, shell syntax, diff gate가 local green이다. Make dry-run은 weekly
public-MIR/default-C target pair에서 self-host build 1회와 installed gate 1회만
보여주며 새 job/standalone target/두 번째 bootstrap은 없다. Full component inventory는
static-loop budget에서 중단되어 green으로 주장하지 않으며, MinGW로 검증할 수 없는
POSIX capture branch는 final run `32926584459`에서 검증됐다. 이 run은 exact HEAD
`b3da55a3`의 29/29 job을 18분 26초에 통과했다. Full self-host는 18분 4초,
`build-linux`는 15분 6초였고 backend toolchain은 7분 31초 뒤 20개 shard를
39~76초에 병렬 완료했다. 첫 원격 적색은 TextBuilder parameter가 bootstrap subset을
넘었고, 다음 적색은 새 `(String) -> String` helper가 likeness 76 ceiling을 넘었으며,
세 번째는 inlining한 owner가 200-line cap을 넘었다. 최종 형태는 builder와 scratch를
한 owner 안에 두고 helper를 삭제해 198줄/200 cap, likeness 76/76을 함께 지킨다.
Full component는 로컬 전체 scan green으로 주장하지 않고, remote `build-linux`의
완전한 structural inventory 통과를 실행 증거로 기록한다.

이는 실제 C-owned public `mir_dump` 경로를 Pergyra implementation으로 바꾼 bounded
`SUBSTITUTING`이다. Native-only lifecycle facts나 top-level registry row는 닫지
않으므로 전체 78%, strict beta 83%, SoT `49 CLOSED / 36 BRIDGE / 1 ACTIVE`
(86 authorities / 180 derived carriers)는 유지한다. 별도 agent-work directive 아래의
semantic-hop/direct-MIR/navigation 감사도 완료됐다. Lease F와 nested one-plan route에는
중복 semantic decision이 없었고, folder move는 최소 후보도 경로 참조 84곳이라
`DEFER`다. Scalar GraphPlan parameter-indirection 중복 후보는 이 remote rung보다
우선하지 않으며 후속 objective card 없이는 구현하지 않는다.

2026-08-26 로컬 실행 갱신: checkpoint `9ad47dd7`, CFG repair `2f4dfe28`, Linux harness repair
`60e9fb8a`는 exact nested
priority/observability family의 source-C와 direct-MIR C를 기존 sealed
`DirectMirNestedIntentProgramPlan` 하나로 수렴시켰다. 이전 direct C는 LLVM-only
분기에서 route claim 전 `None`을 받아 scalar admission까지 떨어진 뒤 실패했고,
source-C는 같은 admitted MIR을 tree text/AST로 재구성하고 semantic/codegen을 다시
수행했다. 이제 projection은 route를 먼저 한 번 claim하고 plan을 한 번 seal한 뒤
C/LLVM emitter를 target fact로 선택한다. Claimed-invalid family는 다른 경로로
재해석되지 않는다.

새 C emitter는 admitted MIR, source, AST, JSON을 읽지 않고 plan 및 canonical
runtime/observability ABI symbol만 소비한다. Source-C의 exact 4-routine/
2-declaration guard는 이 projection을 `DriverRung2IntentTreeEmissionOrDie`보다 앞에
배치하며, direct C도 scalar admission 전에 같은 함수를 부른다. Final
current-source Pergyra-built DRV-2 설치가 성공했고 focused gate는 11.6초에 green이다.
Source/direct C artifact는 2,488 bytes, SHA-256 `4F2B9434...23E644`로 byte-equal이며,
thread-safe zone ABI와 `-Wall -Wextra -Werror`로 컴파일되고 정확한 9줄을 실행한다.
기존 LLVM 5개와 두 C entrypoint의 10개 mutation은 artifact 없이 owned boundary에서
실패한다. 새 target/job/build는 추가하지 않았다.

이는 exact family의 실제 source-C 재구성 path와 direct-C scalar dead end를 Pergyra
plan 하나로 대체한 bounded `SUBSTITUTING`이다. Arbitrary intent C나 top-level
registry row를 닫지는 않으므로 전체 78%, strict beta 83%, SoT
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`는 유지한다. Full component inventory는 90초
무출력 뒤 static-loop budget을 지키기 위해 중단했으며 green으로 주장하지 않는다.
첫 push run `32911287910`은 full self-host body-safety에서 target-invalid `Die` 뒤
명시적 `return None`이 빠진 것을 잡았다. Repair 뒤 동일 native-oracle driver 방출은
로컬 0 error로 완료되고 focused gate도 다시 green이다. Replacement run
`32912230440`은 fixed-point와 DRV-2 설치 뒤 새 thread-safe C harness가 Linux POSIX
feature macro를 누락한 것을 잡았다. Harness는 기존 bootstrap emitted-C profile과
같은 macro를 사용하고 local focused gate도 green이다.

Final checkpoint `60e9fb8a`의 run `32913743277`은 29분 25초에 29/29 green으로
완료됐다. `build-linux`는 15분 24초, full self-host는 29분 20초였으며 20개 backend
shard, sanitizer, Windows/macOS, codegen bootstrap, TSan, Rocq가 모두 통과했다.
Lease E는 `DONE`이며 다음 production bypass와 objective card를 관측하기 전에는 후속
rung을 추정하지 않는다.

2026-08-26 로컬 실행 갱신: installed MIR-C stdout의 기본 요청과 명시적
machine-manifest 요청을 기존 `PgyCompilerWorld.direct_mir` zone과 MIR-C artifact
publication이 공유하는 typed payload admission 뒤로 옮겼다. 기존 verified/
pressure-observed 축과 default/manifest 축은 서로 다른 typed identity로 유지한다.
공통 producer는 두 축, canonical CPU-C target을 검증한 뒤 컴파일러를 한 번만
호출하고 원본 `CompilerEmissionArtifact`와 target fact를 stdout 또는 atomic
publication consumer에 전달한다. Read executor의 두 직접 compiler call은 삭제됐다.

병렬 읽기 전용 감사가 malformed explicit manifest의 hidden default를 재현했다.
변경 전에는 exit 0으로 9,430-byte default C를 내보냈지만, 이제 `MIR C machine
declaration is invalid`와 함께 nonzero로 끝나고 C payload를 내보내지 않는다.
정상 default stdout은 9,430 bytes SHA `A29997AD...B8749`, host-normalized artifact는
9,174 bytes `F36551DE...96A33`, 정상 manifest stdout은 9,472 bytes
`CB37D99B...19BA`로 유지됐다.

기존 installed-driver Make target은 current-source Pergyra-built DRV-2 설치 뒤
local green이며 narrow world/topology/action ratchet도 통과했다. Push workflow는
기존 한 Make invocation에 이 target을 추가해 새 job이나 두 번째 self-host build를
만들지 않는다. Component/world/topology/hard/likeness/progress/SoT/protocol/
documentation/diff gate는 local green이다. Likeness는 sentinel `24/24`,
Result/Option `4287/4287`, world `1`, zone `22`, member `4`; SoT edge는
86 authorities / 180 derived carriers를 유지한다. 이 작업은 production
orchestration의 `REACHABLE` closure이므로 전체 78%, strict beta 83%, SoT
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`는 그대로다.

Code checkpoint `e5b159c3`의 push run `32905167784`는 29분 16초에 29/29
green으로 완료됐다. 새 installed MIR-C stdout falsifier를 기존 Make call에서
실행한 `build-linux`는 15분 04초, full self-host는 29분 12초였고 20개 backend
shard, sanitizer, Windows/macOS, codegen bootstrap, TSan, Rocq가 모두 통과했다.
Lease D는 `DONE`이며 다음 production bypass와 objective card를 관측하기 전에는
후속 rung을 추정하지 않는다.

2026-08-26 로컬 실행 갱신: installed source-C의 기본 stdout, 명시적
`--emit-c-verified`, machine-manifest stdout이 기존 `PgyCompilerWorld.source_c`
zone과 하나의 typed payload admission을 거치도록 바꿨다. Read executor에 있던
두 `CompileSourceToCVerified` 직접 호출은 삭제됐고, artifact publication도 같은
admission의 원본 `CompilerEmissionArtifact`를 소비한다. stdout용 임시 artifact,
두 번째 world/zone, payload/projection/fingerprint 복제 authority는 만들지 않았다.

병렬 읽기 전용 감사에서 malformed explicit manifest가 default empty declaration으로
붕괴해 C를 내보내던 hidden fallback을 찾았다. 이제 `SourceCDefault`와
`SourceCManifestVerified`가 요청 정체성을 보존한다. 잘못된 manifest는 typed
diagnostic과 함께 exit 1, C payload 0으로 닫히고, 정상 default/explicit 출력은
변경 전후 9,430 bytes SHA `A29997AD...B8749`, 정상 manifest 출력은 9,472 bytes
`CB37D99B...19BA`로 정확히 유지됐다.

Focused installed Make gate, component/world/topology/hard/likeness/SoT edge가
local green이다. 새 CI job이나 두 번째 self-host build는 추가하지 않았다. 이
작업은 이미 Pergyra-owned인 컴파일러의 내부 orchestration bypass를 없앤
`REACHABLE` 증거이므로 전체 78%, strict beta 83%, SoT
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`는 그대로다. 다음 falsifier는 이 checkpoint의
기존 29-job push matrix이며, 그 전에는 다음 실행 rung을 추정하지 않는다.

Code checkpoint `20ffa7c7`의 push run `32897701600`은 29분 41초에 29/29
green으로 완료됐다. Linux preparation은 15분 21초, full self-host는 29분 38초였고
20개 backend shard, sanitizer, Windows/macOS, codegen bootstrap, Rocq가 모두
통과했다. Lease C는 `DONE`이며 전체 78%, strict beta 83%, SoT
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`는 그대로다. 다음 작업은 새 production
bypass를 실제로 관측하고 objective card를 고정한 뒤 시작한다.

2026-08-26 로컬 실행 갱신: nested priority fixture가 빠지던 scalar-only
direct-MIR LLVM 경로를 exact mixed-callable route로 대체했다. 새 route는 composite
intent 다음, scalar admission 전에 one function/one method/two intent family를
독점 claim하고 `Main -> OuterPriority -> InnerPriority -> Capture`, subject/zone
field, Outer literal priority `1`, Inner dynamic priority를 admitted declaration,
routine, intent binding/carrier, expression DAG에서 seal한다. Actual MIR에서 intent
header params는 비어 있고 `world/probe/requested`는 ordered intent bindings가
소유하며, method `self`는 null type/ABI의 implicit receiver다. 이를 explicit typed
formal로 완화하거나 raw JSON owner를 다시 읽지 않았다.

Isolated current-source driver와 최종 Pergyra-built installed DRV-2 모두 public
self/native LLVM의 정확한 9줄 출력을 각각 golden과 byte-equal로 실행했다.
160-line focused gate는 약 7초이며 missing Inner priority, priority graph drift,
duplicate source identity, method-owner crosswire, semantic action-name/target-row
crosswire에서 artifact를 남기지 않는다. Component contract와 owner
caps가 green이고 dispatcher는 기존 110-line ceiling을 유지한다. Push/weekly CI는
새 타깃을 기존 single Make invocation에 넣어 job이나 self-host compiler build를
추가하지 않는다. Hard/progress/documentation/diff gate와 likeness sentinel `24/24`,
Result/Option `4287/4287`도 green이다. 이 exact family는 local `SUBSTITUTING`이지만
remote publication과 top-level denominator review 전까지 integrated 78%, strict
beta 83%, SoT `49 CLOSED / 36 BRIDGE / 1 ACTIVE`는 유지한다.

실행 checkpoint `2d43bd66`의 첫 remote run `32884881665`는 28/29였다. Full
self-host, codegen bootstrap, sanitizers, 세 플랫폼, Rocq, backend 20 shards는
모두 green이고, 유일한 red는 Linux preparation의 derived-owner registry
누락이었다. 새 route/graph fact owner 두 개를 기존 `mir.execution_graph`의
`projection`으로만 등록한 local repair는 authority edge `86 authorities / 180
derived carriers`에서 green이다. Top-level SoT 수는 `49/36/1` 그대로이며 다음
falsifier는 이 두 행을 push한 remote 29/29였다.

Repair checkpoint `6be30daa`의 run `32888031601`은 29분 19초에 29/29
green으로 닫혔다. Linux preparation은 15분 18초, full self-host는 29분 15초였고
20개 backend shard, sanitizers, 세 플랫폼, codegen, Rocq가 모두 통과했다. 이
bounded nested-intent LLVM family는 remote `SUBSTITUTING`이며, 전체 78%, strict
beta 83%, SoT `49 CLOSED / 36 BRIDGE / 1 ACTIVE`는 그대로다. 다음 작업은 새 SoT
정리가 아니라 production entrypoint의 다음 직접 우회를 관측해 한 executable
rung의 objective card를 고정하는 것이다.

2026-08-25 로컬 실행 갱신: production source-to-LLVM의 C-owned 두 subprocess
경계를 삭제했다. Public LLVM file/stdout/binary와 package LLVM target은 이제
installed `--emit-source-llvm-ir-verified SOURCE -o OUTPUT` 한 번으로 들어가며,
`PgyCompilerWorld -> CompilePergyraProgram ->
DriverSourceLlvmIntentExecution.Compile`이 기존 source-MIR admission과 direct LLVM
projection/transaction owner를 in-memory receipt로 조정한다. Root intent는
`ProduceMir/PublishLlvm` 고정 lifecycle을 노출하지 않고 한 real-purpose `Compile`
action만 갖는다. Typed outcome은 published/source-rejected/projection-rejected를
구분하고 Bool completion과 불일치하거나 outcome이 없으면 fail closed한다.

최종 current-source Pergyra-built driver 설치가 성공했다. Installed CLI는 direct
MIR oracle과 LLVM byte parity, distinct source/projection failure, no-partial-output을
통과했고, public file/stdout 및 default compile/run은 compiler intent를 정확히 한
번 호출하며 native pipeline에 재진입하지 않는다. Package LLVM counting path는
기존 `mir/mir/llvm/mir/llvm` 5회에서 `mir/intent/intent` 3회로 줄었다. World는
22 concrete zones와 `direct_mir/source_mir/source_llvm/source_c` 네 member,
likeness는 Result/Option 4,267과 zone-bound rows 35를 exact로 관측한다. 이 bounded
compiler-purpose slice는 실제 C path replacement인 `SUBSTITUTING`이지만 아직
overall 78%, strict beta 83%, SoT `49 CLOSED / 36 BRIDGE / 1 ACTIVE`는 유지한다.
Executable checkpoint `3e5ad3d6`의 첫 publication run `32847222699`은 28/29로,
bootstrap/sanitizer/platform/proof/backend shard는 모두 통과하고 `build-linux`의
stale compiler-world owner assertion만 실패했다. Gate repair `43982bbf`는 artifact
요청의 새 owner, exported canonical action, `On action_succeeded` AST carrier를
검사하도록 갱신했다. 후속 run `32851043420`은 28분 43초에 full self-host와 20개
backend shard를 포함해 29/29 green이다. 다음 falsifier는 첨부 리뷰의 intent
mode/priority AST last-consumer를 다음 executable composite-intent LLVM rung에서
실제로 도달할 때 닫는 것이다.

2026-08-25 실행 checkpoint `626f2188`은 installed general MIR-to-C의 직접
`CompileMirJsonToCVerified[Observed] + SelfMirArtifactCommitPayload` orchestration을
삭제했다. Verified/pressure-observed artifact 요청은 기존 `PgyCompilerWorld`의
`direct_mir` zone과 `DriverRung2Execution.PublishMirCArtifact` action을 거치며,
direct C/LLVM action과 하나의 target/artifact/transaction transition을 공유한다.
CLI mode 모양을 따라 네 번째 zone이나 두 번째 world를 만들지 않았다. 격리된
current-source Pergyra-built candidate가 설치됐고, verified stdout/artifact와
observed artifact는 byte-equal이며 pressure stage 4개가 보존됐다. Missing-parent
transaction은 typed diagnostic과 함께 실패하고 artifact를 남기지 않았다. 이
focused installed CLI gate는 candidate 재사용 조건에서 5.97초였다. World 3개와
concrete zone 21개는 그대로이고 zone-bound action만 38에서 39로 증가했다.
Compiler-world/topology/component/atomic-transaction/likeness 및 documentation
gate는 green이다. Adjacent direct C/LLVM positive parity도 green이고, 그 뒤의
diagnostic-specific negative는 기존 handoff에 기록된 stale `expr0_graph`
expectation으로 계속 RED다. 이 변경은 Pergyra-owned compiler의 내부 orchestration
우회를 제거한 production `REACHABLE` 증거이며 새 C-owned 구현 대체가 아니므로
overall 78%, strict beta 83%, SoT `49 CLOSED / 36 BRIDGE / 1 ACTIVE`는 유지한다.
다음 falsifier는 checkpoint를 push한 뒤 remote 29/29와 full bootstrap이다.

첫 publication run `32821686833`은 28/29로 완료됐다. Full self-host는 16분
58초에 green이고 모든 20 backend shard, sanitizers, Windows/macOS, codegen
bootstrap과 formal proof도 통과했다. 유일한 RED인 `build-linux`는 12분 19초
동안 component contract와 actual Coq adequacy까지 통과한 뒤
`sot_authority_edge`에서 멈췄다. `selfhost.compiler_artifact_commit` row가 요구한
새 MIR-C evidence label과 installed CLI gate의 기존 PASS line이 글자 단위로
달랐기 때문이다. Repair `d6a01696`은 기존 `selfhost.driver_cli_request` label과
새 MIR-C label을 같은 실행 gate의 한 line에 함께 보존한다. Authority edge
`49/36/1`, single-owner, protocol registry와 artifact/action ratchet은 local green;
다음 falsifier는 repair push의 remote 29/29다.

Repair와 첫-run handoff를 포함한 run `32823302830`은 29/29 green으로 닫혔다.
전체 run은 28분 54초, full self-host는 28분 50초, build-linux는 13분 41초였다.
Remote SoT edge는 `CLOSED=49 BRIDGE=36 ACTIVE=1`, likeness는 Result/Option 4,257,
concrete zones 21, world members 3, zone-bound actions 39로 통과했다. 이 결과-only
문서 successor는 local documentation/progress/beta gate를 다시 통과한 뒤
`[skip ci]`로 게시해 동일한 29-job matrix의 세 번째 실행을 만들지 않는다.
General MIR-to-C world/action rung은 이제 remote GREEN이며, 다음 작업은 current
production evidence에서 새 executable bypass를 선택해야 한다.

2026-08-25 실행 갱신: checkpoint `19103024`의 exact canonical composite-intent
public self-host LLVM 대체와 문서 checkpoint `4162b81b`는 remote run
`32806933585`에서 29/29 green이다. 전체 run은 28분 42초, full self-host job은
28분 38초였고 새 composite gate 자체는 Linux에서 약 0.74초였다. 직전 green보다
늘어난 약 5분의 대부분은 DRV fixed-point 구간의 변동이며, 새 gate나 matrix/job
증가가 원인이 아니다. 이 exact family만 bounded `SUBSTITUTING`이고 arbitrary
composite intent와 compiler-root intent/world는 계속 open이다.

현재 executable checkpoint `a94737dd`는 다음 production dogfood 우회인 installed
source-to-C의 직접 `CompileSourceToCVerified + SelfMirArtifactCommitPayload` 호출을
삭제했다. `PgyCompilerWorld`의 세 번째 positional `source_c` zone과
`DriverSourceCExecution.PublishSourceCArtifact` action이 기존 compiler/emission
owner를 정확히 한 번 소비하고 typed receipt/rejection/artifact failure를 마지막
CLI consumer까지 유지한다. Canonical composite source의 direct installed/public
C artifact가 byte-equal이고, 생성 C 실행의 intent result/trace가 유지되며,
missing-parent transaction failure는 artifact 없이 typed diagnostic으로 끝난다.
이는 실제 production orchestration의 `REACHABLE` dogfood 진전이지만 새로운
C-owned compiler path를 대체한 것은 아니므로 hard substitution 퍼센트로 세지
않는다. SoT `49 CLOSED / 36 BRIDGE / 1 ACTIVE`, 통합 78%, strict beta 83%는
유지한다. 다음 falsifier는 이 source-C world/action checkpoint의 remote 29/29
publication이다.

첫 publication run `32811885342`는 28/29였다. 새 source-C action을 포함한 full
self-host는 27분 25초에 green이고 focused action gate 자체는 Linux에서 약
0.58초였다. 유일한 red는 `build-linux`의 old exact topology baseline이었다:
실제 world member는 의도대로 3개인데 likeness/topology gate가 2개를 요구했다.
Repair checkpoint `1721b6aa`는 측정 대상에 새 owner를 포함하고 baseline을
`21 concrete zones / 3 world members / 38 zone-bound actions / 4,257 typed
Result·Option uses`로 강화한다. Compiler-world contract와 source-C 실행/transaction
게이트가 로컬 green이며 다음 falsifier는 이 repair의 remote 29/29다. 퍼센트와
SoT state는 바뀌지 않는다.

Repair handoff `6217f0d7`의 run `32814568145`는 29/29 green으로 닫혔다. 전체
29분 21초, full self-host 29분 16초, build-linux 15분 06초였고 source-C focused
gate는 약 0.65초였다. Linux likeness는 Result/Option 4,257, concrete zones 21,
world members 3, zone-bound actions 38을 exact로 확인했다. 이 최종 결과 기록은
로컬 documentation/progress/beta gate를 통과한 docs-only successor이며, 같은
29-job matrix의 세 번째 무의미한 반복을 피하려고 `[skip ci]`로 게시한다. 다음
실행 후보는 installed general MIR-to-C의 direct compile/commit 우회였고, 후속
checkpoint `626f2188`이 그 exact production 경계를 선택해 local closure까지
진행했다. 퍼센트와 SoT state는 그대로다.

## 2026-08-27 프로젝트 퍼센테이지 기준선

한 숫자가 필요할 때의 현재 작업 예측은 **83%**다. 오차 범위는
**81~85%**로 둔다. 이것은 릴리스 판정이나 테스트 통과율이 아니라,
`언어 베타 + SoT 폐쇄 + hard self-host 대체 + bootstrap + CI/release`를
함께 끝내는 데 필요한 남은 작업을 추정하는 통합 진행 지수다.

언어 자체의 공식 **strict beta readiness는 83%로 유지**한다. 6월의
83%보다 코드가 진전됐고 latest implementation source의 remote CI도 green이지만,
모든 beta-critical fallback 폐쇄와 공식 checklist 승격은 아직 없으므로 그
수치를 올리지 않는다.
현재 두 표시값은 모두 83%지만 의미는 다르다. 전자는 언어 베타의 공식선이고,
후자는 셀프호스트와 배포를 포함해 가중한 작업 예측이다.

| 축 | 현재치 | 분모와 근거 | 다음 상승 조건 |
| --- | ---: | --- | --- |
| 언어 strict beta | 83% | `docs/100_beta_readiness_checklist.md`의 현재 공식선 | current full suite와 남은 beta-critical fallback 폐쇄 |
| SoT hard closure | 50/86 = 58.1% CLOSED | owner registry의 `CLOSED 50 / BRIDGE 35 / ACTIVE 1` | consumer migration, old read 삭제, negative gate까지 갖춰 `CLOSED` 승격 |
| SoT migration index | 78.8% | 진행 예측용으로만 `CLOSED=1`, `BRIDGE=0.5`, `ACTIVE=0.25`를 적용 | BRIDGE를 늘리는 문서/owner 추가가 아니라 실제 hard substitution |
| self-host substrate | 10/10 READY | executable scorecard의 capability 4는 allocator lanes, `BoxArray`, explicit destroy, TextBuilder/result assembly를 포함해 이미 READY이고 본문의 measured closure도 Phase 1 closure를 선언한다. | remaining compiler-scale String scope reclamation은 효율 전선이며 새 native fallback의 근거가 아님 |
| 일반 GraphPlan 연속 전선 | complete-source Pergyra producer/gen2/gen3 fixed point + installed public C/LLVM/package/MIR/REPL-compile boundaries | canonical O3 gen2/gen3 byte equality, repository-installed sibling, fail-closed public gates가 누적됐고 latest implementation source의 remote full self-host job도 green이다. routine·row·V·owner 수는 진행률 분자가 아니다. | 기존 fixed point를 유지하고 fresh production compiler bypass가 실제 Pergyra owner에 닿을 때만 후속 rung을 연다. |
| hard self-host replacement 예측 | 75% | complete-source producer/fixed point와 public compiler-bearing C/LLVM/package/MIR/REPL 내부는 bounded `SUBSTITUTING`이지만 whole product와 unsupported RIR/AIR/HIR에는 완전한 Pergyra owner가 없다. 기존 추정 분모를 새 숫자 없이 유지한다. | fresh production bypass, complete Pergyra owner, executable falsifier가 함께 존재하는 다음 target-specific substitution |
| bootstrap fixed point | 4/4 = 100% | current-source MIR producer, DRV-2/gen2 consumers, gen2==gen3 equality, installed reproduction과 같은 implementation source의 remote full-bootstrap green을 모두 관측했다. | current-source fixed point를 계속 green으로 유지; 100%는 이 축의 acceptance evidence이지 whole product 완료가 아님 |
| 마지막 완료 baseline CI/release 증거 | 4/4 = 100% | source-C intent repair/checkpoint `cb53b879`/`c0632e4f`의 exact-head remote run `33071044311`이 34m32에 Linux/Windows/macOS, sanitizer, TSan, Rocq, backend compare 20 shards, codegen bootstrap, full self-host와 installed public gates를 29/29 green으로 닫았다. | 다음 compiler implementation delta에서 같은 merge/push matrix를 재검증 |

통합 83%의 계산 가중치는 다음과 같이 고정한다. 이 가중치는 완료를
예쁘게 보이게 하려고 바꾸지 않는다.

- 언어 strict beta 30%
- SoT migration 25%
- hard self-host replacement 25%
- bootstrap reproducibility 10%
- CI/release evidence 10%

hard self-host replacement는 lexer/parser parity, complete-source MIR producer,
Pergyra MIR consumer, gen2/gen3 fixed point, bounded public C/LLVM/package/MIR/
REPL-compile substitution을 완료 증거로 센다. 일반 GraphPlan canary의 routine
ordinal이나 통과 routine 수는 프로그램 구조가 바뀔 때 함께 변하므로 완료
분수로 환산하지 않는다. 75%는 현재 실제 hard-substitution 범위와 완전한
Pergyra owner가 없는 native product shell 및 unsupported IR producer를 함께 본
target-specific 작업 예측이며 산술 통과율이 아니다.
통합 계산은 `83×0.30 + 78.8×0.25 + 75×0.25 + 100×0.10 +
100×0.10 = 83.35`이며 표시값은 83%다.

2026-08-18 control-flow 실행 갱신: 다음 compiler-scale RED는 SoT 행이
아니라 direct `else if` tail을 source 깊이만큼 재귀 호출하던 MIR lowering이었다.
기존 oracle은 41-condition fixture에서 Windows `0xC00000FD` stack overflow로
종료했고, 전체 source는 routine 2743
`SelfDirIntentStepClauseFactsFromArtifact`에서 멈췄다. 이제 direct nested-If
topology만 explicit entry/then frame으로 내려가고, 기존
`SelfMirMergeIfBranches`를 역순으로 소비한다. 41-condition seed/native MIR
90,132 bytes와 C 11,788 bytes가 각각 byte-equal이며 실행 stdout도
`neg/zero/small/big`와 일치한다. canonical full `driver_bootstrap.sh`를
`PGY_SELFHOST_DRIVER_FULL_FIXPOINT=1`과 release flags
`-O3 -fwrapv -fno-strict-aliasing`으로 실행해 exit 0을 확인했다. seed/oracle
MIR은 232,242,252 bytes/SHA-256
`47679723ED88B38972ACCA78488268277EF7BDFCD3980D33F60DCDC7CDA10F48`,
gen2/gen3 C는 10,265,701 bytes/SHA-256
`9187E188FBA6C0EC405643E14D6A33197B34E025AEA7677962C5214BBE88D0C1`로
동일하다. 이어 기존 installer를 격리 출력으로 실행해 5,903,397-byte
candidate와 byte-identical 1,144-byte machine-manifest replay를 만들었고,
typed argv의 source-C/source-MIR/MIR-C stdout·artifact 분리 gate도 통과했다.
이것은 새 registry 폐쇄가 아니라 실제 executable fixed-point와 local install
transaction 전진이다. 이어 같은 리비전으로 fresh release/LTO native launcher를
격리 sibling 옆에 만들었다. public 상대경로와 native 절대경로가
`source_module_path`를 갈랐던 경계는 기존 import-resolver canonical path owner를
public child handoff가 소비하도록 닫았고, Windows canonical spelling은 `/` 하나로
고정했다. 이 정규화는 MIR/C identity handoff에만 적용하며 public `--tokens`,
`--ast`, capability-manifest, DIR stdout은 사용자가 입력한 상대경로 표기를 유지한다.
네 stdout gate와 fail-closed negative가 모두 통과했다. public/direct MIR은 59,402 bytes/SHA-256
`447440EC0547886CBC0216C70F6466FDF4B4E10A84D3F7C2149CC6072038F491`, native/self
canonical MIR은 64,494 bytes/SHA-256
`CADA3C569501FD2CB18E071D5F0B0A89DF195B57690E8923B2F9D8A344B16DE9`로 각각
byte-equal이다. public `--emit-c`와 plain C compile/run도 missing-sibling
fail-closed를 포함해 green이고, Linux parity CI job은 이제 이 public-MIR gate를
직접 실행한다. CI profile, hard-substitution, documentation-quality gate도 green이다.
실제 `bin/` promotion, current remote CI, broad dirty-tree gate는 아직 남아 있으므로
SoT `49/86`, 통합 78%, strict beta 83%, bootstrap 3/4는 올리지 않는다.

같은 날 release 설치 경계도 실행으로 닫았다. 기존 `all`/`release`는 ordinary
source compile의 필수 sibling 없이 public `pgy`만 만들 수 있었으나, 이제 두 target이
기존 `self-host-compiler` installer를 소비한다. 격리 `BIN_DIR`에서 단일
`make release`가 3,384,801-byte `pgy.exe`, 5,903,397-byte
`pgy-self-driver.exe`, 1,144-byte machine manifest를 함께 설치했고, 환경 override 없이
installed CLI-mode/public MIR/public C emit/plain compile-run gate가 모두 green이다.
Linux parity CI도 이 동일 `make release`를 첫 목표로 사용한다. Remote run
`32071813850`은 완료됐지만 RED였고, 실제 repository `bin/` promotion도 아직
관측하지 않았으므로 위 퍼센트는 유지한다.

같은 설치 경계의 반복 host compile도 source-artifact identity로 좁혔다. 기존
installer stamp는 실제 생성 C 해시를 이미 갖고도 codegen PE 해시를 함께 넣어,
동일 C를 내는 재링크마다 DRV-2를 다시 컴파일·설치했다. 로컬 v4 key는 codegen
binary identity를 제거하고 normalized C, machine manifest, runtime headers,
output, compiler profile/flags/version만 소비한다. 한 바이트가 다른 실행 가능한
seed와, phony bootstrap으로 SHA가 다시 달라진 gen2가 모두 동일한
9,850,372-byte C/SHA-256
`512B512339A70444DAF599361FED30A1CB8F716126E35C3F63FEC94C5C10B0E2`를 냈고,
staging `make all`은 5,903,397-byte driver/SHA-256
`E32850D01A68074CF7E713AE3FC3299671FB6B5724C885B1F38D4B0B958C08D0`의 hash와
mtime을 보존한 채 fingerprint reuse로 끝났다. Invalid profile은 emission 전에
fail-closed다. Codegen bootstrap 자체의 반복 생성은 아직 남아 있으며 이번
변경은 이를 cache로 숨기지 않는다. Commit `c363a94a`의 remote CI run
`32071813850`은 실패로 완료됐으며 이 로컬 v4 delta를 포함하지 않는다.

그 remote 결과에서 현재 source로 재현 가능한 실패는 owner 경계에서 좁게
교정했다. Defer 안의 `AllocatorDestroy`는 defer MIR instruction이 소유한 runtime-
call ABI row를 지연 emit 시점까지 운반하고, intent compensation은 정상 경로와
동일한 final materialization trace를 남긴다. MIR 162/162와 C/LLVM focused
backend 2/2가 green이다. 낡은 default `all` inventory, generated language-word
inventory, 2,600-line beta status gate도 각각 owner 기준으로 갱신해 local green이다.
Installed intent-observability C/LLVM 네 경로도 실행 통과하며, 다음 Linux 실패는
compile phase stdout/stderr를 보존한다. Mac C-only의 typed-intent MIR 실패는 Ubuntu
GCC에서 같은 `step zone identity cross-seal` 진단으로 재현했다. DIR이 소유하고
`dir_destroy`가 해제하는 `step->where_type_name`을 MIR이 얕게 보관한 수명 결함이었다.
MIR materialization은 이제 그 spelling을 routine scratch arena에 복사하고, 정적
negative gate는 옛 borrow를 거부한다. Ubuntu GCC normal/ASan+UBSan과 Windows
GCC/Clang C-only 핵심 MIR가 모두 162/162다. Linux `test-asan`의 중앙 배터리에도
`test_mir`를 편입했고, 의도적 UAF witness, 40-source compiler corpus, AIR/semantic/
parser/MIR 네 배터리가 한 실행에서 clean이다. Native typed-intent gate도 모든
source compile/run에 `--native-pipeline`을 명시하고 32행 observability oracle을
검사한다. 존재하지 않는 self-driver를 지정해도 C/LLVM이 통과했고, 별도 prebuilt
self-host v3 compensation/history parity도 다시 통과했다. Current remote macOS/Linux
재실행은 아직 필요하다. SoT `49/86`, 통합 78%, strict beta 83%, bootstrap 3/4는
그대로다.

2026-08-18 실행 갱신: statement/body 분석의 큰 메모리 증폭은 SoT 문서가
아니라 admitted/checked API 경계 결함이었다. 이미 승인된 match case ancestor를
읽을 때 `AstMatchCasePatternFactFromArtifact`가 `AstTreeArtifactReady`를 214회
다시 호출했고, 그 안의 전역 expression-graph `seen`/stack 성장 요청만 약
673.2MB였다. checked API는 그대로 두고 ready-artifact 로컬 projection을
분리해 admitted match-binding owner만 소비하게 했으며, 옛 전체 artifact/graph
재증명은 focused negative gate가 막는다. Current Pergyra-built driver가 게시한
MIR은 232,064,536 bytes/SHA-256
`56EF4D76E96E8B8E3F8C63B786803506CC841C18CDC7DF5B353DB91582F820EC`다.
생산 시간은 144.550초에서 102.981초, peak private는 2.781GiB에서
1.753GiB로 줄어 attention 선 아래로 내려왔다. 동일 MIR을 DRV-2와
host-compiled gen2가 각각 소비한 gen2/gen3 C는 10,257,419 bytes/SHA-256
`484D5246C782FD7BC70E24B3EE7EE341F9B3D38F962D6786AD4DC0B6B5500608`로
byte-equal이다. Option-match C/LLVM parity와 semantic lifetime gate도 green이다.
이는 실제 executable lifetime/substitution 전진이지만 installed/remote
CI/release promotion과 broad component inventory의 기존 163/125 cap RED는 남아
있다. 따라서 registry census `49 CLOSED / 36 BRIDGE / 1 ACTIVE`, 통합 78%,
strict beta 83%는 올리지 않는다.

2026-08-17 최신 실행 전선은 문서·owner 수가 아니라 고정 MIR 전진으로
판정한다. 현재 Pergyra-built DRV-2는 5,890,881 bytes/SHA-256
`56EA64D1DA7D654AF8075BEEDB39AB98BED01E67567BA70EBB7D0B3D279B9A36`다.
Stable operation 44 하나가 이제 로컬 또는 formal value-result 논리 레코드의
persisted member/index graph와 `Array<Int>`/`Array<String>` element type, 최신
지배 SSA predecessor를 소비한다. 기존 로컬 String gate와 새 value-result Int
gate가 C/LLVM에서 각각 실행됐고, 후자는 두 번의 nested write/copyout 결과 `8`과
8개 damaged-fact 거부를 확인했다. 동일 48,531,749-byte fixed MIR의 첫 실패는
global row 17147에서 17618과 17851을 거쳐 18392로 이동했다. 기존 populated
ArrayInt owner도 `[root_id]`의 정확한 instruction use, LocalRef, value type을
소비하며 C/LLVM local operand 실행과 missing-use 거부를 통과했다. 따라서 이
슬라이스는 실제
executable substitution이지만 top-level registry 행을 새로 닫은 것은 아니며,
통합 78%, strict beta 83%, hard SoT 49/86=57.0%는 그대로다. 다음 RED는
`SemanticAstAnalysisResolveExpressionPlacesFromAdmittedBody`의 value-result
`analysis.expression_surfaces.expression_graph = graph`에서 발생한
`stage=admitted-type`다. 기존 member-rebind fact가 exact local/value type과 target
member type을 결합하는 것이 다음 상승 조건이다. 새 V, operation, cache, shard,
timeout, memory cap은 진행 증거로 인정하지 않는다.

2026-08-17 Git 작업 감사 결론은 두 층으로 나뉜다. 전체 dirty aggregate는
823 paths(`419 tracked`, `404 untracked`)이고 top-level registry는 여전히
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`다. 따라서 파일·owner·fixture·V 증가를
합산하면 맴도는 작업으로 보이는 것이 맞으며, 그 수를 진행률로 세지 않는다.
반면 이번 활성 String/scalar-ABI 슬라이스는 세 MIR producer의 동일 identity,
한 typed runtime-symbol owner, old path 삭제·negative gate, current typed-source
candidate 실행까지 이어져 실제 SoT 치환이다. 전체 작업 트리가 정리됐다는 뜻은
아니므로 hard SoT 57.0%, 통합 78%, strict beta 83%를 그대로 유지한다.

현재 callable ABI 전선도 SoT 개수를 늘리는 작업으로 세지 않는다. 하나의
generic-return production 실행에서 첫 실패가 routine 501 `Array<Bool>`, 860
`Option<String>`, 985 composable logical-record return, 1159 `Option<Int>`, 1173
composable owned `Array<String>` return, 그리고 1228
`ParserImportGraphSeen(Set<String>, String)`을 차례로 통과해 routine 1250
`ParseDestructureLetStmt`까지 이동했다. 각 단계는 기존 ABI
receipt와 unique parameter-role plan의 누락된 마지막 소비자를 연결했고, focused
C/LLVM 실행·negative gate와 component/CI-profile/CI-step ratchet이 green이다.
최신 Pergyra-built driver는 5,850,285 bytes, SHA-256
`607034BDA8E5BB9FF5CE7A4DFDBF523FAA11B77D5AB718335FF05C7524AF745E`다.
`Set<String>`은 이제 central ABI row와 기존 Set runtime symbol fact를 통해
spelling 추론 없이 C/LLVM으로 투영된다. 따라서 이번 이동은 기존 BRIDGE 내부의
실행 폭 증가이고 registry 승격이나 퍼센테이지 상승이 아니다. 다음 실행
falsifier는 `ParseDestructureLetStmt` parameter 4의
`Array<AstExpressionGraphRows>` value-result ABI이며, 기존 nominal-array layout과
logical-record-array copyout owner의 마지막 composable consumer를 먼저 확인한다.

현재 활성 source-module provenance 슬라이스도 같은 기준으로 판정했다. 기존
MIR wire는 parser가 소유한 top-level module path를 잃고 canonical AST를
`unknown` provenance로 재구성했으며, 그 결과 합법적인 compiler-internal
storage-retirement owner도 `compiler_internal_builtin`으로 거부됐다. 지금은
declaration/routine MIR row가 exact `source_module_path`를 운반하고, 두 MIR
index가 빈 경로를 admission 단계에서 fail-closed하며, canonical reconstruction은
기존 `AstSourceModuleFacts`를 다시 결합한 뒤 artifact identity를 재봉인한다.
이름/서명만으로 허용하는 fallback이나 `<unknown>` 보정은 없다. 구조 inventory,
13개 early/7개 final AST backing lifetime gate, wrong-path/external/missing-path
negative를 포함한 C/LLVM provenance gate, CI profile과 step-runner가 local green이다.
CI의 기존 serial self-host job도 provenance와 lifetime 게이트를 함께 실행한다.
따라서 이 reached seam은 실제 SoT 폐쇄로 센다. Current isolated codegen seed에서
driver를 다시 만들고, 그 driver가 229,290,183-byte verified MIR을 게시한 뒤 production
consumer가 10,126,081-byte C를 생성했다. Host-compiled gen2 executable이 다시 만든
gen3 C는 byte-equal이며 SHA-256은 둘 다
`3401A5DD1269E3489DF78046F67016C721A387765A995A12F72A532D71014F35`다. MIR의
`source_module_path` 7,430행은 null/empty가 0개다. 즉 current-source full MIR
게시·소비·고정점까지 관측됐다. 다만 이 driver build는 peak private 2.995GiB로
3072MiB cap 아래 여유가 거의 없었다. 이후 옛 6/7필드 선언 모양을 고정한 여섯
direct-MIR 소비자를 같은 declaration-index provenance owner에 교차 봉인했고, 이를
포함하고 enum `<stdint.h>`/CI ABI drift까지 교정했다. 동시 `gen2 | tr` normalization은
compiler 3,063.9MiB에 orchestration private를 겹쳐 aggregate 3.012GiB cap을 넘겼다.
동일 payload owner를 compiler 종료 뒤 직렬 normalization하도록 바꾸자 authoritative
driver는 217.994초/2.938GiB, SHA-256
`C111DAAD3B19F27CC2B087D788775D8F437BF8B3D9207E2267D01B490F5D2A9E`로 빌드됐다.
격리된 public launcher 옆에서 installed C/LLVM 전체 경로와 두 enum focused gate가
모두 green이다. 다만 source-pressure 영수증은 6,727개 definition 구간에서 private가
2,622.4MiB→3,055.3MiB로 증가함을 보였고, remote CI와 의도적인 diff 통합도 아직
없다. 따라서 wider registry row는 BRIDGE이며 `49/86`, 78%, 83%는 올리지 않는다.

정의 생성 구간의 첫 후속 폐쇄도 같은 방식으로 제한해서 센다.
`CodegenFunctionValueBindingFactFor`가 이미 `binding.env_rows`를 만들었는데도
`EmitLet`의 7개 분기가 동일한 source name/type/value kind/C name 행을 다시
직렬화하고 있었다. 이제 그 분기들은 기존 행을
`CodegenTypeEnvStateAppendOwnedLocalRows`에 넘기며, owner는 local environment로
복사한 뒤 임시 backing만 회수한다. 옛 `EmitLet ->
CodegenTypeEnvStateAppendTypedValueBinding` 경로는 기존 type-env preseal focused
gate가 막는다. 이 gate는 exact 7회 admitted-row 소비와 copy→install→retire 순서,
rebuild 부재를 고정한 뒤 installed C/native-pipeline LLVM ordered-delta 실행을
7.3초에 통과했고, 기존 Linux serial self-host CI invocation에도 연결됐다. CI
profile, 전체 component gate, `string_concat_op` C/LLVM focused parity는 green이다.
`bool_logic`의 실제 10행/expected 9행 차이는 이 변경과 무관한 현재 dirty-tree
oracle drift로 RED 상태를 유지한다. Fresh Pergyra-built codegen seed의 SHA-256은
`0A1068EB4C76F6CBCE24AEF4C631BDF3FC840CAB31356AF57F12EA1D150AC202`다.
같은 3072MiB/300초 process-tree 경계에서 current-source pressure는 113.582초에
`definitions:done:6728`과 `output:finished`까지 exit 0으로 완주했다. 정의 구간
private 증가는 2,622.5MiB→3,038.8MiB, 즉 416.3MiB로 직전 432.9MiB보다
16.6MiB 줄었다. 그러나 전체 peak 3,038.8MiB는 여전히 2.4GiB attention 선을
넘고 hard cap 여유가 33.2MiB뿐이며, 정의 구간 시간도 2.999초→3.878초라
속도 개선은 주장하지 않는다. 즉 이 한 소비 경계는 실제 치환이지만 broader
definition lifetime과 813-path aggregate가 통합됐다는 뜻은 아니다. Registry
승격 없이 SoT 57.0%, 통합 78%, strict beta 83%는 그대로다.

같은 owner의 다음 반복도 범위를 넓히지 않고 제거했다.
`TypeEnvAppendLocalRows`는 admitted local row를 앞에 붙일 때 기존 prefix를
`Substring` suffix, `combined`, 선두 `|` 복원으로 세 번 복사했다. 이제
`Concat(rows, local_rows)` 한 번만 수행하고, local-row scan owner가 offset 0의
첫 행과 기존 delimiter 행을 하나의 row-start fact로 판정해 value/presence lookup이
공유한다. Focused gate는 옛 세 단계 재구성을 거부하고 first-row 조회,
newest-first shadowing, malformed preseal 거부를 C/native-pipeline LLVM으로
검증한다. 전체 component와 `string_concat_op` self-host C/LLVM도 green이다.
Fresh gen2 SHA-256은
`9174B6583E01191C7440C0065A375E3A71655D480AE3306DF07D9E66CF99332E`이며,
동일 3072MiB/300초 pressure에서 106.705초, `definitions:done:6729`,
`output:finished`, peak private 3,063.1MiB로 exit 0이다. Definition marker 구간은
2.738초로 짧아졌지만 단일 실행이므로 속도 개선으로 확정하지 않는다. 50ms 요청
sampler도 `definitions:done`을 3,002.4~3,040.1MiB 사이에서만 포착해 메모리
감소를 증명하지 못했다. 따라서 old repeated operation 삭제는 실제 치환으로 세되,
3GiB definition-lifetime blocker나 registry row를 닫았다고 세지 않는다.

그다음에는 SoT 목록을 더 늘리지 않고 실제 할당 소유자를 계측했다. 이전
Pergyra-built codegen C에 `.tmp` 전용 malloc/realloc/free 계측을 링크해 동일
3,972,166-byte 출력과 SHA-256
`32FD6565FCBFC2E202C6AA6FB2303B0FAF93AB3A928BF64B5EBBA941FC4356EF`를
확인했고, 추적 누락은 0이었다. Codegen 자신의 2,900-definition AST에서 정의
블록은 direct-live payload를 353,146,582→404,218,328 bytes 늘리는 동안
8,285,484회 할당했다. 함수 스택별로 분해하자 `CodegenCharAt`의 439만여
한 글자 String 할당 대부분이 `CsvAt`과 `ParamModeCsvCount`의 쉼표 검사였다.
payload 자체는 각각 6.90MiB/1.39MiB였지만 수백만 allocation의 Windows heap
metadata가 훨씬 큰 private-memory 비용을 만들었다.

두 함수는 이제 기존 `CodegenCharCodeAt(..., length, index) == 44` fact를
소비한다. 새 owner/cache/builtin/표면 문법은 없고 loop 안 한 글자 String
재생성은 negative gate가 막는다. First/middle/last/out-of-range CSV와 empty/3-row
mode fixture는 installed C/native-pipeline LLVM에서 green이고, 전체 component와
`string_concat_op` self-host C/LLVM도 green이다. Fresh gen2 SHA-256은
`F4E1452EB634725C040961C832B667C89E5C26A19A0BAAD6D20F89458B4FF73A`이며,
gen2/gen3 raw C는 3,972,162 bytes/SHA-256
`AED3A59592F6D20662D9BD2C0805E3F3EE5BB2B2206E2A28A48BC363B1C85847`로
byte-equal이다.

같은 6,729-definition driver source와 3072MiB/300초/50ms 조건에서는 172개
stage와 `output:finished`까지 116.577초, peak private 2,890.8MiB로 exit 0이다.
직전 106.705초/3,063.1MiB와 비교하면 peak가 172.3MiB 줄었다. Definition 시작은
2,622.4/2,622.9MiB로 같지만 완료 직후가 3,040.1→2,867.6MiB라 감소 위치도
정의 구간과 일치한다. 시간은 9.872초 늘었으므로 속도 개선은 주장하지 않는다.
Peak는 아직 2.4GiB attention 선 위이고 wider lifetime과 remote CI가 남아 있다.
따라서 이 작업은 reached BRIDGE 내부의 실제 lifetime substitution이지만 registry
승격은 아니며 SoT 57.0%, 통합 78%, strict beta 83%를 그대로 유지한다.

같은 실행 경계에서 다음으로 큰 allocation owner도 새 SoT 행 없이 닫았다.
`SemanticCallSpineViewFromGraph`는 parser call-spine의 argument와 generic actual을
역순으로 수집한 뒤 별도 ordered Array 두 개로 다시 복사하고 있었다. 이제 기존
두 backing을 `ArraySet`으로 제자리 역순 복원해 그대로 반환하며, focused/component
gate가 두 번째 `arguments`/`actuals` 재구성을 거부한다. Clean, callable-resolution,
target/nested/explicit mismatch 다섯 모드는 current native C/LLVM에서 기대 stdout과
exit status가 모두 같고 installed self-host C도 green이다. Installed self-host LLVM은
별도 direct-MIR projector의 `SemanticAstGenericParameterDefaultRowsFromNode` 2번
`Array<String>` value-result parameter에서 RED이며, 이를 call-view green이나 fallback으로
숨기지 않는다.

이 변경을 포함한 isolated Pergyra-built gen2는 2,490,207 bytes, SHA-256
`4C9D3E31C22AAFF9ED48CF0E548255BEE9FE5FEDF3BF59D98DB748DE377DEF3D`다.
Gen2/gen3 normalized C SHA-256은 둘 다
`6C92CC343AE3E80BD77444C1F82CF35C1CA2CDFB71C38829B08801D396E9B2A5`다.
동일 6,729-definition driver source와 3072MiB/300초/50ms 조건에서 172개 stage와
`output:finished`까지 118.113초, peak private 2,823.5MiB로 exit 0이다. 직전
CSV-charcode 영수증보다 peak가 67.3MiB 줄었다. Definition 시작/완료 직후 표본은
2,640.2→2,867.6MiB에서 2,602.2→2,798.0MiB로 바뀌어 구간 증가가
227.4→195.8MiB, 즉 31.6MiB 감소했다. Marker 구간은 2.637→2.614초지만 전체
실행은 1.536초 느려 속도 개선은 주장하지 않는다. Peak는 여전히 2.4GiB attention
선 위이고 installed self-host LLVM, canonical install, remote CI도 남았다. 따라서
기존 BRIDGE 내부의 실제 lifetime 치환으로만 세며 49/86, 78%, 83%는 유지한다.

그다음 작업도 SoT 목록을 늘린 것이 아니라 generic-return production 경로의 첫
실패를 두 번 전진시킨 실행 치환이다. 기존 ArrayBool storage receipt가 by-value
`Array<Bool>` formal을 소유하게 했고, 기존 OptionString layout receipt가 완전한
by-value parameter ABI fact를 instruction receipt와 교차 봉인하게 했다. Parameter
role plan은 이를 하나의 admitted ABI-value 역할로 분류하며 routine admission은
parameter JSON이나 물리 layout을 재구성하지 않는다. 최신 Pergyra-built driver는
5,837,354 bytes, SHA-256
`43F93550C66B01B30AB45D7B889AD2B79EAA4BD6B3F1A51AF509A0C865EC10BB`다.
이 driver에서 OptionString, ArrayInt/ArrayBool, ArrayString focused gate가 모두
C/LLVM 실행과 ABI/carriage/pass-shape negative를 통과했고, component structural
gate와 CI profile도 green이다. 세 focused target은 기존 serial
`self-host-direct-mir-scalar-graph-plan-test-smoke`에 이미 연결되어 새 workflow job은
필요하지 않았다.

전체 generic-return C leg는 통과하고 self-host LLVM projector는 옛 routine 501과
860 경계를 지난 뒤 routine 985 `SemanticAstInitializerEnvironmentCursorAdvance`의
`SemanticAstInitializerEnvironmentCursor` return에서 fail-closed한다. 이 타입은
이미 declaration index가 소유하는 `Bool + Int×4 + Array<Int>` 여섯 필드 record지만,
동일 signature에는 논리 record by-value 다섯 개와 `Array<String>` value-result 세
개도 함께 있다. 따라서 다음 작업은 새 타입/owner를 만드는 일이 아니라 기존
logical-record fact 누락과 compositional role join 누락을 먼저 가르는 focused
falsifier다.
Artifact는 게시되지 않았고 canonical install/remote CI도 하지 않았다. Registry는
여전히 `49 CLOSED / 36 BRIDGE / 1 ACTIVE`, 통합 78%, strict beta 83%다.

정의 블록의 TextBuilder를 하위 emitter에 직접 넘기는 더 큰 가설은 구현 전에
기각했다. 현재 언어 계약은 TextBuilder parameter를 copy/borrow/transfer 미증명
경계로 fail-closed한다. 성능 가설을 위해 ownership 예외나 새 builtin, monolithic
emitter를 만들지 않았다.

7개 `EmitLet` 분기의 `type name = value;` framing을 기존 binding owner 한 곳으로
모으는 더 작은 가설도 C/LLVM·component green 뒤 production pressure로 반증했다.
해당 gen2 SHA-256은
`9E6732E8E453A1C49E6AC73CD0FC2EFB3496B049611834F6AA5373B152FAA1AD`였지만
peak private는 3,061.0MiB로 직전 3,063.1MiB보다 2.1MiB 낮은 데 그쳤고 시간도
개선되지 않았다. 실행·sampling 변동을 넘는 효과가 아니므로 새 함수·7개 call·gate·
owner 문구를 모두 되돌렸다. 이것도 진행률에 포함하지 않는다.

같은 reached rung에서 `CodegenPrefixOwnedStatementLine`을 단일 TextBuilder로
바꾸는 가설도 실행으로 반증했다. 첫 500ms 표본은 정의 구간이 63.8MiB 줄어든
것처럼 보였지만, 같은 seed를 50ms 요청 간격으로 다시 관찰하자 경계는
2,622.7MiB→3,035.5MiB, 약 412.8MiB였다. 직전 416.3MiB와의 3.5MiB 차이는
실행·sampling 변동을 넘어선 효과로 볼 수 없고 support tail도 약 22MiB 늘었다.
C/LLVM 의미는 green이었지만 reached cost를 닫지 못했으므로 이 실험의 source와
structural-gate 변경은 되돌렸다. 이 반증은 진행률로 세지 않으며, active blocker는
여전히 definition 내부 lifetime이다. 최종 payload assembly를 새 owner track으로
승격하지도 않는다.

## 과거 실행 증거 보관 — 현재 작업 큐가 아님

아래 기록은 퍼센트 산정과 원인 비교에 필요한 과거 영수증이다. 현재 활성 작업은
위 source-module provenance 통합 경계 하나이며, 아래의 runtime-value, MIR consumer,
GraphPlan, V 번호를 별도 활성 큐로 되살리지 않는다.

이번 runtime-value ABI 검증 슬라이스는 그 구분을 실제 대형 실행으로 다시
확인했다. 최종 instruction validator가 runtime-value 행마다 여섯 candidate fact를
materialize하고 256행 serialized runtime-call registry를 반복 scan하던 경로를
allocation-free stable identity receipt 소비로 치환했다. Old fact-materializer read는
component negative gate가 거부하며, full fact materialization은 실제 codegen
consumer에만 남는다. Runtime-value lifecycle C/LLVM negative와 runtime-call ABI
manifest parity가 green이고, fresh current-source C 생성 및 canonical host compile도
각각 exit 0이다. 같은 3072MiB 제한의 source-to-verified-MIR 실행은 116.270초,
peak private 2.812GiB로 row 90,112, local refs, instruction ABI, blocks, routines,
`mir-facts:done`, `json-write:done`을 모두 통과했다. 결과 MIR은 228,492,268 bytes,
SHA-256 `F338B0E4F8EDFBAF490E4994726725A32A8E34F6F80160041A98001B79BA773E`다.
이전 run은 row 73,728 부근에서 3.203GiB cap 종료였으므로 이 한 seam은 실제 SoT
폐쇄로 센다. 다만 top-level registry row 승격, installed-driver 교체, current remote
CI green은 아니므로 SoT 57.0%, 통합 78%, strict beta 83%는 올리지 않는다.

다음 production rung인 current MIR consumer는 아직 RED다. 동일 executable과
228,492,268-byte MIR을 `--mir-json --observe-mir-consumer-stages`로 소비한 결과
메모리는 peak private 0.616GiB로 안정적이었지만 300초 경계에서 routine
6,464/6,704, output 미게시 상태로 timeout됐다. 가장 느린 완료 interval은
3,008→3,072의 12.605초이며 4,646,507 raw bytes, 840 blocks, 2,011 instructions를
포함한다. 101개 완료 batch 상관은 instruction `r=0.5833`, raw bytes `r=0.5342`,
sum(blocks×conditionals) `r=0.1605`, sum(blocks²) `r=0.1818`이다. 따라서 현재
증거는 broad CFG 제곱 최적화를 다음 해법으로 승인하지 않는다. 다음 단일
falsifier는 이 interval의 최대 routine 3,056에서 fact-index와 validation/header/
region-render 시간을 분리하는 것이다. 그 전에는 timeout 상향, cache/FactStore,
또 다른 registry owner 확장을 진행으로 세지 않는다. 240초 focused 시도는 host
편차로 routine 2,880까지만 도달해 해당 receipt를 만들지 못했고, 임시 ordinal
focus와 gate 변경은 곧바로 제거했다. 이 실패 관측은 진척으로 세지 않는다.

이번 활성 codegen 문자열 수명 슬라이스는 이 넓은 aggregate와 다르게 실제
production 경계를 바꿨다. semantic expression graph는 그대로 한 fact owner이며,
재귀 C-expression 자식 문자열과 let/assignment/bind/log statement 문자열은
각각 최종 root/line 생성 직후 기존 lifetime owner에서 회수된다. 새 owner, V,
cache, shard, timeout, memory-cap 증액은 없다. Codegen gen6/gen7 C는
3,965,061 bytes와 SHA-256
`86FC064C8B9E6E9AB78104154D671BB3F7F3A965134924AFADA9F97F0F95CF28`로
byte-equal이다. 동일 3072MiB 경계에서 codegen은 127.589초, peak private
3051.6MiB로 `output:finished`까지 완료했고, canonical compiler build도
258.283초, peak private 3041.1MiB, exit 0으로 host compile/source smoke/
machine-manifest replay를 끝냈다. 최신 9,695,682-byte C artifact SHA-256은
`2AC69466D8F9C80570A7B412964E4F930CFCBF6BA8AF8A5EBE41D5BA1109876B`,
5,804,704-byte 임시 driver SHA-256은
`386DDC7FE6F05E57915DA87A7D0E620D0C3153AD811C7C10C38622F0CA50F2C5`다.
String 및 Long division/remainder C/LLVM, component, build inventory,
likeness, size, CI profile/step-runner 게이트가 green이다. 후보는 `.tmp`에
격리되어 있고 remote CI는 아직 committed HEAD의 RED이므로 SoT 57.0%, 통합
78%, strict beta 83%는 올리지 않는다.

현재 최신 설치 영수증은 intent 실행 관측 projection까지 포함한
Pergyra-built DRV-2다. 크기는 5,766,328 bytes, SHA-256은
`76B05F94576EC9EA2F4F61E5FE6CFC380F89559AA34B73FA180C819A14FFDC37`다.
그 직전 고정 MIR 통합 영수증은
`runtime-value-current-consumer-growable-array-string-20260816-v36`이다.
41,051,560-byte/1,660-routine 고정 consumer는 118.815초에 exit 0,
`direct-mir:projection:done`까지 도달했고 2,827,611-byte C artifact를 게시했다.
Peak private는 2.511GiB, working set은 2.438GiB로 3072MiB hard cap 이하지만
2.4GiB attention threshold는 넘었다. 생성 C는
`-Werror=free-nonheap-object` host link를 통과했고 523,305-byte executable은
같은 MIR을 `--verify-input`에서 1.746초/0.049GiB, exit 0으로 검증했다.
MIR 161/161, 816초 full GraphPlan aggregate, component inventory, build
inventory, local CI profile/step, likeness, machine manifest와 affected C/LLVM
gates도 green이다. 같은 레코드의 owner-handle 반환과 value-result formal이
겹치던 callable admission은 fail-closed로 분리됐고, legal value+copyout은
positive fixture로 보존됐다. 최신 remote CI run은 이 uncommitted worktree보다
  이전 committed HEAD의 RED이며 새 run은 없다.
  Self-host scalar ABI의 `Int`/`Long` conflation도 기존 ABI owner에서 닫혔다.
  `Int`는 `int32_t`, `Long`은 `int64_t`, `Array<Int>`/`Array<Long>`과
  `Option<Long>`은 서로 다른 C/LLVM runtime carrier를 소비한다. ABI manifest와
  focused long-division/conversion/projection gate가 green이고,
  `array_scalar_aggregate_core`는 exact `4294967297`, `-2147483648`, `1.500000`,
  `true`, `option_int_core`의 Long 경계는 exact `4294967297`, `0`을 실행한다.
  이것은 기존 scalar ABI seam의 hard closure이며 새 registry 행이나 퍼센트
  상승으로 중복 계산하지 않는다.
현재 typed intent 실행 wire는
`pgy.selfhost.mir-intent-execution-plan.v3`로 전진했다. 각 step은 exact
`where_zone_name`과 zone declaration syntax ID를 함께 운반하고 native producer,
native/self admission, mutation digest, C/LLVM consumer가 한 identity로
cross-seal한다. Stage-0 self C와 native C/LLVM compensation gate는 green이지만,
  이전 canonical Pergyra-built gen2는 unchanged 3072MiB 경계에서
  `definition:done:2432`에 멈췄고, 첫 expression epoch는
  `definitions:done:6716`까지 전진했다. 현재 call/member lifetime epoch까지
  포함한 Pergyra-built codegen은 typed
  `--observe-source-pressure src/self_hosted/compiler/driver_bootstrap_main.pgy`
  를 128.609초, peak private 3066.5MiB로 exit 0 완료해
  `definitions:done:6718`, `support-blocks:done`, `output:finished`를 기록했다.
  9,840,366-byte C artifact(SHA-256
  `257CAFE0C9A87D60F03A33C932B04C3F6E35AE9ABC719CCDD10C604CA9E9B3D6`)는
  canonical host flags로 5,802,628-byte executable(SHA-256
  `6181B9F28F9EBFF6B408D3A3DD3B4B00D5AC757BA8B3525AD9D50BD4E38FB598`)까지
  컴파일됐다. 이 candidate와 installed DRV-2의 `hello.pgy --tokens` 실행은
  둘 다 exit 0이며 byte-identical SHA-256
  `A59B414C2FC153AEA8F008913E3BBE7736FF29C27AB3C744289945DC7B1A29DD`다.
  새 Pergyra-built codegen이 자신의 current AST를 다시 생성한 gen3/gen4 C도
  byte-identical SHA-256
  `71C63F0415648B599FB5D35AAAF2D95E24787E9BD2AD54DC01BD23E3B4A7FEF3`다.
  다만 cap 여유가 5.5MiB뿐이고 driver executable은 아직 설치되거나 full-driver
  fixed point로 봉인되지 않았다. 따라서 installed v2 typed transition의 bounded
  `SUBSTITUTING` 증거는 유지하되 v3 zone/observability delta는 replacement DRV-2가
  생기기 전까지 `REACHABLE`로 기록한다. Registry 분자와 통합 78%는 올리지 않는다.
  Scalar builtin runtime-call ABI ID는 이제 signature fact가 arity/type/kind와
  함께 한 번만 소유한다. Call admission은 MIR-carried ID를 이 fact와
  cross-seal하며 intent/runtime-value registry를 source name으로 다시 읽지
  않는다. Call/signature owner는 각각 115/115, 205/205이고 cap은 올리지 않았다.
  Current-source check와 새 Pergyra-built driver 생성(124.961초, peak private
  3035.2MiB), host compile, native/self MIR 및 direct C/LLVM ABI-identity 실행이
  green이다. 새 candidate SHA-256은
  `6E0FD990A67D6958B787AE5A0829DE5D268FBB779962178E301FA0D685FB04E6`다.
  이어진 compile-time declaration/literal-Log family는 27-line semantic
  pre-scan을 shape-only route claim으로 교체하고 final semantic 판정은 erasure
  fact/plan 한 경로에 남겼다. malformed declaration-bearing 입력은 더 이상
  scalar fallback으로 재시도하지 않는다. 동시에 옛 Int=64 출력 가정을 제거해
  C `int32_t/%d`와 LLVM `i32`를 일치시켰다. Family는 560/560이고, focused
  C/LLVM 실행(ability 7, literal 73, metamorphic positives, 25 negatives)과 full
  component inventory가 green이다. 두 closure를 포함한 최신 candidate는
  142.920초, peak private 3069.8MiB로 hard cap 안에서 생성·host compile됐고,
  SHA-256은
  `E833ACE13B5B3E419B02EA22EFA897193F861E10E9AD5B3C8847AF9947521F3D`다.
  native/self MIR 및 direct C/LLVM ABI identity와 installed token byte parity도
  green이다. 일반 String-builtin drift는 세 독립 producer를 비교한 뒤
  normalized 16,434 bytes/SHA-256
  `C39CF0215F9ACA7CA5841D027966786C418967831A66ADE527FD05B9A04E03CA`로
  폐쇄됐다. Fixture는 `ToString` 결과 `foo`를 실제로 관찰하고 current
  candidate가 positive와 8개 malformed-MIR C/LLVM case를 통과한다. 같은
  candidate에서 `Int`와 `Long`이 shared `int_c_type`으로 합쳐지던 C ABI
  consumer도 분리해 exact `int32_t`/`int64_t` signature와 Long MIN/-1,
  zero-divisor negative를 실행했다. 최종 9,841,295-byte C artifact SHA-256은
  `C7BA467A324EA4283482489A09FEB0B532225E714BC937305294C4603389F819`,
  canonical host executable 5,804,082-byte SHA-256은
  `5940CDC022DCB7B16C5611B3BA41F4BF41806D70436E9F2CD7339BE6D624CFBC`다.
  아직 설치·current full-driver fixed point·remote CI는 아니다.
별도로 parser-tool source-set fingerprint는 2,011개 파일마다 `sha256sum`을
기동하던 Windows CI 증폭을 sorted NUL-delimited batched hashing으로 바꿨다.
동일 source set 2,011행의 deterministic fingerprint를 약 6.6초에 만들며,
이는 CI 측정 owner 폐쇄이지 언어 SoT 대체 진척은 아니다. 이번 ABI owner
이동도 top-level registry 행 전체를 닫지는 않으므로 SoT 57.0%, 통합 78%,
strict beta 83%는 유지한다.
추가로 현재 소스에서 새로 컴파일한 C/LLVM semantic checker가 각각 1,522개
`src/self_hosted` 실소스를 모두 받아 backend당 1,522/1,522, 총 3,044회
real-source selfcheck가 green이다. 이전 완료 기준은 backend당 1,520개,
총 3,040회였다. 진단 registry는 실제 emitter vocabulary에 맞춘 33개이고,
artifact/legacy body 검사는 하나의 collection-mutation caller contract를
소비한다. 이제 complete internal-builtin caller tuple은
`src/common/compiler_internal_builtin_caller_registry.def` 한 곳이 소유하고,
parser-owned `AstSourceModuleFacts`가 import-composed artifact의 선언별 module
path를 semantic signature까지 운반한다. C/LLVM legacy checker와 artifact
분석은 real owner를 승인하고 ordinary external caller 및 exact 이름/서명
wrong-path impersonation을 `compiler_internal_builtin`으로 거부한다. focused
provenance/lifetime, component, compiler-world, build inventory, generated
registry, CI-profile gate가 green이고 CI에도 focused provenance step이
추가됐다. 이것은 ACTIVE semantic-admission family 안의 bounded BRIDGE를
닫는다. 이어 public installed-self-host LLVM과 in-process native LLVM이
`compiler_runtime_cache.c`의 같은 LLVM runtime-object owner를 소비하도록
로컬 build/cache/publish 복제를 삭제했다. `io_probe.pgy`는 양 경로에서
compile/run 0으로 `exists`, `missing`, `has-main`을 출력하고, unresolved
runtime symbol은 기존 gate에서 계속 fail-closed다. Typed-plan observability는
현재 v3 step-zone identity와 함께 stage-0 self C/native C/LLVM까지
`REACHABLE`해졌지만 replacement installed driver가 아직 없고,
non-default-priority, compiler-root intent, composite-intent 의무도 남아
top-level SoT 승격은 보류한다.
이 경계를 통합하던 두 로컬 RED도 실제 소유 경로에 맞춰 닫았다. Hard-contract
gate는 현재 runtime-ABI owner와 modulo-zero/signed-add 실행 계약을 검증하며,
runtime-cache identity gate는 cache producer인 native pipeline을 명시한다.
세 exact Make owner gate(runtime C-extern, cache identity, hard contract)가 한
실행에서 green이고, 기존 단일 self-host Make invocation에 hard contract를
추가한 CI profile과 step runner도 green이다. 이것은 회귀 방지 증거의 완성이지
새 top-level registry 승격은 아니므로 퍼센테이지는 그대로 둔다.
Production driver bootstrap은 이제 codegen의 typed `--source`
artifact를 직접 소비하고 provenance-free parser executable/AST-text detour를
갖지 않는다. 설치 owner인 `self_host_compiler_build.sh`도 같은 typed-source
경로로 이전했고, 실제 emitted-C 해시가 설치 캐시 키를 소유한다. AST-text
route의 module provenance는 `Unknown`이며 internal builtin 사용은 fail-closed다.
Typed production bootstrap의 현재 재실행도 exit 0이다. Pergyra-built seed와
native oracle이 각각 C를 생성하고 host compile된 뒤 sample C, bounded MIR
publication, bounded MIR consumption에서 일치했다. 이 과정에서 self-host
source checker가 놓친 native ownership 오류 82개를 4개 root owner shape로
축약해 닫았고, 이어 generated C의 `literal`/`target` SSA 이름 충돌 두 개도
타입별 local identity로 분리했다. 최종 artifact는 seed C 9,651,147 bytes,
seed executable 5,765,044 bytes, oracle C 29,244,684 bytes, oracle executable
6,405,399 bytes다. 따라서 typed provenance slice는 focused gate뿐 아니라
production seed/oracle 실행 경계까지 통합됐다. 다만 이것은 registry의
36 BRIDGE와 1 ACTIVE 전체를 닫은 것이 아니므로 top-level 상태 승격은 없다.
이때 드러난 회귀 모양도 기존 component 계약에 고정했다. expression admission의
typed `literal_admission`, C/LLVM expression의 `callable_target`, typed-return
owner의 standalone bare `return` 금지를 source ratchet으로 두었고, 기존
expression-admission 상한은 올리지 않고 445/445로 유지했다. Component
inventory와 populated `Array<Int>`, logical-record array value parameter,
`Option<Int>` try-let C/LLVM parity가 모두 통과했다. Build-source inventory와
local CI profile도 green이다. 이는 새 완료 분자가 아니라 방금 얻은 production
대체 증거가 같은 실수로 후퇴하지 않게 만든 negative closure다.
현재 progress metric은 implementation/frontend/backend 65,962 LOC,
compiler core 197,798 LOC, 19.82%, default C emit `substituting`, full default
compile `open`, explicit DRV-2 `live`다. 이 LOC 변화는 완료 분자로 세지 않는다.
  현재 dirty 775개 경로(`381 tracked`, `394 untracked`)에는 누적 GraphPlan work와 이번 MIR identity carriage가
함께 들어 있지만 top-level registry census는 HEAD와 같은
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`다. 따라서 이 전체 파일 폭은 SoT 폐쇄
진척이 아니다. Fixed 1,660-routine publication과 이번 provenance-free
bootstrap detour 삭제는 실제 대체 증거지만, 새 GraphPlan owner/V 확장은
통합 경계까지 동결한다. Top-level SoT 승격과 current remote release evidence가
없으므로 SoT 57.0%, 통합 78%, strict beta 83%는 유지한다.

Intent-observability의 설치 C/LLVM 슬라이스는 실행 경계까지 닫혔다. 51행 `.def`
projection이 self-host codegen usage receipt와 runtime symbol rewrite를 직접
소유하고, 사용된 프로그램에만 enabled runtime header를 낸다. Public installed
C/LLVM과 두 native oracle은 `IntentHistoryCount()`(0인자/Int),
`IntentActiveConcurrent(0)`(1인자/Bool), `IntentActiveStepName(0, 0)`
(2인자/String)을 모두 실행하며 exact stdout `0`, `false`, 빈 문자열을 낸다.
두 public installed 경로는 native pipeline 재진입을 하지 않는다. Registry
gate는 `86 authorities / 168 derived fact carriers`와
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`로 green이고 CI step/profile도 green이다.
Native 7-field와 self-host 10-field `pgy.mir.v1` expression fact는 이제 canonical
`RuntimeCallAbiId`를 싣는다. Direct-MIR admission은 source/name/shape row와 carried
ID를 한 번 교차 봉인하고 C/LLVM은 `RowForId`만 소비한다. 같은 0/1/2인자 fixture의
native/self MIR 네 direct-backend 조합은 exact stdout을 보존했고, missing,
mismatch, non-observability forged ID, source-syntax/runtime-ID conflict는 모두
artifact publication 전에 실패했다. 새 focused gate는 기존 serial self-host CI
job에 연결됐다. 이어 installed self-host의 default-priority legacy intent emitter가
admitted mode/signature/participant/zone/slot fact에서
`enter/step/bind/materialize/fail/ok/exit`를 투영한다. Success와 첫 guard 실패,
둘째 step의 guard/expect/post 실패, 역순 compensation, history 2행,
`post:ForwardB`, exit 뒤 active count 0이 self C/native C/native LLVM에서 동일하다.
LLVM compensation은 forward materialize 사건을 다시 발행해 fail phase를
덮어쓰지 않는다. Typed v2 execution-plan observability와 non-default priority,
compiler-purpose root intent는 남아 있으므로 `abi.intent_observability_rows`는
BRIDGE이며 전체 퍼센테이지는 오르지 않는다.

### 퍼센테이지 해석 규칙

- `V72`, `V76` 같은 V 번호는 작업 체크포인트이지 제품 버전이나 퍼센트가
  아니다. V가 하나 늘었다고 통합 진행률을 자동으로 올리지 않는다.
- `.tmp` 파일 수, owner 파일 수, gate 수, LOC는 진척 분자가 아니다.
- 퍼센트는 `CLOSED`, `SUBSTITUTING`, fixed-point receipt, current CI처럼
  되돌리기 어려운 증거가 생길 때만 오른다.
- focused gate는 한 owner의 정확성을 증명하고, aggregate gate는 이미 닫힌
  owner들의 회귀를 막는다. 둘을 각각 별도 진척으로 중복 계산하지 않는다.
- 2026-08-15 current-source driver는 최종 Pergyra-built DRV-2에서 194.0초에
  설치됐다. 크기는 5,728,025 bytes, SHA-256은
  `E7482523B5FE67856A2E1A37AF022A038099D6E02306F0F8B0A9D50E920B6E33`다.
  Bool 1+ ArrayString과 ArrayBool-bearing collection copyout가 기존
  callable/array ABI/LocalRef owner를 소비하고, LLVM은 exit-block별 copyout
  identity를 쓴다. Fixed canary는 routine 1572/1574를 넘어 source syntax ID
  37479, routine index 1575의 2+2 mixed collection policy에서 fail-closed했다.
  이 증거는 다음 type/count owner를 정하지만 V 번호나 통과 routine 수를
  퍼센테이지 분자로 세지 않는다.
- 현재 routine-admission receipt는 global row/source/stage를 보존한다. 그
  receipt가 지목한 local `Array<Bool>` push는 stable operation 39로 닫혔고,
  source syntax ID 5428의 rebound value parameter는 ordered parameter-set fact와
  canonical parameter LocalRef를 통해 entry `.0`/phi/C/LLVM 초기화까지 닫혔다.
  최종 Pergyra-built self-driver는 5,529,557 bytes, SHA-256
  `4F2931E649D9BF708635223750C138A3D77CF98DAFEEA8DD20061DCA800A84A6`이며,
  canonical build는 223.1초에 통과했다. Array mutation(23.9초), diagnostic
  receipt(14.0초), value-parameter rebind(8.0초), CI profile(6.2초), component
  structural inventory(560.0초)가 green이다. 최종 드라이버에서 mutation과
  diagnostic pair는 38.0초, value-parameter rebind는 14.0초에 다시 통과했다.
  Fixed canary는 234.034초 뒤 global expression row 3227/raw routine row 229
  `MirRoutineGraphDistances`의 populated `Array<Int>` literal `[start]`에서
  fail-closed한다. 다음 seam은 exact `parameter:5969:2` identity/type/source
  order를 기존 literal materializer까지 전달하는 것이다. Routine spelling/size
  분기, same-type formal 추정, backend MIR reread, 새 graph/cache는 금지한다.
  Full GraphPlan aggregate와 remote CI는 재실행하지 않았다.
  이 seam은 이어서 닫혔다. 최종 driver는 5,531,272 bytes, SHA-256
  `9DBCAD01EFCDD71227938F08701765652CCE90255BEAFC341422ED5B8037DDF8`이며,
  populated literal/parameter C/LLVM gate가 12.9초에 통과했다. Fixed canary는
  241.300초 뒤 row4338/raw routine252의 nested record-return expression으로
  이동했다. 첫 rejected nested owner는 아직 receipt가 없으므로 다음 작업은
  broad record/index 허용이 아니라 exact failure stage/node 진단이다.
  이 진단은 `builtin-call@9`를 보존했고, nested constructor의 enclosing
  expected-type 재검증을 제거한 뒤 scalar multi-record C/LLVM gate가 12.1초에
  통과했다. 최종 driver는 5,537,253 bytes, SHA-256
  `46EB4248737E2C71B5B3987D22D60059E9C258FF458936601C15F23D50313188`이다.
  Fixed canary는 210.828초 뒤 row4360/raw routine258
  `MirAbiLayoutMulMod`의 Long remainder로 이동했다. 다음 작업은 dynamic
  divisor/overflow semantics를 소유하는 safety fact이며 C signed UB 허용은
  금지한다. Component inventory와 local CI profile은 각각 343.5초와
  6.1초에 green이다.
  이어 dynamic Long remainder가 stable expression 75와 runtime-call ABI
  row246으로 닫혔다. Focused C/LLVM gate는 ordinary `2`, `INT64_MIN % -1 == 0`,
  zero-divisor panic, malformed type/kind를 12.5초에 검증했고 runtime ABI
  manifest parity와 Int-divide 회귀 gate도 각각 37.4초와 6.2초에 통과했다.
  최종 driver는 5,619,967 bytes, SHA-256
  `73CEE796D3C7AF524E61E20CB86F84EC0A1790284F56B2DCD8F4841BDD916058`였다.
  Fixed canary는 203.644초 뒤 row4360을 넘어 같은 raw routine258의
  Long loop-header phi row4363에서 fail-closed했다. 다음 seam은 기존
  PhiValue owner의 exact Long type/predecessor admission이며 새 opcode나
  routine spelling 분기는 금지한다. 현재 component inventory, local CI,
  progress metric, documentation quality는 각각 348.6초, 4.7초, 23.9초,
  최종 문서 수정 뒤 5.5초에 green이다. Remote CI와 full GraphPlan aggregate는
  실행하지 않았다.
  Registry 승격이 없으므로 SoT hard closure 57.0%, 통합 78%, strict beta 83%는
  모두 유지한다.
  이어 기존 common PhiValue classifier에 Long을 추가했다. 새 opcode나
  backend 분기는 없고, true/false `29`/`11`, wrong incoming type,
  non-dominating incoming, missing incoming을 C/LLVM에서 검증한 focused gate가
  9.4초, 기존 collection PhiValue 회귀가 8.3초에 green이다. Current-source
  driver는 223.3초에 설치됐고 5,619,967 bytes, SHA-256
  `E1A7E97A39E3AD8CB38E58A1E95D6D3993C459AE00B9612679885B0E792AEF87`이다.
  Fixed canary는 214.910초 뒤 phi rows4363-4365를 넘어 row4366
  `right > 0L`의 `expression-kind node=2`에서 fail-closed했다. 다음 seam은
  기존 comparison/branch owner의 exact Long operand/Bool-result identity다.
  Component inventory와 local CI profile은 각각 349.2초와 6.0초에 green이다.
  최종 progress metric은 20.8초에 implementation/frontend/backend 65,124 LOC,
  compiler core 191,251 LOC, 19.57%, default C emit `substituting`, full default
  compile `open`을 재확인했고 documentation quality는 최종 문서 수정 뒤
  8.7초에 green이다.
  Registry 승격이 없으므로 세 퍼센테이지는 다시 유지한다.
  이어 Int-only comparison owner를 typed comparison family로 치환하고 기존 Int
  identity 7/11/60/61/66/67은 보존한 채 exact Long greater/equality identity
  76/77을 append했다. 두 비교의 C/LLVM true/false와 wrong type/kind focused
  gate는 17.9초, 기존 Int comparison/wrap 회귀는 11.8초에 green이다.
  GraphPlan은 carrier 변경 없이 v54로 전진했다. 새 current-source driver는
  5,621,222 bytes,
  SHA-256
  `89B8D1E42E3C410F3AFF2F34ED28F4D73E796F9135B0B45C54F7B4E2312E89A1`이다.
  Fixed canary는 170.869초 뒤 row4368 node2 `result + left`에서
  fail-closed했고 artifact를 게시하지 않았다. Component inventory는
  329.9초에 green이다. Local CI profile, progress metric, documentation
  quality도 9.4초, 9.1초, 10.3초에 green이고 implementation volume은
  19.57%, default C emit은 `substituting`, full default compile은 `open`이다.
  다음 seam은 Long addition의 언어-owned overflow contract다.
  Registry 승격이 없으므로 SoT 57.0%, 통합 78%, strict beta 83%를 유지한다.
  이어 exact Long addition, checked division, inequality, multiplication,
  subtraction을 append-only identity 78~82로 닫았다. 덧셈/곱셈/뺄셈은 언어의
  two's-complement wrap 계약을 공유하고, C는 `-fwrapv`, LLVM은 `nsw` 없는
  `add`/`mul`/`sub`를 낸다. 최종 GraphPlan v59 driver는 226.3초에 설치됐고
  5,625,746 bytes, SHA-256
  `1E1F7546C7AE4110CF17DF4672182F0BB94C9BA7E7E9C780269FE7C311BBAD39`이다.
  Long subtraction focused gate는 12.1초, 최종 division/remainder/comparison
  회귀 묶음은 56.1초에 green이다. Component inventory, local CI, progress
  metric, full UTF-8 documentation quality도 각각 365.8초, 7.2초, 4.0초,
  139.2초에 통과했다. Fixed canary는 row4388을 넘어 204.520초 뒤
  row4397/raw routine261 `MirAbiLayoutHashString`의 `type_name Long` node12에서
  fail-closed했다. 다음 seam은 arbitrary cast widening이 아니라 exact
  `Int -> Long` cast/type-name shape다. Registry 승격이 없으므로 SoT 57.0%,
  통합 78%, strict beta 83%를 유지한다.
  이어 exact `TypeName(Long)`/`Cast(Int, Long)`을 identity 83/84로 닫았다.
  최종 GraphPlan v60 driver는 207.8초에 설치됐고 5,627,785 bytes,
  SHA-256
  `76E1CC93A1F537C730CD70593710EE822C12019069EB54C5D6F433B6C60A2796`이다.
  Focused C/LLVM gate는 32.7초, Long wrap 회귀는 14.3초, component
  inventory와 local CI profile은 357.0초와 8.2초에 통과했다. Progress
  metric, documentation quality, source UTF-8 gate도 4.6초, 6.5초, 31.0초에
  통과했고 implementation volume은 19.58%다. Fixed canary는 row4397을
  넘어 205.389초 뒤 row4402/raw routine262
  `MirAbiLayoutHashU32`의 `unsigned_value < 0L`에서 fail-closed했다. 다음
  seam은 Int-less identity 재사용이 아니라 exact Long less comparison이다.
  Registry 승격이 없으므로 SoT 57.0%, 통합 78%, strict beta 83%는 그대로다.
  이어 exact `Long < Long -> Bool`을 append-only identity 85로 기존 typed
  comparison family에 추가했다. GraphPlan v61 driver는 210.4초에 설치됐고
  5,627,856 bytes, SHA-256
  `ABEAD70CD950AE536B1FD6B63EA9412E4E7BF03C938A7F2C1DA23EA449D4338C`다.
  네 Long 비교의 true/false와 wrong-type/kind gate는 14.9초, component
  inventory와 local CI profile은 306.5초와 5.8초에 green이다. Progress,
  documentation-quality, source UTF-8 gate도 4.8초, 6.4초, 0.8초에 통과했고
  implementation volume은 19.58%다. Fixed canary는
  row4402를 넘어 181.949초 뒤 row4513/raw routine268
  `MirAbiLayoutFieldsCaptureWithin`의 populated `Array<Int>` literal
  `[(0 - 1), (0 - 1), (0 - 1), (0 - 1)]` node16에서 fail-closed했다. 다음
  seam은 기존 populated literal operand owner의 ordered admitted Int
  expression element 소비다. Registry 승격이 없으므로 SoT 57.0%, 통합 78%,
  strict beta 83%는 그대로다.

현재 실행 체크포인트와 다음 falsifier의 권위는
`docs/current_work_handoff.md`의 맨 위 active card다. 이 문서의 아래 기록은
역사적 탐색 자료이며, 오래된 `활성` 또는 퍼센테이지 문구를 현재 상태로
인용하지 않는다.

## SoT 가족 단위 폐쇄판

2026-08-13 registry census의 비폐쇄 row 37개를 누락이나 중복 없이 다섯
가족으로 묶는다. 이것은 다섯 가족이 이미 닫혔다는 뜻이 아니라, 37개의
row를 37개의 순차 V 작업처럼 다루던 방식을 중단하기 위한 실행 순서다.
새로운 실제 fact family가 발견되지 않는 한 분모 86도 늘리지 않는다. 같은
Coq fact를 가리키던 두 Array<String> exact-shape pseudo-owner는 독립 권위가
아니므로 canonical GraphPlan의 derived projection 한 행으로 환원했다. 이는
기능 두 개를 완료 처리한 것이 아니라 잘못 중복 집계한 분모를 교정한 것이다.

| 폐쇄 가족 | row 수 | 포함 owner | 가족 종료 조건 |
| --- | ---: | --- | --- |
| compiler artifact spine | 12 | `selfhost.semantic_artifact_admission`, `selfhost.expression_surface`, `mir.generic_specialization`, `hir.typed_control_flow`, `mir.execution_graph`, `air.evidence_graph`, `target.capability_profile`, `projection.verified_plan`, `semantic.symbol_type_graph`, `abi.layout_rows`, `abi.runtime_call_rows`, `semantic.callable_receiver_carriage` | source admission부터 released C/LLVM artifact까지 typed fact가 한 번만 생산·검증되고 native/AST/text/target 재구성 fallback이 삭제됨 |
| collection GraphPlan | 7 | `projection.direct_mir_array_int_program`, `projection.direct_mir_collection_pop_effect`, `projection.direct_mir_collection_program_plan`, `projection.direct_mir_scalar_cfg_foreach_receipt`, `projection.direct_mir_scalar_cfg_program_extension`, `projection.direct_mir_string_array_push`, `abi.mir_array_string_layout_projection` | signature별 예외 owner 대신 공통 collection ownership·ABI·effect plan이 C/LLVM의 마지막 consumer가 되고 old exact-shape 경로가 없음 |
| domain/resource/intent | 11 | `semantic.domain_runtime_assignment`, `dir.domain_graph`, `semantic.machine_layer_transition`, `selfhost.zone_authority_rows`, `selfhost.intent_declaration_rows`, `semantic.nominal_field_kind`, `resource.region_allocation_plan`, `rir.resource_transition_graph`, `semantic.function_param_flow_summary`, `semantic.resource_flow_universe`, `semantic.loop_flow_summary` | real-purpose intent가 production root에서 domain/resource lifecycle과 하나의 native/self runtime plan을 소비하고 old ordinal/name/lifecycle reconstruction이 없음 |
| syntax/tooling | 4 | `lexer.language_word_registry`, `parser.syntax_provenance`, `selfhost.match_case_pattern`, `selfhost.enum_declaration_rows` | parser-owned typed facts가 semantic/IR/LSP까지 유지되고 direct spelling, pattern-string 재파싱, concatenated-source carrier가 삭제됨 |
| protocol/product | 3 | `compatibility.evolution`, `diagnostic.catalog`, `abi.intent_observability_rows` | manifest·diagnostic·intent ABI가 stable row로 package/release consumer까지 직접 전달되고 free-text/위치 기반 매핑이 삭제됨 |

운영 규칙은 다음과 같다.

- 동시에 활성화하는 가족은 하나다. 현재 executable rung은 collection
  GraphPlan canary의 Bool call-short-circuit seam이며, 그 결과가
  다음 실제 owner boundary를 결정한다.
- V 번호와 fixture 증가는 폐쇄 진척이 아니다. 가족 안에서 old carrier
  삭제, missing-fact fail-closed, negative ratchet, production substitution을
  함께 만족해 registry row가 `CLOSED`가 될 때만 hard closure가 오른다.
- 한 bounded patch가 가족 전체를 닫지 못하면 어떤 row와 fallback이 남았는지
  같은 표에 유지한다. 작은 green gate를 가족 완료로 확대 해석하지 않는다.
- 가족 두 개를 실제로 닫기 전에는 남은 기간을 날짜로 약속하지 않는다.
  두 표본의 row 폐쇄 속도와 integration 시간을 관측한 뒤 ETA를 산정한다.

## 과거 활성 우선순위 archive — for-loop break exit merge

- 실행 체크포인트는 `6da669a4`다. installed Pergyra driver는 4,278,544
  bytes, SHA-256 `C274237F...2A44`다.
- `multiple_break_exit.pgy`의 producer MIR은 8,040 bytes
  (`C2AF131C...835`)이며 exit phi는 predecessor별 세 슬롯
  `[i.2, i.4, i.4]`를 보존한다. C와 LLVM은 같은 MIR에서 exact `2`를
  실행하며 `[i.4, i.2, i.4]` 순열도 backend별 byte-equal이다.
- 단순 slot-consumption 수정은 stale `[i.2, i.2]` 위조를 잘못 허용했다.
  최종 binding은 각 슬롯을 한 번씩 소비하면서 routine 전체의 동일 local
  definition을 확인해 그 predecessor를 지배하는 최신 정의만 선택한다.
- dominance 책임은 새 named owner가 보유한다. dominance 54/70,
  predecessor binding 127/180, focused gate 156/160이고 cap을 올리지 않았다.
  focused C/LLVM, public multi-break+nested, full component ratchet이 green이다.
- 다음 단일 falsifier는 range `for`에서 outer `Int`를 갱신하고 reachable
  `break` 뒤 그 값을 사용하는 `for_break_exit.pgy`다. 현재
  `routine_for_owner.pgy`는 break block과 local-version snapshot을 받지만
  block edge만 소비하고 exit phi는 생산하지 않는다.
- 새 for-break 전용 compiler를 만들거나 block 목록만 authority로 쓰는 것,
  backend phi, fixture/block-count 분기, source/`expr0` 복원, planner retry,
  native fallback은 금지한다. 기존 while/break exit merge와 같은 의미라면
  하나의 일반 loop-exit 책임으로 합친다.
- 메모리는 최종 integration 최대값만 기록한다. 2.4 GiB attention,
  3 GiB hard stop을 유지하며 이번 rung에서는 pressure/full bootstrap/
  gen2==gen3/full CI를 실행하지 않았다. 로컬 prover도 없다.

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
