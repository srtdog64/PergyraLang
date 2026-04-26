# Pergyra Beta Execution Tickets

마지막 업데이트: 2026-04-26

## 목적

이 문서는 [docs/70_beta_closure_master_board.md](docs/70_beta_closure_master_board.md)를
실행 티켓 단위로 자른 보드다.

운영 규칙:

- 이 보드는 `beta blocker`와 `beta trust` 티켓만 담는다.
- `B2` 축은 넣지 않는다.
- 각 티켓은 가능하면 하나의 PR 또는 하나의 작은 PR 묶음으로 끝나야 한다.
- 각 티켓의 완료 조건은 티켓 성격에 맞춘다. 구조 debt 티켓은 먼저 구현
  slice를 닫고, 그 slice의 owner gate만 통과시킨 뒤, 묶음 단위로 wide
  regression을 돌린다.

## Lean Sprint Operating Mode

2026-04-26부터 beta closure 작업은 test-first가 아니라
**debt-slice-first**로 운영한다. 지금 병목은 테스트 부족보다 구조 debt의
청산 속도이며, 모든 작은 변경마다 full regression을 돌리면 실제
implementation closure 시간이 사라진다.

### Sprint Shape

1. Pick one debt owner.
   - 예: DAG alias materialization, AIR/CFG body fact source-of-truth,
     `MIRDeclInventory`, runtime frontier scheduler, ABI ownership/pinning.
2. Define the implementation slice before writing tests.
   - 어떤 source-of-truth를 바꿀지, 어떤 fallback을 줄일지, 어떤 owner file을
     책임질지 먼저 적는다.
3. Implement until the slice is structurally complete.
   - 중간에는 compile smoke나 static gate 정도만 사용한다.
   - full semantic/backend parity는 slice가 끝나기 전까지 반복 실행하지 않는다.
4. Run slice-local gates.
   - DAG slice: `type-resolution-dag-test-smoke`,
     `type-resolution-resolver-inventory-test-smoke`.
   - CFG/AIR slice: `cfg-body-dataflow-test-smoke`,
     `air-drift-test-smoke`, relevant backend non-impact gate.
   - MIR slice: `mir-declaration-inventory-test-smoke`,
     `backend-inc-size-test-smoke`.
   - runtime/ABI slice: targeted ABI/runtime smoke.
5. Add or update only the regressions that prove the slice.
   - Regression is evidence for a completed slice, not a substitute for the
     implementation.
6. Run wider regression once per closed slice or at sprint boundary.
   - `test-semantic`, `llvm-test-backend-compare`, `ci-linux` are batch gates,
     not the inner edit loop.

### Anti-Pattens

- Do not spend a sprint only tightening smoke thresholds without reducing the
  underlying fallback/debt.
- Do not add new `.inc` split fragments to satisfy a line cap unless the split
  is on a real owner seam.
- Do not broaden a shortcut if it bypasses provenance. The rejected alias
  symbol-cache shortcut is the current example: it reduced apparent resolve
  work but broke module visibility and generic ability provenance.
- Do not treat passing tests as beta closure when source-of-truth is still
  duplicated across AST/HIR/MIR/runtime/backend.

### Review Rule

For each sprint update, report these four lines:

- `Debt owner:` the subsystem being reduced.
- `Implementation closed:` the fallback, duplicate source-of-truth, or owne
  seam removed.
- `Local gate:` the narrow command used during the sprint.
- `Batch gate:` the wider command deferred until the slice is complete.

## 사용법

- GitHub issue prefix는 그대로 `B0-xx`, `B1-xx`를 쓴다.
- 상태는 `open / in progress / blocked / closed`만 쓴다.
- 베타라고 부르기 전까지는 `B0-*`가 전부 닫혀야 한다.
- `B1-*`는 베타 신뢰도 티켓이지만, 외부 공개 직전에는 사실상 같이 닫는 것이 맞다.
- 구조 debt가 beta 이후에 폭발할 가능성이 있으면 기능 티켓보다 우선한다.

## B0 Tickets

### B0-01. Beta surface taxonomy freeze

목표:

- `stable subset / explicit reject / beta-out-of-scope` 분류를 README/TODO/docs/diagnostics에 같은 말로 고정한다.

범위:

- generics
- own/ref
- collections
- runtime observability

완료 조건:

- README, status docs, depth matrix, stable example board가 같은 분류를 쓴다.
- unsupported 조합은 조용히 통과하지 않고 explicit reject diagnostics를 낸다.
- stable subset 문구가 example/source-of-truth와 충돌하지 않는다.

### B0-02. Failure handling policy freeze

목표:

- `recoverable failure / contract violation / internal bug` 경계를 runtime, semantic, docs에서 고정한다.

범위:

- intent/zone/world runtime failure
- slot/token/ownership invariant break
- unwrap sharp-tool policy

완료 조건:

- [docs/07_error_handling.md](docs/07_error_handling.md), README, status docs가 같은 분류를 쓴다.
- recoverable path는 `Bool`, `Result<T>`, queryable runtime state로 드러난다.
- invariant break는 hard-fail로 남고, 그 이유가 진단과 문서에서 명확하다.

### B0-03. Beta support matrix freeze

Current execution truth override:

- Linux: C + LLVM
- Windows: C regression always, plus LLVM smoke/backend-compare only when executable `llvm-config --libs core` evidence is available

목표:

- Linux/Windows 지원 계약을 문서, CI, 릴리스 문구에서 같은 truth로 고정한다.

현재 baseline:

- Linux: C + LLVM
- Windows: C regression always, plus LLVM smoke/backend-compare only when executable `llvm-config --libs core` evidence is available

완료 조건:

- README, status docs, release note template가 같은 support matrix를 쓴다.
- CI 설정과 문서 계약이 어긋나지 않는다.
- Windows LLVM이 미지원이면 과장 표현이 남아 있지 않다.

### B0-04. Intent / Zone / World provenance parity

목표:

- intent step contract provenance가 semantic, AST/debug, diagnostics에서 같은 vocabulary를 쓰게 고정한다.

핵심 vocabulary:

- `locally declared`
- `reused from matching action contract`
- `derived from transfer target`
- `derived from using binding`

완료 조건:

- step failure diagnostics가 provenance를 빠뜨리지 않는다.
- AST/debug 출력도 semantic과 같은 provenance를 보여 준다.
- relevant parser/semantic regressions가 존재한다.

### B0-05. Intent / Zone / World embedding and handoff closure

목표:

- embedding ownership, handoff visibility, cross-layer propagation을 semantic/runtime/C/LLVM까지 닫는다.

완료 조건:

- world-owned zone mutation, embedding copy-vs-reference visibility, handoff path가 same-result parity를 가진다.
- failure reason, authority reason, boundary reason이 structured diagnostics로 나온다.
- backend compare와 ABI/runtime probe에 관련 케이스가 들어간다.

### B0-06. relation / effect / projection propagation policy

목표:

- `refresh / publish / bind`와 branch/join/handoff/embedded propagation policy를 source of truth로 고정한다.

완료 조건:

- projection propagation이 helper-heavy best-effort가 아니라 explicit contract로 보인다.
- runtime provenance가 edge path까지 설명 가능하다.
- C/LLVM compare가 propagation visibility를 같은 결과로 보여 준다.

### B0-07. Projection provenance diagnostics freeze

목표:

- projection failure가 “왜 안 되는지”를 종류별로 명확히 나누어 보이게 한다.

필수 diagnostic fields:

- target
- source
- projection kind
- field path or map
- Reason:
- Fix:

완료 조건:

- missing source field
- ambiguous path
- wrong projection kind
- duplicate field map

위 네 케이스가 서로 다른 semantic regression으로 잠겨 있다.

### B0-08. Generic contract multi-path closure

목표:

- current generic stable subset만 beta contract로 얼리고, 그 subset에 대한 전경로 enforcement를 닫는다.

범위:

- `where T: A + B`
- default type arg actual resolution
- module-contract propagation
- richer mismatch provenance

완료 조건:

- declaration, call, contract consumer path에서 같은 generic constraint verdict를 낸다.
- diagnostics가 `generic subject / expected / actual / broken bound / consumer path / fix`를 포함한다.
- broader generic generalization은 beta-out-of-scope로 문서에 남는다.

### B0-09. own/ref general closure freeze

목표:

- own/ref를 일반 타입까지 닫힌 규칙으로 고정하고, 남은 미닫힘 표면은 explicit reject로 분리한다.

stable subset:

- copy-only value의 trivial own/ref
- boundary-visible aggregate own/ref
- slot handle own/ref
- subject/class/object aggregate path

explicit reject:

- `Token<T>` 같은 authority-bearing explicit reject
- 아직 semantic contract를 닫지 못한 surface의 임시 partial acceptance 금지

완료 조건:

- semantic diagnostics, examples, docs wording, backend behavior가 general own/ref 기준으로 일치한다.
- unsupported surface는 explicit reject가 되고, parser-accepted surface를 조용히 약화하지 않는다.

### B0-10. Frozen subset backend parity suite

목표:

- frozen subset에 대해서는 C와 LLVM이 같은 판정을 하도록 regression을 늘린다.

범위:

- intent/zone/world
- relation/effect/projection
- generic contract stable subset
- own/ref general stable subset

완료 조건:

- backend compare 케이스가 frozen subset을 대표하도록 확장된다.
- parity는 stdout뿐 아니라 diagnostics와 runtime state까지 본다.
- 한 backend만 통과하는 surface는 stable로 분류되지 않는다.

### B0-11. Declaration-side MIR inventory closure

목표:

- LLVM declaration/top-level orchestration path의 AST-carried inventory debt를 frozen subset 기준으로 줄인다.

완료 조건:

- zone/world/relation/effect/intent declaration emission이 MIR inventory 기준으로 설명 가능하다.
- fallback comment나 silent recovery가 explicit backend error 또는 explicit unsupported로 바뀐다.
- pain point 고정 이전에 리팩터 범위를 넓히지 않는다.

### B0-12. Arena + lifetime discipline bootstrap

목표:

- beta 전에 arena/lifetime 규칙을 문서와 구현 둘 다에서 고정한다.

범위:

- transpiler scratch arena
- semantic scratch/result boundary
- semantic result-owned diagnostic payload seam
- cache vs arena pointer 금지 규칙
- index/stable handle cross-reference

완료 조건:

- `docs/94_arena_index_lifetime_plan.md`의 규칙이 TODO/master board와 일치한다.
- 최소 1개 vertical slice가 실제 코드에 들어간다.
- `DiagPayload` emit 경로가 result-owned snapshot으로 보존되어 scratch formatting과 result-visible structured data가 분리되기 시작한다.
- cache가 arena-owned pointer를 장기 저장하지 않는다는 규칙이 코드 리뷰 기준으로 고정된다.

## B1 Tickets

### B1-01. Surface trust doc reclassification

목표:

- `alpha-complete / experimental / removed` 분류를 README/docs/examples에 반영한다.

완료 조건:

- “되는 것처럼 보이는 subset”이 남아 있지 않다.
- experimental examples는 stable source of truth에서 분리된다.
- README 첫 화면과 stable example board가 같은 메시지를 준다.

### B1-02. Stable example and smoke source-of-truth expansion

목표:

- explicit/compressed pair example 최소 4쌍을 stable source of truth로 고정한다.

필수 영역:

- app/web orchestration
- game/simulation
- async/worker/device
- world-handoff/domain propagation

완료 조건:

- pair example이 smoke에 직접 연결된다.
- minimal subset example과 canonical pair example의 역할이 문서에 분리되어 있다.

### B1-03. Parser surface removal or explicit reject

목표:

- parser가 받지만 semantic/runtime/backend/docs까지 닫히지 않은 문법을 정리한다.

완료 조건:

- 닫지 못한 parser surface는 제거되거나 explicit reject가 된다.
- grammar/syntax docs가 실제 accepted surface를 과장하지 않는다.
- smoke/source-of-truth example이 이 정책을 반영한다.

### B1-04. CI hardening

Current pipeline truth override:

- `make ci-linux` keeps Linux on mandatory C + LLVM coverage.
- `make ci-windows` runs C regression unconditionally and adds Windows LLVM smoke/backend-compare only when executable `llvm-config --libs core` evidence is available.

목표:

- frozen subset 전체가 CI에서 지속적으로 깨지지 않게 만든다.

범위:

- Ubuntu + Windows
- smoke coverage 확장
- ASan/UBSan 도입 검토

완료 조건:

- frozen subset 대표 케이스가 CI에서 돌아간다.
- Windows C-only 계약과 Linux C+LLVM 계약이 파이프라인에 그대로 반영된다.
- 깨진 경로가 사람이 수동으로만 아는 상태가 아니다.

### B1-05. Security and release hygiene

목표:

- 외부 베타 공개에 필요한 최소 운영 위생을 갖춘다.

범위:

- SECURITY.md
- release/tag policy
- SemVer policy
- CodeQL / secret scanning baseline

완료 조건:

- 첫 beta tag/release를 만들 수 있는 문서와 정책이 있다.
- 릴리스가 무엇을 공식 지원하는지 명확히 적는다.

## Not On This Board

아래는 베타 보드에 올리지 않는다.

- LSP / formatter / debugger 고도화
- stdlib / app infra 확장
- general ownership system
- arbitrary key-universal collection contracts
- richer multi-instance observability query
- quantum full resource model
- WASM
- package manager
- advanced debugger

## Recommended order

1. `B0-01`, `B0-02`, `B0-03`
2. `B0-04` ~ `B0-09`
3. `B0-10`, `B0-11`
4. `B1-01` ~ `B1-05`

## Exit rule

아래 중 하나라도 남아 있으면 베타라고 부르지 않는다.

- parser surface가 semantic/runtime/backend/test/docs까지 닫히지 않았다.
- frozen subset인데 C/LLVM verdict가 다르다.
- 문서가 stable이라고 말하지만 example/smoke가 그걸 뒷받침하지 못한다.
- support matrix가 CI truth와 다르다.
- recoverable failure와 invariant break가 같은 층으로 섞여 있다.
