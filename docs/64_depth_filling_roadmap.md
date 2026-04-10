# Depth Filling Roadmap — 빈 칸 채우기

마지막 업데이트: 2026-04-11

이 문서는 [63_feature_depth_matrix.md](63_feature_depth_matrix.md)의 빈 칸을
채우기 위한 구체적 작업 목록이다.

핵심 원칙:
- **기능 확장 중단**. 새 키워드나 문법을 추가하지 않는다.
- **하강(Lowering) 집중**. 이미 있는 기능의 깊이를 채운다.
- 작업 단위는 "하나의 칸을 ❌→✅로 바꾸는 것"이다.

---

## Phase 1: 안전하지 않은 컴파일 방지 (P0)

> 목표: 타입 안전성이 깨진 경로를 막는다.
> 기준: 잘못된 코드가 에러 없이 컴파일되는 상황 제거.

### 1.1 Event 시맨틱 검증

**대상 칸:** Event → 시맨틱 검증 (❌ → ✅)

**작업:**

```
파일: src/semantic/type_checker.c
     src/semantic/type_checker_decls.inc

1. type_check_event_decl() 구현
   - event 파라미터 타입 해석 및 검증
   - 중복 event 이름 감지
   - event 심볼에 파라미터 시그니처 저장

2. AST_EVENT_SUBSCRIBE 타입 체커 switch 추가
   - 핸들러 함수 시그니처가 event 시그니처와 호환되는지 검증
   - 핸들러 반환 타입 Void 강제

3. AST_EVENT_UNSUBSCRIBE 타입 체커 switch 추가
   - subscribe와 동일한 시그니처 호환성 검증

4. AST_EVENT_INVOKE 인자 타입 검증
   - event 선언 파라미터 수/타입과 invoke 인자 매칭
```

**검증:** `test_semantic.c`에 테스트 추가 — 시그니처 불일치 시 에러 발생 확인.

---

### 1.2 Set/Map/List 시맨틱 검증

**대상 칸:** Set/Map/List → 시맨틱 검증 (❌ → ✅)

**작업:**

```
파일: src/semantic/type_system.c
     src/semantic/type_system.h
     src/semantic/type_checker.c
     src/semantic/type_checker_helpers.inc

1. TYPE_SET/LIST/HASHMAP을 constructed type으로 승격
   - size=0 primitive → generic container type descriptor
   - 타입 파라미터(element type, key type) 저장 구조

2. 제네릭 인스턴스 검증
   - Set<Int>, List<String>, Map<String, Int> 타입 파라미터 해석
   - 존재하지 않는 타입 파라미터 에러

3. 메서드 호출 타입 체크
   - .add(elem) → elem이 Set<T>의 T와 호환
   - .get(index) → index가 Int, 반환이 List<T>의 T
   - .set(key, val) → key/val이 Map<K,V>와 호환
   - .size() → Int 반환
   - .has(elem) → Bool 반환
   - .remove(elem/key) → Bool 반환
```

**검증:** `test_semantic.c`에 테스트 — 타입 불일치 메서드 호출 에러 확인.

---

### 1.3 Set/Map/List LLVM 코드젠

**대상 칸:** Set/Map/List → LLVM 코드젠 (❌ → ✅)

**작업:**

```
파일: src/codegen/llvm_expr.c (또는 새 llvm_collection.c)
     src/codegen/llvm_stmt.c
     src/codegen/llvm_internal.h

1. 컬렉션 생성자 코드젠
   - Set<T> → pgy_set_new_Suffix() 호출
   - List<T> → pgy_list_new_Suffix() 호출
   - Map<K,V> → pgy_map_new_Suffix() 호출
   - 런타임 함수 forward declaration

2. 메서드 호출 코드젠
   - SetAdd/SetHas/SetRemove/SetSize
   - ListPush/ListGet/ListSet/ListSize/ListRemove
   - MapSet/MapGet/MapHas/MapRemove/MapSize
   - C 트랜스파일러의 transpiler_expr_emitters.inc 로직 참조

3. Array literal → List 변환 (선택)
   - [10, 20, 30] → List<Int> 초기화
```

**검증:** LLVM 백엔드에서 Set/List/Map 사용 테스트 컴파일+실행 성공.

---

## Phase 2: 기능 완성 (P1)

> 목표: 부분 구현된 칸을 완성한다.
> 기준: 두 백엔드에서 모든 도메인 기능이 동작.

### 2.1 Event LLVM 코드젠

**대상 칸:** Event → LLVM 코드젠 (⚠️ → ✅)

**작업:**

```
파일: src/codegen/llvm_domain.c (또는 llvm_event.c 분리)

1. event struct 완성
   - 핸들러 함수 포인터 배열
   - handler_count 필드
   - PGY_EVENT_MAX_HANDLERS 상수

2. event_subscribe 코드젠
   - handler_count 범위 검사
   - 함수 포인터 배열에 추가

3. event_unsubscribe 코드젠
   - 배열에서 제거 + 압축

4. event_invoke 코드젠
   - handler_count 루프
   - 각 핸들러에 인자 전달 호출
```

**검증:** C/LLVM 출력 일치 확인.

---

### 2.2 Event 런타임

**대상 칸:** Event → 런타임 (❌ → ✅)

**작업:**

```
파일: src/runtime/pgy_runtime.h
     src/runtime/pgy_runtime_lib.c

1. PGY_EVENT_DEFINE(name, ...) 매크로
   - 핸들러 배열 + count
   - init/subscribe/unsubscribe/invoke 인라인 함수
   - (현재 C 코드젠이 인라인으로 생성하는 것을 런타임 매크로로 통합)

2. 동기 디스패치 구현 (최소)
   - invoke → 등록된 핸들러 순차 호출
   - 스레드 안전성은 P2로 미룸

3. LLVM에서 호출 가능한 raw export
   - pgy_event_subscribe_raw_export
   - pgy_event_invoke_raw_export
```

**검증:** 이벤트 subscribe + invoke 테스트 양쪽 백엔드 통과.

---

### 2.3 World LLVM 검증 강화

**대상 칸:** World → LLVM 코드젠 (⚠️ → ✅)

**작업:**

```
파일: src/codegen/llvm_domain.c

1. llvm_domain.c 내 world 관련 코드 정리 및 검증
   - world struct 생성 경로 검증
   - world_sync 함수 검증
   - world method dispatch table 검증

2. World 전용 테스트
   - world 생성 + zone activation + ShowWorldState
   - LLVM 백엔드로 컴파일 + 실행
   - C 백엔드와 출력 비교

3. (선택) llvm_world.c 분리
   - llvm_domain.c가 2500줄 이상이면 world 부분 분리
```

**검증:** `tests/alpha_full_keyword_test.pgy`의 World 섹션 LLVM 통과.

---

### 2.4 Channel 시맨틱 보강

**대상 칸:** Channel → 시맨틱 검증 (✅ → ✅ 강화)

**작업:**

```
파일: src/semantic/type_system.c
     src/semantic/type_checker.c

1. TYPE_CHANNEL을 정식 constructed type으로 등록
   - Channel<T>의 T를 타입 시스템에 기록
   - 현재 ad-hoc 해석 → 정식 타입 생성자

2. select 문 case 표현식 타입 검증
   - case v = <-ch: ch가 Channel<T>인지, v가 T인지

3. 초기화 전 사용 경고 (선택)
   - 미초기화 채널에 send/recv 시 경고
```

---

## Phase 3: 품질 (P2)

> 목표: 동작하는 기능의 견고성을 높인다.

### 3.1 Intent 런타임 히스토리

```
파일: src/runtime/pgy_runtime.h, pgy_runtime_lib.c

- 인텐트 실행 히스토리 링 버퍼 (최근 N개)
- IntentHistoryCount/IntentLastName/IntentLastFailed가 실제 데이터 반환
- 현재 전역 카운터만 있는 것을 구조체로 확장
```

### 3.2 Zone 런타임 authority 강화

```
파일: src/runtime/pgy_runtime.h
     src/codegen/transpiler_domain_role.inc
     src/codegen/llvm_domain.c

- PGY_ZONE_AUTHORITY_CHECK를 릴리스 빌드에서도 활성화 (옵션)
- 또는 시맨틱 레벨에서 컴파일 타임 authority 위반을 더 정밀하게 잡기
```

### 3.3 Set/Map/List 제네릭 일반화

```
- Map<K,V>: 키 타입을 String 이외로 확장 (Int, enum 등)
- Set<T>: String 이외 요소 타입 (Int, Float 등)
- List<T>: 인덱스 범위 검사
- for-each 이터레이션 지원
```

---

## 작업 순서 요약

```
Phase 1 (P0) — 안전하지 않은 컴파일 방지
  1.1  Event 시맨틱 검증
  1.2  Set/Map/List 시맨틱 검증
  1.3  Set/Map/List LLVM 코드젠

Phase 2 (P1) — 기능 완성
  2.1  Event LLVM 코드젠
  2.2  Event 런타임
  2.3  World LLVM 검증 강화
  2.4  Channel 시맨틱 보강

Phase 3 (P2) — 품질
  3.1  Intent 런타임 히스토리
  3.2  Zone 런타임 authority 강화
  3.3  Set/Map/List 제네릭 일반화
```

---

## 진행 추적

각 작업 완료 시 [63_feature_depth_matrix.md](63_feature_depth_matrix.md)의 해당 칸을 업데이트한다.

| # | 작업 | 매트릭스 칸 변화 | 상태 |
|---|------|-----------------|------|
| 1.1 | Event 시맨틱 | Event 시맨틱 ❌→✅ | 미착수 |
| 1.2 | Collection 시맨틱 | Set/Map/List 시맨틱 ❌→✅ | 미착수 |
| 1.3 | Collection LLVM | Set/Map/List LLVM ❌→✅ | 미착수 |
| 2.1 | Event LLVM | Event LLVM ⚠️→✅ | 미착수 |
| 2.2 | Event 런타임 | Event 런타임 ❌→✅ | 미착수 |
| 2.3 | World LLVM | World LLVM ⚠️→✅ | 미착수 |
| 2.4 | Channel 시맨틱 | Channel 시맨틱 강화 | 미착수 |
| 3.1 | Intent 런타임 | Intent 런타임 스텁→✅ | 미착수 |
| 3.2 | Zone 런타임 | Zone 런타임 스텁→✅ | 미착수 |
| 3.3 | Collection 일반화 | Set/Map/List C ⚠️→✅ | 미착수 |
