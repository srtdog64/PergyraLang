# Pergyra의 언어 표면과 계약 밀도: 비판적 설계 검토

Date: 2026-10-12 KST.
Status: **DESIGN REVIEW; source inspection; implementation closure NOT CLAIMED**.
Base HEAD: `23d1e65fd1709104a0a8c4f47e315a964b356632`.

Coordination: 조율 담당이 이 새 감사 문서 한 파일의 작성 범위를 전달하고
메인 담당에게 경로 편집 금지를 통보했다. 메인의 명시적 답장은 아직 없다.
쓰기 직전 같은 경로가 없고 DRV-2 A/B/C 편집 범위와 겹치지 않음을 확인했다.
production source, tests, active handoff, SoT registry, directive는 수정하지 않는다.

## 1. 판정과 검토 경계

“Pergyra는 작은 사용자 부담을 지향하며, 필요한 계약을 컴파일러 내부에서
조합한다”는 논지는 설계 목표로 일관된다. 언어가 다루는 계약 수와 사용자가
직접 관리하는 계약 수를 구분하는 것은 필요하다. 그러나 내부 의미론의 풍부함이
작은 사용자 부담으로 이어졌다는 결론은 아직 비교 실험으로 입증되지 않았다.

가장 강한 열린 반론은 다음과 같다. 컴파일러 내부에 계약·고정점·증거 운반을
추가하면서 사용자에게도 선언, 우회 코드, 새로운 실패 규칙과 원격 파손을
더할 수 있다. 이 경우 복잡성이 이동한 것이 아니라 양쪽에 증가한다.
키워드 수만으로 이 반론을 증명하거나 반박할 수 없다.

이 문서는 설계 논지, 직접 읽은 소스, 과거의 한정된 실행 관찰, 다음 검증 제안을
구분한다. 현재 DRV-2 구현, registry 상태, installed driver, CI 판정을 소유하지
않는다. Foundation / Resource / Execution / Domain / Host Boundary는 이
검토의 설명용 분류다. 저장소의 구현 분류나 새로운 모듈 체계로 승인하지 않는다.

### 검토 작업 카드

| 항목 | 경계 |
| --- | --- |
| 목적 | 계약을 언어가 이해하는 비용과 사용자가 관리하는 비용을 분리하고, 논지의 반증 조건을 고정한다. |
| 우선순위 | 의미·책임 일치, 증거 범위, 사용자 부담, 검증 비용, 표현의 간결함. |
| 사실 소유자 | 기존 semantic/IR/ABI/runtime owner와 해당 계약. 이 문서는 읽기 검토 결과만 소유한다. |
| 마지막 소비자 | 설계 검토자와 향후 비교 실험 담당. compiler admission의 입력으로 사용하지 않는다. |
| 금지할 대체 근거 | 문서·keyword·fixture 수, 모델 통과, 이전 바이너리의 성공을 현재 구현 완료로 대체하지 않는다. |
| 검증 | 인용 소스·링크 확인, 주장/관찰/제안 구분. 비교 앱과 새 합성 반례 실행은 별도 미실행 계획이다. |

공유 checkout에는 DRV-2의 소유권·파서 수정과 별도 Rocq 작업이 있다. 본 검토는
그 파일을 수정하거나 compiler-scale build를 시작하지 않는다. 이 문서의 작성은
활성 rung의 변경 집합을 늘리거나 다음 구현 순서를 바꾸지 않는다.

## 2. 유지할 논지와 고쳐 쓸 문장

- OS·런타임·프레임워크에 흩어진 계약 중, 소스 의미를 보존하는 데 필요한
  identity·수명·권한·효과·실패 의무를 compiler fact로 가져올 수 있다.
  POSIX API 전체를 문법으로 옮겨야 한다는 결론은 나오지 않는다.
- 익숙한 명령형 표면과 추론·ADT·합성을 지향하는 의미 체계는 양립 가능한 목표다.
  “C# 표면, OCaml 의미”라는 비유는 현재 문법 호환성, Hindley–Milner 추론,
  OCaml module semantics 구현을 뜻하지 않는다.
- 일반 계산에는 `func`/`struct`만으로 충분해야 한다. 추가 구성체는 실제
  authority·resource·관찰 가능한 전이·목적 귀속을 표현할 때 정당화한다.
  소스에서 키워드가 안 보이더라도 수입한 API의 효과와 수명 제약이 호출자에게
  노출될 수 있으므로, 작은 표면만으로 작은 학습 부담이 성립하지는 않는다.
- AI의 생성 비용 감소와 검토·통합 비용의 상대적 중요성은 이 설계의 동기가 될
  수 있다. 이 저장소가 AI 작성 코드의 결함이나 유지 비용을 실제로 줄였다는
  경험적 결론은 별도 측정이 필요하다.
- `Verified Contracts / Developer-Managed Complexity`는 방향을 설명하는
  비유다. 현재 점수나 검증된 비율로 쓰지 않는다. 모델 정리, production 검사,
  runtime 검증을 같은 분자로 합치지 않으며, 분모를 annotation 개수로만
  정의하지 않는다.

권장 요약:

> Pergyra는 필요한 계약을 의미 사실로 다루고, 반복 선언은 추론과 계약
> 상속으로 줄이는 것을 목표로 한다. 성공 여부는 같은 기능·실패 계약을
> 구현했을 때 개발자가 관리할 규칙과 수정 비용이 줄어드는지, 그 계약이
> 실제 실행까지 보존되는지로 판단한다.

## 3. 설계 의도와 확인된 구현

다음 표의 “소스 확인”은 해당 코드 경로를 읽었다는 뜻이다. 이번 리뷰에서
현재 dirty 후보의 컴파일·실행이나 전체 경로 parity를 수행했다는 뜻이 아니다.

| 논점 | 확인한 근거 | 허용되는 결론과 남는 경계 |
| --- | --- | --- |
| 필요한 구성체만 선택 | [Intent 설계](../01_intent_first_design.md), [메모리 합성 계약](../semantics/28_memory_boundary_composition.md) | 계산은 func/struct, 일반 값마다 Slot 불필요라는 설계가 명시돼 있다. 실제 사용자 부담 감소는 미측정이다. |
| Intent의 정적 귀속 | [native typed terminal owner](../../src/semantic/type_checker_intent_typed_outcome.c), [self-host transition owner](../../src/self_hosted/semantic/ast_intent_transition_fact_owner.pgy) | 결과 terminal이 원래 payload binding을 운반하도록 검사하는 구체 코드가 있다. 자연어 목적 이해나 모든 의미 축의 종단 간 귀속 증명은 아니다. |
| 반복 계약의 축약 | [on-receiver inference](../../src/semantic/type_checker_intent_on_inference.c), [계약 출처 진단](../../src/semantic/type_checker_intent_contract_summary.c) | 해당 action에서 requires/causes/authorized-by를 가져오고 출처를 구분하는 경로가 있다. 임의의 다중 후보·동적 dispatch 추론 전체를 증명한 것은 아니다. |
| who와 approval 구분 | [authority owner](../../src/semantic/type_checker_intent_authority.c), [축약 계약 gate](../../tests/intent_compression_contract_smoke.sh) | who만으로 권한을 만들어내지 않는 정책과 missing approval 진단이 있다. 선언에서 계약을 상속하는 것과 runtime 권한이 실제 유효한 것은 별개다. |
| effect의 서로 다른 의미 | [effect 설계](../effect_system_design.md), [effect mask owner](../../src/semantic/type_checker_helpers_effects.c), [call graph propagation](../../src/semantic/callable_capability_inference.c) | 도메인 effect, callable effect mask, runtime capability는 다르다. 사용 효과의 전파 코드가 존재하지만 Koka의 row-polymorphic effect type system을 구현했다는 근거는 아니다. |
| Slot view와 중단 경계 | [view boundary owner](../../src/semantic/type_checker_slot_view_boundary.c), [await 소비자](../../src/semantic/type_checker_expr.c), [spawn/channel 소비자](../../src/semantic/type_checker_async_channel.c) | 활성 pinned view가 await/spawn/channel 경계를 넘는 경우를 거부하는 경로가 있다. 저장된 일반 graph link와 모든 callback 경로의 폐쇄는 별도다. |
| 동적 권한 검사 | [SecureSlot runtime exports](../../src/runtime/pgy_runtime_lib_secure_slot_exports.h), [authority failure contract](../../src/runtime/pgy_runtime_authority_contract.h) | token 일치와 읽기/쓰기 권한 검사 및 구분된 실패 코드가 있다. 전체 OS handle을 포괄하거나 동시 check/use·세대 wrap까지 증명했다는 결론은 안 된다. |
| 풍부한 의미 축의 합성 | [15쌍 매트릭스](../156_axis_composition_safety.md), [실제 gate](../../tests/axis_composition_smoke.sh) | 15쌍의 처분 등록과 일부 실행 kernel을 구분해야 한다. 15쌍 모두를 실행으로 검증한 gate가 아니다. AC-2/4/5는 문서상 planned다. |
| 모델의 합성 정리 | [WholeProgramCore](../semantics/proofs/WholeProgramCore.v) | whole_program_safety의 결론은 WF 보존과 authority circulation 비증가다. native memory의 모든 위험 부재를 결론으로 쓰면 안 된다. |
| AIR의 의미 보존 역할 | [AIR 계약](../104_air_compiler_architecture.md), [driver gate](../../src/compiler/driver_app.c), [MIR evidence 수집](../../src/compiler/air_evidence_mir.c) | AIR은 실제 admission에 참여한다. 등록된 의무의 증거 검사와 필요한 의무가 빠짐없이 만들어졌다는 보장은 별개다. |

`Effect/Action/Intent`를 “부작용/함수/워크플로”로 평탄화하면 의미를 잃는다.
도메인 `effect`는 participant에 적용되는 상태 계층이며, callable effects와
동일하지 않다. `action`은 단순히 impure func라는 분류가 아니고,
`intent`는 action 수와 무관하게 목적에 귀속되는 계약을 결속한다.
`Subject/Role/Relation` 역시 identity·역할 계약·관계라는 서로 다른 질문이다.
이는 구성체의 역할 설명이며 모든 조합이 구현됐다는 선언이 아니다.

[모듈 authority 설계](../202_module_authority_boundary_design.md)는
`DESIGN + MODEL CLOSED / SURFACE NOT IMPLEMENTED`로 표시돼 있다.
[현행 module manifest](../language_module_manifest.json)는 file import와
namespace/export를 현행 표면으로 기록한다. 이 리뷰는 모듈 설계의 모델
정리를 실제 별도컴파일·패키지 계약 안정성의 구현 증거로 사용하지 않는다.

## 4. 합성식과 세 요소 이상에서의 반례

`Valid(A⊕B) iff Valid(A) and Valid(B) and CrossObligations(A,B)`는
검증 요구를 표현한 식으로만 유지한다. Valid가 타입 적합성인지, 메모리
안전인지, 종료·진행성인지, business outcome 보존인지 먼저 정해야 한다.
각 Valid가 서로 다른 환경·권한·사전조건을 가정한다면 그 논리곱으로 합성을
정당화할 수 없다. CrossObligations를 “합성에 필요한 나머지 전부”로 정의하면
식은 검사 알고리즘이나 보장 범위를 주지 않는다.

특히 sound한 보수적 compiler는 안전한 프로그램을 거부할 수 있다.
semantic safety와 compiler acceptance를 같은 Valid로 놓고 iff를 요구하면
필요 이상으로 완전한 분석을 약속한다. 구체 fragment의 환경·합성 연산·외부
가정을 정한 뒤 `Admit(P) => Safe(P)`부터 요구하는 편이 명확하다.
이 문서에서 새 정리를 증명한 것은 아니다.

### 4.1 쌍별 안전성만으로 충분하지 않은 구체 반례

각 작업이 첫 lock을 획득한 뒤 두 번째 lock을 기다린다고 하자.

| 작업 | 먼저 보유 | 다음 요청 |
| --- | --- | --- |
| A | L1 | L2 |
| B | L2 | L3 |
| C | L3 | L1 |

A+B, B+C, C+A 어느 두 작업만 있으면 세 번째 lock을 점유할 작업이 없어
같은 원형 대기가 생기지 않는다. 셋이 각자의 첫 lock을 보유하면 대기 사슬이
L1→L2→L3→L1로 닫힌다. 이는 pairwise cycle 부재에서 전체 cycle 부재가
따라오지 않는 반례다. 종료·진행성의 반례이며 곧바로 memory safety 위반이나
현재 Pergyra runtime의 결함이라고 주장하지 않는다. 글로벌 lock 순서나
전체 dependency graph의 불변조건이 추가돼야 이 사례를 막을 수 있다.

메모리 경계의 다음 시험은 `borrowed view + worker suspension + cancellation
cleanup`이다. 취소 요청 뒤 worker가 여전히 view를 사용하는 동안 owner를
정리하는 순서를 시험한다. 두 기능만 켠 fixture는 이 세 사건의 상대 순서를
빠뜨릴 수 있다. 현재 [Future lifecycle owner](../../src/semantic/type_checker_future_lifecycle.c)는
Cancel 뒤에도 await로 join하라는 규칙을 포함하므로, 이 제안은 이미 확인된
결함 보고가 아니다. 실제 capture·cleanup·callback 경로에서 그 규칙이
누락되거나 다른 owner의 판정으로 우회되는지 겨냥하는 합성 시험이다.

쌍별 검사가 충분하려면 계약 분해가 전체 불변조건을 보존한다는 합성 정리,
환경 가정의 일치, 귀납적으로 닫힌 전이 관계가 필요하다. fixture 쌍의 전수
나열만으로 그 전제를 대신할 수 없다.

### 4.2 의미 사실을 많이 갖고도 중요한 의무를 빠뜨릴 수 있다

이 채팅의 앞선 2026-10-12 실행에서는 10월 10일 native binary에
`MapGet<String> → 같은 키 교체/삭제 → 반환 문자열 읽기`를 넣었을 때
strict AIR의 drift가 0이고 C가 생성됐다. generated C는 Map 내부 포인터를
사용하며 runtime 교체·삭제가 해당 문자열을 해제했다. 위험한 프로그램은
실행하지 않았다. 로컬 receipt는
`.tmp/memory-safety-check-20261012/air-{replace,remove}.json`와
`air-strict-{replace,remove}.c`다.

이는 그 바이너리에서 필요한 borrow/payload-lifetime 의무가 전달되지 않았다는
한정된 관찰이다. DRV-2 수정이 진행 중이므로 현재 dirty 후보가 여전히 같은
결함을 가진다고 재판정하지 않는다. 여기서 얻는 설계 반론은 “증거가 풍부하다”와
“필요한 의무를 빠짐없이 발급한다”가 다른 성질이라는 점이다. 축 수나
EvidenceNode 수가 많아도 후자를 대신하지 못한다.

## 5. 정적 분석·동적 검사·외부 계약의 분업

| 경계 | 책임 | 실패할 수 있는 잘못된 전제 |
| --- | --- | --- |
| 정적 의미론 | source identity, 수명·권한 전이·효과 조건을 승인하거나 명시적으로 거부한다. | 함수 이름이나 effect label만 있으면 실제 저장소 동작까지 증명됐다고 가정한다. |
| 동적 의미론 | 현재 handle/권한/상태를 검사하고 실패 의미와 정리 책임을 구현한다. | 토큰 검사를 한 번 통과하면 중단·재개 이후에도 접근이 유효하다고 가정한다. |
| host boundary | OS·장치·네트워크의 보장과 오류를 정확한 wrapper/ABI 계약으로 반영한다. | 컴파일 성공이 원격 권한 유지, IO 성공, 장치 존재를 보장한다고 가정한다. |

POSIX `close`의 명세는 닫힌 descriptor 번호가 후속 할당에 다시 쓰일 수
있다고 정의한다. 따라서 같은 정수 FD라는 사실만으로 같은 열린 자원임을
판단할 수 없다. 여기서는 이 안정적인 재사용 성질만 인용하며 OS별 close
오류·재시도 정책을 새로 정하지 않는다.
[POSIX close 명세](https://pubs.opengroup.org/onlinepubs/009604499/functions/close.html)

wrapper 소유권과 세대가 모든 close/dup/이동 경로를 중재한다면 그 범위의
오사용을 차단할 수 있다. 외부 FFI가 몰래 close하는 경우까지 단순한 세대
카운터로 알 수 있다고 가정하면 안 된다. 세대 확인과 사용 사이에 다른 작업이
자원을 은퇴시킬 수 있다면 lease/pin 또는 동등한 동기화 의무도 필요하다.
세대 wrap, 재발급, 접근 모드, 실패 시 cleanup 역시 따로 정의한다.
이는 보완해야 할 계약 목록이며 새 runtime 기구의 도입 결정은 아니다.

## 6. 추론이 사용자 부담을 줄이기 위한 조건

### 6.1 Koka에서 비교할 것

Leijen의 2014년 논문은 row-polymorphic 효과 타입의 추론과, 효과 부재가
특정 실행 성질에 연결되는 형식 모델을 제시한다. 특히 §2.3과 부록 A는
추론을, §5–6은 모델의 건전성과 효과의 의미를 다룬다. 이번 리뷰는 논문
원문을 직접 확인했다. 이는 역사적 선례이며 최신 Koka 전체 구현 평가가 아니다.
[논문 원문](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/paper-20.pdf)

Pergyra가 가져올 검토 기준은 “무엇을 추론하며 그 결과가 어떤 의미 정리와
연결되는가”다. Pergyra의 도메인 effect를 Koka의 효과 row와 같은 개념으로
소개하거나, 기존 mask 전파를 row-polymorphic 추론 완성으로 부르면 안 된다.
Koka의 결과는 Pergyra의 ownership, authority 또는 host 계약을 증명하지 않는다.

### 6.2 설명 가능성과 추론의 한계

안전 거부와 분석 한계 거부를 구분하고, 실제 사용 지점 → 요구된 계약 →
그 계약의 owner → 변경된 피호출 요약까지 진단으로 추적할 수 있어야 한다.
컴파일러를 계측해 재빌드해야 원인을 알 수 있다면 줄어든 annotation 비용이
디버깅 비용으로 되돌아온다. 이번 [refusal 설명 모델](../semantics/ownership_refusal_explanation.md)은
그 일부를 한정된 모델에서 다루지만 production source binding과 exporter
연결을 완료했다는 근거로 쓸 수 없다.

일반 프로그램의 모든 안전성을 추론하겠다는 목표 대신 지원 fragment,
보수적 거부, 외부 계약과 불명확한 사실의 처리를 명시한다. 명시적 표기가
필요하다면 실제 권한·전달·상호운용·비용 경계에 근거를 둔다. 단순히 현재
분석이 실패한다는 이유로 field 이동·복원 의례나 추가 복사를 기본 언어
계약으로 승격하지 않는다.

### 6.3 별도컴파일과 계약 변경

추론한 공개 요약도 API다. 본문 수정이 요약을 바꾸면 호출자의 재검증·거부가
어디까지 전파되는지 측정해야 한다. 공개 경계에서 버전이 정해진 승인 요약,
본문이 그 요약을 만족하는지의 검사, 변경 diff와 의존성 무효화가 검토 대상이다.
요약 고정은 사용자에게 모든 annotation을 직접 쓰게 한다는 뜻은 아니다.
이 경계의 구현 방안은 아직 비교·검증 제안이며 새 문법을 승인하지 않는다.

“더 적은 효과”가 언제 더 적은 거부를 뜻하는지도 순서를 정의해야 한다.
may-write 요약의 정밀화와 ownership transfer·성공 후 책임 변화는 같은
Boolean 순서가 아닐 수 있다. 고정 그래프에서 얻은 단조성 정리를 다른
시그니처·결과·외부 계약까지 확대하지 않는다.

### 6.4 핵심 언어에 넣을 정당성

새 계약마다 다음을 묻는다. 라이브러리와 기존 타입/추론만으로 충분한가,
compiler가 알아야 막을 수 있는 구체 반례가 있는가, 런타임 특권 없이
표현할 수 없는 identity·수명·권한이 있는가. “중요한 계약”이라는 이유만으로
새 핵심 keyword가 필요해지는 것은 아니다. 반대로 keyword를 줄이기 위해
검증 책임을 비공식 관례나 사용자 주석으로 돌려도 안 된다.

## 7. 먼저 닫을 최소 fragment: 검증 제안

새 전체 설계 트랙 대신 활성 작업에서 도달한 작은 실행 사슬을 대상으로
다음 범위를 고정할 수 있다. 이것은 현재 구현 완료 목록이 아니다.

- 단일 스레드, 한정된 값·구조체·컨테이너 연산, let/대입/조건/루프/반환,
  이미 승인된 호출 요약과 source identity.
- entrypoint의 초기 상태, 각 정상/조기/오류 exit, 소유 backing과 borrow,
  실제 copy/move/replace/remove/drop 동작 및 allocation failure.
- 일반 사용자가 World/Zone/Authority를 추가하지 않아도 작성할 수 있는
  최소 source와 실제 C/LLVM 결과.
- 지원 밖 구문·빠진 요약·잘못된 identity는 명시적으로 거부한다.
  “미지원 전부 거부”만으로 DX 목표를 달성했다고 세지 않도록 별도의 정상
  외부 corpus와 사용 가능한 연산 범위를 고정한다.

필요한 완료 증거는 owner 발급 → 검증 → 마지막 소비자 → 물리 동작의 연결,
양성 실행, 위험 입력의 admission 거부, source/IR binding 위조의 거부,
변환별 의미 보존 근거다. 이를 충족한 범위에서만 `Admit ⇒ Safe`를 주장한다.
async·Channel·Slot·authority는 각 경계의 환경·스케줄·cleanup 의무를
명시하며 확장한다. 앞 단계의 정리를 나머지 축에 자동 적용하지 않는다.

## 8. 비교 dogfood 실험 계획

Status: **PROPOSED / NOT RUN**. 이 리뷰가 새 구현·벤치마크 작업을 개시하지 않는다.

### 8.1 입력과 비교군

같은 기능과 실패 계약을 가진 세 작은 앱을 고정한다.

1. 파일을 읽어 ADT로 파싱·변환하고 결과를 만드는 CLI: 일반 계산의
   최소 표면, 오류 분기, 컨테이너 소유권을 본다.
2. 취소 가능한 worker와 결과 전달: 중단·취소·join·수명·동시 실패의
   조합을 본다. 2개 기능과 3개 기능을 함께 켠 변형을 둔다.
3. 승인·상태 전이·실패 보상·결과 귀속이 있는 작업: Intent/Authority와
   외부 실행의 실제 차이를 본다. 외부 시스템은 명세된 모사 환경과 실제
   IO 검증을 구분한다.

Pergyra와 C#/Rust/OCaml의 각 idiomatic 구현에 필요한 라이브러리·프레임워크를
포함한다. 비교군에 Pergyra 표기법을 억지로 이식하지 않는다. 팀 숙련도,
도구·버전·최적화·OS·입력·실패 주입을 기록하고, 한 언어에서 제공하지 않는
계약은 기능 미일치로 명시한다. 확인되지 않은 제품 추천이나 성능 순위를
이 계획에서 제시하지 않는다.

### 8.2 측정 벡터

| 축 | 측정 |
| --- | --- |
| 수동 부담 | 고정 AST/source 경계의 계약 반복, annotation, 수동 cleanup·복사·복원, framework glue. 생성 코드와 직접 유지하는 코드는 분리한다. |
| 추론 | 생략이 가능한 site/전체 해당 site, 사용자가 예상한 결과와 실제 결과의 차이. keyword 전체 빈도는 보조 자료다. |
| 수정·진단 | 고정 결함의 원인 식별 시간, 수정 시도, 잘못 고친 횟수, 주진단부터 실제 owner까지의 거리. |
| 계약 보존 | 명세한 위반이 compile reject/runtime reject/미검출 중 어디에 드는지와 정상 입력의 잘못된 거부를 함께 기록한다. |
| 원격 파손 | 공개 본문·요약의 정해진 mutation별 재검증/거부 호출자 수, 변경 사실을 지목한 진단 비율. |
| compiler 비용 | cold/incremental wall time, peak memory, owner별 반복·직렬화 양, 의미상 같은 프로그램의 중복 검사 횟수. |
| 실행 비용 | 같은 작업의 latency, throughput, peak memory, allocations/copies/checks, 실제 실패·취소·정리 비용. |
| 학습·도구 비용 | 처음 필요한 개념 수, 문서 왕복, LSP 계약 설명, 프레임워크와 compiler 내부를 알아야 해결된 작업 수. |

한 aggregate 점수로 안전성과 비용을 상쇄하지 않는다. AI를 쓰는 경우 모델,
prompt, 도구, 예산을 고정하고 평가용 반례를 작성 과정에서 숨긴다. 에이전트가
compiler의 수용 패턴에 맞춰 corpus를 만드는 편향을 피하려고 외부에서 독립
작성한 과제와 사람의 수정 세션을 포함한다.

### 8.3 반증 조건과 종료 기준

- 반복 표기는 줄었지만 진단·수정 시간이나 개념 부담이 증가하면 작은 사용자
  부담이 입증된 것으로 보지 않는다.
- 더 많은 모델·evidence를 만들었지만 중요한 violation이 admission을 통과하면
  계약 밀도 증가를 안전성 향상으로 세지 않는다.
- compile 비용이 의미 입력 증가보다 빠르게 늘면 반복된 owned operation을
  먼저 식별한다. 일반 cache·worker·시간 제한 증가는 이 리뷰의 처방이 아니다.
- pairwise fixture만 통과하고 3자 cycle·취소/borrow/cleanup 조합을 다루지
  못하면 합성 안전성이 닫혔다고 하지 않는다.
- 최소 fragment의 정상 입력을 대부분 거부해서 얻은 안전성은 지원 범위의
  축소로 별도 보고한다.

정량 임계치는 실험 전에 각 workload와 개발 예산에 맞춰 정한다. 이 문서는
근거 없는 성공률이나 상대 우위 숫자를 만들지 않는다.

## 9. 증거와 미확인 항목

문서 검증은 실제 실행했다. 엄격 UTF-8, LF, 후행 공백, 마지막 개행과
본문의 로컬 링크 26개를 확인했고, 저장소의
`tests/documentation_quality_smoke.sh`가 60초 예산 안에 통과했다.
이는 문서 검사 결과이며 아래 구현·비교 실험의 통과로 세지 않는다.

이번 리뷰는 위에 연결한 계약·production source·gate 정의를 읽고, Koka 원문
및 POSIX descriptor 재사용 명세를 확인했다. 기존 gate의 존재와 과거 문서의
PASS 표기를 이번 실행 결과로 옮기지 않았다. 현 후보의 전체 build, C/LLVM
합성 실행, 새 3자 반례, 별도컴파일 계약 변경 실험, 사용자 시험, 비용 비교는
수행하지 않았다.

다음 판단은 열린 상태다.

- 외부 사용자의 정상 코드를 추론이 얼마나 받아들이는가.
- 추론된 공개 계약을 고정·변경·검증하는 production 경계가 어디까지 닫혔는가.
- 각 의미 축의 필요한 의무가 빠짐없이 발급되고 실제 실행까지 보존되는가.
- compiler 내부와 source 양쪽의 복잡성이 줄었는가, 함께 늘었는가.

따라서 본 논지는 채택된 설계 목표의 설명으로 사용할 수 있으나, “풍부한 언어가
이미 더 쉽고 안전하다”는 완료 주장을 뒷받침하지 않는다. 다음 강한 증거는
문서의 추가 확장이 아니라, 명명한 작은 fragment와 같은 앱의 비교 결과다.
