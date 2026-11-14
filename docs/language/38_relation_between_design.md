# relation between 설계 (2026-04-06)

## 한 줄 요약

> relation은 반드시 "누구와 누구 사이"를 선언해야 한다. 최소 1개는 subject.

---

## 1. 문제 — relation이 struct와 구별되지 않았다

기존:
```pergyra
relation Alliance { let trust: Int; }
struct Alliance { let trust: Int; }
// 구조적으로 동일. 키워드만 다름.
```

relation이 keyword를 차지할 자격이 있으려면 struct가 못 하는 걸 해야 한다.

---

## 2. 해법 — `between` 절 강제

```pergyra
relation Name between Left, Right
{
    // 공유 상태
}
```

### 규칙

1. **최소 1개는 subject** — 관계의 주체가 반드시 있어야 한다
2. **나머지는 subject / object / class 가능** — NPC(object), 아이템(class)과의 관계도 표현
3. **1:1 또는 1:N** — `[]`를 붙이면 다수 관계
4. **relation 인스턴스는 양쪽이 공유** — 1개의 관계 = 양쪽이 참조

---

## 3. 카디널리티

| 문법 | 의미 | 예시 |
|------|------|------|
| `between subject, subject` | 1:1 | 결혼, 사제 관계 |
| `between subject, subject[]` | 1:N | 길드 (리더 1, 멤버 N) |
| `between subject, class[]` | 1:N | 소유 (플레이어 1, 아이템 N) |
| `between subject, object` | 1:1 | NPC 계약 |
| `between subject[], subject[]` | N:N | 동맹 (국가 대 국가) |

---

## 4. 예제

### 4.1 결혼 (1:1, subject + subject)

```pergyra
relation Marriage between subject, subject
{
    let years: Int;
    let trust: Int;
}

zone TownZone
{
    subject slot husband: Player;
    subject slot wife: Player;
    relation slot marriage: Marriage;
    // 컴파일러 검증: Marriage는 subject, subject 사이
    // husband(subject) + wife(subject) → OK
}
```

### 4.2 NPC 계약 (1:1, subject + object)

```pergyra
relation Contract between subject, object
{
    let terms: String;
    let expired: Bool;
}

zone VillageZone
{
    subject slot player: Player;
    object slot npc: Villager;
    relation slot contract: Contract;
    // player(subject) + npc(object) → OK
}
```

### 4.3 소유 (1:N, subject + class[])

```pergyra
relation Ownership between subject, class[]
{
    let acquired_at: Int;
}

zone InventoryZone
{
    subject slot player: Player;
    // player가 여러 아이템 소유 가능
    relation slot inventory: Ownership;
}
```

### 4.4 길드 (1:N, subject + subject[])

```pergyra
relation GuildMembership between subject, subject[]
{
    let rank: Int;
    let joined_at: Int;
}

zone GuildHall
{
    subject slot leader: Player;
    // leader 1명, member N명
    relation slot members: GuildMembership;
}
```

### 4.5 동맹 (N:N, subject[] + subject[])

```pergyra
relation Alliance between subject[], subject[]
{
    let trust: Int;
    let treaty_name: String;
}

zone DiplomacyZone
{
    // 여러 국가가 여러 국가와 동맹 가능
    relation slot alliances: Alliance;
}
```

---

## 5. struct와의 결정적 차이

| | struct | relation |
|--|--------|----------|
| 누구의 것인가 | 모름. 독립 데이터 | `between`으로 양쪽 명시 |
| 카디널리티 | 없음 | 1:1, 1:N, N:N |
| zone slot 검증 | 없음 | 양쪽 타입이 zone에 있는지 검증 |
| dangling 감지 | 불가 | 한쪽 제거 시 경고 가능 |
| 공유 의미 | 없음 | 양쪽이 같은 인스턴스 참조 |

---

## 6. 컴파일러 검증 사항

```
1. between 절 필수 — relation에 between이 없으면 컴파일 에러
2. 최소 1개 subject — between object, object는 에러
3. zone slot 매칭 — zone에서 relation slot 사용 시
   양쪽 타입에 맞는 slot이 zone에 존재하는지 검증
4. 카디널리티 — []가 붙은 쪽은 동적 배열로 코드젠
```

---

## 7. 코드젠 (C 출력)

### 1:1

```c
typedef struct {
    Player *left;      // subject
    Player *right;     // subject
    int years;
    int trust;
} Marriage;
```

### 1:N

```c
typedef struct {
    Player *owner;         // subject (1 쪽)
    Item **items;          // class[] (N 쪽)
    size_t items_count;
    size_t items_capacity;
    int acquired_at;
} Ownership;
```

---

## 8. 결정 이력

| 결정 | 선택 | 이유 |
|------|------|------|
| between 강제 | O | struct와 구별. relation만의 고유 기능 |
| 최소 1 subject | O | 관계에 주체가 없으면 의미 없음 |
| 1:N 지원 | O | 소유, 길드 등 현실 관계는 1:N이 대부분 |
| N:N 지원 | O | 동맹, 무역 등 |
| [] 문법 | O | 배열 의미 직관적 |
