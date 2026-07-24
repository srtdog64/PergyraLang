# Build Troubleshooting

마지막 업데이트: 2026-05-24

빌드/회귀 도중 자주 마주치는 문제와 대응. **항상 `mingw32-make rebuild`를 먼저 시도**하면 절반은 풀린다.

---

## 0. Resource pressure first

If the desktop hangs during local builds, check disk and scratch pressure before
debugging compiler logic.

Observed local pressure pattern:

- `make all` builds only `pgy` and `pgy-lsp`; test binaries are behind
  `all-with-tests` and test targets.
- The default build keeps debug symbols off (`PGY_DEBUG_SYMBOLS=0`). Use
  `PGY_DEBUG_SYMBOLS=1 mingw32-make all` or `mingw32-make debug` only when
  symbolized debugging is intentional.
- `make clean` removes only the active `BUILD_DIR` and `BIN_DIR`.
- Ad-hoc roots such as `build-codex*`, `bin-codex*`, `build-llvm*`, and
  `.tmp/self_hosted/*` are intentionally ignored, but they can accumulate.
- LLVM-enabled links are the heaviest local step. With low free disk, linker and
  test scratch writes can make the machine look frozen.

Useful commands:

```sh
mingw32-make build-resource-report
PGY_BUILD_RESOURCE_DEEP=1 mingw32-make build-resource-report  # slower exact sizes
mingw32-make build-pressure-dev-compiler # samples pgy-only build RSS/private bytes
mingw32-make build-pressure-compiler     # samples default LLVM-enabled compiler build
mingw32-make build-pressure-self-host-compiler # samples the Pergyra-built DRV-2 build
mingw32-make self-host-driver-bootstrap-full-test-smoke # full fixpoint; pressure-wrapped on Windows
mingw32-make clean-scratch              # removes .tmp only
mingw32-make clean-local-artifacts      # removes build/bin, .tmp, build-*, bin-*
```

The default resource report is intentionally shallow. It lists artifact roots
and free space without recursively counting files. Use
`PGY_BUILD_RESOURCE_DEEP=1` only when you need exact size/file-count evidence;
on Windows/Git Bash, scanning tens of thousands of scratch files can itself
make the desktop feel stalled.

`build-pressure-dev-compiler`, `build-pressure-compiler`, and
`build-pressure-self-host-compiler` are the memory bug lines. They run the
low-pressure C-only compiler build, the default LLVM-enabled compiler build,
and the Pergyra-built bounded DRV-2 build through
`scripts/measure_build_pressure.ps1`, then sample the process tree. The default
limit is 3 GiB (`PGY_BUILD_PRESSURE_LIMIT_MB`), and all three targets stop the
measured tree when it crosses that line. If one compiler build crosses the
line, treat it as a build/compiler memory defect until the sample log proves
otherwise. Do not use a broad parity matrix's system-wide memory total as the
compiler-build measurement. This is separate from disk/file-count pressure: a
full artifact scan can stall the desktop with small RSS when the repo drive is
nearly full.

The same rule applies to self-host stage tools. A `--check` mode must validate
the stage contract without materializing a full generated artifact unless that
artifact is the thing being tested. 2026-07-09 evidence: self-host codegen
`--check` over an 840 KiB compiler AST peaked at about 3.4 GiB when it called
the full C-emission path; after splitting the check path into structural
subset verification, the same input peaked at about 76 MiB. Treat a future
`--check` path that builds full generated C text as a memory regression.

Do not run broad CI targets when the repo drive has less than about 10 GiB free.
Use the narrow gate named by the source-of-truth seam first. For low-pressure
local builds, prefer:

```sh
mingw32-make dev-compiler              # C-only, no debug symbols, pgy only, serialized
PGY_DEV_COMPILER_JOBS=4 mingw32-make dev-compiler  # explicit opt-in parallelism
mingw32-make LLVM_ENABLED=0 all        # C-only, pgy + pgy-lsp
mingw32-make abi-ownership-shape-test-smoke
```

2026-07-09 local measurement on Windows/MinGW, with tests excluded and debug
symbols off:

- clean `dev-compiler` rebuild after removing `build-dev` / `bin-dev`: peak
  sampled working set 290.5 MB, peak sampled private memory 266.4 MB, top
  process `cc1.exe` at 243.2 MB;
- clean default `compiler` rebuild after removing `build` / `bin`: peak sampled
  working set 385.4 MB, peak sampled private memory 364.3 MB, top process
  `cc1.exe` at 357.2 MB;
- local artifact pressure can still dominate perceived hangs. The resource
  report on the same checkout showed the E: drive at 99% used with about
  15.5 GiB free, many local `build-*` / `bin-*` variants, and more than 28k
  files under the active `.tmp` / `build` / `bin` sample. In that state, broad
  local CI may stall from file churn even when compiler RSS stays under 400 MB.

2026-07-24 Windows/UCRT64 incident evidence separated the build units again:

- a clean `release` rebuild completed in 1,576,373 ms; a second isolated LTO
  relink with detached MSYS compiler-worker tracking peaked at 490.3 MB working
  set and 444.1 MB private memory, with `cc1.exe` the largest process;
- a fresh Pergyra-built bounded DRV-2 build completed in 351,507 ms, producing
  a 2,927,734-byte AST, 2,959,613-byte C unit, and 2,397,166-byte driver. It
  peaked at 1,343.8 MB working set and 1,412.2 MB private memory; `gen2.exe`
  owned 1,134.1 MB of private memory;
- the DRV-2 result is below the 3 GiB hard ceiling, but a 1.1 GiB codegen seed
  for a roughly 3 MiB artifact is explicit optimization debt. Do not describe
  it as normal just because the compiler is being self-hosted;
- the observed desktop pressure also had an unfiltered Git Bash DRV-2 wrapper
  whose worker survived as a reparented native process, a replacement full
  matrix started on top of it, and the D: volume at 97% use. The shallow
  resource report correctly warned that broad CI could stall;
- `measure_build_pressure.ps1` now attributes detached `cc1`, LTO, linker, and
  Pergyra seed workers by probe start time. All compiler pressure targets stop
  the measured workers at 3 GiB instead of reporting only after completion.

These measurements do not claim that the released compiler is self-hosted.
DRV-2 remains a bounded Pergyra-built source/MIR-to-C replacement. C and LLVM
are the native compiler's peer production backends; compiling a self-host tool
through both backends is parity evidence, not evidence that the Pergyra-built
driver owns a self-hosted LLVM emitter.

There is also a distinct, confirmed full-input defect. An earlier isolated
`driver_mir_oracle --emit-mir-json-verified` run over the driver source reached
approximately 17 GiB RSS / 28 GiB private memory and produced no artifact
before it was stopped. That is not a normal compiler build and it is not
excused by self-hosting: it is unresolved full-driver MIR materialization
amplification. On Windows, the official
`self-host-driver-bootstrap-full-test-smoke` entry now runs inside the same
3 GiB hard pressure boundary and attributes reparented `driver_oracle`,
`driver_seed`, and `driver_genN` workers. Do not invoke the script directly
with `PGY_SELFHOST_DRIVER_FULL_FIXPOINT=1` when investigating this defect.

Two bounded builds isolate this defect from ordinary native linking while also
showing real compiler-scale optimization debt. Compiling the approximately
3 MiB driver source to a guarded oracle through the released compiler's C
backend completed in 74,025 ms at 2,138.8 MB working set / 2,145.6 MB private.
The LLVM backend build completed in 147,566 ms at 2,228.2 MB working set /
2,239.5 MB private. These are compile-to-executable measurements, not the
full-input oracle execution. They show that large-source compilation already
needs optimization, but they do not explain away a later 28 GiB oracle process.

The C-built guarded oracle rejects a direct full-driver MIR request before
materialization, rejects use of its full-fixpoint token on a bounded fixture,
and still emits the bounded `let_log` MIR artifact. The linked LLVM-built
artifact currently falls into the CLI usage diagnostic for every argumentful
invocation. Its build therefore proves LLVM lowering/linking of this source,
not runnable argv parity; the LLVM process-entry/`Args()` seam remains a
separate explicit blocker.

Source inspection identifies the strongest current amplification mechanism,
but not yet its exact share of the 28 GiB peak:

- `mir/json_projection_owner.pgy` materializes instruction strings into block
  strings, block strings into routine strings, and all routine strings into a
  program-level `Array<String>` before returning the final JSON string;
- `lib/json_emit.pgy` builds every field/object/array with nested `Concat` and
  `StringJoin`; the Pergyra-built C runtime allocates new buffers for both;
- ordinary temporary `String` buffers are not reclaimed at each emitted
  routine, and `AllocatorScratch()` is currently a system-backed lane label,
  not a bulk-reset arena.

That shape can retain and repeatedly copy full lower-level projections while
the next level is assembled. It is the leading source-backed explanation for
the observed growth. The exact split between MIR fact construction and JSON
projection remains `Unknown` until a stage marker or allocation census observes
it; do not record the inference as a completed root-cause percentage.

The durable repair is a bounded or streaming JSON emission owner consuming the
same Pergyra MIR facts, with an executable byte/schema parity gate and a
per-routine lifetime boundary. Raising the memory ceiling, rearranging the same
nested `Array<String>` values, or adding C-shaped backend fragments is not a
repair. C and LLVM must remain peer consumers of one Pergyra-owned semantic/MIR/
ABI fact spine. The binary token is an accidental-direct-run interlock, not an
authorization secret; the official pressure wrapper remains the resource owner.

The follow-up check found that exact bypass active beside a 95-fixture DRV-2
shard. Together with a short-lived third recursive make probe, the three runs
owned 21 project processes and 2,114 MB private memory at an early snapshot;
they were stopped before the full-input oracle could grow further. This also
exposed a GNU make diagnostic trap: `make -n` still executes a recipe line that
contains `$(MAKE)`, because make treats it as a recursive invocation. Do not
dry-run the full-fixpoint pressure target; use
`tests/build_pressure_contract_smoke.sh` to verify its wiring.

So a stalled desktop is not automatically evidence of a compiler heap leak. If
single `compiler` builds stay below a few hundred MB but broad smokes stall the
machine, treat the first suspect as scratch/file-count pressure from self-host
and backend parity artifacts. Run `PGY_BUILD_RESOURCE_DEEP=1 mingw32-make
build-resource-report`; if `.tmp/self_hosted` or backend campaign scratch owns
the file count, use `mingw32-make clean-scratch` before broad local CI.

For a self-host owner edit, do not rerun the 204-source completeness ledger
until the focused slice is stable. Use the source filter with the relevant
stage first:

```sh
PGY_SELFHOST_COMPLETENESS_STAGES=codegen \
PGY_SELFHOST_COMPLETENESS_SOURCES=src/self_hosted/codegen/input/ast_type_usage_owner.pgy \
mingw32-make self-host-completeness-smoke
```

The source filter is local validation only. It requires every selected source
to pass every selected stage, but it does not prove source-count minima,
pipeline identity, or the full 204-source replacement ledger.

The codegen parity matrix is also an integration gate, not a narrow edit loop.
Its 69 fixtures each run through oracle, C-built self-host, and LLVM-built
self-host legs. The runner uses bounded fixture parallelism with two workers by
default; set `PGY_SELFHOST_CODEGEN_JOBS=1` for pressure diagnosis or at most 4
on a measured CI worker. Unbounded parallelism is forbidden because it trades
wall time for desktop stalls and hides per-process memory regressions. During a
local slice, select only the fixtures that exercise the owner:

```sh
PGY_SELFHOST_CODEGEN_FIXTURES=hello,func_recursive \
PGY_SELFHOST_CODEGEN_JOBS=2 \
mingw32-make self-host-codegen-parity-test-smoke
```

The complete 69-fixture matrix remains the integration proof. It should not be
silently substituted for a compiler build, and it should not be repeated by
multiple aggregate targets in one CI job.

The 280-row DRV-2 body matrix has the same isolation rule. Run its unfiltered
full matrix from MSYS2 bash on Windows. A Git Bash wrapper can exit while its
long-running worker remains reparented as a native Windows process; starting a
replacement then overlaps two full artifact-producing runs. The DRV-2 runner
therefore rejects an unfiltered Git Bash invocation. Git Bash remains available
for a focused development gate when
`PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER` names the exact fixtures.

Windows evidence on 2026-07-12: the serial full matrix took about 31 minutes;
the same 69-fixture C/LLVM matrix with the default two workers completed in
1,342,043 ms (22 minutes 22 seconds), with both backends at 69/69. This is a
bounded wall-time improvement, not a fast edit loop. Parser/tool compilation
and native process orchestration remain serial. Use
`self-host-preparation-contract-test-smoke` for owner-shape edits and reserve
`self-host-preparation-parity-test-smoke` for integration or scheduled CI.

---

## 1. "Nothing to be done for 'bin/pgy.exe'"

### 증상
소스를 수정했는데 `mingw32-make bin/pgy.exe`가 즉시 끝나면서 위 메시지를 출력하고, 실제로는 변경이 반영되지 않은 바이너리를 그대로 사용한다.

### 원인
- `.d` (dependency) 파일이 stale해서 make가 재빌드 필요성을 인식하지 못함
- `.inc` 파일을 수정했는데 의존하는 `.c`가 그 사실을 모름 (MMD가 .inc 까지 추적하지 못하는 경우)
- 파일 시스템 mtime이 빌드 시점과 어긋남 (네트워크 드라이브/MSYS2 vs Windows native 혼용)

### 대응
```sh
mingw32-make rebuild           # clean + all 한 번에
```
또는 명시적으로:
```sh
mingw32-make clean
mingw32-make all
```

특정 파일만 강제 재빌드하고 싶으면:
```sh
rm build/semantic/type_checker.o
mingw32-make bin/pgy.exe
```

---

## 2. PowerShell vs bash vs cmd.exe 차이

### 증상
- PowerShell에서 `mingw32-make ... 2>&1 | Select-Object -Last 30` 호출 시 stderr가 ErrorRecord로 mangled되어 실제 빌드 출력이 깨져 보임
- bash에서 gcc subprocess가 exit 1로 침묵 종료 (sandbox 환경)
- cmd.exe에서 Makefile recipe의 sh 의존 명령(`find`, `sed`)이 실패

### 권장 (Windows)
**MSYS2 MinGW64 shell** 또는 **Git Bash**를 사용한다. PowerShell/cmd.exe는 보조 용도.

CI는 `windows-latest` + `msys2/setup-msys2` native MinGW/MSYS2 runtime이 공식 라인. plain Linux-hosted gcc는 acceptance line이 아님.

### PowerShell에서 어쩔 수 없이 빌드해야 하면
```powershell
Set-Location E:\PergyraLang
& mingw32-make rebuild *>$env:TEMP\build.log
$LASTEXITCODE
Get-Content $env:TEMP\build.log -Tail 30
```
stderr를 파일로 떨어뜨리고 별도로 읽어야 mangled되지 않는다.

---

## 3. CONFIG_STAMP 작동

### 정의
`Makefile:96` — `$(BUILD_DIR)/.config_llvm_$(LLVM_ENABLED)_$(CC_TAG).stamp`

### 트리거
다음이 변경되면 모든 `.o`/`.d`가 강제 삭제 + 재빌드:
- `LLVM_ENABLED` 플래그
- 컴파일러 변경 (`CC_TAG`)

### 트리거 안 되는 것
- 소스 파일 자체의 mtime
- `.inc` include 추가/제거
- 헤더의 macro 정의 변경 (이건 `.d` dependency가 cover해야 하는데 stale 시 누락)

→ 헤더/매크로 의심되면 `make rebuild`로 강제.

---

## 4. Stale .o 진단

### 증상 발견 절차
1. 소스 수정한 사이트에 `#error "marker"` 추가
2. `mingw32-make bin/pgy.exe` 실행
3. 컴파일 에러가 안 나면 → 그 .o가 stale

### 즉시 대응
```sh
find build -name "*.d" -delete   # dependency 캐시 비우기
mingw32-make bin/pgy.exe
```
이래도 안 되면:
```sh
mingw32-make rebuild
```

---

## 5. CI/로컬 차이

### 자주 나오는 패턴
- 로컬에서 통과한 테스트가 CI에서 실패
- 원인: 로컬은 incremental build, CI는 fresh build

### 로컬에서 CI 환경 재현
```sh
mingw32-make rebuild
mingw32-make ci-windows         # 또는 ci-linux
```

`ci-windows`는 Windows C regression(`test-all`, `fmt-test-smoke`, `stdlib-test-smoke`, `example-test-smoke`)을 기본 실행한다. Windows LLVM smoke/backend-compare는 executable `llvm-config --libs core` evidence가 있을 때만 추가 실행하며, 단순 `C:/Program Files/LLVM/lib` 폴더 존재는 beta support evidence가 아니다.

---

## 6. 빌드 시간 단축 vs 신뢰성

| 상황 | 권장 |
|---|---|
| 한 파일만 수정, 빠르게 확인 | `mingw32-make bin/test_semantic.exe` |
| 헤더/매크로 수정 | `mingw32-make rebuild` |
| `.inc` 파일 수정 | `mingw32-make rebuild` (안전) 또는 의존 .o 삭제 후 빌드 |
| PR 직전 / merge 전 | `mingw32-make rebuild && mingw32-make ci-windows` |
| stale 의심 | 무조건 `mingw32-make rebuild` |

원칙: **"빠른 증분 빌드"보다 "신뢰 가능한 재빌드"를 우선**.

---

## 7. Shared `build/` 병렬 실행 금지

### 증상

두 개 이상의 `mingw32-make` gate를 같은 checkout에서 동시에 실행한 뒤,
링커가 다음과 비슷한 오류를 낸다.

```text
file in wrong format
unrecognized storage class
local symbol has no section
```

### 원인

여러 gate가 같은 `build/`와 `bin/`을 공유하면서 같은 `.o`를 동시에
컴파일/링크한다. MinGW object가 부분적으로 쓰인 상태에서 다른 링크가
읽으면 이후 증분 빌드까지 오염된다.

### 대응

순차 실행한다.

```sh
mingw32-make test-transpile
mingw32-make raw-escape-contract-test-smoke
```

`raw-escape-contract-test-smoke`와 `runtime-none-contract-test-smoke`는 source
contract를 항상 검사하고, 이미 있는 `pgy`만 실행 probe에 사용한다. 이 둘은
다른 build gate를 검증하기 위해 전체 compiler rebuild를 강제하지 않는다.

병렬 검증이 필요하면 gate마다 별도 디렉터리를 지정한다.

```sh
mingw32-make BUILD_DIR=/tmp/pgy-a-build BIN_DIR=/tmp/pgy-a-bin test-transpile
mingw32-make BUILD_DIR=/tmp/pgy-b-build BIN_DIR=/tmp/pgy-b-bin raw-escape-contract-test-smoke
```

이미 오염됐다면 해당 `.o`/`.d`를 지우거나 `rebuild`를 사용한다.

---

## 8. 참고

- `Makefile:704` — `clean` target
- `Makefile:707` — `clean-objects` (object만 삭제, 디렉터리 유지)
- `Makefile` — `rebuild` target (clean + all)
- `tests/diagnostics_json_smoke.sh` — JSON 진단 회귀
- `tests/compare_backends.sh` — C/LLVM parity 회귀
