# Keyword Authorship Pain Points

마지막 업데이트: 2026-04-11

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
- zone/world/context 기반 기본값 추론
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

### 1.5 surface trust 부채

최근 가장 직접적으로 드러난 pain point다.

사용자는 다음 셋을 구분할 수 있어야 한다.

- 지금 바로 믿고 써도 되는 stable surface
- smoke-covered subset이지만 범위가 제한된 surface
- design sketch / aspirational demo

이 구분이 흐려지면 "컴파일될 것처럼 보이지만 실제로는 안 되는" 좌절이 생긴다.

대표 사례:

- `HashMap<Int, V>`는 예전에는 문서/표면에 비해 실제 지원이 약했지만 지금은 정렬됐다
- `party_system_demo`, `world_roster_city` 같은 예제는 여전히 설계 스케치인데 stable syntax reference처럼 읽히기 쉽다
- `own/ref`는 단어만 보면 전체 ownership 시스템처럼 보이지만, 현재 닫힌 구현은 anchored subject-slot boundary subset이다

필요한 방향:

- 모든 예제에 `compile-smoke covered` / `design sketch` 라벨을 명시
- README/문서가 "현재 닫힌 subset"을 먼저 말하게 하기
- unsupported 조합은 조용한 acceptance가 아니라 explicit semantic error로 고정

### 1.6 `own/ref`의 과잉 일반화 위험

이건 문서와 사용자 기대 사이의 pain point다.

`own` / `ref`라는 이름만 보면 사용자는 자연스럽게:

- 일반 ownership/borrowing 모델
- 모든 anchored handle에 열린 함수 경계 규칙
- Rust류의 전면 소유권 규칙

을 기대하게 된다.

하지만 현재 실제 구현은 더 좁다.

- `ref Slot<subject-host>`
- `own SecureSlot<subject-host>`

이 두 축이 semantic + backend + tests까지 닫힌 핵심 subset이다.

필요한 방향:

- `own/ref` 문서는 장기 비전보다 현재 닫힌 subset을 먼저 설명
- diagnostics도 "일반 ownership이 아직 아니다"를 더 직접적으로 드러내기
- `DeviceSlot<T>`, 일반 `Slot<T>`, `QubitSlot`까지 같은 경계 규칙이 열린 것처럼 보이는 예제 금지

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

- `step`
- `who`
- `using`
- `transfer`
- `requires`
- `authorized`
- `by`
- `within`
- `causes`
- `where`

문제:

- 이미 action/zone/authority에 있는 정보를 intent step에서 다시 쓰게 된다
- 추론이 들어와도 작성자는 "언제 생략해도 되는가"를 다시 학습해야 한다
- 진단이 inheritance/inference provenance를 더 직접적으로 보여주지 않으면 부담이 줄지 않는다

### 2.3 trust-signaling family

- `subject`
- `class`
- `object`
- `tobject`
- `ability`
- `export`
- `public`
- `private`
- `party`
- `world`

문제:

- 문서, 예제, 구현이 같은 말을 하지 않으면 키워드 자체보다 "무엇을 믿어야 하는가"가 pain point가 된다
- 이 가족은 의미론보다 trust-signaling 품질이 중요하다

## 3. 외부 리뷰 기준 현재 상태

최근 받은 리뷰 중, 현재 코드와 대조했을 때 상태는 이렇게 정리된다.

### 3.1 이미 상당 부분 해결된 항목

1. 함수 declaration clause가 고정 순서라서 고통스럽다는 지적
- 현재 parser는 table-driven으로 처리한다
- `where / with effects / requires / within / causes / authorized by`
  순서는 고정이 아니다
- duplicate clause는 명시적으로 진단한다

2. `subject`와 `class`, `tobject`와 `struct`의 lexer token aliasing
- 이건 과거엔 맞는 지적이었다
- 현재는 `TOKEN_SUBJECT`, `TOKEN_CLASS`, `TOKEN_STRUCT`,
  `TOKEN_OBJECT`, `TOKEN_TOBJECT`로 분리됐다

### 3.2 이미 해결된 항목

1. effect clause token 처리 일관성 부족
- 이건 과거엔 맞는 지적이었다
- 현재는 `secure`, `remote`, `nondeterministic`, `collapse`, `local`
  전부 real token으로 정리됐다

### 3.3 아직도 유효한 항목

1. 이름 토큰 허용 폭
- declaration name은 이제 일반 식별자로 고정됐고, reserved keyword 재사용 surface는 닫혔다
- 현재 남은 문제는 `parser_check_name_token()` 전체가 아니라
  - binding/local/param name 허용 폭
  - 일부 alias surface
  - 문서와 진단이 이 분해를 충분히 설명하는가
  쪽이다

2. sketch example과 stable example의 혼재
- 일부 예제는 여전히 미래 표면을 보여 주는 design sketch다
- 이 경계가 약하면 문법 pain point가 아니라 trust pain point가 된다

3. `own/ref`의 기대 범위
- 이름은 크지만 현재 닫힌 규칙은 anchored subject-slot boundary subset이다
- 이 차이를 문서/진단/예제가 계속 드러내야 한다

3. generic parser는 구조가 semantic보다 앞서 있는 부분이 있다
- 지금은 `ability<T>`, `requires Ability<T>`, generic-aware satisfaction,
  multiple ability-style bounds까지 올라왔다
- `default type arg`는 더 이상 묵인되지 않고 명시적으로 semantic reject된다

4. clause density 자체는 여전히 높다
- token split과 declaration trust는 많이 정리됐지만
  - `where`
  - `with effects`
  - `requires`
  - `within`
  - `causes`
  - `authorized by`
  같은 clause family는 여전히 쓰는 사람 입장에서 밀도가 높다
- 즉 지금 남은 pain point는 "키워드가 모호해서 못 믿겠다"보다
  "계약을 쓰려면 여전히 많이 적어야 한다" 쪽으로 이동했다

5. action/intent/zone contract 중복 기술
- action 선언에 이미 들어 있는 `requires/within/authorized by/causes`
  정보가 intent step에서 반복되면 authoring friction이 커진다
- 현재는 intent-side inference가 꽤 올라왔지만, 사용자는 여전히
  "어디까지 생략해도 안전한가"를 한 번 더 생각해야 한다

6. diagnostics는 좋아졌지만 "상속/추론된 계약" 설명은 더 필요하다
- 최근 진단은 `Reason/Fix` 형태로 개선됐고 anchored boundary도 더 분명해졌다
- 하지만 intent/action/zone 계약 추론에서는
  - 무엇이 상속되었는지
  - 무엇이 명시 override인지
  - 왜 이 위치에서 실패했는지
  를 더 직접 보여줄수록 authoring 피로가 줄어든다

### 3.4 현재 기준 P0 폐인포인트

지금 시점에서 가장 아픈 축은 아래 셋이다.

1. clause density
- 긴 declaration/contract surface가 여전히 밀집돼 있다

2. contract duplication
- action/intent/zone 사이에서 같은 의미를 두 번 적는 순간 피로가 크게 올라간다

3. inferred contract diagnostics
- 추론이 강해질수록 "무엇을 상속했는지"를 에러와 hover에서 더 잘 보여줘야 한다

즉 현재 단계의 폐인포인트는 lexer/token 문제가 아니라
**authoring density + duplicate description + inference explainability**다.
- 남은 것은 richer constraint semantics 쪽이다

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

## 2.5 실제로 가장 많이 반복되는 서술 패턴

추상적인 pain point보다 더 중요한 것은, 실제 코드/예제에서 어디가 반복되는가다.

### A. action clause cluster

가장 대표적인 반복 서술이다.

반복 키워드:

- `requires`
- `within`
- `authorized by`
- `causes`

실제 예제:

- `examples/vessel_action_design.pgy`
- `examples/biome_simulator/creatures.pgy`
- `examples/space_station/crew.pgy`
- `docs/26_vessel_action_model.md`

특징:

- 같은 zone 안 action은 `within`이 반복된다
- self-authorized action은 `authorized by self`가 반복된다
- action이 이미 domain-local인데도 계약을 전부 다시 서술하게 된다

핵심 압축 방향:

- zone 내부 action은 `within` 기본 유도
- self-authorized 기본형은 더 짧은 surface 제공
- `causes`는 effect slot 연결이 자명한 경우 축약 여지 검토

### B. intent step boundary cluster

가장 강한 authoring 압박이다.

반복 키워드:

- `who:`
- `where:`
- `using:`
- `transfer:`
- `requires:`
- `authorized by:`
- `causes:`

실제 예제:

- `examples/etl_workflow.pgy`
- `examples/eda_workflow.pgy`
- `docs/34_intent_oriented_paradigm.md`
- `docs/testdoc/logistics_intent_probe.md`

특징:

- 같은 intent 안에서 `who` / `where`가 step마다 반복된다
- action 계약과 step 계약이 겹치면 중복 서술이 커진다
- `using` / `transfer`는 의미는 강하지만 표면이 rigid하다

핵심 압축 방향:

- intent-level default `who` / `where` 강화
- action 계약의 step 기본 상속 강화
- `transfer` 축약 표면
- `using`과 transfer target의 자동 정렬

### C. authority / ability declaration cluster

반복 키워드:

- `authority ... requires ...`
- `action ... requires ...`
- `step requires: ...`

실제 예제:

- `examples/logistics_intent_probe/zones/loading.pgy`
- `examples/logistics_intent_probe/zones/delivery.pgy`
- `examples/dnd_tavern_campaign/zones/journey.pgy`
- `examples/vessel_action_design.pgy`

특징:

- 같은 ability 계약이 zone authority, action, intent step에 중복 기술된다
- 의미론은 맞지만 작성자는 같은 제약을 세 번 쓴다고 느낄 수 있다

핵심 압축 방향:

- authority contract 기본 상속
- action `requires`의 intent step 기본 상속
- diagnostics에서 “어디서 상속/유도됐는가”를 명확히 표시

## 2.6 용어 고정

이 문서에서는 다음 용어를 고정한다.

- `inheritance`
  - nominal/object hierarchy 의미로만 쓴다
- `inference`
  - 이미 선언된 zone/world/action/authority 계약에서 기본값을 상속/유도해 채우는 것
- `preset/profile`
  - 반복되는 clause 묶음을 미리 정의한 authoring shortcut

즉 `intent step`이 action의 `within/requires/authorized by/causes`를 가져오는 것은
nominal 상속이 아니라 `계약 상속`이다.

### D. projection / domain wiring cluster

반복 키워드:

- `refresh`
- `publish`
- `bind`
- `HasProjection`
- `HasLayer`
- `HasState`
- `HasZone*`

실제 문서:

- `docs/13_world_roster_architecture.md`
- `docs/18_language_status.md`
- `docs/20_compiler_pipeline_guide.md`

특징:

- 언어 강점이지만, 큰 시나리오에서 wiring-heavy하다
- relation/effect/zone/world 계층을 다 쓰는 순간 선언 밀도가 급격히 올라간다

핵심 압축 방향:

- group bind
- projection scaffold
- relation/effect alias 또는 implicit member resolution

### E. built-in capability / slot strictness cluster

반복 규칙:

- named binding 요구
- exact slot kind 요구
- token pairing 요구

실제 코드:

- `src/semantic/type_checker_builtins.c`

특징:

- 언어 안전성에는 필요하지만, 표면상 “이 형식 아니면 전부 실패”로 느껴지기 쉽다
- 특히 `Move/Read/Write/Release`, `HasProjection/HasLayer/HasState/HasZone` family는
  입력 shape가 엄격하다

핵심 압축 방향:

- diagnostics를 domain-first 형식으로 개선
- common safe patterns를 scaffold/intrinsic surface로 올리기

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

## Remaining real pain points (P0 closure set)

The token split and nominal keyword cleanup closed a structural confusion class, but they did not remove the main authoring friction. The remaining real P0 set is narrower and more operational.

### 1. Clause density in action and step declarations

Problem:
The surface still becomes heavy when `where`, `with effects`, `requires`, `within`, `causes`, and `authorized by` appear together. Even when each clause is individually coherent, the combined reading and writing cost is too high for routine code.

Closure target:
- keep declaration order predictable
- keep the short surface canonical
- make the long surface explicit but secondary
- document exactly which clauses belong to the reusable contract pack and which stay declaration-local

Primary implementation fronts:
- parser clause-order diagnostics
- LSP hover/completion for the short surface
- stable reference examples that show long vs compressed forms side by side

### 2. Contract duplication between action and intent step

Problem:
Writers still feel duplication when the same zone, authority, ability, and cause information is spelled once on the action and again on the matching intent step.

Closure target:
- the matching action contract pack is the default source of truth
- step-level spelling is override-only unless the step intentionally diverges
- diagnostics must say when a step value came from the matching action contract instead of local spelling

Primary implementation fronts:
- semantic inheritance summaries on step diagnostics
- AST/debug surfacing of contract-source flags
- examples that show the verbose and compressed forms as equivalent

### 3. Inferred-contract failure opacity

Problem:
Once inference removes boilerplate, failures become harder to interpret unless the compiler shows where the inherited contract came from.

Closure target:
- every boundary/authority/requires failure on an intent step should name the inferred source
- error text should distinguish `locally declared`, `inherited from matching action contract`, and `derived from transfer target`
- the user should not need to reconstruct hidden inference by reading multiple declarations manually

Primary implementation fronts:
- semantic diagnostics
- AST print/debug views
- LSP hover wording for contract-pack behavior

### 4. Clause-family boundaries are still easy to misread

Problem:
`with effects` lives near other action clauses, but it is not part of the matching action contract pack. This remains a subtle source of incorrect expectation.

Closure target:
- treat `with effects` as declaration-local only
- state this rule consistently in docs, hover text, and reference examples
- avoid implying that all post-signature clauses participate in step inference

Primary implementation fronts:
- docs and hover text
- negative tests and diagnostic wording where needed

### 5. Canonical short-surface trust is not yet obvious enough

Problem:
Even when the compressed form exists, users still need a stable answer to: which short form is the recommended one, and which examples are trustworthy enough to copy.

Closure target:
- define one canonical compressed path per common authoring pattern
- separate smoke-covered stable examples from real-but-reference-only examples
- keep the language surface honest about what is stable, what is real, and what is still sketch-only

Primary implementation fronts:
- stable example surface board
- paired reference examples
- roadmap sections that use the same terminology as diagnostics and hover text

## Current prioritization rule

For the next authoring-surface passes, prioritize in this order:
1. reduce repeated writing
2. explain inferred behavior where repetition was removed
3. keep the short surface singular and documented
4. only then widen syntax or add new abstraction layers

This is the current working interpretation of Pergyra's authoring problem: the language is no longer primarily blocked on missing nouns; it is blocked on contract compression, contract provenance, and trustworthy short paths.
