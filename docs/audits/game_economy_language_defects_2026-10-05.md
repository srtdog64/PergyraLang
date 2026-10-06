# 게임 경제 예제가 드러낸 언어 결함과 수정

Delivery base: `8b1f17ec`.
Status: 읽기 전용 탐색 증거다. 컴파일러 의미론의 권위가 아니다. 의미는
`docs/grammar/01_syntax.md`의 intent·zone/world·타입 별칭 절과 아래 게이트가 소유한다.

`examples/game_economy_demo`를 intent-first 길드 경제로 다시 쓰는 동안 보존 법칙 감사와
두 백엔드 비교가 언어 쪽 결함을 드러냈다. 처음에는 예제가 결함을 피해 가게 짰다. 사용자
지시("예제는 피해가는 게 아니라 언어가 잘못되면 고쳐야 한다")에 따라 컴파일러를 고치고
예제를 우회 없는 형태로 되돌렸다.

## 수정한 결함

| # | 증상 | 원인 | 수정 | 게이트 |
|---|---|---|---|---|
| 1 | 실패한 거래의 환불이 원본 지갑에 닿지 않고 사라진다. 진단 없음. C·LLVM 동일 | `pre`/`invariant-pre`와 중첩 intent 실패가 alias를 zone slot에 재바인딩한 채 cleanup으로 갔다. 보상 tail은 재바인딩 없이 compensate를 실행하고 원본을 slot 위에 다시 복사했다 | 승인 검사를 materialize와 재바인딩 사이로 옮겼다. 중첩 intent의 성패는 되쓰기 뒤에 판정한다. 보상은 되돌리는 step의 바인딩 아래에서 돈다(materialize → 재바인딩 → compensate → sync → 되쓰기) | `tests/cases/backend_compare/intent_compensation_canonical_writeback` (수정 전: gold 40, 10, -30, -50, -100) |
| 2 | world 메서드의 subject 매개변수: 의미 검사는 통과하고 C는 인자 형 불일치, LLVM은 verify 실패 | 호출 쪽이 subject를 값으로 넘겼다. C hosted self-call 경로와 LLVM의 hosted·member·boundary 경로가 지역 식별자만 주소로 바꿨다 | C hosted self-call이 subject 인자를 주소로 넘긴다. LLVM은 `llvm_subject_argument_address` 하나로 지역·암묵적 host 필드·멤버 경로의 주소를 구하고, 주소가 없으면 진단으로 멈춘다 | `world_method_subject_param`. `shopping_mall_checkout_refund`가 LLVM에서 처음으로 컴파일된다 |
| 3 | LLVM에서 zone shared `Array`의 원소를 바꿀 수 없다 | 배열 대입·`ArraySet` 수신자가 지역 변수만 찾았다 | 이름만 쓴 host 필드를 `llvm_implicit_host_field_ptr`로 찾는다 | `zone_shared_array_mutation` |
| 4 | `type Gold = Int`: C는 루프 phi 형 충돌, LLVM은 `Result<Gold, E>`를 못 풀고, `Array<Gold>`는 두 백엔드에서 `Array<Int>`와 다른 구조체가 됐다 | 별칭이 투명한데 백엔드가 형 이름 문자열을 그대로 특수화 이름에 썼다 | 각 백엔드의 중심 입구에서 별칭을 토큰 단위로 푼다. C: C 형 요구·형 렌더링·특수화 등록·식 형 추론·phi·전방 선언 단계. LLVM: `pergyra_type_to_llvm`. 별칭이 없는 프로그램은 헤더를 한 번 센 뒤 건너뛴다 | `type_alias_transparent_flow`, 기존 `type_alias_array_context` |
| 5 | 분리된 `async { }` 블록이 멈추면 남은 일이 종료와 함께 사라진다(LLVM). 블록 안 `spawn`은 C에서 함수가 중첩되어 컴파일되지 않는다 | 이벤트 루프가 없어 누구도 남은 코루틴을 돌리지 않았다. 래퍼 본문을 쓰는 중에 안쪽 래퍼가 같은 버퍼에 끼어들었다 | 생성된 `main`이 `Main()` 뒤에 `pgy_async_drain_detached`를 부르고, 끝내지 못하면 `INVALID_LIFECYCLE_STATE` panic. 래퍼 본문 동안 파일 범위 정의는 대기 버퍼에 모았다가 닫은 뒤 붙인다 | `async_block_detached_drain` |
| 6 | `examples/pattern_library_basics`가 LLVM에서 `LLVM member access requires concrete receiver type metadata`로 실패한다 | LLVM lambda가 struct 형 매개변수를 scope에만 선언하고 class 바인딩을 등록하지 않아 `ctx.morale`의 수신자 형을 몰랐다 | 매개변수 형을 정하는 owner를 `llvm_stmt_lambda_param_type_node` 하나로 모았다(주석, 기대 callable 형, 같은 매개변수를 돌려주는 반환 형). LLVM 시그니처와 class 바인딩(`llvm_register_typed_var_binding`)이 둘 다 이 노드를 읽는다. 캡처 closure 경로도 같다 | `lambda_param_member_access`. 예제 출력이 C와 같다 |
| 7 | 파일 맨 앞 UTF-8 BOM이 모든 import 줄을 "parser token stream anchor changed during parse"로 떨어뜨린다 | 렉서가 BOM 세 바이트를 unexpected character로 읽었다. 그런데 오류 토큰은 stream anchor·ordinal을 초기화하지 않은 쓰레기 값을 가져서, 파서가 렉서 진단 대신 anchor 오류를 냈다 | native 렉서와 self-host 렉서가 맨 앞 BOM을 건너뛴다(위치는 byte offset 유지). 오류 토큰은 다른 토큰과 같은 stream에 속한다 | `source_utf8_bom`, `make test-parser`의 lexer anchor 두 경우 |
| 8 | world 밖으로 zone을 꺼내는 오류의 위치가 `0:0`이다. `a.b`, `a[i]`, 메서드 호출 `a.M()`의 진단도 모두 같다 | 파서가 member·index access 노드에 위치를 주지 않았다. 호출 노드는 callee 위치를 복사하므로 메서드 호출도 `0:0`이 됐다 | member access는 멤버 이름 토큰, index access는 `[` 토큰의 위치를 갖는다 | `make test-semantic`의 member location 두 경우(`11:28`, `6:20`) |
| 9 | `docs/01`·`docs/173`이 요구하는 `irreversible` 절이 파서에 없고, 보상 커버리지(INT-2) 검사도 없다 | 설계만 있고 구현이 없었다 | `irreversible: "이유";` 절과 커버리지 검사를 native에 넣었다. 의무는 full rollback을 주장하는 intent에만 생긴다(`docs/173` WO-INT-2 착지 메모에 측정 근거). 어휘 레지스트리 147행. self-host parser는 `surface_not_covered`로 거절한다 | `intent_irreversible_rollback`, `make test-semantic`의 coverage 표 10경우 |
| 10 | 분리 블록 안에서 worker 작업을 `await`할 때 블록과 `Main`의 남은 문장 사이 순서가 실행마다 달랐다. 처음 보고에서 "백엔드 차이"라고 한 것은 틀렸다. 같은 C 바이너리도 순서가 바뀌었다 | 코루틴 안 `await`가 worker 작업의 완료 여부를 먼저 봤다. worker가 이겼으면 블록이 그대로 이어서 실행됐고, 졌으면 양보했다 | 코루틴은 worker 작업을 기다릴 때 항상 한 번 양보한 뒤 결과를 읽는다 | `async_block_await_worker_order` (수정 전 LLVM: `detached done`이 `main end`보다 먼저) |
| 11 | `examples/order_analytics`가 LLVM에서 `LLVM collection operation 'ListSize' requires an identifier receiver`로 실패한다. #6을 고치자 예제 게이트의 LLVM 경로가 여기서 멈췄다 | LLVM 컬렉션 연산이 수신자로 지역 이름만 받았다(`ListSize(batch.orders)`) | 필드 수신자는 그 필드의 저장소 주소(`llvm_emit_member_lvalue_ptr`)를 쓴다. 원소 형이 필요한 연산은 여전히 이름으로 원소 형을 찾으므로, 필드에서는 원소 메타데이터 거절로 멈춘다 | `collection_field_receiver_size` |
| 12 | `examples/fsm_factory`가 LLVM에서 `sealed=0`을 낸다(C는 `sealed=30`). 진단 없이 틀린 값이다. #11 뒤에 가려져 있었다 | zone 메서드 안의 맨 이름이 shared 필드와 같으면 LLVM은 항상 필드로 풀었다(필드의 낡은 SSA 사본을 피하려던 규칙). 그래서 `let sealed = ...; self.sealed = self.sealed + sealed;`가 필드를 자기 자신에 더했다 | 의미 단계가 식별자마다 기록한 바인딩(`binding_syntax_id`, `is_host_field`)이 지역 변수나 매개변수를 가리키면, 읽기와 대입 모두 그 지역 변수를 쓴다(`llvm_identifier_may_denote_host_field`). 바인딩 기록이 없는 이름만 예전처럼 필드로 푼다 | `host_field_local_shadow` (수정 전 LLVM: `total=0`, `local=8`) |

## 측정해서 문서로 옮긴 의미

`docs/grammar/01_syntax.md`에 적었다.

- intent step 절의 평가 순서: `pre → invariant → on → guard → expect → post → invariant`.
  `guard`는 사후 검사다.
- `pre:`는 step마다 하나. `on:`이 없는 step은 같은 이름의 action 계약을 찾는다.
- 권한을 선언한 zone의 step에는 `authorized by:`가 필요하다.
- 사후 검사에서 실패한 step도 자기 compensate로 되돌려진다.
- world/zone 메서드의 subject 매개변수는 slot 핸들이다. world의 zone은 world 밖으로 나갈
  수 없다. zone shared `Array`의 원소를 바꿀 수 있다.
- 타입 별칭은 투명하다. 상태 표에서 "Not Current Surface"를 "Supported but Evolving"으로 옮겼다.

#6, #11, #12로 예제 게이트(`tests/example_contract_smoke.sh`)가 C와 LLVM 양쪽에서
처음으로 끝까지 통과한다(`PGY_EXAMPLE_BACKENDS="c llvm"`). CI는 아직 C 경로만 돌린다.

## 남은 것

- `guard`라는 이름이 사후 검사를 뜻하는 것은 사용자 기대와 다르다. 코퍼스의
  `guard:` 31개 중 실패 경로 시험용 `guard: false` 7개를 뺀 24개의 다수(`guard: price > 0`, `guard: buyer.gold > 50`)가 사전 조건처럼
  쓰였다. 그런 step은 `on:`의 효과가 이미 일어난 뒤 실패한다. 이름을 바꿀지, 평가 위치를
  `on:` 앞으로 옮길지는 결정 사항이다. 바꾸면 실패 기록과 보상 동작이 달라진다.
- 형 주석 없는 람다 let: `let f = (x: Int) => x + 1;`은 C("cannot determine C type for
  MIR local")와 LLVM("missing source-local type metadata") 모두에서 컴파일 오류다. 캡처가
  있는 경우(`let g = (x: Int) => x + bonus;`)는 C만 통과하고 LLVM은 "missing closure
  callable metadata"로 실패한다. MIR source-local 형 fact가 람다 리터럴에서 callable
  시그니처를 만들지 않는다(`src/compiler/mir_source_local_types.c`). `let f: func(Int) -> Int = ...`
  처럼 형을 쓰면 두 백엔드에서 된다. 이번 테스트를 쓰다가 찾았고 고치지 않았다.
- self-host semantic에는 보상 커버리지 검사가 없다. 그래서 self-host parser가
  `irreversible:`을 거절한다. 같은 검사가 self-host에 생기면 거절을 풀 수 있다.
