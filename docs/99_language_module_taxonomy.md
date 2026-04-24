# Language Module Taxonomy

마지막 업데이트: 2026-04-24

이 문서는 Pergyra의 언어 표면을 “무엇이 언어 정체성인가” 기준으로 나눈다. 목표는 베타 전 새 문법을 늘리는 것이 아니라, 이미 구현된 표면을 나중에 모듈 단위로 불러올 수 있도록 경계를 먼저 고정하는 것이다.

현재 stable import 구현은 파일 기반 `import "path.pgy";`와 `namespace` / `export` 조합이다. 아래 `pgy.*` 이름은 당장 새 문법을 의미하지 않는다. 베타 기준에서 문서, diagnostics, tests, future stdlib packaging이 따를 논리 모듈명이다.

## 1. Module Layers

```text
pgy.foundation
  -> pgy.core
       -> pgy.execution
       -> pgy.compat.oop
       -> pgy.compat.fp
       -> pgy.kit.*

pgy.runtime.scheduler is implementation machinery below pgy.execution.
```

실제 의미:

- `pgy.foundation`은 core가 실행될 수 있게 하는 값/제어/ABI 기반이다.
- `pgy.core`는 Pergyra의 언어 정체성이다.
- `pgy.execution`은 `parallel` 아래 실행 family다.
- `pgy.runtime.scheduler`는 fiber/coroutine 구현 층이지 언어 core가 아니다.
- `pgy.compat.*`는 기존 언어 스타일을 수용하는 compatibility surface다.
- `pgy.std.*`는 공통 표준 라이브러리다.
- `pgy.kit.*`는 도메인별 표준 패턴 묶음이다.
- machine-readable manifest: `docs/language_module_manifest.json`

## 2. `pgy.core`

Beta blocker다.

포함:

- `intent`
- `world`
- `zone`
- `subject`
- `relation`
- `effect`
- `projection`
- `authority`
- `handoff`
- runtime observability baseline
- anchored ownership boundary
- generic contract system
- ability bounds / multi-bounds / implemented default type argument resolution
- module visibility/export contract
- `parallel` as the core execution primitive

중요한 결정:

- generics는 FP/OOP 편의가 아니라 core domain contract다.
- `where T: A + B`는 generic library 장식이 아니라 zone/intent/projection/authority의 host/resource shape를 정하는 계약이다.
- `parallel`은 실행 의미론을 바꾸는 core primitive다.
- `intent`는 orchestration core이고 `parallel`은 execution core다. 둘은 병합하지 않는다.

Core에서 제외:

- Functor/HKT abstraction
- class-heavy OOP extension
- coroutine/fiber API 고도화
- app/web/page/storage convenience

## 3. `pgy.foundation`

Beta blocker지만 언어 identity로 과장하지 않는다.

포함:

- primitive values: `Int`, `Bool`, `String`, etc.
- `func`, `let`, expression and control-flow baseline
- callable values / lambda baseline
- `Option`
- `Result`
- stable collections: `List<T>`, `Set<T>`, `HashMap<String|Int|Long|Bool, T>`
- basic runtime ABI required by core contracts

역할:

- core contract가 실제 프로그램으로 컴파일될 수 있게 한다.
- core semantics를 새로 만들지 않는다.
- foundation feature가 core identity를 가리지 않게 문서와 예제를 제한한다.

## 4. `pgy.execution`

`parallel` 아래 실행 family다.

포함:

- `parallel`: core execution primitive
- `spawn`: task-producing surface under `parallel`
- `async`: suspension surface
- `await`: completion join surface
- `select`: readiness arbitration surface
- `channel`: dataflow support
- cancellation: execution control support

경계:

- `parallel`만 core primitive다.
- `spawn` / `async` / `await` / `select` / `channel` / cancellation은 execution family surface다.
- 이 family는 beta에서 smoke/parity를 유지하지만, fiber/coroutine 고도화 자체를 beta identity blocker로 보지 않는다.

## 5. `pgy.runtime.scheduler`

언어 모듈이 아니라 구현 메커니즘이다.

포함:

- POSIX ucontext runtime path
- Windows Fiber runtime path
- scheduler queue / worker machinery
- suspension / resume implementation detail

규칙:

- docs and diagnostics should not present fiber/coroutine as a top-level language axis.
- user-facing language order is `parallel -> spawn -> async/await -> select/channel`.
- runtime implementation may use fibers/coroutines without promoting them to core surface.

## 6. `pgy.compat.oop`

기존 언어 스타일을 수용하는 compatibility surface다.

포함:

- class-heavy convenience surface
- method-style sugar that does not change core authority/projection semantics
- legacy-friendly pattern names when they lower to Pergyra core shapes

주의:

- `object` / `tobject` are not automatically OOP compatibility. Boundary/projection `object` and transfer `tobject` participate in core domain contracts.
- `class` convenience should not become the default explanation for the language identity.
- inheritance-heavy models and hidden callback graphs are not beta core.

## 7. `pgy.compat.fp`

FP style support는 필요하지만 beta core는 아니다.

Beta-safe direction:

- `Map`, `Filter`, `Fold` as library functions where already supported by foundation types
- `OptionMap`, `ResultMap`, `ResultAndThen`
- small combinators that do not require new type-system axes

Beta-out-of-scope:

- `Functor` as a first-class ability over higher-kinded type constructors
- HKT
- type-family generalization beyond the frozen generic contract subset

결정:

- generic contract는 core다.
- Functor/HKT는 compatibility/future FP abstraction이다.
- 이 둘을 섞지 않는다.

## 8. `pgy.std.*` and `pgy.kit.*`

공통 표준 라이브러리와 도메인 키트는 새 키워드가 아니라 library/domain pattern layer다.

공통 표준 라이브러리 예시:

- `pgy.std.money`
- `pgy.std.datetime`
- `pgy.std.timer`
- `pgy.std.versioning`

도메인 키트 예시:

- `pgy.kit.ledger`
- `pgy.kit.obligation`
- `pgy.kit.device_adapter`

규칙:

- domain kit는 core semantics를 바꾸지 않는다.
- domain kit는 `intent/world/zone/subject/relation/effect/projection` 조합을 재사용한다.
- business vocabulary는 새 키워드가 아니라 importable pattern/library로 올린다.
- domain kit는 필요하면 `pgy.std.*`에 의존하지만, `pgy.core` 의미론을 직접 확장하지 않는다.

## 9. Migration Order

## 9. Implementation Rule: module boundary before module syntax

베타 전에는 `use pgy.core;` 같은 새 사용자 문법을 열지 않는다. 대신 다음 순서로 실제 모듈성을 만든다.

- Compiler internals: `.inc`로 붙인 단일 translation unit을 leaf axis부터 `.c` + narrow internal header로 자른다.
- Language packaging: `docs/language_module_manifest.json`의 `pgy.*` 논리 이름을 source of truth로 삼는다.
- Current stable surface: 파일 기반 `import "path.pgy";`와 `namespace` / `export`는 그대로 유지한다.
- Future surface: package-style `use pgy.core;`는 베타 이후 별도 설계로 열며, 기존 core semantics를 바꾸지 않는다.

이 방식은 C/C++의 textual include처럼 “파일을 붙여서 되는 구조”를 줄이고, Pergyra 쪽은 논리 모듈명을 먼저 고정한 뒤 loader/resolver/package syntax를 나중에 얹는 전략이다.

## 10. Migration Order

1. Documentation taxonomy freeze: done in this document.
2. Machine-readable manifest: `docs/language_module_manifest.json`.
3. Representative case tags: `docs/language_module_cases.json`.
4. Smoke gate: `make module-taxonomy-test-smoke`.
5. Diagnostics vocabulary alignment.
6. Examples grouped by module layer.
7. Tests tagged by module layer.
8. Current file-based import remains stable.
9. Future `use pgy.core;` / package-style loading can be designed after beta without changing core semantics.

## 11. Beta Rule

Beta completion means:

- `pgy.core + pgy.foundation` stable subset is end-to-end closed.
- `pgy.execution` has enough smoke/parity for the current supported family.
- `pgy.compat.oop`, `pgy.compat.fp`, and `pgy.kit.*` are allowed only when they do not expand beta blockers.
- Any surface outside this rule must be labeled `explicit reject`, `experimental`, or `beta-out-of-scope`.
