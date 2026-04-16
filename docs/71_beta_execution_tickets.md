# Pergyra Beta Execution Tickets

마지막 업데이트: 2026-04-15

## 목적

이 문서는 [docs/70_beta_closure_master_board.md](docs/70_beta_closure_master_board.md)를
실행 티켓 단위로 자른 보드다.

운영 규칙:

- 이 보드는 `beta blocker`와 `beta trust` 티켓만 담는다.
- `B2` 축은 넣지 않는다.
- 각 티켓은 가능하면 하나의 PR 또는 하나의 작은 PR 묶음으로 끝나야 한다.
- 각 티켓의 완료 조건에는 기본적으로 아래 네 줄이 들어간다.
  - semantic regression
  - example smoke
  - docs wording
  - C/LLVM parity

## 사용법

- GitHub issue prefix는 그대로 `B0-xx`, `B1-xx`를 쓴다.
- 상태는 `open / in progress / blocked / closed`만 쓴다.
- 베타라고 부르기 전까지는 `B0-*`가 전부 닫혀야 한다.
- `B1-*`는 베타 신뢰도 티켓이지만, 외부 공개 직전에는 사실상 같이 닫는 것이 맞다.

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
- Windows: C regression always, plus LLVM smoke/backend-compare when an LLVM toolchain is detected

목표:

- Linux/Windows 지원 계약을 문서, CI, 릴리스 문구에서 같은 truth로 고정한다.

현재 baseline:

- Linux: C + LLVM
- Windows: C only

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

### B0-09. own/ref anchored subset freeze

목표:

- own/ref를 일반 ownership system처럼 보이게 하지 않고 anchored subset contract로 고정한다.

stable subset:

- `ref Slot<subject-host>`
- `own SecureSlot<subject-host>`

explicit reject:

- non-anchored/general value type own/ref

완료 조건:

- semantic diagnostics, examples, docs wording, backend behavior가 anchored subset 기준으로 일치한다.
- unsupported general ownership path는 explicit reject가 된다.

### B0-10. Frozen subset backend parity suite

목표:

- frozen subset에 대해서는 C와 LLVM이 같은 판정을 하도록 regression을 늘린다.

범위:

- intent/zone/world
- relation/effect/projection
- generic contract stable subset
- own/ref anchored subset

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
- `make ci-windows` runs C regression unconditionally and adds Windows LLVM smoke/backend-compare when the LLVM toolchain is available.

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
