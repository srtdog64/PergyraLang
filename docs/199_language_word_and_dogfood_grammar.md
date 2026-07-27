# 199. 언어 어휘 SoT와 도그푸딩 문법

Pergyra의 **언어 단어(language word) 레지스트리**와, Pergyra를 Pergyra로 쓸 때의
**도그푸딩 문법·구현 규칙**을 한 곳에 정리한다. 측정 시점의 실제 트리 기준이며,
수치는 재현 명령과 함께 적는다.

---

## 1. 언어 단어 레지스트리 (단일 권위)

**SoT: `src/lexer/language_keyword_registry.def`** (746줄, 145개 단어)

X-macro 한 줄이 단어 하나의 **9개 사실**을 동시에 선언한다:

```c
PGY_LANGUAGE_KEYWORD(
    spelling, class, token identity, stable debug/word identity,
    grammar context mask, semantic axis, implementation support,
    tooling support, TextMate highlight scope)
```

여기서 파생되는 것들 — 네이티브 `PgyLanguageWordId`, self-host `LanguageWordId`,
LSP 완성/호버, TextMate 하이라이트. **단어를 추가하려면 이 파일 한 곳만 고친다.**

규약 두 가지가 강제된다:
- 행은 spelling 기준 **바이트 정렬** (`lexer_lookup_keyword()`가 이진탐색)
- `HIGHLIGHT`는 정확히 하나만 non-NONE

주의: context/support/tooling/highlight는 **선언된 투영 사실(declared projection
fact)**이지 구현 완료 증거가 아니다. 실제 대조는
`docs/semantics/language_word_implementation_inventory.generated.md`가 한다.

### 1.1 클래스 (145)

| 클래스 | 수 | 의미 |
|---|---|---|
| RESERVED | 71 | 진짜 예약어. 식별자로 못 씀 |
| CONTEXTUAL | 71 | 렉서에선 **식별자로 남고**, 파서/툴링만 단어 ID를 선택 |
| SOFT | 3 | 소프트 키워드 |

절반이 contextual이라는 게 설계 의도다 — 도메인 어휘를 대량으로 들이면서도
사용자 식별자 공간을 잠그지 않는다.

### 1.2 의미 축 (axis)

| 축 | 수 | 성격 |
|---|---|---|
| EXECUTION | 48 | 제어·동시성·트랜잭션 |
| DOMAIN | 46 | 세계/의도 모델링 — **Pergyra 고유 표면** |
| TYPE_CONTRACT | 19 | 타입·계약 |
| GENERAL | 17 | 범용 |
| RESOURCE | 15 | 슬롯·소유·자원 |

DOMAIN 46개가 이 언어를 구별짓는 지점이다. 다른 언어에 대응물이 거의 없다.

### 1.3 전체 어휘 (축별)

범례: 표시 없음 = reserved, `~` = contextual/soft, `*` = **self-host 미지원**

**TYPE_CONTRACT (19)**
`ability as class dyn enum event extends fields~ func impl include innate is* override reflect requires~ struct type where`

**RESOURCE (15)**
`capacity* caps~ collapse forbids* inout~ local mut~ own pin* pool* ref secure shared slot with`

**EXECUTION (48)**
`async await backoff* blocking~ break channel compensate concurrent~ continue continuous~ current* defer else every~ exclusive~ expect~ fail failure~ for full* give* guard~ if invariant* join~ loop~ max* min* nondeterministic none* parallel post~ pre* priority* product* remote retry~ return rollback* select spawn step~ success~ sum* timeout* transaction unsafe while`

**DOMAIN (46)**
`action~ activate* apply~ authority~ authorized~ between~ bind by~ causes~ deactivate* detach* effect effects~ from~ intent involves* layer* lifecycle* link~ maintain* map~ move~ object objects* party projection* publish~ refresh~ relation relations* role roster state~ subject subjects* to~ tobject tobjects* transfer~ unlink* using~ vessel who~ within~ world zone`

**GENERAL (17)**
`all* any~ case default export extern false import in let match namespace on~ private public true use`

### 1.4 자체호스팅 프런티어

**네이티브 145 / self-host 112** → **33개가 네이티브 전용**:

```
activate all backoff capacity current deactivate detach forbids full give
invariant involves is layer lifecycle maintain max min none objects pin pool
pre priority product projection relations rollback subjects sum timeout
tobjects unlink
```

미지원이 EXECUTION(14)·DOMAIN(13)에 몰려 있다. 특히 복수형 짝
(`object`는 되고 `objects`는 안 됨, `tobject`/`tobjects`, `subject`/`subjects`,
`relation`/`relations`)과 예산·정책 어휘(`timeout` `priority` `backoff` `capacity`
`max` `min` `sum` `product`)가 통째로 남아 있다.

재현:
```bash
grep -c "^PGY_LANGUAGE_KEYWORD(" src/lexer/language_keyword_registry.def
grep -oE "PGY_KEYWORD_(CLASS|AXIS|SUPPORT)_[A-Z_]+" src/lexer/language_keyword_registry.def | sort | uniq -c
```

---

## 2. 도그푸딩 문법 — Pergyra로 쓴 Pergyra

`src/self_hosted/` 아래 **.pgy 1019개** (`_owner.pgy` 463, `main.pgy` 41).

### 2.1 기본 문법 규율

`src/self_hosted/lexer/char_owner.pgy`가 전형이다:

```pergyra
func CharAt(s: String, i: Int) -> String {
    let n: Int = StringLength(s);
    if i < 0 {
        return "";
    }
    if i >= n {
        return "";
    }
    return Substring(s, i, 1);
}
```

강제되는 규율:
- **타입 명시 전면** — 파라미터, 반환, 지역 `let` 모두. 추론 의존 없음.
- **가드절 + 조기 반환** — `else` 중첩 대신 실패를 먼저 걷어낸다.
- **예외 없음** — 실패는 값으로 돌아온다. 흔한 형태는 `.ok` 필드를 가진
  fact 구조체(자체호스팅 코드에 **771회**)와 `Result<T>`.
- **함수 PascalCase, 지역 snake_case.**
- 주석은 *무엇*이 아니라 **왜**를 적는다.

### 2.2 고유 표면도 실제로 도그푸딩된다

이게 중요하다 — 컴파일러가 범용 부분집합으로만 쓰인 게 아니다.
**컴파일러 자신의 파이프라인이 world/zone/subject/object/authority로 선언돼 있다.**

`src/self_hosted/compiler/world.pgy`:
```pergyra
object TestHarnessFacts { schema: String; }
tobject ParityVerdict { schema: String; }

zone SemanticVerdictZone {
    subject slot checker: SemanticStage
    object slot verdict: SemanticVerdict
    authority checker requires FactProving
}
```

`src/self_hosted/compiler/stage_intents.pgy` — intent가 zone들을 합성해
가시적 컴파일러 동작으로 만든다:
```pergyra
intent FrontendPipeline(intake: SourceIntakeZone, ...) {
    exclusive;

    step Lex {
        where: TokenStreamZone;   // 장소/수명
        using: tokens;            // 자원 zone
        who: lexer;               // 주체(권한)
        on: LexSource(tokens, lexer);
        expect: true;             // 사후조건
    }
}
```

`step`의 `where/using/who/on/expect`가 곧 docs/198의 포지셔닝
**"의도·권한·수명·예산 없이 효과 없음"**의 문법적 구현체다. 선언은 장식이 아니라
검사 대상이다.

측정된 사용량 (선언형, `grep -rE "^\s*<kw>\s+[A-Za-z_]"`):
`intent` 32, `role` 17, `tobject` 15, `vessel` 10, `world` 2, `party` 1.
`zone`은 `zone SourceIntakeZone {` 형태로 컴파일러 스테이지마다 선언된다.

**정직한 규모 감각**: `let` 13008 / `func` 5172 대비 고유 어휘는 두 자릿수다.
고유 표면은 **아키텍처 골격**(스테이지 = zone, 스테이지 계약 = intent)에
집중적으로 쓰이고, 본문 로직은 보수적 부분집합으로 쓴다. "쓰이지 않는다"는
틀렸고, "전면적으로 쓰인다"도 틀렸다.

---

## 3. 구현 규칙 (강제 장치)

### 3.1 소유자 매니페스트

`src/self_hosted/OWNERS.md`가 활성 컴파일러 스테이지 모듈의 SoT 지도다.

- 활성 `.pgy` 하나는 **하나의 스테이지 책임**만 가진다.
- `main.pgy`는 **엔트리포인트 전용** — argv 배선과 오케스트레이션만, 의미 결정 금지.
- `tests/self_hosted_component_contract_smoke.sh`가 **모든 활성 스테이지 소스가
  OWNERS.md에 등재됐는지** 요구한다.
- 이 파일은 물리 모듈 목록이다. 상위 의미 권위는
  `docs/semantics/sot_owner_spine_registry.md`가 따로 가진다 — 두 번째
  fact-family 레지스트리가 되면 안 된다.

목적은 명시적이다: 트랙이 *"디렉터리 하나 + 거대한 main.pgy"*로 무너지는 것을 막는다.

### 3.2 컴포넌트 계약 게이트

`tests/self_hosted_component_contract_smoke.sh`가 거는 핀:

| 장치 | 수 | 역할 |
|---|---|---|
| `require_text` | 5996 | 이 사실은 여기 있어야 함 |
| `reject_text` | 2314 | 은퇴한 경로 **부활 차단** |
| `require_max_lines` | 317 | 파일별 줄 수 상한 |

`reject_text`는 일반 문법 규칙이 아니라 대부분 **SoT 치환 핀**이다. 옛 경로를
지우고 나면 그 자리에 "다시 나타나면 실패"를 박아, 치환이 되돌아오지 못하게 한다.

`require_max_lines`가 파일 비대화를 막는다 — 상한에 닿으면 **책임 단위로 쪼개고**
핀을 재조준하는 것이 규정된 대응이다.

### 3.3 실무 함의

- 언어 단어 추가 → `.def` 한 줄. 생성물·툴링·self-host 투영이 따라온다.
- self-host에서 쓰려면 `PGY_KEYWORD_SUPPORT_SELF_HOST`를 켜고, 실제 셀렉터와
  픽스처가 생성 인벤토리 대조를 통과해야 한다.
- 새 스테이지 모듈 → OWNERS.md 등재 + 계약 핀 + parity 하네스가 fixture/expected를
  실제로 나열할 것.
- 파일이 상한에 닿으면 늘리지 말고 쪼갠다.

---

## 4. 미해결 / 주의

- **33개 네이티브 전용 단어**가 자체호스팅 프런티어다. 예산·정책 어휘
  (`timeout` `priority` `backoff` `capacity`)가 여기 몰려 있는데, 이는 docs/198이
  안전 주장의 축으로 내세우는 **budget** 축과 겹친다 — 즉 그 축은 아직
  자체호스팅으로 실증되지 않았다.
- context/support/tooling 필드는 **선언**이다. 구현 완료와 동일시하지 말 것.
  대조는 생성 인벤토리가 한다.
- 위 사용량 수치는 선언형 grep 기준이다. 문자열 리터럴(`"zone"` 34회 등)은
  컴파일러가 그 단어를 *파싱*하기 때문에 등장하는 것이므로 사용량이 아니다.
