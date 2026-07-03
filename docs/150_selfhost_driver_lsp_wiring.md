# 150. Self-Host Driver & LSP — 설계 배선도

Status: `wiring-doc, rung-gated`. PROGRESS.md의 두 0% 트랙(compiler
driver, LSP)의 치환 사다리를 고정한다. docs/148(stdlib)과 같은 규율:
**문서가 rung을 약속하고, `selfhost-driver-lsp-wiring-test-smoke`가 §4
rung 표를 문다** — landed 주장에는 실존 artifact+gate가 있어야 하고,
planned rung은 artifact를 주장할 수 없다. 가짜 진척 차단이 이 문서의
절반이다.

## 0. 두 트랙의 위상 (왜 이 순서인가)

- **Driver가 Stage 5의 조립대다.** 지금 self-host 스테이지들은 각자
  parity를 이룬 **부품**이고, 셸 스크립트(`codegen_bootstrap.sh`)가
  임시 조립공이다: `pgy --ast → AST text → gen1(AST) → C → gcc`.
  Driver 치환 = 이 조립을 Pergyra 프로세스 하나가 소유하는 것 — 그리고
  그 순간 `PgyCompilerWorld`(world.pgy)가 계약-토폴로지에서 **실행형**으로
  승격된다: `CompilePergyraProgram` intent가 실제로 돌고, stage Ready
  fact들과 authority 뼈대(FactProving/ArtifactEmission/…)가 런타임
  소비자를 얻는다. A-8(자기를 먹는 부트스트랩)의 최단 경로가 이 트랙이다.
- **LSP는 최소·최고가성비 표적이다.** C LSP는 1,037 LOC(전 트랙 최소)에
  잘 분해돼 있고(protocol/diagnostics/hover/features), 페이로드의 본질이
  **JSON 렌더링** — self-host lib의 검증된 json_emit이 정확히 그 도구다.
  semantic squiggle(docs/140)의 4색 분류가 언어의 차별점인 만큼, 그
  분류기가 Pergyra로 서술되는 것 자체가 thesis 전시다.

## 1. Driver 사다리 (DRV)

C driver가 하는 일: CLI → source read → lexer→parser→semantic→
HIR/DIR/RIR/MIR→AIR verify→backend emit→cc 호출→link. 치환은 관측
가능한 이음새(파일/텍스트 artifact)부터 안쪽으로 진행한다.

- **DRV-0 — in-process 스테이지 조립 (첫 rung, 착수 가능)**
  `driver/main.pgy`가 self-parser owner들(→ AST text)과 self-codegen
  owner들(→ C text)을 **한 프로세스에서 import로 체이닝**. 오늘의 셸
  조립과 동일 산출물을 내되, `pgy --ast` 좌석을 self-parser로 치환
  (byte-equal 실증 완료라 정당). 산출 = C 텍스트 파일 + AST 텍스트.
  **오라클**: 같은 소스에 대해 (a) AST text == `pgy --ast`, (b) C text ==
  oracle-빌드 codegen 산출, (c) gcc/실행은 기존 하니스가 마무리.
  **world 소비 의무**: rung-0부터 `CompilerStagePathManifestReady()` 등
  stage fact를 실행 경로에서 소비 — world가 장식으로 남지 않게.
- **DRV-1 — CLI 표면**: `Args()`(존재, PGY_CAP_ENV 게이트)로
  `driver <src> [--emit-ast|--emit-c] [-o out]` 파싱. 오라클: 산출물
  경로/내용 동일성.
- **DRV-2 — 네이티브 컴파일 호출 (차단: G-EXEC)**: cc/gcc 호출은
  프로세스 spawn builtin이 **없다**(2026-07-04 확인). 필요물 =
  `Exec(argv) -> Result<Int>`류 gated builtin + 신규 process capability
  (world.pgy의 `SubprocessRunnerZone`/`SubprocessCapabilityEnvelope`
  계약이 이미 그 자리를 파놓았다 — env_allowlist/timeout_ms/exit_code
  fact 어휘까지). **G-EXEC는 표면 결정**(BDFL): cap 이름, allowlist
  강제, budget 상호작용. 그 전까지 DRV는 C-텍스트 산출에서 멈추고 셸이
  마무리하는 것이 정직한 경계다.
- **DRV-3 — 플래그 뒤 교체**: `pgy --self-driver`류로 지원 subset에서
  C driver와 run-equal parity. out-of-subset은 관측 가능한 거절.

주의(소유권): self-codegen/emission owner들은 typed-AST 동시 스트림의
활성 영역 — DRV-0은 그 파일들을 **import만** 하지 수정하지 않는다.

## 2. LSP 사다리 (LSP)

C LSP 분해: protocol(framing 229L)/diagnostics/hover/features. 페이로드
계층부터 치환하고 transport는 마지막(차단막 있음).

- **LSP-0 — 진단 페이로드 투영 (첫 rung, 착수 가능)**
  `lsp/diagnostics_owner.pgy`: self-checker 진단(verdict/diag 블록 —
  `SelfHostDiagnostic` 렌더러의 소비자 반대편)을 LSP
  `publishDiagnostics` JSON으로 투영. lib/json_emit 재사용.
  **오라클 배관 필요(O-LSP)**: C LSP에 페이로드 덤프 모드가 없으면
  비교 불가 — `pgy --lsp-dump-diagnostics <src>`류 C-측 덤프 플래그를
  먼저 뚫거나(소형), 초기엔 golden fixture 수동 승인으로 시작하고
  덤프 플래그 착지 시 오라클로 승격.
- **LSP-1 — squiggle 4색 분류기**: RED/AMBER/BLUE/VIOLET 판정
  (docs/140의 색 결정 로직)을 Pergyra 분류기로. 입력 = 진단 코드 +
  AIR advisory fact(JSON — validator가 이미 읽는 그 표면). BLUE는
  noise policy(A-4) 확정분만.
- **LSP-2 — transport 루프 (차단: G-STDIN)**: JSON-RPC Content-Length
  프레이밍은 **바이트 단위 stdin 스트리밍**이 필요 — 현 표면의 입력
  builtin이 라인/파일 기반이면 불충분. 필요물 = `ReadStdin(n) ->
  String`류 gated builtin(caps: input). G-STDIN도 표면 결정. 그 전까지
  LSP-0/1은 파일-입출력 도구로 검증한다(오라클엔 충분).
- **LSP-3 — 플래그 뒤 교체**: hover/features까지 포함해 C LSP와
  세션-스크립트 parity.

## 3. 갭 등록부 (배선이 정직하려면 구멍도 배선에)

| 갭 | 내용 | 소유 | 선행 |
|---|---|---|---|
| **G-EXEC** | 프로세스 spawn builtin + process capability. world.pgy의 Subprocess 계약 어휘(env_allowlist/timeout/exit_code)를 런타임 fact로 | 표면 결정(BDFL) + 양 백엔드 lowering + caps 게이트 | DRV-2 |
| **G-STDIN** | 바이트-단위 stdin 읽기 builtin (Content-Length 프레이밍용), caps: input | 표면 결정 + 양 백엔드 | LSP-2 |
| **O-LSP** | C LSP 진단 페이로드 덤프 플래그(오라클 배관) | C-측 소형 | LSP-0 오라클 승격 |

## 4. Rung 표 (selfhost-driver-lsp-wiring-test-smoke가 잠금)

status ∈ {planned, landed}. landed = artifact와 gate가 실존해야 하고,
planned는 artifact/gate에 `-`만 허용된다. 착지 시 같은 커밋에서 행을
갱신한다.

<!-- DRIVER-LSP-RUNG-BEGIN -->
| track | rung | status | artifact | gate |
| --- | --- | --- | --- | --- |
| driver | DRV-0 | planned | - | - |
| driver | DRV-1 | planned | - | - |
| driver | DRV-2 | planned | - | - |
| driver | DRV-3 | planned | - | - |
| lsp | LSP-0 | planned | - | - |
| lsp | LSP-1 | planned | - | - |
| lsp | LSP-2 | planned | - | - |
| lsp | LSP-3 | planned | - | - |
<!-- DRIVER-LSP-RUNG-END -->

## 5. 순서 권고

DRV-0(조립 — Stage 5 직결) ≻ LSP-0/1(고가성비, thesis 전시) ≻ G-EXEC/
G-STDIN 표면 결정 ≻ DRV-2/LSP-2. DRV-0과 LSP-0은 상호 독립이라 병행
가능. 어느 쪽도 runtime 커널 치환을 전제하지 않는다(런타임 C 잔류는
기존 설계 결정).

## Related

docs/148(stdlib 배선 — 같은 규율의 선례) · docs/140(squiggle 4색) ·
docs/146(SEA — Subprocess/BlockingPool 어휘) · world.pgy
(SubprocessRunnerZone/CompilePergyraProgram — driver의 계약 원본) ·
PROGRESS.md(치환 원장) · TODO 보드 self-host driver/LSP 트랙
