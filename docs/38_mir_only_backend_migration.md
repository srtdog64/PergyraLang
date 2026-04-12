# MIR-Only Backend Migration Plan

마지막 업데이트: 2026-04-12

이 문서는 Pergyra backend를 다음 단방향 파이프라인으로 고정하기 위한 이행 계획을 정의한다.

`HIR -> DIR -> RIR -> MIR -> backend(C / LLVM)`

핵심 목표는 두 가지다.

- backend가 더 이상 `HIR`를 직접 codegen 입력으로 사용하지 않게 한다
- `C`와 `LLVM`이 동일한 `MIR` 계약과 동일한 ABI 계약 위에서 동작하게 한다

이 문서는 구현 진행 순서를 고정하는 문서다.
완료 여부를 보고하는 문서가 아니다.

## 1. 최종 상태

최종 상태에서 지켜야 할 규칙은 아래와 같다.

- `HIR`는 frontend 의미 보존과 diagnostics metadata의 근거다
- `DIR`는 declaration/domain contract의 근거다
- `RIR`는 resource/authority/projection/intent-policy 의미의 근거다
- `MIR`는 backend가 직접 읽는 유일한 executable IR이다
- backend는 `HIR` AST shape를 보고 분기하지 않는다
- backend는 `HIR` fallback path를 가지지 않는다
- `main` wrapper, `intent`, class method, cleanup/rollback/invalidation 모두 MIR routine/metadata로 표현된다

한 줄로 고정하면:

> backend는 MIR를 emit하고, HIR는 backend metadata lookup에만 보조적으로 참조된다.

## 2. 현재 상태 요약

이미 된 것:

- driver는 `HIR -> DIR -> RIR -> MIR` 순서로 lowering한다
- `CompilerIRBundle`은 `hir/dir/rir/mir`를 모두 보관한다
- C backend는 `transpile_with_mir(...)` 경로를 이미 사용한다
- LLVM backend도 `llvm_codegen_*_with_mir(...)` 진입점을 이미 가진다
- MIR validation / topology validation / SSA mapping validation이 들어가 있다

아직 남은 것:

- declaration inventory가 아직 dedicated declaration IR가 아니라 AST-carried inventory다
- `intent` emission은 step/check/eval/meta carrier를 MIR 우선 source로 읽고 누락 시 hard error로 실패한다. 남은 debt는 declaration inventory와 ABI metadata 실사용 쪽이다.
- `main` wrapper / top-level scheduling metadata는 더 MIR 중심으로 밀 여지가 있다
- 일부 declaration registration / naming helper가 AST-carried shape를 소비한다
- transpiler는 ABI metadata를 직접 소비하는 경로가 약하다

### 2.1 현재 확인된 LLVM fallback map

현재 `llvm_backend`에서 확인된 fallback은 아래와 같다.

이미 제거된 것:

- ordinary function body fallback
- intent body fallback
- class/enum method body fallback
- MIR path의 missing routine/carrier silent fallback

현재 남은 debt:

- declaration inventory는 AST-carried top-level inventory를 읽는다
- nominal registration은 MIR inventory를 직접 읽지만 dedicated declaration IR는 아니다
- `main` wrapper / top-level executable contract는 더 MIR entry metadata 중심으로 밀 수 있다
- ABI metadata 실사용은 C transpiler 쪽이 더 약하다

이 목록은 더 줄어들어야 하며, routine-body fallback 항목이 다시 생기면 regression으로 본다.

규칙:

- 이 목록에 새 항목이 추가되면 migration 실패로 본다
- 기존 항목은 줄어들어야지, 늘어나면 안 된다

## 3. 우선순위

### Phase 1. 정책 고정

먼저 고정할 규칙:

- 새 backend 기능은 `HIR direct emission`으로 추가하지 않는다
- 기존 HIR fallback은 제거 대상이며 임시 경로로만 취급한다
- MIR에 없는 의미를 backend에서 복구하지 않고 MIR/RIR/HIR lowering 쪽으로 올려서 해결한다

완료 기준:

- 문서와 코드 주석에 `HIR fallback is temporary`가 명시된다
- 신규 기능 PR은 MIR path를 먼저 요구한다

### Phase 2. LLVM fallback inventory

가장 먼저 해야 할 실무 작업:

- LLVM에서 HIR fallback 지점을 전부 목록화한다
- 각 fallback을 아래 카테고리로 분류한다

카테고리:

- `ordinary function`
- `intent`
- `class method`
- `main wrapper / top-level executable`
- `type materialization / helper declaration`

완료 기준:

- 각 fallback 위치와 제거 조건이 문서/이슈로 정리된다

### Phase 3. MIR routine coverage 확장

MIR가 backend 유일 입력이 되려면 아래를 다 표현해야 한다.

- ordinary function body
- intent body
- cleanup / rollback / invalidation exceptional blocks
- class method body
- top-level executable scheduling
- main entry wrapper contract

완료 기준:

- LLVM에서 더 이상 “MIR가 비어서 HIR로 fallback”하는 루틴이 없어야 한다

### Phase 4. LLVM emission을 MIR-only로 전환

해야 할 일:

- `llvm_emit_program_from_mir(...)`에서 함수 fallback 제거
- `intent`를 MIR routine 기반으로 emit
- class method emission도 MIR routine lookup 기반으로 전환
- `main` wrapper가 HIR executable list 대신 MIR entry metadata를 읽게 전환

현재 상태:

- compiler mainline은 `llvm_codegen_from_mir(...)` / `llvm_codegen_to_object_from_mir(...)`를 사용한다
- MIR 누락 시 routine/intent/method path는 fallback이 아니라 hard error로 실패한다
- nominal registration은 MIR inventory와 legacy inventory가 같은 helper를 공유한다
- transpiler의 MIR resource-op / DEF dumb emitter는 layout 누락 시 ABI table(`mir_abi_lookup`)을 다시 조회한다
- `MIRProgram.has_main_function`은 이제 lowering 시 MIR function inventory를 직접 스캔해 채워진다
- `MIRProgram.has_top_level_exec`도 MIR function inventory에서 직접 파생된다
- top-level wrapper AST는 `MIRProgram`에 별도 저장하지 않고, 필요 시 MIR function inventory에서 `__pgy_top_level_exec`를 다시 찾는다
- MIR backend main-wrapper path는 top-level statement list를 직접 순회하지 않고 `__pgy_top_level_exec`를 top-level execution source of truth로 사용한다
- LLVM/C main-wrapper와 thread-pool 판단의 MIR path도 raw `executables` list를 직접 소비하지 않는다
- C declaration emit의 role/party include 및 ability-vtable naming 경로에서도 `UnknownAbility`/주석 fallback 대신 backend error로 실패시키기 시작했다
- C let/MIR-local 선언 경로도 이제 타입을 모를 때 `int32_t`로 조용히 대체하지 않고 backend error로 실패한다
- MIR resource-op dumb emitter helper도 unknown op를 주석으로 흘려보내지 않고 실패 신호를 반환한다
- MIR function block emission 실패도 더 이상 주석 코멘트로 남지 않고 backend error로 전파된다
- slot sugar / 일부 collection helper도 payload/element type을 모를 때 `Int`/`int32_t`로 덮지 않고 backend error로 실패시키기 시작했다
- ordinary function / extern / class / enum / intent declaration signature 경로도 unresolved type을 `int32_t`로 덮지 않고 backend error로 실패시킨다

남은 완료 기준:

- declaration/top-level inventory도 dedicated declaration IR 또는 동등한 MIR-owned metadata로 더 밀어낸다
- `main` wrapper metadata를 MIR entry contract로 더 정리한다

### Phase 5. C transpiler ABI 연계 강화

해야 할 일:

- transpiler가 문자열 기반 타입 추정보다 `MIRInstruction.type_layout`를 우선 사용하도록 전환
- resource/cleanup hook을 trace-only가 아니라 ABI-aware dispatch 경로로 준비
- block mapping / predecessor / successor / cleanup edge 일관성 검사를 더 강하게 건다

완료 기준:

- transpiler가 `mir_abi_lookup()` 또는 그 결과 metadata를 실질적으로 사용한다
- ABI 불일치가 backend에서 조용히 통과하지 않는다

### Phase 6. 계약 강제 CI

해야 할 일:

- `LLVM MIR-only` 테스트군 추가
- `HIR fallback hit`를 실패로 만드는 assertion 추가
- backend parity test에서 C/LLVM이 동일 MIR dump를 소비하게 고정

완료 기준:

- CI에서 MIR 누락 루틴이나 fallback 사용이 바로 실패한다

## 4. 첫 번째 작업 묶음

지금 바로 시작할 우선 작업은 아래 네 개다.

1. declaration inventory AST-carried debt 목록화
2. `main wrapper`가 요구하는 metadata를 MIR 쪽에 더 밀기
3. transpiler ABI metadata 실사용 경로 강화
4. declaration-side helper의 AST naming 의존 정리

routine-body fallback 제거는 이미 끝났고, 이제는 declaration-side debt를 줄이는 단계다.

## 5. 절대 하지 말아야 할 것

- LLVM에서 routine-body fallback을 다시 도입한 채 새 기능을 계속 추가하는 것
- backend에서 HIR AST shape를 보고 intent/cleanup semantics를 재구성하는 것
- ABI 문제를 backend 개별 ad-hoc struct tweak로 덮는 것
- `MIR missing -> silently fallback`을 계속 허용하는 것

## 6. 작업 순서 제안

가장 안전한 순서는 아래다.

1. declaration inventory debt map 작성
2. main wrapper metadata migration
3. declaration-side AST naming 의존 정리
4. transpiler ABI metadata 실사용 전환
5. CI에서 routine-body fallback 금지 유지

## 7. 성공 판정

이 마이그레이션은 아래가 모두 만족될 때 완료로 본다.

- compiler mainline backend entry가 `MIR` 기준으로 동작한다
- `LLVM`과 `C` 모두 동일한 MIR routine/block graph를 기준으로 emit한다
- `HIR`는 legacy compatibility path와 diagnostics/type/source metadata lookup 용도로만 남고, mainline codegen source of truth는 아니다
- `DIR/RIR/MIR` 검증 실패는 backend 진입 전에 중단된다
- ABI contract 위반은 spec/test/backend validation에서 모두 잡힌다
