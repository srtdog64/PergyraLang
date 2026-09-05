# Ability / World / Role / Action / within / where 삭제·보강 의미 감사

작성일: 2026-09-05. 기준 HEAD:
`bf8b33d078b27c41cc6cdb7ffed2e8fa5c62ef22`.

이 문서는 [작업지시서](../agent_work_directives/intent_and_language_axes_semantic_audit_2026-09-05.md)의
lane 3 결과다. 언어 의미·SoT 상태·self-host 진척을 소유하지 않는다. 조사 중
컴파일러 source, registry, build 설정을 변경하지 않았다.

결론은 **계약 fact를 유지하면서 추론 가능한 표면 반복을 줄이는 방향이 맞다**는
것이다. 실행한 세 치환 쌍 가운데 `where` 생략은 같은 경계 fact를 보존했다.
단순 state action의 `func` 치환은 같은 상태 결과를 냈다. Role dispatch를 두 함수
호출로 편 것은 같은 출력만 보존했으며, 열린 구현 계약과 문맥 바인딩은 보존하지
못했다. 이 세 결과는 서로 다른 종류의 삭제다.

## 판정과 뒤집을 증거

`KEEP-FACT / NO NEW KEYWORD`는 기존 키워드를 지금 삭제한다는 뜻이 아니다.
그 fact의 필요성과 별개로 새 표면 개념을 늘릴 근거가 없다는 판정이다.

| 축 | 잠정 판정 | 현재 검사되는 의미 | 판정을 뒤집을 증거 |
| --- | --- | --- | --- |
| Ability | KEEP-CORE | 구현이 제공해야 할 연산 계약, ability별 role 구현, native generic actual의 자격 | 같은 open polymorphic 계약과 거절을 기존 개념만으로 보존하고 ability-equivalent dictionary/constraint를 다시 만들지 않는 치환 |
| Role | KEEP-FACT / NO NEW KEYWORD | 어느 subject의 어느 ability 구현인지, 명시 slot bind/rebind의 선택, 한 role/ability의 impl 유일성 | 문맥별 구현 선택과 ambiguity 거절을 보존하는 더 작은 표면. 두 개의 고정 호출을 함수로 펴는 것만으로는 부족 |
| World | KEEP-CORE | native world-owned Zone live escape 거절, self-host embedded resource의 경로·carriage owner | Zone graph만으로 같은 root 소유·escape·handoff 판정을 보존하고 graph root 안에 World와 같은 경계 표식을 되살리지 않는 치환 |
| Action | CONDITIONAL | subject-owned callable kind와 action 전용 계약의 귀속. 단순 mutation 자체는 hosted func도 가능 | 계약 없는 state action은 이미 func 치환 가능. 계약 있는 action의 모든 admission과 관측 의미를 func로 보존하면 추가 축 정당화가 약해짐 |
| within | KEEP-FACT / NO NEW KEYWORD | action 선언의 Zone 제약. 명시되면 실제 Zone과 subject/authority 계약에 결속 | exact Zone fact가 항상 소유자에게서 유도되고 명시 assertion의 별도 가치도 없다면 표면 반복 축소 가능 |
| where — intent step | KEEP-FACT / NO NEW KEYWORD | step의 Zone 종류와 실제 `using` participant가 일치해야 함 | explicit/inferred 쌍은 이미 같은 fact를 보존. 다른 합성 형태에서도 이 동치가 성립하면 반복 절을 더 줄일 수 있음 |
| where — generic | KEEP-FACT / NO NEW KEYWORD | native가 actual type의 ability bound를 검사. self-host source MIR에는 아래의 누락이 관측됨 | 같은 bound 판정을 다른 기존 표면으로 보존하는 치환. intent의 위치 제약과는 다른 판단 |

`where`의 두 문법은 같은 철자를 쓸 뿐 의미 owner가 다르다. 하나의 키워드 사용
빈도로 두 계약의 필요성이나 구현 완성도를 평가하면 안 된다.

## 실행 환경과 유지한 테스트

설치 DRV-2 SHA-256은
`FB37EA36D92E9C28B6BB7162F87BA00E733255AD5E46B24A166578713DF75847`이다.
Native SHA-256은
`0F9F4F30255D6850B5A773E21D5815F776B305E5C01A7A2C3DF6D373BB15A29E`이다.
Native는 다른 세션이 재빌드한 로컬 binary이므로 앞선 원격 CI binary와 동일하다는
주장을 하지 않는다. Native와 self-host 경로를 명시적으로 선택했다.

실행 runner는 [domain_axes/run.sh](../../tests/concept_semantics/domain_axes/run.sh)다.
Git Bash에서 `timeout 300 bash tests/concept_semantics/domain_axes/run.sh`로 실행한다.
각 실행은 `.tmp/self_hosted/concept_semantics_20260905/domain_axes/run.XXXXXX`에
독립 결과를 보존한다. 빌드 시스템이나 전체 matrix는 호출하지 않는다.

최종 runner 실행은 PASS이며 결과는
`.tmp/self_hosted/concept_semantics_20260905/domain_axes/run.45PKwr`에 있다.
종료 시 두 binary hash는 시작 값과 같았다. 문서 링크 16개와 명시 source line
anchor 9개의 존재·범위도 확인했다.

| 확인 범위 | 관측 |
| --- | --- |
| 새 3개 source 치환 쌍과 계약 있는 action 대조군 | Native C/LLVM 실행 출력 일치 |
| function의 action-only within, intent where/using 충돌 | Native C/LLVM 및 DRV-2 source MIR 거절, 거절 뒤 해당 executable/MIR 없음 |
| generic ability bound와 World live escape | Native C/LLVM semantic 거절. 기존 정상 source의 native MIR admission도 확인 |
| 새 정상 source 7개 | DRV-2 source MIR admission. callable kind/contract 및 explicit/inferred Zone fact를 구조적으로 확인 |
| 기존 ability_coherence_smoke.sh | PASS: C/LLVM 중복 impl 거절, 두 role + 명시 rebind 합법 |
| 기존 driver_execution_action_optional_within_parity.sh | PASS: native C/LLVM 실행 + DRV-2 MIR. `within:null`, `authorized_by:["self"]`를 별개로 보존 |
| 기존 intent_step_binding_contract_owner.sh | INCONCLUSIVE: probe compile이 `ast_artifact_invalid`, owner `nominal_constructor_argument_type`, constructor `SemanticAstZoneAuthorityFacts`, argument 9에서 거절 |

마지막 행은 step binding의 의미 실패나 성공으로 세지 않는다. Source fixture의
where 충돌 거절은 별도 실행 증거다. 이번 lane에서 Rocq/Lean 정리를 새로 만들거나
실행하지 않았으며, 전체 self-host semantic parity를 증명했다고 주장하지 않는다.

## 치환 실험 A — Action과 hosted func

[action_state.pgy](../../tests/concept_semantics/domain_axes/action_state.pgy)와
[function_state.pgy](../../tests/concept_semantics/domain_axes/function_state.pgy)는
`Advance`의 선언 단어만 다르다. 같은 subject의 `let mut count`를 증가시키며
반환값과 호출 뒤 필드값 모두 C/LLVM에서 `2`, `2`였다. DRV-2 MIR는 두 경우를
각각 `callable_kind:action`과 `callable_kind:function`으로 유지했다.

이는 mutation이 action의 독점 의미가 아니라는 구별이다.
[docs/200 §3.2](../200_object_to_action_boundary_patterns.md#32-func와-action-선택)는
func도 caps/effects를 가질 수 있다고 명시한다. 효과 유무만으로 action을
정당화하지 않는다. 이 좁은 프로그램에는 추가 domain 계약이 없어 func가 충분하다.

반면 [action_zone_contract.pgy](../../tests/concept_semantics/domain_axes/action_zone_contract.pgy)는
CounterZone + subject slot + authority + action을 연결한다. C/LLVM은 `2`, DRV-2
MIR는 `within:CounterZone`와 `authorized_by:[self]`를 보존했다.
[function_zone_contract_rejected.pgy](../../tests/concept_semantics/domain_axes/function_zone_contract_rejected.pgy)는
같은 절을 그대로 둔 채 action만 func로 바꾸면 거절된다. Native의 거절은 parser의
action-only 규칙이다. **이 문법 거절 자체를 새로운 정적 정리로 세지는 않는다.**
같은 domain 계약을 func에 실을 현행 표면이 없다는 치환 한계를 보여준다.

현재 의미 owner는
`src/self_hosted/semantic/ast_action_contract_fact_owner.pgy:166`의 subject owner
검사와 `:244`의 action-only within 처리다. Native
`src/semantic/type_checker_func_action_contract.c:76`은 실제 Zone 존재를,
`:170` 이후는 subject slot/authority 적합성을 검사한다. 마지막 계약 consumer는
MIR method contract admission이며, call-site 권한과 runtime 승인 증거는 별도다.
[docs/200 §4.1](../200_object_to_action_boundary_patterns.md#41-actioncontract-sot-패턴)이
이 구별을 고정한다.

## 치환 실험 B — Ability/Role과 직접 함수

[role_dispatch.pgy](../../tests/concept_semantics/domain_axes/role_dispatch.pgy)는
같은 Fighter에 Warrior와 Berserker가 Combatable을 각각 제공한다. 같은
`team.fighter.Score()` 호출을 명시적으로 rebind하여 `10`, `77`을 얻었다.
[function_dispatch.pgy](../../tests/concept_semantics/domain_axes/function_dispatch.pgy)는
호출 지점에서 구현 함수 이름을 직접 골라 같은 출력을 냈다. 두 경우 모두
native C/LLVM 실행 및 DRV-2 MIR admission을 관측했다.

하지만 함수 치환에는 `Combatable`의 열린 연산 계약과 재바인딩 가능한 slot이
없다. 새 구현을 받아도 같은 호출 지점을 유지하는지, 계약을 빼먹은 구현을
거절하는지까지 보존하지 않았다. 따라서 닫힌 workload의 출력 동치이며 Role
삭제의 완전 증거가 아니다. Subject nominal type 하나에도 구현 선택이 붙지 않는다.

[Ability/Witness 정전 §4.1](../semantics/10_ability_witness_evidence.md#41-coherence--role-scoped-ambiguity--설계로-우회됨-audit-2026-06-20)은
explicit named binding을 coherence 규칙으로 고정한다. 실제 native owner
`src/semantic/type_checker_role_decl.c:203`은 한 role/ability의 중복 impl을
거절한다. 설치 self-host의
`src/self_hosted/semantic/ast_bind_statement_type_fact_owner.pgy:14`는 slot의
ability type과 해당 role의 구현을 함께 판정하고,
`src/self_hosted/semantic/ast_role_fact_owner.pgy:9`는 role/target/impl/ability
identity를 별도 행으로 운반한다. 뒤의 binding emitter가 이 사실을 소비한다.

정전은 내부 구조를 typeclass/dictionary 계열로 설명한다. 따라서 "trait와 모양이
다르다"는 것만으로 새 정적 능력을 주장하지 않는다. 검증할 차이는 **명시 문맥의
구현 선택과 domain 요구의 결합**이다. Witness가 일부 reify돼 있다는 사실도 모든
경계의 구현 refinement가 닫혔다는 증거는 아니다.

## 치환 실험 C — explicit where와 inferred where

[where_explicit.pgy](../../tests/concept_semantics/domain_axes/where_explicit.pgy)에서
`where: CounterZone`만 제거한 것이
[where_inferred.pgy](../../tests/concept_semantics/domain_axes/where_inferred.pgy)다.
두 프로그램 모두 C/LLVM에서 `true`였다. DRV-2 MIR의 같은 step에는 두 경우 모두
`IntentZoneWhere(CounterZone, Advance)`와
`IntentZoneAlias(counter_zone, Advance)`가 정확히 하나씩 남았다.

[where_conflict_rejected.pgy](../../tests/concept_semantics/domain_axes/where_conflict_rejected.pgy)는
using participant는 그대로 두고 명시 where를 OtherZone으로 바꾼다. Native
C/LLVM은 zone type mismatch, DRV-2는 DIR step binding unresolved로 거절하며
MIR를 내보내지 않는다. 이름이 같은 별도 instance의 authority까지 검증한 실험은
아니다. 여기서 검증한 것은 declared Zone type과 using binding의 일치다.

`src/self_hosted/codegen/emission/intent_step_binding_owner.pgy:101`은 where가
없으면 using의 exact Zone에서 도출하고, 명시됐다면 불일치를 거절하는 기존
정책을 설명한다. **삭제된 것은 반복 절이고 보존된 것은 Zone fact**다. 이 결과는
문법 압축에 직접 쓸 수 있지만 새로운 키워드를 요구하지 않는다.

Action의 `within`은 선언 계약이며 intent step의 `where`는 사용 문맥이다.
Generic `where T: Sortable`는 actual type에 관한 명제라서 둘 중 어느 것도 대신하지
못한다. 선택적 within 자체는 앞의 optional-within 실행 gate가 별도로 검증했다.

## World와 Zone graph — 경계 소유를 지우면 달라지는 것

기존 `tests/cases/axis_composition/comp_world_intent/control.pgy`는 free-standing
Zone 사이의 정상 intent transfer이며 native MIR admission, native C 실행 `2`를
관측했다. `cross.pgy`는 world-owned Zone을 live argument로 빼내 같은 모양의
transfer에 건네고 native semantic 단계에서 거절된다. 이 쌍은 서로 다른 소유
경계의 판정 차이를 보이며, 같은 계약을 보존하는 World 삭제 실험은 아니다.

Native owner는 `src/semantic/type_checker_world_embedding.c:217`의 live member
escape 판정이다. Self-host에는
`src/self_hosted/semantic/ast_zone_value_carriage_verdict_owner.pgy:169`의
`world_zone` resource path와
`src/self_hosted/semantic/ast_zone_parameter_boundary_verdict_owner.pgy:78`의
parameter carriage admission이 있다. 하지만 아래 source-MIR 차이가 남아 있으므로
이 owner들의 존재만으로 World의 모든 call 경계를 닫혔다고 말할 수 없다.

Zone graph가 소유·handoff·frontier fact를 전부 담으면 World의 내부 표현으로
사용할 수 있다. 그 graph에 World root와 같은 소유 판정을 다시 넣었다면 fact를
삭제한 것은 아니다. Bounded frontier와 일반 transitive scheduler의 차이는
[semantics/01](../semantics/01_intent_world_zone.md#theorem-worldzone-frontier-termination)에
남아 있다. [docs/195](../195_world_universe_composition.md)는 dynamic World/module
composition을 `proposed, out-of-beta`로 명시하므로 현재 World 보장의 근거로 쓰지 않았다.

## 반대 증거와 보강 제안

다음은 현재 보장이 완전하다는 주장을 제한하는 실행 관측이다. 성공을 기대하는
회귀 테스트로 고정하지 않았고, 이 audit에서 구현 경로를 새로 열지 않았다.

| 관측 | 의미와 다음 보강 대상 |
| --- | --- |
| 기존 f_where_ability_bad.pgy: native는 Rank<Dice>의 Sortable 위반을 거절하지만 DRV-2 source MIR는 status 0으로 Rank_Dice specialization을 출판 | Function-level bound의 실제 call consumer까지 전달·검증해야 함. native owner는 `type_checker_call_generic_where.c:16`. self-host `ast_ability_generic_bound_verdict_owner.pgy:60`의 ability declaration default 검사와 범위가 다름 |
| 기존 world cross.pgy: native는 live world-zone escape를 거절하지만 DRV-2 source MIR는 status 0으로 출판 | World member를 intent argument로 내보내는 경계의 provenance fact와 마지막 call consumer를 명시해야 함. 단순 world value-copy gate와 별도 |
| 최초 action/func 대조의 count를 `let`로 선언했을 때 native는 immutable field mutation을 거절하나 DRV-2 source MIR는 출판 | Native field mutability와 self-host body admission의 일치 보강 필요. 유지 fixture는 약속대로 `let mut`를 사용하며 이 잘못된 수용을 정상으로 세지 않음 |
| f_where_ability_ok.pgy: native C 실행은 1, native LLVM은 generic Rank의 concrete argument type metadata 부족으로 compile 실패 | Generic semantic admission 성공과 LLVM specialization 실행 지원을 분리. retained runner의 이 정상군은 native MIR admission까지만 주장 |
| Intent step binding owner probe가 다른 nominal artifact 문제로 build되지 않음 | Owner 단위 probe의 executable evidence를 먼저 복구해야 내부 binding 검증 전체를 green이라 부를 수 있음 |

다음 보강은 새 명사를 추가하는 작업보다, 위의 같은 source에 대해 native와
설치 self-host가 같은 의미 판정을 내리도록 만드는 작업이 우선이다. Action은
계약 귀속을 실제로 소비하는 곳에서 유지하고, 단순 wrapper는 local reasoning과
오류 표면을 늘리는지 따로 본다. Role은 include/override·다중 ability·generic
default를 묶은 다음 distinguishing pair가 유효한 후속 표본이다. World는 중첩
소유 Zone과 intent call argument의 provenance가 후속 표본이다. 이들은 제안이며
이번 감사의 완료 항목이나 새 active implementation rung이 아니다.

이번 표본은 compiler fixtures와 작은 Counter/Fighter 프로그램이다. 서로 다른
외부 프로젝트 세 개에서의 검증을 대신하지 않으며, workload 수나 테스트 수로
언어 생태계 성숙도를 올리지 않는다.
