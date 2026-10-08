# 컴파일러 메모리 압력 감사 — 3 GiB의 정체와 자동 정리 전환의 영향

날짜: 2026-10-08 KST. Base HEAD `3658548d24bca3d721e4f1974ac7a10da99f7aa8`
(공유 dirty main). 상태: **READ-ONLY 감사.** 코드, 게이트, 레지스트리, 설치본은
바꾸지 않았다. 이 문서는 컴파일러 의미론이나 진척을 소유하지 않는다. 계획은
[`ownership_cutover_plan_2026-10-08.md`](../agent_work_directives/ownership_cutover_plan_2026-10-08.md)
H절과 P0가 소유한다.

## 질문

사용자가 2026-10-08에 물은 네 가지다.

1. 컴파일러가 3 GiB를 쓰는 건 설계 자체가 잘못된 것인가. 다른 언어는 어느 정도인가.
2. 수동 은퇴(`CompilerRetireArrayStorage`, `ArrayDrop`, `ArrayDropOwnedStrings`)를
   자동 정리로 바꾸면 최고치가 지금보다 오르는가.
3. 저사양 기기에서는 쓸 수 없는가.
4. 이건 컴파일러 빌드에서만의 문제이고, 컴파일된 프로그램의 런타임에서는 안 그러면
   되는 것 아닌가.

## 결론

| 질문 | 판정 |
|---|---|
| 1. 설계 결함인가 | 언어 설계 결함의 증거는 없다. 같은 일을 하는 C oracle도 2.365 GiB를 쓰고, Zig의 자가 호스팅 컴파일러도 자기 빌드에 2.7 GiB를 썼다. 부채는 컴파일러 **파이프라인 구조**에 있다(§3.1). |
| 2. 전환 후 최고치 | **UNKNOWN.** 내리는 요인 하나와 올리는 요인 둘이 있다. 정리 시점이 마지막 사용 지점이고 이동이 기본이면 내려갈 것으로 보지만 측정 근거는 없다(§3.2). |
| 3. 저사양 | 보통 프로그램 컴파일은 오늘 측정에서 최고 31 MB 이하다. 컴파일러 자체 빌드는 4 GB 기기에서 어렵다. 155줄과 컴파일러 전체 사이는 측정값이 없다(§3.3). |
| 4. 빌드에서만? | **아니다.** 3 GiB를 쓰는 프로세스는 Pergyra로 컴파일된 프로그램이다. 같은 런타임의 누수는 사용자 프로그램에도 그대로 있다(§3.4). |

## 1. 측정

### 1.1 기존 고정 규모 기록

출처: [`self_host_completion_log.md`](../self_hosted/self_host_completion_log.md)
11620행 근처, [`91_build_troubleshooting.md`](../91_build_troubleshooting.md) 6638행 근처.

| 경로 | 시간 | 최고 private | 출력 |
|---|---|---|---|
| Pergyra로 빌드한 source→MIR producer | 74.077 s | 2.974 GiB | MIR 186,071,774 bytes |
| native oracle(C), 같은 입력 | 128.048 s | 2.365 GiB | 같은 SHA-256 `345DD2E3…59AE95` |

producer는 6,049 루틴과 14 intent를 처리했다. 오늘 줄 수(self-host `.pgy` 291,974줄,
원문 12,791,324 bytes) 기준으로 대략 줄당 10 KB다. 기록 당시 줄 수는 오늘과 달라서
이 비율은 대략치다.

### 1.2 2026-09-28 두 실행

출처: `.tmp/build-pressure/` 아래 `carrier-driver-source*` 파일(추적 안 되는
scratch). 측정은 `scripts/measure_build_pressure.ps1`(500 ms 간격, 프로세스 트리
private 합계). **summary에 명령과 입력 경로가 기록되지 않아서 입력은 UNKNOWN이다.**
stage 출력으로 보면 둘 다 source→C codegen 경로의 front end다.

| label | 최고 프로세스 | 종료 | 시간 | 최고 private | 마지막 stage |
|---|---|---|---|---|---|
| `carrier-driver-source` | `gen2.exe` | -1, 상한에서 정지 | 788.6 s | 3,078.4 MB (3.006 GiB) | `[semantic-body-type-stage] verdict:done` |
| `carrier-driver-source-opt` | `codegen_carrier_opt.exe` | 124, 900 s 시간 제한 | 900.8 s | 2,802.3 MB (2.737 GiB) | `[semantic-body-type-stage] verdict:start` |

`gen2.exe` 실행은 semantic 판정이 끝난 직후 상한에 걸렸다. **C 방출은 시작도 하지
않았다.** front end만으로 상한을 채운다.

### 1.3 단계별 증가 (`carrier-driver-source`, gen2)

stage 출력 시각에 가장 가까운 이전 sample의 private 값이다.

| 구간 | 시각 | private | 증가 |
|---|---|---|---|
| `source` | 0 → 29.5 s | 0 → 1,221 MB | **+1,221** |
| `semantic` | → 153.2 s | → 1,646 MB | +425 |
| `base-initializer` | 153.6 → 173.3 s | 1,684 → 1,819 MB | +135 |
| `call-targets`, `expression-places` | → 178.8 s | → 1,892 MB | +73 |
| `assignment` | → 203.3 s | → 1,964 MB | +72 |
| `statement` | → 279.6 s | → 2,659 MB | **+695** |
| `statement` 끝 → `named-boundary` 시작 | → 351.4 s | → 2,733 MB | +74 |
| `named-boundary`, `zone-carriage`, `generic` | → 434.7 s | → 2,795 MB | +62 |
| `verdict` | → 787.6 s | → 3,061 MB | +266 |
| 상한 정지 | 788.6 s | 3,078 MB | |

- `source` 단계 하나가 상한의 40%를 쓴다. 입력이 self-host 전체라면 원문
  12.2 MiB의 약 100배다(입력 UNKNOWN이라 조건부).
- semantic 단계 중에서는 `statement`가 가장 크다.
- `-opt` 실행도 같은 모양이다(`source` +1,174 MB, `statement` +659 MB).

### 1.4 곡선 모양

- `gen2` 917개 sample, `-opt` 1,042개 sample에서 20 MB 넘게 내려간 적이 **0번**이다.
  두 실행 모두 마지막 값이 최고값이다.
- 해석의 한계: Windows의 private bytes는 `free` 뒤에도 할당기가 페이지를 들고 있어서
  잘 내려가지 않는다. 그래서 이 곡선은 "거의 풀지 않는다"와 맞지만 그 증거는 아니다.
  살아 있는 바이트와 누적 할당 바이트를 가르려면 할당기 카운터가 필요하다(§4).

### 1.5 작은 프로그램 (오늘 측정)

- 실행 파일: `bin/pgy-self-driver.exe`, SHA-256
  `707dcd40049a1697a5827b2a7c8d3cf509573aa3c0031f2eee338c9fa0d78ec7`(10-01 빌드).
- 방법: `pgy-self-driver.exe <example> --emit-c-verified`를 PowerShell
  `System.Diagnostics.Process`로 실행하고 20 ms마다 `PeakWorkingSet64`와
  `PeakPagedMemorySize64`를 읽었다. 0.2초 안에 끝나는 실행은 마지막 sample을 놓칠 수
  있어서 값은 하한이다.
- `examples/*.pgy` 120개 중 67개 성공, 53개는 0이 아닌 종료(3–514줄, 곧바로 끝남).
  실패는 메모리와 무관해 보이고, 이 감사에서 원인을 조사하지 않았다.
- 성공한 67개: 3–155줄, 최고 private 6.7–30.9 MB(중앙값 7.5 MB), 각 0.2초 이하.
  가장 큰 셋은 `heap.pgy`(81줄, 30.9 MB), `deque.pgy`(40줄, 30.7 MB),
  `union_find.pgy`(60줄, 28.6 MB).
- 155줄과 컴파일러 전체(약 29만 줄) 사이에는 측정값이 없다. 선형으로 늘어난다는
  보장도 없다. 2026-08의 38.5 GB 사고는 이차 증가였다(identity row를 행마다 전체
  노드 수로 만든 것).

## 2. 다른 언어 (외부 자료, 2026-10-08 검색)

| 언어 | 대상 | 메모리 | 성격 |
|---|---|---|---|
| Zig | 컴파일러가 자기 자신을 빌드 | C++ 1세대 9.1 GiB → 자가 호스팅 2.7 GiB | 0.10 이전(약 2022) 업그레이드 가이드, 빌드 설정 미상 |
| Zig 0.14.1 | Gentoo 패키지 | 메모리 검사 4G | 패키징 최소치, 측정 아님 |
| Rust | 컴파일러 빌드 | RAM 8 GB 이상 권장 | 권장치, 측정 아님 |
| Go 1.24.1 | 패키지 하나(typescript-go `ast`) | 1,499 MiB, GC를 끄면 3,834 MiB | issue의 `time` 측정 |
| D (DMD) | 기본 모드 | 속도를 위해 GC 없이 돌고 메모리를 풀지 않는다. `-lowmem`이 GC를 켠다 | 2021 메일링 리스트 |
| Pergyra | 컴파일러 전체 source→MIR | 2.974 GiB(Pergyra 빌드), 2.365 GiB(C oracle) | §1.1 |

- Zig의 자가 호스팅 수치와 같은 규모다.
- Go에서 GC를 끈 경우가 지금 Pergyra 상황(풀지 않음)과 가장 닮았다. 같은 입력에서
  2.56배가 된다. 3 GiB 안에도 풀지 않아서 쌓인 몫이 상당할 것으로 **추정**하지만,
  Pergyra에서 그 몫을 잰 적은 없다.
- DMD 사례는 한 번 돌고 끝나는 컴파일러가 해제를 생략하는 선택이 실제로 쓰인다는
  근거다. 언어 런타임에 같은 선택을 허용한다는 근거는 아니다.

출처:
- [Zig Self-Hosted Compiler Upgrade Guide](https://codeberg.org/azhai/zig/wiki/Self-Hosted-Compiler-Upgrade-Guide)
- [Gentoo zig-0.14.1.ebuild](https://mirrors.sjtug.sjtu.edu.cn/gentoo/dev-lang/zig/zig-0.14.1.ebuild)
- [rustc-dev-guide: Prerequisites](https://rustc-dev-guide.rust-lang.org/building/prerequisites.html)
- [golang/go#73044](https://github.com/golang/go/issues/73044)
- [digitalmars-d, "Plan for D" (2021-05)](https://lists.puremagic.com/pipermail/digitalmars-d/2021-May/316173.html)

## 3. 판정

### 3.1 설계 결함인가

언어 의미론 때문이라는 증거는 없다. C로 짠 oracle이 같은 일에 2.365 GiB를 쓴다.
두 구현이 같은 파이프라인 구조를 공유하기 때문이다. 원인은 세 가지다.

1. **프로그램 전체를 한 프로세스에 올린다.** 분할 컴파일이 없어서 메모리가 프로그램
   전체 크기에 비례한다. Go는 패키지 단위로 컴파일해서 메모리가 패키지 크기에 묶인다.
   큰 사용자 프로젝트라면 이게 가장 먼저 벽이 된다. §1.3에서 `source` 단계 하나가
   1.2 GB를 쓰는 것도 이 구조와 맞다.
2. **단계 사이를 MIR JSON 문자열로 넘긴다.** 186 MB 문자열을 만들고 다시 파싱한다
   (AGENTS.md: route action 안의 stage는 MIR JSON 텍스트로 넘긴다).
3. **메모리를 풀지 않는다.** String, List, Set, Queue에는 일반 타입별 drop glue가
   없다. HashMap은 String 값 전용 경로(`pgy_map_drop_string`,
   `pgy_map_drop_string_value_raw_export`)에서 키와 값을 모두 푼다. 일반
   nominal/nested 값과 emitter 도달은 확인되지 않았다. 자동 정리 전환이 고칠
   부분이다. (2026-10-09 정정: 처음 판에는 "HashMap은 값을 풀지 않는다"고 잘못
   적었다. 근거는 `src/runtime/pgy_runtime_map_string_inline.h:178-204`,
   `pgy_runtime_lib_raw_map_exports.h:121-164`.)

1과 2는 전환이 착지한 뒤에 따로 정할 일이다. 지금 새 트랙으로 열지 않는다
(AGENTS.md "Closure-Blocking Optimization Policy").

### 3.2 자동 정리로 바꾸면 최고치가 오르는가

내리는 요인:
- 지금 풀지 않는 String, List, Set, Queue 값과, String 값 전용 경로 밖의 HashMap
  값을 푼다.

올리는 요인:
- **정리 시점.** 수동 은퇴 호출은 최고치를 깎으려고 정확히 그 자리에 놓았다
  (27 §1: `CompilerRetireArrayStorage` 212곳, `ArrayDrop` 148곳,
  `ArrayDropOwnedStrings` 87곳). 자동 drop이 함수 끝에서야 풀면 큰 배열이 함수 끝까지
  살아 최고치가 오른다. self-host에는 아직 liveness가 없어서 실제 위험이다. 계획이
  정리 패스를 DCE 뒤에, 마지막 사용 지점 기준으로 두는 이유다.
- **복사.** 지금 얕은 복사로 backing을 공유하는 곳(C 약 31, LLVM 약 21)은 새 모델에서
  이동이 되거나 깊은 복사가 된다. 추론이 큰 배열에 복사를 고르면 그 자리에서
  메모리가 두 배가 된다. 이동이 기본이고, 복사는 기록된 경우에만 허용해야 한다.

판정은 UNKNOWN이다. §4의 측정이 이 질문의 상한을 정한다.

### 3.3 저사양

- **보통 프로그램 컴파일:** 155줄 이하에서 최고 31 MB 이하(§1.5). 선형이라고 치면
  1만 줄 프로그램은 100 MB 안팎이지만, 측정이 아니라 외삽이다.
- **컴파일러 자체 빌드:** 상한 3 GiB에 OS와 다른 프로세스를 더하면 4 GB 기기에서는
  어렵고 8 GB면 된다. Rust 권장치와 같은 수준이다.
- **컴파일된 프로그램 실행:** 저사양에서는 이쪽이 더 위험하다. 풀지 않는 값 때문에
  오래 도는 프로그램은 메모리가 계속 오른다.

### 3.4 "빌드에서만의 문제"인가

아니다. §1.1의 producer와 §1.2의 `gen2.exe`, `codegen_carrier_opt.exe`는 모두
Pergyra로 컴파일된 프로그램이다. 그 메모리가 곧 Pergyra 프로그램의 런타임 메모리다.
사용자 프로그램도 같은 런타임을 쓴다. 컴파일러는 가장 크고 오래 도는 Pergyra
프로그램이라 먼저 드러났을 뿐이다.

맞는 부분도 있다. 3 GiB 상한 자체는 빌드 환경 예산(CI 러너 7 GB)이지 언어 의미론이
아니다. 그래서 둘을 따로 풀기보다, 자동 정리로 런타임 누수를 막으면 컴파일러 최고치도
함께 내려가는 구조가 맞다. 컴파일러가 그 효과를 재는 가장 큰 측정 대상이다.

## 4. 다음 측정 (계획 P0에 추가)

1. **살아 있는 바이트와 누적 할당 바이트.** §1.1과 같은 고정 입력에서 할당기 카운터로
   최고치 순간의 live bytes와 누적 allocated bytes를 잰다. 이 둘의 차이가 자동 정리가
   줄일 수 있는 최대치다.
   - live bytes가 3 GiB에 가까우면 자동 정리로는 상한을 못 맞춘다. §3.1의 1·2번
     구조를 먼저 다뤄야 하고, 사용자 결정 5가 그 방향으로 정해진다.
   - live bytes가 1 GiB 근처면 자동 정리가 쓸 수 있는 여유가 약 2 GiB다.
2. **측정 출처 기록.** build-pressure summary에 명령, 입력 경로, 실행 파일 SHA-256이
   없다(§1.2). P0 측정은 이 셋을 함께 남긴다.
3. **중간 규모 점.** 1천 줄과 1만 줄 규모 입력에서 최고치를 재서 선형인지 확인한다.

## 5. 하지 말 것

- 상한을 올려서 녹색을 만드는 것(사용자 결정 5 전까지).
- 캐시, shard, worker, 더 작은 입력으로 바꿔서 게이트를 통과시키는 것.
- fact가 소유한 String을 정리 경로로 넘기는 것(`91_build_troubleshooting.md` 6638행
  근처의 경고).
- 분할 컴파일이나 바이너리 MIR을 전환과 같은 트리에서 시작하는 것.

## 6. 증거 위치

- `.tmp/build-pressure/carrier-driver-source{,-opt}.{summary.json,samples.csv,stages.csv}`
  (scratch, 추적 안 됨)
- 작은 프로그램 측정 원본: 세션 scratchpad `examples_memory.txt`(추적 안 됨). 재현은
  §1.5 방법으로 한다.
- 고정 규모 기록: §1.1의 두 문서.
