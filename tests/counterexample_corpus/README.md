# 반례 코퍼스 (counterexample corpus)

Pergyra는 주류 범용 언어가 되려는 것이 아니라, 다른 언어가 그대로 받아들이는 버그 부류의
반례가 되려 한다. 이 코퍼스는 `docs/198_market_safety_positioning.md`의 "필요한 bug corpus"
16개 부류를 같은 이야기(다중 테넌트 서비스 AgentHub에서 AI 에이전트가 테넌트 `acme`,
`globex`를 대신해 도구를 호출한다)로 쓰고, Pergyra와 TypeScript, C#, Python, Rust를 나란히
놓는다.

Pergyra가 잡지 못하는 부류도 빼지 않는다. 그런 행은 `ACCEPTED_GAP`이나 `PARTIAL`로 남고,
무엇이 빠졌는지 근거 위치와 함께 적는다.

## 실행

```bash
PGY_BIN=.tmp/claude-bin/pgy.exe bash tests/counterexample_corpus_smoke.sh
```

- `PGY_BIN`에는 현재 C 소스로 빌드한 컴파일러를 준다. 기본값은 `bin/pgy`이다.
- TypeScript(`tsc`, `node`), C#(`dotnet`, .NET 10의 파일 하나짜리 실행), Python, Rust(`rustc`, 표준 라이브러리만)가 있으면 함께 돌린다. Rust는 `RUSTC`, PATH, `~/.cargo/bin` 순서로 찾는다. 없는 도구는 통과가 아니라 `SKIP`으로 찍는다. `PGY_CORPUS_OTHERS=0`이면 다른 언어를 돌리지 않는다.
- 데모(아래)도 함께 돈다. `PGY_CORPUS_DEMO=0`이면 뺀다.
- 수동 게이트다(`tests/manual_gate_inventory.tsv`). 이 기계에서 약 3분 걸린다.

## 사례 구성

`NN_<부류>/` 디렉터리마다 다음 파일이 있다.

| 파일 | 내용 |
| --- | --- |
| `pergyra_bug.pgy` | Pergyra로 쓴 버그 |
| `pergyra_fixed.pgy`, `pergyra_fixed.expected` | 고친 프로그램과 그 출력 |
| `typescript.ts`, `csharp.cs`, `python.py`, `rust.rs` | 같은 버그. 깨끗하게 빌드되고(TypeScript는 `--strict`), 버그가 일어나는 순간 `BUG_MANIFESTED NN`을 한 줄 찍는다. 그 언어가 자연스러운 코드를 거부하면(Rust의 빌림 검사기) 거부 자체가 관찰이고, 게이트는 기록된 오류 코드로 실패하는지 확인한다 |

`manifest.tsv`(schema `pgy.counterexample.v2`)가 행마다 Pergyra의 판정을 고정한다.

| `pergyra_expect` | 뜻 | 게이트가 확인하는 것 |
| --- | --- | --- |
| `REJECT_COMPILE` | 코드가 생기기 전에 거부 | rc=1, MIR 없음, 기록된 코드나 메시지 |
| `TRAP_RUNTIME` | 빌드되고, 실행 중 멈춤 | 기록된 환경에서 0이 아닌 종료와 `class=` |
| `RUNTIME_SAFE` | 빌드되고 실행되며, 런타임 장치가 상태를 지킴 | 출력이 `pergyra_bug.expected`와 같음 |
| `ACCEPTED_GAP` | Pergyra가 버그를 받아들임 | 여전히 빌드됨. 거부하기 시작하면 실패시켜 행을 고치게 함 |

`pergyra_level`은 그 부류 전체에 대한 판정이다. 버그 프로그램 하나가 거부되더라도, 같은
부류의 다른 모양이 통과하면 `PARTIAL`이다.

## 결과 (2026-10-12, C 백엔드)

| # | 부류 | Pergyra | TypeScript | C# | Python | Rust |
| --- | --- | --- | --- | --- | --- | --- |
| 01 | 잘못된 테넌트 | PARTIAL: 컴파일 거부 (테넌트별 zone 타입일 때만) | 통과, 런타임 버그 | 통과 | 통과 | 통과 (테넌트를 타입으로 두면 거부) |
| 02 | 소유권 누락 | PARTIAL: intent step·zone 규칙만 컴파일 거부 | 통과 | 통과 | 통과 | 통과 (capability 토큰 설계면 거부) |
| 03 | confused deputy | PARTIAL: 모델링한 모양만 컴파일 거부 | 통과 | 통과 | 통과 | 통과 (object-capability 설계면 거부) |
| 04 | 과도한 에이전트 권한 | PARTIAL: `with caps`를 쓴 곳만 컴파일 거부 | 통과 (Node `--permission`은 선택) | 통과 | 통과 | 통과 (`std::fs`는 앰비언트 권한) |
| 05 | 고아 task | PARTIAL: 이름 붙은 `spawn` 컴파일 거부 | 통과 | 경고 CS4014, 런타임 버그 | 통과 | 통과 (경고 없음) |
| 06 | 취소 누수 | COMPILE | 통과 | 통과 | 통과 | 경고 unused_variables뿐 |
| 07 | 채널 데드락 | NONE: 받아들임 (벽시계 감시로만 멈춤) | 통과 | 통과 | 통과 | 통과 |
| 08 | 스코프 밖 자원 | COMPILE | 통과 (조용히 실패) | 런타임 예외 | 런타임 예외 | **거부 E0382** (지역 소유 핸들일 때) |
| 09 | spawn 폭주 | RUNTIME: 호스트 예산으로 멈춤 | 통과 | 통과 | 통과 | 통과 |
| 10 | 할당 폭주 | PARTIAL: 호스트 예산으로 멈춤, 문자열은 계산 안 됨 | 통과 | 통과 | 통과 | 통과 |
| 11 | 호스트 호출 증폭 | NONE: 받아들임 | 통과 | 통과 | 통과 | 통과 |
| 12 | 외부 API 비용 폭탄 | NONE: 받아들임 | 통과 | 통과 | 통과 | 통과 |
| 13 | 잘못된 단계 순서 | PARTIAL: 함수 안 지역 객체만 컴파일 거부 | 통과 | 통과 | 통과 | 통과 (typestate 설계면 거부) |
| 14 | 부분 commit | RUNTIME: 보상이 상태 복원 | 통과 | 통과 | 통과 | 통과 |
| 15 | 보상 누락 | COMPILE | 통과 | 통과 | 통과 | 통과 |
| 16 | stale zone 변경 | PARTIAL: 탈출 컴파일 거부, 런타임은 경고만 | 통과 | 통과 | 통과 | **거부 E0499** (`Rc<RefCell>`·인덱스 핸들이면 통과) |

- **Pergyra**: 컴파일 거부 10, 런타임 차단 2, 런타임 보존 1, 받아들임 3. 부류 전체로는 COMPILE 3, PARTIAL 8, RUNTIME 2, NONE 3이다.
- **TypeScript, C#, Python**: 16개 부류 모두 빌드 단계에서 아무것도 막지 않았다. 컴파일러 반응은 C#의 경고 하나(05의 CS4014)뿐이다. 08에서 C#과 Python은 런타임 예외를 내고, TypeScript는 조용히 실패한다.
- **Rust**: 자연스러운 코드에서 둘을 빌림 검사기로 거부한다. 08(닫은 핸들 사용, E0382)과 16(옛 zone 참조를 쥔 채 회전, E0499)이다. 06에서는 일반 경고(unused_variables) 하나만 낸다. 나머지 13개는 통과한다. 01, 02, 03, 13은 테넌트 타입, capability 토큰, typestate 같은 설계를 택하면 거부할 수 있지만, 기본으로 쓰는 코드는 통과한다.

## Rust와 겹치는 곳, 겹치지 않는 곳

- **겹치는 곳: 메모리와 별칭.** 08과 16은 Rust의 빌림 검사기가 Pergyra와 같은 지점을 막는다. 둘 다
  범위가 있다. 08은 핸들이 지역 소유일 때만이고, 오래 사는 세션 안의 `Option`이면 런타임 검사로
  바뀐다. 16은 참조를 쥐고 있을 때만이고, `Rc<RefCell>`이나 인덱스 핸들로 쓰면 통과한다.
- **설계로만 겹치는 곳: 권한과 상태 순서.** 01~03과 13은 Rust 타입 시스템으로도 표현할 수 있다.
  테넌트 phantom 타입은 E0308, capability 토큰은 E0603·E0616, typestate는 E0599로 막힌다. 다만 그
  설계를 사람이 골라야 하고, 기본 코드는 통과한다.
- **겹치지 않는 곳: 수명, 예산, 보상.** 고아 스레드(JoinHandle은 `#[must_use]`가 아님), 취소, 데드락,
  spawn·할당·호출·비용 예산, 부분 commit과 보상 누락은 Rust도 막지 않는다.
- **Pergyra의 차이**는 이 부류들이 사람이 고르는 설계가 아니라 언어의 기본 검사라는 점이다. 같은
  자리에서 Pergyra도 PARTIAL이 많다. 이 비교를 근거로 "Rust보다 안전하다"고 말해서는 안 된다.

## 데모: `demo_agenthub/`

`agenthub.pgy`는 에이전트 한 턴이다. 검색 도구를 기다리고, 도구 실행을 계획한 뒤 실행하고,
쓰기 권한만 가진 답장 도구로 답하고, 청구와 장부 기록을 하나의 intent로 묶는다.

`demo.sh`는 이 파일을 그대로 실행한 뒤, 흔한 실수 다섯 가지를 실행할 때 sed로 하나씩 넣어
다시 빌드한다. 실수가 원본과 어긋날 수 없다.

| 실수 | Pergyra | TypeScript (`--strict`) |
| --- | --- | --- |
| acme 에이전트가 globex에 청구 | `PGY_SEM_INTENT_STEP_INVALID` … `resolves to non-authority slot 'agent' in zone 'GlobexTenant'` | 받아들임, globex 470으로 청구됨 |
| 답장 도구가 시계를 읽음 | `PGY_SEM_EFFECT_CONFLICT` … `missing declared capabilities: clock` | 받아들임, 실행됨 |
| 검색 도구 호출을 기다리지 않음 | `PGY_SEM_TASK_LIFECYCLE` … `'search' is not retired` | 받아들임, 결과가 답장 뒤에 도착 |
| 청구에 보상이 없음 | `PGY_SEM_INTENT_STEP_INVALID` … `no compensation under 'rollback: full'` | 받아들임, 청구가 남음 (40) |
| 계획 없이 실행 | `Lifecycle violation: 'Execute' on 'run' is not permitted in state 'Pending'` | 받아들임, 계획 없이 실행 |

런타임에서는 호스트 한도 두 개가 지켜진다. `PGY_CAP_GRANT=clock`(쓰기 권한 없음)이면 청구가
일어나기 전에 `class=capability-denied`로 멈추고, `PGY_BUDGET_SPAWN_COUNT=0`이면
`class=budget-exceeded`로 멈춘다.

한계도 같이 적는다.
- 잘못된 테넌트 실수가 거부되는 것은 테넌트마다 zone 타입을 따로 뒀기 때문이다. 한 zone 타입의 두 인스턴스였다면 통과한다(01).
- 테넌트 zone 두 개는 "zone-first shape" 스타일 경고를 낸다. 권한 슬롯은 subject여야 하는데, 같은 모양에 컴파일러가 스타일 경고를 내는 작은 DX 마찰이다.

## 이 코퍼스가 드러낸 Pergyra의 구멍

각 항목이 반례를 하나 더 만들 수 있는 다음 일이다. 근거는 `manifest.tsv`의 `gap_note`에 있다.

- **권한**
  - 한 zone 타입의 두 인스턴스를 구분하지 못한다(01).
  - `authorized by`는 정적 귀속일 뿐이고 런타임 승인이 없다. 일반 함수의 action 호출은 검사하지 않는다(02).
  - 위임 출처가 없다(03).
  - `with caps`를 쓰지 않은 함수는 무제한이다. 과다 선언은 빌드 때 알리지 않고, 네트워크를 지닌 빌트인이 없다(04).
- **수명**
  - 지역 변수를 캡처하지 않는 분리형 `async {}`가 빠져나간다(05).
  - 취소는 협조적이고 취소 트리가 불완전하다(06).
  - 데드락 탐지가 없다(07).
- **예산**
  - 예산이 모두 호스트 opt-in이고 소스 수준 예산이 없다(09).
  - `List<String>`과 `StringConcat` 할당은 예산에 계산되지 않는다(10).
  - 호스트 호출 예산과 네트워크·비용 예산이 없다(11, 12).
- **의도**
  - 생명주기 검사가 함수 인자로 넘어온 객체를 보지 않는다(13).
  - 보상은 원자성이 아니다(14).
  - rollback을 선언하지 않은 intent는 보상 의무가 없다(15).
  - stale zone 읽기는 런타임에 경고만 낸다(16).

## 측정하지 않은 것

- LLVM 백엔드(사례는 C 백엔드로만 검증)
- 의례 줄 수 비교와 성능 비용(`docs/198`의 비교 항목 중 아직 숫자가 없는 것)

## 유지 규칙

- 행 수는 16으로 고정한다. 부류를 더하려면 게이트의 행 수와 이 문서를 함께 바꾼다.
- `ACCEPTED_GAP` 행의 버그를 Pergyra가 거부하게 되면 게이트가 실패한다. 그때 행을 새 판정으로 고치고 `gap_note`를 지운다.
- 다른 언어의 프로그램은 유능한 개발자가 쓸 법한 관용적인 코드로 둔다. 일부러 허술하게 쓴 허수아비는 반례가 되지 않는다.
