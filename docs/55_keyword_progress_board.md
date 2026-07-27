# Keyword Progress Board

Updated: 2026-07-28 (Asia/Seoul)

이 문서는 키워드별 임의 백분율을 매기는 표가 아니다. 언어 어휘의 현재 권위와
구현 증거를 어디서 확인해야 하는지, 그리고 object-to-action 경계의 다음 폐쇄
순서를 기록하는 navigation board다.

## 1. 현재 어휘 권위

- SoT는 `src/lexer/language_keyword_registry.def` 하나다.
- 현재 registry는 145행이며 `RESERVED 71`, `CONTEXTUAL 71`, `SOFT 3`이다.
- 전체 단어와 axis 목록은 `docs/199_language_word_and_dogfood_grammar.md` 1.3,
  행별 parser/tooling 증거는 생성물
  `docs/semantics/language_word_implementation_inventory.generated.md`가 보여 준다.
- Native lexer, token debug, self-host lexer projection, LSP, TextMate와 문서는 이
  registry의 소비자다. 이 파일에 두 번째 spelling 목록을 만들지 않는다.
- `context/support/tooling` 비트는 선언된 projection fact다. parser path 또는
  production self-host 대체 완료를 뜻하지 않는다.

현재 생성 inventory의 구현 증거 분포는 다음과 같다.

| evidence class | language words |
| --- | ---: |
| native + typed self-host selector | 80 |
| native + self-host direct-string selector only | 18 |
| native selector only | 46 |
| parser selector 없음 | 1 (`channel`) |

Typed selector가 있어도 direct-string selector는 34개 단어에 37회 남아 있다.
이 수치는 migration debt이며, fixture 수나 support bit로 숨기지 않는다.

## 2. object-to-action 경계 어휘의 실제 분류

아래 표는 registry class와 현재 parser evidence를 요약한다. 의미 선택과 authoring
best practice는 `docs/200_object_to_action_boundary_patterns.md`가 소유한다.

| word | lexical class | parser evidence | 경계 책임 | 현재 dogfood 판정 |
| --- | --- | --- | --- | --- |
| `object` | reserved | native + typed self-host | 같은 실행 경계의 local read projection | non-empty self DIR `refresh` row는 `SUBSTITUTING`; 일반 compiler object 사용은 `SURFACE` |
| `tobject` | reserved | native + typed self-host | source lifecycle에서 분리된 immutable transfer | artifact receipt는 `REACHABLE`; non-empty self DIR `publish` row는 `SUBSTITUTING` |
| `vessel` | reserved | native + typed self-host | subject-owned pointer-self state/resource | production declaration 0, `SURFACE` |
| `subject` | reserved | native + typed self-host | 복제 불가능한 결정·승인 identity | direct-MIR 한 slice가 `REACHABLE` |
| `action` | contextual | native + typed self-host | subject가 소유하는 관측 가능한 transition | direct-MIR 한 action이 `REACHABLE` |
| `effect` | reserved | native + typed self-host | participant에 적용·유지되는 typed layer | `refresh` row producer는 `SUBSTITUTING`; apply/runtime materialization은 열림 |
| `relation` | reserved | native + typed self-host | 두 participant identity 사이의 materialized edge | `publish`/`link` row producer는 `SUBSTITUTING`; runtime materialization은 열림 |
| `zone` | reserved | native + typed self-host | membership, authority, lifetime, topology frontier | direct-MIR zone과 ID-keyed plan은 `REACHABLE`; non-empty DIR producer slice는 `SUBSTITUTING` |
| `intent` | reserved | typed selector와 direct-selector debt 공존 | 여러 실제 action의 성공·실패·보상 protocol | production call 없음, `SURFACE` |
| `world` | reserved | native + typed self-host | 여러 실제 zone을 묶는 하나의 composition root | direct-MIR 한 composition이 `REACHABLE` |

`action`이 contextual인 것은 약한 기능이라는 뜻이 아니다. lexer는 identifier로
남기고 subject body parser가 exact declaration 문맥에서 선택한다. 반대로
`object`/`effect`/`zone`이 reserved라고 해서 production 경로가 해당 의미를
실행한다는 뜻도 아니다.

## 3. 경계 동사와 clause의 구현 상태 읽기

| family | words | 현재 self-host selector 상태 | 폐쇄 조건 |
| --- | --- | --- | --- |
| projection | `refresh`, `publish`, `bind`, `from` | typed selector와 exact object/tobject slot join 존재 | `bind` 일반화와 runtime materialization; refresh→tobject/publish→object는 이미 fail closed |
| layer transition | `apply`, `link`, `between`, `to` | `link` typed row, `apply` exact-field validation 존재 | apply row carriage와 effect/relation storage·sync runtime operation fact |
| 열린 layer transition | `maintain`, `detach`, `unlink` | native-only | self-host typed parser/semantic/DIR/MIR/runtime와 negative gate |
| authority | `authority`, `authorized`, `by`, `within`, `causes` | typed selector 존재하나 일부 direct debt 잔존 | declaration contract, call binding, runtime identity/ability evidence를 분리해 폐쇄 |
| intent step | `step`, `using`, `who`, `expect`, `success`, `failure` | typed와 direct-only가 혼재 | explicit owner fact와 production intent call, consumed outcome |

키워드 하나의 “진행률”을 숫자로 합치지 않는다. 예를 들어 `causes`의 parser와 MIR
declaration carriage가 닫혀도 effect materialization/runtime transition은 별도 fact
family다. `authorized by` declaration이 존재해도 call-site participant binding과
runtime 승인은 완료되지 않는다.

## 4. 구현 전략

새 언어 word 또는 기존 word의 의미를 확장할 때는 다음 순서를 따른다.

1. registry 한 행에서 spelling, class, stable word identity, context/axis를 정한다.
2. owning parser가 `LanguageWordId`로 exact 문맥을 선택하고 distinct typed node를
   만든다. 직접 문자열 비교는 새 owner가 아니라 migration debt다.
3. semantic/DIR owner가 실제 fact를 결정한다. registry context mask가 grammar나
   의미 판정을 대신하지 않는다.
4. MIR은 owner fact와 source identity를 lossless하게 운반한다. 다른 producer나
   revision으로 canonicalize하면 모든 dependent ID를 원자적으로 remap한다.
5. target-neutral plan을 한 번 만들고 C/LLVM이 같은 plan을 소비한다.
6. old read path, name-only join, missing-fact fallback을 삭제하고 negative gate로
   재도입을 막는다.

증거 등급은 `SURFACE -> REACHABLE -> SUBSTITUTING`을 쓴다. support bit, syntax
highlight, fixture occurrence, import count, readiness `Bool`은 이 등급을 자동으로
올리지 않는다.

## 5. 현재 우선순위

1. 완료: `zone_layer_projection_runtime`의 self-host producer가 effect `refresh`,
   relation `publish`, zone `link`의 non-empty topology 3행을 typed identity로
   생산한다. 이 좁은 DIR→MIR producer 대체만 `SUBSTITUTING`이다.
2. 완료: canonicalization은 owner/directive/field ID를 한 identity epoch으로 함께
   재발급하며 stale raw ID와 `player` name + canonical `enemy` ID를 거부한다.
3. 완료(`REACHABLE`): 한 ID-keyed graph plan이 admission에서 한 번 검증되고 C/LLVM에
   exact `nodes=3 edges=2 depth=2 pass_limit=2`, `trust <- player`,
   `trust <- enemy`를 투영한다. 이 trace는 runtime state materialization 증거가 아니다.
4. 다음: 현재 identity만 검증하고 버리는 `apply`를 MIR row로 운반한 뒤, plan/runtime
   owner가 `.poison`/`.trust` storage와 refresh/publish sync를 실제 materialize해야
   `7`/`dst` runtime gate를 닫을 수 있다. Generic zero-fill과 native graft는 금지다.
5. 그 다음 state/layout/sync operation, `maintain`/`detach`/`unlink`를 각각 별도
   fact와 falsifying fixture로 닫는다.
6. 실제 production action이 둘 이상 연결되기 전에는 root `intent`를 실행 진척으로
   세지 않는다.

이 board가 registry, generated inventory, current source 또는 executable gate와
충돌하면 뒤의 증거가 우선하며 이 문서를 고친다.
