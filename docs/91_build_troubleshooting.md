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
- `make clean` removes only the active `BUILD_DIR` and `BIN_DIR`.
- Ad-hoc roots such as `build-codex*`, `bin-codex*`, `build-llvm*`, and
  `.tmp/self_hosted/*` are intentionally ignored, but they can accumulate.
- LLVM-enabled links are the heaviest local step. With low free disk, linker and
  test scratch writes can make the machine look frozen.

Useful commands:

```sh
mingw32-make build-resource-report
PGY_BUILD_RESOURCE_DEEP=1 mingw32-make build-resource-report  # slower exact sizes
mingw32-make clean-scratch              # removes .tmp only
mingw32-make clean-local-artifacts      # removes build/bin, .tmp, build-*, bin-*
```

Do not run broad CI targets when the repo drive has less than about 10 GiB free.
Use the narrow gate named by the source-of-truth seam first. For low-pressure
local builds, prefer:

```sh
mingw32-make LLVM_ENABLED=0 all
mingw32-make abi-ownership-shape-test-smoke
```

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
