# 164. 런칭 준비 — 지뢰 목록 + 사이트 페이지 인벤토리

Status: `pre-launch-checklist`. 작성 2026-07-05. 계기: 다른 신생 언어 `hica`의
Show HN 스레드(2026-07) 관찰 — 남의 런칭에서 실제로 터진 문제와 실제로 나온
질문을 우리 지뢰·페이지 목록으로 선반영. 상위: memory `project_official_homepage`
(site/ = Pergyra가 생성, webtoon풍, Gyri 마스코트, `site-generator` CI 게이트),
`project_killer_usecase_dungeon_crawler`, `project_capability_sandbox_vision`,
`feedback_capability_overclaim_audit`, `feedback_marketing_language_drift`.

---

## 0. 한 장 요약

- **지뢰(방어)**: hica는 `<meta viewport>` 표준 헤더가 **있는데도** 모바일 Safari에서
  "확대"되어 깨졌다(스레드에서 lioeters 확인). 해결은 `-webkit-text-size-adjust`.
  우리 사이트는 **Pergyra가 생성**하므로 이런 CSS 위생이 손편집이 아니라 **생성기
  요구사항**이 된다 → §1, §4.
- **페이지(공격)**: 스레드에서 나온 질문 = 방문자가 실제로 원하는 것 = 우리가 미리
  만들 페이지. 6개 질문이 곧 6개 페이지 축 → §2.
- **최대 오버클레임 함정**: hica는 **runnable examples**(작동 플레이그라운드)가 있다.
  우리는 **WASM 부재로 in-browser 실행이 아직 없다.** 사이트는 Pergyra가 *생성*하지
  브라우저에서 *실행*하는 게 아니다. 예제는 사전 생성 출력/다운로드지 라이브 REPL이
  아니다 — 이걸 흐리면 거짓말이 된다(memory 가드).
- **현재 site/**: 단일 페이지(`pergyra_home.pgy` → `dist/index.html`). 아래 인벤토리는
  대부분 미착수 = 런칭 전 생성기 작업 목록.

---

## 1. 런칭 지뢰 (방어 체크리스트)

hica 스레드 + 일반 웹 위생. **RED = 반드시, YEL = 권장.**

| # | 지뢰 | 근거 | 조치 | 우선 |
|---|---|---|---|---|
| M1 | 모바일 Safari 확대/텍스트 리사이즈 | viewport meta 있어도 깨짐(스레드 실측) | 생성기가 `-webkit-text-size-adjust: 100%` + `text-size-adjust: 100%` 방출 | RED |
| M2 | viewport meta 자체 누락/오설정 | 표준 = `width=device-width, initial-scale=1` | 생성기 `<head>` 템플릿에 고정 | RED |
| M3 | 실기기 미검증 | 데스크톱만 보고 배포 | 실 iOS Safari + Android Chrome 스모크(뷰포트 320/375/414) | RED |
| M4 | 가로 스크롤 누출 | 넓은 코드블록/표 | 코드블록 `overflow-x:auto` 컨테이너, body 가로스크롤 0 | RED |
| M5 | 폰트/이미지 외부 호스트 의존 | CSP/오프라인/속도 | self-contained(인라인 or 동봉), 외부 CDN 0 | YEL |
| M6 | 다크모드 대비 깨짐 | `prefers-color-scheme` | 앰버/인디고 팔레트 양 모드 검증(Gyri 브랜드색) | YEL |
| M7 | 접근성(대비/포커스/alt) | 스크린리더·키보드 | Gyri 애니에 `prefers-reduced-motion` 존중, alt 텍스트 | YEL |
| M8 | 첫 로드 무거움 | 순차 애니 JS | 애니 없이도 콘텐츠 읽힘(progressive), no-JS fallback | YEL |

**핵심:** M1~M4는 hica가 표준 헤더로도 못 막은 종류다. 우리는 생성기라 한 번
고치면 전 페이지 적용 = 이점. 대신 **생성기 골든/스모크에 뷰포트 회귀 게이트**를
넣어야 회귀가 안 샌다(§4).

---

## 2. 페이지 인벤토리 (질문 → 페이지 → 자산 → 오버클레임 가드)

스레드에서 실제로 나온 질문을 축으로. 각 페이지는 **capability-tag**를 달아 현재
사실만 capability로, 나머지는 vision으로 명시(3-pair 프로토콜, memory).

### P1. 포지셔닝 — "이건 뭐고 누구를 위한 건가"
- **신호**: mogoh "FP의 파이썬을 노리나?", jdw64 "C#처럼 느껴져 배우기 쉬울 듯".
- **우리 답**: 도메인 확장을 가진 **시스템 언어**(memory `systems_language_identity`),
  계보 = C# 아버지(memory `lineage_synthesis`), thesis = 잃어버린 도메인 의미 복원.
- **가드**: "파이썬/러스트-대체" 프레임 금지. multi-paradigm/AI-first는 **vision** 라벨.
  hica의 `hica-vs-python` 같은 비교는 P6에서.

### P2. 동시성/병렬 — "concurrency/parallelism 있나"
- **신호**: smw "Any concurrency / parallelism?"
- **우리 답**: **강점.** SEA 실행 lane(증거-기반 lane, memory `sea_execution_lanes`) +
  channel. 도메인 primitive가 병렬을 표현.
- **가드**: "자동 병렬화"/"M:N"/분산 프레임 금지 — lane은 **증거로 결정**, 분산/양자는
  vision. channel-only 크로스-World는 사실(capability).

### P3. 실전 예제/쇼케이스 — "큰/실용 프로젝트 있나"
- **신호**: alfanick "bigger and practical projects?" (hica는 tbdflow-ui/HML/lisp 제시).
- **우리 답**: **던전 크롤러**(killer usecase, memory) = 도메인 primitive ↔ 게임 1:1.
  + 컴파일러 자신(self-host, baseline 증명).
- **가드**: ★**in-browser 실행 없음.** 예제는 사전 생성된 출력/스크린샷/다운로드지
  라이브 플레이가 아니다. "Pergyra로 작성" = 생성이지 브라우저 실행 아님. WASM은
  vision(docs/161). 여기서 흐리면 즉시 거짓말.

### P4. 이름/발음 + 마스코트
- **신호**: nyankosensei "How do you pronounce the name?" (+ Koka 기반임을 알아챔).
- **우리 답**: Pergyra 발음 표기 + 마스코트 **Gyri(자이리)**, 앵무조개, 앰버/인디고
  (memory `mascot_gyri`).
- **가드**: 브랜드 페이지, 가벼움. 오버클레임 여지 낮음.

### P5. 계보/영감 — "무엇에서 왔나"
- **신호**: nyankosensei가 Koka 기반·Shen 유사성 언급 → 방문자는 계보를 본다.
- **우리 답**: C# 아버지 + Koka/Vale/Erlang/OCaml/MLIR substrate borrow(memory
  `lineage_synthesis`). DDD primitive = 고유 synthesis.
- **가드**: 이론 대응(ambient/effects/ocap/affine/session, memory
  `theoretical_foundations`)은 **lineage**로, "증명된 언어"라 말하지 말 것.

### P6. 비교 페이지 (X-vs-Pergyra)
- **신호**: hica는 `hica-vs-python`을 따로 뒀다 = 방문자가 원함.
- **우리 답**: vs C#(아버지, 친함), vs Python(접근성). 필요시 vs Rust.
- **가드**: ★vs Rust는 **negative-space 표**(docs/118 §8)로만 — "Rust-equivalent 안전"
  절대 금지(memory `marketing_language_drift`). 우리 정적 강도를 borrow-checker로
  표현 금지. 못 하는 걸 정직히 적는 칸이 있어야 신뢰.

### P7. 성능 + 에러 메시지 품질
- **신호**: esafak "compile times, executable speed, error message 얼마나 유익한가?"
- **우리 답**: perf workstream(문자열 무할당 fused builtin, StrView; memory
  `string_alloc_perf_workstream`), 구조적 진단 + **semantic squiggle**(memory,
  advisory 물결선). 이중 백엔드(LLVM perf / C compat).
- **가드**: 벤치는 **실측만.** "idiomatic Rust 대비 유일 구조적 갭 = 문자열 할당"
  같은 정직한 수치가 오히려 신뢰. 컴파일타임/실행속도 수치는 재현 명령과 함께.

### P8. 초보 가이드 (learn-by-building)
- **신호**: hica `/docs/hica-for-beginners/` = 함수·패턴매칭·리스트를 실제 프로그램으로.
- **우리 답**: 던전 크롤러 소재로 점증 튜토리얼(primitive를 게임으로 체감).
- **가드**: "runnable" 표기 시 P3 가드 동일 — 실행 아님, 생성/다운로드.

### P9. 패러다임/개념 가이드
- **신호**: hica는 "함수형 가이드(불변성/고차/파이프라인)"를 런타임 예제로.
- **우리 답**: 우리 축(zone/world/intent/slot/capability) 개념 가이드 + 구두점=register
  원칙(memory `punctuation_register_principle`) 같은 고유 문법 설명.
- **가드**: 개념은 **vocabulary**로 명확히. "이미 다 된다"가 아니라 무엇이 구현/vision인지 표.

### P10. 블로그/개발로그 + GitHub 존재감
- **신호**: hica 저자는 개인 블로그(dev log) + GitHub 리포(hml, hica-lisp) 연결.
- **우리 답**: 개발로그(설계 결정 서사 — docs/*가 원천), README(예제·설치·빠른시작).
- **가드**: README의 capability 섹션은 3-pair 태그 필수. "산업/AI-first"는 vision 섹션.

---

## 3. 현재 site/ vs 필요 (gap)

- **있음**: `pergyra_home.pgy`(생성기) → `dist/index.html` 단일 랜딩. `build.sh`,
  `site-generator` CI 게이트.
- **없음(=작업)**: P1~P10 대부분. 랜딩(P1 일부)만 존재로 추정. 각 페이지는
  **생성기 입력(.pgy)**을 늘리는 방식 — 손 HTML 아님(dogfood 유지).
- **순서 제안**: P1(포지셔닝)·P4(이름)·P5(계보) = 저비용 정체성 → P2/P3/P7 = 강점·증거
  → P6 비교 → P8/P9 가이드 → P10 지속. 지뢰 M1~M4는 **P1 전에** 생성기 `<head>`에 박기.

---

## 4. "생성된 사이트" 특수 규율 (지뢰 → 생성기 요구사항)

우리 사이트는 Pergyra가 만든다 → 위 지뢰가 손편집이 아니라 **생성기 계약**이 된다:

1. `<head>` 템플릿을 생성기에 1곳으로 고정: viewport(M2) + `text-size-adjust`(M1) +
   문자셋 + 다크모드 메타. 전 페이지 자동 적용.
2. **생성기 골든에 뷰포트/헤더 회귀 게이트** 추가(`site-generator` 게이트 확장):
   생성 HTML에 필수 `<head>` 라인이 없으면 실패 → M1/M2 회귀 차단.
3. 코드블록 렌더는 `overflow-x:auto` 컨테이너로(M4) — 생성기의 코드 렌더 경로에 고정.
4. ★**정직 게이트**: 생성기가 "run in browser"/"live"/"playground" 문자열을 내면
   실패(WASM 없기 때문). P3/P8 가드의 기계화. capability 문장은 vision 라벨 없이는
   특정 어휘(AI-first/quantum/distributed/industrial/Rust-equivalent) 금지 — 생성기
   린트로. (memory `capability_overclaim_audit`의 사이트 판.)

---

## 5. 런칭 당일 운영 (스레드가 보여준 것)

- hica 저자는 **거의 모든 댓글에 응답**(모바일 버그 즉시 패치 포함). 런칭은 게시가
  아니라 **실시간 응답**이 일 — Q&A 준비(P1~P7이 곧 예상 질문지).
- 모바일 버그가 첫 인상을 깎았다(호의적 방문자조차 "쓰기 어렵다") → M1~M4를
  **게시 전** 실기기로 닫는 게 최우선.
- "tasteful/looks good"(xixixao) 같은 미학 칭찬도 나옴 → webtoon풍 + Gyri는 차별화
  자산. 단 M7(reduced-motion) 존중해야 애니가 접근성 부채가 안 됨.

---

## Related

memory `project_official_homepage`(site/ = 생성, 오버클레임 금지) ·
`project_killer_usecase_dungeon_crawler`(P3 자산) ·
`project_capability_sandbox_vision`("안전한 new Flash" — 단 in-browser 실행은 WASM
이후 vision) · `feedback_capability_overclaim_audit`(3-pair, 전 페이지 게이트) ·
`feedback_marketing_language_drift`(P6 negative-space) · `lineage_synthesis`(P1/P5) ·
`sea_execution_lanes`(P2) · `string_alloc_perf_workstream`+`semantic_squiggle`(P7) ·
docs/161(WASM/미디어 — in-browser 실행의 미래 경로) · docs/118 §8(negative-space 표).
