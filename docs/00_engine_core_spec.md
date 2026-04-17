# Pergyra Engine Core Spec

이 문서는 Pergyra를 "게임 엔진을 만들기 위한 언어"로 밀어가기 위한 최소 코어 스펙이다.
목표는 문법 실험을 늘리는 것이 아니라, 엔진 코어를 실제로 구현할 수 있는 작고 단단한 언어 축을 고정하는 데 있다.

현재 구현과의 관계:

- 이 문서는 roadmap 성격의 설계 문서다
- 현재 구현 기준의 상태 평가는 `docs/17_development_status.md`, `docs/18_language_status.md`를 우선한다
- 아래 예시 중 일부는 아직 미래 표면이다. 현재 기준으로 `Option<T>`와 `Result<T>`는 stable surface에 가깝지만, `parallel for` 같은 표면은 아직 미래 범주에 남아 있다

## 1. 목표

Pergyra Engine Core는 다음 요구를 만족해야 한다.

- 예측 가능한 성능
- 명시적 메모리 관리
- 데이터 지향 구조
- C ABI 기반 FFI
- 병렬 잡 실행
- 리소스/핸들/컨테이너 구현에 충분한 제네릭

## 2. 우선순위

반드시 먼저 완성할 것:

1. `struct`
2. 제네릭
3. 배열과 슬라이스
4. 명시적 메모리 모델
5. FFI
6. 모듈
7. 오류 처리
8. 잡 시스템

지금 당장 뒤로 미룰 것:

- 고급 메타프로그래밍
- 복잡한 역할/파티/world 문법
- 예외 기반 제어 흐름
- 고급 ability solver
- 상태 타입을 과도하게 얹은 제네릭 설계

## 3. 전역 문법 원칙

- 키워드는 소문자
- 공용 타입과 공용 API는 PascalCase
- 세미콜론 `;` 필수
- 블록은 같은 줄에서 `{`
- 엔진 코어의 기준 문법은 [syntax.md](/mnt/e/PergyraLang/doc/syntax.md)와 이 문서를 함께 따른다

## 4. Struct

`struct`는 엔진 코어의 기본 데이터 단위다.

규칙:

- `struct`는 기본적으로 값 타입이다
- 필드 레이아웃은 선언 순서를 유지한다
- 기본 목표는 POD 친화성이다
- 상속은 없다
- 메서드는 허용하지만 데이터 레이아웃을 숨기지 않는다
- 엔진 코어에서는 `subject/class`보다 `struct`가 우선이다

기준 문법:

```pergyra
struct Vec3 {
    x: Float;
    y: Float;
    z: Float;
}

struct Transform {
    position: Vec3;
    rotation: Quat;
    scale: Vec3;
}
```

필수 후속 기능:

- `sizeof(T)`
- `alignof(T)`
- C ABI 호환 레이아웃
- 배열 원소로 안전하게 배치 가능

## 5. 제네릭

제네릭은 엔진용 컨테이너, 핸들, 슬롯, 리소스 뷰를 위해 필요하다.

허용 범위:

- 타입 제네릭: `Array<T>`, `Slot<T>`, `Slice<T>`, `Handle<T>`
- 함수 제네릭: `func Swap<T>(...)`
- `where` 제약

초기 금지 범위:

- higher-kinded type
- 복잡한 암시적 계약 파생
- ability specialization

기준 문법:

```pergyra
struct Handle<T> {
    id: U32;
}

func Swap<T>(a: Slot<T>, b: Slot<T>) {
    let temp = Read(a);
    Write(a, Read(b));
    Write(b, temp);
}

func Max<T>(a: T, b: T) -> T
where T: Comparable {
    if a > b {
        return a;
    }
    return b;
}
```

구현 원칙:

- 초기 구현은 `monomorphization` 우선
- 디버깅 가능성을 위해 인스턴스화 규칙은 단순해야 한다
- 타입 인자 수와 제약 실패는 명확한 컴파일 오류로 보고한다

## 6. 배열과 슬라이스

엔진 코어는 소유 컨테이너와 비소유 뷰를 분리해야 한다.

핵심 타입:

- `Array<T>`: 소유하는 동적 배열
- `Slice<T>`: 비소유 연속 메모리 뷰
- `StaticArray<T, N>` 또는 추후 고정 길이 배열 문법

최소 요구 API:

- `Length`
- 인덱싱 `arr[i]`
- 슬라이스 생성
- 반복 가능

기준 문법:

```pergyra
let vertices: Array<Vertex> = CreateArray<Vertex>(1024);
let view: Slice<Vertex> = vertices.Slice(0, 256);

for i in 0..view.Length {
    ProcessVertex(view[i]);
}
```

엔진 관점 규칙:

- `Slice<T>`는 복사 비용이 낮아야 한다
- `Slice<T>`는 포인터 + 길이와 동등한 모델이어야 한다
- 컨테이너와 뷰의 소유권을 혼동하지 않게 타입을 분리한다

## 7. 명시적 메모리 모델

Pergyra는 GC 없이 동작해야 한다.

코어 메모리 모델:

- 스택 값
- `Slot<T>`: 명시적 수명 관리가 필요한 핸들형 저장소
- `with slot<T>`: 스코프 기반 자동 해제
- `Arena`
- `Pool<T>`
- `Allocator` 인터페이스

목표:

- 엔진 프레임 메모리
- 리소스 풀
- ECS 저장소
- 임시 scratch allocator

기준 문법 예시:

```pergyra
with slot<Mesh> as mesh {
    mesh.Write(LoadMesh(path));
    Render(mesh.Read());
}

let frameArena = Arena.Create(frameAllocator, 4_MB);
let tempVertices = frameArena.AllocArray<Vertex>(4096);
```

하드 규칙:

- 기본 힙 할당 숨김 금지
- 할당 비용이 큰 연산은 API에서 드러나야 한다
- 자동 해제는 스코프 기반에서만 허용한다

## 8. FFI

게임 엔진은 외부 라이브러리 없이 성립하지 않는다.
SDL, Vulkan, OpenAL, PhysX, platform SDK와 연결되어야 한다.

기준:

- 1차 목표는 C ABI
- `extern "C"` 호출
- 명시적 레이아웃의 `struct`
- 포인터는 FFI 경계에서만 제한적으로 허용

기준 문법:

```pergyra
extern "C" {
    func SDL_Init(flags: U32) -> Int;
    func SDL_Quit();
}
```

규칙:

- 언어 내부에서는 슬롯/슬라이스/핸들 중심
- FFI 경계에서만 raw pointer 타입을 허용
- ABI 불일치는 컴파일 단계에서 최대한 막는다

## 9. 모듈

엔진은 규모가 커서 파일 기반 모듈 시스템이 필수다.

기준 원칙:

- 파일/디렉터리 기반 모듈
- 명시적 `export`
- 명시적 `import`
- 순환 의존 최소화

기준 문법:

```pergyra
module Render.Mesh;

export struct Mesh {
    vertexBuffer: Handle<GpuBuffer>;
    indexBuffer: Handle<GpuBuffer>;
}

export func UploadMesh(mesh: Slice<Vertex>) -> Mesh;
```

```pergyra
import Render.Mesh;
import Core.Math.{Vec3, Mat4};
```

제한:

- 초기에는 제네릭 모듈 금지
- 중첩 모듈보다 파일 경로 기반 해석을 우선

## 10. 오류 처리

엔진 코어는 예외보다 값 기반 오류 처리를 사용한다.

코어 정책:

- 복구 가능한 오류: 장기적으로 `Result<T, E>`를 목표로 함
- 선택 값: 장기적으로 `Option<T>`를 목표로 함
- 복구 불가능한 오류: `Panic`
- 정리 보장: `defer`

현재 구현 기준:

- 안정된 표면은 우선 `Result<T>` + `?`
- `RemoteFuture<T>`는 `await` 시 `Result<T>`로 변환됨
- `Option<T>`와 `Result<T, E>` full surface는 아직 설계 목표에 가까움

기준 문법:

```pergyra
func LoadTexture(path: String) -> Result<Texture> {
    let bytes = ReadFile(path)?;
    let image = DecodePng(bytes)?;
    return Ok(CreateTexture(image));
}
```

원칙:

- 렌더 초기화, 파일 로딩, 쉐이더 컴파일은 `Result`
- 메모리 오염, 내부 불변식 파괴는 `Panic`
- 엔진 메인 루프에서 오류 경계를 명확히 둔다

## 11. 잡 시스템

게임 엔진은 병렬 실행 모델이 필요하다.
Pergyra의 `parallel`은 장식 문법이 아니라 잡 시스템의 표면 API가 되어야 한다.

최소 기능:

- `parallel { ... }`
- `parallel for`
- work-stealing 또는 고정 워커 큐
- 명시적 job dependency

기준 문법:

```pergyra
parallel {
    UpdateAnimation();
    UpdatePhysics();
    BuildRenderList();
}
```

```pergyra
parallel for i in 0..transforms.Length {
    IntegrateTransform(transforms[i], deltaTime);
}
```

안전 규칙:

- 같은 `Slot<T>`에 대한 동시 write-write는 컴파일러/분석기가 막아야 한다
- read-only 공유와 task-local 데이터는 허용
- 스케줄러가 아니라 언어 표면에서 병렬 의도를 드러낸다

## 12. 엔진 코어 표준 라이브러리 최소 집합

반드시 필요한 코어 모듈:

- `Core.Math`
- `Core.Memory`
- `Core.Containers`
- `Core.Job`
- `Core.IO`
- `Core.Platform`
- `Core.Assets`
- `Core.Result`

이 모듈들이 준비되기 전까지는 렌더러나 ECS를 먼저 확장하지 않는다.

## 13. 구현 순서

1. `struct` 문법과 타입 체크 안정화
2. 제네릭 타입/함수/where 최소 코어 고정
3. `Array<T>`와 `Slice<T>` 타입 체계 추가
4. `Allocator`, `Arena`, `Pool<T>` 런타임 설계
5. `extern "C"` FFI 파서와 타입 규칙
6. 파일 기반 모듈 해석
7. `Result<T>`를 중심으로 `?`, `defer` 정리 후 장기적으로 `Result<T, E>` 확장 검토
8. `parallel`과 `parallel for`를 잡 시스템에 연결

## 14. 성공 기준

Pergyra가 아래를 만들 수 있으면 엔진 코어 언어로서 1차 성공이다.

- 수학 라이브러리
- 동적 배열/슬라이스 라이브러리
- arena/pool allocator
- 파일 로더
- SDL 또는 GLFW 초기화
- 멀티스레드 잡 시스템
- 최소 렌더 그래프 또는 렌더 큐

이 단계 전에는 고급 세계관 문법보다 엔진 코어 기능을 우선한다.
