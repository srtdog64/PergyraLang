# Advanced parallel examples and async lifecycle map

Updated: 2026-09-05

이 문서는 현재 실행 예제와 미구현 조합을 구분하는 안내서다. 의미론의 정전은
[memory/concurrency contract](../113_memory_concurrency_model.md),
[parallel boundary evidence](../178_parallel_boundary_evidence.md),
[join surface](../181_parallel_surface_full_design.md)다. 이 문서나 예제 수는
SoT 폐쇄율 또는 self-host 대체율을 올리지 않는다.

## Evidence boundaries

- **Native fixture**: 기존 C/LLVM 비교 corpus에 들어 있는 실행 입력. 이 문구
  자체는 이번 실행 PASS 또는 self-host source 지원을 뜻하지 않는다.
- **Contract**: 해당 owner가 선언한 범위. 모든 경로의 구현 완결성 주장이 아니다.
- **Model only / OPEN**: bounded proof 또는 설계 방향. 실행 가능한 문법으로
  소개하지 않는다.
- 이번 실행의 revision, binary hash, 결과 및 생략 범위는
  [검토·검증 기록](../audits/2026-09-05_concurrency_review_reconciliation.md)에 둔다.

## Nine lifecycle dimensions

[A Design Space Exploration of Async/Await](https://arxiv.org/html/2608.20677v1)
§3은 시작 2축, 종료 4축, 취소 3축을 비교한다. 아래는 그 질문에 기존 Pergyra
owner를 연결한 표이지 새로운 async 계약을 선언한 표가 아니다. 논문의 비교
대상 언어와 Pergyra가 같은 키워드를 쓴다고 같은 lifecycle인 것은 아니다.

| Dimension | Pergyra에서 현재 읽을 수 있는 계약 | 남은 경계 / 근거 |
|---|---|---|
| Eagerness | named `spawn`이 작업 생성 경계다. `async` 선언만으로 병렬 시작을 뜻하지 않는다. | 일반 async 호출의 시작 순서를 논문의 한 분류로 고정할 충분한 계약은 이 표에서 주장하지 않는다. [user guide](../05_async_concurrency.md) §2–3 |
| Suspension | `await`는 완료 join이다. 반드시 양보한다거나 다른 작업이 반드시 실행된다고 추론하지 않는다. | 완료된 Future와 각 lane의 대기 경로를 구분해야 한다. [contract](../113_memory_concurrency_model.md), Future Await Contract |
| Extent | named Future는 모든 정상 exit에서 `await` 또는 명시 `own Future` 인계로 retire해야 한다. `parallel`은 자기 블록을 join한다. | 익명 capture/detach, exceptional exit 전체 보장은 별도다. [lifecycle owner](../../src/semantic/type_checker_future_lifecycle.c) |
| Reference Strength | Future는 복사 가능한 일반 task reference가 아니라 affine handle 계약이다. | GC의 strong/weak 분류와 동일시하지 않는다. aggregate carriage의 알려진 반례가 남아 있다. [기존 감사](../audits/2026-09-05_counterexample_attack_results.md) |
| Destruction | named handle의 미처리 scope exit를 거절한다. 숨은 finalizer나 자동 cancel+join으로 설명하지 않는다. | `parallel`의 암묵 join과 named Future의 명시 retirement는 서로 다른 규칙이다. [contract](../113_memory_concurrency_model.md) |
| Propagation | `RemoteFuture<T>`의 join 결과는 `Result<T>`다. | 일반 panic-to-sibling cancellation, 모든 실패의 집계·정리는 미완성 범위다. [proof obligations](../semantics/proofs/ProofSpine.md) |
| Awareness | `Cancel`은 협조적 요청, `IsCancelled`는 관찰이다. 요청만으로 handle은 retire하지 않는다. | 실행 중 임의 외부 호출의 강제 중단을 보장하지 않는다. [guide](../05_async_concurrency.md) §6 |
| Direction | 현재 spawn runtime은 parent context와 cancellation chain을 전달한다. | 이를 모든 lane에서 동일한 취소 관찰 순서 또는 동시 전파라고 확대하지 않는다. [async models](../semantics/proofs/AsyncModelCores.md) |
| Persistence | 취소를 관찰하고 계속 실행하는 것과 취소 요청을 지우는 것은 다르다. | 논문의 transient/persistent 한 칸으로 전체 runtime을 고정하는 반복 관찰·cleanup 계약은 OPEN이다. [direction models](../semantics/proofs/AsyncDirectionCores.md) |

`async`에 lifetime, authority, cancellation을 다시 몰아넣거나 새 `scope`
키워드를 도입해서 빈 칸을 메우지 않는다. 필요한 것은 기존 owner의 명시 계약과
positive/negative 증거다.

## Executable reading order

아래 여덟 프로그램은 native fixtures다. 코드를 복제하지 않고 원본 입력을
그대로 읽고 실행한다. 각 디렉터리의 `expected.stdout`은 기존 backend compare도
소비하는 정확한 출력 기준이다. C와 LLVM이 같은 오답을 내는 것만으로 통과하지
않도록 기준값을 별도로 고정한다.

| 원본 프로그램 | 함께 사용한 모델 | 정확한 stdout (행 구분 `/`) |
|---|---|---|
| [parallel_snapshot_read](../../tests/cases/backend_compare/parallel_snapshot_read/main.pgy) | channel 신호 + single writer + reader snapshot | `42/1` |
| [parallel_disjoint_split_write](../../tests/cases/backend_compare/parallel_disjoint_split_write/main.pgy) | 한 배열의 서로소 Slice + 두 작성자 + post-join 집계 | `110` |
| [parallel_pingpong_witness](../../tests/cases/backend_compare/parallel_pingpong_witness/main.pgy) | capacity-1 양방향 통신 + 다중 문장 arm | `10/10/20` |
| [parallel_join_expr](../../tests/cases/backend_compare/parallel_join_expr/main.pgy) | 결과 수집 + index-order + 두 번째 병렬 단계 | `204/182/2/72` |
| [parallel_join_stencil](../../tests/cases/backend_compare/parallel_join_stencil/main.pgy) | 이전 tick 이웃 읽기 + 다음 tick 자기 index 쓰기 | `33/33/33/0/0/100/103` |
| [parallel_join_reduce](../../tests/cases/backend_compare/parallel_join_reduce/main.pgy) | checked reduction + empty identity + Float | `204/24/3/-2/0/1` |
| [parallel_join_any_blocked](../../tests/cases/backend_compare/parallel_join_any_blocked/main.pgy) | first-give 경쟁 + 대기 중인 패자 취소 + 전체 join | `42` |
| [parallel_scheduler_showcase](../../tests/cases/backend_compare/parallel_scheduler_showcase/main.pgy) | bounded queue + 분배 + split write + select + snapshot + spawn 검산 | `8/0/4/4/4/1/34552/34552/69104/69104/1` |

### Snapshot is not a shared read

`parallel_snapshot_read`의 reader는 writer의 channel 신호 뒤에 실행되지만
여전히 진입 시점의 `x = 1`을 본다. channel의 happens-before가 이미 복사된
snapshot을 공유 주소로 바꾸지는 않는다. 부모는 join 뒤 writer의 `42`를 본다.
따라서 race-free와 latest-value는 다른 약속이다.

### Independent computation is not a communicating protocol

서로 독립적인 계산은 정규 순차 결과와 비교할 수 있다. 그러나 ping-pong처럼
상대 arm의 메시지가 있어야 진행하는 코드를 arm별 run-to-completion으로
직렬화하면 진행하지 못한다. 모든 `parallel`에 직렬 lowering을 허용하는
보장이 아니다. 일반 deadlock freedom이나 scheduler fairness도 이 예제로
증명되지 않는다.

### Stencil and reduction keep different obligations

stencil의 읽기 배열과 쓰기 배열은 별도 backing이어야 하고, writer는 자기
index만 수정한다. join 뒤에 다음 상태를 사용한다. reduction은 현재
**fixed index-order left fold**다. 임의 tree나 완료 순서로의 재결합이 아니다.
Float의 fold 순서 고정은 SIMD/FMA·numeric profile 전체의 동일성 증명이 아니다.

### First result is not first success

`join with any`는 최초 `give`를 선택한다. **First result is not first success**:
first-success, quorum, deadline은 추가 결과·정리 정책이 필요한 별도 조합이다.
`any`는 unordered all도 아니다. 승자 선택 후에도 패자 취소와 전체 join을
마쳐야 context 수명을 끝낼 수 있으며, 패자의 외부 효과가 rollback되지는 않는다.

### Cancel is not join

[중첩 취소 예제](../../tests/cases/backend_compare/future_cancel_propagation/main.pgy)는
child를 만드는 parent도 자기 child를 join해야 함을 보여준다.
**Cancel is not join**: `Cancel(parent)` 뒤에도 `await parent`가 필요하다.
취소 요청 전에 task가 완료할 수 있으므로 특정 취소 분기나 출력값을 무조건
기대하지 않는다. 이 시간 민감 예제는 위 exact-output 묶음과 구분한다.

## Hard combinations that are not yet implemented as a general feature

다음은 새 Pergyra 의사문법으로 위장하지 않는 acceptance worklist다. 메인 세션의
active self-host rung을 대체하거나 동시에 여는 구현 지시가 아니다.

| 조합 | 필요한 계약 / 반증 조건 | 현재 경계 |
|---|---|---|
| first-success / quorum | 실패가 먼저 도착해도 성공으로 선택하지 않기; 중복 응답을 quorum에 두 번 세지 않기; 패자 정리 후 반환 | `any`만으로 구현 완료 아님 |
| deadline + nested cancellation | timeout 통지와 실제 종료 분리; child가 정리되기 전 자원 수명 종료 금지 | 강제 중단·일반 exceptional cleanup 미완성 |
| capability lend/revoke/return | 대여 중 lender 사용 불가, return 뒤 복귀, 취소 중 정확한 정리 | parent context capture는 실물; 일반 edge carriage는 model only |
| Slot 재사용 + async 결과 | slot/type/generation을 재검증; 확인과 사용 사이도 보호된 접근으로 묶기 | 일반 자동 resume revalidation은 OPEN |
| 같은 entity의 새 요청 | generation이 같아도 이전 요청의 결과는 최신 요청을 덮지 못함 | 요청 revision과 결과 귀속은 응용 프로토콜이며 새 언어 키워드가 아님 |
| deterministic tick commit | 결과 순서뿐 아니라 해당 tick이 받아들일 결과 집합·admission 정책까지 고정 또는 기록 | tick/replay 일반 기능은 설계 단계 |
| World/Zone/Intent 결합 | participant·권한·terminal 귀속을 각 owner로 검사; step 수로 Intent를 판정하지 않음 | INT-1~4 전체 및 self-source typed plan 생성은 별도 잔여 |

**Generation is not request revision.** generation은 같은 incarnation인지 묻고,
요청 revision은 그 incarnation에 대한 최신 계산인지 묻는다. identity를 정수
하나로 섞거나 매번 새 언어 개념을 추가하지 말고 기존 owner/응용 계약에 둔다.

Slot 재검증의 [Rocq 모델](../semantics/proofs/AsyncDirectionCores.md)은 unbounded
`nat`를 사용한다. 실제 `uint32_t` generation의 wrap-around, runtime의 검증과
사용 사이 경쟁, compiler의 resume 검사 삽입까지 증명됐다고 주장하지 않는다.

## Running the bounded gate

```sh
PGY_BIN=/absolute/path/to/pgy bash tests/concurrency_examples_smoke.sh
bash tests/async_model_positioning_smoke.sh
```

실행 gate는 native C/LLVM을 명시적으로 검사한다. self-host 거절을 native
재시도로 숨기는 경로가 아니다. 기존 binary를 사용하고 rebuild/install하지
않는다. 여덟 프로그램을 backend당 한 번씩 실행하며, 정확한 stdout·정상 종료·
빈 runtime stderr를 확인한다. 다섯 기존 거절 입력은 양쪽 backend 입구에서
semantic refusal와 artifact 부재를 확인하고 실행하지 않는다.

전체 예산은 5분, 단일 compile은 최대 45초, 실행은 최대 15초다. timeout이나
누락 도구를 SKIP/PASS로 바꾸지 않는다. 실행 증거는 별도 `.tmp` 디렉터리에
남긴다. 이 수동 집중 gate를 새 CI job으로 등록하지 않는다. 추가된 goldens는
이미 등록된 `tests/compare_backends.sh` 경로가 그대로 소비한다.

## Primary references, not Pergyra implementation authority

- [Async/Await design dimensions](https://arxiv.org/html/2608.20677v1): lifecycle 비교틀.
- [Typestate via Revocable Capabilities](https://pldi26.sigplan.org/details/pldi-2026-papers/80/Typestate-via-Revocable-Capabilities): flow-sensitive capability 수명 참고.
- [Classifying Capabilities](https://arxiv.org/abs/2607.24504): Future의 capture 종류 제약 참고.
- [PS-PDG](https://2026.cgo.org/details/cgo-2026-papers/27/The-Parallel-Semantics-Program-Dependence-Graph-for-Parallel-Optimization): 의미 제약과 물리 실행 계획 분리 참고.
- [WASI 0.3](https://wasi.dev/releases/wasi-p3): component runtime이 소유하는 async 경계 참고. Pergyra의 해당 ABI 지원 완료를 뜻하지 않는다.
