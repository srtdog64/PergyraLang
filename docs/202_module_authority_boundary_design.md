# 202. Module system: `use MODULE;`은 authority 경계다

Updated: 2026-08-10 (Asia/Seoul)

Status: **DESIGN + MODEL CLOSED / SURFACE NOT IMPLEMENTED**. 의미 핵은
`docs/semantics/proofs/ModuleAuthority.v`로 먼저 구현되어 kernel-check를
통과했다(가장 싼 구현). 컴파일러 표면(`use`)은 아직 없다 — self-hosted
parser는 지금도 `use`를 `surface_not_covered`로 정직하게 거부한다
(`src/self_hosted/parser/diagnostic_owner.pgy`). 이 문서가 승인한 의미론
밖의 구현은 착지할 수 없다.

## 1. 왜 모듈이 authority 경계인가

Pergyra의 논제는 "intent/authority/lifetime/budget 없이는 효과 없음"이다.
지금 이 규율은 SoT 레지스트리(`docs/semantics/sot_owner_spine_registry.md`)와
zone/world 표면이 컴파일러 내부에서만 강제한다. 모듈 시스템은 같은 규율의
사용자 표면이다:

- **module = authority 경계.** 모듈은 자신이 소유한 authority(`owns`)와
  수입으로 받은 authority만 재수출할 수 있다. import 그래프를 아무리
  쌓아도 없던 권한이 생기지 않는다.
- **`use MODULE;` = 수입 간선.** 수입자는 모듈의 export 표면을 받고,
  모듈의 재수출 authority 표면을 물려받는다. 그 이상은 없다.
- **export 하지 않은 이름은 존재하지 않는다.** 경로 import가 주던 "파일을
  알면 전부 보인다"는 그늘이 사라진다.

현행 경로 import(`import "relative/path.pgy"`)의 실측 비용이 이 설계의
동기다: per-file import-closure 규율은 1353개 소스에 손 규율로 유지되고,
가시성-전용 import 간선이 실제 순환을 가린 사례가 두 번 있었으며(수동
절단으로 해소), 이름 공간은 함수명 접두사 관례(`SelfMir*`, `DirectMir*`,
`Compiler*`)가 대신한다. stdlib이 자라기 전에 모듈 경계가 서야 이행
비용이 최소가 된다.

## 2. 표면 설계 (요약)

- 모듈 선언은 명시적 manifest다: 모듈이 무엇을 export 하고(이름), 무슨
  authority를 소유/재수출하는지 선언한다. SoT 레지스트리 행처럼 보이고,
  핀 가능하고, 게이트가 읽는다.
- `use MODULE;`은 선언 순서상 **앞서 선언된 모듈만** 참조할 수 있다
  (층화 링크). 순환 import는 검사로 잡는 게 아니라 **문법적으로 표현
  불가능**하다.
- 한 이름의 exporter는 링크 전체에서 **정확히 하나**여야 한다. 둘이면
  그 이름은 해석되지 않는다(다이아몬드 모호성의 원천 봉쇄; SoT의
  "한 fact 한 authority"와 같은 모양).
- `use`는 이미 예약어다(`src/lexer/language_keyword_registry.def`의
  `TOKEN_USE`). 표면 문법의 세부(모듈 선언 블록 vs 디렉터리 manifest)는
  구현 단계 결정으로 남긴다 — 의미론은 아래 모델이 고정한다.

**결정: 해석 우선순위는 도입하지 않는다 (2026-08-10).** import 순서·검색
경로·숫자 우선순위 어느 형태로도 이름 충돌의 티브레이크를 두지 않는다.
근거는 두 겹이다. 형식적으로, 우선순위는 `resolve_extension_stable`을
파괴한다 — 높은 우선순위 모듈 하나를 추가하는 것만으로 기존 코드의
이름이 조용히 재해석된다. 실질적으로, 두 모듈은 의미론이 같아도 세부가
다를 수 있고, 무엇이 교체 가능한지는 구조가 아니라 **저자만 아는
지식**이다 — 컴파일러가 순서로 추측할 대상이 아니다. 교체가 필요해지는
시점(테스트 더블, 계측 주입, 벤더링)의 정답은 링크 manifest의 **선언된
치환** 행("모듈 X의 export 표면을 모듈 Y로 치환")이며, 도입 시 모델에
치환 well-formedness와 frame 정리(치환은 선언된 이름만 바꾸고 나머지
해석은 불변)를 먼저 얹는다. 그 전까지 충돌의 유일한 의미는 거부다
(`ambiguous_name_unresolvable`).

## 3. 가장 싼 구현: ModuleAuthority.v가 지금 보증하는 것

모델: 링크는 모듈의 순서 리스트. 모듈은
`{ m_imports; m_exports; m_owns; m_reexports }`. 검사기는 전부 boolean
함수라 증명이 계산으로 내려간다. kernel-check(coqchk) 통과, admit 0,
새 공리 0(전역 예산은 여전히 2).

| 정리 | 내용 |
|---|---|
| `stratified_edge_lt`, `import_path_descends` | 수입 간선/경로는 링크 위치를 엄격히 감소시킨다 |
| `import_acyclic` | 층화 링크에는 자기 자신으로 돌아오는 import 경로가 없다 |
| `resolve_sound` | 해석 결과는 실제로 그 이름을 export 하는 모듈이다 |
| `resolve_finds` | 유일 exporter가 있으면 해석은 반드시 그 모듈을 찾는다 |
| `hidden_name_unresolvable` | 아무도 export 안 한 이름은 해석 불가(캡슐화) |
| `ambiguous_name_unresolvable` | exporter가 둘이면 해석 불가(모호성 거부) |
| `resolve_extension_stable` | 새 모듈이 기존 이름을 export 하지 않는 한 기존 해석은 불변(확장 안정성) |
| `authority_rooted` | 잘 형성된 링크에서 보이는 모든 authority는 어떤 소유자에 뿌리를 둔다 |
| `unrooted_invisible` | 소유자 없는 authority는 어떤 재수출 표면에도 나타날 수 없다(비증폭) |

거부 위트니스(게이트 문화의 falsifier): `cycle_rejected`(순환 링크),
`authority_from_nowhere_rejected`(무근거 재수출),
`ambiguous_export_unresolvable`(이중 export). 세 실패 형상은 같은
검사기가 계산으로 거부한다.

## 4. 부하: 크기가 커져도 감당하는가

두 겹으로 답한다.

**모델 차원 — 임의 크기 정리.** 생성기 `chain n`(깊이 n 수입 사슬)과
`fanout n`(모듈 n개가 한 소유자를 직접 수입)에 대해:

- `chain_wf : forall n, link_wf (chain n) = true`
- `chain_resolves_every_name : forall n i, i < n -> resolve (chain n) i = Some i`
- `fanout_wf : forall n, link_wf (fanout n) = true`

즉 "모듈 10개 이상"은 고정 시나리오가 아니라 **모든 n에 대한 정리**로
닫혀 있다. 추가로 n=16 실행 위트니스(`chain_16_wf`,
`chain_16_deep_resolution`, `chain_16_rooted`, `fanout_16_wf`,
`fanout_16_wide_resolution`)가 검사기를 `vm_compute`로 실측 평가한다 —
10-모듈 부하선을 넘는 링크에서 boolean 검사기가 실제로 돈다는 실행 증거.

**구현 차원 — 부하 게이트 계획(구현 단계 의무).** 모델의 `resolve`는
사양이지 구현이 아니다(naive filter는 O(N·X)). 구현은 링크 타임에 해석
테이블을 한 번 만들고(선형), **런타임 비용은 0**이어야 한다 — 해석이
전부 링크 타임에 끝나는 것이 "동작 비용으로 가장 싼" 설계의 뜻이다.
구현이 착지할 때 다음 게이트가 함께 착지해야 한다:

- 합성 링크 생성기(chain/fanout 형상, N=16/64/256)로 링크+해석 wall-time
  예산 스모크. 예산은 실측으로 시작해 래칫으로만 조인다(기존 게이트 문화).
- 깊은 사슬(N=256)의 마지막 이름 해석과 넓은 팬아웃의 authority 전파가
  모델 정리와 같은 답을 내는 executable 대조.

## 5. 기존 구조와의 정합

- **SoT 레지스트리:** 모듈 manifest의 owns/재수출 선언은 레지스트리의
  owner/consumer 행과 같은 사실 계열이다. 구현 시 모듈 경계 사실을
  `SoTAuthority.v`의 spine에 fact로 올릴지, ModuleAuthority 모델과
  adequacy 게이트로 별도 결속할지는 구현 설계에서 정한다(후자가 기본).
- **zone/world:** zone은 리소스 경계, 모듈은 코드/authority 경계다. 서로
  대체하지 않는다. world가 authority의 뿌리라는 점은 모델의
  `authority_rooted`가 "소유자 집합"으로 일반화해 이미 담고 있다.
- **lifecycle(#10)·extern "C":** 모듈 표면과 함께 self-host parser의
  `surface_not_covered` 목록에서 빠져나와야 하는 이웃들이다. 착지 순서는
  이 문서 승인 후 별도 계획으로.
- **이행 경로:** 접두사 관례 → 모듈 이름. per-file import-closure 규율은
  모듈 내부에서 그대로 유효하고, selfcheck는 모듈-closure 검사로
  일반화한다. 이행은 stdlib 성장 전에 시작한다.

## 6. 모델이 증명하지 않는 것

파서/링커 구현, 파일-모듈 매핑, 런타임 동작, 링크 타임 성능. 성능은 4절의
구현 게이트가 소유한다. 모델 정리를 구현 적합성이나 언어 전체 검증으로
서술하지 않는다(`ProofSpine.v`의 부정 경계가 이 문서에도 적용된다).

## 7. Self-host 이후의 독립 모듈 빌드 카드

Status: **DEFERRED — SELF-HOST CLOSURE 이후**. 이 절은 용어와 경계만
예약한다. 현재 `pgy build` 동작, module surface, SoT 상태, beta 진행률을
바꾸지 않으며 구현 큐를 열지 않는다.

| 개념 | 소유하는 경계 | 소유하지 않는 것 |
|---|---|---|
| `Module` | 이름, 가시성, export, semantic/authority provenance | 컴파일 묶음, 배포, 런타임 수명 |
| `ModuleInterface` | 검증된 export 의미의 compact receipt | 원 소스와 독립된 새 semantic authority |
| `BuildUnit` | 하나 이상의 Module을 함께 컴파일·캐시·링크하는 물리 단위 | 언어의 이름 해석과 module 의미 |
| `Artifact` | target별 ABI/layout/code materialization | target-independent semantic contract |
| `Package` | 버전·배포 단위 | 한 번의 컴파일 단위 |

따라서 `Combat + Ability + EnemyAI`를 `Gameplay` BuildUnit으로 묶거나,
`Combat + Ability + Network`를 `Server`로 묶는 것은 가능해야 한다. 같은
Module을 서로 다른 target의 BuildUnit에서 재사용할 수 있지만, 하나의 link
closure 안에서 동일 Module identity를 두 번 물리화해 이중 symbol authority를
만들 수는 없다. 명시적 치환 계약이 없다면 충돌은 거부한다.

목표 흐름은 다음과 같다.

```text
Module source
  -> semantic admission
  -> compact ModuleInterface
  -> source/HIR construction evidence erase

changed Module C + admitted interfaces A/B
  -> BuildUnit projection
  -> target Artifact
  -> link admission
  -> executable
```

`ModuleInterface`는 캐시 파일이 semantic authority가 되는 우회가 아니다.
원 Module identity, producer/schema version, export identities, semantic digest,
dependency receipts를 봉인하고, target artifact에는 별도의 ABI/layout digest와
target identity를 요구한다. 누락·불일치·stale receipt는 소스 재해석으로
조용히 보정하지 않고 해당 경계에서 실패한다. capability/authority envelope도
raw manifest 문자열이 아니라 승인된 module facts의 합집합으로만 계산한다.

`world`/`zone`과도 섞지 않는다. Module은 코드와 provenance 경계이고 zone은
런타임 존재·자원 수명 경계다. 한 zone은 여러 Module의 코드를 사용할 수 있고,
한 Module은 여러 zone에서 사용될 수 있다.

물리 grouping은 언어 문법이 아니라 Seashell build manifest 정책으로 시작한다.
generic specialization을 interface MIR, erased representation, link-time
specialization 중 어디서 소유할지는 아직 고정하지 않는다. 실제 첫 외부
프로젝트의 변경 그래프와 비용이 없는데 이 선택을 먼저 고정하면 운반물이
AST/HIR/MIR/owner metadata를 모두 품는 새 거대 artifact가 될 위험이 있다.

착수 순서는 다음으로 고정한다.

```text
self-host closure
  -> evidence/identity compression 안정화
  -> 실제 외부 프로젝트 하나
  -> ModuleInterface 최소 운반물 측정
  -> BuildUnit / incremental module build
```

즉 원칙은 **“모듈은 소스를 공유하지 않고 검증된 의미를 공유한다”**이지만,
그 검증된 의미의 최소 형태는 self-host가 더 닫힌 뒤 실제 workload로 정한다.
