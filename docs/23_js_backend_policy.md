# Pergyra JavaScript Backend Policy

WebGL module boundary (2026-05-04): WebGL is not promoted to core language
surface. The beta path is only `Pergyra -> C backend --emit-c -> optional
Emscripten/WebGL bridge`; it is an embedding/module bridge proof. `pgy.render.webgl`
is reserved as a post-beta ecosystem module alongside `pgy.render.skia`, while
direct JS backend and native LLVM wasm remain beta+1.

留덉?留??낅뜲?댄듃: 2026-05-04

Status: beta+1 / historical design note. Direct `.pgy -> JS` backend work is not
the beta or first dogfood path. The current beta dogfood path is
`Pergyra -> C backend --emit-c -> optional Emscripten/WebGL bridge`; see

직접 JS 백엔드는 베타 dogfood 경로가 아님.
`TODO.md` 짠0b and `docs/100_beta_readiness_checklist.md`.

## 紐⑹쟻

JavaScript 諛깆뿏?쒕뒗 beta+1 ?댄썑 ?ш??좏븷 ???덈뒗 釉뚮씪?곗?? Node.js ?ㅽ뻾
?寃잛씠??
?섏?留?JS ?앺깭怨꾩쓽 `class` / `extends` / 遺紐??몄텧 ?섎?濡좎씠
Pergyra 肄붿뼱 議댁옱濡좎쓣 ?ㅼ뿼?쒗궎硫????쒕떎.

?곕씪??JS 諛깆뿏?쒕뒗 **?몄뼱 ?섎?濡좎쓣 JS??留욎텛???묒뾽**???꾨땲??
**Pergyra ?섎?濡좎쓣 JS ?꾩뿉 蹂댁〈?댁꽌 ?대━???묒뾽**?댁뼱???쒕떎.

?먰븳 硫?고뵆?ロ뤌 ?꾨왂???곗꽑?쒖쐞??媛쒕퀎 ?뚮옯???몄뼱 諛깆뿏?쒕낫??**怨듯넻 UI IR**??癒쇱???
利?Kotlin/Android ?꾩슜 諛깆뿏?쒕? ?쒕몮???섎━湲곕낫??
native / web / mobile???④퍡 ?뚮퉬?????덈뒗
scene/projection UI IR??癒쇱? 怨좎젙?댁빞 ?쒕떎.

## 怨좎젙 ?먯튃

- 肄붿뼱 ?몄뼱??inheritance瑜?湲곕낯 ?섎?濡좎쑝濡?梨꾪깮?섏? ?딅뒗??- 肄붿뼱 ?몄뼱??遺紐??몄텧 ?쒕㈃??湲곕낯 ?쒕㈃?쇰줈 梨꾪깮?섏? ?딅뒗??- ?ъ궗?⑹? `ability`, `role`, composition, delegation???곗꽑?쒕떎
- JS backend??beta+1 ?댄썑 ?꾩슂?깆씠 ?낆쬆?섎㈃ ?대? lowering?먯꽌
  delegation/mixin/object composition???ъ슜?쒕떎
- JS interop? beta+1 ?댄썑 蹂꾨룄 `extern js` 怨꾩링?쇰줈 遺꾨━?쒕떎

利?JS 諛깆뿏?쒓? ?꾩슂?섎떎???댁쑀濡?Pergyra 肄붿뼱??`extends` / 遺紐??몄텧 / prototype-chain 以묒떖 ?ш퀬瑜??ㅼ씠吏 ?딅뒗??

## 議댁옱濡????
### struct

- plain value object
- shallow immutable-ish record lowering???곗꽑
- ?꾩슂?섎㈃ simple object literal ?먮뒗 helper constructor濡??앹꽦

### subject

- identity-bearing active host
- hidden self-cell / state cell??媛吏?object濡?lowering
- plain structural copy瑜??덉슜?섏? ?딆쓬
- method??value-copy receiver媛 ?꾨땲??identity cell 湲곕컲 dispatch濡?蹂몃떎

媛?ν븳 lowering 諛⑺뼢:

- closure-backed cell object
- hidden state slot??媛吏?plain object
- generated factory + method table

?듭떖? JS `class` 臾몃쾿???곕뒓?먭? ?꾨땲??
**subject媛 passive value媛 ?꾨땲??identity-bearing host濡??좎??섎뒓??*??

### class

- passive nominal object/value surface
- ?꾩슂?섎㈃ JS `class` ?먮뒗 plain factory/object濡?lowering 媛??- 肄붿뼱 ?섎?濡좎긽 `subject`? ?숈씪?쒗븯吏 ?딅뒗??
### object / tobject

- projection / transfer representation
- field-only surface瑜??곗꽑
- serialization-friendly shape ?좎?

### participant

- ?낅┰ ontological kind媛 ?꾨땲??`subject` execution profile
- JS lowering?먯꽌??subject + mailbox/scheduler wrapper
- event loop / microtask / runtime queue瑜??ъ슜?섎뜑?쇰룄 蹂몄쭏? execution model?대떎

## ability / role lowering

- `ability`??JS interface 臾몃쾿?쇰줈 吏곸젒 留ㅽ븨?섏? ?딅뒗??- `ability`??contract metadata + dispatch table shape濡?蹂몃떎
- `role`? mixin, delegated method bundle, generated vtable object 以??섎굹濡?lower?????덈떎

沅뚯옣 諛⑺뼢:

1. semantic/HIR?먯꽌??吏湲덉쿂??`ability` / `role`??遺꾨━ ?좎?
2. JS lowering?먯꽌 role impl??method bundle濡??앹꽦
3. subject/party/zone/world媛 洹?bundle??李몄“?섍쾶 ?쒕떎

## relation / effect / zone / world

JS 諛깆뿏?쒕룄 ??怨꾩링??inheritance濡??吏 ?딅뒗??

- `relation` / `effect`??overlay state object
- `zone`? subject/object/tobject projection怨?lifecycle state瑜?媛吏?coordinator object
- `world`??zone registry + lifecycle orchestrator

利?deeper runtime semantics??JS?먯꽌??prototype chain???꾨땲??explicit state object? sync step?쇰줈 ?몃뒗 寃껋씠 留욌떎.

## interop ?뺤콉

?몃? JS 肄붾뱶? 遺숈쓣 ?뚮쭔 蹂꾨룄 interop surface瑜?怨좊젮?쒕떎.

?덉떆 諛⑺뼢:

```pergyra
extern js class HTMLElement;
extern js func setTimeout(cb: JsFn, ms: Int) -> JsHandle;
```

?ш린?쒖쓽 `js class`??Pergyra 肄붿뼱 `class/subject` 議댁옱濡좎씠 ?꾨땲???몃? ?고?????낆쓣 媛由ы궎??interop ?꾩슜 ?댄쐶?ъ빞 ?쒕떎.

## 援ы쁽 ?쒖꽌

0. 怨듯넻 UI IR ?곗꽑
   - `Window`, `Scene`, `Node`, `Layout`, `DrawCommand`, `InputEvent`, `ProjectionBinding`, `DirtyScope`
   - `subject`??吏곸젒 UI node媛 ?꾨땲??projection source
   - `object` / `tobject` / projection surface媛 UI ?뚮퉬 ?쒕㈃
   - `zone` / `world` dirty sync媛 UI 媛깆떊 contract

1. 肄붿뼱 ?섎?濡??좎?
   - `subject != class`
   - inheritance / 遺紐??몄텧 誘몃룄???좎?

2. JS backend IR shape 怨좎젙 (beta+1 ?댄썑)
   - record
   - cell object
   - method bundle
   - async task / mailbox wrapper

3. 理쒖냼 lowering (beta+1 ?댄썑)
   - `struct`
   - `class`
   - `subject`
   - `tobject`
   - basic function / method / projection

4. UI IR consumer濡쒖꽌 web surface ?곌껐
   - scene/projection UI IR -> browser runtime
   - DOM / canvas / WebGL 以?援ъ껜 lowering ?좏깮

5. orchestration/lifecycle
   - participant runtime
   - channel/future shim
   - zone/world sync semantics

6. interop (beta+1 ?댄썑)
   - `extern js`
   - DOM / Node surface

7. mobile ?ы룊媛
   - Android/iOS???곗꽑 怨듯넻 UI IR consumer ?먮뒗 shell bridge濡??묎렐
   - Kotlin backend??web/native 寃쎈줈媛 遺議깊븯?ㅺ퀬 ?뺤씤????蹂꾨룄 寃??
## 寃곕줎

吏곸젒 JS 諛깆뿏?쒕뒗 踰좏? dogfood 寃쎈줈媛 ?꾨땲?? ?꾩슂?깆씠 dogfood evidence濡??낆쬆?섎㈃ beta+1 ?댄썑 ?ш??좏븳?? 洹?寃쎌슦?먮룄 肄붿뼱 ?몄뼱??inheritance??遺紐??몄텧 ?쒕㈃???ｌ쓣 ?꾩슂???녿떎.

Pergyra??怨꾩냽 intent-first ?ㅺ퀎 ?몄뼱?댁옄 subject-core host ?몄뼱濡??④퀬,
JS??洹??섎?濡좎쓣 援ы쁽?섎뒗 ???寃잛씪 肉먯씠??
