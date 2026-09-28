# 206. 최소 단위 결정: party, authority, 이름, 소유 이전, FFI, 시계, 산술

Updated: 2026-09-28 (Asia/Seoul)

Status: **MODEL CHECKED / IMPLEMENTATION BY SECTION**. 각 절의 최소 단위는
`docs/semantics/proofs/`의 Rocq 모델로 증명했고, CI의 Rocq 9와 Coq 8.x가 둘 다
검사한다. 구현 상태는 절마다 적는다. 이 문서는 결정과 그 근거를 한곳에 두는
authoring contract다. 의미 사실의 최종 권위는 여전히 parser/semantic/MIR owner와
실행 게이트다.

## 0. 방법

harness PP-061~065와 red-team R11/R13은 따로 보면 다섯 개의 패치다. 이 문서는
그것들을 **최소 단위**로 쪼갠 다음 합성으로 다시 세운다.

- **최소 단위**는 다른 단위들의 어떤 함수로도 계산할 수 없는 사실이다. 증명은
  `AuthorityIrreducibility.v`와 같은 방식을 쓴다. 다른 사실은 모두 같고 그
  단위만 다른 두 구성을 만들어, 판정이 갈린다는 것을 보인다.
- **합성**은 단위들로 정의되는 구성이다. 안전 속성(dangling 없음, fail-open 없음,
  이중 해제 없음)은 단위의 성질에서 따라 나온다는 것을 같이 증명한다.
- 단위가 증명되면 새 기능은 단위의 합성으로만 늘린다. 합성마다 owner를 새로
  만들지 않고, 단위마다 owner 하나를 두고 여러 합성이 그것을 소비한다.

단위와 docs/42 네 축의 대응은 아래 표와 같다.

| 단위 | 축(docs/42) | 증명 | 공유 owner를 쓰는 합성 |
|---|---|---|---|
| subject identity | Domain | `PartySlotBinding.identity_irreducible` | party slot, zone authority |
| role witness | Type/Contract | `PartySlotBinding.witness_irreducible`, `AuthorityRequiresWitness.witness_does_not_decide` | party slot, zone authority, action `requires` |
| lexical scope tree | Resource(수명) | `BindingIdentityScope.ancestors_linear` | 이름 규칙, party borrow 범위 |
| binding identity | Type/Contract | `BindingIdentityScope.name_is_not_identity` | 이름 규칙, MIR phi 키 |
| move / clone | Resource | `CollectionOwnershipTransfer.move_and_clone_differ` | Array, FFI 경계 복사 |
| 경계 복사 + release 의무 | Resource | `ForeignStringOwnership.obligation_not_in_signature` | `extern` String 반환 |
| 단조 시계 / 벽시계 | Execution | `ClockDomains.monotonic_not_from_wall`, `wall_not_from_monotonic` | `Now()`, `UnixTimeMs()` |
| 정확한 합 + 표현 가능성 | Type/Contract | `RecoverableArithmetic.wrap_hides_overflow` | `+`, `TryAdd`, `CheckedAdd` |

## 1. PP-064: party slot = borrow(subject identity) × role witness

증명: `docs/semantics/proofs/PartySlotBinding.v`.

- `identity_irreducible`, `witness_irreducible`: 어느 subject인가와 어느 role
  구현인가는 서로의 함수가 아니다.
- `role_only_has_no_self`: role 이름만 bind한 slot(`bind team.tank = Role;`)은
  어떤 store에서도 `self`가 없다. 더 작은 단위가 아니라 불완전한 구성이다.
  native가 `self.hp`를 읽다 세그폴트한 이유다.
- `borrow_keeps_identity`, `owned_copy_splits_identity`: slot이 subject를
  빌리면 slot을 통한 쓰기가 subject 자신에게 보인다. 사본을 소유하면 보이지
  않아, identity가 둘로 갈라진다. 그래서 party는 subject를 소유하지 않는다.
- `scoped_borrow_live`, `inner_subject_dangles`, `slot_call_defined`: subject의
  범위가 party의 범위를 감싸면, party가 살아 있는 동안 subject도 살아 있다.
  반대로 감싸지 않으면 dangling 시점이 있다.

결정:

- 표면: `bind team.tank = warrior as WarriorTank;`. `as`는 bind 문 안에서만
  "이 subject를 이 role witness로"를 뜻한다. 다른 곳의 `as`는 숫자 변환 그대로다.
- `bind team.tank = WarriorTank;`(role만)는 두 front end가 거부한다. role 본문이
  지금 `self`를 안 읽어도 거부한다. 나중에 필드 하나를 읽는 순간 메모리 안전이
  깨지기 때문이다.
- party는 subject를 소유하지 않는다. slot은 subject를 가리키고, subject의 수명은
  lexical owner나 zone이 진다(docs/11: party는 협력 단위, 수명 경계는 zone/world).
- 베타 수명 규칙:
  - subject 바인딩의 범위가 party 바인딩의 범위를 감싸야 bind할 수 있다.
  - 그 함수에서 bind한 party는 return, 필드 저장, spawn/async 전달로 내보낼 수
    없다.
  - zone이 가진 subject(`space.agent`)의 bind는 처음에는 fail-closed로 막는다.
- `dyn role slot`만 다시 bind할 수 있고, 새 bind는 이전 쌍을 통째로 바꾼다.
- role 본문의 `self`는 role의 `for` 대상 타입을 가진다. native가 이것을
  `TYPE_UNKNOWN`으로 두어 없는 필드 읽기를 조용히 통과시키던 구멍도 닫힌다.

## 2. PP-065: zone authority = identity × witness

증명: `docs/semantics/proofs/AuthorityRequiresWitness.v`.

- `identity_does_not_decide`, `witness_does_not_decide`: slot의 subject라는
  사실과 그 타입이 ability를 가진다는 사실은 서로를 결정하지 않는다.
- `identity_only_fails_open`: identity만 확인하는 admission은 전체 규칙이
  거부하는 actor를 받아들인다. default 경로의 결함이 정확히 이 모양이다.
- `static_check_suffices`: 선언마다 slot의 subject 타입을 `requires`에 대해 한 번
  확인하면, slot을 차지할 수 있는 모든 actor에서 witness 조건이 성립한다. 그래서
  검사는 호출마다가 아니라 선언 admission에 둔다.

결정: default 경로는 action `requires`와 같은 witness owner
(`SemanticAstActionRequirementImplemented`)로 zone authority의 `requires`를
확인하고, 없으면 `zone_authority_ability_unsatisfied`로 authority 위치에서 거부한다.

## 3. PP-061/062: 이름 하나에 binding identity 하나

증명: `docs/semantics/proofs/BindingIdentityScope.v`.

- `ancestors_linear`: 한 지점을 감싸는 범위들은 사슬을 이룬다.
- `unique_resolution`: "이름이 같고 identity가 다른 두 바인딩의 범위는 어느
  쪽도 다른 쪽을 감싸지 않는다(같은 범위 포함)"는 규칙 아래에서, 모든 지점에서
  이름은 identity 하나로만 풀린다.
- `sibling_reuse_resolves`: 형제 범위의 같은 이름은 만나지 않는다. 두 반복문의
  `for i`는 그대로 허용된다.
- `shadow_is_ambiguous`, `redeclare_is_ambiguous`: PP-061(안쪽 가림)과
  PP-062(같은 범위 재선언)는 같은 규칙 하나가 둘 다 거부한다.
- `name_is_not_identity`: 같은 지점에 같은 이름이 보여도 바인딩은 하나일 수도
  둘일 수도 있다. 이름을 키로 쓰는 phi join은 둘을 구분할 수 없다.

결정: 두 front end가 같은 규칙으로 거부한다(C#식). 그리고 source에서 막는 것과
별개로, self-host MIR의 phi와 지역 변수 표는 이름이 아니라 binding identity를
키로 쓴다. 잘못된 입력이 들어와도 내부 phi 오류로 끝나면 안 된다.

## 4. Array: move와 clone만 기저다

증명: `docs/semantics/proofs/CollectionOwnershipTransfer.v`.

- `move_keeps_unique`, `clone_keeps_unique`, `alias_breaks_unique`: move와 clone은
  "저장소마다 소유 바인딩 하나"를 지키고, 암묵 shallow copy(alias)는 깬다.
- `unique_drops_once`, `alias_drops_twice`: 소유자가 하나면 해제도 한 번이고,
  alias는 같은 저장소를 두 번 해제한다.
- `move_and_clone_differ`: 둘은 서로를 대신할 수 없다.

결정: `let b = a`는 move, 복사는 `Clone(a)`로 쓴다(docs/22 §2). 암묵 deep copy는
clone과 같은 값을 계산하지만 비용을 숨기므로 표면으로 두지 않는다. 구현은 이
영역을 맡은 컬렉션 오너십 레인(`semantic.hashmap_collection_ownership`)이 한다.
그 레인의 현재 WIP는 암묵 deep copy를 넣고 있으니, 이 결정에 맞춰 바꿔야 한다.

## 5. PP-063: extern String = 경계 복사 ∘ (borrow | own + release)

증명: `docs/semantics/proofs/ForeignStringOwnership.v`.

- `obligation_not_in_signature`: 같은 시그니처, 같은 바이트여도 해제 의무가
  다를 수 있다. 의무는 타입에 들어 있지 않은 정보다.
- `borrowed_as_owned_double_frees`, `owned_as_borrowed_leaks`,
  `no_lane_fits_both`: 어느 쪽으로 추측해도 한쪽이 깨진다.
- `lanes_release_exactly`, `copy_survives_foreign_change`: lane을 밝히면 해제는
  정확히 한 번이고, 경계에서 복사한 Pergyra 문자열은 이후 foreign 쪽이 쓰거나
  해제해도 변하지 않는다.

결정: 이미 있는 단어(docs/157 P7)를 쓴다.

- `-> ref String`: callee가 가진 문자열을 경계에서 복사한다.
- `-> own String release FreeName`: 복사한 뒤 `FreeName`으로 한 번 해제한다.
- NULL일 수 있으면 `Option<String>`이고, NULL은 None이다.
- 표시 없는 `-> String`은 거부한다. `docs/semantics/04_ownership_abi.md`가
  런타임 ABI에서 이미 구분하는 `runtime-borrowed string`과 `result-owned
  string`을 FFI 경계로 넓힌 것이다.

## 6. R11: `Now() -> Long`(단조), `UnixTimeMs() -> Long`(벽시계)

증명: `docs/semantics/proofs/ClockDomains.v`.

- `monotonic_not_from_wall`, `wall_not_from_monotonic`: 두 시계는 서로의 함수가
  아니다.
- `int32_breaks_monotonic`: 32비트로 줄이면 2^31 ms(약 24.8일)에서 뒤로 간다.
- `long_keeps_monotonic`: 64비트는 볼 수 있는 모든 값에서 그대로라 단조성이
  유지된다.

결정: `Now()`는 에폭을 정하지 않은 단조 밀리초 `Long`이다(Windows
`GetTickCount64`, POSIX `CLOCK_MONOTONIC`). 벽시계는 `UnixTimeMs() -> Long`으로
나눈다. `let t: Int = Now()` 같은 호출은 `Long`으로 옮겨야 한다.

## 7. R13: `+`는 그대로, `TryAdd`는 새 단위, `CheckedAdd`는 합성

증명: `docs/semantics/proofs/RecoverableArithmetic.v`.

- `wrap_hides_overflow`: 표현 가능한 합과 넘친 합이 같은 값으로 감긴다. 감긴
  결과만 보고는 넘침을 알 수 없으니, 넘침 판정은 따로 있어야 하는 단위다.
- `try_add_sound`, `try_add_complete`, `try_add_none_iff`: `TryAdd`는 표현
  가능할 때 정확히 그 합을, 아니면 오류를 돌려준다.
- `checked_is_try_then_panic`: `CheckedAdd`는 `TryAdd` 뒤에 panic을 붙인 합성이다.
- `wrap_agrees_in_range`: 표현 가능한 범위에서는 감김과 `TryAdd`가 같은 값을
  내므로, Try 계열을 더해도 기존 프로그램의 의미는 바뀌지 않는다.

결정: `Int`의 `+ - *`는 감김 그대로 둔다(`pergyra.int.wrapping.i32.v1`).
`TryAdd`/`TrySub`/`TryMul`과 `Long` 판은 `Result<_, ArithmeticError>`를 돌려준다.
빌드 설정에 따라 의미가 달라지는 산술(debug에서만 panic)은 넣지 않는다.

## 8. 단위 기준 구현 순서

합성보다 단위의 owner를 먼저 세운다. 그래야 뒤의 합성이 같은 owner를 쓴다.

1. **witness owner**: PP-065. action `requires`와 zone authority `requires`가
   같은 판정을 쓴다. PP-064의 bind 검사도 이것을 쓴다.
2. **scope tree + binding identity**: PP-061/062의 이름 규칙과 self-host phi의
   binding identity 키. PP-064의 borrow 범위 검사가 이 scope 사실을 쓴다.
3. **PP-064**: 1과 2의 합성. `subject as Role` bind, role만 쓰는 bind 거부,
   `self` 타입, 수명 규칙, C/LLVM의 slot 데이터 포인터.
4. **transfer lanes**: PP-063의 `ref`/`own` String. Array move/clone은 오너십
   레인이 같은 단위로 구현한다.
5. **값 API**: R11 시계, R13 Try 계열.

## 9. 정직한 범위

모든 증명은 모델 수준이다. 반례 쌍으로 환원 불가능성을 보이고, 작은 모델에서
합성의 안전 속성을 보인다. 언어 전체의 건전성 정리가 아니며, 구현이 모델을
따른다는 것은 각 절의 실행 게이트가 따로 증명해야 한다.
