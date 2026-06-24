# Semantic Squiggle — 의미 물결선

Status: `slices 1-3 landed`. 이 문서는 **의미 축 불일치를 코드 차단 없이 에디터에 표시하는** 진단 표시 정책의 설계다. 구현은 단계적(§6).

진행: slice 1(분류 Policy `squiggle_class_classify`) + slice 2(`DIAG_ADVISORY` 제3 상태 + JSON `squiggleClass` emit) + slice 3(첫 net-new advisory: Subject 정체성 섀도잉, amber) **완료·검증**. 첫 vertical slice가 동작한다 — Subject 섀도 프로그램이 **0 error로 컴파일되면서 amber advisory를 표면화**한다. 남은 건 LSP 전송 + VS Code decoration 클라이언트(slice 4) + BLUE/erasure 배선(slice 5).

## 0. 동기 — 차단에서 인식으로

Word의 빨간 물결선이 한 일은 새 분석이 아니라 **표시 정책의 전환**이었다: 철자 검사를 "사용자가 호출하면 차단하고 보여주는 것"에서 "막지 않고 그 자리에 표시하는 것"으로 바꿨다. 검사를 *차단(blocking)*이 아니라 *인식(recognition)*으로 만든 것이다.

Pergyra의 연구 명제는 "프로그래밍이 소거하는 도메인 의미를 언어 primitive로 복원한다"이다(`project_research_program_thesis`). 그 복원의 **표면 출력(HUD)**이 의미 물결선이다. 특히 `docs/122`의 다섯 drift 차원 중 자동화 불가능하다고 명시한 **recognition**(인식) — 의미가 깨졌음을 *사람이 알아채게 하는 것* — 을 위한 직접 장치다.

원칙 한 줄: **빨강만 막는다. 나머지는 보여줄 뿐 막지 않는다.**

## 1. 핵심 통찰 — 색은 2차원의 함수다

사용자의 4색 직관은 "심각도"와 "의미 차원"을 한 축에 섞었다. 분리하면 원칙이 선다:

```
squiggle_class = f(blocks?, meaning_dimension)

           blocks?              meaning_dimension
빨강 RED   = YES (compile error) SYNTAX | TYPE
노랑 AMBER = NO  (advisory)      AXIS  (World/Zone/Role/Intent/Slot 불일치)
파랑 BLUE  = NO  (advisory)      ERASURE (런타임 소거되는 의미)
보라 VIOLET= NO  (advisory, 강)  AUTHORITY (권한·월드·핀 경계)
```

- **빨강만 컴파일을 막는다.** 노랑·파랑·보라는 "코드를 막지 않고 표시"한다 → 현재 `DiagnosticLevel`(ERROR/WARNING) 아래에 **막지-않는 advisory 등급**이 새로 필요하다(§4).
- 색은 *심각도*가 아니라 *의미 차원*을 칠한다. 두 진단이 똑같이 "막지 않음"이어도 축 문제면 노랑, 권한 문제면 보라다.

## 2. 현재 기반 — 백지가 아니다

이 표시 정책에 필요한 분석·배관은 대부분 이미 있다. 새 분석기를 만들지 않는다(C 분석기가 SoT, 에디터는 thin client — drift-detector 원칙).

| 조각 | 현재 상태 | 위치 |
| --- | --- | --- |
| LSP 서버(hover/진단/protocol) | 있음 | `src/lsp/` (pgy_lsp.c, pgy_lsp_diagnostics.c, pgy_lsp_protocol.c, pgy_lsp_features.c, pgy_lsp_hover.c) |
| 진단 JSON emit | 있음(`{"severity":...}`) | `src/semantic/semantic.c` |
| 안정 코드(routing) | 있음(`Diagnostic.code`, `PGY_CODE_SEM_*`) | `src/semantic/diagnostic_types.h` |
| **의미 차원 분류** | 있음(`DiagnosticLayer`) | `src/common/diagnostic_layer.h` |
| advisory(막지 않는) 등급 | **없음** | — |
| `squiggle_class` 필드 | **없음** | — |
| 파랑(erasure) 데이터 | 측정만 있음, 라이브 진단 미연결 | `docs/14` AIR 소거 계기판 |

`DiagnosticLayer` = `{UNKNOWN, SYNTAX, TYPE, RESOURCE, CONCURRENCY, DOMAIN, BACKEND, DRIVER}` 가 심각도와 직교하는 *의미 차원 축*을 이미 제공한다. squiggle_class는 본질적으로 `(level, layer, code)`에 대한 분류 함수다.

## 3. 매핑 — `(level, layer, code) → squiggle_class`

실제 코드값 기준. layer만으로 안 갈리는 곳(특히 `DOMAIN`이 노랑·보라 둘 다 포함)은 code prefix로 세분한다.

| squiggle_class | 막음 | 주 layer | 대표 code (실재) |
| --- | --- | --- | --- |
| **RED** | YES | SYNTAX, TYPE | `PGY_CODE_SEM_TYPE_MISMATCH`, `PGY_CODE_SEM_UNDEFINED_SYMBOL` |
| **AMBER** (축) | NO | DOMAIN, RESOURCE | `INTENT_BOUNDARY_DRIFT`, `INTENT_STEP_INVALID`, `ROLE_CONTRACT_INVALID`, `INTENT_BOUNDARY_EVIDENCE_MISSING` |
| **VIOLET** (권한) | NO | DOMAIN, CONCURRENCY | `WORLD_CONTRACT_INVALID`, `ZONE_CONTRACT_INVALID`, `VISIBILITY_BOUNDARY`, `EFFECT_CONFLICT`, `PIN_ESCAPE`, `PIN_PARALLEL_CONFLICT`, `PIN_AWAIT_BOUNDARY`, `PIN_TOKEN_INVALID` |
| **BLUE** (소거) | NO | (AIR 파생) | erasure 사이트 — `docs/14` 데이터 필요(§6.2) |

> 매핑은 **데이터(테이블)**로 둔다. ad-hoc `if`가 아니라 `code → squiggle_class` 룩업 테이블 1곳에 모은다(Policy 중심, CLAUDE.md §5). 코드명이 바뀌면 테이블만 갱신.

분류 우선순위: `RED`(막는 것)이 항상 최상위 — 같은 사이트에 type 오류와 축 경고가 동시면 빨강이 이긴다(fail-closed: 막아야 할 건 먼저 막는다).

## 4. advisory 등급 (막지 않는 진단)

현재 `DiagnosticLevel = {DIAG_ERROR, DIAG_WARNING}`. 노랑/파랑/보라는 컴파일을 막지 않아야 하므로:

```c
typedef enum {
    DIAG_ERROR,     /* 막음 — RED */
    DIAG_WARNING,   /* 기존 경고 */
    DIAG_ADVISORY   /* 신규 — 막지 않음, squiggle 전용 */
} DiagnosticLevel;
```

- `DIAG_ADVISORY`는 `error_count`를 올리지 않는다(컴파일 통과). 별도 `advisory_count`.
- `Diagnostic`에 `squiggle_class` 필드 추가(또는 `(level,layer,code)`에서 파생). 명시 필드를 권장 — 미래에 매핑이 layer로 안 끝나는 경우(BLUE) 대비.

## 5. 정직한 제약 (capability guard — vision임)

이 기능은 **비전**이다. 현재 capability로 광고하지 않는다.

### 5.1 VS Code에서 "보라색"은 표준 LSP로 안 나온다
표준 LSP `DiagnosticSeverity`는 `Error/Warning/Information/Hint` 4개뿐이고, 색은 **에디터가** 정한다(보통 빨강/주황/파랑/흐림). 빨강·노랑·파랑은 severity로 매핑되지만 **진짜 보라는 클라이언트 측 decoration provider**가 진단 `data.squiggleClass`를 읽어 직접 그려야 한다. "순수 LSP로 4색"은 과장.

### 5.2 파랑(erasure)이 유일한 진짜 R&D
나머지 3색은 *오늘* 있는 진단 데이터로 된다. 파랑만 "이 식의 의미가 런타임에 소거되는가"를 편집 시점에 알아야 하는데, 그 데이터는 `docs/14`(AIR 소거 계기판)가 **측정은 하지만 라이브 진단 경로엔 미연결**이다. 이게 가장 늦고 가장 차별적인 조각이다 — 소거를 *측정*하는 언어만 보여줄 수 있다. 미연결 상태를 숨기지 않는다.

## 6. 단계 계획

비율: **doc/policy가 80%, 확장이 20%.** 어려운 건 VS Code가 아니라 매핑 원칙이다.

1. ✅ **이 문서(§3 매핑 테이블) 확정** — 의미론 결정. `docs/42`(축 소유)·`docs/14`(소거)·`docs/122`(drift) 옆에 앉음.
   ✅ slice 1: `squiggle_class_classify()` 순수 Policy + 단위 테스트(`src/common/squiggle_class.*`).
2. ✅ slice 2: `DiagnosticLevel`에 `DIAG_ADVISORY`(non-blocking, `advisory_count` 분리) + JSON emit에 `squiggleClass` 필드. 첫 advisory 생산자가 없으면 amber/violet은 안 뜨므로, 같은 슬라이스에서 첫 생산자를 추가한다(↓).
3. ✅ slice 3: 첫 net-new advisory = **Subject 정체성 섀도잉**(`type_checker_ownership_let.c`, 코드 `PGY_SEM_SUBJECT_IDENTITY_SHADOWED` → amber). 기존 에러 0개 강등, 보장 불변. 단위 테스트가 "0 error로 컴파일 + advisory≥1" 검증.
4. ⏳ `src/lsp` 진단 payload에 `data.squiggleClass` 실어 LSP로 송출 + thin VS Code decoration 클라이언트(4색, 보라는 decoration). **주의: pgy 드라이버의 `--error-format=json`은 에러가 있을 때만 JSON을 출력**(advisory-only 성공 컴파일은 무출력). LSP 경로는 항상 진단을 push해야 하므로 이 분기를 우회해 성공 시에도 advisory를 전송해야 한다.
5. ⏳ (R&D) BLUE: AIR 소거 사이트를 라이브 진단에 연결.

## 7. 게이트 (회귀 방지)

- **Golden squiggle-class 스냅샷**: 픽스처 프로그램 → 진단 JSON의 `(line, squiggle_class)` 목록을 golden 비교. 코드→색 매핑이 조용히 바뀌면 게이트가 잡는다(이벤트명/코드 회귀에 취약하므로 — CLAUDE.md §11.2).
- 매핑 테이블은 순수 함수 → 단위 테스트 100%(Policy 테스트).

## 8. 관련 문서
- `docs/42` 직교성/축 소유 — AMBER의 "축"이 무엇인지의 canonical 정의
- `docs/14` AIR 소거 계기판 — BLUE의 데이터 출처
- `docs/122` drift 관리 — recognition 차원, 1년 freeze=recognition window
- `src/lsp/`, `src/common/diagnostic_layer.h`, `src/semantic/diagnostic_types.h` — 구현 기반
