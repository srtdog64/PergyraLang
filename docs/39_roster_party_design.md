# Roster + Party 설계 (2026-04-06)

## 한 줄 요약

> **roster**는 party들의 컨테이너다. 인원 제한, 편성 규칙, 파티 간 공유 상태를 관리한다.
> (구 `systemic`을 대체)

---

## 1. 용어 정리

```
party   = subject 묶음. 함께 행동하는 단위. (가족, 길드, 분대)
roster  = party들의 컨테이너. 편성/제한/관리. (던전 레이드, 대회 참가팀 목록)
zone    = 행위가 일어나는 경계. (던전 층, 전투 구역)
world   = 실행/신뢰 경계. (게임 서버)

zone   → "어디서 행동하는가"    (실행 경계)
party  → "누가 함께 묶여있는가"  (군집 단위)
roster → "몇 파티가 참여하는가"  (편성 컨테이너)
```

---

## 2. 왜 roster가 필요한가

### 예시: 16인 던전 레이드

```
규칙: 4파티 × 4인 = 최대 16인
      최소 2파티 이상 참여해야 입장 가능
```

이걸 표현하려면:

```pergyra
party AdventureParty
{
    subject slot member1: Player
    subject slot member2: Player
    subject slot member3: Player
    subject slot member4: Player
    shared party_level: Int = 1
}

roster DungeonRaid
{
    party slot team1: AdventureParty
    party slot team2: AdventureParty
    party slot team3: AdventureParty
    party slot team4: AdventureParty
    shared max_parties: Int = 4
    shared min_parties: Int = 2

    func IsFull(self) -> Bool
    {
        return true;
    }

    func CanEnter(self) -> Bool
    {
        // 최소 2파티 이상
        return true;
    }
}

world Dungeon
{
    roster raid: DungeonRaid    // 파티 편성
    zone floor1: DungeonFloor   // 던전 1층
    zone floor2: DungeonFloor   // 던전 2층
    zone boss: BossRoom         // 보스방
}
```

### zone만으로는 안 되는 이유

```
zone = subject가 행동하는 경계
     = "이 구역에서 이런 action이 가능하다"

roster = party가 편성되는 컨테이너
       = "이 레이드에 몇 파티가 참여하는가"

zone은 "어디서"를 말한다.
roster는 "몇 팀이"를 말한다.
축이 다르다.
```

---

## 3. party의 가치 — 집계 경계

party는 단순한 그룹이 아니라 **집계 경계(aggregate boundary)**다.

```
tobject  → 데이터 1건을 전송
object   → 읽기 전용 뷰 1개
party    → 묶음 전체를 조회하는 단위

SQL로 치면:
  tobject = SELECT * FROM orders WHERE id = 1
  party   = SELECT * FROM members WHERE group_id = 7
            + JOIN relations ...
            + JOIN history ...
            = 그룹 단위 집계
```

즉 party는:
- 쿼리 이력 조회 필터
- 이 묶음에 속한 모든 subject + 관계 + 이력을 한 단위로 다룸
- DDD의 Aggregate Root와 유사

---

## 4. 이름 변경: systemic → roster

| | systemic (구) | roster (신) |
|--|--------------|-------------|
| 의미 | "시스템적인" (형용사, 모호) | "명부/편성" (명사, 명확) |
| 읽을 때 | "이게 뭐지?" | "아 파티 편성이구나" |
| 게임 맥락 | ? | "던전 로스터 4파티" 자연스러움 |
| 스포츠 맥락 | ? | "팀 로스터" 자연스러움 |
| 군사 맥락 | ? | "근무 명부" 자연스러움 |

### 호환성

- `roster`가 새 키워드
- `systemic`은 deprecated alias로 남겨둠 (기존 코드 호환)
- 새 코드는 `roster` 사용

---

## 5. 전체 도메인 계층과의 관계

```
intent  (왜)       → subject를 움직이는 사용자 의지
world   (경계)     → 실행/신뢰/실패 경계
roster  (편성)     → party 컨테이너, 인원 제한
zone    (어디)     → 행위가 허용되는 구역
party   (누구와)   → subject 묶음, 집계 경계
subject (누가)     → 행동 주체
class   (무엇으로) → 도구/사물
vessel  (내면)     → subject 내부 상태
relation (사이)    → subject 간 관계 (between)
effect  (결과)     → 행위의 결과
ability (자격)     → 행위 수행 자격
role    (이행)     → 자격의 구체적 구현
```

---

## 6. 결정 이력

| 결정 | 선택 | 이유 |
|------|------|------|
| systemic 이름 | roster로 변경 | systemic은 형용사, 의미 불명. roster는 명사, 즉시 이해 |
| party 유지 | O | 집계 경계 + 게임/시뮬레이션 핵심 개념 |
| roster 유지 | O | party 컨테이너로서 zone과 다른 축 |
| systemic 호환 | deprecated alias | 기존 코드 깨지지 않게 |
