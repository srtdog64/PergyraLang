# Pergyra Compiler Architecture Adoption Map

Updated: 2026-08-26

이 문서는 외부 컴파일러의 물리적 구조를 조립하는 설계안이 아니다. Pergyra가 이미
선택한 one SoT, owner spine, fail-closed, target-neutral fact와 self-host substitution
규율을 더 강하게 만드는 불변식만 추린다.

## 작성 당시 Pergyra 기준선 — 역사적 snapshot

다음은 원 작성 기록의 2026-08-26 snapshot이며 현재 상태가 아니다.
2026-09-05 통합 검토는 과거 수치의 원시 실행 증거를 다시 검증하지 않았다.

- top-level SoT registry: `CLOSED=49`, `BRIDGE=36`, `ACTIVE=1`
- integrated forecast: 83%; strict language beta: 83%
- SoT migration index: 78.2%; hard self-host replacement: 75%
- bootstrap/fixed-point와 CI/release evidence: 4/4
- native dogfood 증거는 `SURFACE`, `REACHABLE`, `SUBSTITUTING`을 구분하며,
  old C-owned path를 실제 대체한 `SUBSTITUTING`만 hard replacement로 센다.
- language word registry는 146개 단어를 소유하며 양쪽 parser selector가 모두 없는
  단어는 0개다. 과거 `channel`은 예약어인데 구현 selector가 없어 삭제됐다.

최신 상태는 이 비교 문서가 아니라 [진행 상황](../00_progress.md),
[SoT registry](../semantics/sot_owner_spine_registry.md),
[native dogfood 계약](../self_hosted/17_pergyra_native_dogfood_contract.md)과
executable gate에서 확인한다. 위 과거 수치를 현재 완료율로 인용하지 않는다.

## 결론

Pergyra에 필요한 다음 architecture는 새 compiler framework가 아니다. 이미 있는
owner spine에 아래 세 계약을 더 일관되게 적용하는 것이다.

1. **Identity contract:** 모든 downstream 계산은 stable owner identity로 fact를
   요청하고 wrong-kind/stale identity를 거부한다.
2. **Lifetime contract:** 각 lowering은 무엇을 보존하고 무엇을 잃는지, 그 결과의
   마지막 합법 consumer가 누구인지 선언한다.
3. **Invalidation contract:** owner fact가 바뀌거나 artifact가 삭제됐을 때 어떤
   derived view가 더 이상 유효하지 않은지 명시한다.

이 세 가지가 닫히기 전 범용 query engine, persistent cache, dialect framework,
새 IR 또는 worker expansion을 만들면 현재 구조를 단순화하지 않고 두 번째 권위를
추가할 가능성이 높다.

## 채택 판단표

아래 표는 원칙과 후보 seam의 매핑이다. 각 행의 현재 구현이나 cycle gate가
완료됐다는 뜻이 아니며, 실제 도입에는 구체적인 owner와 실행 gate가 필요하다.

| 후보 불변식 | 참고 구현 | 현재 Pergyra owner/seam | 판정 | 필요한 falsifier | 복제하지 않을 것 |
|---|---|---|---|---|---|
| source form과 semantic provenance 분리 보존 | Clang | parser/semantic fact -> diagnostic projection | 채택 | source renumber/macro-like transform 뒤에도 stable diagnostic identity, codegen은 recovery carrier 거부 | Clang AST class hierarchy |
| transformation의 preserved/invalidated fact 선언 | LLVM | MIR/DIR/AIR owner publication과 consumer | 채택 | owner fact mutation/deletion 뒤 stale derived view가 반드시 거부됨 | LLVM pass manager 전체 |
| typed key와 named provider | rustc | SoT spine identity/owner/accessor | 이미 방향 일치, 더 엄격히 적용 | consumer root rescan과 raw index lookup negative gate | giant `TyCtxt` |
| dependency graph와 cycle path | rustc, Swift | compiler-world request/action/intent orchestration | 채택 | A -> B -> A가 명시적 cycle diagnostic으로 실패하고 partial artifact 없음 | compiler-wide evaluator/cache |
| IR별 information-loss ledger | GHC, Swift | HIR/DIR/RIR/MIR/AIR/ABI contracts | 채택 | 하위 consumer가 이미 소실된 상위 fact를 문자열/AST scan으로 복구하지 못함 | consumer 없는 새 IR |
| semantic invariant를 IR verifier로 확인 | Swift SIL, MLIR | MIR/DIR fact verifier와 AIR evidence verifier | 채택 | wrong-kind, missing provenance, invalid lifetime를 backend 전에 거부 | SIL 또는 dialect framework |
| interface-shaped capability contract | MLIR | SoT registry의 producer/consumer/forbidden fallback | 채택 | 새 fact kind가 verifier special-case 없이 declared interface를 만족하거나 명시적으로 거부 | 모든 fact의 generic operation화 |
| artifact granularity를 owner lifetime에 맞춤 | Zig | program-global immutable view와 routine-local rows | 채택 | routine 수 증가 시 global serialization/copy가 routine마다 재생성되지 않음 | central InternPool authority |
| typed analysis/codegen job | Zig | self-host action/result and materialization boundary | 채택 | readiness fact 없는 job은 queue/publish되지 않고 partial output 없음 | worker 수 자체를 progress로 계산 |
| persistent incremental cache | rustc, Swift, Zig | 아직 named owner 없음 | 조건부 | 반복 owned operation, stable key, deterministic result, crash recovery와 invalidation 비용을 먼저 측정 | cache-first architecture |
| nested parallel pass scheduling | LLVM, MLIR, Zig | 독립 routine artifact가 증명된 범위 | 조건부 | shared mutable owner read가 없고 one integration gate가 있음 | active rung 병렬 구현 track |
| multi-dialect/multi-IR framework | MLIR | 현재 필요 없음 | 거부 | 해당 없음 | framework 자체 |

## 우선 도입할 계약

### Stable identity와 wrong-kind 거부

rustc와 Zig의 compact ID는 전체 객체를 운반하지 않고 fact를 찾는 데 유용하다.
Pergyra는 이미 logical handle과 registry identity 방향을 갖고 있으므로 새 global
interner보다 다음을 통일하는 편이 낫다.

- identity에 owner family와 kind를 포함한다.
- mutation/publish epoch가 바뀌면 이전 derived handle을 거부한다.
- 숫자 우연 일치나 array position을 semantic identity로 승격하지 않는다.
- owner accessor만 identity를 resolve하며 consumer는 program root를 재탐색하지 않는다.

삭제 테스트는 여기서 강력한 정당화 도구다. owner row나 syntax를 삭제했는데 모든
테스트가 그대로 통과한다면 실제 production consumer가 없거나 gate가 주장을 검증하지
못했을 가능성이 크다. 반대로 삭제가 named negative gate를 정확히 깨뜨리면 해당
구조가 실재하는 계약임을 보여 준다. 다만 deletion test 하나만으로 좋은 문법이나
좋은 architecture가 증명되는 것은 아니다. user-facing 의미, owner, 마지막 consumer,
실패 형태까지 함께 있어야 한다.

### Preservation과 invalidation manifest

LLVM의 `PreservedAnalyses`를 작은 Pergyra contract로 번역한다.

```text
transform input owner identity
  -> consumed fact kinds
  -> emitted fact kinds
  -> preserved upstream identities
  -> invalidated derived views
  -> deleted identities
```

처음부터 runtime cache를 만들 필요는 없다. focused verifier가 transform 전후의
manifest를 검사하고, 삭제된 identity 또는 바뀐 digest를 downstream이 읽는 fixture를
거부하면 된다. 실제 repeated computation이 측정될 때 이 계약이 cache correctness의
기초가 된다.

### Request dependency와 cycle diagnostic

Swift와 rustc의 교훈은 lazy evaluation보다 cycle을 숨기지 않는 것이다. Pergyra
compiler-world orchestration에서 action/intent가 다른 typed request를 부를 때 다음을
고정할 수 있다.

- request identity = request kind + owner identity + immutable options digest
- result identity = terminal success/failure variant + owned artifact receipt
- active request stack으로 exact cycle path를 진단
- cycle 또는 missing fact에서 partial artifact를 publish하지 않음
- retry나 native fallback 없음

이것은 language-level `intent` 의미를 query engine으로 바꾸자는 제안이 아니다.
compiler purpose intent 내부의 계산 경계를 inspectable하게 만드는 규율이다.

### Information-loss ledger와 verifier

GHC의 Core/STG/Cmm, Swift의 AST/SIL/LLVM, MLIR verifier가 공통으로 보여 주는 것은
하강이 단순 serialization이 아니라는 점이다. 각 Pergyra stage에는 최소한 아래 표가
있어야 한다.

| 항목 | 질문 |
|---|---|
| input owner | 어떤 upstream authority만 읽는가? |
| preserved facts | 다음 consumer까지 반드시 남아야 하는 것은 무엇인가? |
| admitted loss | 이 단계에서 의도적으로 버리는 것은 무엇인가? |
| loss evidence | 손실이 허용됨을 어떤 receipt/diagnostic이 증명하는가? |
| last consumer | 이 representation을 마지막으로 읽을 수 있는 곳은 어디인가? |
| forbidden recovery | 잃은 사실을 어느 AST scan/string/backend guess로 복구하면 안 되는가? |

Pergyra의 기존 abstraction-loss contract와 AIR epsilon quarantine이 이 역할의 기반이다.
새 IR 대신 누락된 stage row와 negative recovery gate를 채우는 것이 맞다.

## 조건부로 남겨 둘 것

다음은 가치가 없어서가 아니라 현재 문제보다 큰 mechanism이기 때문에 보류한다.

- **Persistent incremental compilation:** fixed compiler-scale input에서 반복 owner
  operation과 invalidation 범위가 계측된 뒤 검토한다.
- **General query/request engine:** 세 개 이상의 독립 consumer가 같은 dependency,
  cycle, caching protocol을 실제로 반복할 때 검토한다.
- **Automatic parallel scheduling:** immutable input, 독립 output, deterministic merge,
  bounded artifact count와 integration gate가 모두 있을 때만 연다.
- **Intern pool expansion:** pointer/ID equality가 semantic equality와 정확히 일치하고
  reclamation/stale-ID policy가 있을 때만 쓴다.
- **New IR:** 기존 IR 어느 것도 필요한 fact lifetime과 last consumer를 표현하지
  못한다는 falsifying case가 먼저 있어야 한다.

## 명시적으로 거부할 것

- backend가 AST/source/string에서 semantic fact를 재구성하는 경로
- `new ? old` dual read와 missing-fact native fallback
- one program-global serialization과 routine-local row를 문자열로 매번 합치는 구조
- cache hit rate나 job 수를 self-host substitution progress로 세는 방식
- 비교 대상의 명칭을 Pergyra 구성체와 동일시하는 방식
- `world`, `zone`, `subject`, `action`, `intent`를 compiler framework class hierarchy로
  바꾸는 방식

## active rung에 적용하는 순서

이 문서는 독립 implementation queue를 열지 않는다. production rung이 특정 seam에
도달했을 때만 다음 순서로 사용한다.

1. production entrypoint와 삭제할 direct bypass를 지목한다.
2. 기존 Pergyra fact owner와 마지막 orchestration consumer를 확인한다.
3. 위 표에서 필요한 불변식 하나만 고른다.
4. consumer migration, missing-fact failure와 old-path deletion을 먼저 설계한다.
5. stale/wrong-kind/recovery/cycle 중 해당하는 falsifier 하나를 둔다.
6. focused gate가 green일 때만 해당 architecture borrowing을 실제 채택으로 기록한다.

이렇게 하면 외부 컴파일러 비교가 “또 하나의 영감 문서”로 끝나지 않고, Pergyra의
실제 owner seam을 닫는 판단 기준이 된다.

## Pergyra 내부 근거

- `docs/semantics/sot_owner_spine_registry.md`
- `docs/180_compiler_logical_spine_handles_gates.md`
- `docs/semantics/09_abstraction_loss_contracts.md`
- `docs/104_air_compiler_architecture.md`
- `docs/self_hosted/17_pergyra_native_dogfood_contract.md`
- `docs/199_language_word_and_dogfood_grammar.md`
- `docs/200_object_to_action_boundary_patterns.md`
