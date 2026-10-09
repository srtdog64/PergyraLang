# 208. 계층별 복잡도 최소화: 의미는 언어에, 기계적 책임은 증거가 있는 곳에

Updated: 2026-10-09 (Asia/Seoul)

Status: **DESIGN + SOURCE REVIEW**. 책임 배치의 최적화 원칙과 제한된 현재
구현 검토다. 보편적인 최적성 정리, Rocq 증명, 새로운 언어 계약 또는 구현
완료 선언이 아니다. 현재 판정은 **설계 적합 / 구현 부분 적합 / 최소성 미검증**이다.

사용자의 질문과 첨부 글을 출발점으로 삼았다. 영상 URL이나 영상 원문은
제공되지 않았으므로 영상 자체의 주장을 검증한 것은 아니다. 외부 근거는
아래의 공식 명세와 원논문이고, Pergyra 평가는 직접 읽은 저장소 계약과 소스에
한정한다. 진행 중인 ownership 전환의 순서·의미·폐쇄 상태를 바꾸지 않는다.

## 1. 목적과 사실 소유자

- 목표: 같은 의미·안전성·자원 예산을 지키면서 사용자의 수동 증명과 기계의
  중복 처리를 줄이는 책임 배치를 설명하고, Pergyra가 실제로 그 역할을 하는지 확인한다.
- 우선순위: 의미 보존과 안전 경계 → 한 사실 소유자와 inspectability →
  불필요한 사용자 부담 감소 → 기존 compile/runtime 예산 → 구조의 단순함.
- 의미 소유자: [VISION](00_vision.md), [compiler contracts](37_compiler_contracts.md),
  [ownership clean](semantics/27_ownership_clean.md),
  [memory-boundary composition](semantics/28_memory_boundary_composition.md).
  이 문서는 그 계약의 대체물이 아니다.
- 사실 발급자와 마지막 소비자: 실제 semantic/storage/call owner가 발급하고
  IR이 운반하며, 적용 C/LLVM emitter와 runtime 경계가 소비한다. ABI의 owner와
  이관 상태는 [protocol/ABI registry](192_protocol_abi_api_registry.md)가 소유한다.
- 금지 fallback: 표기를 늘려 누락된 추론을 숨기기, 검사 없이 동적 실행으로
  넘기기, backend가 의미를 재구성하기, OS 보호를 객체 수명 증거로 간주하기.
- 검증: 이번에는 source/contract 대조와 문서 gate다. 실제 복잡도 감소의
  완료 조건은 §6의 같은-input 비교와 반증이다. 새 최적화 구현 트랙을 열지 않는다.

## 2. 제약을 없애는 것과 사용자에게 제약을 떠넘기지 않는 것은 다르다

POSIX는 운영체제 interface와 환경의 **source-level portability** 계약이다.
binary portability는 명세 범위 밖이다. ABI는 인자·반환값·layout·register·stack
등 바이너리 경계의 계약이다. 예를 들어 Windows x64에는 register 전달과
shadow space 규칙이 있다. 둘은 borrow checker 같은 소유권 판정 알고리즘과
동일한 종류가 아니며, 모든 target이 POSIX를 사용하는 것도 아니다.
([POSIX.1-2024 scope](https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap01.html),
[Windows x64 calling convention](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170))

따라서 “아래 계층이 제약을 맡으면 언어는 무제약이어도 된다”는 결론은 맞지
않는다. ABI가 호출 모양을 정해도 누가 배열을 해제할지는 정하지 않는다.
POSIX도 일반 interface 사용 규칙에서 수명이 끝난 객체의 포인터를 invalid
argument로 다룬다. 동일 주소의 새 객체가 있다고 옛 포인터를 유효하게 보지 않는다.
([POSIX interface use](https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html))

Pergyra의 목표는 필요한 제약을 없애는 것이 아니라 **사용자는 의미 경계를
작성하고, 그 경계를 지키는 기계적 절차는 기계가 맡게 하는 것**이다.
명시적인 권한·외부 소유 이전·caller-visible mutation·손실/비용 허용은
실제 사용자 결정이다. 지역 변수마다 해제 순서를 작성하거나 field를 꺼내
넘긴 뒤 다시 넣는 절차는 컴파일러가 책임질 수 있는 후보다.

자유도도 구분한다. 모든 pointer 연산을 받아주는 자유와, 같은 의미의
프로그램을 적은 의례로 안전하고 예측 가능하게 작성하는 자유는 같지 않다.
불가능한 수명이나 충돌하는 변경을 거부하는 안전 조건과, 정상 코드를
현재 추론기가 처리하지 못하는 구현 한계를 같은 “필수 언어 제약”으로 묶지 않는다.

## 3. 계약의 위치, 증거의 위치, 집행의 위치를 분리한다

한 의미에는 한 owner가 필요하지만, 그 의미를 지키는 집행점은 여러 곳일 수 있다.
서로 다른 boundary state를 검사하는 것은 중복 의미 소유가 아니다.

| 책임 | 계약/의미의 위치 | 증거·집행을 맡길 위치 | 사용자에게 남길 것 |
| --- | --- | --- | --- |
| 일반 값의 수명·이동·정리 | 언어의 값/소유권 계약 | typed owner의 allocation/loan/exit 사실, IR 정리 합성, runtime glue | 필요한 실제 소유 이전과 자원 종료 경계 |
| 배열/view 접근 | 값·view 계약 | 정적 범위/수명 증거; 계약이 정한 동적 bounds/state 검사 | 의미 있는 공유·변경 요구, 실패 처리 |
| 재사용되는 Slot/graph identity | resource/authority 계약 | runtime의 현재 generation·grant·pin 상태; compiler의 정적 사용 제한 | 주체·권한·수명 경계 |
| FFI와 binary interop | extern/API 계약과 target ABI | compiler의 layout/carriage 검증, adapter의 실제 외부 결과 검사 | 외부 retention/transfer 계약과 unsupported target |
| 파일·프로세스 권한 | application/authority 정책 | adapter의 상태 처리, OS의 실제 권한·격리 집행 | 정책과 외부 실패의 의미 |
| linking·SDK·배포 | target/toolchain 계약 | linker/toolchain/platform discovery | target과 명시적인 배포 요구 |

이는 책임 배치 **판단표**이지 위 모든 항목의 현재 Pergyra 지원 선언이 아니다.
language specification, compiler, stdlib/runtime, toolchain, OS, hardware를
단순한 위아래 사다리로 보지 않는다. 서로 다른 정보와 권한을 가진 owner들이다.
lang-level authority 선언만으로 OS sandbox가 완성되지 않으며, process 격리만으로
process 내부의 use-after-free가 해결되지도 않는다.

기본 선택 순서는 다음과 같다.

1. 원래 의미와 관찰 가능한 실패를 소유하는 계약을 먼저 고정한다.
2. 필요한 정보를 이미 가진 owner가 증거를 한 번 발급한다. 증거에는 적용
   대상과 generation/lifetime이 있어야 한다.
3. 소비자는 유효한 증거를 사용한다. 같은 사실을 source spelling이나 AST
   재순회로 추론하지 않는다. 입력 변경·외부 상태 변화에는 새 검증이 필요하다.
4. 계약에 이미 정의된 동적 검사는 runtime이 집행한다. 정적으로 모른다는
   이유만으로 새 “런타임에서 알아서” 경로를 추가하지 않는다.
5. 필요한 증거도, 정당한 동적 집행 계약도 없으면 해당 경계를 거부하고
   누락된 사실과 가능한 정상 작성법을 진단한다.

핵심은 “항상 가장 낮은 계층”도 “항상 compiler”도 아니다. **필요한 정보를
충분히 가지고, 그 정보의 수명과 실패를 책임질 수 있는 위치**가 후보가 된다.
End-to-end 논문도 아래 계층의 검사가 최종 application 보장을 자동으로
대체하지 않으며, 배치는 성능 tradeoff라고 설명한다. 이는 Pergyra 전체의
최적 배치를 증명한 논문은 아니다.
([End-to-End Arguments in System Design](https://web.mit.edu/Saltzer/www/publications/endtoend/endtoend.pdf))

### UB와 최적화에서 지켜야 할 선

LLVM은 immediate UB와 `willreturn`이 있는 예에서 UB 이전의 I/O까지 보존할
의무가 없음을 설명한다. 그 계약 아래의 제거는 곧바로 compiler bug를 뜻하지
않는다. 그러나 Pergyra가 보장한 관찰을 잘못된 lowering이나 과도한 attribute로
LLVM UB에 넘긴다면 **Pergyra의 정제 의무 위반**이다. OS나 ABI가 이미 제거된
관찰을 일반적으로 복구해 주지는 못한다. 다른 UB 종류·attribute 조합까지
같은 예로 일반화하지 않는다.
([LLVM UB manual, Time Travel](https://llvm.org/docs/UndefinedBehavior.html#time-travel))

## 4. 복잡도 최소화 원칙: 현재는 조건부 연구 가설

고정 workload 집합 `W`, target/toolchain `T`, 계약 `K`, 기존 자원 예산 `B`를
먼저 정한다. `Pi_admitted(W,T,K,B)`는 그 범위에서 의미·실패·권한·수명과
자원 제약을 만족한다는 증거가 있는 책임 배치들의 집합이다. 미검증 후보는
단지 비용이 작아 보인다는 이유로 이 집합에 넣지 않는다.

배치 `pi`의 비용은 처음부터 한 숫자가 아니라 다음 벡터로 기록한다.

```text
C(pi) = (H, S, R, X, M)
H: 사용자 규칙/수동 절차/진단 해석 부담
S: compile·analysis 시간과 peak memory
R: 실행·할당·보유량·정리 지연·동기화 비용
X: 경계 변환·증거 운반·반복 검증 비용
M: 변경 전파·API 호환·debugging·owner 유지 비용
```

안전성을 비용 항목으로 넣어 다른 항목과 교환하지 않는다. 단위가 다른 비용을
그대로 더하지도 않는다. 실제 비교는 항목별 budget과 Pareto 비교부터 시작한다.
가중 합을 쓰려면 workload별 정규화와 가중치·측정 방법을 공개해야 한다.
사용자 시간은 소스 keyword 수로 대체하지 않는다.

**제안하는 최적화 원칙:** 같은 계약을 만족하는 두 배치 사이에서, 충분한
증거의 수명 안에 있는 기계적 책임을 사용자나 무정보 소비자에게서 사실
owner로 옮긴다. 그 결과 다른 비용을 악화시키지 않으면서 적어도 한 비용을
줄였다면 그 범위에서는 개선이다. 비용이 교환되면 tradeoff로 기록한다.
이 조건이 실제로 성립하는지는 아래 gate로 확인해야 한다.

“지배되는 후보는 최소 후보가 아니다”라는 Pareto 정의를 새로운 최적성
증명으로 포장하지 않는다. 현재는 비용 실측과 배치 의미론이 없으므로
“Pergyra가 총복잡도를 최소화했다”는 정리를 주장할 수 없다.

향후 제한된 정리로 만들려면 후보 배치의 연산·관찰·거부 의미, evidence
validity와 invalidation, 실행에서 센 비용, 합성 법칙부터 정의해야 한다.
정적 분석의 건전한 근사는 abstract interpretation과 연결되고, 정적·동적
판정을 결합하는 방법은 hybrid type checking과 연결된다. 두 연구 모두
Pergyra의 total human/system cost 최적성을 대신 증명하지 않는다.
([Cousot & Cousot, 1977](https://cs.nyu.edu/~pcousot/COUSOTpapers/POPL77.shtml),
[Flanagan, Hybrid Type Checking, 2006](https://escholarship.org/content/qt0j63v3dn/qt0j63v3dn.pdf))

검사 제거도 별도 조건부 의무다. 제거할 판정이 이미 발급된 사실에서
도출되고, 소비까지 모든 관련 변경이 증거를 보존하거나 무효화하고, 실패
관찰도 달라지지 않을 때만 제거할 수 있다. 옛 handle의 compile-time 증거가
현재 runtime generation을 보장하지 않으면 generation 검사는 지울 수 없다.
정적 증거의 재사용과 실행 중 변한 외부 사실의 재확인은 구분한다.

## 5. 현재 Pergyra는 이 역할을 하고 있는가?

조사 HEAD는 `f8173daa0ac607cdf604d1452c43a81e06b983ba`다. main의 다른 작업은
계속 편집 중이므로 전체 트리가 frozen이라는 뜻이 아니다. 이번 검토는 아래
source anchor의 읽기와 계약 대조이며 새로운 compiler 빌드·runtime 성능
측정·Rocq 커널 검사·CI 검증이 아니다. 오래된 설치본의 결과를 현재 소스의
결과로 사용하지 않았다.

| 항목 | 직접 확인한 근거 | 판정 |
| --- | --- | --- |
| 사용자가 의미를 쓰고 기계가 파생한다 | [VISION DX/cleanup 절](00_vision.md), [27 §5.10](semantics/27_ownership_clean.md) | 설계 적합. 목표와 구현 완료를 구분하고 있다. |
| `inout` field의 수동 carrier/restoration | native [type_checker_helpers_late.c](../src/semantic/type_checker_helpers_late.c)의 non-identifier 거부와 local 추출·복원 진단; self-host [inout verdict owner](../src/self_hosted/semantic/ast_inout_argument_alias_verdict_owner.pgy)의 direct-binding 제한 | 아직 사용자 부담이 남는다. 실제 별칭 검사는 유지하되 정상 field place의 기계적 처리까지 언어의 영구 제약으로 삼으면 목표와 어긋난다. |
| 자동 정리의 실제 발급·호출·view 연결 | [27의 production refinement OPEN 목록](semantics/27_ownership_clean.md), [207의 구현 경계](207_compiler_owned_cleanup_algorithm.md) | 제한 core의 증명은 근거지만 현재 언어 전 범위 자동화나 최소성의 증거는 아니다. CL7 no-call scope와 실제 call-bound Slice는 별개다. |
| release 사실이 없을 때 | self-host [collection ownership verdict](../src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy)의 `aggregate_release_plan_unproved` | 안전 방향의 거부가 존재한다. 이 분기가 있다는 사실은 특정 현재 프로그램이 실패했다거나 모든 release 경로가 닫혔다는 실행 증거가 아니다. |
| ABI shape와 optimizer 약속 분리 | [llvm_decl.c](../src/codegen/llvm_decl.c)와 [143](143_evidence_parameter_attributes.md): carriage로 `noalias`/`readonly`를 발급하지 않음 | 올바른 책임 구분의 구현 사례. optional optimizer 증거가 없으면 attribute를 생략한다. 필수 ownership/ABI 사실 누락을 허용하는 fallback과 다르다. |
| 필수 ABI 사실의 소비 | [MIR fact validator](../src/compiler/mir_fact_surface_validate.c)와 [C resource emitter](../src/codegen/transpiler_mir_resource_op_core.c)의 missing/mismatched ABI 거부; [192 registry](192_protocol_abi_api_registry.md)의 BRIDGE 상태 | owner-directed 소비와 fail-closed 코드가 있다. 모든 경로 이관이나 C/LLVM refinement CLOSED는 아니다. |
| 실제 동적 state | [slot_manager_core_ops.c](../src/runtime/slot_manager_core_ops.c)의 재사용 generation 발급; [slot_manager_pin.c](../src/runtime/slot_manager_pin.c)의 pin generation 검사 | 정적으로만 해결할 수 없는 상태를 runtime에 둔 사례다. 전체 Slot·graph 안전성이나 동시성 검증을 여기서 재확인한 것은 아니다. |

결론: Pergyra는 **복잡성을 없애는 언어**보다 **의미를 보존하면서 기계적
복잡성을 적절한 owner로 옮기는 언어**로 설명하는 것이 정확하다. 그 방향은
실제 소스 일부와 맞지만 사용자 수명 의례와 production refinement의 열린
경계가 남아 있다. 총복잡도와 authoring 부담의 최소성은 아직 측정하지 않았다.

### 반증으로 유지할 사례

- field place를 허용하면서 같은 place, root+child 또는 공유 backing을 두
  독립 소유자로 인정하면 DX 개선이 아니라 안전성 훼손이다.
- copy를 무조건 borrow로 바꾸면 원본 변경이 관찰에 새어 나올 수 있다.
  표기 감소만으로 값 의미론 보존을 주장하지 않는다.
- raw `Unpack`은 중복 소유권을 가진 malformed 상태에서 남의 block을 지울 수
  있다. admitted `INV` 유지가 전제인 core 건전성과 무조건 raw-state 안전성은 다르다.
- OS page protection과 올바른 호출 ABI가 있어도 같은 process 안의 해제된
  객체·재사용된 handle·이미 제거된 효과를 일반적으로 복구하지 못한다.
- cleanup을 마지막 사용으로 당기면서 관찰 가능한 resource finalizer의 순서를
  바꾸거나, 취소 요청만으로 worker가 쓰는 저장소를 해제하면 의미가 달라진다.
- 다른 module의 변경이 inferred mode/copy 비용을 바꾸면 계약 version과 원인이
  inspectable해야 한다. 조용한 공유 의미 변경은 자동화의 편의가 아니다.

앞의 copy/Unpack 반례와 여섯 자동 정리 trace는 이전 격리 검증에서 확인했다.
ignored 경로 `.tmp/ownership-manual-contrast-2026-10-09-side-01/README.md`의
15개 axiom-free 명제와 30개 추출 판정이 그 **한정 모델**의 근거다. canonical
core가 의미의 소유자이며 이 scratch suite는 CI 등록이나 production 메모리
검증의 대체물이 아니다. 이번 문서 작업에서 다시 실행한 것은 아니다.

## 6. 실제 감소를 판정하는 gate와 다음 작업 경계

새 실험 track 대신 현재 ownership 전환의 같은 acceptance 입력에 다음 관찰을
붙이는 것이 우선이다. 실제 gate 연결은 해당 구현 owner의 승인된 변경에서 한다.

1. **Authoring:** 고정 fixture/AST 범위에서 불필요한 explicit drop,
   field 추출·복원 쌍, readonly 임시 binding, 추가 own/ref 표기를 전후 비교한다.
   정당한 transfer/authority/FFI 표기는 별도로 분류한다. global keyword census는
   자동화나 인지 부담의 직접 측정이 아니다.
2. **Meaning:** 같은 입력의 적용 C/LLVM 경로에서 acceptance, 결과, 효과·정리
   순서, 안정적 실패가 같아야 한다. 분기 이동·조기 반환·처리된 에러·view
   마지막 사용·재사용·부분 초기화 실패를 포함한다. manual release와 합성
   cleanup의 이중 소비가 없어야 한다. source producer부터 마지막 consumer까지 본다.
3. **Static cost:** 고정 source/hash·executable·toolchain에서 시간, peak memory,
   같은 사실의 발급/재검증 횟수를 기록한다. 새 gate 허용량을 늘려 감소처럼
   만들지 않는다. 소스의 반복 패턴만으로 병목을 선언하지 않는다.
4. **Runtime/boundary cost:** 같은 value/copy/allocator 의미에서 시간·peak live
   bytes·할당/복사량·정리 tail과 사실 운반 비용을 비교한다. hidden Clone/RC/GC,
   silent retry, 공유 backing이나 value semantics 변경은 다른 정책이다.
5. **Change predictability:** callee 요약이 바뀌는 반증 fixture에서 영향을 받은
   derived mode/copy와 진단을 확인한다. 한 증거 identity가 source→IR→backend까지
   이어지고, missing/stale/contradictory fact는 해당 경계에서 거부되어야 한다.

현재 보장할 수 있는 완료는 이 원칙의 문서화와 source 대조까지다. 위 비용은
**UNMEASURED**, 전 범위 자동화와 일반 최적성은 **OPEN/UNPROVEN**이다.
실행 최적화는 기존 정책대로 다음 활성 폐쇄 단계가 측정된 비용으로 막힐 때만
그 owner에서 한다. 이 문서는 새 cache/query engine, 새로운 문법, broad runtime
fallback 또는 ownership 전환 순서 변경을 승인하지 않는다.

한 문장: **언어는 사용자가 지켜야 할 의미를 모으되, 그 의미를 유지하는
기계적 책임은 충분한 정보와 검증 가능한 계약을 가진 계층에 배치한다.**

## 7. 이번 문서 변경의 확인 범위

2026-10-09에 `tests/documentation_quality_smoke.sh`가 exit 0으로 통과했다.
이 문서와 INDEX의 strict UTF-8 검사 및 local link target 197개 확인도 통과했다.
검토한 inout native/self-host owner, LLVM declaration, MIR ABI validator와 C
resource emitter는 읽기 전후 SHA-256이 같았다. 이는 그 소스 대조의 안정성이지
다른 작업자가 편집 중인 전체 repository의 동결이나 행동 검증이 아니다.

이 변경은 새 문서와 INDEX 안내만 수정했다. compiler/runtime/proof/gate,
registry 상태, Git index와 원격은 변경하지 않았다. 다른 작업자의 미커밋
변경을 보존했다. 문서 gate PASS는 비용 감소·새 ownership 구현·CI green이 아니다.
