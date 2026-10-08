# 노드 소유 해체 — 전체 경로 레드팀과 국소 갱신

상태: **한정 모델 수정·추출 실행 검증 완료; 제품 구현 OPEN**.
기준 HEAD: `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, dirty main.
사용자가 해체 모델 전체를 다시 검토하고 가능한 개선·성능 검사를 요청했다.
소유권/DX self-host hold는 해제하지 않았다. 컴파일러, 런타임, 설치본,
SoT 상태는 바꾸지 않았고 커밋·푸시·외부 전달도 하지 않았다.

계획: [작업 지시서](../agent_work_directives/ownership_teardown_redteam_locality_2026-10-08.md).
의미 주인: [OwnershipTeardown.v](../semantics/proofs/OwnershipTeardown.v).
[문서 28](../semantics/28_memory_boundary_composition.md)의 단일 소유·접근·은퇴
요구를 새 모델이나 사용자 표기로 대체하지 않았다. 작성 스킬의 DX 원칙에 따라
검증·색인 전략을 사용자 코드가 수동으로 선택하거나 증명하게 하지 않는다.

## 조사한 전체 사슬과 판정

`RootNew/Alloc -> SetField/Attach -> Release/RootDrop -> 세대·저장 링크·두 색인
-> 임의 길이 실행의 불변식·재사용 -> RC 비교와 빈 힙 실행 예제`를 모두 조사했다.
변경한 결정의 forest/RC 호출자와 마지막 증명 소비자를 함께 이전했다.

| 항목 | 판정과 조치 |
|---|---|
| 해제 목록 중복 | 이전의 정확한 **집합** 조건은 `[x;x]`도 허용했다. 함수형 `phase2`는 중복 횟수만큼 free하지 않으므로 실제 이중 해제 버그를 증명한 것은 아니다. 그러나 한 번씩 방문하는 물리 해제 목록으로 정련하려면 부족하다. 두 은퇴 단계에 `NoDup`을 요구하고, 정확한 고유 목록의 존재와 중복 거부를 증명했다. |
| 링크 갱신의 무관한 행 필터 | 실제 이전 링크의 목적지 행만 제거 대상으로 삼았다. 정확한 역색인 아래 기존 스캔 명세와 목록까지 동일함을 증명했다. |
| 부모 이동의 무관한 행 필터 | 실제 이전 부모/루트의 자식 행만 제거 대상으로 삼았다. 정확한 자식 색인 아래 스캔 명세와 동일함을 증명했다. |
| `row ++ []` 숨은 복사 | 무관한 링크 행은 그대로 반환하도록 수정했다. 추출된 코드에서 물리 목록 공유까지 검사하고, 값은 같지만 복사하는 변형을 검사기가 잡는 것도 확인했다. |
| 해제 권한·활성 loan/pin | forest 단계에는 이 사실이 없다. 읽기 링크만으로 추상 Release가 가능한 반례를 영구 소비자에 남겼다. 문서 28의 `RetireValid` 발급·합성이 구현될 때까지 OPEN이다. |
| 재선언된 루트 ID | 렉시컬·비탈출 전제는 유지된다. 같은 ID의 옛/새 RootDrop을 모델이 구별하지 못하는 실행을 영구 소비자에 남겼다. 탈출 가능한 권한에는 루트 세대/비재발급과 arena-domain 결합이 필요하다. |
| 저장 링크와 지역 복사 | 저장 필드가 비워져도 저장해 둔 지역 Link는 갱신되지 않는다. 옛 핸들은 계속 거부되지만 실제 접근의 resolve/loan 합성은 아직 필요하다. |
| finalizer·동시성 | Step은 원자적 추상 전이이다. 두 단계 사이 재연결 반례가 남는다. 실제 은퇴 상태, 대여·pin, 재진입·동기화와 물리 해제의 합성은 OPEN이다. |

`index_set_scan_spec`과 `kids_move_scan_spec`은 같은 주인 안의 비교 명세이다.
forest/RC 전이는 새 국소 변환만 사용한다. 명세를 실행 fallback으로 고르거나,
잘못된 색인을 추측하여 복구하지 않는다. 잘못된 색인은 `IndexExact/KidsExact`
전제를 충족하지 못한다. 순수 `teardown` 함수 자체가 그 전제를 검사한다는 뜻은 아니다.

## 관측한 검증

- 집중 게이트: `tests/ownership_teardown_redteam_smoke.sh`, rc=0.
  fresh Rocq 9.3.0 / `rocqchk` 3개 모듈, **가정 0개**, admit/unsafe kernel 없음.
- 전체 통합: `tests/formal_semantics_smoke.sh`, rc=0.
  명시 inventory 64개와 독립 소비자 7개를 **동일한 fresh snapshot에서 71개**
  커널 검사했다. 승인 export/binding 소비자도 통과했다. 기존 Slot의 두 승인
  추상화만 남고 추가 가정은 없다. 검사한 71개 해시가 현재 소스와 모두 일치한다.
- 실제 모델 함수의 fresh OCaml 추출: 4,096개 유한 필드 상태에서 링크 갱신
  **98,304건**, 해체 **16,384건**, 부모 이동 16건, 목록 고유성 4개 대조군이
  독립 값 oracle과 일치했다. 해체는 세대 증가·노드 생존·남은 필드·두 색인·
  외부 빈 칸을 검사한다. 순수 변환 관측이지 production Step admission 구현이 아니다.
- 영구 typed 거부: 중복/부분 은퇴 목록, 자기/자손 아래 붙이기, 미선언 루트,
  재사용된 subject/parent/저장 링크, 누락·오배정된 두 색인. 기존 소유자의
  임의 단계 불변식, 프레임·재사용·순환 링크/RC/재진입 반례도 재검사했다.
- 고유성, 조상 검사, 역색인 제거, 자식 색인 제거를 약화한 독립 복사본 4개는
  증명 검사에서 거부됐다. 고유성 변형은 양의 생성 예제도 수정하여 **고유성
  정리**까지 도달한 뒤 실패하게 했다. 별도로 값 동일 복사 변형 1개는
  `unrelated field row copied`로 거부됐다. 알 수 없는 CLI 인자도 거부한다.
  타임아웃, 컴파일러 부재, 다른 레인의 프로세스 종료는 성공으로 세지 않는다.
- 커널 게이트 자체 음성 검사도 통과했다: planted admit, 동일 이름의 승인
  타입 변형, 빈/누락 승인 모듈을 실제 게이트가 거부했다.
- 문서 품질, evidence-lifecycle owner/inventory 연결, Bash 문법, Make 진입점
  해석, formal CI YAML/관측물 연결, 범위 내 UTF-8 14개 파일과 로컬 링크,
  추적 파일의 diff whitespace 및 새 파일의 trailing whitespace 검사를 통과했다.

추출 출력 디렉터리 안내와 기존 corpus의 module masking, nested-list scheme,
deprecated notation, loadpath 경고 및 theory dependency 보고는 로그에 보존했다.
조용히 지우거나 가정 0개로 전체 corpus를 설명하지 않는다.

## 성능 — 기존 스캔 명세와 같은 입력

환경: WSL2 Linux, OCaml 4.14.1 `ocamlopt -O2`, Rocq 9.3.0 / Stdlib 9.2.0.
512/4,096/16,384 노드, 고정 입력·출력 oracle, 각 64회 반복한 5표본의 중앙값.
각 표본 전 `Gc.full_major`; `Sys.time` CPU와 `Gc.allocated_bytes`를 관측했다.
두 구현 모두 같은 전체 행 projection/목록 길이 관측을 수행한다. 아래 시간·
할당은 **64회 합계**이며, 5개 원표본은 `cost.jsonl`에 함께 저장된다.
이것은 OCaml 모델 비용이다. C/LLVM allocator, 실제 GUI, GC와의 비교가 아니다.

16,384 노드의 최종 관측:

| 입력 | 필터 방문: 스캔 → 국소 | CPU: 스캔 → 국소 | 할당 byte: 스캔 → 국소 |
|---|---:|---:|---:|
| 분산 링크 | 16,384 → 1 | 35.442 → 13.146 ms (2.70×) | 117,444,704 → 18,016 |
| 이전 목적지에 링크 집중 | 16,384 → 16,384 | 56.103 → 36.455 ms (1.54×) | 117,444,704 → 50,345,056 |
| 새 목적지에 링크 집중 | 16,384 → 1 | 58.043 → 18.563 ms (3.13×) | 117,444,704 → 25,180,768 |
| 분산 부모/루트 | 32,768 → 1 | 42.551 → 20.281 ms (2.10×) | 151,002,208 → 33,565,792 |

**상수 시간 갱신이라는 결론은 금지한다.** 이전 행이 크면 필터가 그 행 전부를
읽고, 새 행이 크면 기존 순서 보존을 위한 append가 그 행을 복사한다. 새 목적지
집중 입력의 append 방문은 16,383이다. 표의 필터 수는 append 비용을 포함하지
않으며, 실제 CPU/할당 관측은 이를 포함한다. 함수형 행을 반복 projection하는
표현, 목록 길이 oracle, 부모 key 관측도 비용에 들어간다. 파괴적 색인 구현이나
전체 해체 비용 상한을 증명한 것이 아니다.

추출된 `unit_unique`는 유한 admission certificate 관측기로만 추가했다.
4,096개의 고유 목록에서 membership 비교 8,386,560회, CPU 69.800 ms가
관측됐다. 이 목록 방식은 O(n²)이므로 **production subtree issuer로 선택하지
않았다**. `unit_from_kids/root_unit_from_kids`는 정확한 색인의 inductive
reachability와 unit의 일치를 증명하고, `enum_below`는 bound 아래 고유 witness를
준다. 이는 실행 가능한 indexed walker나 그 복잡도 증명이 아니다.

## 다음 실제 정련 경계

1. 실제 루트/forest/Slot의 권한·footprint·arena-domain 발급자를 하나로 연결하고,
   loan/pin 또는 잘못된 권한이면 파괴 전에 거부할 것.
2. exact/unique unit을 생산하는 indexed walker와 은퇴 목록의 한 번 방문을
   실행 알고리즘으로 증명할 것. 목록 중복 검사를 해제마다 재증명하는 기본값은 피한다.
3. 유한 세대 고갈, checked local access, 재진입/동시성, 실제 색인 저장소와
   해제 순서·물리 배치·성장·copy-before-free·실패 원자성을 기존 주인들과 합성할 것.

이 단계들은 별도 구현 결정을 요구하는 OPEN 의무이다. 현재 모델 변경을
언어 전체 메모리 안전, executable CLOSED, self-host 대체나 remote CI green으로
기록하지 않는다. formal CI에 집중 실행과 관측물 보존을 연결했지만 원격 실행은 하지 않았다.

## 재현과 파일 식별

```bash
OPAMROOT=/home/c/.local/share/pergyra-rocq/opam \
  timeout 300s bash scripts/run_rocq_toolchain.sh \
  bash tests/ownership_teardown_redteam_smoke.sh
OPAMROOT=/home/c/.local/share/pergyra-rocq/opam \
  timeout 1800s bash scripts/run_rocq_toolchain.sh \
  bash tests/formal_semantics_smoke.sh
```

관측물: `.tmp/ownership-teardown-improvement-2026-10-08/`의 `formal.log`,
`kernel-extraction.log`, `kernel-selftest.log`, `controls.log`, `cost.jsonl`,
`cost-scope.log`, `environment.log`, `mutation-*.log`, fresh `extracted/`.
Make 진입점: `ownership-teardown-redteam-test-smoke`.

최종 SHA-256:

- 소유 모델: `d633dfcd7e71417d8a6ba0d4702f532988eb839d0da7d93f41b599568f78f33a`
- typed 레드팀: `e1fa6d271c2bbb34fa048c9dd73c06f939846b64155717c01eb0c080aa90e611`
- 추출 계약: `e43399ea0b988559402f74b1557e05a7efc18dddd926d65e5c8e4bd591e9ce7e`
- 유한 observer: `1414d8bca18fcb952e286c7ed2b18d3c90a797b56764b8875f1c367c07cb673c`
- 집중 게이트: `172c4a65295053dd7fe147472b50bdb46931e1070f0221cd4f4287ddfb8340e9`

원본 `6b8e1711...` 검토와 이 감사의 해시는 각각 당시 판의 검증이다.
다음 판은 [루트 세대·지역 읽기 감사](ownership_teardown_root_epoch_2026-10-08.md)가
식별한다. 이 감사 시점의 Windows Git 관측은 dirty 항목 303개(시작 300개), index 0개다.
WSL Git의 별도 상태 관측은 358개로 달라 그 수를 같은 상태 census로 합치지
않았다. 공통 HEAD와 빈 index, 검사한 71개 소스 해시는 직접 확인했다.
기존 공유 dirty 변경을 보존했고 기존 공유 파일·프로세스를 정리하지 않았다.
