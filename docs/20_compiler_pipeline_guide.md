# Pergyra 컴파일러 파이프라인 가이드

마지막 업데이트: 2026-03-18

이 문서는 "현재 저장소가 실제로 어떻게 동작하는가"를 설명한다. 이상적인 미래 설계가 아니라, 다음 기여자가 바로 코드를 따라 들어갈 수 있게 만드는 contributor guide다.

## 1. 한눈에 보는 파이프라인

```text
.pgy source
  -> read_file()                        [src/pgy_driver.c]
  -> Lexer                              [src/lexer/*]
  -> Parser -> AST_PROGRAM              [src/parser/*]
  -> import inline merge                [src/pgy_driver.c]
  -> semantic_analyze()                 [src/semantic/*]
  -> annotated AST
  -> hir_lower()                        [src/compiler/hir.c]
  -> HIRProgram (top-level buckets)
  -> backend dispatch                   [src/pgy_driver.c]
       -> LLVM backend (default if enabled)
          -> object file
          -> gcc link + runtime
       -> C backend (fallback/reference)
          -> generated .c
          -> gcc compile + runtime
  -> native binary
```

중요한 점은 세 가지다.

- 프론트엔드의 기준 자료구조는 여전히 AST다.
- 백엔드 진입점은 이제 AST 루트가 아니라 `HIRProgram`이다.
- HIR는 SSA 같은 깊은 IR이 아니라, "top-level 분류 버킷 + 원래 AST 노드 참조"에 가깝다.

## 2. 어디서 시작하나

실제 진입점은 `src/pgy_driver.c`의 `main()`과 `run_pipeline()`이다.

이 파일이 하는 일:

- 소스 파일 읽기
- 토큰 dump / AST dump / HIR dump 같은 CLI 모드 처리
- 렉서와 파서 생성
- import 해석과 AST 병합
- 시맨틱 분석 호출
- HIR lowering 호출
- LLVM 또는 C 백엔드 선택
- 네이티브 바이너리 링크 및 `--run` 실행

즉, "언어 파이프라인의 연결 지점"은 대부분 이 파일에서 보인다.

## 3. 단계별 설명

### 3.1 Lexer

관련 디렉터리:

- `src/lexer/lexer.h`
- `src/lexer/lexer.c`

역할:

- 소스 문자열을 `Token` 스트림으로 변환
- 키워드, 식별자, 리터럴, 연산자 인식
- 행/열 위치 추적
- 파서가 pull 방식으로 `lexer_next_token()`을 호출하게 지원

드라이버에서 `--tokens`를 주면 이 단계만 실행해서 토큰을 출력한다.

### 3.2 Parser

관련 파일:

- `src/parser/parser.h`
- `src/parser/parser.c`
- `src/parser/ast.h`
- `src/parser/ast.c`

주 진입점:

- `parser_create(Lexer *lexer)`
- `parser_parse_program(Parser *parser)`

파서는 재귀 하향 방식이다. `Parser` 구조체에는 단순 토큰 상태 외에도 문맥 플래그가 들어 있다.

- `in_parallel_block`
- `in_with_statement`
- `in_async_context`
- `in_select_statement`
- `in_extern_block`
- `scope_depth`

이 플래그들은 "현재 어떤 문맥에서 이 문법을 허용할지"를 관리할 때 쓰인다. 새 문법을 넣을 때 파싱 가능 위치가 문맥 의존적이면 이 구조체를 먼저 봐야 한다.

파서 결과는 `AST_PROGRAM` 루트를 가진 AST다. 실제 노드 종류는 `src/parser/ast.h`의 `ASTNodeType`에 거의 전부 모여 있다.

현재 AST가 담는 범위:

- 함수, 클래스, extern block
- `let`, `if`, `for`, `while`, `return`
- `with`, `parallel`
- async 관련 구문
- role / ability / party / world
- event 관련 구문
- unsafe / defer / bind
- 호출, 멤버 접근, 배열 접근, 대입, 람다 등 표현식

### 3.3 Import 해석

이 프로젝트에서 import 해석은 별도 모듈 로더가 아니라 드라이버 안에서 처리된다.

관련 위치:

- `src/pgy_driver.c`

흐름:

1. 메인 파일을 파싱해 `AST_PROGRAM` 생성
2. top-level에서 `AST_IMPORT_DECL`을 찾음
3. 가져온 파일을 다시 lex/parse
4. 가져온 AST의 statements를 원래 AST에 inline splice

즉 현재 import는 "semantic 이전의 AST 병합"이다. 따라서 다음 단계들은 import가 이미 펼쳐진 단일 프로그램처럼 본다.

이 구조의 장점은 단순하다는 것이고, 단점은 import 시스템이 아직 드라이버 레벨 구현이라는 점이다.

### 3.4 Semantic

관련 파일:

- `src/semantic/semantic.h`
- `src/semantic/semantic.c`
- `src/semantic/type_checker.*`
- `src/semantic/slot_analyzer.*`

주 진입점:

- `SemanticResult *semantic_analyze(ASTNode *ast)`

`semantic_analyze()`는 현재 두 덩어리로 동작한다.

1. `type_check_program(ast, ctx)`
2. 에러가 없을 때 `slot_analyze_program(ast, sa)`

출력은 `SemanticResult`다.

- `success`
- `annotated_ast`
- `diagnostics`
- `error_count`
- `warning_count`

중요한 점은 `annotated_ast`가 새로운 트리를 만드는 게 아니라, 기존 AST에 타입 정보와 검증 결과를 붙인 같은 트리라는 것이다. 이후 HIR lowering은 이 annotated AST를 입력으로 받는다.

### 3.5 HIR Lowering

관련 파일:

- `src/compiler/hir.h`
- `src/compiler/hir.c`

주 진입점:

- `HIRProgram *hir_lower(ASTNode *annotated_ast, char **error_message)`

현재 HIR의 역할은 "깊은 중간표현 생성"이 아니라 "top-level program 분류와 백엔드 입력 정규화"에 가깝다.

`HIRProgram`이 갖는 핵심 버킷:

- `externs`
- `types`
- `abilities`
- `roles`
- `parties`
- `systemics`
- `worlds`
- `actors`
- `events`
- `functions`
- `executables`
- `items`

여기서 `items`는 선언 순서를 보존하는 ordered top-level 목록이고, 나머지 배열들은 종류별 빠른 접근용 버킷이다.

중요한 구현 특성:

- HIR는 AST 노드를 복사하지 않는다.
- 각 버킷 원소는 여전히 `ASTNode *`다.
- `Main` 함수 존재 여부는 lowering 단계에서 `has_main_function`으로 기록된다.
- `AST_IMPORT_DECL`은 여기 오기 전에 드라이버에서 이미 해소되어 skip된다.

즉, 현재 HIR는 "백엔드가 AST 전체를 다시 뒤지지 않도록 top-level 구조를 정리한 뷰"라고 보는 편이 맞다.

### 3.6 Backend: LLVM과 C

관련 파일:

- `src/compiler/compiler.h`
- `src/compiler/compiler.c`
- `src/codegen/llvm_backend.h`
- `src/codegen/llvm_backend.c`
- `src/codegen/transpiler.h`
- `src/codegen/transpiler.c`

드라이버는 HIR 이후에 백엔드를 선택한다.

#### LLVM 백엔드

기본 경로다. `PGY_LLVM_ENABLED`로 빌드된 경우 `pgy`는 LLVM을 기본 백엔드로 사용한다.

주 API:

- `compiler_emit_llvm_ir()`
- `compiler_emit_llvm_ir_to_file()`
- `compiler_build_native_llvm()`

LLVM 경로는 대략 다음 순서다.

1. `llvm_codegen()` 또는 `llvm_codegen_to_object()`
2. `.o` 생성
3. `gcc`로 런타임과 함께 링크
4. 실행 파일 생성

즉 "순수 LLVM 툴체인만 사용"은 아니고, 최종 링크는 현재도 `gcc`와 런타임 C 파일을 사용한다.

#### C 백엔드

참조 구현이자 fallback 경로다.

주 API:

- `compiler_emit_c()`
- `compiler_build_native()`
- `transpile()`

흐름:

1. HIR를 C 코드로 변환
2. `.c` 파일 생성
3. `gcc`로 컴파일
4. 실행 파일 생성

중요한 점은 C 백엔드도 이제 AST 루트를 직접 받지 않고 `HIRProgram`을 입력으로 받는다는 것이다.

### 3.7 Runtime / Link

관련 파일:

- `src/runtime/pgy_runtime.h`
- `src/runtime/pgy_runtime_lib.c`

백엔드가 무엇이든 최종 바이너리는 현재 런타임 심볼에 의존한다. 특히 LLVM 경로도 object만 LLVM이 만들고, 링크 시에는 런타임 C 구현이 같이 들어간다.

따라서 새 기능이 런타임 내장 함수나 ABI를 요구하면 백엔드만 고치면 끝나지 않고 런타임도 같이 수정해야 한다.

## 4. 디렉터리별 책임

```text
src/
  lexer/       토큰화
  parser/      AST 생성
  semantic/    타입 검사, 슬롯 분석, 진단
  compiler/    HIR + 빌드 파사드
  codegen/     LLVM / C 백엔드
  runtime/     런타임 심볼과 헬퍼
  lsp/         LSP 서버
  pgy_driver.c CLI와 전체 파이프라인 연결

examples/      예제 입력
tests/         회귀 테스트, 백엔드 비교 스크립트
docs/          설계 문서와 상태 문서
```

## 5. 새 문법이나 기능을 넣을 때 체크리스트

새 기능이 들어오면 보통 아래 순서로 본다.

1. 렉서
   새 키워드나 토큰 종류가 필요하면 `src/lexer/*` 수정.

2. AST
   새 노드 타입이나 필드가 필요하면 `src/parser/ast.h`와 생성/파괴 로직 수정.

3. 파서
   `parser_parse_*` 계열 함수 추가 또는 기존 분기 확장.
   문맥 제약이 있으면 `Parser` 플래그도 같이 검토.

4. 시맨틱
   타입 검사 규칙, 심볼 등록, 진단 메시지 추가.
   슬롯 생명주기와 관련 있으면 `slot_analyzer`까지 같이 수정.

5. HIR
   top-level 선언이면 `hir_lower()`의 분류 규칙에 추가.
   expression/statement 레벨 기능이면 HIR 수정이 필요 없을 수도 있다.

6. C 백엔드
   `src/codegen/transpiler.c`에 해당 노드 emit 추가.

7. LLVM 백엔드
   `src/codegen/llvm_backend.c`에 동일 기능 추가.

8. 런타임
   새 builtin, 메모리 모델, ABI가 필요하면 `src/runtime/*` 수정.

9. 테스트
   최소한 단위 테스트와 예제를 추가.
   가능하면 `tests/compare_backends.sh` 대상에도 넣어서 C/LLVM 결과를 맞춘다.

이 프로젝트에서는 "파서만 되고 LLVM은 안 됨" 상태가 금방 쌓이기 쉽다. 새 기능은 가능하면 C와 LLVM을 같이 닫는 편이 맞다.

## 6. 자주 쓰는 로컬 워크플로

빌드:

```bash
make LLVM_ENABLED=1 bin/pgy
make LLVM_ENABLED=1 all
```

예제 실행:

```bash
./bin/pgy examples/hello.pgy --run -v
./bin/pgy examples/slots.pgy --run -v
```

IR 출력:

```bash
./bin/pgy examples/hello.pgy --emit-llvm -o hello.ll
```

테스트:

```bash
make llvm-test-all
make llvm-test-backend-compare
```

파이프라인 중간 상태 확인:

```bash
./bin/pgy examples/hello.pgy --tokens
./bin/pgy examples/hello.pgy --ast
./bin/pgy examples/hello.pgy --hir
```

## 7. 현재 구조에서 꼭 알아야 할 제약

### 7.1 HIR는 아직 얕다

이름은 HIR지만, 지금은 top-level 분류기가 더 가깝다. expression/statement 수준이 별도 IR 노드로 재구성되지는 않는다.

### 7.2 import는 드라이버 책임이다

모듈 시스템이나 패키지 로더가 따로 있는 게 아니다. import 확장은 `src/pgy_driver.c`에 있다.

### 7.3 annotated AST가 프론트엔드의 실질 기준 구조다

semantic 단계 이후에도 많은 정보는 AST에 달려 있다. 따라서 AST 구조를 바꾸면 semantic, HIR, 백엔드가 함께 영향을 받는다.

### 7.4 LLVM가 기본이지만 런타임 의존은 남아 있다

LLVM이 object를 만들어도 최종 실행 파일은 런타임 C 구현과 링크된다. 따라서 "완전 독립 LLVM 세계"라고 생각하면 구조를 잘못 읽게 된다.

### 7.5 C 백엔드는 아직 중요한 참조 구현이다

기본 백엔드는 LLVM이지만, 기능 확인과 회귀 비교에서는 C 백엔드가 여전히 기준점 역할을 한다.

## 8. 다음 주자가 처음 읽으면 좋은 파일 순서

추천 순서:

1. `src/pgy_driver.c`
2. `src/parser/parser.h`
3. `src/parser/ast.h`
4. `src/semantic/semantic.h`
5. `src/semantic/type_checker.*`
6. `src/compiler/hir.h`
7. `src/codegen/llvm_backend.c`
8. `src/codegen/transpiler.c`
9. `src/compiler/compiler.c`
10. `tests/compare_backends.sh`

이 순서로 보면 "입력 -> 프론트엔드 -> lowering -> 백엔드 -> 검증" 흐름이 가장 빨리 잡힌다.
