# 의미 축(Semantic Axis) 전수 감사 + 수정 영향 분석

> 읽기 전용 분석. 축 소유권은 `src/lexer/language_keyword_registry.def`와
> `docs/42_keyword_orthogonality.md`에 있다. 이 문서는 판정·수정안이 아니라
> 감사 기록이다.

## 1. 센서스 (147개 키워드 전수)

```
EXECUTION 49 | DOMAIN 47 | TYPE_CONTRACT 19 | GENERAL 17 | RESOURCE 15
```

- 예약어 70 / 문맥 74 / 소프트 3.
- 타당한 할당: 약 132개 (~90%). 문제: 12~15개에 집중.

## 2. 감사 발견

### 2.1 확정 오배정 (3개)

| 단어 | 현재 | 수정안 | 근거 |
|---|---|---|---|
| `on` | GENERAL | **EXECUTION**(또는 DOMAIN) | intent step의 액션 호출 절(`on: hero.Guard()`) |
| `all` | GENERAL | **EXECUTION** | `join with all`의 조인 모드 — 같은 계열 `join`/`sum`/`product`/`min`/`max`는 전부 EXECUTION |
| `any` | GENERAL | **EXECUTION** | `join with any` — 위와 동일 |

### 2.2 계열 분열 (3건)

1. **제어 흐름**: `match`/`case`/`default` → GENERAL vs
   `if`/`else`/`while`/`for`/`loop`/`break`/`continue`/`return` → EXECUTION.
2. **모듈·가시성**: `export`/`import`/`use`/`namespace`/`public`/`private`/
   `extern` 7개 전부 GENERAL — 전용 축 없음.
3. **액션 계약 절**: `requires` → TYPE_CONTRACT vs `authorized`/`causes`/
   `within` → DOMAIN. (방어 가능: requires는 ability를 지칭)

### 2.3 의심 (방어 가능, 논쟁 여지 — 5건)

| 단어 | 현재 | 논점 |
|---|---|---|
| `with` | RESOURCE | effects(계약)/caps(자원)/retry(실행)/slot(자원)/값 바인딩(도메인) 다중 용법인데 한 축만 반영 |
| `where` | TYPE_CONTRACT | 제네릭 제약은 반영하나 intent step의 `where: Zone`(DOMAIN) 미반영 |
| `unsafe` | EXECUTION | 원시/자원 탈출 경계 → RESOURCE가 더 자연스러움 |
| `event` | TYPE_CONTRACT | 도메인 신호(구독/해지) → DOMAIN도 방어 가능 |
| `intent`(DOMAIN) ↔ `step`/`success`/`failure`/`priority`(EXECUTION) | — | intent 본체 요소가 상위와 다른 축 (방어 가능) |

### 2.4 메타 결론

GENERAL(17개) 내역: 정당 4(`let`/`true`/`false`/`in`) + 오배정 3(`on`/`all`/
`any`) + 제어흐름 소속이어야 할 3(`match`/`case`/`default`) + 무소속 모듈 7.
즉 **GENERAL 17개 중 13개가 "마땅한 축이 없어서" 들어와 있음** — "각 키워드는
정확히 하나의 축"이라는 직교성 주장은 GENERAL 배수구에 의해 지탱됨.

## 3. 축 소비 지도 (영향면)

축은 **선언적 메타데이터**이며, 의미 검사기가 분기하지 않는다. 소비처:

| # | 위치 | 역할 |
|---|---|---|
| 1 | `src/lexer/language_keyword_registry.def` | SoT: 행별 `PGY_KEYWORD_AXIS_*` |
| 2 | `src/lexer/lexer_keywords.h` L46-52 | 네이티브 enum `PgyLanguageKeywordAxis` (GENERAL=0 … TYPE_CONTRACT=4) + `PgyLanguageKeywordRow.axis` 필드 |
| 3 | `src/lexer/lexer_keywords.c` | .def include로 생성되는 행 테이블 (axis 포함) |
| 4 | `src/lsp/pgy_lsp_protocol.c` L78-82 | axis → 문자열 switch ("general"/"resource"/…) |
| 5 | `scripts/render_language_keyword_registry.py` L41-46 | `AXIS_VALUES` dict → 자체호스팅 투영(axis int + axis_name) 생성 |
| 6 | `src/self_hosted/lexer/language_keyword_registry_projection_owner.pgy` | 생성된 투영 (validator: axis < 0 거부) |
| 7 | `src/self_hosted/lsp/completion_owner.pgy` | axis_name을 완성 라벨에 사용 |

> `PGY_CALLABLE_CONTRACT_AXIS_*`(CAPABILITY/EFFECT)는 **별개의 축 시스템**
> (callable contract vocabulary)이므로 무관하다.

## 4. 수정 영향 분석

### 4.1 `on`/`all`/`any` 재할당 (GENERAL → EXECUTION)

| 단계 | 영향 |
|---|---|
| .def 3행 수정 | O(3) |
| enum 변경 | **불필요** (EXECUTION 기존재) |
| LSP switch | **불필요** ("execution" 기존재) |
| 렌더러 AXIS_VALUES | **불필요** |
| 투영 재생성 | `render_language_keyword_registry.py --write` 1회 |
| 자체호스팅 소비자 | 코드 변경 불필요 (axis_name 동적 소비) |

**리스크: 낮음.** 순수 선언적. 게이트: 렌더러 `--check`(투영 드리프트) + 키워드
레지스트리 스모크.

### 4.2 MODULE 축 신설 (7개 단어 수용)

| 단계 | 영향 |
|---|---|
| .def 7행 재분류 (`export`/`import`/`use`/`namespace`/`public`/`private`/`extern`) | O(7) |
| `lexer_keywords.h` enum에 `PGY_KEYWORD_AXIS_MODULE`(값 5) 추가 | **필수** (.def include가 컴파일되려면 enum 먼저) |
| `pgy_lsp_protocol.c` switch에 `"module"` case 추가 | 필수 |
| 렌더러 `AXIS_VALUES`에 `(5, "module")` 추가 | 필수 — **C enum과 숫자 동기화 필수** |
| 투영 재생성 | `--write` 1회 |
| 자체호스팅 소비자 | 코드 변경 불필요 (validator는 axis ≥ 0만 검사) |
| 문서 | `docs/42_keyword_orthogonality.md`·`156_axis_composition_safety.md`·`151_generic_axis_composition.md`의 축 목록 5→6 갱신 |
| 축 센서스를 고정하는 게이트가 있는지 확인 | 필요 (고정 카운트가 있으면 갱신) |

**리스크: 중간-낮음.** 신규 enum 값은 additive이지만, **C enum ↔ 렌더러 dict의
숫자 동기화**가 단일 실패점. 축 수가 5→6으로 바뀌므로 "5축"을 언급하는
문서/게이트를 함께 갱신해야 함.

### 4.3 선택지

- **A (최소)**: `on`/`all`/`any` 재할당만. (계열 분열은 남김)
- **B (권장)**: A + `match`/`case`/`default`를 EXECUTION으로 통일.
- **C (완전)**: A + B + MODULE 축 신설(7개 수용). 이러면 GENERAL은
  `let`/`true`/`false`/`in` 4개만 남아 "진짜 일반"이 됨.

## 5. 판정

- 축 모델 자체는 건전하고, ~90% 할당이 타당.
- 문제는 `on`/`all`/`any` 3개 오배정 + 2건 계열 분열 + 7개 무소속 모듈 단어.
- 최대 임팩트 수정은 **C(완전)**: 3개 재할당 + 3개 제어흐름 통일 + MODULE 축
  신설로 GENERAL을 4개로 축소하면, "정확히 하나의 축" 주장이 실제로 성립한다.
- 총 변경: .def 13행 + enum 1줄 + LSP 1 case + 렌더러 1줄 + 투영 재생성 +
  문서 3곳. 리스크는 낮고, 전부 선언적.
