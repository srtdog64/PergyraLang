# 159. Stdlib 승격 실행 설계도 (Sketch → Active Blueprint)

Status: `execution-blueprint`. 작성 2026-07-05. docs/148(stdlib **배선**: 층/
경계/7계약/inventory)이 *무엇을 지켜야 하나*를 고정한다면, 이 문서는 그 아래 —
**각 sketch 모듈을 active로 올리는 per-module 집행 스펙** — 을 diff-급으로
못박는다. 나 없이 집행 가능한 정밀도가 목표. 상위: docs/148(wiring), docs/12
(domain-lifecycle doctrine), docs/15(capability). 짝 문서: docs/158(self-bootstrap).

---

## 0. 한 장 요약

- **현재: active 2(option, strview) / sketch 11(datetime · device_adapter ·
  http · ledger · money · obligation · page · spray · storage · timer ·
  versioning).** sketch = 게이트 0 · caps 선언 0 · 도메인 fail-closed 0. 코드는
  있으나 **호환성 약속 없음, 예고 없이 변경/삭제 가능**(docs/148 §3-7).
- **집행 가능한 갭 = per-module doctrine-pass**(§2). docs/148이 순서와 계약을
  줬지만, "money.pgy를 어떻게 active로 올리나"의 파일-수준 diff는 없었다 —
  이 문서가 그것.
- **★ docs/158 분기와의 연결:** self-bootstrap에서 M1(코드젠 fixed-point
  선언)을 받고 레버리지를 thesis로 돌리면, **L2 stdlib가 더 중요해진다** —
  thesis(도메인 의미의 언어-복원)는 domain primitive로 증명되고, L2 domain
  모듈이 바로 그 primitive의 stdlib 표현이기 때문. self-host 대신 stdlib+
  던전크롤러가 근시일 레버리지라면, 이 문서가 그 실행 계획이다.
- **thesis 쇼케이스 = money/ledger/obligation.** std에 money가 있는 주류
  언어는 없다(docs/148 §2 "유일한 베팅"). 그리고 현 `MoneyAdd`가 통화 불일치를
  무검사 통과하는 게 docs/12 domain-UB의 정확한 반례 — 이걸 fail-closed로
  닫는 게 thesis의 stdlib 실증이다.

**★재서열 (BDFL 2026-07-06, docs/176):** stdlib은 두 트랙 — **기반 트랙 =
mathlib**(알고리즘 table stakes: random/hash/sort/floatconv/… — 논문이 스펙,
doctrine-pass 8번 항 신설)이 먼저, **전시 트랙 = 도메인 삼형제**(본 문서의
money 1호)는 WO-MATH-1~3 뒤 재개. 근거·모듈-논문 표·L0 선행(wrapping builtin)·
순서는 docs/176이 정본. §2 템플릿에 8번 항(canonical reference + 논문 유래
vector fixture)이 추가된 것으로 읽을 것.

**★모듈 명명 원칙 (BDFL 문답 2026-07-06):** **개념 = 모듈, 버티컬 = 예제/
네임스페이스.** money는 Fowler Money 패턴·JSR-354 계보의 개념명으로 유지;
"finance"는 산업 버티컬이라 모듈명이 될 수 없고, 이미 올바른 층(examples/
finance_ledger_probe)에 산다. 가족 묶음(finance = money+ledger+obligation)은
self-host 후 네임스페이스가 올 때 그 이름으로. 게임 금화가 `use finance`를
쓰게 만들지 않는다(킬러 유즈케이스 정합).

---

## 1. 현재 상태 (docs/148 §4 inventory)

active 2: `option.pgy`(per-type Option/Result 브리지), `strview.pgy`(무할당 뷰).
sketch 11: 전부 L2 domain. **일괄 sketch 사유(2026-07-04 감사): 게이트 0 ·
caps 0 · 도메인 fail-closed 0.** 표본 반례(현 `stdlib/money.pgy`):

```pgy
func MoneyAdd(a: Money, b: Money) -> Money {
    return Money(a.minorUnits + b.minorUnits, a.currency);   // ← USD+EUR가 조용히 USD
}
```

이건 docs/12의 domain-UB: 통화 불일치(도메인 불변식 위반)를 무검사 통과시키고
`a.currency`로 결과를 조작한다. 산업 SW라면 정확히 "작은 로직 오류가 큰 사고"인
지점(CLAUDE.md §0). doctrine-pass가 닫아야 할 바로 그것.

---

## 2. ★ Doctrine-Pass 템플릿 (재사용 체크리스트 — 이 문서의 코어)

**어느 sketch 모듈이든 active로 올리려면 아래 7항을 전부 만족해야 한다.** 이건
docs/148 §3의 7계약을 per-module 집행 단계로 편 것. 각 항 옆에 집행자 게이트.

1. **불변식 명문화** `[설계]` — 이 모듈의 도메인 불변식을 한 줄로 적는다
   (money: "산술·비교는 같은 통화에서만"; datetime: "month∈1..12, day∈1..31/월별").
   불변식이 없으면 그 모듈은 doctrine-pass 대상이 아니라 순수 값 모듈(L1 후보).
2. **fail-closed 집행** `[gate: 전용 fixture]` — 불변식 위반 = **거절**. 형태:
   - 복구 가능(호출자 오류) → **`Result<T>` + `Err`**(Result-first, CLAUDE.md §1.2).
     money/datetime 생성·산술이 여기.
   - 복구 불가(계약 파탄) → **패닉 클래스**(런타임 fail-closed). 도메인 상태
     불변식(vessel Empty→Filled 무효연산 류, docs/12) 위반이 여기.
   - **무음 통과 절대 금지**(§1 반례가 바로 그것).
3. **per-type API** `[gate: G1]` — generic seam(docs/151 §8 G-rung) 닫히기 전
   `<T>` 금지. 명명 `<동사구><Type>`. **domain 모듈은 대개 이미 per-type**
   (Money·LocalDate는 concrete class라 G1 자동 충족 — option.pgy `OptionOrInt`
   선례와 동형).
4. **caps 선언** `[gate: G2]` — ambient 빌트인(ReadFile/Now/Random/WriteFile/
   Args/DirWalk…)에 닿으면 `with caps` 의무. **순수 산술 모듈(money)은 caps
   무접촉 = 공허 충족.** timer(Now)·storage(파일)·http(NETWORK)가 실접촉.
   stdlib이 capability showcase(docs/15) — caps 선언이 곧 전시.
5. **namespace 블록** `[gate: G3]` — `SelfHostDiagnostic` 선례대로 namespace로
   감싼다(2026-07-04 grandfather 13개 외 신규/승격 전부 G3가 요구).
6. **fixture ≥1** `[gate: inventory + G4-lite]` — backend_compare fixture 또는
   전용 smoke 하나 이상. 정상 경로 + **fail-closed 경로 둘 다** 단언, C==LLVM.
   (docs/148 §3-5: active = fixture 실존 + docs/138 행 + 본 inventory 행.)
7. **inventory 행 flip** `[gate: stdlib-inventory-test-smoke]` — docs/148 §4
   표에서 `sketch → active` + gate 열 채움. **같은 커밋에서** 트리·docs/138·
   docs/148이 양방향 일치(inventory leg가 강제).

**완료 정의:** 위 7항 green + `make stdlib-inventory-test-smoke` green + 해당
전용 fixture green(정상+fail-closed 양방향, 양 백엔드).

---

## 3. 킬러-유즈케이스 → 모듈 매핑 (진짜 순서)

docs/148 §4의 견인 순서(datetime → page/spray → money/ledger → http)는 **doctrine
난이도 순**(datetime가 가장 단순한 템플릿)이다. 하지만 **킬러 유즈케이스(웹
던전크롤러, docs/15)가 실제로 당기는 순서**는 다를 수 있다:

| 던전크롤러가 필요로 하는 것 | 모듈 | 우선도 |
|---|---|---|
| 화면 렌더(방/인벤토리/전투 UI) | **page · spray** | **★ 최우선**(콘텐츠 그 자체) |
| 통화/보상/상점 경제 | money · ledger · obligation | 높음(thesis 쇼케이스) |
| 세션 타임스탬프/쿨다운 | datetime · timer | 중간 |
| 세이브/로드 | storage | 중간(caps: 파일) |
| 멀티플레이/리더보드 | http | 낮음(caps: NETWORK, WASM 후) |

**권고:** doctrine-pass **템플릿을 datetime로 1회 완주**(가장 단순, §4.2)해서
7항 파이프라인을 검증한 뒤, **던전크롤러가 실제 당기는 page/spray/money를
그 템플릿으로 찍어낸다.** "무엇을 먼저"는 당신의 던전크롤러 작업이 무엇을
막히게 하느냐가 정한다 — stdlib은 dogfood 압력을 받는 층(docs/148 §2)이니까.

---

## 4. 두 모듈 완전 스펙 (템플릿)

### 4.1 money.pgy — thesis 쇼케이스 + 반례 (완전 diff)

**불변식:** 산술(add/sub)·비교(eq/gte)는 **같은 통화에서만**. 다른 통화 = 도메인
위반 = 거절. minorUnits 산술 overflow는 언어의 checked-arith가 이미 fail-closed
(class `arithmetic-overflow`) — 모듈은 통화만 책임.

**diff(before → after):**

```pgy
// BEFORE (sketch — 무음 통과)
func MoneyAdd(a: Money, b: Money) -> Money {
    return Money(a.minorUnits + b.minorUnits, a.currency);
}

// AFTER (doctrine-pass — Result-first fail-closed)
func MoneyAdd(a: Money, b: Money) -> Result<Money> {
    if a.currency != b.currency {
        return Err("MoneyAdd currency mismatch: " + a.currency + " vs " + b.currency);
    }
    return Ok(Money(a.minorUnits + b.minorUnits, a.currency));
}
```

동일 처리 대상: `MoneySub`(통화 검사), `MoneyEq`/`MoneyGte`(현재 통화 다르면
`false` 반환 — 이건 무음 아님이나, "다른 통화 비교" 자체가 caller 버그일 수
있으니 doctrine 결정점: `false` 유지 vs `Result<Bool>`. **권고: 비교는 `false`
유지**(순수 술어, 총함수), **산술만 `Result`**). `MoneyNeg`(단항, 불변식 무관 —
그대로). `MoneyOf`/`MoneyZero`/`RenderMoney`(생성·표시 — 그대로).

**7항 적용:**
1. 불변식: "산술은 동일 통화". ✅ 명문.
2. fail-closed: `Result<Money>` + `Err`(복구 가능 — caller가 통화 정렬). ✅
3. per-type: Money = concrete class, `<T>` 없음. G1 ✅ 자동.
4. caps: 순수 산술, ambient 무접촉. G2 ✅ 공허.
5. namespace: `namespace Money { ... }` 블록. G3.
6. fixture: `tests/cases/backend_compare/stdlib_money_failclosed/` — 같은 통화
   add = Ok, 다른 통화 add = Err, 양 백엔드 동일. **fail-closed 경로 단언 필수.**
7. inventory: docs/148 §4 `money.pgy | domain | sketch → active`, gate 열 채움.

**호출부 영향:** `MoneyAdd`가 `Money → Result<Money>`로 바뀌므로 소비자는
`?`(Result 전파, docs/147 U1 착지) 또는 명시 분기 필요. 이게 domain UB를
컴파일 타임에 caller로 밀어내는 것 = thesis의 실증(의미가 구문으로 올라옴).

### 4.2 datetime.pgy — 가장 단순한 템플릿 (파이프라인 검증용)

**불변식:** LocalDate month∈1..12, day는 월별 유효 범위. 현재 `LocalDate`는
year/month/day를 **무검증** 저장(month=13, day=99 통과).

**diff:**

```pgy
// BEFORE (sketch — 무검증 생성자 사용)
export class LocalDate { let year: Int; let month: Int; let day: Int; ... }
// (생성이 class 생성자 직호출 → 범위 검증 없음)

// AFTER (doctrine-pass — 검증 생성자)
func LocalDateOf(year: Int, month: Int, day: Int) -> Result<LocalDate> {
    if month < 1 || month > 12 {
        return Err("LocalDate month out of range: " + ToString(month));
    }
    if day < 1 || day > DaysInMonth(year, month) {
        return Err("LocalDate day out of range: " + ToString(day));
    }
    return Ok(LocalDate(year, month, day));
}
```

`DaysInMonth`(윤년 포함)는 per-type 순수 함수. `Key`/`Format`(현재 메서드)은
그대로(총함수). 7항은 money와 동형(caps 공허, per-type 자동, namespace, fixture
정상+범위위반 양방향, inventory flip).

**왜 datetime 먼저:** 불변식이 자명(달력)하고 caps 무접촉이라 doctrine-pass 7항
파이프라인을 **가장 적은 도메인 논쟁으로 1회 완주**할 수 있다 — 템플릿 검증용.
그다음 money/page/spray를 같은 틀로 찍는다.

---

## 5. 11 sketch 모듈 per-module 스펙 표

각 모듈의 (불변식 / fail-closed 형태 / caps / 던전크롤러 견인). 착수 시 §2
7항 + 아래 행으로 스펙 확정.

| 모듈 | 도메인 불변식 | fail-closed 형태 | caps | 견인 |
|---|---|---|---|---|
| **money** | 산술=동일 통화 | Result<Money> Err | 없음 | 높음(thesis) |
| **ledger** | 차변합=대변합(복식) | Result Err(불균형 거절) | 없음 | 높음(thesis) |
| **obligation** | 상태 전이 유효(Pending→Authorized→Captured, docs/12) | 무효 전이=패닉/Result | 없음 | 높음(thesis) |
| **datetime** | month/day 범위, 윤년 | Result Err | 없음 | 중간 |
| **timer** | 단조 시각, 음수 duration 금지 | Result Err | **Now → clock** | 중간 |
| **page** | 렌더 트리 well-formed(닫힘 태그) | Result Err | 없음(순수 렌더) | ★최우선(콘텐츠) |
| **spray** | 좌표/색 범위, 레이어 순서 | Result Err | 없음 | ★최우선 |
| **storage** | 키 유효, 경로 sandbox | Result Err | **ReadFile/WriteFile → io_read/io_write** | 중간 |
| **http** | URL 형태, 메서드 유효 | Result Err | **NETWORK(신규 cap)** | 낮음(WASM 후) |
| **versioning** | semver 순서 전순서 | Result Err | 없음 | 낮음 |
| **device_adapter** | 디바이스 계약 형태 | Result Err | **Input/Render → media caps** | 낮음 |

**주의:** caps 있는 모듈(timer/storage/http/device_adapter)은 doctrine-pass 시
`with caps` 선언 + G2 무장 + capability showcase 문서화 동반(docs/15). http의
NETWORK cap은 **신규 capability 축**(현재 file/clock/random/env/input/media만) —
표면 결정(BDFL) 필요, WASM/실미디어와 같은 계열로 후순위 정당.

---

## 6. 교차 의존 (착수 전 확인)

- **per-type(G1) 해금과의 관계:** domain 모듈은 대개 concrete class라 G1 자동
  충족(§2-3). 단 콤비네이터형(map/andThen 류)을 stdlib에 넣으려면 docs/141
  Stage B + F1(closure callable 반쪽)이 선행(docs/148 §5). money/datetime/page는
  콤비네이터 아니라 무관.
- **generic G-rung(docs/151):** stdlib에 `<T>` 재도입은 G-rung이 param까지
  열린 뒤(현재 G-2 C-측 landed). 그전까진 per-type 유지 — 이 문서 스펙 전부
  per-type이라 무관.
- **checked-arith:** money/ledger의 Int 산술 overflow는 언어가 이미 fail-closed
  (class arithmetic-overflow, redteam 메모리). 모듈은 도메인 불변식(통화/균형)만.
- **docs/158 M1 분기:** M1 선언(self-host 유지보수 모드) → 레버리지가 thesis →
  이 문서가 근시일 실행 계획의 본체가 된다. M2 완주 → 이 문서는 병행 트랙.

---

## 7. WO 등록 (보드 편입)

- **WO-L4-DATETIME** — datetime doctrine-pass(§4.2, 템플릿 검증 1회 완주).
- **WO-L4-MONEY** — money doctrine-pass(§4.1, thesis 쇼케이스 + 반례 닫기).
- **WO-L4-PAGE/SPRAY** — 던전크롤러 렌더 층(§5, 킬러 견인 최우선).
- **WO-L4-\<모듈\>** — 나머지는 던전크롤러 견인 순(§3 표), 각 §2 7항.
- **WO-CAP-NETWORK** — http용 신규 NETWORK capability 축(표면 결정 BDFL,
  WASM 계열 후순위).
- 각 WO 완료 = §2 7항 + inventory flip + fixture(정상+fail-closed) green.

## Related

docs/148(stdlib 배선 — 7계약 원본) · docs/12(domain-lifecycle doctrine —
fail-closed 근거) · docs/15(capability showcase) · docs/138(scope ledger) ·
docs/151(generic G-rung — per-type 해금) · docs/158(self-bootstrap — M1 분기가
이 트랙을 근시일 본체로) · `stdlib/option.pgy`(active per-type 선례) ·
`stdlib-inventory-test-smoke`(§2-7 집행)
