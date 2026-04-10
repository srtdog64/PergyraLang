# Token Split Plan

마지막 업데이트: 2026-04-10

이 문서는 `subject/class/struct/object/tobject`의 lexer/parser/AST token aliasing을 제거하는 계획을 고정한다.

## 1. 문제 정의

현재 상태:

- `subject`와 `class`가 둘 다 `TOKEN_CLASS`로 lex된다
- `struct`와 `tobject`가 둘 다 `TOKEN_STRUCT`로 lex된다
- `object`는 dedicated token이 아니라 contextual keyword로 처리된다

이 상태는 구현 편의로는 버티지만, 언어 계약으로는 좋지 않다.

문제:

1. 코어 존재론이 lexer 단계에서 숨겨진다
2. parser가 `token kind + token text`를 함께 봐야 해서 불필요하게 취약하다
3. 문서의 존재론과 구현 내부 표상이 어긋난다
4. 이후 tooling/LSP/formatter/diagnostics가 declaration flavor를 더 불명확하게 읽게 된다

## 2. 목표 상태

다음 token을 독립시킨다.

- `TOKEN_SUBJECT`
- `TOKEN_CLASS`
- `TOKEN_STRUCT`
- `TOKEN_OBJECT`
- `TOKEN_TOBJECT`

parser/AST/semantic/codegen은 더 이상

- `TOKEN_CLASS + text == "subject"`
- `TOKEN_STRUCT + text == "tobject"`
- contextual `object`

같은 우회 규칙에 의존하지 않는다.

## 3. 범위

직접 영향:

1. lexer
2. parser
3. AST printer / formatter / LSP keyword surface
4. semantic declaration dispatch
5. codegen declaration flavor 분기
6. 문서와 키워드 보드

이번 계획에서 직접 바꾸지 않는 것:

1. 존재론 자체 재설계
2. `class` 축 축소/삭제 여부
3. `object/tobject` 의미론 변경

즉 이번 작업은 의미론 재설계가 아니라 내부 표상 정직화다.

## 4. 단계별 계획

### Phase 1. Lexer token 분리

작업:

1. `lexer.h`
   - `TOKEN_SUBJECT`
   - `TOKEN_OBJECT`
   - `TOKEN_TOBJECT`
   추가
2. `lexer.c`
   - `"subject" -> TOKEN_SUBJECT`
   - `"object" -> TOKEN_OBJECT`
   - `"tobject" -> TOKEN_TOBJECT`
   로 변경
3. `token_type_to_string()` 갱신

완료 조건:

- 더 이상 `subject -> TOKEN_CLASS`, `tobject -> TOKEN_STRUCT` alias가 없다
- `object`는 contextual keyword가 아니라 real token이다

### Phase 2. Parser declaration entry 정리

작업:

1. `parser.c`
   - declaration dispatch를 token text 비교 없이 token kind 기준으로 바꿈
2. `parser_decl.c`
   - `parse_subject_declaration()`
   - `parse_class_declaration()`
   - `parse_object_declaration()`
   - `parse_tobject_declaration()`
   진입 조건을 분리
3. `parser_domain.c`
   - `subject/object/tobject` slot/header parsing에서 text 비교를 줄이고 token kind를 우선 사용

완료 조건:

- `strcmp(token.text, "subject")`
- `strcmp(token.text, "tobject")`
- contextual `object`
경로가 declaration parsing에서 제거된다

### Phase 3. AST/semantic/codegen surface 정리

작업:

1. diagnostics가 `token text hack` 없이 nominal flavor를 직접 사용
2. codegen declaration flavor branch가 token aliasing 가정 없이 동작
3. AST print / formatter / LSP keyword expectation 갱신

완료 조건:

- parser 이후 단계가 lexer aliasing을 가정하지 않는다

### Phase 4. 테스트/문서 정렬

작업:

1. parser tests
2. semantic tests
3. transpile/LLVM smoke
4. grammar/status docs

완료 조건:

- 문서가 “코어 존재론이 lexer 단계에서도 독립”이라고 말할 수 있다

## 5. 리스크

### 리스크 1. parser_domain 영향 범위가 넓다

`subject/object/tobject`는 declaration뿐 아니라 domain slot/header parsing에도 걸쳐 있다.

대응:

- Phase 1, 2를 먼저 닫고
- domain parser는 그 다음 targeted fix

### 리스크 2. formatter/LSP keyword surface mismatch

`object`가 real token이 되면 keyword 하이라이트/완성/정의점프 주변의 표면 기대가 달라질 수 있다.

대응:

- lexer split 직후 문서보다 먼저 parser/LSP smoke를 확인

### 리스크 3. 기존 contextual helper 경로 잔재

`parser_check_contextual_keyword(parser, "object")` 같은 helper가 오래 남으면 반쯤만 고친 상태가 된다.

대응:

- 최종 완료 조건에 “declaration/object slot 관련 contextual dependency 제거”를 포함

## 6. 우선순위

지금 가장 먼저 할 단계:

1. Phase 1. lexer token 분리
2. Phase 2. parser declaration dispatch 정리

이 둘이 닫혀야 그 다음 semantic/codegen 정리가 의미가 있다.

## 7. 한 줄 결론

`subject/class/struct/object/tobject`는 이미 언어 코어 존재론으로 취급되고 있다.
그러면 lexer/parser 내부 표상도 그 사실을 숨기지 말고 그대로 드러내야 한다.
