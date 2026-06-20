# 컬렉션 리터럴 전면 확대 — 설계 (잠금)

표면 일관성 패스 ①. 현재 컬렉션 *생성* 규칙이 비대칭이다: Array `[]`·Map `{k:v}`는
리터럴이 있고 List/Set/Queue는 `TNew()`만 있으며, Map은 `{}`+`MapNew()`로 중복이다.
또 Array/List가 둘 다 growable이라 타입이 중복된다.

이 문서는 **결정된 설계**를 기록한다. 구현은 별도 focused 세션(dual-backend, 다단계).
관련: [[project_core_module_layering]] [[project_signed_default_decision]] (불필요 surface 억제 결).

## 결정 (BDFL, 2026-06-20)

### 2단 규칙

1. **문법이 shape를 결정한다.**
   | 문법 | shape |
   |---|---|
   | `[e, ...]` / `[]` | 시퀀스 |
   | `{k: v, ...}` / `{:}` | Map |
   | `{e, ...}` / `{}` | Set |
2. **어노테이션/추론이 시퀀스 내 concrete 타입을 결정한다** (Array 기본, `: List<T>`→List,
   `: Queue<T>`→Queue). 다형 시퀀스 리터럴.

근거: Map과 Set은 *다른 shape*라 문법으로 구분한다. Array/List/Queue는 *같은 shape*
(순서 있는 시퀀스, 성능 특성만 다름)라 type-directed로 흡수한다 — 이로써 Array/List
중복을 리터럴 레벨에서 해소한다.

### 빈 컬렉션: `{:}` 분리 (type-directed 아님)

- `{:}` = 빈 Map, `{}` = 빈 Set.
- 근거: **"콜론 = Map 마커"가 빈 경우에도 안 깨진다.** type-directed 빈 Set은 어노테이션이
  해석 가능할 때만 동작 → 제네릭/추론 문맥(`f({})`)에서 모호. `{:}`는 문법만으로 결정돼
  robust. (Python `{}`=dict 기본의 "빈 set은 `set()`" wart를 피하는 길.)
- **마이그레이션 비용 0**: 현재 코퍼스에 빈 `{}` 사용 없음 (확인됨). `{}`→Set 변경 무해.

### `TNew()` 생성자 유지

`ListNew()`/`SetNew()`/`MapNew()`/`QueueNew()`는 유지한다 (프로그래매틱/명시 생성).
규칙: **모든 컬렉션 = 리터럴 + `TNew()` 둘 다**, 리터럴은 sugar. `{}`+`MapNew()` 중복은
"리터럴=sugar, New=명시" 로 negative-space 문서화하여 의도된 것으로 못 박는다.

## 구현 증분 (순서, 각각 C==LLVM parity 게이트)

1. **Set 리터럴** — 가장 self-contained (새 shape, 기존 array/map 타이핑 불변).
   - 파서: `{` 진입 후 첫 요소에 `:` 있으면 Map, 없으면 Set. 빈 `{:}`→Map, `{}`→Set.
     새 노드 `AST_SET_LITERAL`.
   - 의미: `type_check_set_literal` (map literal mirror).
   - codegen: C(transpiler) 3사이트 + LLVM 8사이트 = map literal mirror → `SetNew`+`SetAdd`.
   - 빈 `{}`=Set 무마이그레이션.
2. **다형 시퀀스** — `[...]`가 expected-type(let 어노테이션/파라미터 타입)에 따라
   List/Queue로. expected-type 인프라(`llvm_mir_local_expected_type.c`) 활용.
   array 리터럴 타이핑/codegen이 타겟 타입을 존중하도록 확장. 기본은 Array(현행 유지).
3. **negative-space 문서** + parity fixture (set/list/queue/map 리터럴 각각 C==LLVM).

## 검증

- 각 증분: C 백엔드 == LLVM 백엔드 byte/run parity (abstraction portability 게이트).
- self-host 툴 무회귀.
- `{}`→Set 전환 시 코퍼스 빈-맵 사용 재확인 (현재 0).

## 구현 상태

- **증분 1a — Set 리터럴 front-end: 완료** (커밋 `3c973c60`). `AST_SET_LITERAL`
  노드 + 파서 disambiguation(`{}`=Set, `{:}`=Map, 콜론 peek) + `type_check_set_
  literal`(Set<T>) + printer. `pgy --ast`로 검증, 기존 map 무회귀(빈 `{}` 마이그
  레이션 0). 의미분석 통과, codegen만 미지원(node type 28).
- **증분 1b — Set 리터럴 codegen: 미구현 (다음 focused 세션).** findings:
  - Set 런타임 **이미 존재**: `pgy_set_new_*` / `pgy_set_add_<suffix>`
    (요소 타입별; `pgy_set_add_string`/`_raw`, `PgySetRaw`).
  - 따라서 codegen = `SetNew()`+`SetAdd()` 빌트인이 lower하는 것과 **같은 경로**.
    map 리터럴 방출(`transpiler_expr_composite_literal_emit.c`
    `emit_map_literal_expression`)을 mirror: `({ T s = pgy_set_new_*();
    pgy_set_add_<suffix>(&s, e); ...; s; })`. suffix 해석은 SetAdd 빌트인
    (`transpiler_expr_stdlib_collection_builtin.c`)의 set-inner 해석 재사용.
  - 손댈 파일(전부 현재 clean, 당신 active set 무충돌 — 단 LLVM 쪽은 작업 전
    재확인): C = `transpiler_expr_composite_literal_emit.c`(+dispatch
    `transpiler_expr_dispatch_emit.c`, type-infer `transpiler_expr_type_infer.c`);
    LLVM = `llvm_expr.c` dispatch+emit, `llvm_stmt_type_infer.c`
    (`Set<` expected-type, map의 `HashMap<` 케이스 mirror).
  - **C==LLVM parity 게이트 필수** (abstraction portability). fixture로 set 리터럴
    추가.
- **증분 2 — 다형 시퀀스 `[...]`→List/Queue: 미구현.** (위 설계 §2단 규칙.)

## 미결 (구현 중 결정)

- List vs Queue의 런타임 표현이 충분히 구분되는가 (둘 다 시퀀스). 다형 시퀀스가
  실제로 별개 concrete를 내는지 codegen에서 확인.
- Set 요소 동등성/해시 정책 (SetAdd 기존 구현 따름).
