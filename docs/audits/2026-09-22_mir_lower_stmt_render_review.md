# MIR 문장 렌더러 리뷰와 남은 한계 (2026-09-22)

Status: AUDIT RECORD. 게이트나 owner 문서가 아니다.
Snapshot: `main@951e04df` + 다른 세션의 미커밋 변경이 있는 작업 트리.

대상:

- `src/self_hosted/mir_lower/stmt_render.pgy`
- `src/self_hosted/mir_lower/match_binding_render_owner.pgy`
- 호출부 `src/self_hosted/mir_lower/routine_lower.pgy`
- 배열 fact 읽기 `src/self_hosted/lib/json_bounded_fact_read.pgy`

MIR JSON fact를 compact AST 트리 텍스트로 되돌리는 렌더러를 읽고, 에러 없이 틀린 출력을
만드는 경로를 고쳤다. 고칠 수 없었던 것은 원인에 따라 **언어 한계(L)**와 **fact/인프라
공백(F)**으로 나눠 아래에 남긴다.

## 1. 이번에 고친 것

| # | 이전 동작 | 이후 동작 |
| --- | --- | --- |
| 1 | `def`/`AST_LET_DECL`의 `arg0`(이름)가 비면 `Assign: expr1 = expr0` 또는 `=`가 없는 `Assign: value`로 바뀌어 출력됨 | `local declaration is missing its binding-name fact`로 실패. let이 아닌 `def`도 대입 대상(`expr1`)이 없으면 실패 |
| 2 | return/assign/def/stmt가 아닌 kind(`phi`, `cleanup` 등)는 `expr0` 텍스트가 트리 한 줄로 그대로 나감 | `instruction kind has no statement rendering: <kind>`로 실패 |
| 3 | `StmtToTreeLine`이 첫 `:`를 타입 구분자로 봐서, 타입 표기 없는 `let x = {"a": 1}`가 `Let: x = {"a" : 1}`로 깨짐 | `:`가 이름 뒤, `=`보다 앞에 있을 때만 받아들이고 나머지는 실패 |
| 4 | destructure 바인딩 중간에 빈 이름이 있으면 루프가 거기서 끝나 나머지 바인딩이 사라짐 | 모든 이름을 먼저 검사하고, 빈 이름은 `destructure binding-name fact is empty at index N`로 실패 |
| 5 | destructure `elem_type`이 `"Unknown"`이어도 통과 (match binding은 거부함) | 같은 기준으로 거부 |
| 6 | 배열 fact 읽기(`MirObjectArrayStringFactsAtBounds`)가 문자열이 아닌 원소를 만나면 그때까지 읽은 앞부분만 반환 | `lib/json_bounded_fact_read.pgy`에 `JsonArrayStringsWithin` 추가. 원소 하나라도 잘못되면 false. destructure와 match binding 렌더러는 `MirObjectArrayBoundsAtBounds`(필드 존재) + `JsonArrayStringsWithin`(원소 검증)으로 읽고, 두 렌더러에 느슨한 reader가 다시 들어오지 못하게 component contract에 음성 규칙을 추가 |
| 7 | defer 본문의 직접 호출이 `f(x)`로 나가고, 일반 문장은 `Call: f(x)`로 나감 | defer 쪽도 `Call: f(x)`. `Log(...)`는 두 경로 모두 bare로 유지 |
| 8 | `RenderDirectDeferCallFromGraph`가 `node_kinds` 길이만 검사하고 나머지 arena 열은 검사 없이 인덱싱. `left_children`에 순환이 있으면 무한 루프 | 열 길이가 모두 같은지 검사하고, 호출 인자 체인을 노드 수만큼만 따라감 |
| 9 | 호출되지 않는 래퍼 `RenderDestructureFromFacts`, `RenderDeferFromFacts` (행 번호 0과 버리는 order를 넘김) | 삭제 |
| 10 | `routine_lower`에서 destructure 행이 바깥 분기에 다시 들어가 분기마다 `ikind != "destructure"`를 반복 검사 | 바깥 조건에서 destructure를 빼고 반복 검사를 제거 |
| 11 | Concat 7~8단 중첩, match binding의 `"\n"`이 initializer 안에 섞여 있음 | 배열 + `StringJoin`으로 조립하고, 줄바꿈은 줄 끝에서만 붙임 |

### 리뷰 때 틀렸던 판단

"match binding은 `match_bindings` 필드가 없으면 조용히 빈 결과를 낸다"는 리뷰 판단은
틀렸다. 필드가 없거나 `match_binding_types`에 `null`이 들어간 입력은 렌더러보다 먼저
routine fact index가 `routine MIR fact index is incomplete: ... [match_binding_type_count]`로
거부한다. 렌더러의 엄격한 읽기는 방어를 한 겹 더하는 것이고, 새 스모크는 이 두 입력에 대해
앞 단계의 진단을 기대하도록 작성했다.

## 2. 검증

- 새 게이트 `tests/self_hosted/mir_lower_stmt_render_fail_closed_smoke.sh`
  (`make mir-lower-stmt-render-fail-closed-test-smoke`, `test-mir`에도 연결)
  - 양성: destructure 임시 변수 줄, direct-call defer의 `Call: Cleanup("done")`
  - 음성 7건: 빈 바인딩 이름, 문자열 아닌 바인딩, `Unknown` 원소 타입,
    match 필드 삭제, `null` 바인딩 타입, 이름 없는 let, `cleanup` kind
- 새 parity 픽스처 `src/self_hosted/mir_lower/fixture/defer_direct_call.pgy`
  (MIR parity manifest 31번, core 개수 31 → 32). 이전에는 direct-call defer 경로를
  실행하는 parity 픽스처가 없었다.
- `tests/match_binding_type_fact_smoke.sh`, `tests/destructure_type_fact_smoke.sh`: 통과.
- MIR JSON parity (`intent_nested_direct` 제외 118개): 97개 실행 결과 일치.
  `defer_direct_call`도 포함되며 oracle과 via-MIR 모두 `work`, `done`을 출력했다.
  98번째 `str_case_math`에서 codegen이 재구성 AST를 거부해 중단됐다(3절).
- 수정 전후 `mir_lower` 출력 비교 (manifest 119개 전체): 118개는 바이트 단위로 같다.
  다른 하나는 `defer_direct_call`의 `Call:` 접두사뿐이다. parity가 중단돼 실행하지 못한
  나머지 20개도 재구성 AST가 수정 전과 같다.
- `tests/self_hosted_component_contract_smoke.sh`: 수정한 파일에 걸린 규칙 106개는 통과.
  스크립트 전체는 다른 세션이 수정 중인 파일 5개(`ast_collection_ownership_verdict_owner.pgy`,
  `mir/json_projection_owner.pgy`, `collection_ownership_fact_owner.pgy`,
  `routine_fact_index_owner.pgy`, `src/pgy_driver.c`)의 줄 수 상한 초과로 실패한다.

## 3. 수정 전 기준선에서 이미 실패하던 것

`tests/self_hosted/parity/mir_json_parity.sh`는 이 수정 **전** 트리에서도 실패했다.

```
[self-host-parity:mir-json] intent_nested_direct: missing-zone-authority diagnostic drifted
MIR-LOWER ERROR: MIR domain topology facts are missing or invalid
```

이 픽스처 전용 음성 검사가 기대하는 진단 문구와 실제 문구가 다르다. 렌더러와는 관계가
없다. 그래서 수정 후 parity는 `PGY_SELFHOST_MIR_FIXTURES`로 `intent_nested_direct`만 빼고
나머지 118개로 돌렸다.

두 번째로, `str_case_math`에서 codegen이 재구성 AST를 거부한다.

```
[self-host-parity:mir-json] str_case_math: codegen rejected the reconstructed AST:
CODEGEN ERROR: codegen semantic body type bundle is missing or invalid: bundle_verdict:ast_artifact_invalid:node=9:Diagnostic: pgy.selfhost.semantic.v1
```

수정 전에 빌드한 `mir_lower.exe`도 같은 MIR JSON에서 바이트 단위로 같은 AST를 만든다.
따라서 이것도 이 수정과 관계없는, codegen 쪽에 원래 있던 실패다.

## 4. 남은 한계 — 언어

### L1. 반환하지 않는 함수를 표현할 타입이 없다

`MirLowerFailClosed(msg) -> Void`는 `Exit(1)`로 끝나지만 타입 검사기는 이 함수가
돌아온다고 본다. 그래서:

- 호출 뒤의 코드가 타입상 계속 실행되는 것으로 읽힌다. 예를 들어 `StmtToTreeLine`은
  `colon < 0`에서 실패시킨 다음 줄에서 `Substring(rest, 0, colon)`을 부른다.
- 값을 반환해야 하는 분기 끝에 실행되지 않는 `return "";`를 붙일지 말지가 파일마다 다르다.

제안: `Never`(또는 `-> !`) 반환 타입. 호출 뒤 코드를 도달 불가로 보고, 값 반환 분기의
끝으로 인정한다. 이 타입이 생기면 `MirLowerFailClosed`와 codegen/global `Die`를 가장
먼저 바꾼다.

### L2. 문자열 조립에 2항 `Concat`만 있다

`Let: NAME : T = V\n` 한 줄에 `Concat` 7~8단 중첩이 필요했다. 이번에는
`let parts: Array<String> = [...]; StringJoin(parts, "")`로 피했지만, 배열을 하나 할당해야
하고 줄마다 임시 변수가 생긴다. AGENTS.md가 DX 부채로 꼽은 "string-concatenation pyramids"와
같은 문제다.

제안: 문자열 보간(`"Let: {name} : {ty} = {init}"`)이나 가변 인자 `Concat`.
`StringJoin([a, b], "")`처럼 배열 리터럴을 인자로 바로 넘기는 형태도 지금 코드베이스에서
쓰는 곳이 없어서, 이번에는 지역 변수를 거쳤다.

### L3. `Some`/`None`/`Ok`/`Err`가 예약어처럼 쓰이지만 예약되어 있지 않다

`src/common/match_variant_policy.c`의 이름표를 parser, semantic, C, LLVM이 함께 쓰고,
self-host 렌더러도 variant 이름 문자열로 `Unwrap*`을 고른다. 그런데 사용자 enum에
같은 이름의 variant를 선언해도 막지 않는다. 재현:

```pgy
enum Verdict { Ok(Int), Bad(String) }
func Describe(v: Verdict) -> Int {
    match v { case Ok(p): return p; case Bad(r): return -StringLength(r); }
}
```

- semantic: `0 error(s)`
- MIR JSON: `"match_variant":"Ok","match_bindings":["p"],"match_binding_types":["Int"]`.
  subject의 타입 계열을 나타내는 fact는 없다.
- 네이티브 C: `C MIR match lowering cannot derive payload type for Ok(p); explicit
  Option<T>/Result<T,E> subject type is required`로 실패한다. 적어도 실패하긴 한다.
- self-host `mir_lower`: `Let: p : Int = Unwrap(v)`로 렌더링한다. 사용자 enum에
  `Unwrap`을 부르는 틀린 AST다.

pgy 쪽만으로는 고칠 수 없다. 둘 중 하나가 필요하다.

1. semantic이 사용자 enum variant 이름으로 `Some`/`None`/`Ok`/`Err`를 거부한다
   (언어 규칙으로 예약).
2. MIR instruction에 match subject의 타입 계열(`option`/`result`/`nominal`)을 fact로
   싣고, C와 self-host가 이름 대신 그 fact로 분기한다.

`match_binding_render_owner.pgy` 파일 머리 주석에 이 공백을 적어 두었다.

### L4. 실패 가능한 읽기를 한 칸짜리 배열과 inout으로 표현한다

`MirObjectArrayBoundsAtBounds(..., inout bounds: Array<Int>) -> Bool`,
`ReadJsonStringBounded(..., value_end: Array<Int>)`, 이번에 추가한
`JsonArrayStringsWithin(..., inout values) -> Bool` 모두
"성공 여부 Bool + 값은 out 배열" 모양이다. AGENTS.md가 DX 부채로 꼽은
"one-element out-parameter arrays"다.

제안: `Result<Array<String>, E>`나 `Option<(Int, Int)>` 반환이 비용 없이 쓰일 수 있게
되면(튜플/제네릭 Option의 self-host 지원) 이 모양을 모두 옮긴다.

## 5. 남은 한계 — fact/인프라

### F1. destructure 임시 변수가 필요한지를 텍스트로 판단한다

`MirDestructureNeedsTemp`는 초기식 텍스트에 `(`, `[`, 공백, `,`가 있는지만 본다.
`stmt_render.pgy`와 `destructure_expression_projection_owner.pgy`가 같은 판단을
공유해야 expression 순서가 맞기 때문에, 한쪽만 graph 기반으로 바꿀 수 없다.
필요한 것: C MIR가 초기식이 "한 번만 평가해야 하는 식"인지를 fact로 싣고, 두 owner가
그 fact를 읽는다.

### F2. `_pgy_destructure_` 접두사가 예약되어 있지 않다

임시 변수 이름은 `_pgy_destructure_<첫 바인딩>`이다. 같은 스코프에서 `let`
재선언을 금지하는 한 destructure끼리는 충돌하지 않지만, 사용자가 이 이름으로 변수를
선언하는 것은 막지 않는다. 필요한 것: semantic에서 `_pgy_` 접두사 예약 진단.
이름에 행 번호를 섞는 방법도 있지만, 그러면 F1과 같은 이유로 두 owner를 함께
바꿔야 한다.

### F3. 느슨한 배열 reader가 남아 있다

`MirObjectArrayStringFactsAtBounds`는 여전히 잘못된 원소를 만나면 앞부분만 반환한다.
이번에는 렌더러 두 곳만 엄격한 읽기로 옮겼다. 남은 호출부(`match_binding_local_fact_owner`,
`structured_condition_emission_owner`, `routine_instruction_use_fact_owner`)를 옮긴 다음
느슨한 버전을 지우고 음성 게이트로 막아야 한다.

엄격한 버전을 `mir_lower/json_fact_read.pgy`에 두지 못한 이유: 이 파일은 줄 수 상한 450에
이미 도달해 있고, 쓰이지 않는 `MirObjectEnd`/`MirRoutineArrayBounds`는 contract가 안정
facade로 존재를 고정하고 있다. 그래서 범용 lib에 12줄짜리 함수로 넣었고, 그 파일도
정확히 450줄이 되어 주석을 달 자리가 없었다. `JsonArrayStringsWithin`은 원소마다
`JsonArrayStringFactWithin`으로 배열 전체를 다시 훑기 때문에 원소 수의 제곱에 비례한다.
바인딩 배열처럼 짧은 배열에는 문제가 없지만, 긴 배열에 쓰기 전에 한 번 훑는 구현으로
바꿔야 한다.

### F4. `routine_lower`가 expr0 없는 행을 말없이 버린다

`EmitBlockStmtsWithExpressionOrder`는 bind/defer/void-return 분기에 해당하지 않고
`expr0`도 빈 행을 아무것도 출력하지 않고 넘어간다. `cleanup`, `phi`, 구조적 `stmt`처럼
원래 출력할 게 없는 행과 fact가 빠진 행을 구분할 fact가 없어서, 이번에는 fail-closed로
바꾸지 않았다. 필요한 것: 문장을 출력하지 않는 kind/source_type의 허용 목록, 또는 MIR의
"statement-bearing" fact.

### F5. defer 본문은 `Log` 하나 또는 직접 호출 하나만 된다

parser는 `defer { a(); b(); }`를 받지만 MIR 렌더러는 `Log`/`Call` 한 문장짜리 본문만
재구성하고 나머지는 fail-closed한다. 틀린 출력을 내지는 않지만, 표면 문법보다 지원
범위가 좁다.
