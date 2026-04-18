# Type Resolution Audit (단계 1.0)

**작성일:** 2026-04-18
**목적:** Zig 0.16의 "type resolution DAG overhaul"이 Pergyra에도 필요한지 **데이터로** 판정.
**계측 도구:** `PGY_TYPE_RES_STATS=1` 환경변수로 활성화 (`src/semantic/type_checker.c`).

---

## 요약

| 질문 | 답 |
|------|-----|
| 타입 의존성에 사이클이 존재하는가? | **아니오** (모든 예제 topo_ok=1, topo_produced=100%) |
| DAG 인프라가 이미 있는가? | **예** — `TypeResolutionGraph`, `validate_graph`, `build_topo_order` 모두 구현 + 호출됨 |
| `resolve_type_node`가 같은 AST 노드를 재방문하는가? | **예, 평균 73%** — 명확한 낭비 |
| Zig식 "DAG 전면 재설계"가 필요한가? | **아니오** — 그래프는 이미 있고 사이클도 없음 |
| 실제 필요한 작업은? | **AST 노드 단위 memoization** (Type* 캐시) |

**결론:** 베타 게이트로 잡힌 "타입 해석 DAG 전환"은 **1단계 인프라 완료** 상태다. 남은 것은 `topo_order`를 declaration worklist / staged resolver에 더 깊게 연결하는 실행 단계다.

---

## 계측 결과 (7개 대표 예제)

### 그래프 구조 (사이클, 토폴로지)

| 예제 | nodes | edges | topo_ok | topo_produced | 사이클 |
|------|------:|------:|:-------:|:-------------:|:------:|
| order_analytics | 100 | 91 | ✓ | 100/100 | 0 |
| shopping_mall_checkout_refund | 928 | 915 | ✓ | 928/928 | 0 |
| dnd_tavern_campaign | 1168 | 1168 | ✓ | 1168/1168 | 0 |
| campaign_graph_fsm | 386 | 387 | ✓ | 386/386 | 0 |
| logistics_intent_probe | 266 | 275 | ✓ | 266/266 | 0 |
| composite_intent_orchestration | 66 | 69 | ✓ | 66/66 | 0 |
| battle_simulator | 61 | 53 | ✓ | 61/61 | 0 |

**관측:** 모든 예제에서 `topo_produced = node_count`. 즉 **순수 DAG**. 상호참조 nominal type, 재귀 제네릭, where-bound 체인 전부 현재 예제에서 순환 형태로 나타나지 않음.

### resolve_type_node 호출 통계

| 예제 | 호출 수 | 유니크 AST | 재방문율 |
|------|-------:|----------:|--------:|
| order_analytics | 138 | 48 | **65.2%** |
| shopping_mall_checkout_refund | 1469 | 441 | **70.0%** |
| dnd_tavern_campaign | 3352 | 577 | **82.8%** |
| campaign_graph_fsm | 728 | 187 | **74.3%** |
| logistics_intent_probe | 515 | 125 | **75.7%** |
| battle_simulator | 94 | 26 | **72.3%** |

**평균 재방문율 ~73%.** 같은 AST 노드를 2~5회씩 리졸브. memoization 적용 시 호출 수 75~80% 감소 예상.

### 노드 kind 분포

| 예제 | TYPE_REF | BUILTIN | DECL | ALIAS | GENERIC_PARAM |
|------|---------:|--------:|-----:|------:|--------------:|
| shopping_mall | 882 (95%) | 4 | 42 | 0 | 0 |
| dnd_tavern | 1135 (97%) | 4 | 29 | 0 | 0 |
| campaign_graph | 372 (96%) | 4 | 10 | 0 | 0 |

**관측:**
- 그래프는 **call site 중심** — 95% 이상이 TYPE_REF (참조 지점)
- ALIAS, GENERIC_PARAM이 0개인 예제가 대부분 — 타입 alias/제약 기능은 만들어져 있지만 예제에선 사용 드물게
- DECL 비율이 낮음 (3~5%) — 실제 정의된 타입은 소수, 참조는 다수

### 가장 많이 참조되는 타입 (top-indeg)

| 예제 | 1위 | 2위 | 3위 |
|------|----|-----|-----|
| dnd_tavern | Int (609) | String (229) | Void (58) |
| shopping_mall | String (293) | Int (218) | Member (42) |
| campaign_graph | Int (212) | String (51) | Void (22) |
| logistics | Int (89) | String (60) | Courier (12) |

**관측:** 기본 타입 (Int/String/Void)이 압도적. 도메인 타입은 10~50 in-degree. 이 4~5개 타입만 캐싱해도 재방문의 대부분이 제거됨.

---

## 판정 기준 대입

플랜에 명시된 판정 기준:

> - 사이클 0개 + 재방문 호출 <10% → DAG 전환 불필요, 재귀 리졸버 유지
> - 사이클 있음 OR 재방문 >30% → DAG 전환 필요
> - 중간값 → 부분 캐싱(memoization)만으로 해결

우리 데이터:
- 사이클 **0개** ✓
- 재방문 **73%** (>>30%)

**플랜 기준상 "DAG 전환 필요".** 지금은 그 방향이 이미 시작된 상태다. 실제 상황:

- `TypeResolutionGraph`: inventory + cycle diagnostic + topo derivation 활성
- provider-first staged worklist가 top-level declaration inventory와 local/projection synthetic node를 실제로 소비
- generic `default_type` / generic constraint / `where` bound가 staged DAG resolver 경로를 통과
- role impl / action / intent step / zone authority / party role slot ability consumer가 같은 DAG provenance 회귀 범위에 포함

즉, **그래프 인프라는 존재한다**가 아니라, **그래프 인프라 위로 semantic source-of-truth를 점진적으로 올리는 중**이 현재의 정확한 표현이다.
- `validate_graph`: 사이클 DFS 검증 활성 (type_checker.c:556)
- `build_topo_order`: 토폴로지 정렬 활성 (type_checker.c:626)
- 두 함수 모두 check_program 말미에 호출됨 (type_checker.c:~6142, ~6146)
- **Pass 1 (type alias)** 이후 **Pass 2 (full type-check)**가 재귀 리졸버로 돌아감
- 토폴로지 결과는 buildable 여부만 체크하고 버려짐

즉 그래프는 **검증용**으로만 쓰이고 **실행 순서 가이드**로는 쓰이지 않음.

---

## 진짜 최적화 기회

### A. AST 노드 단위 memoization (Small, High ROI)

**대상:** `resolve_type_node(ASTNode*, SemanticContext*)`
**방법:** `ASTNode *` → `Type *` 해시맵. 같은 AST 노드가 다시 들어오면 즉시 반환.
**위험:** 낮음 — `Type *` 싱글톤 포인터 반환이라 동일성 보장. context 의존 분기는 primitive/slot 경로에만 있고 그건 이미 O(1).
**효과:** 73% 호출 감소 = 컴파일 시간 체감 개선. 대형 예제(dnd_tavern)에서 3352 → 577.

### B. 토폴로지 순서로 Pass 1 확장 (Medium, Medium ROI)

**현재:** Pass 1은 type alias만 해석 → Pass 2가 전부 재귀로.
**개선:** Pass 1에서 토폴로지 순서대로 DECL/ALIAS/GENERIC_PARAM 노드의 타입을 **사전 계산**하고 Pass 2에서는 lookup만.
**위험:** 중간 — 일부 타입은 컨텍스트(scope, current_nominal_decl)에 의존해서 사전 계산이 부정확할 수 있음. 먼저 단순한 케이스(alias만 확장)로 실험.
**효과:** 재방문 + 재귀 깊이 감소. 의존성 체인이 길 때 특히 효과.

### C. 네임스페이스-only 타입 스킵 (Small, Low-Medium ROI)

**현재:** 정의만 하고 인스턴스화 안 되는 타입도 레이아웃/효과 분석 진행.
**개선:** DECL 노드 중 `in-degree == 0`이면 스킵. (코드젠에서 emit 안 하는 것과 연계.)
**위험:** 낮음 — 사용 안 되는 타입을 분석 안 한다고 문제 없음. 다만 에러 메시지 품질 유지 주의.
**효과:** Pergyra는 현재 샘플 규모라 미미. 프로젝트 규모가 10x 커지면 의미 있음.

---

## 현재 판정 교정

이 문서의 초안 표현은 현재 구현보다 한 단계 앞서 있었다.

- 맞는 말:
  - `TypeResolutionGraph`, cycle validation, topo derivation은 이미 구현돼 있다
  - 타입 해석이 순수 ad-hoc recursive lookup만으로 돌아가지는 않는다
- 과장된 말:
  - `타입 해석 DAG 엔진 완성`
  - `베타 게이트 해제 완료`
- 현재 더 정확한 표현:
  - `1단계 DAG 인프라 완료`
  - `graph inventory / cycle diagnostic / topo derivation 완료`
  - `local contract / projection path synthetic node 도입`
  - `provider-first topo-driven staged resolution worklist가 top-level decl 뿐 아니라 local/projection synthetic node를 label별 narrow handler로 소비함`
  - `generic default_type / generic constraint / where bound가 staged DAG resolver 경로에 편입됨`
  - `action / intent step / zone authority / party role slot ability consumer가 provider pre-stage를 타며 staged DAG 경로에 편입됨`
  - `graph regression이 generic consumer schedule / alias cycle provenance / generic default-bound cycle provenance / action-intent-zone-party ability consumer provenance까지 포함됨`
  - `full graph-backed evaluator는 아직 미완`

즉, DAG는 이미 실체가 있지만 아직 컴파일러 전체의 단일 실행 엔진이라고 부르기엔 이르다.

## 권장사항

### 즉시 해야 할 것
- `topo_order`를 declaration staged worklist와 더 직접적으로 연결
- generic default / multi-bound / ability consumer가 이미 타는 staged DAG 경로를 broader declaration consumer까지 확대
- AST 노드 단위 memoization 설계를 별도 debt가 아니라 DAG Phase D와 연결
- 계측 코드(`PGY_TYPE_RES_STATS`)는 **코드에 남겨둠** — 미래 진단용.

### 베타 게이트 판정
**"타입 해석 DAG 1단계 완료"는 맞다.**  
하지만 **"타입 해석 DAG 엔진 완성"은 아직 아니다.**

- 인프라: 완료
- 사이클 검증: 활성
- 토폴로지 정렬: 활성
- topo-driven staged declaration resolution: 진행 중
- full graph-backed evaluator: 미완

따라서 이 축은 베타 직전까지 계속 닫아야 한다. Zig 0.16식 전면 재설계가 그대로 필요한 것은 아니지만, `build_topo_order()` 결과를 실제 declaration worklist로 쓰는 단계까지는 올라와야 문서 표현과 구현이 일치한다.

### 베타 기간 작업으로 미루기 (권장)
1. **AST 노드 단위 memoization** — 작고 안전한 최적화. 다음 세션에서 1~2시간 작업.
2. **토폴로지 순서 Pass 1 확장** — 중간 크기 리팩터링. 실측 데이터 수집 후 필요 시.
3. **namespace-only 타입 스킵** — 규모 성장 후 고려.

### 정식 버전(v1.0) 전
- memoization 완료 후 다시 계측: 재방문율 ~0% 확인
- 컴파일 시간 벤치마크 (대형 예제)

---

## 감사 재현 방법

```bash
mingw32-make "CC=clang --target=x86_64-w64-mingw32" LLVM_ENABLED=0 bin/pgy.exe
PGY_TYPE_RES_STATS=1 bin/pgy.exe examples/shopping_mall_checkout_refund/main.pgy --backend=c 2>&1 | grep "type-res-stats"
```

출력 예시:
```
[type-res-stats] nodes=928 edges=915 duplicate_labels=496 topo_ok=1 topo_produced=928/928
[type-res-stats] resolve_type_node: calls=1469 unique_nodes=441 revisit_rate=70.0%
[type-res-stats] kind: TYPE_REF=882 BUILTIN=4 DECL=42 ALIAS=0 GENERIC_PARAM=0
[type-res-stats] top-indeg[0] String (in=293)
...
```

---

## 한 줄 결론

**타입 해석 DAG는 이미 우리 안에 있다.** 다만 현재 상태는 `검증 + topo derivation`까지이고, 남은 것은 그걸 declaration worklist와 staged resolver의 **실행 가이드**로 끝까지 연결하는 일이다.
