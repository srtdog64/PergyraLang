# 만화 컷씬 (manga cutscene) — 설계 문서

상태: 설계 + 참조 프로토타입 존재. 이 문서는 Pergyra의 킬러 피쳐 후보인
"만화/웹툰 연출을 코드로 표기하고 HTML로 컴파일"의 결정 기록이다. 무엇을
만들지, 왜 Pergyra에 맞는지, 컴파일 타깃을 어떻게 잡을지, 그리고 첫 슬라이스를
핀으로 박는다.

## 무엇인가

일본만화/웹툰의 *연출(演出)* — 컷 나눔, 등장 순서, 시선 유도(視線誘導),
이펙트, SFX — 를 1급 언어 구문으로 쓰고, **클릭(또는 키)으로 한 컷씩 진행되는
HTML 페이지**로 컴파일한다. 스크롤 웹툰도 같은 모델의 한 변종이다.

핵심 통찰: 만화 연출은 **스크롤 위치 또는 클릭 횟수를 시간축으로 하는
타임라인**이다. "버튼 누르면 다음 컷"은 *이산적 결정론 상태머신*이고, 이건
정확히 Pergyra의 `intent`/step 모델이다. 즉 새 패러다임이 아니라 기존 의미론을
한 도메인에 겨눈 것이다.

## 참조 자료 (시각 레퍼런스)

이 기능의 시각 목표는 유희왕 배틀 시티의 컷 연출이다(레퍼런스 용도). 원본 캡쳐는
`examples/manga_cutscene/reference/` 에 보관하며, 아래에 임베드한다. (파일을 그
폴더에 저장하면 표시된다.)

![시선 유도 — "28:" 페이지, 핑크 화살표로 독자 시선 경로](../examples/manga_cutscene/reference/ref_gaze.png)

![액션 스플래시 — 파편 컷, 방사 스피드라인, 召雷弾!! / 神だ!!](../examples/manga_cutscene/reference/ref_action.png)

![대치 — 카이바 vs 유우기, 사다리꼴·쐐기 패널의 대각선 분할](../examples/manga_cutscene/reference/ref_faceoff.png)

이 캡쳐들에서 뽑은 디자인 어휘(= DSL이 표현해야 할 것):

| 캡쳐가 보여주는 것 | DSL 기능 |
|---|---|
| 사다리꼴·쐐기·평행사변형·파편 패널 (그리드 아님) | `shape: trap/wedge/shard/polygon(...)` → clip-path |
| 페이지를 대각선으로 가르는 컷, 기울어진 패널 | clip-path polygon + `transform`(rotate/skew) |
| 핑크 화살표 = 독자 시선 경로(視線誘導) | `gaze A -> B` (1급 구문) |
| 모서리에 비스듬히 박힌 큰 SFX (召雷弾!!/神だ!!) | `sfx "…" at edge, angle: …` |
| 카드 드로우 광휘, 방사 스피드라인 | `tone: speedlines`, `enter: flash` |
| 화살표가 가리키는 읽기 순서 | cut 선언 순서 = beat = 노출 순서 |
| 컷 밖으로 튀어나오는 캐릭터/효과 | 패널 overflow + z-order 레이어 |

즉 레퍼런스의 *모양·시선·SFX·읽기순서* 가 각각 `shape`·`gaze`·`sfx`·`cut 순서`로
1:1 매핑된다. 캔버스 에디터의 프리셋(사각형/사다리꼴/쐐기/파편)도 이 캡쳐에서
직접 나온 것이다.

## 왜 Pergyra에 맞는가

- `scene` = `intent`형 순서 있는 시퀀스. `cut`/`gaze`/`sfx` = 정렬된 beat(step).
  클릭 = step advance. Pergyra의 결정론적 조정이 그대로 연출 페이싱이 된다.
- `gaze A -> B`(시선 유도)를 1급 문법으로. 만화 연출의 핵심인데 어떤 도구도
  이걸 코드화하지 않았다 — 차별점.
- `reflect`로 scene의 cut들을 순회 → HTML/CSS(/JS)를 생성. "언어를 뱉는 언어"
  (MPaC)가 사람이 즉시 이해하는 도메인에서 구체화된다.
- 한국의 거대한 웹툰 문화가 가치를 직관적으로 이해하는 청중이다.

## 컴파일 타깃 결정 (핵심)

참조 프로토타입(`examples/manga_cutscene/`)은 지금 HTML + CSS + JS 세 층이다:
구조는 HTML, *보이는 것 전부*(clip-path 패널, 스피드라인, 등장 transition,
SFX 타이포, 흔들림/플래시 keyframe)는 CSS, *진행 상태(현재 beat, 다음으로)*는
JS가 한다.

그러나 진행 자체는 **순수 HTML/CSS로 가능**하다:

- `:target` — 다음 컷의 id를 가리키는 앵커를 클릭하면 URL 해시가 바뀌고
  CSS `:target`이 해당 컷을 노출. 이전 컷 유지에는 `~` 형제 결합자를 쓴다.
- `:checked` — 라벨/체크박스 토글로 단계 진행.
- shake/flash/등장 = `:target` + `animation`으로 표현 가능.
- 세로 웹툰 스크롤은 CSS scroll-driven animation으로도 가능.

따라서 코드젠 정책 결정:

> **기본 타깃은 순수 정적 HTML/CSS (런타임 0, 완전 inert, JS 꺼진 환경·메일·
> 임베드에서도 동작).** JS는 *선택적 capability* 로, auto-advance, 키보드
> 내비게이션, 복잡한 시선 애니메이션, 사운드 같은 고급 연출에서만 방출한다.

이 결정은 Pergyra의 컴파일타임·결정론·no-runtime 철학과 일치하며, 그 자체가
세일즈 포인트다 — "런타임 없는 인터랙티브 만화."

연출의 *어떤* 기능이 JS를 요구하는지는 effect로 모델링한다: `advance: click`은
순수 CSS로 충분하고, `advance: auto(0.8s)`나 `sfx … on key`는 `js` effect를
방출한다. effect 시스템이 "이 scene이 JS 런타임을 요구하는가"를 컴파일타임에
알려준다.

## 동적 연출 = Pergyra-WASM 디렉터

동적 연출(shake/flash 타이밍, auto-advance, 키보드, 사운드)이 필요한 scene은
손코딩 JS가 아니라 **Pergyra를 WASM으로 컴파일해 구동**한다. 이게 가장
on-thesis하다 — "언제·어떤 순서로 무엇을 발동하나"는 타임라인·상태·타이밍
결정이고, 그게 곧 intent/effect 프로그램이기 때문이다.

역할 분리 (과하게 WASM화하지 않는 것이 핵심):

| 층 | 담당 |
|---|---|
| **Pergyra/WASM = 디렉터** | beat 상태, "beat N에 effect X 발동", auto-advance 타이밍, 시퀀싱 |
| **CSS = 렌더러** | shake/flash *애니메이션 자체*. Pergyra는 class만 토글 |
| **얇은 JS 호스트 = 다리** | 이벤트→`advance()`, rAF→`tick(dt)`, DOM 연산 import |

WASM은 픽셀을 그리지 않는다. **타임라인을 지휘**한다. shake 한 방은 CSS로
충분하지만, *언제 터지고 다음으로 언제 넘어가나* 라는 연출의 연결조직은 Pergyra가
소유한다 — 그게 reflect 가능하고 컴파일타임 검사 가능한 진짜 가치다.

세 가지 어려운 지점과 회피:

1. **WASM은 DOM을 못 만진다.** → 고정 JS 호스트("스테이지 런타임", ~80줄,
   제네릭, 한 번 작성하면 모든 scene 재사용)가 DOM 연산을 import로 노출한다.
2. **WASM엔 타이머가 없다.** → 호스트가 매 프레임 `tick(dt_ms)`를 Pergyra
   export로 호출하고, Pergyra가 경과시간으로 advance를 결정한다(auto-advance).
3. **문자열 경계(id/class 마샬링)가 까다롭다.** → **정수 핸들로 회피.** Pergyra는
   cut/effect를 작은 int id로 참조하고 호스트는 고정 테이블을 가진다. 경계로는
   숫자만 흐르므로 메모리 마샬링이 사라진다.

호스트 import 표면 (스테이지 런타임이 노출, Pergyra가 호출):

    show(cutId: Int)            // class "show" 토글
    hide(cutId: Int)
    fire(cutId: Int, fx: Int)   // fx: SHAKE=0, FLASH=1, ... CSS animation 트리거
    scroll_to(cutId: Int)
    now() -> Int                // ms (auto-advance 계산용; tick(dt)로도 가능)

Pergyra가 export (호스트가 호출):

    advance()                   // 클릭/키 입력 시
    tick(dt_ms: Int)            // rAF 프레임마다 (auto-advance 진행)

Pergyra 디렉터 스케치 (의미상 — scene이 이 형태로 lower된다):

```pergyra
intent SceneDirector {
    let beat: Int = 0;
    let elapsed: Int = 0;

    func advance() -> Void {
        play(beat);
        beat = beat + 1;
    }
    func tick(dt: Int) -> Void {
        elapsed = elapsed + dt;
        if auto_mode && elapsed >= 800 {
            elapsed = 0;
            advance();
        }
    }
    func play(b: Int) -> Void {
        // beat 표에서 그 beat의 cut/effect를 호출
        show(cut_of(b));
        if has_fx(b) {
            fire(cut_of(b), fx_of(b));
        }
    }
}
```

`scene` 컴파일러는 cut/effect/advance 선언을 이 디렉터(beat 표 + play 디스패치)로
lower하고, WASM으로 빌드한 뒤 고정 호스트 shim과 함께 묶는다. CSS·HTML 정적
부분은 순수 타깃과 동일하게 방출한다.

타깃 분기는 effect가 정한다:

> 클릭만 쓰는 scene → 순수 정적 CSS(런타임 0). `auto`/동적/사운드 effect를 쓰는
> scene → Pergyra-WASM 디렉터 + 호스트 shim. 컴파일러가 effect로 자동 분기한다.

난이도: **중간, 선행투자형.** 디렉터 로직은 작고, 유일한 진짜 작업은 ~80줄
정수-키 호스트 shim이며, 그 뒤로는 모든 scene이 공짜다. 권장 순서는 rung 1(순수
CSS)로 출시 가능한 데모를 먼저 확보하고, rung 5에서 이 WASM 디렉터를 얹는 것 —
WASM 다리가 더 어려운 부분이라 첫 증거물을 막으면 안 된다.

## DSL 표면 (초안 — 형태는 확정 전)

```pergyra
scene "28" {
    advance: click;                  // click | key | tap | auto(0.8s) | scroll

    cut Draw {
        shape:  shard(top-left);     // 이름 붙은 clip-path polygon 프리셋
        enter:  slide(left) + scale; // transform+opacity transition 프리셋
        tone:   speedlines(-22deg);
        say:    "ドロー!!" vertical;
        caption:"神はオレの手の中に";
    }

    gaze Draw -> Summon;             // 시선 유도

    cut Summon {
        shape:  wedge(right);
        enter:  slam;
        shake:  0.22s x2;
        sfx "召雷弾!!" at right-top, angle: -8deg;
    }

    cut Awe {
        shape:  slash(bottom-left);
        enter:  flash;
        sfx "神だ!!" at left-bottom;
    }
}
```

## 구문 → 출력 매핑

| DSL | 출력 | JS 필요? |
|---|---|---|
| `scene` / `cut` 순서 | beat 인덱스(`data-beat` 또는 `:target` 체인) | 아니오 |
| `advance: click` | 앵커 + `:target` 노출 | 아니오 |
| `advance: auto(t)` / `key` | beat 컨트롤러 | 예 (`js` effect) |
| `shape: shard/wedge/slash` | `clip-path: polygon(...)` | 아니오 |
| `enter: slide/slam/flash` | transform/opacity transition + `@keyframes` | 아니오 |
| `shake`, `flash` | `:target ~ * { animation }` | 아니오 |
| `gaze A -> B` | A·B bbox로 계산한 SVG path + stroke 애니메이션 | 정적은 가능, 복잡하면 예 |
| `say` / `sfx` / `caption` | 스타일된 텍스트 오버레이(말풍선/SFX 타이포) | 아니오 |

## 모델 매핑 (의미론)

- `scene` → `intent`형 선언. 본문의 cut/gaze/sfx = ordered steps.
- `cut` → step(beat). 등장 = advance 한 번.
- `advance` → beat 머신의 입력 모드.
- `gaze` → 두 cut 사이의 시선 경로. 만화 고유 1급 구문.
- `shape`/`enter`/`tone`/`shake`/`flash` → 시각 effect. JS 요구 여부가 effect로 분류됨.
- 타입 검사: 존재하지 않는 cut을 `gaze`하거나, 정의 안 된 actor를 `say`하면
  컴파일타임 에러. (오케스트레이션 사실의 정적 검사 = Pergyra의 강점.)

## 비주얼 저작 (캔버스 에디터)

핵심 원칙: **캔버스 에디터와 PGY `scene`은 같은 것의 두 뷰다.** `scene`이
SoT(진실의 원천)이고, 공유 표현은 `polygon()` 좌표(= clip-path %)다.

- 캔버스에서 패널 꼭짓점을 드래그 → `shape: polygon(...)` 좌표 갱신 → PGY 코드
  실시간 생성. (역방향: PGY 편집 → 캔버스 렌더 = round-trip)
- 컷 추가/삭제/순서(앞으로·뒤로) = `cut` 선언 추가/삭제/재정렬. **순서 = 실행
  (클릭) 노출 순서.**
- "▶ 실행" = 현재 scene을 그 자리에서 클릭-advance 연출로 미리보기.
- "PGY 복사" = 생성된 scene 코드를 클립보드로 → 소스에 붙여넣기.

좌표 규약: 캔버스 좌표를 stage 대비 **%로 방출**(해상도 독립, CSS clip-path %와
동일). 그래서 **에디터 출력 ≡ 컴파일러가 emit할 shape** — 둘이 같은 표현을
공유하므로 어긋나지 않는다.

프레임워크 정당성: 비주얼 에디터는 약점이 아니라 *채택 동력*이다
(Twine/Scratch/게임엔진 씬에디터). 언어는 포맷, 에디터는 저작 표면 — 서로를
강화한다. 게다가 에디터는 PGY 컴파일러를 *호출하지 않고* 같은 polygon 표현만
공유하므로, **언어 구현과 독립적으로 먼저 출시 가능**하다(rung 0).

참조 구현: `examples/manga_cutscene/editor.html` — 작동하는 프로토타입.
꼭짓점 드래그=모양, 패널 드래그=이동, 프리셋(사각형/사다리꼴/쐐기/파편),
실시간 `scene` 코드, ▶ 실행 미리보기.

## Rung 사다리

0. **비주얼 에디터 (언어와 독립 — 먼저 출시 가능)**: 캔버스에서 모양 조절 →
   `scene` 코드 생성 → 실행 미리보기. 같은 polygon 표현만 공유하므로 컴파일러
   불필요. `examples/manga_cutscene/editor.html` 이미 작동. 위 "비주얼 저작" 참조.
1. **세로 웹툰 슬라이스(첫 컴파일 타깃)**: `scene` + `cut` N개 + `advance: click`
   + `caption`/`say`. shape·gaze 없이. 순수 HTML/CSS(`:target`) 방출.
   `examples/manga_cutscene/scene_webtoon.html`이 목표 출력의 손코딩 버전.
2. **만화 페이지 슬라이스**: `shape`(clip-path 프리셋) + `enter` transition.
   `prototype.html`이 목표 출력.
3. **시선 유도**: `gaze A -> B` → SVG path 생성. `prototype.html`/`scene_standoff.html`.
4. **이펙트**: `shake`/`flash`/`tone` + SFX 타이포. `scene_awakening.html`.
5. **Pergyra-WASM 디렉터 (선택)**: `advance: auto(t)`, 키보드, 사운드 등 동적
   연출. 손코딩 JS가 아니라 Pergyra→WASM 디렉터 + 정수-키 호스트 shim으로 구동.
   위 "동적 연출 = Pergyra-WASM 디렉터" 절 참조. `auto`/`js` effect를 요구하는
   scene에서만.

각 rung은 기존 패턴대로 파서(`scene`/`cut`) → AST → emit, 그리고 목표 출력과
byte 비교하는 게이트로 잠근다.

## 참조 프로토타입 (목표 출력의 손코딩 버전)

`examples/manga_cutscene/`:

- `index.html` — 갤러리.
- `prototype.html` — 액션(대각선 컷 · 시선 화살표 · SFX). rung 2~4 목표.
- `scene_standoff.html` — 대치(분할 화면 · 시선 핑퐁 · 충돌). rung 3 목표.
- `scene_awakening.html` — 각성(상승 연출 · 방사 스피드라인 · 플래시). rung 4 목표.
- `scene_webtoon.html` — 세로 웹툰(같은 beat 엔진). rung 1 목표.

이들은 "PGY가 무엇을 생성해야 하는가"의 명세다. 다음 작업은 이 출력을 손코딩이
아니라 PGY 컴파일 결과물로 뽑는 것 — rung 1부터.
