# self-host 드라이버 source→C 경로 메모리 조사 (2026-11-16)

## 0. 정책 기록 (AGENTS.md "Closure-Blocking Optimization Policy")

| 항목 | 값 |
|---|---|
| 리비전 | main `f1456722d` (작업 트리 `worktree-agent-af85c92af2b740c49`, 변경 없음) |
| 실행 파일 | `/root/tg-own/bin/pgy-self-driver` (BuildID `7ebddfb3…`, 16일 03:56 빌드, main `f1456722d` 내용) |
| 실행 위치 | WSL `Ubuntu-C-WSL`, cwd `/root/tc-mem` (`f1456722d` 체크아웃). 산출물은 `/root/tc-mem/out` |
| 고정 입력 | `src/self_hosted/compiler/driver/driver_rung2_main.pgy` (import 닫힘: 파일 2,176개, 299,442줄, 함수 선언 9,457개) |
| 막힌 게이트 | weekly Linux driver shard 첫 단계: 설치된 드라이버로 `--emit-c-artifact-verified` |
| 관측 비용 | 158 s에 7.4 GB, 212 s에 16.6 GB, 계속 증가 (`docs/truth/evidence/2026-11-15_weekly_driver_shard_causes.md`) |
| 도달한 소유자 | `CompileSourceToCVerified` (`src/self_hosted/compiler/driver/driver_rung2_owner.pgy:471-491`) |
| 반복되는 소유 연산 | 아래 3절. 한 프로세스 안에서 (a) 앞단 사실, (b) MIR JSON 전체 String 투영의 중첩 조각, (c) MIR→AST 왕복 소비자가 모두 해제 없이 쌓인다 |

DRV-2 전체 입력은 끝까지 돌리지 않았다. 측정한 프로세스는 모두 RSS 8 GB에서 PID로 죽이는 감시 루프 안에서 돌렸고, 무거운 측정은 한 번에 하나만 돌렸다.

## 1. 측정

경로:
- `c` = `--emit-c-artifact-verified <src> <out.c>` (한 프로세스)
- `mir` = `--emit-mir-json-verified <src> -o <out.mir.json>`
- `mirc` = `--mir-json <out.mir.json> --observe-mir-consumer-stages -o <out.c>`

RSS는 `/proc/<pid>/status`의 VmRSS를 0.1 s마다 읽은 최댓값(MiB)이다. 시간은 벽시계이며 다른 에이전트 빌드와 겹쳐서 ±수 배 흔들린다(semantic `mir`가 5.0 s와 16.7 s 두 번 나왔다).

| 입력 | 줄 수 | MIR 루틴 | MIR JSON | `mir` RSS / 시간 | `mirc` RSS / 시간 | `c` RSS / 시간 | `c`와 `mirc` 출력 |
|---|---:|---:|---:|---:|---:|---:|---|
| `codegen/fixture/hello.pgy` | 3 | 1 | 2 KB | 4 / 0.1 s | 5 / 0.1 s | 3 / 0.1 s | 같음 |
| `lexer/main.pgy` | 1,985 | 76 | 3.81 MB | 40 / 0.3 s | 82 / 0.6 s | 194 / 1.2 s | 같음 |
| `parser/main.pgy` | 16,420 | 644 | 25.1 MB | 379 / 3.4 s | 664 / 8.3 s | 1,512 / 10.9 s | 같음 |
| `semantic/main.pgy` | 28,597 | 1,097 | 41.1 MB | 608 / 16.7 s | 1,019 / 19.2 s | 2,392 / 34.2 s | 같음 |
| `mir_lower/main.pgy` | 115,162 | 3,709 | 155.2 MB | 2,269 / 63 s | 3,194 / 98 s | 8,195 초과, 174 s에 중단 | (미완) |
| `codegen/main.pgy` | 148,546 | 4,597 | 199.6 MB | 3,112 / 101 s | 4,354 / 133 s | 8,195 초과, 중단 | (미완) |
| DRV-2 (참고, 직접 측정 아님) | 299,442 | 약 9,500 | 약 390~400 MB | — | 10,352 MiB (`docs/truth/state/measurements.md` gen2 C emit) | 212 s에 16.6 GB, 증가 중 | — |

"같음"은 `cmp`로 바이트 단위 비교한 결과다. 완주한 네 입력에서 한 프로세스 경로와 두 프로세스 경로의 C가 같다.

## 2. 증가 법칙

입력보다 빨리 자라지 않는다. MIR 크기에 선형이고 상수가 크다.

| 입력 | `mir` RSS / MIR 바이트 | `mirc` RSS / MIR 바이트 | `c` RSS / MIR 바이트 | `c` MiB / 루틴 | `c` KiB / 줄 |
|---|---:|---:|---:|---:|---:|
| lexer | 11 | 23 | 53 | 2.55 | 100 |
| parser | 16 | 28 | 63 | 2.35 | 94 |
| semantic | 15 | 26 | 61 | 2.18 | 86 |
| mir_lower | 15 | 22 | 55 초과 | 2.21 초과 | 73 초과 |
| codegen | 16 | 23 | 43 초과 | 1.78 초과 | 56 초과 |

- `c` ≈ MIR JSON 바이트의 55~63배, 루틴당 2.2~2.6 MiB.
- DRV-2 외삽: 약 9,500 루틴 × 2.2 MiB ≈ 20 GiB, 또는 약 400 MB × 58 ≈ 23 GB. 212 s에 16.6 GB였고 자라는 중이었다는 관측과 맞는다.
- 두 프로세스 경로의 최댓값은 `mirc` 쪽이고 MIR 바이트의 22~26배다. DRV-2에서 약 10 GB이며 기록된 gen2 C emit 10,352 MiB와 맞는다.

### 왜 `c`가 `mir` + `mirc`보다 큰가

LD_PRELOAD 힙 샘플러(`/root/mem-tools/heapsamp.c`, 할당 512 KB마다 스택 표본, 살아 있는 바이트를 할당 스택으로 집계)로 최고점의 살아 있는 바이트를 단계별로 나눴다.

| 단계 (살아 있는 MiB) | semantic `c` | semantic `mir` | semantic `mirc` | mir_lower `c` (7.26 GB 시점, 소비자 미완) |
|---|---:|---:|---:|---:|
| 앞단: AST | 166 | 152 | — | 605 |
| 앞단: semantic | 30 | 30 | — | 115 |
| 앞단: verify(body types) | 217 | 221 | — | 885 |
| 앞단: MIR facts | 70 | 69 | — | 225 |
| 앞단: JSON 파일 writer | — | 25 | — | — |
| **앞단: JSON 전체 String 투영** | **727** | — | — | **2,773** |
| 소비자(MIR→AST 왕복 이후 전부) | 815 | — | 860 | 2,170 (미완) |
| 합계(표본) | 2,030 | 500 | 860 | 6,783 |

- 앞단 사실: MIR 바이트의 11.7배(semantic), 11.8배(mir_lower). 선형.
- JSON 전체 String 투영: 17.7배(semantic), 17.9배(mir_lower). 선형. `c` 경로에만 있다.
- 소비자: `mirc` 단독과 같은 크기(815 대 860).
- 그래서 `c` ≈ 앞단 12× + JSON String 18× + 소비자 20~25× + 할당기 오버헤드.

mir_lower `c`의 누계: 할당 9.75 GB, 해제 2.48 GB(할당 1.98억 회, 해제 0.40억 회). 할당한 바이트의 75%가 끝까지 살아 있다. 방출된 C에도 정리 코드가 거의 없다. `semantic/main.pgy`의 C 2.67 MB에서 `pgy_as_drop_owned` 17회, `pgy_*_drop_storage` 3회, `pgy_allocator_destroy` 13회가 전부이고 `pgy_concat`는 496회다. 즉 설치된 드라이버의 RSS는 "살아 있는 사실"이 아니라 누적 할당량에 가깝고, 한 프로세스에서 단계를 이어 붙이면 각 단계의 쓰레기가 그대로 더해진다.

## 3. 책임 소유자와 자료 구조

1. **MIR JSON 전체 String 투영의 중첩 조각 (피크의 34%, `c` 경로 전용)**
   - 호출: `CompileSourceToCVerified` → `DriverRung2MirProjectionJson` (`driver_rung2_owner.pgy:178-185`, 484행에서 호출) → `SelfMirProgramJson` (`src/self_hosted/mir/json_projection_owner.pgy:498-587`).
   - 구조: 루틴/블록/명령/표현식 그래프/필드마다 `Array<String>` 조각을 만들고 `JsonEmitCollection`(`src/self_hosted/lib/json_emit.pgy:126-150`)과 `JsonEmitFieldRaw`(같은 파일 160-172)가 한 단계 위 문자열로 다시 복사한다. 아래 단계 조각은 해제되지 않는다. 결과 JSON 1바이트당 조각 약 17.8바이트가 남는다.
   - `mir` 경로는 같은 사실을 `SelfMirProgramJsonWriteArtifactVerified`(`src/self_hosted/mir/program_json_artifact_writer_owner.pgy:313-389`)로 내보낸다. 이 writer는 조각을 `JsonOwnedFragmentWriteFile`(`json_emit.pgy:116-124`)로 쓰고 바로 버려서 25 MiB만 남는다. 파일 머리말이 "redundant whole-program materialization"을 없애는 소유자라고 적고 있다. source→C 경로만 옛 String 투영을 쓴다.
2. **앞단 사실과 쓰레기가 소비자 단계 내내 남음 (22%)**
   - `CompileSourceToCVerified`의 `artifact`, `analysis`, `projection`과 그 단계의 임시 문자열. 대부분 도달 불가능한 쓰레기라서 같은 프로세스 안에서는 돌려받을 수 없다.
   - 가장 큰 한 곳: `SemanticArrayStorageReleaseVerdict`(`src/self_hosted/semantic/array_storage_release_verdict_owner.pgy:54` 이하, 표현식 그래프 노드 전체를 도는 루프)의 `pgy_concat` 85 MiB(semantic). 왕복 때문에 소비자에서 한 번 더 돌아 170 MiB가 된다. mir_lower에서는 608 MiB. gcc 인라인 때문에 호출된 보조 함수의 할당이 이 이름으로 잡혔을 수 있다.
3. **소비자의 phi predecessor binding (semantic에서 소비자의 25%, mir_lower에서 약 9%)**
   - `MirPhiPredecessorBindingFactFromOwners`(`src/self_hosted/mir_lower/phi_predecessor_binding_fact_owner.pgy:23-121`)가 phi × predecessor × 후보 × 루틴의 모든 결과 이름으로 중첩 루프를 돈다(가장 안쪽은 81-98행).
   - 안쪽에서 매번 `MirSsaNameMatches`(`ssa_identity_owner.pgy:7-25`)가 `Concat`과 `Substring`으로 문자열 두 개를 만들고, `MirRoutineBlockDominates`(`routine_definition_dominance_fact_owner.pgy:6-22`)가 `MirRoutineGraphDistancesAvoiding`(`mir_cfg_graph_owner.pgy:45-88`)으로 블록 수 길이의 BFS 배열을 새로 만든다. 둘 다 해제되지 않는다.
   - semantic: BFS 배열 173 MiB, 이름 비교 문자열 83 MiB. 루틴 크기에 대해서는 초선형이지만 프로그램 전체로는 루틴 수에 선형이다. 이 비용은 gen2/gen3 emit(`--mir-json`)에도 똑같이 들어 있다.

## 4. 수정 제안

구현은 하지 않았다. 아래 "구현하지 않은 이유" 참조.

### 제안 A — 게이트 쪽: DRV-2 소스 빌드를 artifact 경계의 두 프로세스로
- 바꾸는 것: weekly driver shard의 첫 단계가 `--emit-c-artifact-verified` 대신 `--emit-mir-json-verified <src> -o x.mir.json`과 `--mir-json x.mir.json -o x.c`를 차례로 부른다. 고정점(gen2 == gen3)이 이미 쓰는 경로다.
- 권한: 컴파일러 소유자는 그대로다. 테스트 스크립트만 바뀐다.
- 예상 상한: max(`mir` 약 15×, `mirc` 22~26×) ≈ 10 GB. 16 GB 러너 안이지만 여유는 6 GB 미만이다.
- 근거: 완주한 네 입력에서 두 경로의 C가 바이트 단위로 같다.
- 한계: `--emit-c-artifact-verified` 자체는 여전히 DRV-2 크기에서 죽는다. 그 경로가 OOM 대신 진단으로 닫히게 하려면 source MIR이 이미 하는 것(`DriverSourceMirRequestDiagnostic`, `driver_source_mir_execution_owner.pgy:9-31`)처럼 `DriverSourceCRequestDiagnostic`에 크기 정책을 두는 방법이 있다. 다만 이것은 V1의 "native가 받는 프로그램은 설치 드라이버도 컴파일한다"와 긴장이 있으므로 사용자 결정이 필요하다.

### 제안 B — source→C 경로에서 JSON 전체 String 투영의 조각 제거
- 바꾸는 것: `CompileSourceToCVerified`가 `SelfMirProgramJson`으로 중첩 조각을 만들지 않게 한다. 두 가지 길이 있다.
  1. `DriverSourceCExecution.Compile`(`driver_source_c_execution_owner.pgy:86-158`, 이미 `io_write`와 출력 경로를 가짐)이 MIR을 `SelfMirProgramJsonWriteArtifactVerified`로 출력 옆의 트랜잭션 임시 파일에 쓰고 `MirJsonReadMachineAdmittedInput`으로 다시 읽은 뒤 임시 파일을 지운다. stdout 경로(`ProduceSourceC`, `io_read`만 있음)는 권한이 달라서 그대로 남거나 따로 결정해야 한다.
  2. artifact writer의 row renderer에 TextBuilder 싱크를 넘길 수 있게 해서 `SelfMirProgramJson`을 그 위에 다시 세우고 옛 중첩 구현을 지운다. writer 파일 5개(`program_json_artifact_writer_owner.pgy`, `instruction_json_artifact_writer_owner.pgy`, `expression_identity_json_projection_owner.pgy`, `intent_execution_json_projection_owner.pgy`, `runtime_call_abi_json_projection_owner.pgy`)가 `CompilerArtifactWrite(output: Int, …)`를 직접 부르므로 한 소유자 안의 변경이 아니다.
- 권한: MIR 사실과 스키마는 `json_projection_owner`/artifact writer가 그대로 가진다. 검사를 건너뛰는 캐시는 없다. 소비자는 지금처럼 JSON 전체를 다시 admission한다.
- 지우는 것: (2)라면 `SelfMirProgramJson`의 조각 배열 구현. (1)이라면 source→C 경로의 `DriverRung2MirProjectionJson` 호출.
- 예상 상한: MIR 바이트의 약 18배 감소. semantic 2,392 → 약 1,650 MiB, mir_lower 약 9 GB → 약 6.2 GB(8 GiB 안에서 완주), DRV-2 약 23 GB → 약 16 GB. DRV-2는 이것만으로는 16 GB 러너에 들어가지 않는다.
- `JsonEmitCollection`에서 `items`를 버리는 식의 수정은 권하지 않는다. 호출자가 빌린 문자열과 리터럴을 섞어 넣는 곳이 있어(`json_emit.pgy:113-115` 주석) 잘못된 해제가 된다.

### 제안 C — 소비자의 phi predecessor binding (gen2/gen3 emit에도 적용)
- 바꾸는 것(소유자 한 곳, `phi_predecessor_binding_fact_owner.pgy`와 그것이 부르는 `ssa_identity_owner.pgy`):
  - `MirSsaNameMatches`를 할당 없는 문자 비교로 바꾼다. 판정은 같다.
  - 한 phi를 판정하는 동안 같은 `(dominator, predecessor)` 질문을 다시 BFS하지 않게, predecessor마다 "entry에서 predecessor까지 이 블록을 피해 갈 수 있는가"를 한 번의 함수 호출 범위 안에서만 재사용한다. 루틴 인덱스가 바뀌지 않는 한 호출 안에서만 살아 있으므로 유효성 검사를 건너뛰지 않는다.
- 예상: semantic에서 소비자 1,019 → 약 760 MiB(−25%), mir_lower에서 약 −9%. DRV-2 gen2 emit 10.35 GB에서 0.5~1 GB 정도로 추정하지만 측정하지 않았다.

### 제안 D — 뿌리
- 방출된 C가 임시 값을 해제하지 않는 한 RSS는 누적 할당량이다. 한 프로세스 왕복은 어떤 국소 수정으로도 앞단 12× + 소비자 22~26× ≈ 34~38×(DRV-2 약 14~15 GB) 아래로 내려가지 않는다. 이 경로를 한 프로세스에서 16 GB 안에 넣는 것은 self-host emitter의 정리 합성(V1 소유권 자동화)이나 MIR→AST 텍스트 왕복 삭제(V1 범용 emitter)에 달려 있다.

### 게이트
1. 정확한 출력 비교: canonical 프로그램의 MIR을 `--mir-json`으로 C로 내린 결과가 수정 전후 `cmp`로 같아야 한다(제안 C). 제안 A/B는 추가로 source 경로 C와 MIR 경로 C가 같아야 한다(지금 네 입력에서 같음).
2. codegen bootstrap gen2 == gen3 (`make self-host-codegen-bootstrap-test-smoke`).
3. 메모리 상한(중간 크기 입력, 같은 입력·같은 경로):
   - `semantic/main.pgy` `--emit-c-artifact-verified` 피크 RSS: 현재 2,392 MiB. 제안 B 뒤 1,800 MiB 이하.
   - `mir_lower/main.pgy` `--emit-c-artifact-verified`: 현재 8 GiB에서 죽음. 제안 B 뒤 8 GiB 안에서 완주하고 C가 `mirc` 결과와 같아야 한다.
   - `semantic/main.pgy` `--mir-json`: 현재 1,019 MiB. 제안 C 뒤 800 MiB 이하.
4. 음성 게이트: 조각을 버리는 변경은 `tools/mir_json_instruction_writer_probe`(String 투영과 artifact writer의 바이트 동일성, 잘못된 사실 거부)를 통과해야 한다.

## 5. 구현하지 않은 이유

- 드라이버 쪽 수정은 드라이버를 다시 빌드해야 확인할 수 있다. 그 빌드(gen2 C emit)와 요구된 게이트(canonical MIR 경로 비교, gen2 == gen3)는 프로세스 하나가 약 10.35 GB를 쓴다. 이번 작업의 8 GB 감시 한도를 넘는다. 한도를 올리거나 입력을 줄이는 것은 금지 사항이다.
- 제안 B는 한 소유자 안에 들어가지 않거나(2), 권한(`io_write`)과 부수 효과가 바뀐다(1).
- 제안 A는 스크립트 변경이고 evidence 문서에 따르면 shard는 이미 같은 실행의 설치 드라이버를 prebuilt로 넘기도록 우회돼 있다.

## 6. 재현

- 스크립트: WSL `/root/mem-tools/` (조사 때 쓴 scratch 복사본)
  - `measure.sh <c|mir|mirc|mirc-obs> <input> <label> [limit_kb]` — RSS 표본과 8 GB 감시.
  - `prof2.sh <c|mir|mirc> <input> <label>` — `heapsamp.c`를 빌드해 LD_PRELOAD로 붙이고 `heapreport.py`로 단계/스택별 집계.
  - `closure.py` — import 닫힘의 줄 수.
- 원자료: WSL `/root/tc-mem/out/` (`*.samples`, `hs-*.peak`, 방출된 `.c`, `.mir.json`).
- 다른 `/root/*` 트리는 건드리지 않았다. 저장소 변경과 커밋은 없다.

## 7. 한계

- DRV-2 자체는 돌리지 않았다. DRV-2 수치는 선형 외삽과 기존 기록이다.
- `c` 경로의 가장 큰 완주 표본은 semantic(28.6천 줄)이다. mir_lower와 codegen은 8 GB에서 끊겨 하한만 있다.
- 힙 샘플러의 함수 귀속은 gcc 인라인 뒤의 심볼 기준이다. 단계 분류는 스택 48프레임 안에 단계 함수가 있는지로 했다.
- 벽시계 시간은 다른 에이전트의 부하와 섞여 있다.
