# 199. 언어 어휘 SoT와 도그푸딩 문법

Pergyra의 **언어 단어(language word) 레지스트리**와, Pergyra를 Pergyra로 쓸 때의
**도그푸딩 문법·구현 규칙**을 한 곳에 정리한다. 측정 시점의 실제 트리 기준이며,
수치는 재현 명령과 함께 적는다.

---

## 1. 언어 단어 레지스트리 (단일 권위)

**SoT: `src/lexer/language_keyword_registry.def`** (145개 단어)

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

레지스트리의 `SUPPORT_SELF_HOST` 비트만 세면 112개지만, 이것은 선언된
지원 의도이지 parser 구현 증거가 아니다. 생성 inventory가 현재 source의
native selector, typed self-host selector, raw direct selector를 따로 센 결과는
다음과 같다.

| 구현 증거 | 단어 수 |
|---|---:|
| native + typed self-host selector | 80 |
| native + self-host direct-string selector만 존재 | 18 |
| native selector만 존재 | 46 |
| 양쪽 parser selector 없음 | 1 (`channel`) |

self-host direct-string selector는 34개 단어에 37회 남아 있다. 이는 구현
진척이 아니라 typed identity로 옮겨야 할 migration debt다. 정확한 행별
상태는 생성물
`docs/semantics/language_word_implementation_inventory.generated.md`가 소유하며,
이 fact family는 계속 `BRIDGE`다.

재현:
```bash
grep -c "^PGY_LANGUAGE_KEYWORD(" src/lexer/language_keyword_registry.def
grep -oE "PGY_KEYWORD_(CLASS|AXIS|SUPPORT)_[A-Z_]+" src/lexer/language_keyword_registry.def | sort | uniq -c
```

---

## 2. 도그푸딩 문법 — Pergyra로 쓴 Pergyra

`src/self_hosted/` 아래 **.pgy 1026개** (`_owner.pgy` 470, `main.pgy` 41).

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

### 2.2 direct-MIR subject/action/zone/world 한 slice만 production 도달

`struct`/`class`/`object`/`tobject`/`vessel`/`subject`/`action`의 선택 규칙은
[`200_object_to_action_boundary_patterns.md`](200_object_to_action_boundary_patterns.md)가
소유한다. 이 절은 그 구성체가 self-host 실행 그래프에서 실제로 도달하는지만
판정한다.

컴파일러 파이프라인의 목표 토폴로지는
world/zone/subject/object/authority로 선언돼 있다. 그러나 “선언이
컴파일된다”와 “실제 bootstrap entrypoint가 그 책임을 호출한다”는 다른
증거다.

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

`step`의 `where/using/who/on/expect`는 docs/198의 포지셔닝
**"의도·권한·수명·예산 없이 효과 없음"**을 표현할 수 있는 문법이다. 이
readiness world/zone/intent 묶음 자체는 load-bearing bootstrap 실행 증거가
아니다. 다만 direct-MIR mode에는 `DriverRung2Execution` subject/action,
`DriverRung2DirectMirZone`, 기존 `PgyCompilerWorld`의 한 slice가 실제 production
call chain으로 연결됐다. root intent와 나머지 resource zone은 아직 호출되지
않는다.

canonical bootstrap entrypoint에서 import를 재귀 해석한 감사 결과는 다음과
같다.

- import closure 443개, missing import 0;
- import-reachable 선언은 `func` 3,473개, `struct` 176개, `enum` 4개,
  `object` 18개, `tobject` 1개, `subject` 17개, `action` 17개, `zone` 19개,
  `world` 1개, `intent` 14개, `role` 4개, `ability` 4개;
- `class`/`vessel`/`effect`/`relation`/`party`/`roster` 선언은 0개;
- direct-MIR subject/action/zone과 world composition slice는 production mode에서
  `REACHABLE`이고 `Main`의 direct action/backend bypass는 삭제됐다;
- `world.pgy`의 나머지 subject/action 16개는 `Compiler*Ready()` 결합만
  반환하고 production artifact 경로에서 호출되지 않는다;
- intent 14개와 object/tobject projection schema도 import-reachable하지만
  active call-site가 없으므로 `SURFACE`다.

따라서 현재 bootstrap에는 subject/action/zone/world를 잇는 첫
Pergyra-native orchestration slice가 생겼지만 root intent와 source-to-MIR
artifact action은 아직 목표 골격이다. 이 direct-MIR slice는 `REACHABLE`이며
C-owned 구현을 대체하지 않았으므로 `SUBSTITUTING` 진척으로 세지 않는다.
또한 self-host parser가 action contract의 caps/effects를 보존하지 않고
`pgy.mir.v1`도 action 계약을 운반하지 않으므로 문법 fixture 통과를 full action
semantic parity로 승격하지 않는다. 이후 takeover 규칙은
`docs/self_hosted/17_pergyra_native_dogfood_contract.md`가 소유한다.

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

- direct-MIR의 `DriverRung2Execution.EmitDirectMir`과 그 zone/world slice만
  현재 `REACHABLE`이다. source-to-MIR/source-to-C mode와 root intent는 아직
  기존 func 경로나 호출되지 않는 표면에 남아 있다.
- subject/action이 전역 helper를 호출하는 C 선언 순서는
  `subject_action_global_helper` backend-compare 사례가 고정한다. action에
  중복 C prototype이나 local fallback을 넣어 이 순서를 우회하지 않는다.
- 실제 parser selector 증거는 typed 80 / direct-only 18 / native-only 46 /
  양쪽 없음 1이다. `SUPPORT_SELF_HOST` 비트로 계산한 112개를 구현 완료로
  인용하지 않는다.
- 예산·정책 어휘(`timeout`, `priority`, `backoff`, `capacity`)는 native-only
  집합에 남아 있으므로 docs/198의 budget 축은 아직 self-host parser로
  실증되지 않았다.
- context/support/tooling 필드는 **선언**이다. 구현 완료와 동일시하지 말 것.
  대조는 생성 인벤토리가 한다.
- 위 사용량 수치는 선언형 grep 기준이다. 문자열 리터럴(`"zone"` 34회 등)은
  컴파일러가 그 단어를 *파싱*하기 때문에 등장하는 것이므로 사용량이 아니다.
- 선언형 raw count도 production reachability가 아니다. fixture/generated/probe를
  제외한 entrypoint import/call graph로 `SURFACE`, `REACHABLE`,
  `SUBSTITUTING`을 구분한다.

---

## 5. 키워드 적정성 감사 (145행 전수)

`docs/42_keyword_orthogonality.md`의 **키워드 적정성 규칙**은 핵심 키워드가
(1) 구별되는 세계 모델링 좌표를 이름하고, (2) 컴파일러 fact owner를 가지며,
(3) proof/diagnostic/backend/runtime/verifier 의무를 지고, (4) 이웃과 직교하며,
(5) 도메인 명사가 아닐 것을 요구한다. 이 중 (2)·(3)은 기계적으로 감사 가능하다 —
"어떤 파서가 이 단어를 실제로 읽는가"로 환원되기 때문이다.

생성 인벤토리 기준 증거 클래스 × 단어 클래스:

| 증거 | reserved | contextual | soft |
|---|---:|---:|---:|
| native + self-host typed | 47 | 33 | – |
| native-only | 13 | 30 | 3 |
| direct-string only (부채) | 10 | 8 | – |
| **parser selector 없음** | **1** | 0 | 0 |

**규칙은 145행 중 144행에서 지켜진다.** 확인된 긍정 신호 하나: fixture 0개인
단어가 31개인데 **전부 contextual/soft**이고, 예약어 71개는 **전원 최소 1개
픽스처를 가진다.** 이름을 뺏는 단어에는 예외 없이 테스트가 붙는다.

### 5.1 위반 1건: `channel`

레지스트리는 `channel`을 RESERVED로, `SUPPORT_NATIVE | SUPPORT_SELF_HOST`로,
declaration/expression/type 세 문맥 전부로 선언한다. 실제로는:

- `TOKEN_CHANNEL`은 `src/lexer/lexer.h:115`에 **선언만 되고 그 외 참조가 0건**이다
  (`TOKEN_CHANNEL_OP`은 `<-` 연산자로 별개 토큰);
- 네이티브 파서 selector 0, self-host typed selector 0.

실측 (`bin/pgy --backend=c`):

```
channel<Int> ch;        → parse error: Unexpected token in expression
let channel: Int = 1;   → parse error: Expected variable name
```

**양방향 손실**이다. 예약어라 사용자에게서 `channel` 스펠링을 뺏지만, 문법으로는
아무것도 하지 않는다. 적정성 규칙 (2)·(3) 위반. 같은 치트시트 줄의 `select`는
정상 파스되며 `spawn`/`async`/`await`도 통과하므로, async 계열 전체가 아니라
**이 한 단어만 구멍**이다.

조치:
- `docs/grammar/00_cheatsheet.md`가 `channel<Int> ch;`를 작동 문법으로 광고하던
  줄을 제거했다(문서 드리프트).
- `tests/language_keyword_registry_smoke.sh`에 **적정성 래칫**을 추가했다:
  "reserved인데 parser selector 0"인 행 집합을 `KNOWN_DEAD_RESERVED="channel"`로
  핀 고정하고, 집합이 달라지면 fail-closed. 인벤토리는 이 상태를 **보고**만 했고
  아무것도 실패시키지 않아 조용히 누적될 수 있었다. Coq axiom budget과 같은
  방식 — 감춤이 아니라 **선언된 예외**이며, 이 목록은 줄어들 수만 있다.

`channel`의 처분(구현 / contextual 강등으로 스펠링 반환 / 행 제거)은 언어 로드맵
결정이므로 이 문서가 정하지 않는다. 어느 쪽이든 위 래칫을 갱신해야 한다.
