# 수학 계층 설계안 (Math Layer Design)

> 작성일: 2026-04-08  
> 상태: 설계 문서 — 미구현

---

## 설계 원칙

```
코어 언어 = 일반 표현식 + 타입 안정성
표준 라이브러리 = 수학 함수
선택 DSL = 수식 친화 표기 (texmath)
별도 확장 = symbolic algebra (미래)
```

Pergyra의 핵심은 **행위/의도/경계**(subject, intent, zone, world, authority)이다.
수학 기능은 이 철학을 해치지 않는 **계산 객체**로 들어간다.

---

## 구현 로드맵

### Phase 1: 표준 수학 라이브러리 (math stdlib)

```pergyra
use math;

let x: Float = Math.Sqrt(25.0);
let y: Float = Math.Sin(Math.Pi() / 2.0);
let z: Float = Math.Pow(2.0, 10.0);
```

#### 필수 함수 목록

| 카테고리 | 함수 |
|----------|------|
| 기본 | `Abs`, `Min`, `Max`, `Clamp` |
| 거듭제곱/로그 | `Sqrt`, `Pow`, `Exp`, `Log`, `Log10` |
| 삼각함수 | `Sin`, `Cos`, `Tan` |
| 역삼각함수 | `Asin`, `Acos`, `Atan`, `Atan2` |
| 반올림 | `Floor`, `Ceil`, `Round`, `Trunc` |
| 상수 | `Pi`, `E` |
| 검사 | `IsNaN`, `IsFinite`, `IsInf` |

#### 런타임 전략

- C 런타임(`pgy_runtime_lib.c`)에 `pgy_math_*` 래퍼 추가
- LLVM 백엔드: `llvm.sin`, `llvm.cos`, `llvm.sqrt`, `llvm.pow` 등 intrinsic 직접 매핑
- 상수(`Pi`, `E`): 컴파일 타임 상수 폴딩

#### 구현 범위

```text
- 새 파일: src/runtime/pgy_math.h, src/runtime/pgy_math.c
- 시맨틱: Math 네임스페이스 빌트인 등록
- LLVM: llvm_expr.c에 Math.* 호출 → LLVM intrinsic 매핑
- C 백엔드: transpiler.c에 math.h 함수 매핑
```

---

### Phase 2: 확장 수치 타입

#### 우선순위

| 타입 | 용도 | 우선순위 |
|------|------|----------|
| `Decimal` | 금융/측정/산업 — 정밀 소수 | P1 |
| `Rational` | 정확 분수 — symbolic 브리지 | P2 |
| `Complex` | 과학/공학 — 복소수 | P3 |

#### Decimal 예시

```pergyra
let price: Decimal = 19.99d;
let tax: Decimal = price * 0.08d;
let total: Decimal = price + tax;
```

#### Rational 예시

```pergyra
let half: Rational = 1 // 2;
let third: Rational = 1 // 3;
let sum: Rational = half + third;  // 5//6
```

---

### Phase 3: texmath 리터럴 (LaTeX 수식 입력층)

#### 핵심 구조

```
LaTeX-like source → Math Parser → Math AST (IR) → { Eval, ToLatex, TypeCheck }
```

계산용 의미와 문서용 표기를 **분리**한다.

#### 문법

```pergyra
let eq = texmath `\frac{x^2 + 1}{x - 1}`;
let rendered: String = Math.ToLatex(eq);
let value: Float = Math.Eval(eq, Bind("x", 2.0));
```

또는 선언적 형태:

```pergyra
formula quadratic = texmath `ax^2 + bx + c`;
let y: Float = quadratic(a: 2.0, b: 3.0, c: 1.0, x: 5.0);
```

#### Math AST 노드

```text
MathNumber          // 숫자 리터럴
MathSymbol          // 변수/그리스 문자
MathBinaryOp        // +, -, *, /, ^
MathUnaryOp         // -, sqrt
MathFraction        // \frac{num}{den}
MathFunctionCall    // \sin, \cos, \log
MathSubscript       // x_i
MathSuperscript     // x^2
MathMatrix          // \begin{pmatrix}...\end{pmatrix}
MathSummation       // \sum_{i=a}^{b} (Phase 3+)
MathIntegral        // \int_a^b (Phase 3+, render only)
```

#### 지원 LaTeX subset (Phase 1)

```text
지원:
  \frac{a}{b}          분수
  x^{n}                거듭제곱
  x_{i}                첨자
  \sqrt{x}             제곱근
  \sin \cos \tan \log \exp \ln   함수
  \pi \alpha \beta ...  그리스 문자/상수
  + - * / =            연산자
  ( ) [ ]              괄호

미지원 (Phase 3+):
  \int \sum \prod      적분/합/곱 (렌더만, Eval은 에러)
  \lim                 극한
  \partial             편미분
```

#### Eval 규칙

- 모든 symbol은 바인딩 필수: `Math.Eval(expr, Bind("x", 2.0))`
- 미바인딩 symbol → 에러 (NOT silent 0)
- 타입은 `Float` 기본, `Rational`/`Decimal` 옵션
- 계산 불가 표기(`\int` 등) → 명시적 에러

#### ToLatex 규칙

- Math AST → LaTeX 문자열 렌더링
- 역변환: `Math.ParseTex(latex_string) → MathExpr`
- 양방향: `source → IR → LaTeX → IR` 라운드트립 보장

#### Ambiguity 정책

```text
인접 symbol: ab = a * b (곱셈 기본)
명시 식별자: \text{ab} = identifier "ab"
```

---

### Phase 4: 벡터/행렬 라이브러리

```pergyra
use math.matrix;

let a: Matrix = Matrix.FromRows([
    [1.0, 2.0],
    [3.0, 4.0]
]);
let b: Matrix = Matrix.Identity(2);
let c: Matrix = a * b;
let d: Matrix = Matrix.Inverse(a);
let det: Float = Matrix.Det(a);
```

#### 문법 설탕 (선택)

```pergyra
let c: Matrix = a @ b;         // 행렬 곱
let v: Vector = [1.0, 2.0, 3.0];
let dot: Float = v . w;        // 내적
```

---

### Phase 5: Symbolic 모듈 (미래)

```pergyra
use symbolic;

let expr = Sym.Parse("2*x^2 + 3*x + 1");
let diff = Sym.Differentiate(expr, "x");   // 4*x + 3
let simp = Sym.Simplify(diff);
let tex: String = Sym.ToLatex(simp);
```

**주의**: symbolic은 별도 엔진이다. 코어 문법에 직접 넣지 않는다.

```text
필요 컴포넌트:
- symbolic AST
- rewrite engine
- simplifier
- exact arithmetic
- pattern matching
```

---

## Pergyra 도메인 철학과의 정합

수식은 **행동 주체가 다루는 계산 객체**로 위치한다.

```pergyra
subject Solver {
    let model: MathExpr;

    func Evaluate(self, x: Float) -> Float {
        return Math.Eval(model, Bind("x", x));
    }

    func RenderPaper(self) -> String {
        return Math.ToLatex(model);
    }
}

zone SimulationZone {
    subject slot solver: Solver
}

intent ComputeTrajectory(zone: SimulationZone, solver: Solver) {
    step solve {
        where: SimulationZone;
        who: solver;
        on: solver.Evaluate(5.0);
        post: true;
    }
}
```

```text
subject = 계산하는 자
object  = 내부 수학 모델 (MathExpr)
tobject = 외부 전달 결과 (LaTeX 문자열)
intent  = 계산 목적
zone    = 계산이 허용되는 공간
```

---

## 논문 기여 포인트

### 1. 수식 표기 ↔ 실행 의미 연결

```text
Researchers can write familiar mathematical notation (LaTeX subset)
while preserving executable, typed semantics within a domain language.
```

### 2. 논문 작성 ↔ 실행 모델 브리지

```text
Same source mathematical expression serves both:
- paper-ready LaTeX rendering
- executable numeric evaluation
```

### 3. 도메인 언어 + 수식 서술 결합

```text
Mathematical objects are first-class citizens within the
intent/subject/zone domain model, not isolated utilities.
```

---

## MVP 요약

```text
최소 구현 (Phase 1):
  ✓ Math.Sqrt, Math.Sin, Math.Cos, Math.Log, Math.Pow ...
  ✓ Math.Pi(), Math.E()
  ✓ LLVM intrinsic 매핑
  ✓ C 런타임 래퍼

다음 단계 (Phase 2-3):
  ○ Decimal 타입
  ○ texmath `...` 리터럴
  ○ Math.ParseTex() / Math.Eval() / Math.ToLatex()
  ○ Math AST (IR)

미래 (Phase 4-5):
  □ Rational, Complex
  □ Matrix/Vector 라이브러리
  □ Symbolic 모듈
```
