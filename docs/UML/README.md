# Pergyra 아키텍처 UML

분류: **내비게이션 문서**. 이 폴더는 의미 규칙, 현재 진행 상태, SoT 폐쇄
여부를 소유하지 않는다. 도표와 실제 소스·registry·실행 gate가 다르면 실제
owner와 gate가 맞으며, 이 도표를 고친다.

이 폴더의 목적은 수정 전에 다음 세 가지를 빠르게 찾는 것이다.

1. 요청이 어느 실행 경로를 통과하는가.
2. 바꾸려는 사실의 owner와 마지막 소비자는 누구인가.
3. 어떤 우회 경로와 검증 경계까지 함께 확인해야 하는가.

현재 revision, blocker, gate 결과는 [Project truth](../truth/README.md)에서 본다.
의미 owner는 [SoT Owner Spine Registry](../semantics/sot_owner_spine_registry.md),
V1 종료선은
[V1 scope](../agent_work_directives/v1_scope_ownership_and_selfhost_generality_2026-10-14.md)가
소유한다.

## 도표 목록

| 문서 | 답하는 질문 |
|---|---|
| [01. 시스템 문맥](01_system_context.md) | 저장소 밖 입력부터 launcher, 두 컴파일러 경로, backend, runtime, 검증까지 어떻게 연결되는가? |
| [02. 네이티브 컴파일 파이프라인](02_native_compiler_pipeline.md) | C 구현 안에서 source·AST·semantic·IR·AIR·projection·backend가 어떤 순서로 흐르는가? |
| [03. Self-host world와 V1 교체 경로](03_self_host_world.md) | `PgyCompilerWorld`의 실제 실행 zone은 무엇이며 일반 emitter가 어떤 우회를 치환해야 하는가? |
| [04. 소유권 기반 자동 메모리 관리](04_ownership_automation.md) | 타입·호출·사용 사실이 move/copy/borrow/drop 결정과 C/LLVM cleanup으로 어떻게 이어지는가? |
| [05. SoT와 검증 폐쇄](05_sot_and_verification.md) | owner 이관을 언제 닫았다고 말할 수 있으며 문서·gate·CI는 어떤 역할을 하는가? |

## 표기 규칙

- **실선:** 현재 소스에 존재하는 호출 또는 사실 전달 경계.
- **점선:** 목표 경계, 검증 관계, 또는 제거 중인 전환 경계. 선의 라벨을 함께
  읽어야 한다.
- **`CURRENT`:** 현재 production 경로에서 도달하는 구성 요소.
- **`TARGET`:** 도달해야 하지만 존재만으로 진척으로 세면 안 되는 구성 요소.
- **`TRANSITION`:** 현재 존재하지만 V1에서 치환·삭제해야 하는 우회.
- **`PROOF`:** 특정 모델과 전제 아래의 증명. production 구현 완료를 뜻하지
  않는다.

색은 보조 표현일 뿐이다. 색 없이도 상태가 드러나도록 모든 전환 노드에는
텍스트 라벨을 붙인다.

## 수정할 때 읽는 순서

| 변경 종류 | 먼저 볼 UML | 실제 권위 |
|---|---|---|
| 문법·타입·진단 | 02 | parser/semantic source, [compiler contracts](../compiler/37_compiler_contracts.md) |
| 소유권·Clone·cleanup | 04 → 02 | [Ownership Clean](../semantics/27_ownership_clean.md), 도달한 typed/MIR owner, 실행 gate |
| MIR instruction·expression·ABI | 02 → 03 | MIR owner, ABI/runtime-call registry, 일반 C/LLVM emitter |
| self-host route·world·zone | 03 → 05 | `src/self_hosted/compiler/world.pgy`, [dogfood contract](../self_hosted/17_pergyra_native_dogfood_contract.md) |
| SoT 이관·fallback 삭제 | 05 | [SoT registry](../semantics/sot_owner_spine_registry.md), owner source, negative gate |
| 현재 blocker·CI 판단 | 05 | [truth/state](../truth/state/), 실행된 exact-revision gate |

## 유지 규칙

1. 도표에 현재 오류 수, 최신 SHA, `PASS/CLOSED` 상태를 복사하지 않는다. 해당
   truth/evidence에 링크한다.
2. 새 owner를 도표에서 먼저 만들지 않는다. 실제 owner가 착지한 뒤 구조를
   갱신한다.
3. 같은 사실을 두 상자에 넣지 않는다. producer와 consumer를 화살표로 잇는다.
4. 전환 경로가 삭제되면 점선을 제거한다. `old ? new` 양쪽을 정상 경로처럼
   남기지 않는다.
5. 새 diagram은 가능한 한 한 질문만 답한다. 거대한 한 장으로 다시 합치지
   않는다.

## 대조한 주요 원본

- [현재 네이티브 파이프라인 안내](../compiler/20_compiler_pipeline_guide.md)
- [IR 계층 계약](../compiler/36_ir_pipeline_architecture.md)
- [논리 spine과 handle/gate](../compiler/180_compiler_logical_spine_handles_gates.md)
- [Self-host substrate](../self_hosted/13_compiler_substrate_architecture.md)
- [Target compiler world](../self_hosted/14_target_compiler_world.md)
- [Ownership cleanup 알고리즘](../compiler/207_compiler_owned_cleanup_algorithm.md)
- [Ownership lifecycle 설명](../compiler/209_ownership_lifecycle_algorithm_code.md)

