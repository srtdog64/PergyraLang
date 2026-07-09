# 158. Self-Bootstrap 실행 설계도 (Execution Blueprint)

Status: `execution-blueprint`. 작성 2026-07-05. 목적: self-eating bootstrap을
**나 없이 집행 가능한 정밀도**로 고정한다. docs/150(driver/lsp wiring)이 rung을
약속하고, 이 문서는 그 아래 층 — **어느 rung이 임계 경로이고, 정확히 무엇이
남았고, 어떤 결정이 누구 것인가** — 를 실측 기반으로 못박는다. 상위 문서:
docs/self_hosted/01(North Star + Stage 5), docs/150(driver/LSP rung), PROGRESS.md
(치환 원장). 이 문서의 모든 숫자는 2026-07-05 실측(PROGRESS.md + codegen_bootstrap.sh
+ likeness 래칫).

---

## 0. 한 장 요약 (먼저 이것부터)

**당신은 0이 아니다. 방어 가능한 self-hosting 마일스톤 하나를 이미 달성했다.**

- **M1 — 코드젠 fixed-point: ✅ 달성(2026-06-17).** self-host 코드젠이 자기
  자신을 컴파일해 `gen2 == gen3` byte-identical(5,484줄 C). self-built 코드젠이
  **22/22 self-host 컴포넌트 전부**를 유효한 C로 컴파일. 이건 진짜 3-stage
  fixed-point이다 — 단, **코드젠 컴포넌트의 소스에 대해서만.**
- **M2 — 전체 컴파일러 self-eating bootstrap: ~6.57%.** 전 컴파일러(lexer+
  parser+semantic+IR+codegen)를 Pergyra로 써서 자기 전체 소스를 컴파일하고
  B==C. semantic이 46k C LOC 중 6%만 포팅됨 = 진짜 벽.

**"진척도가 안 올랐다"는 정확한 관찰이다 — 단 이유가 특정된다:** 최근 120커밋
중 66개(55%)가 self-host인데 **거의 전부 LSP(2a~2h 9렁) + 오라클/parity fact**다.
LSP는 compiler-core 치환 **0%** 트랙(별도 집계). 그래서 M2 %는 안 움직였다.
에너지가 임계 경로(semantic/parser 완전성) 밖으로 샜다.

**정정(내 지난 진단): "self-host는 G-EXEC에 막혔다"는 부정확했다.** fixed-point
증명(B==C)은 **셸 하니스가 외부 gcc로 돌려서 이미 작동**한다(§2). G-EXEC는
self-DRIVER 소유(DRV-2, §5)만 막는다 — 증명이 아니라 순도(purity) 문제다. 진짜
벽은 **완전성**(§3)이다.

**그래서 핵심 결정(§4)은 BDFL 것이다:** M2(전체 부트스트랩, semantic 포팅 =
수개월)를 종착지로 밀 것인가, 아니면 **M1(코드젠 fixed-point)을 방어 가능한
self-hosting 선언으로 받고 레버리지를 thesis(던전크롤러)로 돌릴 것인가.**

---

## 1. 실측 현재 위치 (2026-07-05, PROGRESS.md)

**compiler-core 치환: ~6.57% (16,341 Pergyra LOC / 248,794 C LOC).**

| 컴포넌트 | C LOC | Pergyra LOC | 실질 커버리지 | 임계 경로? |
|---|---:|---:|---|---|
| lexer | 921 | 677 | 993/993 예제 byte-equal (**subset 완결**) | 사실상 완료 |
| parser | 20,579 | 8,127 | ~52% (187 fixture, 120/121 예제) | **YES — 완전성 필요** |
| semantic | 46,203 | 2,716 | rung-2 subset(bounded body check만) = **6%** | **YES — 최대 벽** |
| codegen | 107,123 | 4,821 | rung-0..20, **gen2==gen3 self-host** | subset 자기부트 완료 |
| mir_lower | — | — | fact-only 90 PASS / 0 gap (rung-0b CFG subset) | 부분 |
| runtime | 29,627 | 0 | **0% — 설계상 C 잔류**(런타임 커널은 self-host 대상 아님) | N/A |
| compiler(driver) | 43,304 | 0 | DRV-0/1 landed(별도 집계) | §5 |
| lsp | 1,037 | 0 | LSP-0..2i landed(별도 집계, **0% core**) | 임계경로 **아님** |

**진짜 metric(likeness 래칫, `self_host_pergyra_likeness_smoke.sh`):**
`core_string_munge_sig` **116/116 = GREEN(2026-07-05 실측)**, broad
`total_string_munge_sig`는 **166(info)**. 이전 broad `string_munge_sig=166` red는
tools/LSP/fuzz/path/fixture/harness 텍스트 라우팅까지 compiler-core linchpin에
섞은 metric-scope 문제였다. 이제 blocking ratchet은 core transform owner만 세고,
total은 리뷰용 정보로 남긴다. `ast: String` 표면은 0(잠김), typed-AST 스켈레톤 1
착지. AST를 typed-AST로 교체하는 게 linchpin(§3.3)이고, 앞으로의 진척은
core_string_munge↓로 측정한다.

**해석:** 언어의 "쉬운 절반"(lexer, 그리고 codegen을 subset에서 self-host)은
됐다. "어려운 절반"(semantic 46k LOC, parser의 나머지 48%, typed-AST 교체)이
남았고, 최근 수개월은 그걸 안 건드리고 LSP를 팠다.

---

## 2. Fixed-point는 이미 작동한다 (G-EXEC 불필요)

**하니스: `tests/self_hosted/parity/codegen_bootstrap.sh`.** 3-stage 부트스트랩을
실제로 돌린다(셸이 조립공, gcc는 외부):

```
gen0 (C-oracle 빌드) --[main.pgy AST]--> gen1.c --[gcc]--> gen1.exe
gen1.exe             --[main.pgy AST]--> gen2.c --[gcc]--> gen2.exe (Pergyra-빌드)
gen2.exe             --[main.pgy AST]--> gen3.c
FIXPOINT: gen2.c == gen3.c (byte-identical)          ← ✅ green
```

로드맵 Stage 5 정의(docs/self_hosted/01):
- stage1: C 컴파일러가 self-host 소스 → 바이너리 A.
- stage2: A가 self-host 소스 → 바이너리 B.
- stage3: B가 self-host 소스 → 바이너리 C.
- **B == C byte-identical = self-hosting 증명.**

**핵심: gcc 호출은 하니스(bash)가 한다.** self-host 도구는 C 텍스트를 *방출*
하고, bash가 gcc에 먹인다. 그러므로 **fixed-point 증명은 Pergyra가 gcc를 띄우는
능력(G-EXEC)을 요구하지 않는다.** 결정론적 gcc를 같은 입력에 돌리면 B==C는
"A의 C-출력 텍스트 == B의 C-출력 텍스트"로 환원되고, 그건 이미 셸로 검사된다.

이 하니스가 추가로 검사하는 breadth(이미 green):
- gen0 vs gen2 바이너리 parity(8 fixture: hello, func_recursive, struct_param,
  array_push, str_indexof, else_if_chain, string_equality, io_probe).
- gen2가 lexer/parser/semantic/mir_lower 컴파일 → oracle-빌드와 출력 일치.
- gen2가 18 audit tool + fuzz generator 컴파일 → 출력 일치.

**함의:** "코드젠은 self-host한다"는 지금 진실이고 gated이다. 이게 M1이다.

---

## 3. M2로 가는 임계 경로 = 완전성 (semantic → parser → typed-AST)

M2(전체 self-eating bootstrap)를 밀기로 결정한다면(§4), 임계 경로는 이 순서다.
G-EXEC도, 웹사이트도, 더 많은 LSP 렁도 아니다.

### 3.1 완전성의 정의 (측정 방법)

M2의 "완료" = **self-host front-to-back가 자기 자신의 전체 ~18k-LOC 소스의 모든
구문을 parse+lower할 수 있다.** 로드맵 명시 목록: closures, generics, `match`,
`Option`, `Array<class>`, 중첩 배열 전부 round-trip.

**측정 하니스(신설 권고 = WO-SH-COMPLETE):** `codegen_bootstrap.sh`가 이미
22개 컴포넌트에 breadth 검사를 한다. 이걸 확장 — self-host의 **자기 소스 파일
전수**(src/self_hosted/**/*.pgy)를 self-parser→self-semantic→self-codegen에
먹여 각 파일에서 무엇이 깨지는지 실측·집계. 산출 = "완전성 갭 목록"(구문별
미지원 카운트). 이게 진척도의 정직한 metric이 된다(LOC%보다 우월 — 무엇을
닫으면 되는지 직접 준다).

### 3.2 semantic이 최대 벽 (46k C LOC, 6% 포팅)

현 self-semantic은 rung-2 subset(bounded `let`/return/call typing, 108 fixture)
뿐이다. C semantic 46k LOC의 실체: 타입 추론, 도메인 계약 검사(zone/authority/
intent/effect), witness 시스템, capability 분석, lifecycle taint, slot 안전,
generic 바인딩 — 이 언어를 이 언어답게 만드는 검사 전부. **이걸 Pergyra로
포팅하는 게 M2 비용의 대부분이고 수개월급이다.** 정직하게 적는다: 2주 작업 아님.

포팅 순서 권고(의존성 낮은 것부터, 각 rung은 C oracle과 병행 parity):
1. **표현식 타입 추론 코어**(현 rung-2 확장) — 산술/비교/논리/호출 반환 타입.
2. **선언 검사**(subject/object/zone/world/intent 계약 형태 검사).
3. **witness/ability 만족 검사**(role impl → ability, docs/semantics/10).
4. **capability 분석**(declared⊇used, interproc 전파 — docs/15).
5. **lifecycle/slot 안전**(taint, own/ref release, generation).
각 rung은 self-semantic이 C semantic과 같은 진단(code/cause)을 내는지 parity로
검증. LSP-0 진단 parity 배관(docs/150 O-LSP)이 이미 canonical event 비교를
갖고 있으니 재사용.

### 3.3 typed-AST 교체 = linchpin (recommended-not-required)

로드맵: "text-munging 컴파일러도 기술적으로 fixed-point에 도달 가능하나, BDFL
선호는 idiomatic 컴파일러를 부트스트랩하는 것." 현 코어는 `ast: String` 표면
0(잠김) + `core_string_munge_sig` 116개. **typed-AST 노드로
교체**하면 self-host가 "C를 Pergyra 문법으로 쓴 것"이 아니라 진짜 Pergyra가
된다. 래칫(`self_host_pergyra_likeness_smoke.sh`)이 이 전환을 단조 강제
(core_string_munge_sig↓, typed_ast_contract↑). **M2를 밀면 이걸 병행해야 "un-Pergyra
컴파일러를 부트스트랩"하는 자기모순을 피한다.** 순서 옵션: fixed-point 먼저
도달 후 likeness를 끌어올려도 됨(로드맵이 둘 다 허용).

### 3.4 parser 완전성 (52% → 100% on 자기 소스)

parser는 이미 120/121 예제·187 fixture byte-equal. 남은 48% LOC = 자기 소스가
쓰는 구문 중 아직 self-parser가 못 읽는 것. §3.1 완전성 하니스가 정확한 목록을
준다. 이건 semantic보다 훨씬 작은 잔여(구문 추가는 mechanical, semantic 검사는
설계).

---

## 4. ★ 전략적 분기 — BDFL 결정 (이 문서의 핵심)

**두 종착지가 실재하고, 비용이 자릿수 다르다:**

| 옵션 | 내용 | 비용 | 상태 |
|---|---|---|---|
| **M1 선언** | "코드젠이 self-host한다(gen2==gen3)"를 방어 가능한 self-hosting 마일스톤으로 받고, 레버리지를 thesis(던전크롤러)로 | 이미 달성. 문서화·홍보만 | ✅ 지금 가능 |
| **M2 완주** | 전체 컴파일러(semantic 46k LOC 포팅 + parser 완전성 + typed-AST 교체)를 Pergyra로, 전체 소스 B==C | 수개월(semantic dominant) | ~6.57% |

**로드맵 자신의 입장(docs/self_hosted/01 Stage 5):** "이 종착지 도달은 substrate
성취(모든 진지한 언어는 결국 self-host)이며 **그 자체로 language thesis를 진전
시키지 않는다. 최고 레버리지 근시일 작업이 아니다.** thesis는 실제 프로그램의
도메인 primitive로 증명된다(=던전크롤러)."

**즉 로드맵이 이미 M2를 "종착지이되 최고 레버리지 아님"으로 규정했다.** 그리고
당신의 killer usecase(안전한 new Flash / 던전크롤러)는 WASM+미디어+서명로더가
남았지 semantic self-host가 남은 게 아니다.

**권고(제 판단, BDFL이 뒤집을 수 있음):** **M1을 self-hosting 마일스톤으로
선언하고, 레버리지를 thesis로 돌려라.** 근거 3층:
1. M1은 진짜다(gen2==gen3, 22/22 컴포넌트). "코드젠이 자기를 컴파일한다"는
   정직한 방어 가능 주장이고, 웹사이트 헤드라인으로도 충분하다.
2. M2의 semantic 포팅은 수개월인데 로드맵 스스로 "thesis 진전 없음"이라 못박음.
3. 당신의 킬러 승부처(sandbox 콘텐츠)는 WASM에 막혔지 self-host에 막힌 게 아님.
   같은 수개월을 WASM+던전크롤러에 쓰면 thesis가 전시된다.

**단 M2를 밀 정당한 이유도 있다:** typed-AST 교체는 언어 표현력을 스스로
증명하는 궁극 dogfood이고(§3.3), self-host된 semantic은 "이 언어로 이 언어의
복잡한 검사를 쓸 수 있다"의 최강 증거다. **이건 당신 결정이다** — 이 문서는
둘 다 실행 가능하게 스코핑했다(M2 경로 §3, M1 선언 §6).

이 분기를 미결로 두면 §0의 문제가 재발한다(에너지가 임계 경로 밖으로 샘).
**결정을 먼저 하라.**

---

## 5. DRV-2 / G-EXEC 이차 트랙 (driver 순도 — fixed-point와 독립)

M1이든 M2든, "Pergyra 프로세스 하나가 source→binary 전체를 소유"하는 순도
업그레이드는 G-EXEC를 요구한다. **이건 fixed-point 증명과 독립**(§2). 원하면
별도로 집행.

### 5.1 무엇을 사는가

현재 DRV-0/1(landed): Pergyra가 조립(self-parser→AST text→self-codegen→C text)을
in-process 소유하되 **C 텍스트에서 멈춘다**; 셸이 gcc를 마무리. DRV-2 = Pergyra
드라이버가 gcc를 직접 호출 → 셸 하니스 로직(codegen_bootstrap.sh 208~383행)을
Pergyra로 이관 → `PgyCompilerWorld`가 계약-토폴로지에서 **실행형**으로 승격.

### 5.2 예약된 어휘 (이미 자리 파짐)

`src/self_hosted/compiler/subprocess_runner_owner.pgy`:
- schema `pgy.selfhost.subprocess-runner.v1`, 7 fact: executable_path, argv, cwd,
  env_allowlist, timeout_ms, stdout_stderr, exit_code.
- 3 use case: oracle_compare / fixture_build(gcc 호출) / artifact_probe.
- `authority_owner.pgy`: `ability SubprocessDiscipline` + `role SubprocessAuthority`.
- `world.pgy`: `SubprocessRunnerZone` + step `Subprocess requires: SubprocessDiscipline`.
- **막힌 곳: 실제 `Subprocess(...)` 호출 표면이 없다.** 계약만 있고 구현 0.

### 5.3 ★ G-EXEC 표면 결정 (BDFL — 스코핑 완료)

이건 "안전한 sandbox"가 핵심 피치인 언어에 "gcc를 띄운다 = 임의 코드 실행"을
넣는 결정이라 sandbox 모델과 정면으로 부딪친다. **그래서 미룰 만했다.** 하지만
화해는 깔끔하다 — capability 축이 정확히 이걸 위해 있다:

**결정 형태(권고):**
- **builtin:** `Subprocess(argv: Array<String>, opts) -> Result<SubprocessOutcome>`
  (exit_code + stdout/stderr 캡처). world.pgy 어휘와 정렬.
- **capability:** 신규 `PGY_CAP_SUBPROCESS`(기존 budget/capability 양축에 편입).
  기본 **미부여**. self-driver만 명시 grant(`PGY_CAP_GRANT=subprocess`).
  **sandbox 콘텐츠(던전크롤러 등)는 절대 grant 안 함** — 이게 "safe Flash"와
  "self-compile"이 공존하는 정확한 분리선.
- **envelope 강제:** env_allowlist(부모 env 미상속, 명시만) + timeout_ms(budget
  wall-time twin) + cwd 제한. 예약된 7 fact가 그대로 런타임 강제 대상.
- **양 백엔드:** C(fork/exec 또는 posix_spawn) + LLVM(export twin). budget/
  capability와 동일한 single-instance 규율(멀티인스턴스 static 함정 주의 —
  redteam 메모리의 g_pgy_budget 사례).

**선례:** Deno `--allow-run`(정확히 이 모델 — subprocess는 명시 grant),
WASI(subprocess 부재 = 안전 기본), Go `os/exec`(무제한 = 반례). Deno 노선 채택.

**결정점(당신 것):** cap 이름 확정 / allowlist 기본값(빈 = 전부 거절 권고) /
budget wall-time과의 상호작용(spawn된 자식도 wall budget에 계상?). 이 셋만
정하면 DRV-2가 mechanical.

### 5.4 DRV-2/DRV-3 rung

- **DRV-2**(G-EXEC 후): 셸 하니스 로직을 Pergyra로. `SubprocessRunnerZone`을
  실제 gcc 호출에 배선. 오라클: 기존 codegen_bootstrap.sh와 byte-equal 산출.
- **DRV-3**: `pgy --self-driver` 플래그 뒤에서 지원 subset을 C driver와
  run-equal parity. out-of-subset은 관측 가능한 거절.

---

## 6. M1 선언 경로 (분기에서 M1 택할 시)

M2를 미루고 M1을 self-hosting 마일스톤으로 받기로 하면:
1. PROGRESS.md에 "codegen self-host fixed-point 달성(gen2==gen3)"을 headline
   마일스톤으로 승격(현재 % 원장에 묻혀 있음).
2. 정직 경계 명문화: "**코드젠**이 self-host한다. 전체 컴파일러는 아니다
   (semantic 6%)." — 오버클레임 방지(marketing drift 메모리).
3. 웹사이트/발표에서 쓸 방어 가능 문장: "The Pergyra code generator compiles
   its own source into a byte-identical fixed point." (전체 self-host 주장 금지.)
4. self-host 트랙을 유지보수 모드로(래칫 green 유지), 레버리지를 thesis로.

---

## 7. WO 등록 (보드 편입 대상)

- **WO-SH-COMPLETE** — 완전성 측정 하니스(§3.1): self-host 자기 소스 전수를
  front-to-back에 먹여 구문별 갭 집계. **M2 진척의 정직한 metric.** (M2 택 시 선행)
- **WO-SH-SEM** — semantic 포팅 rung 사다리(§3.2, 5단계, 각 C-oracle parity).
- **WO-SH-TYPEDAST** — typed-AST 교체(§3.3, likeness 래칫 구동).
- **WO-GEXEC** — subprocess builtin + PGY_CAP_SUBPROCESS(§5.3, BDFL 결정 선행).
- **WO-DRV-2/3** — driver 순도(§5.4, WO-GEXEC 선행).
- **★ 선행 결정: §4 분기(M1 선언 vs M2 완주).** 이거 없이 착수하면 에너지 샌다.

## Related

docs/self_hosted/01(North Star + Stage 5 정의) · docs/150(driver/LSP rung —
DRV-2 G-EXEC 블로커) · docs/148(stdlib — 병행 베팅) · PROGRESS.md(치환 원장) ·
`tests/self_hosted/parity/codegen_bootstrap.sh`(fixed-point 하니스) ·
`tests/self_host_pergyra_likeness_smoke.sh`(진짜 metric) · docs/159(stdlib 설계도)
