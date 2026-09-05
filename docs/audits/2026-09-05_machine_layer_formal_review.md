# Machine layer 형식 모델 대조 감사 — 2026-09-05

Status: READ-ONLY REVIEW COMPLETE; 증명 재실행·구현 수정·SoT 승격 없음.

Base revision: `f34355b37dbd9e86ef574399e895a78fd41dd0a3` (dirty shared tree).
대상은 사용자가 제공한 기계층 평가 첨부이며, 이 문서는 평가를 그대로 정전으로
승인하지 않는다. [작업 지시서](../agent_work_directives/machine_layer_documentation_review_2026-09-05.md)의
Formal reviewer 범위에서 정의·정리·기존 게이트를 읽었다. 아래 `파일:줄`은 이
체크포인트의 탐색 앵커이며, 문서가 해당 사실의 owner를 대체하지 않는다.

## 판정 요약

**주소 근거와 실제 접촉을 분리했다는 평가는 맞다.** 다만 확인되는 것은
선언에 조건부인 작은 형식 모델이다. `Grant` 소유, `Region` 생성, `Slot` 배치,
resource-to-machine binding, 실제 접촉 승인은 서로 같은 증거가 아니다.
모델의 조건이 현재 하드웨어에서 성립한다는 주장이나, 전체 언어·백엔드가
이 모델을 정제한다는 주장으로 승격할 수 없다.

| 첨부 주장 | 대조 판정과 정밀화 | 정의·정리 앵커 |
| --- | --- | --- |
| `Grant -> Region`은 주소·범위·출처를 표현한다 | 채택. `Grant` 자체가 현재 caller의 접근 권한은 아니다. `region_valid`는 선언된 grant와 provenance/mode/bounds의 일치를 뜻한다 | `docs/semantics/proofs/MachineLayerCore.v:99`, `:112`, `:176`, `:222` |
| `Region + TypeLayout -> place -> Slot` | 조건부 채택. `place`는 Plain/크기/alignment를 검사한다. 선언에 대한 `region_valid`는 함수 내부 검사가 아니라 `place_grounds_slot` 정리의 별도 전제다 | `MachineLayerCore.v:273`, `:293`, `:315` |
| volatile/atomic Region은 ordinary Slot으로 배치할 수 없다 | 채택. 정확히 이 모델의 plain-data `place`에 대한 거절이며, 모든 종류의 device handle 생성을 금지한다는 뜻은 아니다 | `MachineLayerCore.v:372`, `:376` |
| Slot은 typed resource identity다 | 배치 모델에 한정해 채택. 이 파일의 `Slot`은 type id/base/size/mode/provenance 레코드다. 전체 언어 Slot의 소유·generation·수명·API 비위조 보장이 이 레코드에 들어 있지는 않다 | `MachineLayerCore.v:279`, `:301`, `:336`, `:364` |
| contact에는 region, hardware adequacy, authority, live lease, mode가 필요하다 | 채택. 다섯 전제가 `contact_step` 생성자에 있으며 각각의 필요조건 정리가 있다. authority는 현재 config의 grant-id membership, lease는 그 grant-id의 Live 상태다 | `MachineLayerCore.v:464`, `:472`, `:475`, `:547`, `:624` |
| Read/Write/VolatileRead/VolatileWrite/AtomicRmw/Fence가 있다 | 모델의 여섯 연산군으로 채택. 현재 DeviceSlot의 Claim/Read/Write/Release/SubmitRead 다섯 API와 동일한 집합이 아니며, 여섯 연산의 사용자 표면 지원을 뜻하지 않는다 | `MachineLayerCore.v:432`; [구현 범위](../semantics/proofs/MachineLayerCore.md#implementation-status) |
| contact는 머신 상태를 바꾸고 사건을 남긴다 | 채택하되 추상 셀 의미로 한정. read는 기존 값을 관찰하고 write는 base 셀을 갱신한다. fence는 메모리를 보존하고 사건만 기록한다. 모든 접촉이 메모리 값을 바꾼다는 뜻은 아니다 | `MachineLayerCore.v:498`, `:501`, `:520`, `:691`, `:700`, `:716`, `:757` |
| hardware adequacy는 선언 fact이며 실리콘 측정이 아니다 | 채택. Rocq에서는 `Grant -> Prop`와 그 성립 증거가 선언에 포함된다. 구현의 bool 필드와 이 논리적 witness를 동일시할 수 없다 | `MachineLayerCore.v:138`, `:157`, `:207`, `:597` |
| resource와 machine 정보는 명시적 bridge로 결합한다 | 최소 모델로 채택. authority·양의 extent·정확한 resource/address/extent/mode binding을 요구한다. 상세 `contact_step`과의 합성·백엔드 정제를 증명한 파일은 아니다 | `docs/semantics/proofs/ResourceMachineBridge.v:44`, `:50`, `:53`, `:61`, `:71`, `:80`, `:89` |
| machine-neutral은 machine layer와 다르다 | 채택. 전자는 언어 fact의 target 비종속성, 후자는 주소·접근 모드·접촉의 경계다. 이를 AIR가 실행 IR로 내려가거나 backend가 AIR를 직접 읽는 선형 파이프라인으로 그리면 정전과 충돌한다 | `docs/semantics/18_machine_neutral_compute.md:36`, `:76`, `:152`, `:278` |

위 표의 짧은 파일명은 [MachineLayerCore.v](../semantics/proofs/MachineLayerCore.v)를
가리킨다. bridge owner는 [ResourceMachineBridge.v](../semantics/proofs/ResourceMachineBridge.v),
설명 owner는 [Machine Layer Core](../semantics/proofs/MachineLayerCore.md)와
[Machine-Neutral Compute Contract](../semantics/18_machine_neutral_compute.md)다.

## 정리에서 실제로 읽을 수 있는 보장

`grant_yields_valid_region`과 `carve_preserves_validity`는 선언된 grant 안에서
범위·출처·mode가 유지되는 조건부 정리다. `carve_disjoint`는 이미 서로 겹치지
않는 offset 전제를 요구한다. `placed_slots_disjoint`도 두 Region의 비중첩을
전제로 하므로, 모든 allocator나 모든 aliasing 상황의 안전성을 자동으로
얻는 정리가 아니다 (`MachineLayerCore.v:222`, `:238`, `:254`, `:400`).

`place_grounds_slot`과 `chain_grant_carve_place_grounded`는 유효한 grant 계보를
plain Slot까지 보존한다. 공개 `mkSlot` 생성자로 임의의 레코드를 만드는 것을
봉쇄하지 않는다. `no_wild_slot`도 `slot_grounded`를 전제한 projection이다.
현재 source/API의 비위조성까지 얻으려면 별도 compiler/runtime admission
연결이 필요하다 (`MachineLayerCore.v:301`, `:315`, `:350`, `:364`).

접촉의 긍정 경로는 `contact_step_constructible`과 `sample_plain_read_contact`에
있다. 부정 경로는 authority 부재, revoked lease, mode 불일치에 각각 이름이
있는 정리로 남아 있다. `contact_step_preserves_authority`와
`contact_step_preserves_lease`는 접촉 자체가 권한을 만들어 내거나 lease를
갱신하지 않음을 보장한다. 이 모델의 lease는 grant별 Live/Revoked 상태이지,
프로그램 전체의 temporal Slot/generation 체계를 합성한 증명이 아니다
(`MachineLayerCore.v:451`, `:557`, `:609`, `:675`, `:683`, `:768`, `:777`, `:788`).

## 반드시 남겨야 할 한계

- **hardware adequacy는 외부 조건이다.** `MachineDeclaration`은 unique id,
  non-overlap, address-space bound, hardware predicate와 증거를 담는다.
  `sample_declaration`은 그 predicate로 `fun _ => True`를 사용한다. 긍정 예제가
  있다는 사실은 모델의 구성 가능성을 보이지만 board/MMU의 진실을 관찰하지 않는다
  (`MachineLayerCore.v:138`, `:597`).
- **placement 검사는 grant 검증과 다르다.** `place`에는 machine declaration
  인자가 없고, `region_valid`는 정리 전제다. 따라서 "아무 Region도 자동으로
  안전한 Slot이 된다" 또는 "place 한 번이 전체 자원 admission이다"라고
  설명하면 안 된다 (`MachineLayerCore.v:293`, `:315`).
- **현재 접촉은 byte/버스 모델이 아니다.** 주소와 셀 값은 `nat`이고 write는
  `r_base` 한 셀을 갱신한다. `range_within`/`carve`는 0 크기 Region을 별도로
  배제하지 않으며 `contact_step`에도 nonempty/access-width/alignment 전제가
  없다. 이는 정의를 읽어 확인한 범위 제한으로, 이번에 실행·기계 확인한 새
  반례가 아니다. 실제 접근 폭, 유한 주소 overflow, register-width, empty span의
  접촉 admissibility를 증명했다고 표현할 수 없다 (`MachineLayerCore.v:81`,
  `:120`, `:233`, `:464`, `:498`, `:547`).
- **AtomicRmw/Fence 이름이 hardware ordering 증명은 아니다.** 이 RMW는 옛 값을
  읽기 기록에 넣고 주어진 새 값을 쓰는 한 추상 transition이다. arbitrary RMW
  함수, compare-exchange, interleaving 또는 C11 ordering을 모델링하지 않는다.
  Fence는 Atomic mode에서만 허용되고 사건을 기록하며 메모리를 보존한다
  (`MachineLayerCore.v:440`, `:537`, `:738`, `:757`).
- **ResourceMachineBridge는 전체 합성 정리가 아니다.** `MachineWitness`는
  `machine_extent > 0`뿐이다. `MachineLayerCore.v`를 import하지 않으며
  `MachinePlacement`를 Region으로 정제하는 함수나 `GroundedContact`에서
  `contact_step`을 얻는 정리가 없다. 두 non-inference 정리는 같은 resource와
  다른 주소, 같은 주소와 다른 principal의 구체 예로 비유일성을 보인다.
  암호학적 비위조성이나 universal noninterference 정리가 아니다
  (`ResourceMachineBridge.v:17`, `:50`, `:98`, `:102`, `:110`, `:114`).
- **문서의 IR 화살표는 소유·검증 관계로 읽어야 한다.** AIR는 verification과
  evidence를 소유하고 backend는 검증된 projection을 소비한다. machine fact는
  이미 RIR/MIR와 plan에 있으므로 machine layer를 "backend 출력 다음에 처음
  등장하는 단계"로 고정하는 것도 부정확하다. 비 CPU projection은 지원 목록이
  아니라 계약이다 (`18_machine_neutral_compute.md:36`, `:76`, `:152`, `:264`).

이 구분은 [Architecture Boundary Cores](../semantics/proofs/ArchitectureBoundaryCores.md):11의
owner 표와 일치한다. 모델 정합성, compiler 구현 적합성, 실제 hardware 적합성은
서로 대체하지 않는다. 이를 새 구현 트랙이나 지금 닫아야 할 작업 큐로 승격하지 않았다.

## 기존 검증 게이트와 이번 실행 범위

| 기존 게이트 | 읽어 확인한 책임 | 이번 감사 |
| --- | --- | --- |
| [machine_layer_core_smoke.sh](../../tests/machine_layer_core_smoke.sh):16, :63 | 주요 정의·정리 및 과장 금지 문구, 금지된 legacy 이름을 검사한 뒤 MachineLayerCore를 compile한다. prover 부재는 기본 실패이고 explicit skip은 미검증이라고 출력한다 | 소스만 읽음; 미실행 |
| [formal_semantics_smoke.sh](../../tests/formal_semantics_smoke.sh):581, :1038, :1142 | ResourceMachineBridge의 계약 inventory와 두 파일을 포함한 proof corpus의 등록·compile 경로를 검사한다 | 소스만 읽음; 미실행 |
| [coq_kernel_check.sh](../../tests/coq_kernel_check.sh):28, :103, :122, :166 | 현재 `.v` corpus를 compile한 뒤 그 module만 kernel-check하고 axiom budget을 비교한다. corpus 허용치는 SlotCalculus의 두 명시적 abstraction이며 전체 0-axiom 주장이 아니다 | 소스만 읽음; 미실행 |
| [coq_kernel_check_selftest.sh](../../tests/coq_kernel_check_selftest.sh):46, :58, :79 | clean control은 수용하고 planted Admitted는 axiom-budget 오류와 해당 이름으로 거절하는지 검사한다 | 소스만 읽음; 미실행 |
| [ci.yml](../../.github/workflows/ci.yml):228, :244, :252 | 고정 Rocq 9.0.1 job이 kernel check와 negative self-test를 호출한다 | 설정만 읽음; 원격 run 결과는 이번 감사에서 확인하지 않음 |

두 대상 `.v`의 현재 텍스트에는 `Axiom`, `Parameter`, `Admitted` 선언이나 `admit`
명령을 추가한 흔적이 없고 정리마다 proof term/script와 `Qed`가 있다. 이는
읽기 확인일 뿐 새 compile/kernel-check 결과가 아니다. 기존 `.vo` 등 산출물의
존재도 현재 소스와 prover에 대한 증명 실행 영수증으로 사용하지 않았다.
컴파일·테스트·하드웨어 접촉은 전혀 실행하지 않았으며, 이 감사만으로 CI green,
self-host substitution, SoT closure 또는 hardware refinement를 선언하지 않는다.

## Intent 정의에 연결할 문단 제안

Intent가 보존하는 것은 여러 fact의 목적 귀속이고, machine operation admission이
판정하는 것은 그 목적에 귀속된 접촉을 실제로 허용할 증거다. 따라서 Intent
identity나 주소의 존재만으로 grant authority, live lease, mode 적합성,
hardware declaration, resource-to-machine binding을 만들어서는 안 된다.
각 fact의 owner는 분리한 채 필요한 귀속과 binding을 마지막 admission consumer까지
보존하고, 이후 증거는 해당 owner의 계약에 따라 압축·소거한다. 이는 모든 기계
연산을 Intent로 감싸라는 규칙이 아니며, Intent가 하드웨어 권한의 owner라는
뜻도 아니다. [Intent의 정적 identity](../01_intent_first_design.md#intent의-정적-identity)와
현재 두 machine/resource 모델은 이 설명에 양립하지만, 두 `.v`에는 Intent identity가
등장하지 않으므로 이 연결 자체가 이미 기계 증명됐다고 쓰면 안 된다.
