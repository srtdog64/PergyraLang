# 161. WASM/미디어 Substrate 실행 설계도 (킬러 유즈케이스)

Status: `execution-blueprint`. 작성 2026-07-05. 킬러 유즈케이스 = **안전한
in-browser 던전크롤러("안전한 new Flash", docs/15)**. 이 문서는 그 substrate를
파일·rung 단위로 고정 — 나 없이 집행 가능한 정밀도. 상위: docs/15(capability
sandbox), docs/135(WASM/backend SoT 가드), docs/159(stdlib page/spray = 렌더러
표면). 짝: docs/158(M2 분기에서 M1 택하면 이 트랙이 근시일 본체였음 — M2 택했으니
병행 트랙).

---

## 0. 한 장 요약 — 벽은 WASM이 아니다 (실측 정정)

- **WASM transport: ✅ 이미 검증됨.** docs/135 §2: **C→wasm32-wasi 경로 end-to-end
  검증** — Pergyra→C→`wasm32-wasi`→Node WASI 실행→JS에서 export 호출(`ViewAfter`).
  던전크롤러는 **이미 브라우저에서 돌 수 있다**(transport 층). 직접 `pgy --emit-wasm`은
  post-beta 순도 목표지 벽 아님.
- **미디어 API 표면: ✅ 이미 있음(stub).** `pgy_runtime_media_stub.h` = 7개
  capability-gated 엔트리(render clear/fill_rect, audio play_tone, input poll_key),
  **API 모양 고정**, headless(call count만). manifest가 RENDER 생략하면 **오늘도
  그릴 수 없다**(fail-closed 게이트 실작동).
- **★진짜 벽 = 3개**: (W-2) stub을 **실 브라우저 미디어 백엔드**(canvas/WebGL/
  WebAudio)로 + JS glue, (W-3) **렌더러 언어 표면**(`pgy.render.webgl` /
  stdlib page·spray — docs/159 §5와 직결), (W-4) **서명 로더**(배포 신뢰 =
  "안전한 Flash"의 안전 절반).
- **5-rung 중 2개(transport, API 표면) 완료.** 남은 3개가 이 문서.
- **capability 화해는 이미 됨**: RENDER/AUDIO/INPUT이 이미 capability 축
  (PGY_CAP_RENDER/AUDIO/INPUT). "안전한 Flash" = sandbox 콘텐츠에 미디어는 grant,
  **file/network/subprocess는 거부**(docs/158 §5 G-EXEC 분리선과 동형).

---

## 1. 실측 현재 상태

- **WASM transport**(docs/135 §2, `wasm-backend-parity-test-smoke`, ci-linux:61):
  C→wasm32-wasi verified. `backend-wasm-pointer-closure-test-smoke`(docs/135)가
  pointer/closure lowering 잠금. LLVM→wasm은 runtime-link debt(ucontext shim +
  wasm-buildable runtime object 부재) — C 경로가 있으니 블로커 아님.
- **미디어 stub**(`src/runtime/pgy_runtime_media_stub.h`, 7 엔트리):
  `pgy_render_clear(rgba)` · `pgy_render_fill_rect(x,y,w,h,rgba)` [RENDER] ·
  `pgy_audio_play_tone(freq,ms)` [AUDIO] · `pgy_input_poll_key()` [INPUT] +
  `pgy_media_call_count`. 전부 `pgy_cap_require_export`로 게이트, static-inline
  C-twin(LLVM은 미래 external twin — ambient op 패턴 동일).
- **던전크롤러 콘텐츠**: `examples/dnd_tavern_campaign/`(combat_cards/dialogue/
  events/intents/…) 실재 — Pergyra 프로그램으로 이미 존재(`llvm-dnd-campaign-
  test-smoke`). 단 현재는 텍스트 출력(미디어 미사용).
- **dogfood WebGL 브리지**(`dogfood-webgl-test-smoke`, `examples/wasm_hello`):
  **host 브리지만** 검증(C emit → wasm). "stable WebGL language-surface 게이트
  아님 — 렌더러 API는 post-beta `pgy.render.webgl`"(스크립트 명시).

---

## 2. Rung 사다리 (W-0..W-5)

status ∈ {done, gap}. done = artifact+gate 실존.

| rung | 내용 | status | artifact/gate |
|---|---|---|---|
| **W-0** | WASM transport (C→wasm32-wasi) | **done** | docs/135 §2, wasm-backend-parity-smoke |
| **W-1** | 미디어 API 표면 (capability-gated stub) | **done** | pgy_runtime_media_stub.h, test-capability |
| **W-2** | 실 미디어 백엔드 (canvas/WebGL/WebAudio) + JS glue | **gap** | §3 |
| **W-3** | 렌더러 언어 표면 (`pgy.render.webgl` / stdlib page·spray) | **gap** | §4, docs/159 §5 |
| **W-4** | 서명 로더 (capability manifest + 서명 검증) | **gap** | §5 |
| **W-5** | 던전크롤러 dogfood (dnd → wasm + 미디어 + 서명 = 데모) | **gap** | §6 |

**순서**: W-2(실 백엔드) ≻ W-3(렌더러 표면, W-2의 소비자) ≻ W-4(서명, 배포) ≻
W-5(통합 데모). W-3는 docs/159 page/spray doctrine-pass와 합류(같은 작업).

---

## 3. W-2 — 실 미디어 백엔드 + JS glue (핵심 벽)

**계약**: stub의 7 엔트리가 **고정된 API**다. 실 백엔드는 그 시그니처를 그대로
구현하되 call-count 대신 실제 draw/play/read. wasm32-wasi 모듈은 이 함수들을
**import**로 남기고, JS glue가 브라우저 API에 연결.

**구현 3부:**
1. **wasm import 경계**: media 함수를 wasm import로 방출(현 static-inline C는
   네이티브 전용). wasm 빌드 시 `pgy_render_*`/`pgy_audio_*`/`pgy_input_*`를
   `extern` import 선언으로 → wasm 모듈이 host(JS)에게 요구.
2. **JS glue**(`site/` 또는 `runtime/wasm/media_glue.js`): import를 canvas 2D/
   WebGL/WebAudio/keyboard 이벤트에 바인딩. `render_fill_rect(x,y,w,h,rgba)` →
   `ctx.fillRect`; `audio_play_tone(f,ms)` → WebAudio OscillatorNode;
   `input_poll_key()` → keydown 큐 pop. **capability 게이트는 wasm 안에 그대로**
   (import를 호출하기 전에 `pgy_cap_require_export`가 fail-close — glue는 게이트
   우회 못 함).
3. **parity**: headless stub(call count)과 실 백엔드가 **같은 호출 시퀀스**를
   내는지 검증(stub이 oracle). 던전크롤러 한 프레임을 stub로 돌려 call-count
   기록 → 실 백엔드가 같은 호출 순서 방출 확인.

**게이트**: `wasm-media-backend-parity-smoke`(신설) — stub 호출 시퀀스 == wasm
import 호출 시퀀스. 실 draw 픽셀 비교는 스코프 밖(호스트 렌더러 몫), **호출
계약**만 잠근다.

**입력 확장**: 현 stub은 `poll_key`만. 던전크롤러는 마우스/터치/키 필요 →
input API 확장(poll_pointer, poll_key_down/up). API 모양을 W-2에서 확정(미래
백엔드가 구현할 계약).

---

## 4. W-3 — 렌더러 언어 표면 (docs/159와 합류)

**현 문제**: 콘텐츠가 `pgy_render_fill_rect`를 raw로 부르는 건 저수준. 던전크롤러는
"방/카드/전투 UI"를 idiomatic하게 그려야 한다 → **stdlib page/spray**(docs/159 §5,
견인 최우선)가 정확히 이 층. `page`(렌더 트리 well-formed 불변식) + `spray`
(좌표/색/레이어 불변식)를 doctrine-pass(docs/159 §2 7항)해서 W-2의 media 백엔드
위에 얹는다.

**연결**: docs/159 WO-L4-PAGE/SPRAY == 이 rung. page/spray의 fail-closed 불변식
(닫힌 태그, 레이어 순서)이 렌더 오류를 컴파일/런타임에 잡는다 = thesis(도메인
의미)의 그래픽 표현. **`pgy.render.webgl`은 이 stdlib 모듈의 별명/네임스페이스**
(docs/135가 예고한 post-beta module ecosystem 자리).

**게이트**: page/spray의 doctrine-pass fixture(docs/159 §2-6) + 렌더 트리 →
media 호출 시퀀스 골든.

---

## 5. W-4 — 서명 로더 ("안전한 Flash"의 안전 절반)

**왜**: 배포형 인터랙티브 콘텐츠(던전크롤러)를 신뢰하려면 — 이 wasm이 선언한
capability만 쓰는가? 서명은 유효한가? = "안전한 new Flash"의 *안전*.

**설계:**
1. **capability manifest**(이미 있음, `--capability-manifest`, docs/15): wasm에
   used-capability 집합을 정적 방출. 로더가 이걸 읽어 grant 결정.
2. **서명 검증**: wasm 모듈 + manifest를 서명(콘텐츠 제작자 키). 로더가 로드
   시점에 서명 확인 → 변조 거부. slot_security의 crypto(SHA-256, docs/security)
   재사용.
3. **로드-시점 grant**: 로더가 manifest의 declared caps를 host env로 주입
   (PGY_CAP_GRANT 채널, docs/15). **콘텐츠는 RENDER/AUDIO/INPUT만 요청 가능,
   file/network/subprocess 요청 시 로더가 거부**(= 정확히 docs/158 §5 분리선).
4. **budget 동반**: wall-time/memory budget(R6, docs/redteam)를 로더가 강제 —
   무한루프/메모리고갈 콘텐츠 차단. capability(정성) + budget(정량) 양축이
   이미 런타임 강제(레드팀 R6 RESOLVED).

**게이트**: `signed-loader-smoke`(신설) — (a) 서명 유효 + declared caps ⊆
{render,audio,input} → 로드, (b) 서명 변조 → 거부, (c) file/network cap 요청 →
거부, (d) budget 초과 → fail-close. 양 백엔드.

**결정점(BDFL)**: 서명 방식(단순 SHA-256+키 vs 정식 PKI), 배포 포맷(단일 wasm+
manifest 번들 vs 분리). 권고: 단순 시작(SHA-256 서명 + 임베디드 pubkey), PKI는
사용자 생기면.

---

## 6. W-5 — 던전크롤러 dogfood (통합 데모)

`examples/dnd_tavern_campaign`(텍스트)를 **미디어 사용 버전으로 확장** → wasm32-wasi
빌드 → 실 미디어 백엔드(W-2) + page/spray 렌더(W-3) + 서명 로더(W-4) → 브라우저에서
플레이. **이게 킬러 유즈케이스의 완성이자 thesis 전시**(도메인 primitive =
combat/dialogue/events intent가 안전 sandbox에서 그래픽으로).

**게이트**: `dogfood-webgl-test-smoke` 확장 — 현 host 브리지 검증에 미디어 호출
시퀀스 + capability manifest(render/audio/input만) + 서명 검증 추가. 실 브라우저
렌더는 수동 확인(자동 픽셀 비교는 스코프 밖).

---

## 7. 시퀀싱 + WO + 정직한 스코프

**순서**: W-2 실 백엔드 ≻ W-3 렌더러 표면(docs/159 page/spray 합류) ≻ W-4 서명
로더 ≻ W-5 통합. W-2가 최대 신규 작업(JS glue + wasm import 경계). W-3/W-4는
기존 자산 조립(stdlib doctrine-pass + capability manifest + crypto 재사용).

**WO 등록:**
- **WO-W2-MEDIA** — 실 미디어 백엔드(wasm import 경계 + JS glue + stub-parity).
- **WO-W3-RENDER** — page/spray 렌더러 표면(= docs/159 WO-L4-PAGE/SPRAY).
- **WO-W4-SIGNED** — 서명 로더(manifest+서명+grant+budget, 서명 방식 BDFL).
- **WO-W5-DOGFOOD** — 던전크롤러 미디어 버전 + 통합 게이트.

**정직한 스코프(docs/135 준수):** 렌더러 API는 **post-beta module ecosystem**
(`pgy.render.webgl`) — beta-stable 언어 표면 주장 금지. "던전크롤러가 브라우저에서
돈다"는 W-5 완료 후에만, 그것도 "C→wasm 경로 + 실 미디어 백엔드"로(직접
emit-wasm 아님 — 오버클레임 금지, docs/135 forbidden wording). 미디어 stub이
"실 렌더한다"고 말하지 말 것(headless call-count일 뿐).

## Related

docs/15(capability sandbox — 미디어 게이트 + 서명 로더 근거) · docs/135(WASM SoT
가드 — transport 검증 + forbidden wording) · docs/159 §5(page/spray = W-3 렌더러
표면) · docs/158 §5(G-EXEC 분리선 = W-4 콘텐츠 cap 제한) · redteam R6(budget —
W-4 정량 강제) · `pgy_runtime_media_stub.h`(7 엔트리 API 계약) ·
`examples/dnd_tavern_campaign`(W-5 콘텐츠) · docs/security(서명 crypto 재사용)
