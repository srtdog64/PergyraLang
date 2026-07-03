# 148. Stdlib Architecture — 설계 배선도

Status: `wiring-doc, inventory-gated`. docs/138이 **무엇을**(scope ledger)
담는다면, 이 문서는 **어떻게**(층/경계/정책/게이트 배선)를 고정한다.
§4 inventory 표는 `stdlib-inventory-test-smoke`가 잠근다 — 이 문서는
장식이 아니라 계약이다.

## 1. 타 언어 비교 — 무엇을 빌리고 무엇을 피하는가

| 언어 | stdlib 구조 | 빌려오는 것 | 피하는 것 |
|---|---|---|---|
| **Rust** | `core`/`alloc`/`std` 3층, `no_std`, `#[stable]` 안정성 속성 | **층 분리**(하부는 상부를 모름) + **안정성 원장**(모듈별 상태 명시) | 속성-주도 안정성 기계 전체(1인 규모에 과함 — 원장은 표로) |
| **Go** | 평평한 curated std, Go 1 호환 약속, generics 이전 시대의 per-type API(`sort.Ints`/`sort.Strings`) | **per-type API의 정당성**(Go가 10년 그렇게 살았다) + curated("작지만 전부 진짜") | 평평함의 끝없는 확장 — 우리는 층 태그로 관리 |
| **C#/.NET** (아버지, docs/119) | CoreLib(런타임 결합) vs BCL(표면) 분리, `Span<T>` 뷰 타입 | **CoreLib/BCL 경계 = 우리의 L0/L1 경계**, `Span<T>` = `StrView` 계보 | BCL의 다중 프레임워크 표면적 |
| **Python** | batteries-included 거대 평면 std | 킬러-유즈케이스 견인의 편의성 | **PEP 594 교훈**: 한 번 넣은 battery는 죽어도 못 뺀다 → domain 모듈은 킬러 유즈케이스가 *견인하는 것만* 입고 |

종합: **Rust의 층 + Go의 per-type 선례 + C#의 CoreLib/BCL 경계 + Python의
반면교사**. 그리고 어느 언어도 갖지 않은 것 하나를 우리가 베팅한다 — §2 L2.

## 2. 3층 배선

```
┌───────────────────────────────────────────────────────────────┐
│ L2  stdlib/ domain     money · ledger · obligation · datetime │  도메인 어휘
│                        page · spray · storage · timer · http  │  (Pergyra만의 베팅)
│                        versioning · device_adapter            │
├───────────────────────────────────────────────────────────────┤
│ L1  stdlib/ core       option · strview · [json* ← WO-L1]     │  순수 Pergyra
├───────────────────────────────────────────────────────────────┤
│ L0  builtin substrate  collections · string core · Option/    │  컴파일러 소유
│     (컴파일러/런타임)   Result 술어 · IO/clock/random(caps게이트)│  (C+LLVM lowering)
└───────────────────────────────────────────────────────────────┘
      의존 방향: L2 → L1 → L0 (역방향 금지)
```

- **L0 잔류 조건** (셋 중 하나 이상): (a) lowering/ABI 결합(collections,
  string core), (b) capability 게이트 결합(ReadFile/Now/Random — 런타임
  강제 지점), (c) 측정된 fused 성능 경로(SubIndexOf류, docs/perf).
  표면 Pergyra로 표현 가능하면 L1이 기본이다 — dogfood 압력을 stdlib이
  받는다.
- **L1**: capability 무접촉·순수 함수. per-type 콤비네이터, 뷰, 텍스트/JSON
  primitives. self_hosted/lib의 승격 목적지(WO-L1).
- **L2 — 유일한 베팅**: money/obligation/ledger가 std에 있는 주류 언어는
  없다. 이것이 [[thesis]](도메인 의미의 언어-복원)의 stdlib 표현이다.
  단 Python 교훈에 따라 **킬러 유즈케이스(웹 던전 크롤러, docs/15)가
  견인하는 모듈만** 입고·활성화한다. 견인 없는 도메인 어휘는 sketch로
  남는다.
- 물리 배치: 당분간 `stdlib/` 평면 + 본 문서의 layer 태그. 층별 모듈이
  10개를 넘으면 폴더 분리(그 전 폴더링은 cosmetic).

## 3. 정책 배선 (모듈이 지켜야 할 7계약)

1. **per-type 우선** — generic 함수의 구성-타입 단형화 seam(MIR 타입-텍스트
   fact, docs/147 · TODO 발견-1)이 닫히기 전까지 stdlib에 `<T>` 시그니처
   금지. `generic-nested-failclosed-test-smoke`가 회귀를 잠근다. Go-1
   시대가 이 형태의 10년 선례다. 명명: `<동사구><Type>` (`OptionOrInt`).
2. **caps 의무** — ambient에 닿는 모듈(http/storage/timer/device_adapter)은
   함수 시그니처에 `with caps <cap>` 선언 의무. **stdlib이 capability
   showcase다**: 샌드박스 스토리(docs/15)의 첫 소비자가 자기 표준
   라이브러리여야 한다. 현재 0/13 — caps 없는 ambient 모듈은 active 불가.
3. **namespace 의무(신규/승격분)** — `SelfHostDiagnostic` 선례대로
   namespace 블록 사용. 평평한 전역에서 이름-접두사로 소속을 짊어지는
   짐(OWNERS의 `Compiler...` 장명 교훈)을 stdlib에 반입하지 않는다.
   기존 gated 2종(option/strview)은 소급 리네임 비용 > 이득이라 유예,
   승격/신규부터 적용.
4. **import 경로** — 상대경로 import는 실증됨(`stdlib_option_bridges`가
   `../../../../stdlib/` 관통). `std:` 별칭 resolver = WO-L3 (편의지
   차단막 아님).
5. **게이트 의무** — `active` 모듈 = ① backend_compare fixture 또는 전용
   smoke ≥1, ② docs/138 행, ③ 본 문서 §4 행. `stdlib-inventory-test-smoke`
   가 ③과 파일계의 양방향 일치 + active의 게이트 실존을 검사한다.
6. **승격 파이프라인** — self_hosted/lib → stdlib(L1)로만, **stdlib이
   원본**이 되고 self_hosted가 import(역방향 의존 금지). 1호 = json*
   (WO-L1, 착수 조건은 TODO 보드).
7. **안정성 원장** — `active`(게이트 green, 계약 준수) / `sketch`(코드는
   있으나 게이트·caps·doctrine 미충족 — **호환성 약속 없음, 예고 없이
   변경/삭제 가능**) / 추후 `stable-subset`(docs/107 확장, beta 시점 동결)
   3단. Go 1 호환 약속의 축소판은 stable-subset 승격 시 명문화한다.

## 4. Inventory (stdlib-inventory-test-smoke가 잠금)

<!-- STDLIB-INVENTORY-BEGIN -->
| module | layer | status | gate |
| --- | --- | --- | --- |
| option.pgy | core | active | tests/cases/backend_compare/stdlib_option_bridges |
| strview.pgy | core | active | tests/string_window_builtins_smoke.sh |
| datetime.pgy | domain | sketch | - |
| device_adapter.pgy | domain | sketch | - |
| http.pgy | domain | sketch | - |
| ledger.pgy | domain | sketch | - |
| money.pgy | domain | sketch | - |
| obligation.pgy | domain | sketch | - |
| page.pgy | domain | sketch | - |
| spray.pgy | domain | sketch | - |
| storage.pgy | domain | sketch | - |
| timer.pgy | domain | sketch | - |
| versioning.pgy | domain | sketch | - |
<!-- STDLIB-INVENTORY-END -->

**sketch 일괄 판정 사유(2026-07-04 감사)**: 게이트 0 · caps 선언 0 ·
도메인 fail-closed 미구현. 표본: `MoneyAdd`가 통화 불일치를 무검사
통과시킨다 — docs/12(domain-lifecycle evidence) doctrine의 정확한 반례.
sketch → active 경로 = **WO-L4 doctrine-pass**: caps 선언 + 도메인
fail-closed(불변식 위반 = 거절/패닉) + fixture, 킬러-유즈케이스 견인
순서로 (datetime → page/spray → money/ledger → http[NETWORK cap]).

## 5. WO 훅 (TODO 보드와 상호참조)

- **WO-L1** json* 승격 (조건부 대기 — 보드 참조)
- **WO-L3** `std:` import 별칭 resolver
- **WO-L4** domain 모듈 doctrine-pass (sketch→active, 견인 순서 §4)
- 콤비네이터의 callable 반쪽(map/andThen)은 stdlib 소관이 아니라
  docs/141 Stage B + F1의 소관 — 그쪽이 닫히면 §3-1 아래에서 입고.

## Related

- docs/138 (scope ledger — 무엇을) · docs/12 (domain-lifecycle doctrine)
- docs/15 (capability sandbox — L2의 caps 의무 근거)
- docs/119 (계보 — C# CoreLib/BCL 경계) · docs/147 (per-type 근거 발견)
- TODO 보드 Stdlib-먹기 트랙
