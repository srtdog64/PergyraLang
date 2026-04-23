# GitHub Linguist 등록 준비 체크리스트

GitHub repository 의 **Languages** 위젯에 `Pergyra` 를 표시하기 위한 운영 문서.

## 목표

[github-linguist/linguist](https://github.com/github-linguist/linguist) 에 Pergyra 를 공식 등록시켜, 이 repo 및 `.pgy` 를 사용하는 다른 공개 repo 에서 고유 색상/이름으로 분류되게 한다.

## 왜 이 문서가 필요한가

- Linguist 에 없는 언어는 GitHub UI 에 **이름이 나오지 않는다**.
- 로컬의 어떤 설정(`.gitattributes` 포함)도 Linguist 미등록 언어의 이름을 만들어내지 못한다.
- 따라서 등록은 "준비 — 제출 — 심사 — merge" 의 **외부 의존 프로세스**이며, 이 문서는 그 프로세스의 내부 자료를 고정해두는 용도다.

---

## 현 자산 인벤토리

| 항목 | 상태 | 경로 |
|---|---|---|
| 파일 확장자 | `.pgy` (고유, 충돌 없음) | 315 파일 |
| TextMate grammar | 완성 | [editor/vscode-pergyra/syntaxes/pergyra.tmLanguage.json](../editor/vscode-pergyra/syntaxes/pergyra.tmLanguage.json) |
| TextMate scope | `source.pergyra` | ↑ 동일 |
| VSCode extension | 완성 (marketplace 미배포) | [editor/vscode-pergyra/](../editor/vscode-pergyra/) |
| 언어 개요/문법 | README + docs/grammar | [../README.md](../README.md), [grammar/](grammar/) |
| tree-sitter grammar | 없음 | — |
| Vim/Emacs/Sublime | 없음 | — |
| `.gitattributes` | 있음 (forward-declared) | [../.gitattributes](../.gitattributes) |

---

## Linguist 제출 시 필요한 산출물

### 1. `lib/linguist/languages.yml` 항목 (제출 시점에 Linguist fork 에 추가)

```yaml
Pergyra:
  type: programming
  color: "#<Section 2 에서 선택된 hex>"
  extensions:
  - ".pgy"
  tm_scope: source.pergyra
  ace_mode: text
  language_id: <Linguist bot 이 자동 부여>
```

**주의**:
- `tm_scope` 는 반드시 grammar 의 최상위 `scopeName` 과 일치해야 한다. 현재 grammar 의 `scopeName` 은 `source.pergyra` 이므로 그대로 사용.
- `language_id` 는 직접 정하지 않는다 — Linguist 팀이 PR 머지 시점에 할당한다.
- `ace_mode` 는 처음엔 `text` 로 제출. 추후 Ace editor 에 공식 Pergyra 모드가 생기면 바꾼다.

### 2. 샘플 소스 6개 — `samples/Pergyra/` (Linguist repo 안)

Linguist 는 heuristic 학습과 classifier 검증을 위해 언어당 **6개 이상의 대표 소스**를 요구한다. 문법 다양성을 고르게 커버하는 후보:

| 순위 | 이 repo 경로 | 커버 영역 |
|---|---|---|
| 1 | [examples/hello.pgy](../examples/hello.pgy) | 기초 선언 / 출력 |
| 2 | [examples/fizzbuzz.pgy](../examples/fizzbuzz.pgy) | 제어 흐름 (if/for/match) |
| 3 | [examples/class_test.pgy](../examples/class_test.pgy) | subject/class 선언 |
| 4 | [examples/battle_simulator/fighters.pgy](../examples/battle_simulator/fighters.pgy) | vessel / ability / role 도메인 모델 |
| 5 | [examples/logistics_intent_probe/](../examples/logistics_intent_probe/) 대표 1개 | intent / relation / effect |
| 6 | [stdlib/http.pgy](../stdlib/http.pgy) | 표준 라이브러리 / generic 사용 |

**제출 시 처리**:
- 위 파일들을 Linguist fork 의 `samples/Pergyra/` 로 그대로 복사 (리네이밍 금지, 문법 손상 금지).
- 파일당 **최소 수십 줄**은 되어야 classifier 가 유의미한 특징을 학습한다 — 너무 짧으면 컷.
- 이 repo 원본은 건드리지 않는다.

### 3. TextMate grammar — 공개 stand-alone repo

Linguist 는 grammar 를 `vendor/grammars/` 아래 **git submodule** 로 가져온다. 따라서 현재 vscode-pergyra 에 번들된 grammar 를 독립 repo 로 분리해야 한다.

**분리 계획**:
1. 새 공개 repo 생성: 권장명 `pergyra-lang/pergyra-tmLanguage`
2. 포함 파일:
   - `pergyra.tmLanguage.json` (현재 [editor/vscode-pergyra/syntaxes/pergyra.tmLanguage.json](../editor/vscode-pergyra/syntaxes/pergyra.tmLanguage.json) 복사본, 단일 정본)
   - `LICENSE` — MIT 권장 (Linguist 가 받아들이기 쉬움)
   - `README.md` — grammar 사용법 + Pergyra repo 링크
3. 이 repo 의 grammar 는 **mirror** 로 재정의 — 이후 CI 로 sync
4. Linguist fork 에서:
   ```bash
   git submodule add https://github.com/pergyra-lang/pergyra-tmLanguage.git vendor/grammars/pergyra-tmLanguage
   script/grammar-compiler add vendor/grammars/pergyra-tmLanguage
   ```
5. `vendor/licenses/grammar/pergyra-tmLanguage.txt` 에 MIT LICENSE 본문 복사

---

## 색상 — 확정: `#DD891D` (amber primary)

Linguist 는 **단일 hex 만** 받는다 (gradient/multi-color 불가).
Pergyra 로고는 **indigo 배경 + amber 캐릭터(링/"P"/Gyri)** 2색 팔레트라서
primary 는 amber 쪽으로 고정 — 로고의 "캐릭터 역할" 색이자 GitHub Language
bar 의 작은 원형 dot 에서 가독성이 높다.

확정값: **`#DD891D`** — 로고 외곽 링 및 "P" 글자 톤.

출처: [../editor/vscode-pergyra/icons/pergyra-logo.png](../editor/vscode-pergyra/icons/pergyra-logo.png),
[pgy-icon-dark.png](../editor/vscode-pergyra/icons/pgy-icon-dark.png),
[pgy-icon-light.png](../editor/vscode-pergyra/icons/pgy-icon-light.png).

### 현 Linguist 등록 언어와의 충돌 분석

`github-linguist/linguist` main branch 의 `lib/linguist/languages.yml`
스캔 결과 (검사 시점 기록: 2026-04-24):

| 등급 | 언어 | Hex | 델타 | 비고 |
|---|---|---|---|---|
| 정확 일치 | — | — | — | **없음** ✓ |
| 매우 근접 | Clarion | `#DB901E` | R+2, G-7, B-1 | obscure 4GL. 리뷰어가 구분성 지적할 수 있음 |
| 중간 근접 | Faust | `#C37240` | 톤 차이 큼 | 안전 |
| 멀음 | Modelica `#DE1D31`, Genero per `#D8DF39`, Max `#C4A79C` | — | — | 안전 |

**판단**: Clarion 과의 근접성은 존재하지만 정확 일치가 아니므로
1순위 `#DD891D` 그대로 제출한다. Linguist 팀이 "Clarion 과 구별이
어렵다" 고 지적하면 아래 회전 후보로 넘어간다.

### Rollback 회전 후보 (Clarion 지적 시)

Clarion `#DB901E` 에서 **시각적으로 분명히 다른 방향**으로 이동시킨 값:

| 순위 | Hex | 방향 | Clarion 과의 관계 |
|---|---|---|---|
| 1 fallback | `#E67E22` | 더 orange 쪽 (yellow 제거) | 확실히 구별 |
| 2 fallback | `#CC6600` | 더 진한 burnt orange | 채도 낮추고 darken |
| 3 fallback | `#D97706` | Tailwind amber-600 톤 | 살짝 어둡게 |

회전 전략: fallback 도 시각적으로 `#DB901E` 에서 명확히 구별되는
방향(더 orange / 더 dark)을 유지한다. 다시 `#DB...` 계열로 돌아가지 않는다.

### 원본 hex 정밀 추출 (선택)

현재 `#DD891D` 는 다운스케일된 PNG 렌더에서 추정한 값이라 ±3 오차 가능.
PR 제출 전 원본 PNG 를 color picker(Figma, Windows Paint, macOS Digital
Color Meter 등) 로 찍어 **정확한 링/P 톤 hex 를 확인**하고, 다르면
이 문서와 [../README.md](../README.md), [../assets/branding/](../assets/branding/)
팔레트와 삼자 동기화한다.

---

## "In use" gate — 현실 체크

Linguist 의 명시적 승인 기준 (CONTRIBUTING.md):

> The language must be **in use**. You must be able to point to 200+ unique :user/:repo pairs that use the extension for that language.

**현 상태**: 이 repo 단독 → 통과 불가.

**신호 확보 경로** (병행):
1. **VSCode marketplace 게시** — 다운로드 수는 계량 가능한 adoption 지표
2. **공개 예제 repo 분리** — `pergyra-examples` 같은 별도 조직 repo 로 예제 분리 배포
3. **소개 블로그/기고** — Hacker News, Lobsters, dev.to
4. **커뮤니티** — Discord / Matrix / Reddit r/ProgrammingLanguages
5. **언어 벤치마크 참여** — Rosetta Code, computer-language-benchmarks-game 등에 Pergyra 구현 제출

신호가 **눈에 띄게 쌓이기 전 PR 은 거의 자동 reject** 된다. 최소한 VSCode marketplace 에서 수백 단위 다운로드 + 외부 공개 repo 5개 이상에서 `.pgy` 사용이 보여야 제출 타이밍.

---

## Phase 별 단계

### Phase 1 — 로컬 자산 정리 (완료 시점: 이 문서 생성 시점)

- [x] `.gitattributes` 작성 — forward-declared linguist hint
- [x] `docs/96_linguist_submission.md` 작성 — 본 문서
- [ ] `README.md` 에 "Editor Support / Linguist status" 섹션 추가
- [ ] 색상 1순위 확정 후 브랜딩 문서에 기록

### Phase 2 — Grammar / Marketplace 분리 배포 (외부 작업)

- [ ] `pergyra-lang/pergyra-tmLanguage` 공개 repo 생성
- [ ] 현 grammar 를 해당 repo 에 복사 + LICENSE + README
- [ ] 이 repo 의 vscode-pergyra 를 marketplace 에 publish
- [ ] marketplace 확장의 Repository 필드가 `pergyra-lang/pergyra-tmLanguage` 를 참조하도록 설정

### Phase 3 — Linguist PR 제출

- [ ] `github-linguist/linguist` fork
- [ ] `lib/linguist/languages.yml` 에 Pergyra 알파벳 순 위치에 항목 추가
- [ ] `samples/Pergyra/` 에 본 문서 § 2 의 6개 파일 복사
- [ ] `vendor/grammars/` 에 pergyra-tmLanguage submodule 추가
- [ ] `vendor/licenses/grammar/pergyra-tmLanguage.txt` 에 LICENSE 사본
- [ ] `script/grammar-compiler add vendor/grammars/pergyra-tmLanguage`
- [ ] `bundle exec rake test` 회귀 통과 확인
- [ ] PR body 에 다음 포함:
  - Pergyra repo 및 문서 링크
  - VSCode marketplace 통계
  - 외부 공개 `.pgy` 사용 repos 목록
  - 언어 distinctness 요약 (intent-first, subject/world/zone, slot resource model)
  - 색상 선택 근거

### Phase 4 — 지속 adoption (수개월)

- [ ] marketplace 다운로드 월별 추적
- [ ] 외부 공개 repo `.pgy` 사용 현황 분기별 스냅샷
- [ ] Linguist PR 이 stale 되면 adoption 숫자 업데이트해서 ping

---

## 리스크 & 대응

| 리스크 | 대응 |
|---|---|
| Linguist "in use" gate 미통과 | Phase 4 지속, PR 은 타이밍 될 때까지 제출 대기 |
| 색상 충돌로 rollback 요청 | 1~3순위 후보 미리 고정, 회전 전략 문서화 |
| grammar repo 의 LICENSE 불일치 | MIT 로 고정, 복사 시점에 LICENSE 파일 최상위 배치 |
| TextMate scope 충돌 | `source.pergyra` 는 고유, 안전 |
| 제출 시점의 grammar 가 이 repo 의 정본과 drift | Phase 2 완료 후 **정본은 독립 repo** — 이 repo 의 editor/ 는 그 repo 를 참조하도록 재구성 |
| PR 리뷰 기간 중 `.pgy` 다른 언어 충돌 발생 | heuristic 추가로 disambiguate — `.pgy` 는 현재 다른 언어와 충돌하지 않음 (재확인 필요) |

---

## 참고 자료

- Linguist CONTRIBUTING: https://github.com/github-linguist/linguist/blob/main/CONTRIBUTING.md
- Linguist languages.yml schema: https://github.com/github-linguist/linguist/blob/main/docs/new-lookup-table.md
- TextMate grammar guide: https://macromates.com/manual/en/language_grammars
- 등록 선례 — Gleam PR: https://github.com/github-linguist/linguist/pulls?q=Gleam (검색 참고)
