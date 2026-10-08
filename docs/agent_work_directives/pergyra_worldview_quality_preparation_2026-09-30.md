# Pergyra 세계관·코드 품질 작업 준비

> **OLD** (2026-10-08 표시): 연혁 기록이다. 지금의 작업 대기열이 아니며, 아래 원래 상태 줄과 내용은 바꾸지 않았다. 현재 협업 상태는 [claude_gpt_ownership_collaboration_2026-10-08.md](claude_gpt_ownership_collaboration_2026-10-08.md)와 인계 문서의 활성 카드를 본다.

Date: 2026-09-30 (Asia/Seoul)
Status: PREPARED — implementation candidates, serial integration
Base revision: `d9b9eaaac285eb3c63cca6b0e06ac3bf6db49ca7`
Evidence: [1차 공격 기록](../audits/pergyra_worldview_quality_attack_2026-09-30.md)

이 준비서는 세계관의 실행 의미를 코드 품질의 기준으로 삼는다.
cap은 그 책임을 읽고 수정할 수 있는 크기 신호다.
현재 semantic ownership P1이 활성이다. 아래 순서는 target 문서와 활성 owner를
소비하며, 새 활성 rung이나 semantic authority를 만들지 않는다.
P1 이후 하네스 보고 H의 우선순위도 target 문서 그대로 따른다. 아래 표는
책임별 준비 범위이며 H를 생략하는 별도의 실행 큐가 아니다.

## Shared objective card

- Objective: 같은 revision과 같은 callable identity의 의미가 무관한 함수 수,
  선언 순서, backend, 직렬화 형태에 의해 바뀌지 않는 compiler를 준비한다.
- Priority: 의미 identity와 한 SoT → owner-directed facts → 실제 자원/승인 경계
  → 중복 판정 제거 → 실패의 설명 가능성 → 핫패스 비용 → cap과 표현 간결성.
- Fact owner: 기존 semantic collection ownership family.
  Pergyra 목표 owner는 `SemanticAstCollectionOwnershipVerdictFromResolvedFacts`.
  native C는 현재 registry owner이자 유지할 bootstrap/test oracle이다.
- Last legitimate consumers: MIR ownership carrier, admitted GraphPlan,
  direct C/LLVM cleanup·materialization, public compiler artifact path.
- Forbidden fallback: 무관한 routine 수를 소유권 승인으로 사용, 타입/이름/Slot로
  owned status 추측, C oracle을 production retry로 사용, 소비자의 새 body proof로
  같은 의미를 재정의, cap만 맞추는 파일 쪼개기.
- Verification: wrapper/no-wrapper/reordered/unrelated-tail 함수를 C/LLVM에서
  실행 비교하고 missing/wrong-target/forged-source/cycle을 각각 거부한다.
  기존 final artifact는 거부 시 보존되어야 한다.

## W0 — 현재 P1에 전달할 가장 작은 실행 반례

[wrapper](../audits/repros/owned_string_wrapper_2026-09-30.pgy)와
[unrelated tail](../audits/repros/owned_string_wrapper_padded_2026-09-30.pgy)의
기존 callable/receiver identity와 의미는 같다. 현재 고정 installed packet에서
앞은 public C/LLVM 거부, 뒤는 실행 성공이다. native oracle은 wrapper/producer
선언 순서에 따라 거부/성공한다.

준비한 변경의 책임:

- semantic owner가 exact callable result ownership과 transfer 의미를 결정한다.
- external MIR은 기존 admission boundary에서 evidence와 exact binding을 검증한다.
  입력이 typed JSON이라는 이유만으로 caller-supplied positive receipt를 신뢰하지 않는다.
- 동일 admitted revision/plan의 projection 소비자는 identity에 결속된 증거를 읽는다.
  의미 승인과 source body 재판정을 혼합하지 않는다.
- cycle/expr traversal 한도는 실제 graph/경로의 성질을 검사한다.
  일반 함수 수를 늘려 증명 예산을 얻는 경로를 없앤다.
- source-level 거부는 callable와 소유권 이유·위치를 설명한다.
  `program_readiness=26`은 재현 근거로 남기되 정상 사용자 설명의 끝으로 삼지 않는다.

이것은 구현 후보의 목표다. 새 receipt schema나 새 owner를 미리 확정하지 않는다.
먼저 현 owner/callable fact/GraphPlan의 표현으로 닫을 수 있는지 검사한다.
단순 depth 상향, 함수를 추가해 통과시키는 workaround, native fallback은 수정안이 아니다.

## 책임별 작업 단위와 순서

| Unit | 소유할 결정 / 수명 | 자연스러운 Pergyra 표현 | 마지막 소비자 / 제거할 중복 | Activation |
|---|---|---|---|---|
| 소유권 전이 | exact binding·producer origin, move/clone/drop/argument/result/inout | func + typed value rows | MIR·GraphPlan·C/LLVM; 동일 의미의 재판정 | 현재 P1 |
| collection 계획 수렴 | type/operation을 입력으로 받는 한 계획 | func + struct, target은 투영 입력 | shape-specific route·planner | P1/P2b 도달 slice |
| revision 사실 인계 | 한 compilation revision의 불변 facts 수명 | 계산은 func/struct, 실제 봉인은 subject.action | MIR→AST→semantic 재구성; 이후 내부 JSON 재색인 | 기존 P2b → P2a |
| artifact 게시 | candidate→committed/rejected, prior-final 보존, temp 정리 | subject/action + 실제 transaction zone | C/LLVM/MIR route별 중복 게시 정책 | 기존 P3 |
| 실행 stage 전이 | 실제 입수와 admission/sealing | 실제 책임이 생긴 subject.action | readiness-only stage action | 기존 P4 |
| target 계획 | 같은 revision과 target env에서 파생되는 projection 사실 | struct/func; target 자원 경계는 zone | emitter-local ABI/semantic 추측 | 기존 P5 |
| Check/Format/Debug | 판정·rewrite·세션이라는 각각의 사용자 성공 | 실제 목적의 intent와 기존 session/revision 소비 | 따로 가진 revision/store 의미 | 기존 P6 |
| 표현과 유지보수 | row 단위 변경, match/for-in, 이름·오류 전달 | 지원되는 record/hosted func/generic 등 | 열별 mutation, 수동 조회/Option 의식 | P1 닫힌 뒤 기존 S0–S6 |

큰 계산을 subject로 승격하지 않는다. 실제 목적에 성공/실패가 닫힌 단일 step
intent는 유지할 수 있다. 실제 artifact transaction을 backend별 세계로 나누지 않는다.

## 캡을 나눌 때의 판단

현재 수치를 새로 복사해 별도 cap authority를 만들지 않는다.
component contract, shared scalar caps, responsibility policy가 해당 수치를 소유한다.

1. 한 단위의 입력·결정·출력·실패를 이름 붙인다.
2. 다른 변경 이유를 가진 판정과 최종 표현을 분리한다.
3. 같은 전이의 불변조건, cleanup, 실패 경계는 함께 추적한다.
4. 타입/연산/target별로 의미 정책을 복제하지 않고 parameter와 projection으로 표현한다.
5. 분리한 소유자를 기존 cap owner에 등록하고 old consumer read를 제거한다.
6. 전체 authority 수와 shape route 수가 증가할 필요가 있는지 반증한다.

현재 큰 파일 가운데 enum active-variant proof처럼 응집된 책임은 cap만으로
위반 판정하지 않는다. 5줄짜리 ABI/identity vocabulary owner도 자동 제거 대상이 아니다.
`EmitStmtList`의 lexical cleanup은 exit semantics와 분리 후에도 같은 증거를 읽어야 한다.

## 핫패스 검토를 쉽게 만드는 계측 준비

이번 탐색 timing은 moving binary와 host variance 때문에 병목 증거로 사용하지 않는다.
owner 계측은 활성 slice가 실제로 도달하는 곳에 한정한다.

| Owner seam | 입력 단위 | 확인할 비용 | 반증 사례 |
|---|---|---|---|
| owned String result facts | 동일 revision의 function/call graph | 고정점 pass, 검사 함수 수, membership lookup | 같은 의미의 선언 순서 변경 |
| collection verdict | binding + transition event | local/graph visits, receipt 수, 전체 재검증 | 1배/2배/4배 binding·event |
| admitted projection | 같은 plan + target | full Ready/digest 횟수, fact 복사 | 같은 plan을 두 target이 소비 |
| revision handoff | 동일 프로그램 | serialization count/bytes, AST/semantic 반복 | 한 revision에서 Check와 Compile |
| artifact transaction | 게시 1회 | retained payload, temp write, publish/abort | write/flush/publish 실패와 기존 final |

시간 하나만 비교하지 않는다. 증가하는 입력과 반복 횟수/바이트/할당의 관계를 함께 기록한다.
관측 전에는 cache, worker, timeout 확대, 별도 query engine을 추가하지 않는다.

## Edit scopes, budgets and integration

현재 작업은 단일 통합자가 순서대로 준비·검토한다. 병렬 구현 트랙을 열지 않는다.

- 이번 edit scope: authoring skill, read-only audit/repro, 이 준비서, handoff의 짧은 감사 메모.
- 기존 compiler source, cap authority, registry와 다른 작업의 dirty 파일은 겹치지 않는다.
- W0 구현 후보가 활성 작업에 합류하면 semantic fact → admitted plan → public artifacts
  순서로 한 slice만 통합한다. 다른 unit은 해당 순서가 도달할 때 재검토한다.
- Allowed commands: source/status/hash 읽기, 새 scratch directory, 고정 packet의 compile/run,
  좁은 owner mutation/parity, skill/diff 검증.
- Budget: static owner checks 60초, focused parity 5분, integration shard 30분.
  전체 matrix는 기존 scheduled/merge boundary에 둔다.
- Integration owner: 활성 P1의 primary compiler 작업.
- One integration gate: `tests/self_hosted/parity/direct_mir_owned_string_call_result_owner.sh`에
  W0 composition/unused/reordered 반례와 forged-source refusal을 결속시키고
  public/native/direct C/LLVM 결과를 확인한다. 현재 게이트에 추가하거나 실행했다고
  주장하지 않는다.
- Outputs: 관찰한 실패, source observations, 구현 준비 후보. self-host CLOSED나
  publication 결과로 환산하지 않는다.

통합 직전 exact HEAD/dirty, source graph와 설치 receipt를 다시 확인한다.
감사 중 다른 작업의 HEAD와 설치 binary가 움직였다는 사실을 지운 채 결과를 이식하지 않는다.
