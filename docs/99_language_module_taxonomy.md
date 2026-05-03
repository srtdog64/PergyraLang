# Language Module Taxonomy

마지막 업데이트: 2026-04-24

이 문서는 Pergyra의 언어 표면을 “무엇이 언어 정체성인가” 기준으로 나눈다. 목표는 베타 전 새 문법을 늘리는 것이 아니라, 이미 구현된 표면을 나중에 모듈 단위로 불러올 수 있도록 경계를 먼저 고정하는 것이다.

현재 stable import 구현은 파일 기반 `import "path.pgy";`와 `namespace` / `export` 조합이다. 아래 `pgy.*` 이름은 당장 새 문법을 의미하지 않는다. 베타 기준에서 문서, diagnostics, tests, future stdlib packaging이 따를 논리 모듈명이다.

## 1. Module Layers

```text
pgy.foundation
  -> pgy.core
       -> pgy.execution
       -> pgy.accel.spray
       -> pgy.render.webgl
       -> pgy.render.skia
       -> pgy.compat.oop
       -> pgy.compat.dop
       -> pgy.compat.fp
       -> pgy.kit.*

pgy.runtime.scheduler is implementation machinery below pgy.execution.
```

실제 의미:

- `pgy.foundation`은 core가 실행될 수 있게 하는 값/제어/ABI 기반이다.
- `pgy.core`는 Pergyra의 언어 정체성이다.
- `pgy.execution`은 `parallel` 아래 실행 family다.
- `pgy.accel.spray`는 GPU/AI 가속 라이브러리 축이다.
- `pgy.render.skia`는 Skia/render/shader integration을 위한 라이브러리 축이다.
- `pgy.runtime.scheduler`는 fiber/coroutine 구현 층이지 언어 core가 아니다.
- `pgy.compat.*`는 기존 언어 스타일(OOP/FP/DOP)을 수용하는 compatibility surface다.
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
- GPU/AI accelerator API
- Skia/render/shader API
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

## 6. `pgy.accel.spray`

AI-first 방향을 위해 필요한 GPU/accelerator 축이다. 다만 이것은 Pergyra core를 넓히는 새 키워드 축이 아니라, `parallel` / ownership / module visibility 위에 올라가는 library + runtime backend 축이다.

포함 후보:

- GPU device/context handle
- buffer/tensor memory ownership
- kernel launch graph
- parallel-to-accelerator scheduling bridge
- AI operator library boundary
- backend adapter: CPU fallback, CUDA, ROCm, Metal/Vulkan 후보

베타 기준:

- `pgy.accel.spray`라는 논리 모듈명과 경계를 예약한다.
- 새 키워드는 추가하지 않는다.
- 베타 blocker로 보지 않는다.
- core 문법에 CUDA/ROCm/Metal 같은 backend-specific surface를 섞지 않는다.
- `parallel`은 core execution primitive로 남고, Spray는 이를 가속 실행으로 lowering할 수 있는 라이브러리/런타임 축으로 둔다.

장기 설계 원칙:

- GPU memory는 일반 값이 아니라 owned accelerator resource로 취급한다.
- buffer/tensor는 `own` / `ref` / anchored handle policy와 충돌하지 않아야 한다.
- kernel launch는 암묵적 global state가 아니라 explicit context/stream/graph에 묶는다.
- AI operator는 언어 키워드가 아니라 `pgy.accel.spray`의 표준 라이브러리 API로 제공한다.
- tensor/functor/HKT 일반화는 이 모듈의 전제 조건이 아니다. 베타 이후에도 먼저 concrete tensor/operator contract를 닫고, 추상 FP 계층은 별도 검토한다.

## 7. `pgy.render.webgl`

Post-beta browser render bridge module. WebGL is not core language syntax and
is not beta-stable surface. The beta dogfood smoke only validates emitted-C
host imports and optional Emscripten linking; real WebGL API shape, resource
wrappers, shader/upload helpers, and browser runtime glue belong here after
beta closure.

Beta 기준:

- `pgy.render.webgl` is a reserved logical module name only.
- No WebGL keyword, shader syntax, or renderer-specific ABI is opened in core.
- `examples/wasm_hello/` is a bridge proof, not the module API contract.
- Native LLVM wasm remains beta+1.

## 7b. `pgy.render.skia`

Skia, shader, render graph는 장기 경쟁력에 중요하지만 core 문법이 아니다. `pgy.render.skia`는 Spray와 같은 생태계 모듈이며, 그래픽 리소스 수명과 backend adapter를 명시적으로 다룬다.

포함 후보:

- Skia canvas/surface handle
- render graph
- shader module boundary
- CPU/GPU render backend adapter
- texture/surface resource lifetime

베타 기준:

- `pgy.render.skia` 논리 모듈명과 경계를 예약한다.
- 새 키워드는 추가하지 않는다.
- shader language를 core syntax에 임베드하지 않는다.
- implicit global graphics context를 만들지 않는다.
- renderer-specific API는 library/backend adapter로 둔다.

장기 설계 원칙:

- render surface, texture, shader module은 owned resource다.
- render graph는 `parallel`과 Spray scheduler 위에서 실행될 수 있지만, core execution semantics를 바꾸지 않는다.
- Skia는 첫 render backend 후보일 수 있으나, 언어 계약은 Skia 하나에 종속되지 않는다.

## 8. `pgy.compat.oop`

기존 언어 스타일을 수용하는 compatibility surface다.

포함:

- class-heavy convenience surface
- method-style sugar that does not change core authority/projection semantics
- legacy-friendly pattern names when they lower to Pergyra core shapes

주의:

- `object` / `tobject` are not automatically OOP compatibility. Boundary/projection `object` and transfer `tobject` participate in core domain contracts.
- `class` convenience should not become the default explanation for the language identity.
- inheritance-heavy models and hidden callback graphs are not beta core.

## 9. `pgy.compat.dop`

DOP(Data-Oriented Programming)는 성능과 게임/시뮬레이션/AI 데이터 파이프라인에서 중요하지만 core identity가 아니다. 구조체 배치, SoA/Batch helpers, cache-friendly adapters는 compatibility/library surface로 둔다.

포함 후보:

- struct-of-arrays style helpers
- batch processing patterns
- data layout metadata
- cache-friendly collection adapters

베타 기준:

- 새 layout keyword나 unsafe memory model 확장은 열지 않는다.
- 현재 foundation collection과 ownership/ABI 계약 위에서 문서화만 한다.
- 베타 이후 ABI ownership closure가 충분해진 뒤 concrete library부터 추가한다.

## 10. `pgy.compat.fp`

FP style support는 필요하지만 beta core는 아니다.

Beta-safe direction:

- `Map`, `Filter`, `Fold` as library functions where already supported by foundation types
- `OptionMap`, `ResultMap`, `ResultAndThen`
- small combinators that do not require new type-system axes

Post-beta research notes:

- Zig `comptime`-style type-level computation is not part of the beta type
  system. Pergyra currently has monomorphized generics plus ability bounds, not
  first-class `type` values, arbitrary type-level functions, imperative
  compile-time membership checks, or user-customizable compile-time errors.
- Sbv-style symbolic execution and solver DSL ports belong here as a
  post-beta `pgy.compat.fp` experiment. They are useful stress tests for the
  generic/ability system, but they must not redefine the beta core language.
- If such a port needs type-level operators later, it must enter as an
  importable compatibility module with explicit diagnostics and no new core
  keyword by default.

Beta-out-of-scope:

- `Functor` as a first-class ability over higher-kinded type constructors
- HKT
- type-family generalization beyond the frozen generic contract subset
- Zig-comptime-style type-level metaprogramming
- user-customizable compile-time error generation

결정:

- generic contract는 core다.
- Functor/HKT는 compatibility/future FP abstraction이다.
- 이 둘을 섞지 않는다.

Decision addendum:

- Zig comptime / Sbv-style symbolic DSL work is compatibility/future FP
  territory, not beta core.

## 11. Module Ecosystem Update Policy

업데이트 빈도와 안정성 기준:

- `pgy.core`: 가장 자주 업데이트되지만 표면은 가장 작고 강하게 검증한다. core 변경은 semantic/runtime/C/LLVM/docs/tests 전체 기준을 만족해야 한다.
- `pgy.foundation`: core를 실행시키는 ABI/value baseline이다. core보다 느리게 움직이며 backend parity를 깨면 안 된다.
- `pgy.execution`: `parallel` 중심으로 core와 붙어 있지만, fiber/coroutine 구현 세부는 runtime mechanism으로 숨긴다.
- `pgy.accel.spray` / `pgy.render.skia`: 생태계 경쟁력 축이다. 빠르게 실험할 수 있지만 core keyword 확장 없이 module API와 backend adapter로 진화한다.
- `pgy.compat.oop` / `pgy.compat.fp` / `pgy.compat.dop`: 기존 스타일 수용층이다. core identity를 설명하는 기본 축으로 쓰지 않는다.
- `pgy.std.*` / `pgy.kit.*`: 도메인 패턴과 라이브러리다. 새 키워드가 아니라 importable module/API로만 확장한다.

## 12. `pgy.std.*` and `pgy.kit.*`

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

## 13. Implementation Rule: module boundary before module syntax

베타 전에는 `use pgy.core;` 같은 새 사용자 문법을 열지 않는다. 대신 다음 순서로 실제 모듈성을 만든다.

- Compiler internals: `.inc`로 붙인 단일 translation unit을 leaf axis부터 `.c` + narrow internal header로 자른다.
- Language packaging: `docs/language_module_manifest.json`의 `pgy.*` 논리 이름을 source of truth로 삼는다.
- Current stable surface: 파일 기반 `import "path.pgy";`와 `namespace` / `export`는 그대로 유지한다.
- Future surface: package-style `use pgy.core;`는 베타 이후 별도 설계로 열며, 기존 core semantics를 바꾸지 않는다.

이 방식은 C/C++의 textual include처럼 “파일을 붙여서 되는 구조”를 줄이고, Pergyra 쪽은 논리 모듈명을 먼저 고정한 뒤 loader/resolver/package syntax를 나중에 얹는 전략이다.

## 14. Migration Order

1. Documentation taxonomy freeze: done in this document.
2. Machine-readable manifest: `docs/language_module_manifest.json`.
3. Representative case tags: `docs/language_module_cases.json`.
4. Smoke gate: `make module-taxonomy-test-smoke`.
5. Diagnostics vocabulary alignment.
6. Examples grouped by module layer.
7. Tests tagged by module layer.
8. Current file-based import remains stable.
9. Future `use pgy.core;` / package-style loading can be designed after beta without changing core semantics.

## 15. Beta Rule

Beta completion means:

- `pgy.core + pgy.foundation` stable subset is end-to-end closed.
- `pgy.execution` has enough smoke/parity for the current supported family.
- `pgy.accel.spray` is reserved as a post-beta accelerator module and must not expand beta blockers.
- `pgy.render.skia` is reserved as a post-beta render/shader module and must not expand beta blockers.
- `pgy.compat.oop`, `pgy.compat.fp`, `pgy.compat.dop`, and `pgy.kit.*` are allowed only when they do not expand beta blockers.
- Any surface outside this rule must be labeled `explicit reject`, `experimental`, or `beta-out-of-scope`.
