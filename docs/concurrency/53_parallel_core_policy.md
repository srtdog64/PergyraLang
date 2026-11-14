# Parallel Core Policy

마지막 업데이트: 2026-04-09

이 문서는 `parallel`을 Pergyra의 언어 코어 실행 primitive로 고정한다.

핵심 결정:

- `parallel`은 라이브러리 표면이 아니다.
- `parallel`은 단순 문법 설탕이 아니다.
- `parallel`은 실행 의미론을 바꾸는 코어 키워드다.

## 1. 왜 코어인가

`parallel`은 다음을 직접 바꾼다.

- 실행 순서
- 동기화 지점
- 슬롯 충돌 규칙
- 캡처/변이 허용 범위
- 취소/합류 계약
- backend lowering
- runtime scheduler 계약

즉 `parallel`은 “어떤 helper를 호출하느냐”가 아니라 “프로그램이 무엇을 의미하느냐”를 바꾼다.

한 줄 정의:

> `parallel`은 Pergyra의 기본 실행 모델을 확장하는 1급 실행 primitive다.

## 2. 코어와 키워드는 같은 말이 아니다

Pergyra에는 키워드이지만 코어 존재론이 아닌 축도 있다.

예:

- `class`
- `party`
- `roster`

이들은 여전히 언어 표면에서 중요할 수 있다.
하지만 “존재론/실행 계약의 중심축”인지와 “키워드로 존재하는지”는 다른 문제다.

구분 기준:

- 코어 축:
  - 타입/실행/권한/자원 규칙을 바꾼다
  - IR와 backend contract를 직접 가진다
  - runtime contract가 바뀐다
- 보조 축:
  - 도메인 모델링을 돕는다
  - 표면상 중요하지만 전체 실행 의미를 다시 정의하지는 않는다

이 기준에서 현재 코어 실행 축은 다음과 같다.

- `intent`
- `parallel`
- `async`
- `spawn`
- `await`
- `select`

이 중에서도 계층 중심은 `parallel`이다. 나머지는 그 하위 실행 surface다.

## 3. `parallel`과 `intent`의 관계

`intent`는 orchestration의 1급 구조다.
`parallel`은 execution primitive의 1급 구조다.

둘은 역할이 다르다.

- `intent`
  - 왜 이 일을 하는가
  - 어떤 경계와 승인, 보상, 실패 정책을 가지는가
- `parallel`
  - 이 일을 동시에 돌릴 수 있는가
  - 어떤 자원 충돌과 합류 규칙을 가지는가

즉:

- `intent`는 orchestration core
- `parallel`은 execution core

둘 다 1급시민이다.

## 4. 재배치 원칙

앞으로 concurrency 계층은 아래 순서로 읽는다.

1. `parallel`
2. `spawn`
3. `async`
4. `await`
5. `select`
6. `channel`
7. `cancel`

의미:

- `parallel`이 최상위 실행 primitive다
- `spawn`은 `parallel`의 task-producing surface다
- `async`는 suspension/coroutine surface다
- `await`는 async completion join surface다
- `select`는 readiness arbitration surface다

## 5. 고정 계약

### 5.1 `parallel`

`parallel`은:

- 실제 병렬 실행을 의미한다
- semantic 단계에서 slot/resource 충돌 검사를 요구한다
- backend에서 scheduler/runtime lowering을 가진다
- cancellation/합류/공정성과 연결된다

### 5.2 `spawn`

`spawn`은:

- 병렬 task를 생성하는 surface다
- 결과를 `Future<T>` 또는 관련 future handle로 돌려준다
- standalone concurrency가 아니라 `parallel` 계층 아래에 위치한다

### 5.3 `async`

`async`는:

- suspension/coroutine surface다
- 병렬성 그 자체가 아니라 비동기 suspension discipline이다
- 즉 `parallel`과 같지 않다

### 5.4 `await`

`await`는:

- async completion join surface다
- 병렬 작업의 종료를 관측하는 한 방법이다

### 5.5 `select`

`select`는:

- readiness arbitration surface다
- 병렬 채널/이벤트 입력을 어떤 순서로 소비할지 결정한다

## 6. intent 안에서의 제약

`intent` clause는 orchestration 계약에 머문다.

따라서 intent clause 안에 다음을 직접 넣지 않는다.

- `parallel`
- `spawn`
- `async`
- `await`
- `select`
- channel send/recv

이 규칙은 유지한다.
이유는 간단하다.

- `intent`는 orchestration declaration
- `parallel`은 execution primitive

둘을 섞으면 정적 계약과 실행 메커니즘이 다시 얽힌다.

## 7. 구현 방향

앞으로 구현은 아래 방향으로 정렬한다.

1. 문서에서 `parallel`을 코어 실행 primitive로 일관되게 설명
2. `spawn/select/async` 문서를 `parallel` 아래 실행 표면으로 재배치
3. slot/effect/capability 규칙에서 `parallel context`를 명시적으로 다룸
4. 테스트도 다음 축으로 분리
   - sequential
   - parallel-safe
   - parallel-rejected

## 8. 결정 요약

- `parallel`은 언어 코어다
- `intent`도 1급 코어다
- `class` 같은 축은 키워드일 수 있어도 코어 존재론일 필요는 없다
- 코어 여부는 “키워드인가”가 아니라 “실행/타입/권한 의미를 바꾸는가”로 판단한다

