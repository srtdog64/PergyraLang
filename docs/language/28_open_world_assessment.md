# Pergyra Open-World Readiness Assessment (2026-04-05)

## 진단: 두 가지 구조적 위험

### 1. 닫힌 세계의 함정 (Closed-World Trap)

현재 통과하는 모든 시뮬레이터(battle_sim, biome_simulator, space_station, dnd_tavern)는 **결정론적(Deterministic) 닫힌 환경**이다.

- 입력값이 고정
- 오케스트레이션 순서가 스크립트화
- 외부 I/O 없음
- 단일 스레드 실행

상용 궤도에 오르려면 증명해야 할 것:

- 예측 불가능한 네트워크 I/O
- 멀티스레드 상태 변이
- 악의적/잘못된 외부 데이터 주입
- Zone과 Effect의 오버레이 충돌

### 2. 얕은 의미론의 자기 충족적 증명 (Self-Fulfilling Prophecy)

relation, effect, zone의 런타임 전파 모델은 **플래그 기반 배치 동기화**다.

테스트가 통과하는 이유: 현재 구현된 '얕은 의미론' 범위 안에서만 테스트가 작성되었기 때문.

---

## 현재 런타임 전파 모델의 실체

### Layer Activation: bool 플래그

```c
typedef struct BattleZone {
    Player attacker;
    Player defender;
    DamageEffect damage;
    bool __layer_active_damage;   // 단순 bool
    bool __state_poisoned;        // 단순 bool
} BattleZone;
```

- 효과당 하나의 bool 플래그
- **참조 카운팅 없음, 중첩 레벨 없음**
- "데미지 2번 적용"이 불가능 -- 하나의 zone = 하나의 effect 인스턴스
- 충돌 시 last-write-wins (중재 없음)

### _sync 헬퍼: 3단계 배치 파이프라인

```
Zone_sync(self):
  Phase 1: 모든 state/layer/projection 플래그 → false
  Phase 2: apply/maintain/link 지시문 재평가 → 조건부 true
  Phase 3: 활성 layer마다 Effect_sync() / Relation_sync() 호출
```

- **진짜 계산**이다 (플래그만 토글하는 게 아님)
- projection은 실제 데이터 복사
- 하지만 **sync 호출 사이에 모든 상태는 stale**
- 무효화(invalidation) 메커니즘 없음

### 동시성 안전: **없음**

- pthread_mutex_t, _Atomic, volatile, lock/unlock: 전무
- 생성된 코드는 모두 순차적 struct 멤버 접근
- Thread A가 zone_sync() 중에 Thread B가 HasLayer() 읽으면 → **data race**
- 찢어진 읽기(torn read) 가능

### 스케일링: 정적 크기 zone

- subject slot은 컴파일 타임에 struct 필드로 고정
- 10,000개 slot → 10MB struct → 스택 폭발
- 런타임 동적 slot 추가 불가능

---

## 수정 방향: 4단계

현재 상태 메모:
- `PGY_ZONE_THREADSAFE`, zone rwlock macro, generation counter, effect pool runtime macro는 이미 runtime에 존재한다.
- C backend `HasLayer(...)`는 generated helper로 rdlock + generation stale-warning을 감싸도록 올라왔다.
- `effect pool damage: DamageEffect capacity 8` parser/semantic/C transpile surface는 구현됐다.
- 남은 큰 공백은 LLVM parity와 더 공격적인 open-world validation이다.

### Phase 1: 동시성 기본 보호 (Critical)

zone struct 접근에 최소한의 동시성 안전 추가.

```c
// 생성되는 C 코드에 추가
typedef struct BattleZone {
    pthread_rwlock_t __zone_lock;    // 읽기-쓰기 락
    // ... fields ...
} BattleZone;

void BattleZone_sync(BattleZone *self) {
    pthread_rwlock_wrlock(&self->__zone_lock);   // 쓰기 락
    // ... sync logic ...
    pthread_rwlock_unlock(&self->__zone_lock);
}

bool BattleZone_HasLayer_damage(BattleZone *self) {
    pthread_rwlock_rdlock(&self->__zone_lock);   // 읽기 락
    bool result = self->__layer_active_damage;
    pthread_rwlock_unlock(&self->__zone_lock);
    return result;
}
```

비용: struct당 rwlock 하나. 단일 스레드에서는 no-op 매크로로 빠짐.

### Phase 2: Effect Instancing (Important)

현재: zone당 effect 1개 (구조체 필드)
목표: effect를 배열로 관리, 같은 타입 복수 적용 가능

```pergyra
// 현재 (불가능)
zone BattleZone {
    effect slot damage: DamageEffect    // 1개만
}

// 목표
zone BattleZone {
    effect pool damage: DamageEffect capacity 8   // 최대 8개
}
```

런타임: 고정 크기 배열 + 활성 카운트.

```c
typedef struct {
    DamageEffect instances[8];
    uint8_t active_count;
    bool __layer_active[8];
} DamageEffectPool;
```

### Phase 3: Stale State 해결 (Important)

**세대 번호(generation counter)** 도입:

```c
typedef struct BattleZone {
    uint32_t __sync_generation;        // sync 호출마다 증가
    // ...
} BattleZone;

// HasLayer가 generation을 체크
bool BattleZone_HasLayer_damage(BattleZone *self, uint32_t expected_gen) {
    if (self->__sync_generation != expected_gen) {
        // 경고: stale state 읽기
    }
    return self->__layer_active_damage;
}
```

컴파일러가 zone func 안에서 sync 이후 generation을 캡처하고, HasLayer 호출 시 자동 전달.

### Phase 4: Open-World 스트레스 테스트 (Validation)

실제로 다음을 테스트하는 예제:

1. **동시 zone 접근**: spawn으로 여러 fiber가 같은 zone을 읽고 쓰기
2. **대량 엔티티**: 1000개 creature가 하나의 zone에서 상호작용
3. **무작위 외부 입력**: Random으로 예측 불가능한 effect 적용/해제
4. **라이프사이클 엣지 케이스**: apply → detach → reapply → detach 반복

---

## 평가 등급

| 항목 | 현재 | 목표 |
|------|------|------|
| Layer activation | B- (bool 플래그, 정확하지만 경직) | B+ (pool + reference count) |
| Sync 헬퍼 | B (배치 파이프라인, 올바름) | A- (incremental + generation) |
| 동시성 | F (없음) | B (rwlock, 단일 스레드 no-op) |
| 스케일링 | C (정적 struct) | B (고정 배열 pool) |
| 스트레스 테스트 | F (닫힌 세계만) | B (open-world 시나리오) |

---

## 구현 완료 (2026-04-05)

### Phase 1 + 3: Zone 동시성 보호 + 세대 카운터

- `pgy_runtime.h`: `PGY_ZONE_LOCK_FIELD`, `PGY_ZONE_WRLOCK/RDLOCK/UNLOCK` 매크로
- `PGY_ZONE_THREADSAFE` 정의 시 pthread_rwlock, 미정의 시 no-op (비용 0)
- `PGY_ZONE_GENERATION_FIELD` + `PGY_ZONE_GENERATION_INC` — sync마다 증가
- zone struct에 자동 삽입 (transpiler_domain_role.inc)

### Phase 2: Effect Pool 런타임 매크로

- `pgy_runtime.h`: `PGY_EFFECT_POOL_DEFINE(Type, Cap)` → 고정 크기 배열 pool
- `PGY_EFFECT_POOL_APPLY/DETACH/DETACH_ALL/ACTIVE_COUNT/FOR_EACH` API
- 컴파일러 자동 생성은 미래 작업, 현재는 수동 사용 가능

### Phase 4: Open-World 스트레스 테스트

- `examples/stress_test/main.pgy` — 8 유닛, 100 틱, 무작위 effect apply/detach
- 랜덤 이벤트: poison, burn, heal, damage, cleanse, shield (확률 기반)
- duration 기반 effect (poisoned/burning 턴 수 자동 감소)
- shield absorption 메커닉
- 결과: 100틱 26ms 실행, 전원 사망 확인, 에너지 underflow 정상 처리

## 발견 경위

biome_simulator, battle_simulator, space_station, dnd_tavern_campaign 예제 구현 후 구조적 한계 식별.
핵심: 현재 테스트가 통과하는 것은 언어가 올바르기 때문이 아니라, 테스트가 현재 구현의 범위 안에서만 작성되었기 때문.
stress_test로 open-world 시나리오의 기본 검증 완료.
