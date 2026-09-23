# 205. 언어 한계와 fact 공백 해소 설계 (L1–L4, F1–F5)

Updated: 2026-09-22 (Asia/Seoul)

Status: **DESIGN — 구현 순서와 규칙 제안.** 게이트나 owner 문서가 아니다. 문장 하나가
실행 계약이 되려면 해당 owner 문서에 들어가고 동작 게이트가 붙어야 한다.

출처: `docs/audits/2026-09-22_mir_lower_stmt_render_review.md` §4–5 (L1–L4, F1–F5).
조사 스냅숏: `main@c1a0e4a7`. 파일 위치는 그 시점 기준이며, 일부 파일은 다른 세션이
미커밋으로 수정 중이었다.

## 0. 요약

| 항목 | 판정 | 첫 단계 |
| --- | --- | --- |
| L3 사용자 enum의 `Ok`/`Err`/`Some`/`None` | **정확성 결함.** self-host가 틀린 AST를 만든다 | match subject의 타입 계열을 MIR fact로 싣는다 |
| L2 문자열 조립 | 보간은 **이미 있다**. 문제는 두 컴파일러의 escape 차이, 소유권, 누수다 | self-host escape parity (정확성 결함) |
| L1 `Never` | 언어 기능. `Exit`조차 경로 종료로 취급되지 않는다 | native semantic의 경로 종료 규칙 |
| L4 한 칸 out 배열 | DX 부채. 튜플 없이 지금 고칠 수 있다 | `Option<명명 struct>` 반환으로 이관 |
| F2 `_pgy_` 임시 이름 | 사용자 이름과 겹칠 수 있다. 저장소 정책은 예약이 아니라 위생이다 | mir_lower 임시 이름 gensym |
| F1, F3, F4, F5 | mir_lower의 fact/인프라 공백 | F3 (엄격한 reader 이관) |

**착지 상태 (2026-09-22):** L2a와 F3는 이 문서와 같은 날 착지했다(각 절의 "착지" 문단).
R6(`inout_argument_alias`, 레드팀 캠페인)도 같은 방식으로 착지했다.

## 1. 공통 원칙

- **의미는 semantic이 한 번 결정해 fact로 싣는다.** 백엔드와 self-host 단계는 그 fact를
  읽고, 이름이나 텍스트에서 의미를 다시 만들지 않는다. L3과 F1이 이 원칙을 어긴 사례다.
- **각 단계는 native와 self-host를 함께 닫는다.** 한쪽만 고치면 parity 부채가 남는다.
  R6(`inout_argument_alias`)가 이 방식의 첫 사례다.
- **각 단계는 동작 게이트를 가진다.** 소스 문자열 검사가 아니라 거부 여부, 산출물이
  남지 않는지, 실행 결과를 본다. 그리고 push CI에 연결한다.
- **언어 어휘는 늘리지 않는다.** `Never`는 `Void`처럼 builtin 타입 이름이다.
  `src/lexer/language_keyword_registry.def` 146행은 바뀌지 않는다.

## 2. L3 — match subject의 타입 계열 fact와 variant 이름 해석

### 2.1 현재

- 이름표 하나(`src/common/match_variant_policy.c`)를 parser, semantic, C, LLVM이 함께 쓴다.
  MIR 계층(`src/compiler`)은 이 표를 쓰지 않는다. `mir_json_dump.c:113`은 `"None"`을
  직접 비교한다.
- native semantic은 이미 subject 타입으로 먼저 분기한다
  (`type_checker_flow_match.c:224`: Option :241, Result :279, 사용자 enum :321).
  그런데 이 결정을 기록하는 fact가 없다. `match_binding_type_fact.h:17-25`에는 바인딩
  타입만 있다.
- MIR match arm에는 subject 타입 필드가 없다(`mir_types.h:169-174`). JSON에는
  `match_variant`, `match_bindings`, `match_binding_types`만 나간다(`mir_json_dump.c:95-146`).
- 그 결과 소비자가 이름으로 분기한다. 네이티브 C MIR는 사용자 enum `Ok(p)`에서 실패하고,
  self-host는 `Unwrap(v)`를 렌더링한다(`mir_lower/match_binding_render_owner.pgy:62-71`).
  2026-09-22에 네 경로를 직접 돌려 보면 올바른 프로그램이 모두 거부된다. native C는
  payload 타입을 유도하지 못한다고 거부하고, native LLVM은 **내부 verify 오류**
  (`ret %"Verdict$Ok"` 대 `i32`)로 죽고, self-host 기본 경로 C/LLVM은
  `ast_artifact_invalid`(`match_binding_environment`)로 거부한다.
  같은 이름 분기가 `structured_condition_emission_owner.pgy:56-68`,
  `expression_graph_match_owner.pgy:38-54`, `mir/routine_match_owner.pgy:58-70`,
  `semantic/ast_match_binding_environment_owner.pgy:121-127`,
  `codegen/emission/option_match_owner.pgy:45-70`에도 있다.
- 이름 해석 쪽에도 결함이 둘 있다.
  - 서로 다른 enum이 같은 variant 이름을 선언하면 **먼저 선언된 쪽이 진단 없이 이긴다**
    (`type_checker_program.c:302`, `scope_lookup_current`가 이미 있으면 `continue`).
  - `Verdict.Bad(..)`는 `Verdict_Bad`를 찾지 못하면 bare `Bad`로 돌아간다
    (`type_checker_expr_call.c:282-288`). 한정자가 그 variant를 가졌는지 확인하지 않는다.

### 2.2 결정: 이름을 예약하지 않고 fact로 간다

이름 예약(`Some`/`None`/`Ok`/`Err`를 사용자 variant로 금지)은 가장 작은 수정이지만,
언어가 커지면 막다른 규칙이 된다.

- 사용자 enum과 제네릭 enum이 늘면, 이름으로 의미를 고르는 코드가 다른 이름으로 같은
  문제를 다시 만든다. 위 2.1의 결함 둘도 이름 기반 해석에서 나왔다.
- Option/Result를 나중에 일반 stdlib enum으로 옮기면, 그 variant 이름은 평범한 variant
  이름이 된다. 예약 규칙은 그때 다시 풀어야 한다.
- 이 저장소의 fact-owner 원칙과 맞는 쪽은 fact다. semantic은 이미 답을 알고 있다.

### 2.3 규칙

1. **패턴은 subject 타입 안에서 해석한다.** `case Ok(p)`는 subject가 `Result`면
   Result의 `Ok`, `Verdict`면 Verdict의 `Ok`다. 패턴 위치에는 모호성이 없다.
2. **한정 생성식 `E.V(..)`는 E가 V를 가졌는지 검사한다.** bare로 돌아가는 경로를 없앤다.
3. **비한정 생성식 `V(..)`**
   - 문맥 기대 타입(`let x: Verdict = Ok(1)`, 인자, 반환)이 있으면 그 타입의 variant로 해석한다.
   - 기대 타입이 없으면, 그 이름을 선언한 후보가 하나일 때만 해석한다.
   - 후보가 둘 이상이면 모호성 오류로 거부하고 한정(`Verdict.Ok(1)`)을 요구한다. 내장
     `Ok`와 사용자 `Ok`가 겹치는 경우도 여기에 포함된다.
4. **서로 다른 enum의 같은 variant 이름은 허용한다.** 조용한 first-wins는 없앤다.
   비한정 사용이 모호하면 3번 규칙으로 거부한다.

### 2.4 fact 모양

- semantic의 match 결정 결과를 **case 단위의 새 fact**로 기록한다:
  `subject_family ∈ {option, result, enum}`, 그리고 enum일 때 `subject_enum` 이름.
  `PgyMatchBindingTypeFact`에 넣지 않는 이유: 그 fact는 바인딩마다 한 행이라
  바인딩이 없는 case(`case None:`, `case Red:`)에는 행이 없다. 그런데 조건 방출
  (`IsSome`/태그 비교)은 그런 case에서도 계열을 알아야 한다.
- HIR routine fact → `mir_capture_match_case_facts`(`mir_branch_source_facts.c:1200-1239`)
  → MIR JSON으로 흘린다. JSON 필드는 `match_binding_types` **뒤에** 넣는다
  (`"match_subject_family":"enum","match_subject_enum":"Verdict"`).
  기존 연속 문자열 핀(`mir_json_parity.sh:507-538`, `driver_rung2_match_parity_owner.sh:32-47`)은
  `match_binding_types`에서 끝나므로 그대로 둘 수 있다.
- self-host MIR producer(`mir/json_projection_owner.pgy:136-141`,
  `mir/instruction_json_artifact_writer_owner.pgy:150`)도 같은 키를 낸다.

### 2.5 단계와 게이트

| 단계 | 내용 | 동작 게이트 |
| --- | --- | --- |
| L3a | 같은 variant 이름의 조용한 first-wins를 진단으로 바꾸고, 한정자 소유 검사를 넣는다 (native + self-host) | 두 enum이 같은 variant를 선언한 프로그램, `Verdict.Bad`가 아닌 한정자 사용이 모두 산출물 없이 거부된다 |
| L3b | subject 타입 계열 fact를 native와 self-host MIR JSON에 싣는다 | Option, Result, 사용자 enum `Ok` 세 프로그램에서 필드 값이 정확하다. 필드가 없거나 틀린 MIR은 소비자가 거부한다 |
| L3c | 소비자를 fact 기반으로 바꾼다. 틀린 AST를 만드는 self-host `mir_lower`가 먼저다. 그다음 C/LLVM MIR 순서로 바꾼다 | `enum Verdict { Ok(Int), Bad(String) }` 재현이 C, LLVM, self-host 기본 경로에서 모두 정답을 출력한다. 이름 분기로 되돌리는 변경은 음성 게이트가 잡는다 |
| L3d | 비한정 생성식의 문맥 해석과 모호성 진단 | 기대 타입이 있는 `Ok(1)`은 사용자 enum으로 실행되고, 기대 타입이 없는 모호한 `Ok(1)`은 거부된다 |

`perf_contract_smoke.sh:960-989`(match 파일의 lookup 사용 핀)와
`build_source_inventory_smoke.sh:362-383`(정책 파일 밖의 variant 문자열 금지)은 L3c에서
같이 갱신한다.

### 2.6 착지 상태

- **L3b·L3c (native, mir_lower) 착지.** semantic이 case마다 `PgyMatchSubjectFamily`를
  기록하고(`type_check_special_match_pattern`), MIR 명령어와 MIR JSON
  `match_subject_family`(match case에만)로 흘린다. C/LLVM의 MIR·AST match 방출기와
  mir_lower의 렌더러·조건·expression graph가 이 fact로 wrapper 여부를 정한다.
  `enum Verdict { Ok(Int), Bad(String) }`와 `enum Maybe { Some(Int), None }`이 native C와
  LLVM에서 정답을 출력한다. 게이트는
  `tests/self_hosted/parity/match_subject_family_owner.sh`(push CI core shard)다.
- 계열이 없는 case(semantic 이후 컴파일러가 합성한 match)는 기존 철자 규칙을 쓴다.
- **self-host 기본 경로는 아직 이 프로그램들을 거부한다.** self-host semantic과 MIR producer는
  대상 타입으로 계열을 먼저 정하도록 바뀌었지만, self-host에는 한정 payload 생성식
  (`Verdict.Ok(7)`, `Shape.Circle(2)`)이 없다. 내장 이름과 겹치는 variant는 한정해야만
  만들 수 있으므로 이것이 L3d의 첫 항목이다. 게이트는 지금의 거부(산출물 없음)를
  고정하고, 생성식이 들어오면 실행 확인으로 바뀐다.
- **L3d 한정 생성식 착지(기본 C 경로).** 함수 표가 한정 variant 행을 `Shape.Circle`로
  등록했지만 호출 조회는 `.`을 `_`로 바꿔 찾았으므로 자기 행을 찾지 못했다. 행을 C 생성자
  심볼과 같은 `Shape_Circle`로 등록하자 Namespace 호출로 해석되고, C codegen은 기존
  namespace 행으로 방출한다. payload provenance 검사도 Namespace 대상을 받는다.
  `Shape.Circle(2)`는 기본 C 경로에서 실행된다. 기본 LLVM 경로(direct MIR)는
  `payload-enum-zero-constructor` admission에서 거부한다.
- **L3d 계열 fact 착지.** self-host MIR producer도 match case 행에 `match_subject_family`를
  싣는다(`SelfMirMatchSubjectFamily`, 두 JSON writer 모두 native처럼 match case 행에만).
  그래서 mir_lower가 `case Ok(p)`를 철자가 아니라 계열로 읽고, `Verdict.Ok(7)`/
  `Maybe.Some(4)` 프로그램이 기본 C 경로에서 native와 같은 값을 출력한다. 기본 LLVM
  경로는 한정 생성식을 여전히 거부한다.
- **L3a 착지.** native는 variant 이름이 같은 범위의 다른 선언(다른 enum의 variant, 함수)과
  겹치면 `PGY_SEM_REDECLARATION`으로 거부하고, `Enum.Variant(..)`의 이름 폴백이 다른
  enum의 variant에 닿으면 거부한다. self-host는 두 enum의 같은 variant를
  `enum_variant_redeclaration`으로 거부한다(`ast_declaration_contract_owner.pgy`).
  게이트는 `tests/self_hosted/parity/enum_variant_identity_owner.sh`다. 서로 다른 enum의 같은
  variant 이름을 한정해서 허용하는 규칙 4는 L3d의 문맥 해석과 함께 들어온다.
- 확인 중 드러난 기존 결함(이 변경과 무관, L3가 없는 빌드에서도 같다):
  - **고침.** `Result<Int, String>`이 native C에서 `PgyResult_Int_String` 미선언으로
    컴파일되지 않았다. C 특수화 등록기가 `Int/Bool/String` + `String` 오류를 런타임에 이미
    있는 타입으로 보고 건너뛰었지만, 런타임은 인자 하나짜리 `PgyResult_Int` 등만 정의한다.
    두 인자 Result는 이제 모두 특수화된다. self-host C 경로는 이 프로그램을 이미 정답으로
    실행한다(2026-09-22 재확인). 게이트는 match subject family 게이트의 `result-control` 행이다.
  - Option match는 self-host LLVM direct MIR에서 `enum-match` admission으로 거부된다.

## 3. F2 — 컴파일러 임시 이름의 위생

- destructure 임시 변수는 `_pgy_destructure_<첫 바인딩>`이다. 사용자가 같은 이름을 선언하는
  것을 막지 않는다.
- **리뷰 문서의 제안(접두사 예약)은 채택하지 않는다.** 저장소는 이미 반대 결정을 했다.
  `tests/concept_semantics/nominal/named_record_identifier_hygiene_valid.pgy`는 사용자 변수
  `_pgy_record_value_1`을 **정상 프로그램**으로 고정하고(`source_admission_parity.sh`),
  native C는 레코드 임시 변수를 충돌 검사 gensym으로 만든다
  (`transpiler_class_constructor_emit.c`: "source identifiers can use compiler-looking
  prefixes. This is C name hygiene"). 예약은 이 결정을 뒤집고 기존 프로그램을 거부하게 된다.
- 규칙: 컴파일러가 만드는 임시 이름은 그 routine의 선언 이름과 겹치지 않게 고른다.
  mir_lower의 destructure 임시 이름도 routine의 지역 변수 목록에 있으면 번호를 붙여 피한다.
  `stmt_render.pgy`와 `destructure_expression_projection_owner.pgy`가 같은 이름을 써야
  하므로(F1), 이름을 고르는 함수 하나를 두 owner가 함께 호출한다.
- 게이트: 사용자가 `_pgy_destructure_<첫 바인딩>`을 이미 선언한 프로그램을 mir_lower가
  재구성하고, 그 결과가 native와 같은 값을 출력한다.
- **F2 착지.** `mir_lower/destructure_temporary_owner.pgy`가 이름을 고른다. 그 routine의
  매개변수, `source_locals`, 프로그램의 routine 이름에 없을 때까지 `_1`, `_2`를 붙인다.
  두 목록 중 하나라도 없거나 두 번 있으면 이름을 정하지 않고 거부한다. 렌더러와
  expression graph projection이 이 owner를 함께 부른다. 재현에서 사용자 지역 변수
  `_pgy_destructure_first`는 기본 C 경로에서 `Array<String>`으로 덮여 컴파일되지 않았다.
  게이트는 `tests/self_hosted/parity/destructure_temporary_hygiene_owner.sh`(native C/LLVM,
  기본 C 경로. 기본 LLVM 경로는 지금 모든 destructure를 거부한다)와 렌더러 스모크의
  numbered-temporary 행이다.

## 4. L1 — `Never` 반환 타입

### 4.1 현재

- `Void`는 키워드가 아니라 builtin 타입 이름이다(`parser_type.c:394`, `type_system.c:231`).
  semantic에는 bottom 타입 종류가 없다(`type_system.h:16-26`).
- 경로를 끝내는 문장은 `return`, `break`, `continue`뿐이다. `Exit(...)`를 포함한 식 문장은
  `FLOW_FALLTHROUGH`다(`type_checker_flow.c:399-401`). 그래서 `-> String` 함수가
  `Die(...)`로 끝나도 뒤에 `return`이 필요하다.
- self-host semantic에는 모든 경로가 값을 반환하는지 보는 검사가 없다. L1c에서 이 공백을
  먼저 확인하고 메운다. 그 전까지 이 점을 결함으로 단정하지 않는다.
- self-host에서 `Exit`는 전용 문장 종류(`TypedAstKindExitStmtTag` 17)가 된다.
  self-host MIR은 여기서 routine을 끝낸다(`mir/routine_statement_owner.pgy:91-93`).
  사용자 실패 헬퍼 호출에는 그런 처리가 없다.
- MIR에는 unreachable terminator가 없다(`mir_types.h:81-93`, 후속자는 암묵적).
  HIR에는 `HIR_BLOCK_UNREACHABLE`이 있다.
- 백엔드는 이름으로 추측한다. LLVM `llvm_fn_never_returns`(`llvm_runtime_attrs.c:196-213`)는
  이름에 `panic`이 들어가거나 `pgy_exit`면 noreturn으로 본다. C의 `PGY_RUNTIME_NORETURN`은
  `pgy_runtime_panic_emit`에만 붙어 있고, `pgy_exit`에는 없다.
- 실패 헬퍼는 `Die`(호출 1,965곳, 451 파일), `MirLowerFailClosed`(198곳),
  `LspLiveSessionFail`, `ParserProgramGraphFail`이 있고, 맨 `Exit(1)`이 559곳 있다.
  바로 뒤에 죽은 `return`이 붙은 곳은 대략 37곳이다.

### 4.2 규칙

1. `-> Never` 함수는 fallthrough와 `return`이 모두 금지다. 모든 경로가 Never 호출로 끝나야 한다.
2. Never 호출 **문장**은 경로를 끝낸다. 새 flow 플래그(`FLOW_DIVERGE`)가 missing-return을
   충족시키고, 그 뒤 문장에는 기존 unreachable 경고가 붙는다.
3. 첫 단계에서는 값 위치의 Never 호출(`let x: Int = Die("..")`)을 거부한다. 모든 타입으로
   바뀌는 bottom 변환은 나중에 따로 정한다.
4. builtin `Exit`의 반환 타입을 `Never`로 바꾼다
   (`pgy_builtin_type_table.c:70`, self-host `builtin_signature_owner.pgy:70`).

### 4.3 단계

| 단계 | 내용 |
| --- | --- |
| L1a | native semantic: `Never` 타입, 규칙 1–4, 진단 |
| L1b | MIR: Never 호출 뒤 블록을 명시적으로 끝내는 fact(JSON 포함). C: 선언에 `PGY_RUNTIME_NORETURN`, 호출 뒤 `__builtin_unreachable()`. LLVM: 선언에 `noreturn`, 호출 뒤 `unreachable`. 이름 추측(`llvm_fn_never_returns`)을 fact로 바꾼다 |
| L1c | self-host semantic: missing-return 검사 확인·보강, 그다음 Never 규칙 |
| L1d | self-host codegen과 direct MIR의 noreturn 방출 |
| L1e | 이관: `Die`, `MirLowerFailClosed`, `LspLiveSessionFail`, `ParserProgramGraphFail`을 `-> Never`로 바꾸고 죽은 `return`을 지운다. gen2==gen3 fixpoint 유지 |

게이트: Never 함수 안의 `return`과 fallthrough가 거부된다. Never 호출로 끝나는 non-Void
함수가 missing-return 없이 컴파일되고, 실행하면 호출 지점에서 종료 코드가 나온다
(C, LLVM, self-host 모두).

**L1a·L1c·L1d 착지.**

- native: `Never` primitive, Never 식 문장의 경로 종료, Never 함수 안 `return` 거부,
  값 위치 거부. `Exit`의 결과 타입이 Never다. 확인 중에 `Exit`가 native semantic에서
  **한 번도 타입 검사되지 않았다**는 것이 드러났다. `type_check_builtin_call`의 switch에
  `BUILTIN_EXIT`가 없어 `Unknown`으로 빠졌고, 인자도 검사하지 않았다. 이제 Int 인자를
  요구한다. C/LLVM 타입 매핑은 `Never`를 void로 내린다. HIR은 이미 non-Void 함수의
  끝을 도달 불가로 표시하므로 Never 함수의 끝도 그렇게 처리된다.
- self-host: C 방출과 direct MIR의 routine 목록이 `Never`를 Void로 읽는다. self-host에는
  missing-return 분석이 없어서, `ast_never_function_verdict_owner.pgy`가 보수적으로
  검사한다. 마지막 최상위 문장이 `Exit`이거나 다른 Never 함수 호출이어야 하고, 아니면
  `never_function_fallthrough`로 거부한다. 모든 분기가 Never로 끝나는 본문은 native는
  받고 self-host는 거부한다. 틀린 쪽으로 받아들이지 않도록 고른 차이다.
- 게이트: `tests/self_hosted/parity/never_return_type_owner.sh`(push CI core shard).
- **L1b (LLVM) 착지.** 확인 중에 이름 추측이 오컴파일을 만든다는 것이 드러났다. LLVM 속성
  패스는 모듈의 **모든** 함수에 이름 표(`panic`을 포함하거나 `pgy_exit`)를 적용했다.
  그래서 `count_panics(n: Int) -> Int` 같은 사용자 함수가 noreturn이 되었고, native LLVM
  실행 파일은 호출 뒤 코드가 사라져 `machine-layer runtime bind rejected`로 중단되었다.
  이제 이름 표는 런타임 선언에만 적용하고, 사용자 함수는 선언된 `Never` 타입으로만
  `noreturn`을 얻는다(`llvm_decl.c`). 게이트는 Never 게이트의 `panic-name` 행(네 경로)이다.
- 남은 것: C의 `PGY_RUNTIME_NORETURN`(런타임 `pgy_exit` 선언부터 필요), 호출 뒤 명시적
  `unreachable`, L1e의 이관(`Die`, `MirLowerFailClosed` 등을 `-> Never`로 바꾸고 죽은
  문장을 지우는 일).

## 5. L2 — 문자열 조립

### 5.1 현재 (리뷰 문서의 전제 정정)

리뷰 문서는 "2항 `Concat`뿐"이라고 했지만, **문자열 보간은 두 컴파일러 모두에 이미 있다.**

- 형태: `"...${expr}..."`, `f"...{expr}..."`, `$"...{expr}..."`. 모두 `ToString(expr)` 조각을
  잇는 왼쪽으로 기운 `+` 체인이 된다(native `parser_expr.c:556-567`,
  `parser_expr_string.c:72-178`; self-host `parser/expr_string_owner.pgy`).
  self-host 소스에서도 74곳이 이미 쓴다.
- **escape가 두 컴파일러에서 다르다.** native는 홀수 개의 백슬래시가 앞에 붙은 opener
  (`\${`, `\{`)를 문자 그대로 둔다(`parser_expr_string.c:53-69`). self-host parser는 이
  검사를 하지 않는다(`expr_string_owner.pgy:42,107`). 같은 소스가 두 컴파일러에서 다른
  문자열이 될 수 있다.
- **보간 결과는 owned String으로 인정되지 않는다.** owned producer는 stdlib
  `Concat`/`StringConcat` 직접 호출뿐이다(`type_checker_ownership_call.c:330-349`). 그래서
  보간 결과는 `own String` 인자나 `Array<String>` 소유 이전에 쓸 수 없다.
- **중간 임시 문자열이 해제되지 않는다.** `docs/197_region_arena_strategy.md:92-98`의
  "measured leak"다. region 경로는 문자열 리터럴이 척추에 있는 `+` 체인이 호출 인자로
  빌려질 때만 적용된다.
- `Concat`의 인자 수는 모든 계층에서 2로 고정이다. self-host에는 `Concat` 호출이
  13,961개 있고, 그중 2,388개가 최상위 피라미드다(400 파일, 최대 깊이 17).

### 5.2 단계

| 단계 | 내용 | 동작 게이트 |
| --- | --- | --- |
| L2a (착지) | self-host parser의 opener escape를 native와 맞춘다 | `\${x}`, `\{x}`, `\\${x}`가 native와 self-host 기본 경로에서 같은 문자열을 출력한다 |
| L2b | 보간과 N항 `Concat(a, b, c, ...)`을 하나의 typed 연산(StringBuild)으로 낮춘다. semantic에서 owned producer로 인정한다. 런타임은 길이를 합산해 한 번만 할당한다 | 보간 결과를 `own String` 인자와 `ArrayPush`에 넘길 수 있다. ASan/누수 검사에서 중간 임시 할당이 없다. C와 LLVM 출력이 같다 |
| L2c | self-host 피라미드를 기계적으로 이관한다(owner 단위 배치). likeness 래칫에 Concat 중첩 깊이 지표를 추가해 역행을 막는다 | gen2==gen3 fixpoint, 드라이버 메모리와 빌드 시간 측정치 |

서식 지정자와 중첩 보간은 범위 밖이다(`docs/grammar/01_syntax.md:530-548`의 beta 범위 유지).

**L2a 착지.** 재현 결과는 예상보다 나빴다. `Log("a\${x}b")`는 native에서 `a${x}b`,
self-host 기본 경로에서 `5b`였다. opener 앞에서 잘린 리터럴 조각이 `\"`로 끝나 자기
닫는 따옴표를 escape했고, 그 앞의 텍스트가 사라졌다. self-host parser는 이제 현재 조각
안의 백슬래시 개수로 opener escape를 판단한다. direct MIR 문자열 리터럴 해석은 `\$`와
`\{`에서 백슬래시를 뗀다. 게이트는
`tests/self_hosted/parity/string_interpolation_escape_parity_owner.sh`(push CI core shard)다.

## 6. L4 — 한 칸 out 배열

### 6.1 현재

- 성공 여부 `Bool`과 out 배열 조합을 쓰는 함수가 약 95개(45 파일) 있다. 호출부의
  `[0]`/`[""]`/`[false]` 임시 배열이 159개, `[0, 0]` 범위 배열이 186개다.
  예: `lib/json.pgy:96` `ReadJsonStringBounded`(호출 24곳),
  `mir_lower/json_fact_read.pgy:206` `MirObjectArrayBoundsAtBounds`(참조 61곳),
  `parser/expr_precedence_owner.pgy:368` `ParseExprFact`(`cursor_out[0]` 읽기 226곳).
- 리뷰 문서는 튜플과 제네릭 Option 지원을 선행 조건으로 봤다. 그런데 self-host는 이미
  `Option<Struct>`를 반환한다(`mir_lower/machine_layer_fact_owner.pgy:313`,
  `lib/snapshot_ticket.pgy:24`). `Option<Array<..>>`와 튜플만 없다.

### 6.2 설계

튜플을 기다리지 않는다. 명명 struct를 담은 `Option`으로 옮긴다.

- 범위: `struct JsonSpan { start: Int; end: Int; }`, `-> Option<JsonSpan>`
- 값과 끝 위치: `struct JsonStringRead { value: String; end: Int; }`
- 배열 결과: `struct JsonStringList { values: Array<String>; }`. Array 필드를 가진 struct가
  Option payload로 C/LLVM과 소유권 규칙을 통과하는지 L4a 첫 커밋에서 먼저 확인한다.

단계: L4a `lib/json*` → L4b `mir_lower/json_fact_read.pgy` → L4c parser `cursor_out`(가장 큼).
각 단계마다 gen2==gen3 fixpoint를 유지하고, 드라이버 메모리를 측정한다. 반환 struct 복사가
메모리 벽(38.5GB 사례)을 되살리지 않는지 확인한다.

**L4a 첫 칸 착지 (문자열 읽기).** `ReadJsonStringBounded(json, open, limit, end)`는 실패를
빈 문자열로 돌려주고 `end[0]`을 건드리지 않아서, 호출부마다 `end[0]`을 다시 비교해야
실패와 빈 문자열을 구분할 수 있었다. 이제 `JsonReadStringBounded(json, open, limit) ->
Option<JsonStringRead>`(`value`, `end`)가 실패를 None으로 돌려주고, 정확히 `[start, end)`를
차지하는 리터럴은 `JsonStringValueSpanning(json, start, end) -> Option<String>`으로 읽는다.
`lib/json*`, `mir_lower`, direct MIR 문자열 리터럴의 호출부 24곳이 옮겨 갔다.
`mir_lower/routine_instruction_scalar_capture_owner.pgy`의 2곳은 공유 작업 트리에 다른
레인의 미커밋 수정이 겹쳐 있어 옛 형태를 쓰고, 옛 함수는 그 호출부만을 위한 얇은 형태로
남았다. 그 파일이 커밋되면 지운다. 범위 배열(`[0, 0]`)과 parser `cursor_out`은 다음 칸이다.

## 7. F1, F3, F4, F5 — mir_lower 공백

- **F3 (착지)**: 리뷰 문서는 남은 호출부를 3곳으로 적었지만, 실제로는 9개 파일에
  12곳이었다. 호출부를 하나씩 옮기는 대신 `MirObjectArrayStringFactsAtBounds` 자체를
  엄격하게 바꿨다. 필드가 없으면 빈 배열이고, 있는데 문자열이 아닌 원소가 있으면
  `MIR string array fact is malformed: <field>`로 멈춘다. 한 번 훑는 구현이다.
  렌더러 두 곳이 쓰는 `JsonArrayStringsWithin`은 여전히 원소 수의 제곱 비용이다.
  두 렌더러의 배열은 바인딩 이름처럼 짧아서 남겨 두었다. 게이트는
  `tests/self_hosted/mir_lower_stmt_render_fail_closed_smoke.sh`의 `non-string-use` 행이다.
- **F1**: destructure 초기식을 한 번만 평가해야 하는지를 C MIR가 fact로 싣는다(초기식
  graph의 루트가 지역 식별자 leaf가 아니면 참). `stmt_render.pgy`와
  `destructure_expression_projection_owner.pgy`가 같은 fact를 읽는다. 텍스트 휴리스틱
  `MirDestructureNeedsTemp`는 지운다.
- **F1 (착지)**: 새 MIR 키를 두지 않았다. native와 self-host MIR producer가 이미 싣는
  `expr0_graph`의 루트 노드 종류가 그 fact다. 루트가 leaf면 제자리에서 읽고, 아니면
  임시 변수에 한 번 담는다. 그래프를 읽을 수 없으면 거부한다(렌더러 스모크의
  `rootless-graph` 행). 식별자 leaf는 두 번 읽어도 값이 같으므로 지역 변수인지는
  따로 보지 않는다. 필드 접근 같은 초기식은 이전 휴리스틱과 달리 임시 변수를 얻는다.
- **F4 (착지)**: 닫힌 목록을 측정으로 정했다. mir_lower 픽스처와 backend_compare 케이스
  1,049개를 계측한 mir_lower에 통과시키니, 문장 텍스트 없이 이 분기에 닿는 행은 `phi`
  (1,131행), `cleanup`(83행), `cleanup`/`AST_BLOCK`(23행)뿐이었다. 구조적 `stmt`는 없었다.
  `routine_lower`는 이제 그 둘 밖의 행을 `instruction has no statement text: <kind>/<source>`로
  거부한다. 게이트는 렌더러 fail-closed 스모크의 `textless-stmt` 행이다.
- **F5**: F4 뒤에 defer 본문을 여러 문장으로 재구성한다(`Log`, 직접 호출, 대입).
- **F5 착지 (`Log`와 직접 호출).** self-host MIR producer는 본문 문장마다
  `stmt`/`AST_DEFER_STMT` 행을 소스 순서로 내고, 문장이 n개면 `arg1`에 `1/n`..`n/n`을
  싣는다(한 문장이면 빈 값). defer를 n개로 쪼개면 역순으로 실행되므로 mir_lower는 연속한
  행을 하나의 `Defer` 블록으로 다시 묶는다. 순서가 어긋나거나, 빠지거나, 형식이 틀린 part는
  거부한다. 게이트는 `tests/self_hosted/parity/defer_multi_statement_owner.sh`(native C/LLVM,
  기본 C 경로)와 렌더러 스모크의 part 행이다. 기본 LLVM 경로는 defer 자체를 거부한다.
- **F5 착지 (native MIR 행).** native MIR도 본문 문장마다 `stmt`/`AST_DEFER_STMT` 행을 낸다.
  각 행은 자기 part 인덱스와 routing(`Log`/`Call`/`Assign`)을 싣고, 문장이 둘 이상이면
  `arg1`에 `k/n`을 싣는다. 표현식 graph의 lane도 그 문장을 따른다(대입은 값이 lane 0,
  대상이 lane 1). MIR 경로의 C/LLVM 백엔드는 본문의 첫 행에서만 defer 블록을 등록하므로
  n개 행이어도 본문은 한 번만 돈다. 그래서 여러 문장 defer 픽스처를 rung2 MIR parity
  매니페스트에 넣을 수 있게 됐고(픽스처 285개), native oracle과 self-host MIR이 같은
  canonical 형태로 모인다.
- **F5 착지 (대입).** 대입 문장은 `arg0=Assign` 행이 된다. 일반 대입 행처럼 값 graph가
  주 graph이고 대상 graph가 보조 graph(`expr1_graph`)다. mir_lower는
  `Assign: <대상> = <값>`으로 재구성하고, 대상 graph가 없으면
  `defer assignment target graph is missing`으로 거부한다. defer 행에는 binding mode를 실을
  자리가 없어서, 기본 경로는 대상이 지역 변수(`local`)인 대입만 받는다. inout 매개변수나
  owner field 대상은 `defer assignment target is not a local binding`으로 거부하고,
  mir_lower의 binding mode 검사도 defer 대입 행을 semantic 대입 fact와 짝지어 `local`인지
  본다. native C/LLVM은 매개변수 대상도 실행한다. `Log`, 직접 호출, 대입 밖의
  문장(if, while 등)은 기본 경로에서 행을 하나도 내기 전에
  `defer body statement is outside the Log, direct-call and assignment rung`으로
  거부된다. 게이트는 같은 parity owner의 대입 픽스처(세 경로)와 제어문 음성 픽스처다.

## 8. 순서

1. L3a → L3b → L3c: 틀린 AST를 없애는 정확성 결함이 먼저다.
2. L2a: 두 컴파일러의 문자열 불일치(정확성 결함).
3. F2: 임시 이름 위생. mir_lower 안에서 끝난다.
4. L1a–L1e: Never. 실패 헬퍼 정리의 전제다.
5. F3 → F1 → F4 → F5.
6. L2b → L2c, L3d, L4a–L4c: DX 이관. 기계적이지만 양이 크다.

native parser/semantic 파일과 self-host `mir_lower` 파일은 다른 세션이 미커밋으로 고치고
있는 경우가 많다. 겹치는 단계는 별도 worktree에서 개발하고, 그 파일이 커밋된 뒤 합친다.

## 9. 사용자 결정이 필요한 것

- Never를 값 위치에서 모든 타입으로 받아들일지(bottom 변환). 첫 단계는 문장 위치만 허용한다.
- 비한정 variant의 모호성을 오류로 할지 경고로 할지. 이 문서는 오류를 제안한다.
- N항 `Concat`을 공개 표면에 둘지, 보간만 강화할지. 이 문서는 둘 다 같은 연산으로 낮추는
  것을 제안한다.

## 10. R7 — 빌트인 철자와 프로그램 함수

### 10.1 현재 (착지 전)

- native는 이름 해석에서 빌트인을 사용자 함수보다 먼저 골랐다. `func Max(a, b)`를 선언해도
  `Max(2, 3)`은 빌트인이 됐고, `MapKeys`·`IsCancelled`·`Clone`·`Contains`·`Trim`도 같았다.
  진단은 없었다(SILENT-WRONG).
- 기본 경로(self-host)는 함수 표에서 빌트인 행과 시그니처가 같으면 사용자 함수를 부르고,
  다르면 `ast_artifact_invalid`로 거부했다. 같은 프로그램이 두 컴파일러에서 다르게 돌았다.
- 지역 값과 사용자 class는 이미 빌트인 철자를 가린다. 함수만 예외였다.

### 10.2 결정: 가리기가 규칙이고, 예약은 철자가 호출 이상의 뜻을 가질 때만이다

- **top-level 함수가 빌트인·stdlib 철자를 가린다.** 새 빌트인이 생겨도 그 이름을 쓰던
  프로그램이 깨지지 않는다. native semantic이 결정하고 호출 노드에
  `semantic_callee_declared_callable` fact를 싣는다. C·LLVM emitter, 두 백엔드의 호출 타입
  추론, MIR의 부작용 판정(`mir_source_shape.c`), C `let` Box 방출은 이 fact를 읽고 이름으로
  다시 고르지 않는다. self-host는 함수 표가 그 함수가 가져간 빌트인 행을 싣지 않는다
  (`semantic/builtin_shadow_owner.pgy`).
- **예약 가족.** 아래 철자는 top-level 함수가 쓸 수 없고, 두 컴파일러가 선언에서 거부한다
  (native `PGY_SEM_REDECLARATION` / `semantic:function:builtin_name_reserved`, self-host
  `builtin_name_reserved`). 행은 `src/semantic/builtin_name_reservation.def`가 소유하고
  self-host 행은 그 투영이다.
  - `CAPABILITY`: ambient capability 빌트인. 행은 `builtin_capability_registry.def` 그 자체다.
    `ReadFile(...)`을 읽는 사람은 그것이 권한 게이트가 걸린 연산이라고 믿을 수 있어야 한다.
  - `RUNTIME_ABI`: 런타임이 같은 철자의 C/LLVM 심볼을 선언한다. native C는 사용자 함수 이름을
    그대로 내므로 충돌한다. 게이트가 행을 런타임 선언과 대조한다.
  - `RESOURCE`: slot·device slot·view·move·channel 연산. 자원 패스(MIR/RIR)와 LLVM의 source-local
    channel let이 이 철자로 수명을 추적한다.
  - `STATEMENT_FORM`: self-host typed AST가 이 callee의 호출 문장에 전용 statement kind를 준다
    (`hir/ast_node_kind_owner.pgy`). 문장 위치 `Log(r);`가 사용자 함수 대신 빌트인이 됐다.
  - `CONSTRUCTOR`: `Some`/`None`/`Ok`/`Err`. 같은 이름의 함수는 생성식과 모호하다(2.3의 3번 규칙).
  - `TYPED_PROTOCOL`: self-host semantic이 이름 해석 전에 빌트인 프로토콜로 타입을 매긴다
    (collection 프로토콜, Option/Result 투영, `Max`/`Min`, domain query). **이관 부채**다. 그
    owner들이 declared-callable fact를 읽으면 행이 이 가족을 떠나고, 떠난 이름은 가리기 쪽이
    된다. 예약을 푸는 것은 호환되는 완화다.
- 나머지 이름은 `tests/self_hosted/fixtures/builtin_name_shadow_names.txt`에 있다.

### 10.3 게이트

`tests/self_hosted/parity/builtin_name_shadow_owner.sh`:

- 두 예약 표가 같은지, `RUNTIME_ABI` 행이 런타임 선언인지, 가리기 목록의 이름이 런타임 선언이
  아닌지, 빌트인 표(`builtin_resolve`, `pgy_builtin_type_table.c`, self-host 시그니처 행)의 모든
  이름이 두 부류 중 하나에 들어가는지 본다. 새 빌트인은 분류되기 전까지 게이트를 통과하지 못한다.
- 가리기 목록의 모든 이름으로 함수를 선언한 한 프로그램을 native C/LLVM과 기본 C/LLVM 경로에서
  돌리고, 식 위치와 문장 위치의 모든 호출이 프로그램 함수를 실행했는지 출력으로 확인한다.
- 예약 가족마다 한 이름이 네 경로 모두에서 바이너리 없이 거부되는지 본다.

### 10.4 착지하면서 드러난 것

- 문장 위치 호출을 넣자 native가 `ChannelCapacity`·`HasZone` 계열 이름의 사용자 함수 호출
  문장을 "순수 질의"로 보고 DCE로 지우는 것이 드러났다. 이름 표(`mir_source_call_is_pure_query`)
  앞에서 fact를 먼저 본다.
- 기존 픽스처 셋이 예약 이름을 함수로 선언하고 있었다. `backend_compare/max_reduction_slot`과
  `tests/alpha_full_keyword_test.pgy`의 `Max`는 `Larger`로, semantic 단위 테스트의 top-level
  `Read`는 `Inspect`로 바꿨다. `tests/concept_semantics/hashmap/shadowed_map_keys_drop.pgy`
  (`MapKeys`)는 이제 선언에서 거부된다.
- 남은 것(10.5에서 다룸): method의 bare 호출, 빌트인 철자가 아닌 이름과 런타임 심볼의 충돌.

### 10.5 host method와 런타임 심볼

- **host method도 빌트인 철자를 가린다.** method 본문 안의 bare 호출 `Clone(1)`은 native에서
  host의 `Clone` method가 아니라 빌트인이 됐다. 코퍼스에서 `Abs`, `Clone`, `ToString`, `Trim`을
  포함해 30개 이름이 틀린 값을 냈다. 이제 host의 method가 더 가까운 scope이므로 먼저 잡힌다.
  예약 철자는 bare 호출에서 언제나 빌트인이고, 같은 이름의 method는 `self.Name(..)`으로 부른다.
  fact 이름은 `semantic_callee_declared_callable`(top-level 함수 또는 host method)로 바꿨다.
  기본 경로는 bare method 호출 자체를 아직 해석하지 못한다(`call_arity_mismatch`). 이 차이는
  빌트인 이름과 무관한 기존 공백이라 게이트는 native C/LLVM만 본다.
- **런타임 심볼.** 런타임 라이브러리 object가 export하는 PascalCase 심볼 42개를 `nm`으로 뽑았다.
  모두 `RUNTIME_ABI` 또는 `CAPABILITY` 가족에 이미 들어 있다. 빌트인이 아닌 이름(`SlotRead` 등)은
  네 경로 모두에서 컴파일되고 돈다.

## 11. 호출 인자 평가 순서, `ToInt`, 산출물 게시

### 11.1 인자 평가 순서

- 사용자 함수 호출은 native C에서 이미 왼쪽부터 평가됐다. 빌트인·stdlib 호출(`Concat`,
  `Substring` 등)과 기본 C 경로의 모든 호출은 C의 순서(GCC는 오른쪽부터)를 따랐다. LLVM 두
  경로는 왼쪽부터였다.
- native C: 인자가 둘 이상이고 그중 하나가 literal이나 지역 읽기보다 큰 빌트인 호출은, 효과를
  내거나 관찰할 수 있는 인자를 소스 순서대로 임시 변수에 묶는다. 빌트인 emitter는
  `emit_expression`을 거쳐 그 임시 변수를 읽는다. 어떤 인자를 임시 변수로 읽지 않은 emitter는
  두 번 평가할 수 있으므로, 조용히 순서를 되돌리지 않고 컴파일을 거부한다. 저장소 인자
  (컬렉션, slot, builder)는 주소를 쓰므로 제자리에 둔다.
- 기본 C 경로: `codegen/emission/call_argument_order_owner.pgy`가 같은 규칙으로 by-value 인자를
  statement expression 안의 임시 변수에 묶는다. inout·ref·identity 인자는 주소라 제자리에 둔다.
  이항 연산자 피연산자는 이미 같은 방식이었다.
- 게이트: `tests/call_argument_evaluation_order_smoke.sh`가 네 경로에서 사용자·빌트인 호출을 보고,
  native C/LLVM과 기본 C 경로에서 method 호출과 `ArraySet`을 본다. 기본 LLVM 경로는 method
  픽스처의 모양을 아직 받지 않고 거부한다. 틀린 순서로 도는 경로는 없다.
- receiver: native C는 호출식 receiver를 인자보다 먼저 임시값에 묶는다
  (`tests/cases/backend_compare/method_receiver_before_arguments`). 전에는 receiver가 호출 안에
  남아 인자보다 늦게 평가됐다: `Make("r").Pair(Tag("a"), Tag("b"))`가 C에서 `a b r`,
  LLVM에서 `r a b`였고, 인자가 하나일 때도 갈렸다. 바인딩이나 그 멤버인 receiver는 효과가 없고
  주소를 넘길 수 있어야 하므로 제자리에 둔다.
- 남은 것: 기본 C 경로(self-host)는 아직 호출식 receiver를 인자보다 늦게 평가한다(`a b r`).
  그 방출 owner는 메인 체크아웃에서 커밋 전 작업이 진행 중이라 이번에 고치지 않았다.
  인덱스 식 receiver(`xs[F()].M(...)`)와 동적 ability 호출(vtable)도 이 경로를 타지 않는다.

### 11.2 `ToInt`/`ToFloat`는 문자열만 받는다

런타임의 `ToInt`/`ToFloat`는 인자를 C 문자열로 읽는다. 두 컴파일러가 인자 타입을 보지 않아서
`ToInt(1)`이 컴파일되고 실행 중에 죽었다(native C, 기본 C). 이제 두 컴파일러 모두 String 인자만
받는다. 숫자 변환은 `value as Int`다. 게이트는
`tests/self_hosted/parity/text_conversion_argument_owner.sh`다. 문자열 변환은 native C/LLVM과
기본 C 경로에서 돌고, 숫자 인자는 두 front end 모두 바이너리 없이 거부한다.

### 11.3 산출물 게시

리뷰가 지적한 대로 기존 동작은 "먼저 지우기"였다. 이제 네 경로 모두 바이너리를 목적지와 같은
디렉터리의 staging 파일(`<name>.pgy-staging-<pid><ext>`)에 만들고, toolchain이 성공한 뒤에만
한 번의 rename으로 목적지에 게시한다(Windows는 `MoveFileEx(MOVEFILE_REPLACE_EXISTING)`).
실패하면 staging 파일을 지우므로 새 바이너리는 나타나지 않는다. 옛 바이너리는 여전히 compile
시작 전에 지운다(stale 실행 방지). `tests/self_hosted/parity/binary_output_refusal_owner.sh`가
거부와 성공 뒤에 staging 파일이 남지 않는지 본다.

## 12. 남은 일과 측정된 규모 (2026-09-23)

이 표는 이 문서의 항목 중 아직 닫히지 않은 것과, 착지하면서 새로 드러난 공백을 규모와 함께
적는다. 숫자는 2026-09-23 트리에서 센 값이다.

| 항목 | 측정된 규모 | 판단 |
| --- | --- | --- |
| F5(b) native MIR JSON의 여러 문장 defer | **착지.** MIR 명령어가 본문 문장 인덱스를 싣고, CFG 생성부 3곳과 non-CFG 생성부 1곳이 문장마다 행을 낸다. rung2 MIR parity 매니페스트의 여러 문장 defer 픽스처가 native와 self-host MIR을 같은 자리에서 비교한다 | 7절 참고 |
| F5(c) 기본 LLVM 경로의 defer | `src/self_hosted/compiler/direct_mir_*.pgy` 974개 파일에 defer 관련 코드가 0곳이다. 현재는 `scalar CFG program routine admission` 단계에서 `source=AST_DEFER_STMT`로 거부한다 | 새 기능 규모. cleanup 등록과 모든 탈출 경로의 LIFO 실행이 필요하다 |
| L4b 범위 배열(`[0, 0]`) | `MirObjectArrayBoundsAtBounds` 호출부 59곳, 30파일. 그중 2파일(`mir_lower/collection_ownership_fact_owner.pgy`, `mir_lower/routine_fact_index_owner.pgy`)은 다른 레인이 미커밋으로 고치는 중이라 L4a처럼 옛 함수를 남겨야 한다 | 다음 칸. 기계적이지만 드라이버 메모리를 다시 재야 한다 |
| L4c parser `cursor_out` | `inout cursor_out: Array<Int>` 시그니처 48개, `cursor_out[0]` 읽기 226곳, 26파일. parser는 self-host 전체 소스를 도는 가장 뜨거운 경로다 | 크다. 슬라이스를 나눠 각 슬라이스마다 fixpoint와 메모리를 재야 한다 |
| L2b StringBuild | 보간·N항 `Concat`을 typed 연산으로 낮추고 owned producer로 인정하는 일이다. 같은 트리에서 다른 레인이 `own String`의 `Array<String>` 이전과 P0 R1/R2/R3/R5 반례를 작업 중이다(`docs/current_work_handoff.md`) | 대기. 같은 소유권 표면을 두 레인이 동시에 바꾸면 안 된다 |
| L2c self-host 피라미드 이관 | L2b 뒤에만 의미가 있다. `Concat` 호출 13,961개, 최상위 피라미드 2,388개(400 파일) | 대기 |

### 12.1 착지하면서 드러난 공백

- **기본 경로는 bare method 호출을 해석하지 못한다.** method 본문 안의 `Name(..)`은 기본
  C/LLVM 경로에서 `call_arity_mismatch`로 거부된다. 빌트인 이름과 무관한 기존 공백이라
  R7 게이트는 host method 구간을 native C/LLVM에서만 본다(10.5). 닫으려면 self-host
  semantic이 호출부의 enclosing host를 scope로 넣어야 한다.
- **기본 LLVM 경로(direct MIR)의 모양 coverage.** 인자 순서 픽스처를 돌리면서 세 가지
  거부를 봤다: `let` 초기식의 literal 단계(`stage=literal ... source=AST_LET_DECL`),
  세 루틴짜리 프로그램(`three-routine structural shape is unsupported`), defer 전체.
  모두 틀린 출력이 아니라 거부이고, 각각 별도 rung이다.
- **기본 C 경로의 member 호출 receiver**, 인덱스 식 receiver, **동적 ability 호출(vtable)**은
  인자 순서 고정 경로를 타지 않는다(11.1). native C의 호출식 receiver는 닫혔다.
- **native C의 emitted-name 충돌.** 빌트인 철자가 아닌 사용자 함수 이름이 런타임 심볼과
  겹칠 수 있다. 런타임이 export하는 PascalCase 심볼 42개는 모두 예약 가족에 들어 있으므로
  지금은 충돌이 없지만(10.5), 런타임에 새 심볼이 생기면 다시 열린다. emitted-name 소유를
  한 곳에서 정하는 것이 진짜 해법이다.
