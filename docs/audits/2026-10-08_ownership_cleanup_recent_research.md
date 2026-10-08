# Ownership cleanup: recent research and applicability review

검토일: 2026-10-08 KST. 범위: 자동 소유권 cleanup의 추론·검증·물리 구현 경계.
상태: **research/review; bounded Rocq follow-up recorded below**.
최초 검토는 읽기 전용이었다. 뒤의 사용자 요청으로 합성 증명·추출 하네스만
보완했으며, 논문의 전체 type/effect calculus나 production 알고리즘을 이식한 것은 아니다.

핵심 설명과 도표는 [207](../207_compiler_owned_cleanup_algorithm.md)에 있다.
기존 owner는 [27](../semantics/27_ownership_clean.md)와
[OwnershipCleanCore.v](../semantics/proofs/OwnershipCleanCore.v)다.
아래 적용성 평가는 이 저장소에 대한 검토자의 판단이며, 논문 자체의 결론과
구분한다. 논문의 benchmark 수치를 Pergyra 성능으로 환산하지 않는다.

## 1. 결론

자동 cleanup을 포기하거나 GC/전역 RC로 바꿀 근거는 찾지 못했다.
우선 필요한 것은 새 문법보다 **함수 간 ownership summary, loan이 붙잡는
저장소 identity, 추상 소유권과 실제 allocation의 연결**이다.

연구에서 가져올 후보는 기존 사실 소유자 안에서 검증해야 한다. 논문의
언어 전체, 별도 분석 엔진, Rust 포인터 모델 또는 OCaml 구현을 통째로
들여오는 제안이 아니다. 일반 값·Slot·zone의 책임도 합치지 않는다.

## 2. 확인한 최신 논문과 직접 적용의 한계

검색 결과의 갱신 날짜를 출판 날짜로 사용하지 않았다. 특히 `Free to Move`
초안은 2026-09-26 v2에서 제목과 범위가 바뀌었으므로 아래 최신판을 기준으로 한다.

| 연구 / 확인 수준 | 가져올 후보 — Pergyra에 대한 판단 | 그대로 가져오면 안 되는 전제 |
|---|---|---|
| **Capture Now, Consume Later**, arXiv v2, 2026-09-26. 본문의 §§1–3, 7 확인 | 호출 시 발생하는 use/kill과 captured alias를 함께 추적. callback을 통한 소비 반례에 유용 | preprint이며 자동 drop 삽입 완성품은 아님. idempotent free와 실제 in-place move를 구분해야 함 |
| **Escape with Your Self**, PLDI 2026. 본문의 §§1–2, 알고리즘 설명 확인 | 반환 aggregate/closure의 내부 의존성을 scope 밖에서도 잃지 않는 qualifier 추론·avoidance | 새로운 reachability 타입 체계 전체의 이식은 범위 밖. Pergyra 자동 cleanup의 증명이 아님 |
| **Fully-Automatic Type Inference for Borrows with Lifetimes**, OOPSLA1 2026 | 사용자 lifetime 주석 없이 내부 borrow를 추론하는 방향은 잘 맞음 | 저자 초록과 프로젝트 설명만 확인; 본문 PDF 확보 실패. typing 불가 사례를 RC 연산으로 보완하므로 전략 전체는 채택 불가 |
| **When Lifetimes Liberate**, OOPSLA1 2026 / arXiv v3 2026-03-28. §§3, 6–7 확인 | packet/phase처럼 실제 공동 수명이 입증되는 저장소의 일괄 회수 참고 | scope 종료 회수에 한정. non-lexical 전체 회수나 GC 제거, 실용 추론기 완료를 보장하지 않음 |
| **Sound Borrow-Checking for Rust via Symbolic Semantics**, 2024 long version. §§1–3, join/loop 논지 확인 | 추상 borrow 의미 → 실제 heap 의미의 refinement와 branch join을 별도 의무로 둠 | Rust의 포인터 표현을 Pergyra Slot/handle 표현으로 대체 없이 이식할 수 없음 |

### R1. Capture Now, Consume Later

함수 타입에 잠재된 use/kill을 두어 클로저 생성 시점과 자원 소비 시점을
구별한다. 사용 후 무효화는 직접 변수뿐 아니라 추적된 별칭에도 적용된다.
다만 §2.3의 `free`는 중복 실행을 허용하는 의미이고, §7의 move/swap은
새 위치 할당으로 모델링된다. Pergyra의 exactly-once 물리 drop과 zero-copy
move는 이 정리에서 자동으로 나오지 않는다.
[최신 v2 본문](https://arxiv.org/html/2510.08939v2).

**적용 후보:** 기존 retention/capture owner가 이미 보유한 root 관계를
cleanup 소비까지 유지한다. 언어에 `kill`을 추가하거나 동적 revoke 테이블을
새 기본값으로 도입하지 않는다. Pergyra의 실제 effectful release 순서와
물리 representation에 대해 별도의 refinement가 필요하다.

### R2. Escape with Your Self

반환되는 클로저·복합 값의 내부 자원 이름이 원래 scope를 벗어날 때,
의존성을 지우지 않고 표현하는 avoidance 및 bidirectional typing을 제시한다.
알고리즘의 soundness와 decidability를 Lean에서 검증했다. 이는 단순히
`owner 변수가 더 이상 안 보인다 → 해제 가능`이라고 판단하면 안 된다는
설계 참고가 된다.
[PLDI 2026 저자 본문](https://continuation.passing.style/static/papers/pldi26.pdf).

**적용 후보:** 반환 값/closure가 도달하는 저장소를 stable owner identity에
연결한다. 먼저 기존 typed facts에 그 관계가 있는지 확인하고, 필요한 관계만
보완한다. 이 논문을 이유로 새 범용 type inference/query engine을 만들지 않는다.

### R3. Fully-Automatic Type Inference for Borrows with Lifetimes

순수 함수형 Morphic에서 lifetime borrow를 자동 추론하며, 불가능한 경우에는
참조계수 연산을 넣는다. 사용자 주석을 줄이는 목표는 맞지만 모든 입력에
GC/RC 없이 적용되는 소유권 알고리즘은 아니다.
[저자 공개 초록](https://www.cs.princeton.edu/~mpmilano/publication/fully-automatic-type-inference/),
[Morphic 공식 설명](https://morphic-lang.org/).

**적용 후보:** 본문·artifact를 추가 검토한 뒤 borrow lifetime 제약과
함수 결과의 의존성 요약을 비교한다. 현재는 조사 후보이며 구체 규칙 채택은
보류한다. 해결 안 된 입력을 silent RC fallback으로 받지 않는다.

### R4. When Lifetimes Liberate

shadow arena로 공동 수명 자원을 묶고 scoped arena를 일괄 해제하는
Rocq 모델이다. 최신 §7은 scope 중간/일반 arena의 회수, GC의 완전 제거,
실용적인 타입 추론·구현을 완료 범위로 주장하지 않는다.
[최신 v3 본문](https://arxiv.org/html/2509.04253v3).

**적용 후보:** packet 단위로 공동 수명이 확인된 뒤에만 기존
[region owner](../197_region_arena_strategy.md)와 접점을 검토한다.
현재 일반 cleanup 공백을 arena로 덮거나 원소별 owner 검증을 생략하지 않는다.
성능 최적화는 같은 입력에서 현재 closure gate를 막는 비용이 측정될 때만 한다.

### R5. 추상 모델에서 실제 힙까지

Aeneas 연구는 borrow 중심 LLBC와 저수준 heap/address 실행 사이의 관계,
symbolic join 및 loop 검사를 분리해서 정당화한다. 이 증명 구조는
Pergyra의 pure value/footprint 모델과 실제 C/LLVM storage 사이에 남은
간극을 설명하는 데 유용하다.
[2024 저자 long version](https://arxiv.org/pdf/2404.02680).

**적용 후보:** Pergyra의 typed move/copy/drop과 실제 String/aggregate/collection
glue를 연결하는 simulation 의무를 C2/C3/G3 경계에 둔다. 논문이 검증한
메모리 모델을 현재 Pergyra ABI가 이미 만족한다고 가정하지 않는다.

## 3. 논문과 별도로 확인한 실무 구현

Lean의 현재 `InferBorrow`는 borrowed로 시작해 필요에 따라 owned로
올리고 함수 간 정보를 전파한다. tail call 보존 및 reuse를 위한 조건도
구별한다. 따라서 Pergyra도 단일 함수만 보고 mode 추론을 끝내면 안 된다는
비교 자료다. Lean은 RC 기반이므로 inc/dec나 reuse 정책을 이식하는 근거는
아니다. [공식 pass 설명](https://lean-lang.org/doc/api/Lean/Compiler/LCNF/InferBorrow.html).

이것은 논문이 아닌 live implementation 문서이며, 2026-10-08 열람 기준이다.
OCaml/Rust/Lean을 Pergyra의 구현 언어로 채택하자는 뜻도 아니다.

## 4. 기존 계획 안에서의 적용 우선순위와 반례

아래는 **제안**이다. 새 SoT row나 CLOSED 상태를 부여하지 않는다.

| 순서 / 기존 경계 | 제안할 검증 의무 | 최소 반례 또는 성공 대조군 |
|---|---|---|
| 1 / C1 전달 추론 | 함수 요약의 고정점·callee admission·누락/borrowed 구별 | 3단계 호출, 상호 재귀, sink 두 곳에 같은 actual, sink+borrow 동시 전달 |
| 2 / C1 borrow/place | source 이름의 죽음과 storage의 죽음을 구별; readonly loan 동안 owner 유지 | `view` 생성 후 owner의 마지막 직접 사용, 이어지는 view 사용; overlap 중 mutation/reallocation; 반환 view |
| 3 / C2/C3/G3 물리 구현 | typed 사실로 glue 선택; emitter 재추론 금지; abstract/physical 연결 | nested collection copy 뒤 한쪽 mutation/drop, static String drop, 부분 초기화, branch/return/error exit |
| 4 / C1 후속 capture 경계 | capture와 호출의 effect를 구별하되 모든 loan/capture에 소유 root 존재 | 소비 callback 호출 뒤 sibling callback 실행, 살아 있는 callback의 root 선해제, worker escape |
| 5 / 기존 region 경계 | 공동 수명과 escape가 증명된 저장소만 bulk release | region 밖 반환/retention, 반복 packet 생성, 종료 후 남은 접근 경로 |

이 우선순위에서 당장 production에 필요한 것은 1–3이다. 4–5를 이유로
현재 executable rung을 더 큰 언어 연구 프로젝트로 확장하지 않는다.
현재 코어에는 일반 closure/local loan/early exit 문법이 없으므로 그 반례들은
**후속 acceptance 조건**이지 이번에 실행 성공했다고 주장하는 테스트가 아니다.

검사할 핵심 부등식은 단순한 `owner 변수 ∉ live`가 아니다.
일반 loan 확장에서는 적어도 다음 의무가 필요하다.

```text
storage를 해제하려면:
  그 storage의 owner가 더 이상 필요하지 않고
  살아 있는 loan/view/capture도 그 storage에 의존하지 않아야 한다.

별칭을 무효화하며 이전하려면:
  이후 실행 경로에서 그 별칭을 통한 사용이 없음을 입증해야 한다.
```

이는 이번 검토에서 도출한 구현 요구사항이다. 현재 `B` 매개변수 집합만으로
이미 모든 종류의 loan이 해결되었다는 주장이 아니다.

## Local verification

HEAD `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, 기존 dirty tree 보존.
production 코드·proof owner·의미 정책·SoT 상태는 이 검토에서 수정하지 않았다.

검토 중 다른 작업이 proof를 갱신했다. 결과는 버전별로 분리한다.

| 고정 입력 / 명령 | 관찰 결과 |
|---|---|
| 모델 `6979f767a8072e253549e62ec26edd98de03407457238df82d0b4eb3a539e375`, 정식 extraction smoke | Rocq proof 2336행 실패: `Found no subterm matching "length svs"` |
| 모델 `f8ac63cf9a9a47389863761b3c84a49279b7e683f807956950844bc3d0d69d00`, isolated kernel | PASS, axiom 0 / admit 0 / unsafe feature 없음 |
| 위 모델 + `OwnershipCleanupDocExamples.v` | PASS, 2-module kernel, 문서 관련 15개 예제 |
| 최신 정식 `tests/ownership_cleanup_smoke.sh` 재실행 | 2-module kernel PASS 후 OCaml observer 32행 API 불일치로 FAIL |
| `tests/ownership_clean_direction_smoke.sh` | PASS, 폐기 owner 27개 및 중복 모델 부재; 구조 검사만 |

후속 해시의 PASS는 앞선 해시의 실패 원인을 설명하지 않는다. 증명 수정의
원인은 추정하지 않았고 이번 검토에서 수리하지 않았다. 기본 코어의 중첩
`SVal`에 대한 `register-all` 경고는 관찰되었으며 커널 검사를 막지 않았다.

고정 snapshot과 예제:
`.tmp/ownership-cleanup/algorithm-doc-2026-10-08/`.
예제 SHA-256:
`8a73fd8e162c532f4aa614b6102cfd35530aeca2a04365c3d8f4021d310baf27`.
이는 ignored review scratch이며 새로운 semantic owner나 CI 등록 게이트가 아니다.

PowerShell에서 고정 예제를 다시 검사하는 명령:

```powershell
wsl -d Ubuntu-E-WSL --exec env OPAMROOT=/home/c/.local/share/pergyra-rocq/opam PGY_COQ_PROOFS_DIR=/mnt/d/PergyraLang/.tmp/ownership-cleanup/algorithm-doc-2026-10-08 PGY_COQ_EXPECTED_AXIOMS= bash /mnt/d/PergyraLang/scripts/run_rocq_toolchain.sh timeout 300 bash /mnt/d/PergyraLang/tests/coq_kernel_check.sh
```

현재 source에서 정식 extraction/실행 하네스를 검사하는 명령:

```powershell
wsl -d Ubuntu-E-WSL --exec env OPAMROOT=/home/c/.local/share/pergyra-rocq/opam bash /mnt/d/PergyraLang/scripts/run_rocq_toolchain.sh timeout 300 bash /mnt/d/PergyraLang/tests/ownership_cleanup_smoke.sh
```

이전 `d4cca76d...` 모델의 cost receipt는 보존하지만 새 sink 알고리즘의
실행·성능 증거로 사용하지 않는다. 논문 artifact 재현, C/LLVM 자동 cleanup의
sanitizer 검사, installed driver 교체, 원격 CI, commit/push는 이번 범위에서
수행하지 않았다. **모델 검증과 연구 검토 완료는 implementation closure가 아니다.**

## Follow-up: verified composition, not arbitrary effect deletion

사용자가 Rocq 적용과 모나드식 분기를 요청하여, 기존 기계를 import하는
`OwnershipCleanComposition.v`를 추가했다. 목표·전체 소비 체인·범위는
[bounded directive](../agent_work_directives/ownership_clean_composition_2026-10-08.md),
원리와 다음 반례는 [207 §9](../207_compiler_owned_cleanup_algorithm.md#9-순차-합성과-분기-모나드에서-가져올-것)에 있다.

논문에서 참고한 구분은 **순차 효과 합성 vs 비순차 join**이다. 이번 실제
증명은 `TSeq`의 unit/associativity, guarded branch/continuation 분배,
재귀적 Skip 정규화에 한정된다. 새 loan/closure 타입 체계, 일반 mode 수렴 정리,
실제 할당 감소는 구현하지 않았다. 논문의 idempotent free도 채택하지 않았다.

고정 입력:

- HEAD `3658548d24bca3d721e4f1974ac7a10da99f7aa8` + 보존된 dirty main.
- canonical core SHA-256:
  `f8ac63cf9a9a47389863761b3c84a49279b7e683f807956950844bc3d0d69d00` (이번 수정 없음).
- composition SHA-256:
  `e12836730e707b7a6bf4e753cbf7480ccf9d9bf9c320315253b3282679f7c7a0`.
- extraction signature consumer SHA-256:
  `bb3fbd9b4e3b5b2c07570cb08c96296b1e4042e7040c77b700ac59ca01d3d733`.
- observer SHA-256:
  `bf8348f760a91f42ac747d817e0e1935c4157b2e5fbf537a39584458205b936e`.

관찰된 결과:

| 게이트 | 결과와 범위 |
|---|---|
| isolated core + composition | fresh 2-module kernel PASS, axiom 0 |
| `ownership_cleanup_smoke.sh`, 최종 실행 | fresh 3-module kernel PASS, axiom 0; 15 decision + 24 refusal + 40 sink/composition controls PASS; 고정 cost 사례 30개 PASS |
| `formal_semantics_smoke.sh` | 59-file fresh kernel + approval binding consumer PASS. 전체 corpus의 기존 SlotCalculus 추상 가정 2개 유지; 새 admit/unsafe feature 없음 |
| `ownership_clean_direction_smoke.sh`, 이번 재실행 | 60초 budget에서 exit 124. 이전 PASS와 구분하며 이번 전체 residue 검색은 미검증 |
| 문서 링크 / fence / diff whitespace | 확인한 문서의 로컬 링크와 fence 및 수정 tracked diff PASS. Mermaid 그림의 화면 렌더링은 검사하지 않음 |

기존 observer가 소비하지 못했던 명시적 `Modes` 인자와 4-field routine을
연결했다. inout live-in 기대값 `[1;0;1]`은 새 owner의 식
`args ++ (L minus result)`에 맞는 `[1;0]`으로 수정했다. 독립 C1 인계의
scratch 결과도 같은 차이를 기록한다. GUI 호출 전체의 borrow-only 2-copy,
inferred-sink 0-copy, 원본 재사용 1-copy를 구분해 검사한다. 세 함수 사슬의
3회 전파 사례는 일반 recursive/SCC solver의 수렴 증명이 아니다.

`OwnershipCleanupExtraction.v`는 정규화 전후의 **정확한 texec 명제**와
거부 동치, copy site 보존 명제를 타입으로 소비한다. 이름이 존재하거나
OCaml 예제만 통과하는 것으로 증명 소비를 대체하지 않는다.

첫 보완 실행에서는 모든 proof/control 뒤 receipt의 전역 `git status`가
WSL mount에서 300초 budget을 소진했다. 해당 자식이 155초째 I/O wait인 것을
관찰했다. 이제 상태 수집은 receipt의 hash-bound 입력 7개로 한정하고 그 범위를
명시한다. 전체 repo dirty count처럼 표시하지 않는다. semantic 입력·반복 횟수·
시간 예산은 그대로이며, 전후 source hash drift는 계속 거부한다.

최종 로그와 receipt:

- `.tmp/ownership-cleanup/composition-extraction-final-2026-10-08.log`
- `.tmp/ownership-cleanup/composition-formal-2026-10-08.log`
- `.tmp/ownership-cleanup/composition-direction-2026-10-08.log`
- `.tmp/ownership-cleanup/model-cost.json`, schema `pergyra.ownership-clean.elab-cost.v3`
  (최종 측정 UTC 2026-10-08 04:07:53; normalization은 timed elab 구간 밖).

최대 단일 elaboration 중앙값은 wide-live 41.184 ms였다. OCaml 분석기 비용이며
Pergyra runtime latency/메모리 또는 속도 개선 수치가 아니다. 구문 정규화 예는
GUI inline 23→11 nodes와 0→0 copies다. 실제 allocation 감소와 cleanup의
물리 refinement는 여전히 별도 증거가 필요하다.

동시 작업에서 문서 27 §5의 C2 MIR 계약이 작성된 것을 확인했다. 따라서
"C2 문서가 없으니 G3가 막혔다"는 과거 상태를 현재로 재사용하지 않는다.
이번 slice는 C2/C3/G3, installed driver, CI, SoT 상태를 변경하지 않았다.

## Follow-up: checked read-only alias elision

사용자가 다음 단계도 진행하도록 요청하여, canonical core를 바꾸지 않는
지역 whole-value 별칭 제거를 구현·증명했다. 전체 체인과 거부 조건은
[작업 범위](../agent_work_directives/ownership_clean_readonly_elision_2026-10-08.md),
설명과 도표는 [207 §10](../207_compiler_owned_cleanup_algorithm.md#10-읽기-전용-별칭-제거-같은-계산-한-번-적은-추상-할당)에 있다.
여기서 완료한 것은 Rocq/추출 slice이며 native/self-host 구현 인수가 아니다.

### 변경과 증거의 경계

`OwnershipCleanReadOnly.v`는 a/x의 쓰기, 저장·소비, 다른 복사와 호출을
거부하는 지역 검사와 읽기 identity 치환을 소유한다. 별칭은 target에서
사라지고 기존 `elab`가 root의 모든 치환된 사용을 보게 된다. 새 heap,
runtime loan, 포인터, 소유 descriptor, source annotation을 추가하지 않았다.
일반 member-path loan이나 `let a = x.field`의 field-copy 제거는 구현하지 않았다.

- `readonly_copy_elision`: admitted 원본의 유효한 source 실행에 대해 같은
  trace와 제거된 별칭 외의 최종 값 보존. 유효하지 않은 모든 프로그램의
  양방향 동치나 일반 종료성을 주장하지 않는다.
- `readonly_elaboration_frees_everything`: source 실행과 기존 callee admission
  전제 아래 canonical cleanup 정리로 empty heap을 물려받는다.
- `fixed_allocations_sound`: 실제 `texec` allocation frontier 증가분과
  conservative certificate의 일치. 양쪽 팔을 더하지 않으며 loop/call/서로
  다른 팔 비용은 `None`이다. syntax site 수를 runtime 비용으로 바꾸지 않는다.
- `ro_demo_one_fewer_allocation`: 모든 flag에서 원본/변환본의 **target 실행**,
  동일 `[7,7]` trace, 최종 empty heap과 allocation frontier 3/2를 증명한다.
- `ro_mutation_changes_observation`: 원본 변경 뒤 alias를 읽을 때 무검사 치환은
  7 대신 9를 관찰한다는 양쪽 source 실행을 증명하고, 그 변환을 거부한다.

원본 closed admission을 먼저 요구하도록 보강했다. 사용되지 않는 alias를
지우면서 undefined root read까지 지워 버리는 경우가 반례다. production은
기존 scope/type/identity 사실을 선행 검증하되, D1은 허용된 elision 이후
**실제로 남는 copy**에 적용해야 한다. 추상 core에는 D1 타입 정책이 없으며
이 설명을 doc 27의 정책이나 production 진단을 바꾸는 것으로 읽으면 안 된다.

### 고정 입력과 결과

HEAD는 `3658548d24bca3d721e4f1974ac7a10da99f7aa8`; dirty main과 빈 index를
보존했다. 다음 SHA-256은 최종 receipt와 현재 소스의 일치를 다시 확인했다.

| 입력 | SHA-256 |
|---|---|
| canonical core, 변경 없음 | `f8ac63cf9a9a47389863761b3c84a49279b7e683f807956950844bc3d0d69d00` |
| composition, 변경 없음 | `e12836730e707b7a6bf4e753cbf7480ccf9d9bf9c320315253b3282679f7c7a0` |
| read-only supplement | `14b0122898faf83bffb13836087a6c6bbdd0bf97ef255d08e57fe28a5eb5d8bb` |
| exact extraction consumers | `5be188cdd059a32eb734844b9237e47befbd45abb3f17da794686d2dd0ad566b` |
| extracted-code observer | `dc1950819a263c0fb0cff11a01d0a19483a5f65d2a74a8014d6969df412d7823` |

| 최종 실행 | 관찰 결과 |
|---|---|
| `ownership_cleanup_smoke.sh`, 300 s budget | PASS: fresh 4-module kernel, axiom 0; 기존 15 decision + 24 refusal + 40 sink/composition 검사와 새 read-only 검사 32개 |
| 같은 gate의 고정 cost 입력 | 기존 30사례 PASS; flag 0/1의 copy 1→0, 추상 allocation 3→2 witness 2개 |
| `formal_semantics_smoke.sh`, 1800 s budget | PASS: 60-file fresh kernel + approval export/binding consumer. 기존 `SlotCalculus.MaxSlotId`, `verify_token` 가정 2개만 허용; admit/unsafe kernel feature 없음 |
| direction + direction selftest, 60 s budget | PASS: Windows native Git Bash에서 같은 전체 source set 검사. 폐기 owner 27개/중복 모델 부재; 잔여 참조 및 tool/I/O 오류 거부 |
| 문서와 whitespace 검사 | 관련 문서의 로컬 link target 131개, paired fence, 수정 tracked diff와 새 파일 whitespace PASS. Mermaid 화면 렌더링은 미검사 |

read-only 검사에는 한 팔/loop 안의 변경, storage/소비/inout, live-out과
borrowed destination, 원본 admission 누락, root의 마지막 직접 사용 뒤의
alias 읽기, loop certificate 치환, copied field의 allocation 유지와
unknown cost 구분이 포함된다. `count_copies`는 `TCopy`만 세므로 field 예제는
1→0이며, `TField`의 새 값 allocation은 별도로 남는다. OCaml은 추출 알고리즘의
observer일 뿐 source/target 인터프리터나 소유권 기계를 새로 구현하지 않았다.

full corpus에서는 기존 absolute-name masking, nested `SVal`의 `register-all`,
approval load-path remapping 경고가 관찰됐다. 경고를 숨기지 않았으며 fresh
compile과 kernel recheck는 exit 0이다. 이 slice의 새 증명 가정은 0개다.

receipt는 schema `pergyra.ownership-clean.elab-cost.v4`, UTC
`2026-10-08T04:43:25.363865+00:00`이며 8개 source input hash를 묶는다.
기존 30사례는 변경하지 않았고 timed 구간은 여전히 canonical `elab`뿐이다.
최대 단일 elaboration 중앙값 39.302 ms는 **OCaml 분석기 비용**이다.
readonly rewrite 속도, 생성 프로그램의 실제 바이트·RSS나 speedup 측정이 아니다.

보존한 로그와 receipt:

- `.tmp/ownership-cleanup/readonly-extraction-final-2026-10-08.log`
- `.tmp/ownership-cleanup/readonly-formal-2026-10-08.log`
- `.tmp/ownership-cleanup/readonly-direction-2026-10-08.log`
- `.tmp/ownership-cleanup/readonly-cost-2026-10-08.json` (날짜 고정 사본)
- `.tmp/ownership-cleanup/model-cost.json` (최신 출력 경로, 후속 실행으로 갱신 가능)

앞 절의 WSL direction 시간초과는 이 native-host 실행에서 재현되지 않았다.
소스 범위·60초 allowance·의미 검사 내용은 줄이거나 늘리지 않았고 검사 코드도
바꾸지 않았다. direction의 PASS는 여전히 구조·거부 동작 증거이지 cleanup
의미/물리 실행 증거가 아니다.

재실행 명령은 위 extraction 명령과 다음 두 가지다.

```powershell
wsl -d Ubuntu-E-WSL --exec env OPAMROOT=/home/c/.local/share/pergyra-rocq/opam bash /mnt/d/PergyraLang/scripts/run_rocq_toolchain.sh timeout 1800 bash /mnt/d/PergyraLang/tests/formal_semantics_smoke.sh
& 'C:/Program Files/Git/bin/bash.exe' -lc 'cd /d/PergyraLang && timeout 60 bash tests/ownership_clean_direction_selftest.sh'
```

### 실제 컴파일러의 남은 경계

이 실행에서 source로 확인한 `driver_app -> mir_lower`는 SSA, use/cleanup
edges와 recompute를 연결하지만 C2의 ownership-clean producer/pass를 아직
연결하지 않는다. C2 **문서는 이미 존재**한다. C3/C5의 기존 작업 분담은
그대로 두었으며 그 범위의 인수 여부만 사용자에게 비차단 질문으로 확인했다.
새로운 production 소유권, backend별 재추론, allocator glue를 추측해 넣지 않았다.

다음 반례는 일반 member-projection alias의 owner lifetime과 invalidation이다.
그 경계는 기존 C1/C2 owner 아래에서 먼저 정의해야 한다. 현재 전체 liveness를
두 번 실행하는 모델 wrapper를 production에 그대로 이식하라는 요구도 아니다.
installed driver, physical C/LLVM sanitizer, self-host parity, commit/push,
exact-SHA CI와 GUI 전달은 이 후속 proof slice에서 실행하지 않았다.
**bounded proof 완료는 SoT나 자동 cleanup 전체 완료가 아니다.**
