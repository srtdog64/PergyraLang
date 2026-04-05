# 보일러플레이트 감소 설계 (2026-04-06)

## 원칙

> 낱개도 되고 묶음도 된다. 둘 다 유효한 문법이다.

---

## 1. Zone — 그룹 slot 선언

### 현재 (낱개만 가능)

```pergyra
zone BattleZone
{
    subject slot attacker: Player;
    subject slot defender: Player;
    subject slot healer: Player;
    effect slot burn: BurnEffect;
    effect slot poison: PoisonEffect;
}
```

5개 slot = 5줄. 같은 종류가 반복.

### 개선 (낱개 + 묶음 둘 다 가능)

```pergyra
// 묶음 — 같은 타입의 slot 여러 개
zone BattleZone
{
    subjects [attacker, defender, healer]: Player;
    effects [burn]: BurnEffect;
    effects [poison]: PoisonEffect;
}

// 낱개 — 기존 문법 그대로 유효
zone BattleZone
{
    subject slot attacker: Player;
    subjects [defender, healer]: Player;
    effect slot poison: PoisonEffect;
}

// 혼합도 가능
```

### 문법 규칙

```
// 낱개 (기존)
subject slot <name>: <Type>

// 묶음 (신규)
subjects [<name1>, <name2>, ...]: <Type>
effects [<name1>, <name2>, ...]: <Type>
relations [<name1>, <name2>, ...]: <Type>
```

복수형 키워드(`subjects`, `effects`, `relations`) + `[]` 안에 이름 나열.

### 배타적 on/off

```pergyra
zone RaidDungeon
{
    subjects [team1_tank, team1_healer, team1_dps1, team1_dps2]: Player;
    subjects [team2_tank, team2_healer, team2_dps1, team2_dps2]: Player;

    // 묶음 단위로 활성화/비활성화 가능
    // "team1 전원 비활성" 같은 연산이 자연스러워짐
}
```

---

## 2. Action — zone 안에서 within 생략

### 현재 (매번 within 명시)

```pergyra
zone BattleZone
{
    subject slot attacker: Player;

    action Attack(self, target: Player)
        requires Combatable
        within BattleZone          // ← 자기 zone인데 왜 또 쓰는가
        authorized by attacker
        causes DamageEffect
    {
        ...
    }
}
```

### 개선 (zone 내부 action은 within 자동)

```pergyra
zone BattleZone
{
    subject slot attacker: Player;

    action Attack(self, target: Player)
        requires Combatable
        // within BattleZone ← 생략. zone 안이니까 자명.
        authorized by attacker
        causes DamageEffect
    {
        ...
    }
}

// zone 밖에서 선언하면 within 필수
action GlobalAction(self)
    requires SomeAbility
    within SomeZone              // ← 필수
{
    ...
}
```

### 규칙

```
zone 내부 action → within 생략 가능 (자기 zone으로 추론)
zone 외부 action → within 필수
```

---

## 3. Intent step — 공통 컨텍스트 끌어올리기

### 현재 (who 매번 반복)

```pergyra
intent Purchase(buyer: Member)
{
    step browse  { where: ProductView;  who: buyer; }
    step select  { where: CartSection;  who: buyer; requires: Purchasable; }
    step pay     { where: PaymentZone;  who: buyer; authorized by: buyer; }
}
```

`who: buyer`가 3번 반복.

### 개선 (공통 절 끌어올리기)

```pergyra
intent Purchase(buyer: Member)
{
    who: buyer;        // 전체 step 공통 — 한 번만 선언

    step browse  { where: ProductView; }
    step select  { where: CartSection; requires: Purchasable; }
    step pay     { where: PaymentZone; authorized by: buyer; }
}

// step에서 who를 덮어쓸 수도 있다
intent Trade(buyer: Member, seller: Merchant)
{
    who: buyer;        // 기본값

    step browse  { where: ProductView; }
    step negotiate
    {
        where: TradeZone;
        who: buyer, seller;    // 이 step만 덮어쓰기
    }
    step pay     { where: PaymentZone; }
}
```

### 규칙

```
intent 레벨 who  → 모든 step에 적용 (기본값)
step 레벨 who    → 해당 step만 덮어쓰기
둘 다 없으면     → 컴파일 에러 ("who is required")
```

같은 원리로 `where`도 끌어올릴 수 있다:

```pergyra
// 전부 같은 zone에서 일어나는 intent
intent Tutorial(player: Player)
{
    who: player;
    where: TutorialZone;     // 공통 zone

    step intro  { on: player.StartTutorial(); }
    step combat { on: player.PracticeFight(); }
    step finish { post: player.tutorial_done; }
}
```

---

## 4. 정리 — 전부 "낱개 + 묶음" 이중 문법

| 대상 | 낱개 (기존) | 묶음 (신규) |
|------|-----------|-----------|
| zone slot | `subject slot x: T` | `subjects [x, y]: T` |
| action within | `within ZoneName` 명시 | zone 안이면 생략 |
| intent who | `who: x` 매 step | `who: x` intent 레벨 |
| intent where | `where: Z` 매 step | `where: Z` intent 레벨 |

기존 문법은 100% 유효. 묶음은 **추가 옵션**이지 강제가 아니다.

---

## 5. Action 계약 — within/authorized by/causes 전부 선택적

### 현재 (이미 구현됨)

```pergyra
// 모든 계약 절은 선택적이다. requires만 유일하게 의미 있는 기본 계약.
action Climb(self)
    requires Climber
    // within ← 생략. 산이면 어디서든 가능.
    // authorized by ← 생략. 본인이니까.
    // causes ← 생략. 효과 없을 수도 있음.
{
    ...
}

action NuclearLaunch(self)
    requires Commander
    within ControlRoom              // 장소 제한 필요할 때만
    authorized by president         // 승인 필요할 때만
    causes RadiationEffect          // 효과 있을 때만
{
    ...
}
```

### 규칙

```
requires      → 유일한 핵심 계약. "자격 없는 action은 의미 없다"
within        → 선택. 장소 제한이 필요할 때만.
               zone 안 action은 자동 추론 (2026-04-06 구현)
authorized by → 선택. 승인이 필요할 때만.
causes        → 선택. 효과가 있을 때만.
```

### zone 안 action의 within 자동 추론 (2026-04-06 구현)

```pergyra
zone BattleZone
{
    action Attack(self)
        requires Combatable
        // within BattleZone ← 자동 추론. 컴파일러가 채움.
    {
        ...
    }
}
```

시맨틱 검증 단계에서 `ctx->current_zone`이 존재하면 within을 자동 주입.

---

## 6. 구현 상태

| 기능 | 상태 | 파일 |
|------|------|------|
| action within 자동 추론 | 구현 완료 | type_checker.c (zone 내부 action within 자동 주입) |
| action within/authorized/causes 선택적 | 이미 구현됨 | parser_decl.c (조건부 파싱) |
| intent 공통 who/where | 구현 완료 | parser_intent.c (intent-level → step 전파) |
| zone 그룹 slot | 구현 완료 | parser_domain.c (subjects/effects/relations 복수형 파싱) |

---

## 7. 결정 이력

| 결정 | 선택 | 이유 |
|------|------|------|
| 낱개/묶음 공존 | O | 자유도. 낱개 = 유연, 묶음 = 간결 |
| zone 복수형 키워드 | subjects/effects/relations | 단수=낱개, 복수=묶음 직관적 |
| within 자동 추론 | zone 내부만 | zone 외부는 명시 필수 (안전) |
| intent 공통 절 | who, where | 반복 최소화. step에서 덮어쓰기 가능 |
