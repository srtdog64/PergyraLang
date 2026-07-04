# 147. Surface Sugar Triad — `?` / 문자열 보간 / tuple 반환

Status: `?`+보간 = **landed & gated**, tuple = **design-deferred (WO-U3)**.
Owner doc for the top-3 user pain points measured on the largest real Pergyra
corpus (`src/self_hosted/`, ~20k lines, 2026-07-03 감사).

## 0. 왜 이 셋인가 — 실측 근거

self_hosted 코드베이스에서 매일·전 파일에서 반복되는 3개 의식(ritual):

1. **Option 소비 4줄 의식** — `let o = F(); if !IsSome(o) { return X; } let v = UnwrapOption(o);`
   가 한 파일에 수십 회. sentinel `-1`이 오래 살아남은 진짜 이유: 교리(errors-as-data)를
   따르는 쪽이 타이핑 3배 비쌌다.
2. **Concat 피라미드** — `Concat(Concat(s0, key), "|")`. 문자열 조립이 항상 소음.
3. **`inout Array<Int>` 아웃파라미터 흉내** — 다중 반환 부재로 1칸 배열을
   C 포인터처럼 사용 (`ArraySet(cursor, 0, i)` / `i = cursor[0]`).

## 1. 감사 결과 — "없다"는 전제가 틀렸다

| sugar | 감사 전 가정 | 실제 (2026-07-03 확인) |
|---|---|---|
| `?` 전파 | 없음 | **Result<T,E> + Option<T> full end-to-end** (parser/semantic/C/LLVM, fixtures `try_operator_result`, `try_operator_option`). let-초기화 한정 계약 |
| 보간 | 없음 | **full end-to-end**: `"...${expr}..."`(일반 문자열), `f"...{expr}..."`, `$"..."` 전부. fixture `string_interpolation` parity green |
| tuple 반환 | 없음 | 없음 (MIR 타입명 인코딩만 존재 — F1 확인) |

**교훈**: 미래 세션은 "sugar가 없다"는 전제로 착수 금지 — 이 표를 먼저 보라.
진짜 갭은 두 개였다: (a) `?`의 **Option 변형 부재**(당시 semantic이
명시 거절 — self-host는 Result가 아니라 Option-heavy라 정확히 여기가
아팠다), (b) **subset adoption gap**: self-호스트 bounded subset이 sugar를 실제
코퍼스에 넓게 채택할 만큼 아직 소비하지 못했다. (a)는 §2에서 닫혔고,
(b)는 §5의 adoption/parity hardening으로 축소됐다.

## 2. 이번에 landed — `?` Option<T> 확장

계약 (Result 대칭 + None 특성 활용):

```pergyra
func Doubled(x: Int) -> Option<Int> {
    let found: Int = FindEven(x)?;   // None이면 즉시 return None
    return Some(found * 2);
}
```

- **let-초기화 한정** (기존 Result 계약과 동일; 중첩 식 위치는 parse-level 거절 유지).
- **None 전파는 cross-type 허용**: `Option<Int>` operand가 `Option<String>` 반환
  함수 안에서도 전파된다 — None은 payload가 없어 반환형의 fresh None을 재구성
  (Err는 payload 타입이 맞아야 하는 Result와 대비되는 Option만의 이점).
- **enclosing이 Option/Result 아닌 함수에서의 `?`** = checked unwrap:
  `option-unwrap-none` panic (Result의 기존 계약과 대칭).
- **혼합 kind 전파 금지** (Option operand in Result-fn, 역방향): 양 백엔드 동일
  panic — LLVM 구조 분기(2-field vs 3-field)를 operand field 수로 게이트해 parity 고정.

구현 좌석: `type_checker_expr_ops.c`(수용), `transpiler_let_emit.c` +
`transpiler_mir_preserved_let_emit.c`(C 트윈 2좌석), `llvm_expr_unary_core.c`
(2-field None 재구성 분기 + 3-field 가드). `llvm_stmt_type_infer.c`는 구조적이라
무변경 호환.

게이트: fixture `try_operator_option`(값/전파/cross-type/문자열 payload 4-leg,
C==LLVM), semantic 케이스 2종(수용 + `5?` 거절 메시지), 기존 Result fixture
무회귀, self-hosted parser/semantic/codegen subset fixtures.

## 3. 보간 — 결정 기록 (구현 변경 없음)

- 정본 형태: 일반 문자열 내 `${expr}`. `f"..."`(bare `{expr}`), `$"..."` 는
  수용되는 별칭 (lineage: C# `$`, Python `f`).
- format specifier(폭/정밀도)는 **비도입** — 필요 시 별도 결정.

## 4. tuple 반환 — 의도적 보류 (WO-U3)

parser+semantic 수용만 먼저 넣으면 **accepted-then-broken**(semantic 통과 후
codegen 실패 = 최악 클래스)이라 부분 착지 금지. 필요한 전체 묶음:
tuple 타입 표면 + 리터럴 + 구조분해 let + **양 백엔드 값 ABI**(struct-like
lowering) + parity. 전부 한 번에 — TODO WO-U3.

주의: 아웃파라미터 고통의 절반은 subset 채택(§5) 후 `?`가 이미 줄인다
(cursor 프로토콜의 상당수가 Option 반환으로 재설계 가능).

## 5. 남은 진짜 갭 — subset adoption/parity hardening (WO-U2)

self-호스트 subset은 더 이상 "sugar를 모른다"가 아니다. parser는 postfix
`?`와 문자열 보간 desugar owner를 갖고, semantic/body checker는 Option
`?` operand를 검사하며, codegen subset에는 `option_try` fixture가 있다.
남은 갭은 **기능 부재**가 아니라 **최대 self_hosted 코퍼스 채택과
hard-replacement parity**다: json.pgy 계열 Option 의식을 `?`로 재작성
(감사 §0의 실증 파일부터) → selfcheck/codegen parity 유지 → sentinel/munge
ratchet 하향.

## 6. 표면 위생 3-pair (docs/134)

- vocabulary: 신조어 0 (`?`, `${}` — 범용 관례).
- lineage: Rust(`?`), C#(`$""`)/Python(`f""`) — docs/119 substrate-borrow 정합.
- capability: 주장 변화 0 — 순수 sugar, 의미론/안전 등급 불변
  (`?`는 fail-closed panic 계약을 그대로 상속).

## Related
- docs/134 표면 위생 / docs/119 계보 / F1(튜플 MIR 인코딩) / TODO WO-U1~U3
