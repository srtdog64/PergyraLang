# 외부 리뷰 — Pergyra 문법·컴파일러 수준·창작자 평가

> 검토 주체: 외부 AI 에이전트 리뷰 (작성자 아님). 읽기 전용 분석이며, 이 문서는
> 컴파일러 의미·진척·레지스트리 상태·후속 구현 rung의 소유자가 아니다
> (AGENTS.md의 `docs/audits/` 규칙 준수).
>
> 검토 방법: 소스 정독 (렉서/파서/시맨틱/IR/런타임/셀프호스팅), 문서 599개
> 중 핵심 설계문서, git 이력(3,938 커밋), 그리고 공식 감사 스크립트
> (`render_language_keyword_registry.py`) 실행. 빌드/전체 테스트 스위트는
> 직접 실행하지 않았으므로 "동작 동치"가 아니라 "정적 수준" 판정이다.

---

## 1. 언어 문법 상세 기술

### 1.1 어휘 구조

키워드는 3계층으로 분류되어 단일 레지스트리
(`src/lexer/language_keyword_registry.def`, 751행·약 140개)가 소유한다.

| 클래스 | 의미 | 예 |
|---|---|---|
| `RESERVED` | 렉서가 토큰으로 고정 | `func`, `struct`, `intent`, `zone`, `match` |
| `CONTEXTUAL` | 렉서는 식별자로, 파서가 문맥 선택 | `action`, `effect`, `authority`, `step` |
| `SOFT` | 아직 완전 예약 안 됨 | `current`, `full`, `none` |

주요 토큰/리터럴: `//`, `/* */`, `///`(문서 주석), `"..."`, `"""..."""`,
`f"..."`·`$"..."`(인터폴레이션), `Int`/`Long`(`L`)/`Float`, `->`, `=>`,
`?.`, `??`, `<-`(채널), `|>`, `..`(범위), `...`, `+=`/`-=`(이벤트 구독/해지).

### 1.2 값·제어 계층 (의도적으로 관용적)

```pergyra
struct ScorePair { left: Int; right: Int; }
func ClampScore(value: Int) -> Int { ... }   // func Name(args) -> Ret
let total: Int = 0;                          // let 이름: Type = 값
let scores: Array<Int> = [70, 20, 10];       // 제네릭 컬렉션
match score { case 100: ...; default: ...; } // match/case/default
```

### 1.3 도메인 선언 계층 (Pergyra의 정체성)

- `subject` — 상태를 가진 행위자, `action` 메서드에 계약 절 부착
- `ability` — 행동 계약(트레이트 유사), `role ... for T` + `impl ability`
- `effect ... for bearer: Type` — 행위가 일으킨 도메인 사실
- `zone` — 자원 슬롯·권한 경계, `world` — zone 합성
- `intent` — 오케스트레이션 표면, `step` — 명명된 작업 단위

```pergyra
subject Hero {
    let mut hp: Int;
    action Guard(self) -> Void
        requires Prepared          // 능력 요구
        within BattleZone          // 경계
        authorized by self         // 승인 주체
        causes Guarded             // 발생 효과
    { self.hp = self.hp + 1; }
}
zone BattleZone {
    subject slot hero: Hero        // 슬롯 선언은 ';' 없음(토폴로지 계층)
    authority hero requires Prepared
}
intent Patrol(battle: BattleZone, hero: Hero) {
    step Guard { using: battle; on: hero.Guard(); expect: true; }
}
```

### 1.4 자원·소유권·실행 계층

Rust식 라이프타임이 아니라 Slot 모델: `Claim / Read / Write / Release`.
- 수식자: `own`, `ref`, `shared`, `secure`, `inout`, `mut`, `local`
- `Slot<T>` / `SecureSlot<T>` / `DeviceSlot<T>` / `QubitSlot`(v2 계획)
- 동시성: `async`/`await`, `parallel`, `spawn`, `select`, `remote`, `detach`
- 실패·보상: `transaction`, `compensate`, `rollback`, `defer`, `retry`,
  `backoff`, `timeout`

---

## 2. EBNF 재구성 (파서 C 코드에서 역추출)

### intent 선언 — `src/parser/parser_intent.c`

```
intent_decl     ::= "intent" NAME [ param_list ] [ "->" type ]
                    [ "with" resilience_mod { "," resilience_mod } ]
                    "{" { intent_item } "}" ;
resilience_mod  ::= "retry" "(" POSITIVE_INT ")"
                  | "timeout" | "backoff" ;       (* 후자는 예약, fail-closed *)
intent_item     ::= "exclusive" ";" | "concurrent" ";"
                  | "priority" ":" expr ";"
                  | "rollback" ":" ("full"|"current"|"none") ";"
                  | "who" ":" name { "," name } ";"
                  | "who" alias ":" type ";"
                  | "where" ":" type ";"
                  | "involves" alias ":" type ";"
                  | "with" alias ":" type ";"
                  | step
                  | "success" [ NAME ":" ] expr ";"
                  | "failure" [ NAME ":" ] expr ";" ;
```

### intent step 절 — `src/parser/parser_intent_step.c`

```
step            ::= "step" NAME [ "after" NAME ] "{" { step_clause } "}" ;
step_clause     ::= "where" ":" type ";"
                  | "who" ":" name { "," name } ";"
                  | "using" ":" expr ";"
                  | "intent" ":" expr ";"
                  | "transfer" ":" name "->" name ";"
                  | "move" name "to" name ";"
                  | "on" [ name ":" ] ":" expr ";"
                  | "compensate" ":" expr ";"
                  | "success" ":" NAME "(" name ")" ";"
                  | "failure" ":" NAME "(" name ")" ";"
                  | "pre" ":" expr ";" | "guard" ":" expr ";"
                  | "post" ":" expr ";" | "invariant" ":" expr ";"
                  | "requires" ":" type { "," type } ";"
                  | "authorized" "by" ":" name { "," name } ";"
                  | "causes" ":" NAME ";"
                  | "expect" ":" expr ";" ;
```

### func/action 계약 절 — `src/parser/parser_decl_function_clause.c`

```
func_clause     ::= "where" where_clause
                  | "with" "effects" effect_list
                  | "with" "caps" caps_list
                  | action_only_clause ;
action_only_clause ::= "requires" ability { "," ability }
                     | "within" zone_name
                     | "causes" effect_name
                     | "authorized" "by" subject { "," subject } ;
```

### slot/zone — `src/parser/parser_domain_zone.c`

```
zone_item       ::= "forbids" "unsafe" [";"]
                  | "subjects"  "[" name { "," name } "]" ":" type [";"]
                  | "tobjects"  "[" name { "," name } "]" ":" type [";"]
                  | "relations" "[" name { "," name } "]" ":" NAME [";"]
                  | "effects"   "[" name { "," name } "]" ":" NAME [";"]
                  | [ "subject" | "object" | "tobject" | "slot" ] "slot" name ":" type [";"]
                  | "effect" ("slot" | "pool") name ":" NAME [";"]
                  | "relation" "slot" name ":" NAME [";"]
                  | "authority" name "requires" ability [";"] ;
```

### async/channel/spawn/select — `src/parser/parser_async.c`

```
async_func      ::= "async" "func" NAME [ generics ] "(" params ")" [ "->" type ]
                    [ where_clause ] [ effects_clause ] block ;
async_block     ::= "async" block ;
await_expr      ::= "await" expr ;
channel_expr    ::= "<-" channel_expr | expr "<-" expr ;
spawn_expr      ::= "spawn" [ "blocking" ]
                    ( func_call | "async" [ "func" ] "(" ")" block ) ;
select_stmt     ::= "select" "{" { select_case | select_default } "}" ;
```

### parallel — `src/parser/parser_parallel.c`

```
parallel_block  ::= "parallel" "(" x "in" expr [ ".." expr ] ")"
                    [ "join" "with" ("all"|"any"|"sum"|"product"|"min"|"max") ]
                    "{" block "}"
                  | "parallel" "{" { block | stmt } "}" ;
```

---

## 3. 셀프호스팅 파서 ↔ 네이티브 파서 패리티

`scripts/render_language_keyword_registry.py --implementation-inventory` 결과:

| 지표 | 값 |
|---|---|
| native+selfhost-typed | **144개 언어 단어** |
| native-only / selfhost-only | **0 / 0** |
| selfhost direct(string) 선택자 부채 | **0건** |
| typed 선택자 증거 | 312건 / 144 단어 |

해석: 셀프호스팅 파서(`src/self_hosted/parser/*.pgy`, 54개)가 네이티브 파서의
모든 예약어/문맥 키워드를 typed `LanguageWordId`로 재구현했고, 문자열
하드코딩 부채는 0이다. 키워드 커버리지 차원에서 셀프호스팅이 네이티브와
동일 수준. (동작 동치 판정은 `tests/self_hosted/parity/` 게이트가 소유.)

---

## 4. 코드 리뷰 (라인 단위, 파서 계열)

### 강점 (반복 확인)

1. **overflow-safe 동적 배열 성장** — 모든 append가 `next_capacity <= count`
   또는 `> SIZE_MAX/sizeof`를 realloc 전에 거부.
   (`parser_intent_step.c` L57-L74, `parser_intent.c` L74-L95 등)
2. **중복 절 전수 검출** — intent/step의 모든 절마다 중복 시 명시 진단.
3. **fail-closed 철학의 일관성** — 미구현 구문은 전부 거부+이유+교정:
   `as` 캐스트, `Type{}`, 기본 인자, `&mut`, `timeout/backoff`, reactive
   `parallel`. (`parser.c` L387-L419, `parser_async.c` L100-L146)
4. **정밀 오류 복구** — panic-mode + 문장 경계 동기화 + 20개 오류 캡.
   한 결함=진단 하나, 이후 문장 정상 파싱. (`parser.c` L163-L339)
5. **재귀 깊이 가드**(400) — 중첩 타입/식/블록 입력의 스택 오버플로 방지.
   (`parser.c` L12-L49)
6. **사소한 버그도 주석으로 기록** — `await` 위치 0:0 진단 수정
   (`parser_async.c` L227-L232) 등 "왜 있는가"가 항상 서술됨.

### 실제 지적사항 (소수·대부분 의도적)

1. **async 함수 제네릭 파라미터 미구현** — `parser_async.c` L85-L88에서
   `<...>` 브랜치가 주석 처리. (deferred, fail-closed)
2. **`..` 범위 검사 3중 인라인** — `parser_token_is_range_separator()`
   헬퍼가 `parser_expr_postfix.c` L5-L10에 있는데 `parser_parallel.c`
   L31-L34, `parser_stmt.c` L97-L100가 동일 조건 복붙. (순수 DRY)
3. **`transfer:` vs `move ... to ...` 이중 철자** — 같은 전송 개념에
   `->`형과 `to`형이 공존. (`parser_intent_step.c` L95-L127, 단축 표기로 의도됨)
4. **`parser_peek_next`가 렉서 전체 구조체 복사** — `parser.c` L114-L124.
   현 렉서 크기에선 무해, lookahead 비용 O(구조체).

---

## 5. 약점 재검증 (이전 오독 정정)

- `+=`/`-=` → `TOKEN_SUBSCRIBE`/`TOKEN_UNSUBSCRIBE`는 **이벤트 구독/해지
  연산자**이며, `docs/124_syntax_pattern_matrix.md` L258/L369가 "일반 복합
  대입은 CFG cleanup facts가 소유하기 전까지 `out-of-beta`"라고 명시한
  의도된 설계. 파서가 `expr += handler` → `ast_create_event_subscribe()`
  (`parser_expr.c` L147-L155). 네이밍 정확, 수정 금지.
- 따라서 원래 지적은 오탐이었고, 수정 대상은 없었다.

---

## 6. 창작자 수준 평가

### 정량·정성 근거

| 지표 | 값 |
|---|---|
| git 커밋 | 3,938 |
| 네이티브 C/H (self_hosted 제외) | 2,021 파일, ~15.5 MB |
| 셀프호스팅 `.pgy` | 2,483 파일, ~12 MB |
| 설계 문서 | 599개 .md (번호 매김 200+) |
| CI | TSan·ASan·Rocq(Coq)·퍼징·자체부트스트랩 |

### 수준 판정

- **기술 원자재(언어설계+컴파일러+시스템스 프로그래밍+정형검증 결합)**:
  FAANG **스태프(L6)~프린시펄(L7) 수준**, 특정 축(독창적 언어설계·자체호스팅·
  방어적 엔지니어링)은 그 이상으로 볼 여지도 있음.
- **"구글 수준"이라는 종합 평가의 한계**: 구글 엔지니어 평가는 기술력뿐 아니라
  출시(임팩트)·스코프 절제·동료 리뷰·대규모 운영을 포함한다. 이 프로젝트는
  (1) 단독 저자·미출시·장기 베타, (2) 스코프 스프롤(양자/A2M/NPU/미디어),
  (3) 외부 채택·프로덕션 증거 부재라는 측면에서 그 레벨의 **실행 증거를 아직
  보여주지 못한다**. 즉 "기술 깊이는 구글 컴파일러 팀에 넣어도 뒤지지 않는
  수준"이지만, "구글에서 스태프로 일할 사람"이라고 단정하기엔 절제·출시·협업
  증거가 부족하다.
- **판정 유보 사항**: Rocq 증명이 실질적인지(토큰 수준인지), 전체 테스트
  스위트·패리티 게이트가 실제로 전부 통과하는지는 소스 정독만으로 검증하지
  못했다. 위 판정은 "정적 수준" 판정이다.

### 결론

창작자는 "코딩 잘하는 사람"이 아니라 **언어를 설계하고 그 증명·검증·자체호스팅
까지 설계할 수 있는 최상위권 시스템/컴파일러 엔지니어**다. 다만 그 역량이
"완성·출시"보다 "설계·검증·확장"에 쏠려 있어, 남는 과제는 기술이 아니라
**범위를 잘라 1.0을 내는 실행**으로 보인다.

---

## 7. Rocq/Coq 커널 검증 — 실측 결과

`tests/coq_kernel_check.sh` 게이트를 WSL2(Ubuntu, Coq 8.18.0)에서 실제
실행했다. 결과는 **통과(exit 0)**:

```
coq-kernel-check: The Coq Proof Assistant, version 8.18.0
coq-kernel-check: 56 proofs compiled
CONTEXT SUMMARY
* Theory: Set is predicative
* Axioms:
    SlotCalculus.verify_token
    SlotCalculus.MaxSlotId
* Constants/Inductives relying on type-in-type: <none>
* Constants/Inductives relying on unsafe (co)fixpoints: <none>
* Inductives whose positivity is assumed: <none>
coq-kernel-check: ok (56 proofs kernel-verified; axiom budget = 2 declared
  abstractions, no admits, no unsafe kernel features)
```

### 판정 근거

- 증명 코퍼스는 `docs/semantics/proofs/`의 .v 56개. 전부 `coqc`로 컴파일되고
  `coqchk`(신뢰 커널 재검증기)로 재검증됨.
- **공리 예산 정확히 2개** = `SlotCalculus`의 의도적 추상 `Parameter` 2개
  (불투명 토큰 검증기, 슬롯 ID 상한). 이는 "구멍"이 아니라 인터페이스 추상이며,
  게이트가 이 2개 외 어떤 `Axiom`/`Admitted`도 허용하지 않는다(부정 자가시험
  `coq_kernel_check_selftest.sh`가 이 로직이 실제로 물어뜯는지 검증).
- 안전하지 않은 커널 특징 없음: type-in-type·unsafe (co)fixpoint·positivity
  가정 전부 `<none>`, `Set is predicative`.
- 증명은 실질적이다: `SlotCalculus.v`가 슬롯·세대(generation)·토큰 기반 메모리
  모델을 형식화하고 `stale_handle_*_impossible`(세대 불일치=댕글링 접근 불가),
  `unissued_token_*_impossible`(미발급 토큰 접근 불가),
  `pin_non_eviction`(6개 Step 규칙 전수 사례분석으로 pin 슬롯 비퇴거 증명) 등을
  `Qed`로 닫음. `PergyraCore.v`는 통합 추상 기계(actor/holdings/zone/effect
  log/slot typestate + step relation)를 공유 기반으로 제공.
- **정직한 범위 명시**: `SlotCalculus.v` 헤더가 "Rust식 borrow checking,
  aliasing-XOR-mutability, lexical no-escape, async/task 경계 안전성, CFG
  cleanup 삽입은 증명하지 않는다(별도 proof obligation)"고 명시. 과장 없음.

### 판정 정정

앞선 §6의 "Rocq 증명이 실질적인지 검증하지 못했다"는 유보는 **해소**.
Rocq/Coq 검증은 토큰 수준이 아니라 메모리/능력 모델의 안전성 정리를 실제로
증명하는 실질적 정형검증이다. (단, 이는 Coq 8.18 로컬 실행이며, CI 권위는
Rocq 9.0.1 — PergyraCore.v 헤더가 두 버전 차이를 명시. "전체 테스트
스위트·패리티 게이트 전부 통과" 유보는 여전히 미검증.)
