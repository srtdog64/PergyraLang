# Keyword Authorship Pain Points

마지막 업데이트: 2026-04-10

이 문서는 "기능이 없는가"가 아니라 "기능은 있는데 작성 피로를 만드는가"를 기준으로
Pergyra의 현재 pain point를 정리한다.

구체적인 완화 방향과 우선순위는
[59_authoring_surface_compression_plan.md](/mnt/e/PergyraLang/docs/59_authoring_surface_compression_plan.md)에 정리한다.

핵심 원칙:

- Pergyra는 개념을 줄여서가 아니라 작성 경로를 압축해서 살아남아야 한다.
- 강한 의미론은 유지하되, 표면에서 반복 선언과 중복 기술을 줄여야 한다.
- diagnostics는 문법의 일부로 취급해야 한다.

## 1. 현재 가장 큰 pain point

### 1.1 선언 과잉

가장 큰 pain point다.

작은 문제를 풀 때도 아래 축을 동시에 의식하게 만들면 작성 피로가 급격히 오른다.

- `subject`
- `zone`
- `world`
- `intent`
- `effect`
- `relation`

문제는 개념 수 자체보다 "어디서부터 써야 하는가"가 흔들리는 것이다.

현재 체감:

- 사용자는 "한 줄 쓰려고 다섯 줄 선언한다"는 압박을 받기 쉽다
- 철학적으로는 정합하지만, authoring entry path가 무겁다

키워드 family:

- host/declaration family
  - `subject`
  - `class`
  - `object`
  - `tobject`
  - `vessel`
  - `party`
  - `roster`
  - `world`
  - `zone`
  - `relation`
  - `effect`
  - `intent`

필요한 방향:

- 공통 scaffold 강화
- host/domain/intent를 전부 명시하지 않아도 되는 기본 작성 경로 제공
- 예제와 문서에서 "정석 진입점"을 3개 정도로 좁히기

### 1.2 경계 중복 기술

현재 가장 강한 문법적 pain point다.

하나의 step/action에 아래가 동시에 들어가면 의미는 분명해진다.

- `where`
- `who`
- `requires`
- `authorized by`
- `within`
- `causes`
- `using`
- `transfer`

하지만 이 중 일부는 이미:

- `zone`
- `action`
- `ability`
- `role`
- authority declaration

안에 선언돼 있는 경우가 많다.

즉, 작성자는 "계약을 다시 써야 한다"는 중복을 강하게 느낀다.

키워드 family:

- intent/boundary clause family
  - `step`
  - `who`
  - `using`
  - `requires`
  - `authorized`
  - `by`
  - `within`
  - `causes`
  - `where`
  - `expect`
  - `rollback`
  - `cleanup`
  - `compensate`

필요한 방향:

- `intent` 작성 시 반복되는 `who/where/requires` 추론
- zone authority 자동 승계
- action/ability에 이미 박힌 계약의 재기술 최소화

현재 완화된 부분:

- function/action declaration parser는 `where / with effects / requires / within / causes / authorized by`
  절을 고정 순서 `if` 연쇄가 아니라 table-driven parser로 처리한다
- 즉 clause 순서는 현재 자유롭고, duplicate clause는 명시적으로 진단한다

### 1.3 projection / sync / transfer의 정신적 비용

언어 강점이면서 동시에 가장 무거운 사용성 부채다.

문서상 이 축은 Pergyra의 차별점이지만, 작성자 입장에서는:

- projection이 언제 필요한가
- sync를 언제 직접 써야 하는가
- `using:`과 `transfer:`를 어디까지 명시해야 하는가

를 계속 계산해야 한다.

즉 "맞는 형식으로 써야만 통과하는" 압박으로 읽히기 쉽다.

키워드 family:

- boundary/projection family
  - `object`
  - `tobject`
  - `bind`
  - `using`
  - `transfer`
  - `relation`
  - `effect`
  - `zone`
  - `world`

필요한 방향:

- projection wiring 축약
- common projection pattern scaffold
- `using:` / `transfer:` authoring template 제공
- "local object view"와 "boundary tobject transfer"를 예제에서 더 강하게 분리

### 1.4 권장 surface의 흔들림

성장기 언어에서 치명적인 pain point다.

문서 일부는:

- `subject`와 `class`가 현재 surface에서 유사하게 동작한다고 설명하고
- `relation/effect/zone`은 "현재 stable surface"와 "장기 목표"를 같이 적는다

즉 작성자는 순간적으로 아래를 헷갈릴 수 있다.

- 지금 진짜 권장되는 표면이 무엇인가
- 미래 표면과 현재 표면 중 뭘 따라야 하는가

키워드 family:

- dual-surface family
  - `subject`
  - `class`
  - `object`
  - `tobject`
  - `export`
  - `public`
  - `private`
  - `ability`
  - `role`
  - `zone`
  - `world`

필요한 방향:

- "current stable surface"와 "design target"을 분리 표기
- 예제는 항상 현재 권장 표면만 사용
- obsolete 또는 transition surface는 문서 본문이 아니라 migration note로 격리

## 2. 지금 가장 아픈 keyword family

### 2.1 declaration family

- `subject`
- `class`
- `object`
- `tobject`
- `vessel`
- `party`
- `roster`
- `zone`
- `world`
- `relation`
- `effect`
- `intent`

문제:

- 시작점이 무겁다
- 작은 문제에도 존재론 선택 비용이 들어간다

### 2.2 boundary clause family

- `where`
- `who`
- `using`
- `requires`
- `authorized by`
- `within`
- `causes`
- `transfer`

문제:

- 중복 기술을 강하게 유발한다
- 이미 선언된 계약을 step에서 다시 쓰는 경우가 잦다

### 2.3 visibility/module family

- `export`
- `public`
- `private`
- `ability`
- `import`
- `use`

문제:

- 의미론은 맞지만 authoring 직관이 흔들리기 쉽다
- 대표 사례:
  - `ability`는 기본 공개인데 `export ability`를 반복하면 잘못 학습된다

### 2.4 execution family

- `parallel`
- `async`
- `await`
- `spawn`
- `select`
- `channel`
- `defer`

문제:

- 개념 자체는 정리되고 있지만, 아직 "정석 작성 경로"가 약하다
- 특히 일반 앱/게임/장치형 패턴별 입문 경로가 더 강해야 한다

## 3. 이 pain point를 이기는 조건

### 3.1 전부 명시하지 않아도 되게 해야 한다

필요한 것:

- `intent` clause 추론
- zone authority 자동 승계
- effect/relation 연결 축약
- common pattern scaffold

즉 모델은 유지하되, surface redundancy를 줄여야 한다.

### 3.2 정석 작성 경로를 3개 정도로 좁혀야 한다

권장 family:

1. 일반 앱/웹형 흐름
2. 게임/시뮬레이션형 흐름
3. 비동기/워커/장치형 흐름

필요한 것:

- 예제
- scaffold
- 문서 입문 경로

### 3.3 diagnostics를 제품 수준으로 끌어올려야 한다

이 언어는 개념이 많기 때문에, diagnostics 품질이 낮으면 즉시 피로가 폭발한다.

오류는 최소한 아래를 바로 말해줘야 한다.

- 왜 실패했는가
- 어느 boundary인가
- 어느 authority가 비었는가
- 어느 slot이 문제인가
- 어느 projection 또는 transfer가 불일치인가

즉 Pergyra는 문법만이 아니라 진단기가 반쯤 제품이다.

## 4. 지금 당장 해야 하는 일

1. `intent` 반복 clause 추론 규칙 정리
2. authority 자동 승계 규칙 정리
3. projection/transfer scaffold 예제 추가
4. "정석 작성 경로" 3개를 문서 첫 계층에 명시
5. diagnostics 개선을 별도 보드로 관리

## 5. 한 줄 결론

Pergyra의 가장 큰 pain point는 "개념이 많다"가 아니라,
"강한 의미론을 쓰려면 작성자가 너무 많은 것을 매번 다시 적어야 한다"는 점이다.

이 언어는 개념을 줄이는 방향보다, 반복 선언과 authoring 경로를 압축하는 방향으로
정리돼야 한다.
