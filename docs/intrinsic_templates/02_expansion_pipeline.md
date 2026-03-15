# Intrinsic Template Expansion Pipeline

## 1. 개요

intrinsic template는 렉서 단계의 특별 문법이 아니라, 파싱 이후 컴파일러가 해석하는 구조적 확장으로 다룬다.

기본 파이프라인은 다음과 같다.

```text
Source
  -> Lexer
  -> Parser
  -> Builtin/Intrinsic Resolve
  -> Intrinsic Template Expansion
  -> Semantic Validation
  -> C Transpiler / LLVM Backend
```

## 2. 현재 구현과 맞물리는 지점

현재 저장소에는 이미 "이름 기반 built-in dispatch" 경로가 있다.

- `src/lexer/lexer.c` -- 내장 API 이름 식별
- `src/semantic/type_checker.c` -- `builtin_resolve()` 기반 분기
- `src/codegen/transpiler.c` -- built-in별 코드 생성 분기

따라서 intrinsic template의 첫 구현은 기존 built-in 경로를 확장하는 것이 가장 단순하다.

## 3. 권장 단계

### 3.1 1단계: 이름 등록

특정 API 이름을 intrinsic template 후보로 등록한다.

예:

- `HttpGetJson`
- `EventHandler`
- `SystemQuery`

이 단계에서는 일반 함수와 구분되는 내부 enum 또는 registry entry가 필요하다.

### 3.2 2단계: 구조 검증

호출 시그니처를 먼저 검증한다.

예:

- 제네릭 인자 수
- 인자 개수
- 호출 가능한 위치
- async 문맥 요구 여부

### 3.3 3단계: AST 또는 HIR 확장

검증이 끝나면 intrinsic call을 일반 구문으로 환원한다.

권장 방향:

- 단기: AST helper로 직접 확장
- 중기: HIR에 `HIR_INTRINSIC_EXPANSION` 같은 중간 표현 추가
- 장기: 확장 추적 정보와 진단 메타데이터 보존

### 3.4 4단계: 일반 시맨틱 처리

확장된 결과는 일반 타입 검사, 슬롯 규칙 검사, 효과 검사를 통과해야 한다.

### 3.5 5단계: 코드 생성

확장 이후에는 가능하면 전용 코드 생성 분기를 줄인다.
즉, intrinsic 자체보다 "확장된 일반 코드"가 백엔드 입력이 되게 하는 편이 유지보수에 유리하다.

## 4. 왜 문자열 치환이 아닌가

문자열 치환 방식은 다음 문제를 만든다.

- 위치 정보가 약해진다.
- 타입 오류가 템플릿 내부에서 불명확해진다.
- 부분 확장과 최적화가 어렵다.

Pergyra에서는 intrinsic template를 AST/HIR 레벨 구조 변환으로 정의한다.

## 5. 초기 구현 제안

초기 구현은 별도 범용 매크로 엔진보다 아래 순서를 권장한다.

1. `BuiltinKind` 또는 별도 `IntrinsicTemplateKind` 추가
2. `builtin_resolve()`와 유사한 registry 함수 추가
3. intrinsic call용 타입 검사 함수 추가
4. AST helper로 확장 노드 생성
5. 확장 결과를 기존 transpiler 경로로 통과

## 6. 진단 요구사항

intrinsic template는 반드시 진단 친화적으로 설계한다.

- 어떤 intrinsic가 선택되었는지 보여줄 수 있어야 한다.
- 어떤 코드로 확장되는지 추적 가능해야 한다.
- 잘못된 인자나 문맥 사용 시 일반 함수 오류보다 더 구체적인 메시지를 내야 한다.

예:

```text
HttpGetJson<T> requires an async context
```

```text
SystemQuery requires at least one component type
```

## 7. 문서상 결정

이 디렉터리의 설계에서는 intrinsic template를 다음으로 정의한다.

- 문법 추가보다 "의미 추가"에 가깝다.
- 내장 함수와 매크로의 중간 성격을 가진다.
- 첫 구현은 기존 built-in dispatch 위에 쌓는다.
