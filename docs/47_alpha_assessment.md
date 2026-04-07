# Pergyra Alpha 평가 및 페인포인트

> 작성일: 2026-04-08  
> 테스트: `tests/alpha_full_keyword_test.pgy` — 전체 키워드 종합 예제

---

## 알파 언어 판정: **YES, 조건부**

Pergyra는 알파 수준의 언어이다. 근거:

### 알파 기준 충족 항목

| 기준 | 상태 | 비고 |
|------|------|------|
| 코어 타입 시스템 | ✅ | Int, Float, Bool, String, Array, Enum |
| 제어 흐름 | ✅ | for, while, if/else, match, break, continue |
| 함수 + 재귀 | ✅ | 파라미터, 반환값, 제네릭 |
| OOP 기본 | ✅ | subject, class, object, tobject, method dispatch |
| Ability/Role/Party | ✅ | 인터페이스, 구현, 합성 |
| 비동기 | ✅ | spawn, await, channel, select, async block |
| 도메인 모델 | ✅ | zone, world, relation, effect, event |
| Intent 시스템 | ✅ | step, guard, post, compensate, rollback, history |
| Slot 시스템 | ✅ | Slot, SecureSlot, DeviceSlot, QubitSlot |
| 모듈 시스템 | ✅ | namespace, import, export |
| 2개 백엔드 | ⚠️ | C: 완전, LLVM: 기본 동작 (intent 미지원) |
| 테스트 스위트 | ✅ | 100+ 케이스, backend_compare 체계 |

### 알파 판정 조건

```
✅ 핵심 기능이 end-to-end로 동작한다
✅ 현실적인 시나리오(거래 시스템)를 작성할 수 있다
✅ 최소 1개 백엔드(C)에서 모든 기능이 동작한다
⚠️ LLVM 백엔드는 기본 기능만 동작 (intent/world는 미완)
⚠️ 일부 엣지 케이스에서 불일치/미동작
```

---

## 발견된 페인포인트

### P1 — 심각 (기능 결손)

#### 1. LLVM 백엔드: Intent 포함 시 실패
```
pgy: LLVM compile failed: MIR routine 'Main' block[4] uses invalid source_hir_block_id
```
- Intent 시스템이 MIR에 cleanup/rollback 블록을 추가하지만, LLVM 에미터가 HIR 블록 ID로 역참조할 때 범위 초과
- **영향**: LLVM 백엔드에서 intent 포함 프로그램 컴파일 불가
- **원인**: `mir_populate_stmt_instructions`가 `source_hir_block_id`로 HIR 블록을 찾는데, intent 전용 블록은 HIR에 대응하는 블록이 없음

#### 2. Intent에 값 파라미터(Int, String) 전달 불가
```pergyra
// 이것이 안 됨:
intent ExecuteTrade(market: MarketZone, seller: Player, buyer: Player, price: Int) { ... }
// 에러: DIR intent[0] participant 'price' is unresolved
```
- Intent 파라미터는 subject/zone만 허용
- **영향**: 거래 금액, 수량 같은 값을 intent에 직접 전달 불가 → subject state에 미리 저장해야 함
- **제안**: `with price: Int` 같은 값 파라미터 문법 추가, 또는 context object 활용

#### 3. Relation publish 필드 매칭 제약
```pergyra
// 이것이 안 됨:
relation TradeLink for source: Player, target: Player {
    tobject slot receipt: TradeReceipt  // TradeReceipt 필드가 Player와 불일치
    publish receipt from target         // 에러: 필드 불일치
}
```
- `publish`는 source subject의 필드명과 tobject 필드명이 정확히 일치해야 함
- **영향**: 변환/매핑이 필요한 projection이 불가능
- **제안**: `publish receipt from target map { sellerId: target.id }` 같은 매핑 문법

### P2 — 중간 (DX 저하)

#### 4. Bool 출력 불일치
```pergyra
Log(!false);  // 출력: "1" (기대: "true")
```
- `!false`가 Bool이 아닌 Int로 취급되어 `pgy_log_int`로 출력
- **영향**: 디버깅 시 혼란

#### 5. Zone 내부 slot은 복사본
```pergyra
let seller = Player("Alice", 200, 3);
let market = MarketZone(seller, buyer);
market.ShowState();  // seller.gold=0 (기대: 200)
```
- Zone 생성자에 전달한 subject는 복사됨, 원본과 별개
- **영향**: 사용자가 원본 참조를 기대할 때 혼란
- **참고**: 이건 의도된 value semantics일 수 있음. 문서화 필요.

#### 6. Zone authority 경고 verbose
```
[WARNING] Zone apply should specify 'by <subjectSlot>' when authority is declared
[WARNING] Zone link should specify 'by <subjectSlot>' when authority is declared
[WARNING] Zone refresh should specify 'by <subjectSlot>' when authority is declared
```
- `authority` 선언 시 모든 zone 연산에 `by <slot>` 필수
- **영향**: 보일러플레이트 증가
- **제안**: authority가 1개면 auto-infer

#### 7. `Emit(event, args)` 구문 미동작
```pergyra
Emit(OnTradeComplete, 50);  // 에러: Undefined function 'Emit'
```
- 이벤트 발화가 빌트인 함수로 인식 안 됨 (C 백엔드 한정?)
- **영향**: 이벤트 시스템 활용 제약
- **우회**: 핸들러 직접 호출로 대체 가능

#### 8. Generic 컬렉션 생성 문법
```pergyra
let list: List<Int> = List();  // 에러: Undefined function 'List'
let map: Map<String, Int> = Map();  // 에러
```
- `List()`, `Map()` 생성자가 인식 안 됨
- **영향**: 제네릭 컬렉션 사용 제한
- **참고**: LLVM smoke test에서는 동작 → C 백엔드 경로 차이?

### P3 — 경미 (개선 희망)

#### 9. Intent guard 실패가 IntentLastFailed()에 반영 안 됨
```pergyra
// guard: buyer.gold >= 50 → false일 때
Log(IntentLastFailed());  // "false" (기대: "true")
```
- guard 실패로 intent가 실패해도 `IntentLastFailed()`가 false 반환
- **영향**: intent 실패 추적 불완전

#### 10. `!false`의 타입이 Bool이 아닌 Int
- 부정 연산자의 결과 타입이 피연산자 타입을 따르지 않음
- 타입 체커 or 코드 생성 단계에서 Bool 보존 필요

#### 11. `Slot<Int> = Claim()` vs `Slot<Int> = 0`
```pergyra
// Claim() → Undefined function 에러 (C 백엔드)
// 대안: Slot<Int> = 0  → 동작함
```
- Slot 초기화 문법이 백엔드마다 다름

---

## 키워드 사용 현황 (종합 테스트 기준)

### 사용된 키워드 (43/57)

```
ability  action   as       async    await    bind     break    case
channel  class    continue default  defer    else     enum     event
export   extends  extern   false    for      func     if       impl
import   in       include  let      match    namespace override own
parallel party    private  public   ref      require  return   role
secure   select   shared   slot     spawn    struct   subject  super
tobject  trait    true     type     unsafe   use      where    while
with
```

| 카테고리 | 키워드 | 테스트 커버 |
|----------|--------|------------|
| 제어 흐름 | let, func, return, break, continue, if, else, while, for, in, match, case, default | ✅ 전부 |
| 타입 | class, subject, struct/tobject, enum, type | ✅ 전부 |
| 가시성 | public, private, with, as | ⚠️ public/private 미테스트 |
| 비동기 | async, await, channel, select, spawn, parallel | ✅ channel+select |
| 소유권 | own, ref | ⚠️ 이 테스트에서 미사용 (smoke test에서 커버) |
| 모듈 | export, namespace, import, use, extern | ✅ namespace+export |
| Role/Ability | ability, role, include, require, override, super, secure | ✅ ability+role+require |
| 도메인 | party, relation, effect, zone, slot, shared, context | ✅ 대부분 |
| Systemic/World | systemic, world | ✅ world |
| 안전 | unsafe, defer, bind | ✅ defer |
| 리터럴 | true, false | ✅ |

### 미사용 키워드

| 키워드 | 이유 | 상태 |
|--------|------|------|
| `dyn` | dynamic dispatch — 고급 기능 | 별도 테스트 필요 |
| `trait` | ability의 별칭? | 명확화 필요 |
| `extends` | 상속 — 별도 시나리오 | 기본 테스트 필요 |
| `super` | 부모 호출 | extends와 함께 |
| `systemic` | 시스테믹 컴포넌트 | world 내부 고급 |
| `actor` | 액터 모델 | 별도 동시성 테스트 |

---

## 결론 및 다음 단계

### 알파로서 강점

1. **Intent 시스템** — 다른 언어에 없는 차별점. guard/post/compensate/rollback이 선언적으로 동작
2. **Zone/World 계층** — 공간적 경계 모델링이 자연스러움
3. **Ability/Role 합성** — trait + vtable 동적 디스패치
4. **Slot 시스템** — 자원 추상화가 타입 레벨에서 동작
5. **2개 백엔드** — C(완전) + LLVM(기본)

### 베타 진입을 위해 필요한 것

| 항목 | 우선순위 | 설명 |
|------|----------|------|
| LLVM intent 지원 | P0 | MIR cleanup 블록 처리 |
| Intent 값 파라미터 | P1 | `with price: Int` 문법 |
| Bool 타입 일관성 | P1 | `!false` → Bool |
| Emit() 빌트인 | P1 | 이벤트 발화 통합 |
| 에러 메시지 개선 | P2 | 행번호 + 컨텍스트 |
| Zone authority auto-infer | P2 | 보일러플레이트 감소 |
| 표준 수학 라이브러리 | P2 | Math.Sin/Cos/Sqrt |
| LSP 서버 | P3 | IDE 지원 |
