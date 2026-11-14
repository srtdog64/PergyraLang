# texmath 사양서 (LaTeX 수식 입력층)

> 작성일: 2026-04-08  
> 상태: 사양 문서 — 미구현  
> 선행 조건: Phase 1 수학 라이브러리 완성 후

---

## 개요

`texmath`는 LaTeX 수식 subset을 Pergyra 내부에서 파싱, 계산, 렌더링하는 계층이다.

```
입력: texmath `\frac{x^2 + 1}{x - 1}`
     ↓
파싱: Math Parser (LaTeX subset → Math AST)
     ↓
계산: Math.Eval(ast, bindings) → Float
     ↓
출력: Math.ToLatex(ast) → String (LaTeX)
```

---

## 문법

### 리터럴 형태

```pergyra
let expr: MathExpr = texmath `\frac{x^2 + 1}{x - 1}`;
```

백틱(`` ` ``) 내부는 LaTeX math mode로 파싱된다.
코어 Pergyra 파서는 백틱 경계만 인식하고, 내부 파싱은 별도 Math Parser가 처리한다.

### 함수 형태

```pergyra
let expr: MathExpr = Math.ParseTex("\\frac{x^2 + 1}{x - 1}");
```

문자열로 전달 시 백슬래시 이스케이프 필요.

### formula 선언

```pergyra
formula quadratic = texmath `ax^2 + bx + c`;

// 사용
let y: Float = quadratic(a: 2.0, b: 3.0, c: 1.0, x: 5.0);
```

`formula`는 `MathExpr`를 반환하는 선언적 바인딩이다.
자유 변수는 호출 시 이름 기반으로 바인딩한다.

---

## Math AST (IR)

### 노드 타입

```c
typedef enum {
    MATH_NUMBER,          // 3.14, 42
    MATH_SYMBOL,          // x, y, alpha
    MATH_BINARY_OP,       // +, -, *, /, ^
    MATH_UNARY_OP,        // - (negation), sqrt
    MATH_FRACTION,        // \frac{num}{den}
    MATH_FUNCTION_CALL,   // \sin(x), \log(x)
    MATH_SUBSCRIPT,       // x_i
    MATH_SUPERSCRIPT,     // x^2 (alias for BINARY_OP POW)
    MATH_PAREN,           // (expr)
    MATH_MATRIX,          // \begin{pmatrix}...\end{pmatrix}
    MATH_SUMMATION,       // \sum_{i=a}^{b} expr  (Phase 3)
    MATH_INTEGRAL,        // \int_a^b expr dx     (Phase 3)
} MathNodeKind;
```

### 노드 구조

```c
typedef struct MathNode {
    MathNodeKind kind;
    union {
        double number;                           // MATH_NUMBER
        struct { const char *name; } symbol;     // MATH_SYMBOL
        struct {
            char op;              // '+', '-', '*', '/', '^'
            struct MathNode *left;
            struct MathNode *right;
        } binary;
        struct {
            char op;              // '-'
            struct MathNode *operand;
        } unary;
        struct {
            struct MathNode *numerator;
            struct MathNode *denominator;
        } fraction;
        struct {
            const char *name;     // "sin", "cos", "log", ...
            struct MathNode *arg;
        } func_call;
        struct {
            struct MathNode *base;
            struct MathNode *index;
        } subscript;
        struct {
            struct MathNode *inner;
        } paren;
        struct {
            struct MathNode **elements;
            size_t rows;
            size_t cols;
        } matrix;
    } data;
} MathNode;
```

---

## 지원 LaTeX 문법

### Phase 1 (MVP)

| LaTeX | 의미 | Math AST |
|-------|------|----------|
| `123`, `3.14` | 숫자 | `MATH_NUMBER` |
| `x`, `y`, `n` | 변수 | `MATH_SYMBOL` |
| `a + b` | 덧셈 | `BINARY_OP(+)` |
| `a - b` | 뺄셈 | `BINARY_OP(-)` |
| `a \cdot b` 또는 `ab` | 곱셈 | `BINARY_OP(*)` |
| `\frac{a}{b}` | 분수 | `MATH_FRACTION` |
| `x^{n}` 또는 `x^2` | 거듭제곱 | `BINARY_OP(^)` |
| `x_{i}` | 첨자 | `MATH_SUBSCRIPT` |
| `\sqrt{x}` | 제곱근 | `FUNC_CALL("sqrt")` |
| `\sin(x)` | 삼각함수 | `FUNC_CALL("sin")` |
| `\cos(x)` | | `FUNC_CALL("cos")` |
| `\tan(x)` | | `FUNC_CALL("tan")` |
| `\log(x)` | 자연로그 | `FUNC_CALL("log")` |
| `\ln(x)` | 자연로그 | `FUNC_CALL("log")` |
| `\exp(x)` | 지수함수 | `FUNC_CALL("exp")` |
| `\pi` | 원주율 | `MATH_NUMBER(3.14159...)` |
| `\alpha` ... `\omega` | 그리스 문자 | `MATH_SYMBOL` |
| `(`, `)` | 괄호 | `MATH_PAREN` |
| `\left(`, `\right)` | 크기조절 괄호 | `MATH_PAREN` |

### Phase 2 (확장)

| LaTeX | 의미 | 상태 |
|-------|------|------|
| `\begin{pmatrix}...\end{pmatrix}` | 행렬 | `MATH_MATRIX` |
| `\sum_{i=a}^{b} expr` | 합 | 렌더만 / Eval은 루프 전개 |
| `\prod_{i=a}^{b} expr` | 곱 | 렌더만 / Eval은 루프 전개 |

### Phase 3 (미래, 렌더 전용)

| LaTeX | 의미 | 상태 |
|-------|------|------|
| `\int_a^b f(x)\,dx` | 적분 | 렌더만, Eval 시 에러 |
| `\lim_{x \to a}` | 극한 | 렌더만 |
| `\partial` | 편미분 | 렌더만 |

---

## 연산자 우선순위

```text
1. ( )                    괄호
2. \frac{}{}, \sqrt{}     구조 연산
3. ^                      거듭제곱 (우결합)
4. 암묵적 곱셈 (ab)       인접 symbol
5. * / \cdot              명시적 곱셈/나눗셈
6. + -                    덧셈/뺄셈
7. =                      등호 (formula 정의용)
```

---

## Ambiguity 규칙

### 인접 symbol

```text
ab      → a * b     (암묵적 곱셈)
abc     → a * b * c
2x      → 2 * x
xy^2    → x * (y^2)
```

### 명시 식별자

```text
\text{ab}  → symbol "ab" (단일 변수)
\mathrm{speed} → symbol "speed"
```

### 함수 인식

```text
\sin x   → sin(x)       (단일 인자)
\sin xy  → sin(x) * y   (첫 atom만 인자)
\sin(xy) → sin(x * y)   (괄호로 명시)
```

---

## API

### Core API

```pergyra
// 파싱
func Math.ParseTex(source: String) -> MathExpr;

// 평가
func Math.Eval(expr: MathExpr, bindings: Map<String, Float>) -> Float;

// LaTeX 출력
func Math.ToLatex(expr: MathExpr) -> String;

// 자유 변수 목록
func Math.FreeVars(expr: MathExpr) -> Array<String>;

// 대입
func Math.Substitute(expr: MathExpr, name: String, value: MathExpr) -> MathExpr;

// 단순화 (상수 폴딩 수준)
func Math.Simplify(expr: MathExpr) -> MathExpr;
```

### 바인딩 헬퍼

```pergyra
func Bind(name: String, value: Float) -> Map<String, Float>;
func BindAll(pairs: Array<(String, Float)>) -> Map<String, Float>;
```

### 사용 예시

```pergyra
let eq = texmath `\frac{x^2 + 1}{x - 1}`;

// 자유 변수 확인
let vars: Array<String> = Math.FreeVars(eq);  // ["x"]

// 평가
let y: Float = Math.Eval(eq, Bind("x", 3.0));  // (9+1)/(3-1) = 5.0

// LaTeX 출력
let tex: String = Math.ToLatex(eq);  // "\\frac{x^{2} + 1}{x - 1}"

// 대입
let specific = Math.Substitute(eq, "x", texmath `a + 1`);
// → \frac{(a+1)^2 + 1}{(a+1) - 1}
```

---

## 에러 처리

```text
Math.Eval 에러 조건:
  - 미바인딩 변수 → "unbound symbol 'x'"
  - 0 나눗셈 → "division by zero in \frac"
  - 계산 불가 구조 (\int 등) → "cannot evaluate integral; render-only"
  - 타입 불일치 → "expected numeric, got matrix"

Math.ParseTex 에러 조건:
  - 미닫힌 중괄호 → "unmatched '{' at position N"
  - 미인식 명령 → "unknown command '\\xyz'"
  - 문법 오류 → "unexpected token at position N"
```

모든 에러는 `Result<T>` 패턴을 따른다.

---

## 컴파일러 구현 지침

### 파서 경계

```text
Pergyra 파서:
  - `texmath` 키워드 + 백틱 ` 인식
  - 백틱 내부 문자열을 raw로 추출
  - AST_TEXMATH_LITERAL 노드 생성

Math 파서 (별도):
  - LaTeX subset 전용 파서
  - MathNode 트리 생성
  - src/math/math_parser.c (새 파일)
```

### 런타임 통합

```text
pgy_runtime_lib.c에 추가:
  - pgy_math_parse_tex(const char *source) → MathExpr*
  - pgy_math_eval(MathExpr*, bindings) → double
  - pgy_math_to_latex(MathExpr*) → char*
  - pgy_math_free_vars(MathExpr*) → char**
  - pgy_math_substitute(MathExpr*, name, replacement) → MathExpr*
  - pgy_math_simplify(MathExpr*) → MathExpr*
```

### LLVM 백엔드

```text
- texmath 리터럴 → 컴파일 타임에 Math Parser 실행 → MathNode 트리를 상수 데이터로 embed
- Math.Eval → 인라인 산술 생성 (상수 바인딩 시) 또는 런타임 호출
- Math.ToLatex → 런타임 호출
```

---

## 파일 구조 (예상)

```text
src/math/
  math_ast.h          MathNode 정의
  math_parser.h       파서 인터페이스
  math_parser.c       LaTeX subset → MathNode
  math_eval.c         MathNode → double
  math_latex.c        MathNode → LaTeX string
  math_simplify.c     상수 폴딩 / 기본 정리
```

---

## 비고

- `texmath`는 코어 파서를 오염시키지 않는다 (백틱 경계로 격리)
- 계산 불가 표기는 명시적 에러로 처리한다 (silent fallback 금지)
- symbolic 계층(미분, 적분, 패턴 매칭)은 이 사양의 범위 밖이다
- 이 사양은 Phase 1 math stdlib 완성 후 구현을 시작한다
