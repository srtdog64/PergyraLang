# Pergyra Programming Language

> Current status: executable experimental alpha / beta-closure sprint.
> Feature-surface feel is about 85%, and strict beta readiness is now about 83%.
> The authoritative beta contract is `docs/100_beta_readiness_checklist.md`.

> 현재 상태: 실행 가능한 experimental alpha이며, 기능 체감 진행도는 약 85%,
> strict beta readiness는 약 83%로 본다. 아직 beta-complete는 아니다.
> 이 문서는 소개용 문서이고, 베타 계약의 최종 기준은 `docs/107_beta_stable_subset.md`,
> `docs/100_beta_readiness_checklist.md`, `docs/118_slot_model_rigor_audit.md`,
> `docs/119_pergyra_lineage_positioning.md`, `docs/120_vision_and_capability_audit.md`다.
> Pergyra를 production-ready, Rust-level memory safe, AI-first, quantum-ready,
> zero-cost, fully proven 언어로 설명하지 않는다.

> 서로 다른 자원을 같은 사고 체계로 다루기 위한 의미 통일 언어

## Intent의 정적 의미

> **Intent는 ‘프로그래머의 의도’를 주석처럼 표현하는 것이 아니라, 여러 compiler fact가 동일한 목적에서 발생했다는 귀속 관계를 보존하는 정적 identity다.**

Intent는 참여자·실행 순서·결과·권한·경계 등의 fact를 한 목적에 귀속시키는
source-level binder입니다. 각 fact의 소유권은 해당 owner에 남습니다.
목적을 선언했다고 권한이나 효과의 증거가 생기지는 않습니다. 같은 이름이나
같은 결과값만으로 동일한 귀속을 인정하지도 않습니다.

이는 [설계 정전](01_intent_first_design.md)의 정의이며, 모든 self-host 경로가
이미 완성됐다는 뜻은 아닙니다. 현재 검증한 범위와 남은 생산 경계는
[Intent 삭제·보강 의미 감사](audits/2026-09-05_intent_graph_semantic_audit.md)에
구분해 기록합니다.

## 기계층: 주소의 증거와 접촉의 허가

**`Region`은 포인터가 아니고, 주소를 안다는 사실만으로 기계 접촉이 허용되지는 않습니다.**

형식 모델은 선언된 범위(`Grant`), 그 범위에서 유도한 주소 증거(`Region`),
실제 상태 전이(`contact_step`)를 구분합니다. Intent가 보존하는 것은 이 행위의
목적 귀속이지 접근 권한이 아닙니다. 권한·살아 있는 lease·접근 모드 등의
접촉 조건은 별도 owner의 증거가 필요합니다.

현재 사용자 표면은 `DeviceSlot<T>`와 claim/read/write/release/submit-read
연산입니다. 명시적인 MIR fact, AIR 검증, C/LLVM 투영과 런타임 매핑 검사가
연결되어 있지만, 기본 대상은 host-sim입니다. 매핑 provider의 승인은 실제
MMIO 구현이나 보드 동작의 증명이 아닙니다. `Grant`/`Region` 사용자 문법,
실제 보드 연결, 하드웨어 volatile/atomic ordering은 아직 완성된 계약이 아닙니다.

[기계층 설계와 구현 범위](semantics/proofs/MachineLayerCore.md)에 현재 경로,
형식 모델의 전제, 구현하지 않은 범위를 함께 기록합니다.

## 사명

Pergyra는 자원의 세부 구현을 직접 드러내기보다, 서로 다른 자원들을 같은 사고 체계로 다룰 수 있게 만드는 **의미 통일 언어**입니다.

PergyraLang은 자원을 **Slot 계열 핸들 + 비동기 경계**라는 공통 모델로 다룹니다.

`Slot<T>`, `SecureSlot<T>`, `DeviceSlot<T>`는 로컬에 고정된 anchored 자원 핸들입니다. 원격 작업은 슬롯 자체를 직접 넘기기보다 `RemoteFuture<T>`를 `await`해 `Result<T>`로 회수합니다.

이로써 언어는 최고 성능 대신, **이종 자원 통합**과 **도메인 파편화 감소**를 목표로 합니다.

| Slot이 담는 것 | 예시 |
|----------------|------|
| 수명(Lifetime) | 스코프 진입 시 점유, 탈출 시 반환 |
| 소유권(Ownership) | anchored handle은 단일 바인딩 유지, movable handle은 명시적 이동만 허용 |
| 접근 권한(Permission) | SecureSlot의 토큰 기반 읽기/쓰기 제어 |
| 해제 의미론(Release) | `with` 블록 자동 해제, 명시적 `Release` |
| 보호 의미론(Protection) | 보안 슬롯의 권한 제어 및 토큰 기반 검증 |
| 전이 의미론(Transfer) | `recv/await` 경계 전달, 원격 결과는 `RemoteFuture<T>` → `Result<T>` |

## 개요

Pergyra는 포인터나 도메인별 핸들 API를 언어의 중심에 두기보다, **Slot**과 **병렬 오케스트레이션**으로 이종 자원의 수명과 흐름을 통일하려는 언어입니다.
LLVM 지원 빌드에서는 LLVM을 기본 백엔드로 사용하고, 그렇지 않은 경우 C 백엔드로 폴백합니다.

```
.pgy  -->  Lexer  -->  Parser  -->  Semantic  -->  HIR  -->  DIR  -->  RIR  -->  MIR  -->  Backend
                                                    │                  │                   ├→ LLVM → Object → Binary
                                                    │                  │                   └→ C    → C → GCC/Clang
                                                    ↓                  ↓
                                                    AIR (read-only synthesis, verification-only)
                                                                       ↓
                                                    drift / abstraction-safety 검증
```

> `AIR`는 codegen path에 얹지 않고 옆에 합성되는 verification-only IR입니다 (`HIR`/`RIR`로부터 단방향 합성, backend로 lowering 안 됨). 자세한 내용은 `docs/104_air_compiler_architecture.md`.

### 핵심 특징

- **Slot 기반 자원 모델**: `Slot<T>`/`SecureSlot<T>`/`DeviceSlot<T>`는 로컬 자원에 고정된 anchored handle 모델로 작동
- **보안 슬롯**: `SecureSlot<T>`에 토큰 기반 접근 제어
- **Subject-first 철학**: `subject`는 코어 host, `class`는 보조 nominal value 축으로 semantic/codegen에서 실제로 구분됨
- **제네릭 클래스**: `class Pair<T>` 단형화 기반 제네릭 (Pair<Int> → Pair_Int)
- **비동기 오케스트레이션**: `async/await`, `spawn`, `Channel<T>`, `select`, `parallel`
- **원격 결과 의미론**: `RemoteFuture<T>`를 `await`하면 `Result<T>`가 되어 실패 가능성을 타입에 남김
- **내장 병렬성**: `parallel` 블록으로 선언적 병렬 처리
- **스코프 기반 해제**: `with` 블록으로 자동 자원 반환
- **이종 자원 모델 확장성**: 디바이스 제어(`DeviceSlot`) 및 토큰 기반 보안 슬롯 등을 동일한 추상화 경계로 수용

## 빠른 시작

### 필요 조건

- GCC (C11 지원)
- GNU Make

### 빌드 및 실행

```bash
# 기본 빌드: 컴파일러와 LSP만 빌드한다. 테스트 바이너리는 포함하지 않는다.
make all

# 개발 빌드: 컴파일러/LSP와 프런트엔드·런타임 테스트 바이너리를 함께 만든다.
make all-with-tests

# LLVM 기본 백엔드 빌드
make LLVM_ENABLED=1 all

# Hello World 실행
./bin/pgy examples/hello.pgy --run -v

# LLVM IR 출력
./bin/pgy examples/hello.pgy --emit-llvm -o hello.ll

# 슬롯 데모 실행
./bin/pgy examples/slots.pgy --run -v

# 직접 회귀 진입점
make test-transpile
make test-abi
make llvm-test-backend-compare
make example-test-smoke
make ir-pipeline-test-smoke
make fmt-test-smoke
make dogfood-webgl-test-smoke
```

WebGL note: `examples/wasm_hello/` is a C `--emit-c` host-bridge dogfood gate,
not stable WebGL language surface. `pgy.render.webgl` is post-beta module work.

안정 예제 가이드:

- smoke-covered 예제 목록은 [docs/65_stable_example_surface_board.md](65_stable_example_surface_board.md)를 기준으로 본다
- 현재 바로 따라 써도 되는 대표 예제:
  - `examples/logistics_intent_probe/`
  - `examples/resource_scheduler_async_probe/`
  - `examples/order_analytics/`
  - `examples/battle_simulator/`
  - `examples/biome_simulator/`
  - `examples/wasm_hello/`
- `examples/party_system_demo.pgy`, `examples/world_roster_city.pgy` 같은 예제는 design sketch이며 stable syntax reference가 아니다

## 문법 예제

```pergyra
func Main() -> Void {
    let msg: Slot<String> = "Hello, Pergyra!";
    Log(msg);
}
```

```pergyra
// 스코프 기반 자동 해제
with slot<Int> as counter {
    Write(counter, 100);
    Log(Read(counter));
}  // counter 자동 해제

// 병렬 처리
parallel {
    Write(a, 10);
    Write(b, 20);
}

// 보안 슬롯
let ss: SecureSlot<Int> = ClaimSecureSlot();
let rv: ReadView<Int> = ViewRead(ss);
let wv: WriteView<Int> = ViewWrite(ss);
Write(wv, 42);
Log(Read(rv));
```

## 프로젝트 구조

```
PergyraLang/
  src/
    lexer/          # 토크나이저
    parser/         # AST 생성
    semantic/       # 타입 검사, 슬롯 분석
    codegen/        # C/LLVM 백엔드
    compiler/       # 컴파일 파사드와 네이티브 빌드
    runtime/        # 슬롯/병렬/채널/큐비트 런타임
    pgy_driver.c    # 컴파일러 드라이버
  examples/         # .pgy 예제 파일
  docs/             # 언어 설계 문서
  Makefile
```

## 테스트

```bash
# C/LLVM 결과 비교 회귀 테스트
make llvm-test-backend-compare

# 개별 테스트
make test           # 렉서
make test-parser    # 파서
make test-semantic  # 시맨틱 분석
make test-transpile # C 백엔드
make test-memory    # 메모리 레이아웃
```

현재 직접 확인한 범위는 다음과 같습니다.

- `make test-transpile` 통과 (`470 passed`)
- `make test-abi` 통과 (`56 passed`)
- `make llvm-test-backend-compare` 통과
- `make example-test-smoke` 통과
- `make ir-pipeline-test-smoke` 통과
- `make fmt-test-smoke` 통과

`make llvm-test-backend-compare`는 대표 예제 코퍼스에 대해 C/LLVM 결과를 비교하지만, 이것만으로 전체 기능 parity를 선언하는 문서는 아닙니다.

## 예제 신뢰도

모든 예제를 같은 강도로 믿으면 안 됩니다.

- `compile-smoke covered`: 현재 회귀 smoke가 직접 밟는 예제, stable reference로 우선 추천
- `design sketch`: 미래 표면이나 aspirational syntax를 보여 주는 예제, stable reference로 사용 금지

source of truth:

- [Stable Example Surface Board](65_stable_example_surface_board.md)

추천 stable 예제:

- `examples/logistics_intent_probe/`
- `examples/resource_scheduler_async_probe/`
- `examples/order_analytics/`
- `examples/subject_object_tobject/`
- `examples/ownership_forwarding_probe/`

design sketch 예제:

- `examples/party_system_demo.pgy`
- `examples/world_roster_city.pgy`

## Slot 시스템

Slot은 "무엇을 가리키는가(handle)"가 아니라, **"어떻게 다뤄야 하는가(규율)"**를 정의한다.

| 타입 | 자원 | 규율 |
|------|------|------|
| `Slot<T>` | 메모리 | anchored handle: 점유 → 읽기/쓰기 → 반환 |
| `SecureSlot<T>` | 보안 메모리 | anchored handle: 토큰 없이 접근 불가 |
| `DeviceSlot<T>` | 디바이스/가속기 자원 | anchored handle: device read/write/submit/release |

현재 `own/ref` 함수 경계 규칙은 일반 자원 전체에 열린 것이 아닙니다.
직접 확인 가능한 안정 범위는 `ref Slot<subject-host>` / `own SecureSlot<subject-host>` 입니다.
즉 `own/ref`는 현재 "전체 ownership 시스템"이라기보다 anchored subject-slot boundary subset입니다.

| 연산 | 설명 |
|------|------|
| `ClaimSlot<T>()` | 자원 점유 |
| `Write(slot, value)` | 값 쓰기 |
| `Read(slot)` | 값 읽기 |
| `Release(slot)` | 자원 반환 |
| `SubmitDeviceRead(slot)` | 원격/디바이스 읽기 제출 → `RemoteFuture<T>` |

`await` 결과는 경계 종류에 따라 다릅니다: `Future<T>`는 `T`를, `RemoteFuture<T>`는 `Result<T>`를 돌려줍니다.

런타임은 `PGY_PANIC`으로 다음을 방지합니다:
- 해제 후 읽기/쓰기
- 이중 해제
- 잘못된 토큰 접근

## TODO

- [ ] 어셈블리 최적화 런타임 (x86-64 슬롯 연산)
- [ ] 오케스트레이션 모델 강화 (`select` 공정성, timeout, cancellation, backpressure)
- [ ] Effect System (I/O, Timer 등 부작용 타입화)
- [ ] LLVM 백엔드 최적화 및 coverage 확장
- [ ] JVM 연동 (JNI 브릿지)
- [ ] 패턴 매칭 고도화
- [ ] 표준 라이브러리 안정화
- [ ] 패키지 매니저
- [ ] 모듈 시스템 안정화
- [ ] WebAssembly 타겟
- [ ] product-grade debugger/runtime integration

## 문서

- [컴파일러 파이프라인 가이드](20_compiler_pipeline_guide.md)
- [현재 진행 상황](docs/00_progress.md)
- [구문 레퍼런스](docs/grammar/01_syntax.md)
- [문법 정의](docs/grammar/02_grammar.md)
- [네이밍 규칙](docs/grammar/03_naming.md)
- [언어 상태 평가](docs/18_language_status.md)
- [보안 모드 설계](docs/03_security_mode_design.md)
- [제네릭 설계](docs/04_generic_design.md)
- [언어 비전](docs/00_vision.md)
- [비동기/동시성 설계](docs/05_async_concurrency.md)
- [개발 현황](docs/17_development_status.md)
- [Class 객체 모델](docs/22_class_object_model.md)
- [Intrinsic Template 개요](docs/intrinsic_templates/README.md)


## 라이센스

BSD 3-Clause License. [LICENSE](LICENSE) 참조.
