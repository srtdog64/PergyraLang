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

## 남은 것 (이번에 고치지 않음)

- `examples/pattern_library_basics`가 LLVM에서 `LLVM member access requires concrete receiver
  type metadata`로 실패한다. 수정 전 빌드에서도 같다. 지금까지는 앞선 예제의 LLVM 실패에 가려
  드러나지 않았다. CI 예제 게이트는 C만 돌려서 보이지 않는다.
- `docs/01_intent_first_design.md`는 `irreversible` 절을 요구하지만 파서에 없다.
- 파일 맨 앞 UTF-8 BOM이 모든 import 줄을 "parser token stream anchor changed during parse"로
  떨어뜨린다. 원인을 짚지 못하는 메시지다.
- world 밖으로 zone을 꺼내는 오류의 위치가 `0:0`으로 나온다.
- `guard`라는 이름이 사후 검사를 뜻하는 것은 사용자 기대와 다르다. 이름을 바꿀지는 결정 사항이다.
- 분리 블록 안에서 `spawn`한 작업을 `await`하면, 블록과 `Main`의 남은 문장 사이 순서가
  백엔드마다 다르다. C에서는 `Main`이 먼저 끝나고, LLVM에서는 블록이 먼저 끝난다. 언어가
  이 순서를 정하지 않으니 둘 다 허용되는 실행이다. 다만 lane 계획이 같으면 실행도 같아야
  한다는 실행자 불변성(`docs/146`)에 비추어, 두 백엔드의 코루틴 안 `await` 경로
  (`pgy_await_take` 대 `pgy_await_export`)를 조사할 가치가 있다.
  `async_block_detached_drain`은 이 순서에 기대지 않는다.
