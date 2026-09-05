# Authority / Effect 삭제·대체 의미 감사 — 2026-09-05

Status: AUDIT COMPLETE; 구현 수정과 SoT 승격은 하지 않았다.

Base revision: `bf8b33d078b27c41cc6cdb7ffed2e8fa5c62ef22`.
지시서: [Intent and language-axis semantic audit](../agent_work_directives/intent_and_language_axes_semantic_audit_2026-09-05.md), lane 2.

## 가장 중요한 관찰

Capability, Authority, 두 종류의 Effect를 한 축으로 합치면 현재 native가
구별하는 정적 의무가 사라진다. 그러나 설치된 self-host의 일반 source-to-MIR
경계는 아래 네 거절을 아직 재현하지 못했다. 이는 개념을 삭제할 근거가 아니라
현재 약속과 self-host admission 사이의 구현 격차다.

| 입력 변화 | native `--native-pipeline --mir-json` | 설치 DRV-2 `--emit-mir-json-verified` |
| --- | --- | --- |
| `Now()` 사용, `with caps random` | exit 1; missing declared capabilities: clock | exit 0; MIR JSON 3,175 bytes |
| `Now()` 사용, `with caps clock with effects local` | exit 1; missing declared effects: nondeterministic | exit 0; MIR JSON 3,167 bytes |
| 같은 `Player` 타입의 `owner`와 `observer`; authority는 owner인데 `apply ... by observer` | exit 1; observer is not declared in zone authority set | exit 0; MIR JSON 2,524 bytes |
| `effect slot mark: Marked`에 `class Marked` 결속 | exit 1; unknown effect type Marked | exit 0; MIR JSON 2,019 bytes |

모든 retained 입력에는 유효한 `Main`이 있다. 초기 clock 실험의 Main 부재로
발생한 entrypoint-cardinality 거절은 위 증거에서 제외하고 수정 후 재실행했다.
위 self-host 결과는 source→MIR stdout까지의 관찰이다. 해당 잘못된 MIR를
코드 생성하거나 실행하지 않았으며 backend/runtime 수용 여부는 주장하지 않는다.
설치 capability-manifest 전용 경계는 첫째 cap 누락을 제대로 거절하지만,
둘째 effect 누락 입력은 exit 0으로 수용했다.

## 비교 단위와 판정

| 축 | 삭제하면 잃는 사실 | 이번 판정 | 판정을 뒤집을 증거 |
| --- | --- | --- | --- |
| ambient operation Capability | 호출 그래프를 따라 추론한 사용 operation 집합, 선언 상한, host grant와의 검사 | KEEP-CORE. `with caps` 표기는 선택적 상한이며 자동 추론 자체와 구별한다 | 기존 코어의 국소 재작성만으로 동일한 interprocedural compile-time 거절과 runtime grant 의무를 보존 |
| zone/participant Authority | 같은 타입의 여러 instance 중 어느 subject slot이 이 mutation을 승인하는가 | KEEP-FACT / NO NEW KEYWORD. 기존 `authority`/`by`의 사실은 유지 | operation mask와 zone membership만으로, 별도 승인 관계를 재도입하지 않고 아래 same-type 쌍을 구별 |
| delegation history | 같은 cap·zone 상태에서도 누가 누구에게 위임했는지에 따라 달라지는 승인 | CONDITIONAL: 모델의 독립 사실은 확인, 현 표면 구현 증거는 미완 | 구체 source declaration→검증된 delegation chain→승인 consumer의 수용/거절 쌍과 구현 연결 |
| 함수의 coarse `with effects` | nondeterminism, authority/resource boundary 등 operation grant와 다른 분류·조합 의무 | KEEP-FACT / NO NEW KEYWORD. 선택적 assertion과 추론 fact를 구별 | cap mask만으로 분류와 effect closure/conflict, 명시 effect 상한 검사를 정보 손실 없이 재현 |
| domain `effect E for participant` | participant에 적용·유지되는 nominal 시간적 layer, effect slot/causes/topology/projection 결속 | KEEP-CORE, 현재 bounded domain contract에 한정 | 일반 class/struct 인코딩으로 같은 typed layer identity와 lifecycle/projection 의무를 별도 compiler fact 재도입 없이 보존 |

DELETE 수를 맞추지 않았다. KEEP-CORE는 전체 self-host 구현이 끝났다는 뜻도,
다른 언어에서 절대 구현할 수 없다는 정리도 아니다.

## 현재 owner와 마지막 소비자

| 사실 | owner와 확인한 소비 경계 |
| --- | --- |
| builtin operation→cap mask | [builtin capability registry](../../src/semantic/builtin_capability_registry.def)는 Now→CLOCK, Input→IO_READ, Random→RANDOM을 구별한다. [native function contract](../../src/semantic/type_checker_func_decl.c)는 used/declared를 비교한다. self-host는 [semantic capability owner](../../src/self_hosted/semantic/ast_capability_fact_owner.pgy)가 admitted call identity에서 fixed point와 callable/program mask를 만들고 [manifest owner](../../src/self_hosted/compiler/capability_manifest_owner.pgy)가 최종 공개 JSON을 출력한다 |
| coarse effect | [builtin semantics](../../src/semantic/type_checker_builtins_stdlib_body.c)는 Now에 NONDETERMINISTIC과 CLOCK을 각각 기록한다. [effect mask owner](../../src/semantic/type_effects.c)가 closure/join/meet/authority/resource/conflict를 소유하며 [function contract](../../src/semantic/type_checker_func_decl.c)와 [call contract](../../src/semantic/type_checker_call_contract_helpers.c)가 소비한다. self-host [action contract facts](../../src/self_hosted/semantic/ast_action_contract_fact_owner.pgy)의 선언 carriage를 body-derived effect 검증 완료로 승격할 수 없다 |
| zone authority identity | [semantic authority facts](../../src/self_hosted/semantic/ast_zone_authority_fact_owner.pgy)는 authority declaration, owner zone, subject slot, required ability의 node ID를 운반한다. [admission validator](../../src/self_hosted/semantic/ast_zone_authority_validation_owner.pgy)와 [DIR graph owner](../../src/self_hosted/dir/domain_graph_fact_owner.pgy)가 exact slot/ability carriage를 확인한다. native mutation 승인은 [participant authority check](../../src/semantic/type_checker_decls_domain_helpers.c)의 `type_check_zone_participant_authority`를 [zone lifecycle consumer](../../src/semantic/type_checker_zone_lifecycle.c)가 호출한다 |
| required witness / runtime token | [zone authority declaration checker](../../src/semantic/type_checker_zone_decl_authority.c)는 authority의 대상이 subject slot인지, 요구 ability가 구현됐는지 검사한다. [runtime authority consumer](../../src/runtime/pgy_runtime_zone_result_option_inline.h)는 zone/participant 존재 및 양의 expected/provided token 동등성을 검사한다. 타입의 ability 만족과 특정 slot의 승인은 서로 다른 사실이다 |
| domain effect identity / projection | [domain authoring contract](../200_object_to_action_boundary_patterns.md) §2.1은 effect를 temporal state layer로 구별한다. [native zone layer checker](../../src/semantic/type_checker_zone_decl_authority.c)의 `type_check_zone_layer_slots`는 class를 effect slot에 사용할 수 없게 한다. self-host [action contract owner](../../src/self_hosted/semantic/ast_action_contract_fact_owner.pgy)는 causes와 coarse effects를 별도 field로 운반하고, [domain runtime plan](../../src/self_hosted/mir_lower/domain_runtime_plan_owner.pgy)은 admitted topology/participant/projection 행을 backend 직전 소비 대상으로 만든다 |
| source-MIR publication | [production source-MIR boundary](../../src/self_hosted/compiler/driver_source_mir_execution_owner.pgy)의 `DriverSourceMirProjectionFromAdmittedRequest`는 typed analysis와 MIR projection을 통과한 payload를 만든다. 위 네 입력이 이 경계에서 JSON으로 나왔으므로 개별 fact-owner gate의 성공이 이 publication의 semantic completeness를 증명하지 않는다 |

현재 `Authority`라는 단어에는 세 층이 섞이기 쉽다. `with caps clock`은 operation
종류, `authority owner`는 승인 주체의 zone slot, runtime token 검사는 실제 호출의
증거다. `role`이 Ability를 구현한다는 사실도 임의의 동일 타입 instance에
승인권을 부여하는 증거는 아니다. 위임 이력은 여기에 추가되는 별도 모델 사실이다.

## 실행한 source-level 대체 실험

보존한 [fixtures와 runner](../../tests/concept_semantics/authority_effect/run.sh)는
compiler 자체를 변경하지 않는다. 따라서 아래는 source assertion 삭제/대체
실험이며, compiler 내부 concept 삭제 실험이라고 부르지 않는다.

1. `clock_both_contracts`의 `Stamp`는 `with caps clock with effects nondeterministic`
   상태에서 `Now()`를 호출한다. native 수용, 설치 manifest CLOCK/0x8.
2. `clock_effect_only`는 caps assertion을 제거한다. 같은 호출과 coarse 분류는
   남고, native 수용과 CLOCK/0x8 추론이 유지된다. 그러나 random만 허용하려는
   상한은 사라진다. 실제 `clock_wrong_capability`는 원래 상한을 유지하면
   native/설치 manifest 모두 거절한다. coarse nondeterministic만으로 CLOCK과
   RANDOM 권한을 구별할 수 없다.
3. `clock_capability_only`는 effects assertion을 제거한다. CLOCK/0x8은 유지되며
   native가 수용한다. 이 사실은 body effect 추론을 삭제했다는 뜻이 아니다.
   `clock_wrong_effect`는 CLOCK 권한이 충분해도 local effect 약속 때문에 native가
   거절한다. 어떤 연산을 허용하는지와 nondeterminism을 금지하는지는 다른 질문이다.
4. `authority_named_slot`/`authority_wrong_same_type_slot`은 승인 이름만 owner에서
   observer로 바꾼다. 두 slot은 같은 Player 타입이며 Main의 `with caps clock`도
   같다. native는 전자를 수용하고 후자를 거절한다.
5. `authority_removed_same_capability`는 잘못 승인한 쌍에서 authority 선언만
   제거한다. native가 수용한다. named-slot 원본과 삭제본의 native capability
   manifest는 모두 used=[] / 0x0이다. 승인 집합을 지웠으므로 이 수용은
   동등성 성공이 아니라 잃은 static guarantee의 관찰이다. 권한 없는 zone도
   허용하는 현재 계약에 따라 warning은 남는다.
6. `effect_layer_declared`의 `effect Marked for bearer: Player`를 class로 바꾼
   `effect_layer_as_class`는 같은 effect slot/apply 표면을 유지한 채 native에서
   거절된다. 단순 상태 class가 participant-bound layer identity를 대신하지 못한다.
   apply/maintain/detach까지 수작업 함수로 다시 구현하는 완전한 대체는 이번에
   만들지 않았다. 그렇게 재작성하면 사라지는 typed participant/lifecycle/
   projection 결속을 먼저 명세해야 정확한 동등 비교가 가능하다.

추가 runtime positive로 기존 [zone action/effect fixture](../../tests/cases/backend_compare/zone_action_effect_runtime/main.pgy)를
native C와 LLVM에서 각각 컴파일·실행했다. `Attack` 후 `HasLayer(poison)`은 true,
effect의 `PlayerView.hp`는 6이고 두 stdout이 byte-equal이었다. domain effect가
단순 함수 effect mask가 아니라 materialized layer와 projection을 운반하는
bounded 동작 증거다. 전체 frontier/임의 effect algebra를 증명하지 않는다.

## Gate 결과와 재현 범위

실행 binary SHA-256:

- native local: `0F9F4F30255D6850B5A773E21D5815F776B305E5C01A7A2C3DF6D373BB15A29E`.
  다른 세션이 재빌드한 로컬 binary이므로 base commit 재현 binary라고 주장하지 않는다.
- installed DRV-2: `FB37EA36D92E9C28B6BB7162F87BA00E733255AD5E46B24A166578713DF75847`.

| 실제 실행 | 결과 | 해석 한계 |
| --- | --- | --- |
| `bash tests/concept_semantics/authority_effect/run.sh` | PASS, 약 4.5초: native 6 accepted/4 rejected; installed manifest 3 accepted/1 rejected; 두 assertion 삭제본 CLOCK byte parity | 일반 source-MIR의 네 실패는 이 green runner의 보장 범위 밖이며 위에 별도 기록 |
| `bash tests/capability/run_manifest.sh` | PASS: 2 clean, 8 under-declaration rejection | 설치 public capability manifest 경로의 현재 operation-bound 검사 |
| `PGY_SELFHOST_ZONE_AUTHORITY_BUILD_DIR=.../authority_effect/zone_owner_gate bash tests/self_hosted/parity/zone_authority_fact_owner.sh` | PASS, 약 9.8초 | producer carriage, missing/duplicate ability/slot, DIR no-rescan owner probe. 실제 모든 source mutation 승인까지 확인하는 gate가 아님 |
| `bash tests/runtime_authority_contract_smoke.sh` | PASS | 공유 runtime 진단/토큰 export 구조 계약. runtime 전체 실행 검증으로 세지 않음 |
| 기존 zone_action_effect_runtime native C/LLVM 컴파일·실행 | PASS, 각각 `true\n6`, 약 3.3초 | 해당 effect activation/projection 프로그램 한 개 |
| 네 negative의 installed `--emit-mir-json-verified` | semantic completeness claim에 FAIL: 모두 exit 0 + JSON | 잘못된 수용을 expected success 회귀로 고정하지 않음 |

Windows 실행은 Git Bash와 설치 binary를 사용했다. 작업 로그는
`.tmp/self_hosted/concept_semantics_20260905/authority_effect/` 아래에 있다.
새 native negatives의 진단을 generic parse failure로 대체해 통과시키지 않았으며,
native 거절 뒤 MIR stdout가 발행되지 않았음을 retained runner가 확인한다.
두 same-type subject만 둔 최소 fixture의 zone-shape warning과 authority-free
layer warning은 관찰했다. 경고가 없었다고 보고하지 않는다.

## 모델 증거와 구현 증거의 분리

[AuthorityIrreducibility.v](../semantics/proofs/AuthorityIrreducibility.v)는 동일한
cap/zone 사영과 다른 delegation graph를 가지는 두 configuration을 정의하고
`delegation_distinguishes`, `authority_beyond_cap_zone`를 보인다. 그 모델에서
authority를 cap×zone의 함수로 환원할 수 없다는 근거다. Pergyra source에
일반 위임 이력 표면이 구현됐다는 증거나 Felleisen 정리가 아니다.

[AuthorityDelegationCore.v](../semantics/proofs/AuthorityDelegationCore.v)의 범위도
principal→caps와 단일 delegation step이며, 헤더는 live AIR authority facts에
대한 binding이 아직 없다고 명시한다. 이번 lane은 Rocq를 재실행하지 않았고
proof spine 연결/이전 CI를 이번 실행 성공으로 세지 않았다.
[축 비표현성 문서](../semantics/22_axis_macro_expressibility.md) §1.5의 위임
표면 의무 잔여와 일치한다. 그 문서의 강한 역사적 평점을 위 네 self-host
거절 실패보다 우선시해서는 안 된다.

## 다음 보강의 판정 기준

제안이며 구현 착수나 successor rung 지정은 아니다.

- source-MIR publication이 설치 manifest와 같은 admitted capability verdict를
  소비하는지 확인하고, caps 누락 source가 MIR를 발행하지 않는 한 경계의 증거로
  닫아야 한다. manifest 소유자를 다시 계산하거나 backend에서 보충하면 안 된다.
- coarse effect의 body-derived 사용/closure와 선언 상한의 검증 owner를 명시한다.
  effect spelling carriage가 있다는 이유로 의미 검증을 완료 처리하지 않는다.
- zone authority approval은 exact subject-slot identity와 mutation topology의
  join으로 검사한다. same-type observer, missing approving slot, required ability
  mismatch를 구별하며 grant mask나 첫 authority row로 대체하지 않는다.
- domain effect의 nominal identity와 participant role을 publication 전에 확인한다.
  class/struct로 명목 변경했을 때 실패해야 할 곳을 parser/backend가 대신 정하지 않는다.
- delegation 독립성은 실제 source↔admitted history↔consumer 연결이 생겼을 때
  새 source pair로 재평가한다. 현재 증거만으로 새 키워드나 일반 분산 위임을 추가하지 않는다.

실제 workload 표본은 컴파일러의 capability publication, 도메인 effect/projection,
same-type 승인 fixture로 제한된다. 세 개 외부 응용에서 검증됐다는 주장은 하지
않는다. 새 개념을 추가하기보다 이미 있는 독립 fact들이 production 경계까지
동일한 거절을 보존하는지 닫는 것이 이번 관찰에서 나오는 우선순위다.
