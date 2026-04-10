# Feature Depth Matrix — "넓지만 얕다" 현황판

마지막 업데이트: 2026-04-11

이 문서는 Pergyra의 각 도메인 기능이 파이프라인 어느 단계까지 **실제로** 구현되어 있는지를
코드 기준으로 추적한다. 문서에 적힌 설계가 아니라, 코드에 존재하는 구현만 기록한다.

핵심 원칙:
- 파싱만 되고 시맨틱이 없으면 **안전하지 않은 컴파일**이다.
- 코드젠이 있어도 런타임이 없으면 **링크 실패**다.
- 모든 칸이 채워져야 "기능이 있다"고 말할 수 있다.

---

## 1. 전체 매트릭스

| 영역 | 파싱 | 시맨틱 검증 | C 코드젠 | LLVM 코드젠 | 런타임 | 판정 |
|------|:----:|:----------:|:-------:|:----------:|:-----:|------|
| **기본** (let/func/class/if/for/while/match) | ✅ | ✅ | ✅ | ✅ | — | **완성** |
| **Intent** | ✅ | ✅ 실질적 | ✅ | ✅ | 스텁 | 동작하나 런타임 thin |
| **Zone** | ✅ | ✅ 실질적 | ✅ | ✅ | 스텁 | 동작하나 런타임 thin |
| **World** | ✅ | ✅ | ✅ | ⚠️ 부분 | 스텁 | LLVM 분리 필요 |
| **Event** | ✅ | ❌ 최소 | ✅ | ⚠️ 등록만 | ❌ 없음 | **위험** |
| **Channel** | ✅ | ✅ | ✅ | ✅ | ✅ 매크로 | 런타임 존재, 검증 보강 필요 |
| **Set/Map/List** | ✅ | ❌ 깨짐 | ⚠️ 부분 | ❌ 없음 | ✅ 매크로 | **LLVM 불가, 시맨틱 부재** |
| **Slot** | ✅ | ✅ | ✅ | ✅ | ✅ | **완성** |

범례: ✅ 동작 / ⚠️ 부분 구현 / ❌ 부재 또는 깨짐

---

## 2. 영역별 상세

### 2.1 기본 (let / func / class / if / for / while / match)

**판정: 완성**

모든 파이프라인 단계에서 동작. C/LLVM 출력 일치 확인됨.
enum, struct, object, tobject, subject 선언 + 메서드 디스패치 포함.

| 항목 | 상태 | 위치 |
|------|------|------|
| 파싱 | ✅ | `parser.c`, `parser_decl.c` |
| 시맨틱 | ✅ | `type_checker.c` 전체 |
| C 코드젠 | ✅ | `transpiler.c` |
| LLVM 코드젠 | ✅ | `llvm_stmt.c`, `llvm_expr.c`, `llvm_decl.c` |
| 런타임 | — | 런타임 불필요 (언어 코어) |

---

### 2.2 Intent

**판정: 동작하나 런타임이 thin**

step/guard/post/compensate/rollback 전체 흐름 동작.
3가지 rollback 정책(full/current/none) 정확 구현.
C/LLVM 출력 일치 확인됨.

| 항목 | 상태 | 위치 | 상세 |
|------|------|------|------|
| 파싱 | ✅ | `parser_intent.c` | step, guard, post, compensate, on, who, where, using, transfer |
| 시맨틱 | ✅ | `type_checker_decls.inc:960-1200+` | involves 해석, step zone/authority 검증, 계약 추론(who/where/transfer) |
| C 코드젠 | ✅ | `transpiler_domain_role.inc` | 전체 intent 흐름 생성 |
| LLVM 코드젠 | ✅ | `llvm_intent.c` (1673줄) | MIR topology 기반, cleanup/rollback/invalidation 블록 |
| 런타임 | 스텁 | `pgy_runtime_lib.c` | `IntentHistoryCount`, `IntentLastName`, `IntentLastFailed` → 최소 구현, 실제 히스토리 저장소 없음 |

**빈 칸:**
- 런타임 인텐트 히스토리 저장소 (현재 전역 카운터만)
- 인텐트 값 파라미터 (`with price: Int` 문법) 미지원
- 분산 인텐트 실행 (단일 프로세스 전용)

---

### 2.3 Zone

**판정: 동작하나 런타임이 thin**

zone slot, layer, state, projection, authority 전체 구조 동작.
refresh/publish/apply/link/detach/unlink 파서+시맨틱+코드젠 연결됨.

| 항목 | 상태 | 위치 | 상세 |
|------|------|------|------|
| 파싱 | ✅ | `parser_domain.c` | subject/object/effect/relation slot, state, authority, apply/link/detach/refresh/publish |
| 시맨틱 | ✅ | `type_checker_decls.inc:3331-3750+` | slot 타입 매칭, authority requires 검증, 중복 authority 감지, apply/link/refresh 대상 유효성 |
| C 코드젠 | ✅ | `transpiler_domain_role.inc` | struct 생성, sync 함수, projection, layer/state 플래그 |
| LLVM 코드젠 | ✅ | `llvm_domain.c` | struct 타입 + zone_sync 함수 생성 |
| 런타임 | 스텁 | `pgy_runtime.h` | `PGY_ZONE_AUTHORITY_CHECK` 매크로 (PGY_DEBUG only), HasLayer/HasState/HasProjection 플래그 |

**빈 칸:**
- 런타임 authority 검증이 `PGY_DEBUG` 전용 (릴리스에서 무시됨)
- zone 간 projection 경로 검증 없음
- zone 트랜잭션 시맨틱 (multi-slot 원자 업데이트) 없음

---

### 2.4 World

**판정: LLVM 분리 필요**

C 백엔드에서는 전체 동작. LLVM에서는 `llvm_domain.c`에 임베디드되어 있으나 분리/검증 불충분.

| 항목 | 상태 | 위치 | 상세 |
|------|------|------|------|
| 파싱 | ✅ | `parser_domain.c` | zone slot, roster, state, activate/deactivate/maintain |
| 시맨틱 | ✅ | `type_checker_decls.inc:1660-1850+` | roster/zone 참조 검증, composed state (ALL/ANY) 검증 |
| C 코드젠 | ✅ | `transpiler_domain_role.inc:1554-1900+` | struct + World_sync() + activation/zone sync/derived state |
| LLVM 코드젠 | ⚠️ | `llvm_domain.c:790-1368` | struct 타입 생성 + world_sync 존재하나 `llvm_domain.c`에 임베디드, 별도 파일 없음 |
| 런타임 | 스텁 | — | World 전용 런타임 함수 없음 (zone 런타임에 의존) |

**빈 칸:**
- LLVM world 코드젠을 `llvm_domain.c`에서 분리 또는 검증 강화
- World derived state projection 캐싱 미구현
- World 전용 런타임 함수 (현재 zone 런타임 재사용)
- LLVM 경로 world 초기화 함수 미정형

---

### 2.5 Event (**위험**)

**판정: 파이프라인 전반에 구멍**

파싱은 되지만 시맨틱 검증이 거의 없고, 런타임이 존재하지 않음.
핸들러 시그니처 불일치를 잡지 못하고 컴파일됨.

| 항목 | 상태 | 위치 | 상세 |
|------|------|------|------|
| 파싱 | ✅ | `parser_domain.c` | event 선언, subscribe(+=), unsubscribe(-=), invoke |
| 시맨틱 | ❌ 최소 | `type_checker.c:2748-2765` | 심볼 테이블 등록만. **핸들러 시그니처 검증 없음, subscribe/unsubscribe/invoke 미처리** |
| C 코드젠 | ✅ | `transpiler_domain_role.inc:2016-2125` | struct + INIT/SUBSCRIBE/UNSUBSCRIBE/INVOKE 인라인 함수 생성 |
| LLVM 코드젠 | ⚠️ | `llvm_domain.c:1841-1867` | struct 타입 등록만. **invoke/subscribe/unsubscribe 코드젠 불완전** |
| 런타임 | ❌ | — | **이벤트 전용 런타임 없음. 동기 직접호출만.** 큐/비동기 디스패치 없음 |

**빈 칸 (전부 채워야 함):**
- 시맨틱: `type_check_event_decl()` 구현 — 파라미터 타입 검증, 핸들러 시그니처 호환성
- 시맨틱: `AST_EVENT_SUBSCRIBE/UNSUBSCRIBE` 타입 체커 switch 추가
- 시맨틱: `AST_EVENT_INVOKE` 인자 타입 검증
- LLVM: event invoke/subscribe/unsubscribe 코드젠
- 런타임: 이벤트 핸들러 배열 + 동기 디스패치 (최소)

---

### 2.6 Channel

**판정: 런타임 존재, 시맨틱 보강 필요**

유일하게 런타임이 실제로 존재하는 도메인 기능 (pthread 기반 bounded buffer).

| 항목 | 상태 | 위치 | 상세 |
|------|------|------|------|
| 파싱 | ✅ | `parser.c` | `Channel<T>`, `ch <- val`, `<-ch`, `select { case }` |
| 시맨틱 | ✅ | `type_checker.c:1797-1880` | send/recv 타입 검증, anchored resource 거부, capability 전송 차단 |
| C 코드젠 | ✅ | `transpiler.c:714+` | `pgy_channel_init_T`, send/recv 호출 |
| LLVM 코드젠 | ✅ | `llvm_expr.c:188-235`, `llvm_stmt.c:2581-2790` | send/recv + select 문 |
| 런타임 | ✅ | `pgy_runtime.h:3704-4155` | MPMC bounded ring buffer + SPSC lock-free variant |

**빈 칸:**
- 시맨틱: `TYPE_CHANNEL` 정식 타입 없음 (ad-hoc constructed)
- 시맨틱: select 문 case 표현식 미검증
- 시맨틱: 초기화 전 사용 감지 없음
- SPSC variant가 언어 표면에 노출 안 됨

---

### 2.7 Set / Map / List (**LLVM 불가, 시맨틱 부재**)

**판정: C 백엔드에서만 부분 동작, LLVM 전혀 안 됨**

런타임 매크로(C 전처리기)는 존재하지만, 타입 시스템 통합이 깨져 있고
LLVM 백엔드에 코드젠이 없음.

| 항목 | 상태 | 위치 | 상세 |
|------|------|------|------|
| 파싱 | ✅ | `parser.c` | `Set<T>`, `List<T>`, `Map<K,V>`, 메서드 호출 |
| 시맨틱 | ❌ | `type_system.c:29-65` | `TYPE_SET/LIST/HASHMAP` 선언은 있으나 **size=0**, 제네릭 인스턴스 검증 없음, 메서드 타입 체크 없음 |
| C 코드젠 | ⚠️ | `transpiler.c:780-889`, `transpiler_expr_emitters.inc` | Set/List/Map 생성자 + 일부 메서드 (SetAdd/SetHas/ListPush/MapSet 등). **Set은 String만, Map은 String 키만** |
| LLVM 코드젠 | ❌ | — | **전혀 없음.** LLVM 백엔드에서 Set/List/Map 사용 시 컴파일 실패 |
| 런타임 | ✅ | `pgy_runtime.h:2422-3050`, `pgy_runtime_lib.c:515-980` | `PgyList_T`, `PgyHashMap_T`, `PgySet_T` 매크로 + raw export 함수 |

**빈 칸 (심각):**
- 시맨틱: 제네릭 컬렉션 타입 파라미터 검증
- 시맨틱: 메서드 호출 (`.add()`, `.size()`, `.get()`) 타입 체크
- LLVM: 컬렉션 생성자 + 메서드 코드젠 전체
- C 코드젠: Map 키 타입 일반화 (현재 String만), Set 요소 타입 일반화
- 런타임: 인덱스 범위 검사, iteration 지원

---

### 2.8 Slot

**판정: 완성**

lifecycle 강제 (claim→write→read→release), SecureSlot 토큰 검증,
DeviceSlot, QubitSlot 전부 동작.

| 항목 | 상태 | 위치 | 상세 |
|------|------|------|------|
| 파싱 | ✅ | `parser.c` | `Slot<T>`, `SecureSlot<T>`, `DeviceSlot<T>`, `QubitSlot<T>` |
| 시맨틱 | ✅ | `type_checker_builtins.c` | 빌트인 Read/Write/Release/Claim 검증, 토큰 페어링 |
| C 코드젠 | ✅ | `transpiler.c` | 모든 slot 타입 코드젠 |
| LLVM 코드젠 | ✅ | `llvm_runtime.c` | 런타임 바인딩 |
| 런타임 | ✅ | `pgy_runtime.h`, `pgy_runtime_lib.c`, `slot_security.c` | 6타입 claimed 검사, SHA256 토큰, 하드웨어 핑거프린트 |

---

## 3. 우선순위 분류

### P0 — 즉시 (안전하지 않은 컴파일 방지)

| # | 영역 | 빈 칸 | 이유 |
|---|------|-------|------|
| 1 | Event | 시맨틱 검증 전체 | 핸들러 시그니처 불일치가 컴파일 통과 → 런타임 크래시 |
| 2 | Set/Map/List | 시맨틱 검증 | 타입 파라미터 무시 → 타입 안전성 없음 |
| 3 | Set/Map/List | LLVM 코드젠 | LLVM 백엔드에서 컬렉션 사용 불가 |

### P1 — 단기 (기능 완성)

| # | 영역 | 빈 칸 | 이유 |
|---|------|-------|------|
| 4 | Event | LLVM 코드젠 | invoke/subscribe/unsubscribe LLVM 미구현 |
| 5 | Event | 런타임 | 이벤트 디스패치 메커니즘 없음 |
| 6 | World | LLVM 검증 강화 | 임베디드 코드 분리 + 테스트 |
| 7 | Channel | 시맨틱 보강 | TYPE_CHANNEL 정식화, select 검증 |

### P2 — 중기 (품질)

| # | 영역 | 빈 칸 | 이유 |
|---|------|-------|------|
| 8 | Intent | 런타임 히스토리 | 인텐트 추적/디버깅 |
| 9 | Zone | 런타임 authority | 릴리스 빌드에서도 authority 검증 |
| 10 | Set/Map/List | 제네릭 일반화 | Map 키 타입, Set 요소 타입 |

---

## 4. 달성 기준

모든 칸이 ✅가 되면 이 문서의 목적이 달성된 것이다.

```
기본       ✅ ✅ ✅ ✅ —   ← 완성
Intent    ✅ ✅ ✅ ✅ ✅
Zone      ✅ ✅ ✅ ✅ ✅
World     ✅ ✅ ✅ ✅ ✅
Event     ✅ ✅ ✅ ✅ ✅
Channel   ✅ ✅ ✅ ✅ ✅
Set/Map/List ✅ ✅ ✅ ✅ ✅
Slot      ✅ ✅ ✅ ✅ ✅   ← 완성
```

모든 ✅가 채워지면 Pergyra는 "넓고 깊다"고 말할 수 있다.
