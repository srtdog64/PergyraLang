# 셀프호스팅 잔여 봉합선 센서스 (읽기 전용 발견)

> 읽기 전용 분석. 컴파일러 의미·진척·레지스트리 상태의 소유자가 아니다.
> 권위 출처: `docs/semantics/sot_owner_spine_registry.md`,
> `docs/current_work_handoff.md`, AGENTS.md.

## 센서스

```
CLOSED=70  BRIDGE=23  ACTIVE=2
```

- `CLOSED`: 소비자 이전 + missing-fact 거부 + 옛 C 경로 삭제 + 네거티브 게이트 全.
- `BRIDGE`: ID·투영 안정적이나 구현이 C/typed-self-host로 분할(옛 경로 미삭제).
- `ACTIVE`: 이행 중인 rung.

## ACTIVE (2) — 현재 임계 경로

| id | 축 | 남은 falsifying case |
|---|---|---|
| `semantic.hashmap_collection_ownership` | semantic | move/clone, argument/return/inout, branch/multi-block exact cleanup, **UNKNOWN deep drop(owned String drop)**, **MapGet<String> borrow escape**, materialization/aggregate facts, collection_program_plan consolidation, **last C bypass removal** |
| `selfhost.semantic_artifact_admission` | semantic | body materialization, world/zone 생성·소멸, callable identity cross-seal; **general zone transfer/move open** |

## BRIDGE (23) — 파이프라인 단계별

### 1) 의미 기반층 (가장 먼저, 하위 전부의 의존) — 9행

- `semantic.resource_flow_universe` (resource)
- `semantic.symbol_type_graph` (semantic)
- `semantic.nominal_field_kind` (semantic)
- `semantic.domain_runtime_assignment` (semantic)
- `semantic.loop_flow_summary` (semantic)
- `semantic.generic_specialization` (semantic)
- `semantic.machine_layer_transition` (execution)
- `parser.syntax_provenance` (syntax)
- `hir.typed_control_flow` (semantic axis, HIR 소유)

### 2) IR 계층 (의미가 닫혀야 닫힘) — 4행

- `dir.domain_graph` (semantic)
- `rir.resource_transition_graph` (resource)
- `mir.execution_graph` (execution)
- `air.evidence_graph` (verification)

### 3) 투영·ABI·타깃·진단 (최하단) — 10행

- `projection.direct_mir_string_array_push` (projection)
- `projection.direct_mir_collection_pop_effect` (projection)
- `projection.direct_mir_array_int_program` (projection)
- `projection.direct_mir_collection_program_plan` (projection)
- `projection.direct_mir_scalar_cfg_program_extension` (projection)
- `abi.layout_rows` (abi)
- `abi.callable_receiver_carriage` (abi)
- `resource.region_allocation_plan` (resource)
- `target.capability_profile` (target)
- `diagnostic.catalog` (diagnostic)

## 임계 경로 판정

1. **지금(ACTIVE)**: `semantic.hashmap_collection_ownership` — CI가 `owned_string_drop`
   에서 실패 중. 여기가 병목. `selfhost.semantic_artifact_admission`이 병행 ACTIVE.
2. **그다음(의미 기반층)**: resource_flow_universe → symbol_type_graph →
   nominal_field_kind → domain_runtime_assignment → loop_flow_summary.
   이들이 CLOSED가 되어야 HIR/DIR/RIR/MIR/AIR가 닫힌다.
3. **그다음(IR)**: typed_control_flow → domain_graph → resource_transition_graph →
   execution_graph → evidence_graph.
4. **최하단(투영/ABI/타깃/진단)**: direct_mir_* 5행 + layout_rows +
   callable_receiver_carriage + region_allocation_plan + capability_profile +
   diagnostic.catalog.

**결론**: 임계 경로는 "넓은 기능"이 아니라 **의미 소유권·컬렉션 수명 계층**이다.
모든 하위 IR/투영이 이 위에 얹히므로, ACTIVE 2행 → 의미 기반층 9행 → IR 4행 →
투영/ABI/타깃 10행 순으로 봉합된다. 남은 구체 falsifier는 owned String deep drop,
move/clone, argument/return/inout, MapGet borrow escape, zone transfer/move,
그리고 각 행의 "last C bypass removal"이다.

## 한계

이 센서스는 레지스트리 상태(정적) 기준이며, 실제 실행 봉합 진척은
`docs/current_work_handoff.md`의 활성 rung 카드와 설치 드라이버/패리티 게이트가
소유한다.
