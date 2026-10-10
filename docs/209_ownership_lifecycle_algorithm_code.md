# 소유권 생애 알고리즘 코드

상태: **설명용 투영(explanatory projection)이며, 새 의미 owner가 아니다.**
각 코드 블록의 의미는 옆에 적은 Rocq 모델과 [27](semantics/27_ownership_clean.md)·
[28](semantics/28_memory_boundary_composition.md)·[29](semantics/29_action_scoped_references.md)번
문서가 소유한다. 이 문서와 모델이 다르면 모델이 맞다. production 구현은 OPEN이다.
§5 일괄 해체와 §8의 원자 batch는 함수형 모델의 원자성이다. 실제 `free`를
되돌릴 수 있다는 뜻이 아니다. 구현 착수 전에는
[경계 문서](semantics/ownership_lifecycle_implementation_boundaries.md)를 함께 읽는다.

일반 값의 move/copy·정리 결정 절차는
[207번 문서](207_compiler_owned_cleanup_algorithm.md)가 설명한다. 이 문서는 그
위에 트리 해체, 일괄 해체, 그래프 store, 행동 범위 참조와 saga, 선택적
순환 회수를 하나의 코드 흐름으로 잇는다.

## 0. 읽는 법

- 코드는 의사코드다. 실제 Pergyra 문법이나 MIR 출력이 아니다.
- 실패는 예외가 아니라 값이다. `Refused(why, 원래상태)`나 `Missing(why)`가
  돌아온다. **원시 연산의 거절과 모델 batch 거절은 상태를 바꾸지 않는다.**
  여러 연산으로 된 단계는 다르다. 앞부분이 성공한 뒤 실패하면 그 효과는 남고,
  결과가 성공한 본문 접두부 길이를 기록한다(§7).
- 함수 이름 옆 주석의 이름이 대응하는 Rocq 정의다.
- 어떤 값에도 선언된 라이프타임은 없다. 노드를 끝내는 것은 소유자 사건뿐이고,
  참조는 이름이다.

```text
생성 ──▶ 이동/복사(컴파일러가 결정) ──▶ 사용(행동 동안 붙잡기) ──▶ 해체(소유자 사건)
                                              │
                         낡은 이름으로 접근 ──▶ 명시적 거절 (UB 아님)
```

## 1. 상태

```text
Slot      = { gen: Nat, node: Option<Node> }      # gen은 되돌아가지 않는다
Link      = (slot, gen)                           # 이름일 뿐 소유하지 않는다
Owner     = Root(r) | Parent(p)                   # 노드마다 정확히 하나
Node      = { owner: Owner, fields: Field -> Option<Link> }
Index     = target -> [(src, field)]              # 역참조 색인 (Qt의 연결 목록)
Kids      = owner -> [child]                      # 소유자별 자식 색인
Authority = { forest, acting_context,
              cleanup_rights: root -> Option<(epoch, holder)>,
              leases: [Lease{id, holder, target, Borrow|Pin}] }
```

`OwnershipTeardown.v`의 `St`와 `OwnershipTeardownAuthority.v`의
`AuthorityState`다. 그래프 store(§6)는 같은 원리를 store 단위로 다시 쓴 별도
모델이다(`OwnershipGraphLinks.v`의 `GS`).

## 2. 일반 값: 컴파일러가 해제 위치를 계산한다

자세한 규칙은 207번 §4–6에 있다. 여기서는 뼈대만 옮긴다
(`OwnershipCleanCore.v`의 `elab`).

```text
def elab(stmt, live_after, borrowed) -> (target, live_before):
    match stmt:
      Seq(s1, s2):
        t2, L2 = elab(s2, live_after, borrowed)      # 뒤에서 앞으로
        t1, L1 = elab(s1, L2, borrowed)
        return Seq(t1, t2), L1
      If(c, a, b):
        ta, La = elab(a, live_after, borrowed)
        tb, Lb = elab(b, live_after, borrowed)
        Lin = {c} ∪ La ∪ Lb
        return If(c, Seq(settle(Lin, La, borrowed), ta),
                     Seq(settle(Lin, Lb, borrowed), tb)), Lin
      While(c, body, h):                             # h: loop solver가 제시
        tbody, Lbody = elab(body, h, borrowed)
        require Lbody ⊆ h and live_after ⊆ h and c ∈ h   # 아니면 거부
        return Seq(While(c, Seq(settle(h, Lbody, borrowed), tbody)),
                   settle(h, live_after, borrowed)), h
      Use(y) consuming:
        if y ∈ live_after or y ∈ borrowed: emit Copy(y)  # 이후에도 필요
        else:                              emit Move(y)  # 마지막 사용

def settle(A, L, borrowed):
    for v in A - L:
        if v ∈ borrowed: emit EndBorrow(v)
        else:            emit Drop(v)                    # 그 binding의 footprint만
```

실행 시에는 선택된 경로의 `Drop`만 돈다. 전체 힙을 추적하지 않는다.
이 뼈대는 mode·호출·반환·inout·view 전제를 생략했다. 그것들이 없어도 추측해서
move/copy해도 된다는 알고리즘이 아니다. 실제 lowering은 27/207의 계약을 소비하며
필수 사실이 없으면 거절한다. 사용자가 수명·수동 deep-drop으로 빈 추론을 메우지 않는다.

## 3. 트리 해체 (Qt식 부모-자식 소유)

```text
# OwnershipTeardownAuthority.v : retire
def retire(a, target, U) -> Accepted(a') | Refused(why, a):
    if not target_current(a.forest, target):          # 핸들/epoch가 현재인가
        return Refused(StaleIdentity, a)
    if not target_authorized(a, target):              # 루트의 정리 권한을 이 맥락이 쥐었나
        return Refused(MissingCleanupRight, a)
    if not valid_unit(a.forest, target, U):           # U가 정확히 그 서브트리인가
        return Refused(InvalidUnit, a)
    if any(lease.target.slot ∈ U for lease in a.leases):   # 대여·핀이 걸린 멤버
        return Refused(ActiveLease, a)
    if target is RetireNode:
        return Accepted(a with forest = teardown(a.forest, U))
    else:                                             # RetireRoot(r, epoch)
        return Accepted(a with forest = root_drop(a.forest, r, U),
                                cleanup_rights[r] = None)

# OwnershipTeardownAuthority.v : valid_unit / unit_member_expected
def valid_unit(s, target, U):
    if has_duplicate(U) or any(x >= s.bound for x in U): return false
    for x in 0 ..< s.bound:
        expected = (x == target.node) if target is RetireNode else
                   (owner(x) == Root(target.root))
        expected = expected or (owner(x) == Parent(p) and p ∈ U)
        if (x ∈ U) != expected: return false
    return true
```

모델의 `valid_unit`은 검증용 전체 비교다. production의 목표는 단위를 자식 색인에서
만드는 것이다. 자식 색인이 정확하면 그것이 정확히 서브트리라는 정리가
`unit_from_kids`·`root_unit_from_kids`다.

```text
# OwnershipTeardown.v : teardown = phase2(phase1(...))
def teardown(s, U):
    # phase 1: 밖에서 U로 들어오는 저장된 링크를 지운다.
    #          역색인으로 찾는 목표 순회다. 물리적 국소성/비용은 별도 정제한다.
    for t in U:
        for (src, field) in s.index[t]:
            if src ∉ U:
                s.heap[src].node.fields[field] = None
    # phase 2: U의 슬롯을 비우고 세대를 올린다. 엣지를 따라가지 않는다.
    for x in U:
        s.heap[x] = Slot(gen = s.heap[x].gen + 1, node = None)
    s.index = { t: [e for e in s.index[t] if e.src ∉ U] for t ∉ U }
    s.kids  = { o: [c for c in s.kids[o] if c ∉ U] }
    return s

# OwnershipTeardown.v : root_drop
def root_drop(s, r, U):
    s = teardown(s, U)
    s.roots.remove(r)
    s.root_gen[r] += 1          # 낡은 루트 핸들은 다시 맞지 않는다
    return s
```

모델에서 phase 1은 각 `src`마다 `hit(U, index, src, field)`로 표현된다.
위의 색인 순회와 같다는 근거가 `index_finds_every_incoming`이다.
아래 색인·kids 재구성은 여러 행을 필터한다. 이 의사코드나 그 정리만으로
전체 metadata 순회가 없거나 비용이 `|U|`에만 비례한다고 말하지 않는다.
저장된 필드는 지워지고, 지역 변수에 남은 핸들은 세대가 맞지 않아 검사 읽기가
`None`을 돌려준다(`released_unit_local_read_none_forever`).

```text
# OwnershipTeardownAuthority.v : begin_lease / end_lease
def begin_lease(a, link, kind, borrower):
    if not target_current(a.forest, RetireNode(link)):  return Refused(StaleIdentity, a)
    if not target_authorized(a, RetireNode(link)):      return Refused(MissingCleanupRight, a)
    a.leases.push(Lease(a.next_lease, borrower, link, kind)); a.next_lease += 1
    return Accepted(a)                     # 번호는 재사용 후에도 다시 쓰지 않는다

def end_lease(a, id):                      # 번호를 복사해 와도 소유자가 아니면 거절
    lease = find(a.leases, id) or return Refused(MissingLease, a)
    if lease.holder != a.acting_context: return Refused(WrongLeaseHolder, a)
    a.leases.remove(lease); return Accepted(a)
```

## 4. 무엇을 증명했나: 트리 해체

| 성질 | 정리 |
|---|---|
| 정확한 해체 단위는 항상 존재하므로, 기본 모델에서 해제와 루트 drop은 항상 한 걸음이 있다 | `release_always_succeeds`, `root_drop_always_succeeds` |
| 해체 후 고아, 매달린 저장 링크가 없다 | `inv_step`, `release_clears_incoming` |
| 결과는 단위를 나열한 순서에 의존하지 않는다 | `teardown_deterministic` |
| 해체된 노드의 옛 핸들로는 이후 어떤 걸음도 할 수 없다. 슬롯이 재사용돼도 같다 | `released_handle_never_acts`, `released_slot_reusable` |
| 대여나 핀이 걸린 후손이 있으면 해체가 거절된다 | `active_descendant_loan_or_pin_blocks` |

"항상 성공"은 권한 검사 없는 기본 모델의 말이다. 권한 계층(§3)에서는 권한
없음, 대여, 낡은 식별자로 거절될 수 있고, 거절은 상태를 바꾸지 않는다.

## 5. 일괄 해체: 전부 아니면 아무것도

```text
# OwnershipTeardownAtomicBatch.v : retire_batch
def retire_batch(a, requests) -> Accepted(a') | Refused(why, a):
    # 1) 모든 요청을 같은 원래 상태 a에 대해 검사한다
    for i, req in enumerate(requests):
        r = retire(a, req.target, req.unit)
        if r is Refused: return Refused(r.why, a)
        if not independent(req, requests[i+1:]):     # 같은 대상, 겹치는 단위
            return Refused(InvalidUnit, a)
    # 2) 순서대로 적용한다. 중간 상태는 수학적 값이다.
    cur = a
    for req in requests:
        r = retire(cur, req.target, req.unit)        # 현재 식별자를 다시 검사
        if r is Refused: return Refused(r.why, a)    # 원래 상태를 돌려준다
        cur = r.after
    return Accepted(cur)
```

실제 메모리 해제는 되돌릴 수 없다. 그래서 native 구현은 묶음 전체의 안정된
사전 검사와, 그 뒤 거절하지 않는 커밋이 필요하다. 임시 공간 할당·권한/lease
검사·재진입과 publication까지 이 경계에 포함된다. 이 연결은 OPEN이다.

## 6. 그래프 store: 한 소유자, 여러 이름

```text
# OwnershipGraphLinks.v
def resolve(g, link):                     # link = (store, slot, gen)
    s = g.stores[link.store] or return Missing(RNoStore)
    sl = checked_slot(s, link.slot) or return Missing(RStale)
    if sl.gen == link.gen and sl.node: return Found(sl.node)
    return Missing(RStale)

def insert(g, store, idx, data, edges, blocks, new_table):
    s = g.stores[store] or return Refused(RNoStore)
    if idx < len(s.slots):                          # 빈 슬롯 재사용
        if s.slots[idx].node:          return Refused(RConflict)
        if s.slots[idx].gen >= gmax:   return Refused(RExhausted)   # 은퇴한 슬롯
        require blocks are distinct and free, else Refused(RNotFree)
        put node at idx with current gen
    else:                                           # 테이블 확장
        if store_borrowed(store):      return Refused(RBorrowed)    # 테이블이 움직인다
        if idx != len(s.slots) or gmax == 0: return Refused(RBadIndex)
        require new_table and blocks are distinct and free after old-table release,
                else Refused(RNotFree)
        replace table storage with new_table
        append slot with gen 0
    return Link(store, idx, gen)

def delete(g, link):
    check store exists, else Refused(RNoStore)
    check slot exists and occupied and gen matches, else Refused(RStale)
    if borrowed(link.store, link.slot): return Refused(RBorrowed)   # 지우는 쪽이 받는다
    slot.gen += 1; slot.node = None; free(node.blocks)

def begin(g, link, write):                          # 붙잡기
    resolve(g, link) or Refused(RStale/RNoStore)
    if conflicts(write, link): return Refused(RConflict)
    g.borrows.push_front(Borrow(link.store, link.slot, write))

def drop(g, store):
    check store exists, else Refused(RNoStore)
    if store_borrowed(store): return Refused(RBorrowed)
    free(store.table and every slot's blocks)       # 슬롯 단위, 엣지를 따라가지 않음
```

store가 끝날 때의 정리는 살아 있는 store 안의 회수가 아니다. 링크가 하나도
가리키지 않는 노드도 store가 끝날 때까지 남는다(`unreachable_nodes_retained`).
store id는 `g_new`가 단조 증가하는 자연수 `gsid`로 발급하고 `g_drop`은 이를
되돌리지 않는다. 모델에 store id 재사용 결함이 남았다는 뜻은 아니다. 유한
native store/root/lease 번호의 고갈·wrap 방지는 별도 구현 의무다. 또한 이 연산은
graph fragment다. 다른 owner의 블록까지 고려한 할당은 28의 `gexec_framed`
경계가 필요하며, raw `gexec`를 whole-heap 안전성으로 읽으면 안 된다.

## 7. 행동 범위 참조와 saga

```text
# OwnershipGraphActionScope.v
Receipt = { out: StepDone | AcqFailed | BodyRefused | DerefFailed,
            completed: Nat }                     # 성공한 본문 접두부; 읽기도 센다

def run_step(g, step) -> (g', Receipt):
    n = 0
    for (link, write) in step.acq:                  # 시작할 때 전부 붙잡는다
        r = begin(g, link, write)
        if r is Refused:
            return release(g, n), Receipt(AcqFailed(r.why), 0)
        g = r.after
        n += 1
    out = StepDone; completed = 0
    for op in step.body:                            # 본문에서는 붙잡기를 바꾸지 않는다
        if op is Read(link):
            if resolve(g, link) is Missing:          # 허용된 단계에서는 일어나지 않음
                out = DerefFailed(link); break
        else:
            r = exec(g, op)
            if r is Refused: out = BodyRefused(r.why); break
            g = r.after
        completed += 1
    return release(g, n), Receipt(out, completed)   # 앞서 성공한 본문 효과는 남는다

def run_saga(g, steps, gaps):                       # gaps(t): 단계 사이에 다른 코드가 하는 일
    done = []
    for i, s in enumerate(steps):
        g = exec_all(g, gaps(next_t()))
        g, out = run_step(g, s.forward)
        if out.out != StepDone:
            for (j, comp) in done:                  # 최근 것부터 보상
                g = exec_all(g, gaps(next_t()))
                g, c = run_step(g, comp)
                if c.out != StepDone: return g, SagaStuck(i, j, out, c)
            return g, SagaCompensationFinished(i, out)
        done.push_front((i, s.compensation))
    return g, SagaDone
```

허용 조건(`step_ok`)은 본문이 읽는 링크를 모두 `acq`에 넣고, 본문 안에서
`begin`·`end`를 하지 않는 것이다. 이 조건이면 `DerefFailed`는 나오지 않는다
(`admitted_step_never_fails_deref`, `saga_never_fails_deref`). 보상 대상이 단계
사이에 지워지면 `SagaStuck`이 된다. 이를 막는 삭제 권한 연결은 OPEN이다.

여기서 **본문 short-circuit와 효과 rollback은 다르다.** 실패 이후의 연산은
실행하지 않지만 그 이전의 write/insert는 남는다. `run_body_receipt_sound`와
`run_body_receipt_projects`가 기록한 접두부와 실제 상태 전이를 연결한다.
`completed`는 undo log나 효과 개수가 아니며 정확한 본문·실행 세대에 묶여야 한다.
모델의 borrow 표는 ghost state다. 매 획득마다 runtime 레코드를 할당하라는 규칙이 아니다.

`SagaCompensationFinished`도 성공이 아니다. 끝난 단계들의 보상 코드가 끝났다는
뜻이고, 실패한 현재 단계의 전체 보상은 실행하지 않는다. 부분 forward에 전체
보상을 적용해도 맞는다는 증명이 없기 때문이다. `SagaStuck`은 처음 실패한 단계와
실패한 보상 양쪽의 결과를 남긴다. 자동 재시도나 성공으로의 변환은 허용하지 않는다.

| 실행 | 관측되는 결과 | 남는 상태 |
|---|---|---|
| 새 노드 삽입 → 없는 노드 삭제 실패 | `SagaCompensationFinished(0, BodyRefused, prefix=1)` | 삽입한 노드가 남음 |
| A를 10→20으로 씀 → 다음 단계 획득 실패 → 빈 보상 | `SagaCompensationFinished(1, AcqFailed, prefix=0)` | A는 여전히 20 |
| 위 실패의 보상이 A를 15로 씀 → 그 보상도 실패 | `SagaStuck(1, 0, forward=AcqFailed/0, comp=BodyRefused/1)` | A는 15, 두 실패 모두 보존 |

이 반례들은 `step_ok`인 프로그램이다. 참조 안전성 증명의 오류가 아니라 보상
종료를 효과 복구로 읽으면 안 되는 이유다. 복구 성공을 주장하려면 단계 원자성,
부분 진행 보상 또는 명시적 도메인 사후조건을 따로 선택·증명해야 한다.

## 8. 선택적 store 국소 순환 회수

사용자가 2026-10-09에 고른 범위는 **선택적, store 국소**다. 기본 정리는 여전히
추적 GC가 아닌 소유권 정리다. 이 선택적 도달성 회수 자체는 GC 계열이며,
그 분류를 기본 정책과 혼동하지 않는다.

```text
# OwnershipGraphCycleReclaim.v : counted_reclaim
def counted_reclaim(c, store, C, fuel, roots) -> Accepted(c') | Refused(c, why):
    s = c.graph.stores[store] or return Refused(c, BatchMissingStore)
    if err := candidate_error(s, C):                 # 중복 후보, 빈 슬롯
        return Refused(c, err)
    held = [i for i in C
            if rooted(i, roots) or internal(C, i) < c.counts[store][i]]   # 밖에서 잡힘
    L = close(fuel, s, C, held)                      # C 안의 엣지를 따라 held를 닫는다
    if not (covers(held, L) and closed_in(s, C, L)):
        return Refused(c, BatchSelectionDeferred)    # round 한도 안에 닫힘 미확인; 삭제 없음
    G = C - L
    return delete_batch(c, [Link(store, i, gen(i)) for i in G])

def delete_batch(c, links):                          # counted_delete_batch
    if dup := first_duplicate(links): return Refused(c, BatchDuplicate(dup))
    cur = c
    for l in links:
        r = counted_step(cur, Delete(l))             # 카운터도 함께 갱신
        if r is Refused: return Refused(c, BatchDeleteRefused(l, r.why))
        cur = r.after
    return Accepted(cur)

# 카운터 유지 (count_update): 연산마다
#   옛 엣지가 가리키던 (slot, gen)의 수를 줄이고, 새 엣지가 가리키는 수를 늘린다.
#   delete는 그 슬롯의 식별자 카운트를 0으로 되돌린다(세대가 바뀌므로).
```

`held`의 근거는 바깥 참조 수 = 전체 들어오는 수 − 후보 안에서 오는 수다.
카운터가 실제보다 적게 세지 않는 한, 루트에서 닿는 노드는 절대 고르지 않는다
(`trial_garbage_with_unreachable`). 루트 목록 자체의 완전성은
`OwnershipGraphRootCompleteness.v`가 검사된 언어에서 증명한다. 내부 수
`internal`은 아직 슬롯 목록 전체를 접으므로, 물리적 국소성과 작업량 상한은
증명되지 않았다.
`fuel`은 closure round 수이며 전체 방문·검증·할당·해제의 작업량/시간 한도가 아니다.
`counted_reclaim`은 설명용 raw 경계다. 실제 연결은 권한과 checked roots를 받는
`checked_ledger_reclaim`을 기준으로 하되, 그 정리의 `CountsExact`,
`AllEdgesIssued`, `SimInv` 전제를 실제 producer가 발급해야 한다. 호출자가
L/R을 임의로 빈 목록으로 주는 것은 루트 완전성 검사가 아니다. root binding,
forest/graph 은퇴와 lease/pin의 일치는 여전히 OPEN이다.

## 9. 누가 실패를 받는가

| 상황 | 결과 | 받는 쪽 | 상태 |
|---|---|---|---|
| 낡은 링크로 접근 | `Missing(RStale)` | 읽는 쪽 | 불변 |
| 행동 안에서 읽을 대상이 이미 없음 | `AcqFailed` (단계 시작) | 단계, saga는 보상 | 잡은 것 해제 |
| 본문 일부 실행 뒤 원시 연산 거절 | `BodyRefused` + 성공 prefix | 단계/saga 호출자 | 앞선 효과 유지, 자기 hold 반환 |
| 붙잡힌 노드 삭제, store drop | `RBorrowed` | 지우는 쪽 | 불변 |
| 권한 없는 해체 | `MissingCleanupRight` | 해체 요청자 | 불변 |
| 대여·핀이 걸린 단위 해체 | `ActiveLease` | 해체 요청자 | 불변 |
| 단위가 서브트리와 다름 | `InvalidUnit` | 해체 요청자 | 불변 |
| 일괄 해체 중 하나라도 거절 | 그 이유로 전체 거절 | 요청자 | 원래 상태 |
| closure round 한도 내 닫힘 미확인 | `BatchSelectionDeferred` | 회수 호출자 | 아무것도 안 지움; 총 비용 상한 아님 |
| 완료 단계들의 보상 실행 종료 | `SagaCompensationFinished` | saga 호출자 | 복구 미확인; 실패 단계의 효과가 남을 수 있음 |
| 보상 대상이 사라지거나 보상 중 실패 | `SagaStuck` + 두 receipt | saga 호출자 | 보상의 부분 효과도 남을 수 있음 |

## 10. 열린 것

- 생산자: 최종 MIR에서 루트, 대여, 결과, inout 복원 사실을 만드는 owner.
- 원자성: native의 사전 검사와 거절하지 않는 커밋, 할당 실패 지점.
- 표면: 행동 단계가 붙잡을 것을 이름 붙이거나 추론하는 방법. 새 키워드는 정하지 않았다.
- 순회: 처음 닿을 때 붙잡기와 그 원자성.
- 권한 연결: 보상 대상 보호, 그래프 store와 트리 권한 장부의 하나의 owner.
- 효과 복구: prefix-aware 보상/단계 원자성과 source effect 연결; 함수 종료는 복구 증거가 아니다.
- 유한 표현: store/root/lease identity 고갈, 카운터 overflow와 재사용 정제.
- 동시성, async 중단, FFI, C/LLVM refinement, 비용 측정.

이 문서는 언어 전체의 메모리 안전, GC 대비 성능 우위, 구현 완료를 주장하지 않는다.
생산자·마지막 소비자·거절·은퇴·반증·착수 조건은
[구현 경계 문서](semantics/ownership_lifecycle_implementation_boundaries.md)에 모았다.
