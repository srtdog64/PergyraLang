# Slot · generation · Arena/Region · ABI 통합 및 부족분 점검

관측 기준: 2026-09-16, 게시 HEAD `ebc9f28720c77ffee6b9773bb565a2effa218879`.
작업 트리는 다른 세션의 `Slice<T>` 코드/테스트와 감사·handoff 편집으로 이미
dirty였다. 이 문서는 그 변경의 검증이나 SoT 상태 승격이 아니다. 아래 실행 결과는
이 작업 트리의 설치 바이너리와 소스에 대한 **좁은 검사**이고, 게시 HEAD 전체
CI 결과와 구별한다. 의미 권위는 베타 계약, 각 fact owner, SoT 레지스트리와 실행
게이트에 있다.

## 판정

**합성의 방향은 타당하지만, 전체 모델의 구현은 부분적이다.** Arena가 저장과
일괄 회수, handle/generation이 재사용 뒤 시간적 유효성, capability/token이 접근
권한, ABI가 C·LLVM·런타임 사이의 물리적 의미를 각각 맡는다. `Slot`은 이들을
모두 같은 C 구조체로 만드는 장치가 아니라, 자원 경계의 소스 계약이다.
일반 값 계산에는 Slot 수명 의식을 요구하지 않는다.

Vale의 [generational reference 설명](https://vale.dev/guide/references)은
소유/비소유 참조와 생존 검사에 관한 **참고 모델**이다. Pergyra가 Vale 전체
메모리 모델이나 모든 참조에 대한 Vale급 보장을 구현했다는 근거가 아니다.
Vale의 [세대 참조 전망](https://vale.dev/vision/safety-generational-references)은
2021년 설계/전망 글이므로 현재 성능이나 구현 상태의 증거로 사용하지 않는다.

이 감사의 목적 카드:

- 목표: 서로 다른 저장·유효성·권한·물리 표현 층을 혼동하지 않고, 구현된 범위와
  끝나지 않은 소비자 이전을 반례에 묶는다.
- 우선순위: 정확한 owner/identity → 실행 경로 → 옛 읽기와 무음 폴백 제거 →
  음성 게이트 → 비용 측정. 문서·키워드·row 수는 대체 진척이 아니다.
- fact owner/마지막 소비자: compiler arena는 `src/common/arena.*`와 각 pass의
  release point, Slot handle은 `src/runtime/slot_manager*`, 물리 배치는
  `src/runtime/pgy_abi_spec.h`에서 MIR ABI row로 검증되어 C/LLVM 소비자에게
  전달된다. Region 선택은 검증된 escape/region plan 뒤 backend가 소비한다.
- 금지 폴백: `Slot<T>`의 `occupied`를 generation으로 간주, Arena index를
  reset 뒤 자동 유효하다고 간주, backend에서 ABI 배치를 재구성, Zone의 `ref`/`own`
  파라미터를 spawn worker 전송 허가로 간주, 미증명 Region 할당을 무음 승인.
- 당시 다음 반례는 서로 다른 actor·authority와 두 subject slot을 갖는 생산
  Intent였다. 같은 작업 트리의 후속 closure packet이 설치된 source/MIR-to-C와
  direct MIR-to-C를 실행하고, 누락·드리프트·비-Zone authority row를 거절하며
  옛 self-C binding owner를 삭제했다. 이는 Zone authority row의 폐쇄 증거이지
  새 ABI/Arena 구현 트랙이나 양성 spawn-worker handoff 증거가 아니다.

## 무엇이 실제로 서로 다른가

| 층 | 현재 owner/표현 | 확인된 범위 | 잘못된 동치 |
| --- | --- | --- | --- |
| 컴파일러 `PgyArena` | `src/common/arena.h`, `arena.c`; named ledger의 owner·last consumer·release point | 일괄 할당/파괴와 copy·peak 계측. HIR/MIR scratch 등에서 사용 | Arena pointer가 다른 pass/result/cache까지 자동 유효함 |
| 고정 용량 런타임 `PgyArena` | `src/runtime/pgy_runtime_memory_array_slot_inline.h`; ABI `{buffer,capacity,offset}` | legacy runtime helper용 계열과 고정 ABI row | 아래의 성장형 `PgyRegion` 또는 Zone Arena와 같은 실행 owner |
| 성장형 `PgyRegion` | `src/runtime/pgy_runtime_region_inline.h`, `src/compiler/verified_region_plan.c` | WO-REG-1: 인증된 문자열 임시값의 C/LLVM region 실행, heap 경로와 구별 | 모든 배열·spawn·channel 할당이 region-backed임 |
| 일반 `Slot<T>` 값 ABI | `src/runtime/pgy_abi_spec.h`, plain-slot 런타임; `{value,occupied}` + padding | build mode와 무관한 checked 배치 | 모든 `Slot<T>` 값에 세대 필드가 있음 |
| 테이블형 `SlotHandle` | `src/runtime/slot_manager.h`, `slot_manager_core_ops.c`; 32-bit slot ID + 32-bit generation + type/token/pin 상태 | 재사용 시 세대 증가, stale/ID 소진 거절 | Vale의 모든 소유·비소유 참조를 대체한 범용 generational reference |
| MIR ABI 배치·호출 row | `src/compiler/mir_abi_layout.c`, runtime ABI spec/asserts, runtime-call row owner | bounded scalar/Option/Array/nominal·resource 소비자 | 모든 target/nominal/generic/외부 ABI 소비자가 이전됨 |

ABI의 `Arena` row는 **고정 용량 런타임 형상**을 핀한다. 컴파일러
`PgyArena` ledger나 성장형 `PgyRegion`을 같은 24-byte 구조로 합친다는
뜻이 아니다. `PgyRegion`의 head record는 별도 ABI mirror를 가진다.
Zone-channel Arena 설계는 ABI spec 주석에 남아 있지만, 현재 channel
런타임의 실제 Arena 소비자로 판정하지 않는다.

소스에서 자원 경계를 선택한 뒤에는 두 실행 길이 다르다:

```text
ordinary value → Semantic/MIR → C 또는 LLVM                (Slot 의식 없음)
anchored resource → own/ref·CFG 검사 → Slot/Token 경계 → MIR ABI row
                  → backend → runtime handle generation/token/pin 검사
certified temporary → escape + AIR 검증 → sealed RegionPlan → C/LLVM Region
uncertified temporary → 현재 명시된 heap 경로; Region 보장이라고 보고하지 않음
```

`SlotHandle`의 세대 검사와 정적 borrow 안전성은 **다른 명제**다. 세대 재사용
검사는 런타임에 도달한 오래된 handle을 거절한다. 임의 `ref`의 배타성,
모든 CFG 종료의 release/unpin 정확성, task/channel escape는 정적
ownership·body dataflow가 별도로 증명하거나 해당 베타 범위 밖에서 거절해야
한다. 좁은 Rocq/Coq Slot 모델과 API 대응 게이트도 전체 언어 증명은 아니다.

## `arena`/`region` 키워드 삭제 테스트 — 2026-09-16

현재 소스에 이 두 **사용자 키워드는 없다**. `src/lexer/language_keyword_registry.def`,
native lexer/parser, self-host parser에서 해당 spelling/token selector 검색은
0건이었다. 따라서 현재 키워드 spelling을 삭제해 보는 실험은 판정력을 갖지
못한다. 대신 아래처럼 **표기 삭제**, **현재 실행 경로의 대체**, **미래의 명시적
계약 삭제**를 따로 판정했다. 미래 계약은 아직 코드가 없으므로 실행 결과라고
적지 않는다.

| 삭제/대체 대상 | 이번에 관측한 반례·대조 | 판정 |
| --- | --- | --- |
| `region` source spelling | registry/parser에 없음. `region_backend_wiring_smoke.sh`가 키워드 없이 인증된 `Print("a"+"b")`와 직접 callee 임시값을 Region으로 내린다. | **spelling removable**: 현재 자동 임시값 경로에 키워드 불필요. |
| `arena` source spelling | registry/parser에 없음. `examples/battle_simulator/main.pgy`의 `zone arena`/`let arena`, `src/self_hosted/parser/expression_fact_owner.pgy`의 `arena:` field처럼 기존 `.pgy`에서 이름으로 쓰인다. | **spelling removable**: 현재 의미 손실 없음. 새 hard keyword로 예약하면 기존 소스와 self-host owner 이름을 깨므로 근거가 더 필요. |
| 인증된 RegionPlan 대신 기존 heap 할당 | 같은 `"a"+"b"`를 직접 `Print`하는 fixture는 Region call, 이름 붙인 `let value` 뒤 `Print`하는 fixture는 `StringConcat` heap call을 C/LLVM에서 만든다. gate는 두 경로의 배치를 검사했고 Region runtime smoke는 region concat·destroy·budget을 실제 실행했다. | 배치 전략은 분명히 달라진다. 단일 값의 **behavior replaceable**은 소스/방출 코드로부터의 추론이며 이번 A/B stdout 실행 판정은 **unverified**. 반복 workload의 peak-live·누수·처리량에 **no same-guarantee replacement**인지도 아직 검증되지 않았다. |
| 미래 `region frame { ... }` 선언 | 현재 구현·fixture가 없다. 설계는 *특정 회수 범위를 요구하고*, escape/증거 누락을 진단하며, 명시적 예산을 그 범위에 귀속시키려 한다. 자동 Region 추론은 불확실한 site를 heap으로 둔다. | **unverified**, 다만 독립적인 사용자 계약 후보. `scope`+기존 budget만으로 site별 Region 귀속·escape 거절이 동일하게 표현되는지 실사례 3종과 양 경로 음성 게이트 전에는 “대체 불가”라 하지 않는다. |
| 미래 `arena` hard keyword | allocator 구현 선택을 일반 소스에 드러내는 것 외의 새 정적 명제가 아직 제시되지 않았다. `docs/197` §2는 raw allocator-handle surface를 금한다. | **spelling removable / 도입 유보**. 물리 저장 선택이 진짜 FFI·예측 가능 용량·loss 경계라면 `Arena` API/타입이나 별도 explicit boundary를 검토할 수 있지만 일반 키워드의 근거는 아님. |

`region`이 추가된다면 named reclamation scope는 **Zone/Subject의 고정
생명주기**가 아니다. 그 블록의 익명 임시 저장만 소유하며 Slot/Zone identity는
계속 별도 owner다. 다만 `region`이 단순히 “가능한 곳에 최적화해 줘”라면
이미 있는 유도된 RegionPlan과 중복이다. 문법을 정당화하려면 선언 후에는
불확실한 site/escape를 어떤 diagnostic으로 거절할지, 허용 heap site가 있다면
한도 밖 비용을 어떻게 드러낼지, budget과 기존 `scope`로는 표현할 수 없는
정확한 계약을 먼저 고정해야 한다. 아직 WO-REG-3 구현을 시작할 근거는 없다.

임시 A/B 실행 파일을 만드는 별도 진단 명령은 접근 정책에 거부되어 실행하지
못했다. 정책 우회나 미관측 stdout/peak-live 결과의 추정 없이, 이미 실행한
`region_backend_wiring_smoke.sh`의 C/LLVM 배치 비교와
`region_arena_smoke.sh`의 런타임 범위까지만 삭제 테스트의 실행 증거로 쓴다.

## 이번에 실행한 좁은 검증

MSYS2 UCRT64 bash에서 아래 스크립트를 직접 실행해 **각 종료 코드 0과 출력**을
관측했다. 다른 세션의 dirty 변경을 보존하기 위해 clean rebuild를 수행하는
`tests/verify_arena_closure.sh`는 실행하지 않았다.

| 게이트 | 관측 결과 | 증명하지 않는 것 |
| --- | --- | --- |
| `tests/arena_ledger_smoke.sh` | PASS: owner·consumer·release·copy·peak 필드 | 모든 pass의 pointer escape 부재, 전체 메모리 비용 |
| `tests/abi_ownership_shape_smoke.sh` | PASS: Slot/Pin ABI 형상·cleanup source ratchet | 새 빌드의 전체 ABI 실행 파리티 |
| `tests/slot_calculus_adequacy_smoke.sh` | PASS: 모델 명제/연산명 ↔ runtime API 구조 대응 | 이 실행에서 Rocq/Coq proof 재검사 또는 arbitrary CFG 안전성 |
| `tests/security_portability_contract_smoke.sh` | PASS: ID/세대 소진·보안 계약 source ratchet | 이 실행에서 stale handle 동적 corpus 전체 재실행 |
| `tests/region_arena_smoke.sh` | PASS: 성장·정렬·reset·concat·예산 거절, inline/extern 실행 일치 | 전체 allocation의 Region 채택 |
| `tests/region_backend_wiring_smoke.sh` | PASS: native C/LLVM의 인증 경로와 비인증 heap 경로 구별 | 설치 self-host 전체 source 경로의 Region 치환 |
| `tests/runtime_abi_lifetime_smoke.sh` | PASS: borrowed export/result/file handle source contract | 전체 FFI ownership 또는 external target ABI |

게시 HEAD의 30/30 full-CI 성공은
[실행 기록](https://github.com/srtdog64/PergyraLang/actions/runs/34981095622)에
있다. 그 성공은 이 dirty 소스, 아래 BRIDGE row, 별도 RED인
`loop_summary_kind` 직접-MIR 변조 음성 게이트를 폐쇄하지 않는다.

## 부족분과 다음 반례

판정 어휘: **BRIDGE**는 옛/넓은 소비자가 남은 SoT row, **BOUNDED**는
실행 가능한 일부 입력만 확인, **DEFERRED**는 의도적으로 베타 밖,
**UNVERIFIED**는 이 감사에서 필요한 광범위 증거를 확인하지 못함이다.
`DEFERRED`를 베타 결함이나 자동 우선순위로 바꾸지 않는다.

| 축·현재 판정 | 부족한 계약/사실 | 다음 falsifier 또는 폐쇄 조건 |
| --- | --- | --- |
| `abi.layout_rows` **BRIDGE** | 고정 row와 bounded nominal/Array/Option 소비는 있지만 pointer-bearing, unknown/target-dependent, 넓은 nested wrapper/general generic 및 외부 target profile이 열린다. | 실제 target을 달리한 필수 배치 row/ID 변조가 설치 self-host·C·LLVM에서 artifact 없이 거절되고 모든 마지막 소비자가 row만 읽는다. |
| `abi.runtime_call_rows` **BRIDGE** | Claim 등 여러 row 소비자는 이전됐으나 constructed nominal materialization과 C/LLVM/self-host 전체 호환 소비자가 남는다. | 존재하지 않거나 call shape가 다른 runtime row를 주입해 세 경로가 같은 owner 진단으로 거절; constructed spelling 재생성 경로 삭제. |
| anchored own/ref·Slot 정적 안전성 **BOUNDED** | 일반 alias/exclusivity, arbitrary CFG no-escape/no-suspend, 모든 종료의 release/unpin exactly once가 전체 언어 정리로 닫히지 않았다. | pin/view escape, await 뒤 stale borrow, 성공·실패·cancel 갈래에서 cleanup 누락/중복을 베타 허용 소스가 거절 또는 정확히 정리. |
| compiler Arena 경계 **UNVERIFIED** | ledger와 HIR/MIR scratch 구현은 확인했지만 모든 scratch/result/cache의 cross-arena pointer 생존을 이번 검사로 증명하지 않았다. | scratch reset 후 result/diagnostic/cache 소비가 이전 pointer나 재사용 index를 읽지 못하도록 owner identity·lifetime 음성 게이트와 고정 규모 계측. |
| runtime Region **BOUNDED** | WO-REG-1 인증 문자열 class만 live. spawn pack, Array 임시값, interpolation 및 Zone-channel 저장은 일반화되지 않았다. | 각 추가 class에서 escape/cancel/예산 실패가 artifact/누수/무음 heap demotion 없이 처리됨; 실제 비용을 기존 heap 및 free-per-temp 기준과 비교. |
| Zone authority transition **CLOSED** / worker handoff **BRIDGE** | 생산 Intent의 distinct actor·authority·두 slot은 exact MIR transition과 shared Zone-sync owner를 거쳐 self/native C/LLVM에서 실행되고 legacy binding은 삭제됐다. 별도의 양성 spawn-worker resource handoff는 아직 없다. | authority row는 새 missing/drift/non-Zone 변조 게이트를 유지한다. worker handoff는 실제 channel/result/copy 경계와 취소·수명 반례를 별도 실행해야 한다. |
| 사용자 `region`/`arena` 문법 **DEFERRED** | WO-REG-3 source surface는 아직 없다. 현재 spelling 삭제 테스트는 둘 다 의미 손실 0; 명시적 `region`의 추가 보장만 미검증 후보다. | 베타 이후 site별 Region 귀속·escape refusal·budget이 기존 `scope`와 유도 plan으로 대체되지 않는 실제 사례를 먼저 확인. `arena`는 일반 hard keyword로 예약하지 않음. |
| raw escape·explicit layout·niche **DEFERRED** | `SlotRawPointer`와 일반 source-level packed/field-offset/union/niche layout은 구현하지 않고 명시적으로 막는다. | scoped FFI/raw 권한과 `LayoutFact`가 들어온 이후에만 `sizeof`/`offsetof`, endian, C/LLVM/외부 ABI 및 negative gate. |
| 64-bit generation 확대 **DEFERRED** | 현재 테이블은 32-bit ID + 32-bit generation이며 소진을 OOM과 구별해 거절한다. | ABI 확대가 실제 workload로 정당화될 때 runtime·ABI spec·MIR·C/LLVM·self-host를 한 버전/identity로 이전; 32-bit wrap을 묵살하지 않음. |
| 비용/보장 결합 **UNVERIFIED** | ledger와 좁은 Region 경로는 실재하지만 이 감사는 현재 소스의 대형 프로젝트 RSS·세대 검사 hot-loop 비용·backend별 Region 비용을 재측정하지 않았다. | 고정 프로그램을 동일 source/target/입력에서 반복 측정하고 peak-live, cross-stage copies, 세대 검사 수, 실패 class와 출력 동등을 함께 기록. |

`abi.mir_option_match_layout_admission`처럼 **CLOSED인 좁은 row**는
`abi.layout_rows` 전체 BRIDGE와 양립한다. `docs/semantics/13`의 문서 상태
`ACTIVE`도 SoT row 수를 뜻하지 않는다. 이 감사에서 SoT registry의 전체
상태는 CLOSED 56 / BRIDGE 31 / ACTIVE 2로 재검증했다. `Slot<T>` ABI의
단일 배치가 녹색인 것과 전체 resource/target ABI consumer 폐쇄는 별개다.

## 완료 판정과 작업 순서

“개념을 합쳤다”의 **현재 사실**은 각 층의 owner와 bounded 실행 계약이
존재한다는 것까지다. “Vale급 전체 세대 참조 안전성”, “모든 Arena/index가
자동 stale-safe”, “모든 ABI/FFI 배치가 닫힘”, “언어 전체가 정적으로 memory
safe”는 현재 주장이 아니다.

사용자가 정한 **언어 완성**의 필수 문턱은 모든 SoT row가 실행 증거와 함께
`CLOSED`인 것: 마지막 소비자 이전, 누락 fact 실패, 옛 경로 삭제, 음성 게이트.
그 뒤에도 베타 subset의 공개/native 수용·거절·관측 실행 의미와 외부 프로그램
증거가 필요하다. ABI/Arena 아이디어의 존재나 CI 녹색은 이를 대신하지 않는다.

`selfhost.zone_authority_rows`의 도달한 실행 rung은 이번 closure packet에서
닫혔다. 후속 self-host rung은 이 감사 문서가 임의로 열지 않으며, 현재 생산
실행에서 도달한 BRIDGE를 다시 고른다. 이 표를 독립 ABI/Arena 구현 backlog로
진행하지 않는다. 새 문법·64-bit handle·Region 확대는 베타 핵심 폐쇄 뒤 실제
workload가 요구할 때 재판정한다.

주요 권위: `docs/107_beta_stable_subset.md`, `docs/semantics/08_slot_capability_calculus.md`,
`docs/semantics/04_ownership_abi.md`, `docs/semantics/13_slot_abi_single_owner.md`,
`docs/94_arena_index_lifetime_plan.md`, `docs/197_region_arena_strategy.md`,
`docs/136_abi_niche_and_explicit_layout.md`,
`docs/semantics/sot_owner_spine_registry.md`, `docs/current_work_handoff.md`.
