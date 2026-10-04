# Pergyra 세계관·책임·핫패스 1차 공격 기록

Date: 2026-09-30 (Asia/Seoul)
Status: AUDIT COMPLETE / implementation preparation only

이 기록은 전체 소스 목록을 스캔하고 핵심 실행 경로를 공격한 1차 결과다.
모든 함수를 정밀 검토했다거나 전체 회귀·CI·self-host 폐쇄를 완료했다는 뜻은 아니다.
사용자의 우선순위는 Pergyra 세계관에 맞는 실행 의미, 단일 책임, 관리 가능성,
핫패스의 확인 용이성이다. cap과 게이트는 이를 뒷받침하는 증거로 사용했다.

## Snapshot and ownership

- 시작 HEAD: `035d621af1a63e93186fc43e266af49b056f0ab2`.
- 고정 실행 패킷을 준비할 때 HEAD: `d9b9eaaac285eb3c63cca6b0e06ac3bf6db49ca7`.
  다른 작업이 driver 변경을 커밋하고 설치 바이너리도 바꿨다. 이 감사는 커밋하지 않았다.
- 최종 검증 때 HEAD: `64f250e1f26f411cc3921e1bcf72e0ef499a7db0`.
  다른 작업의 statement-transition gate 수정이 착지했다. 두 핵심 owned String
  source owner와 고정 실행 번들의 해시는 그대로였다.
- 원래 dirty: `docs/00_vision.md`, `docs/current_work_handoff.md`,
  `src/compiler/self_host_driver.c`; untracked 과거 감사와 conditional-push fixture.
  이후 driver 수정은 다른 작업에 의해 착지했고 collection semantic gate에 변경이 생겼다.
- 활성 작업은 P1 collection ownership이다. registry의 해당 행은 ACTIVE다.
  이 기록과 작업 준비서는 semantic authority나 새 활성 rung을 만들지 않는다.
- 현재 target과 순서는 `docs/self_hosted/14_target_compiler_world.md`가 소유한다.
  P1과 연결되는 P2b 소비자 전환을 제외한 후속 전환은 준비 상태로 둔다.

검증에 사용한 고정 번들은 `.tmp/worldview_attack_20260930_6d75/`에 남겼다.

| Artifact | SHA-256 |
|---|---|
| pgy-pinned.exe | 261504A560D9E6EAEC7C1C79A019C3B90F23578440505D3458CBDA510CE98F2E |
| pgy-self-driver-pinned.exe | E40DE66B33DA69CE6C398D091B5D714C19CC0B5AC25813682EDFCC2B85188094 |
| pgy-self-driver-pinned.machine-layer-manifest.json | 0A83B0DB5EFE3C00C6D9413C63045C4B17AFF079781213B280442C588E5A9C19 |

첫 pin 시 manifest를 빠뜨린 C 실행은 `source C machine declaration is invalid`로
실패했다. 이는 감사 준비 오류다. manifest까지 복사·해시 확인한 뒤 C 대조군과
반례를 다시 실행했다. 이 초기 실패를 컴파일러 결함의 증거로 사용하지 않는다.
위 번들은 설치된 바이너리의 재현 증거이며 현재 HEAD의 새 빌드 증명은 아니다.

두 핵심 source owner의 해시는 고정 번들을 만들기 전후 동일했다.

- semantic owned String result owner: `62B9F4034C36B4123DA12EB065951B7A99C8F5AF0976E6466BCCD14D49C1E447`.
- direct-MIR owned String result owner: `4B3D4508720ECC22EA78D4F1C9466F1D90BE4FB11F19C9576964463B78FC9DF6`.

## F1 — CONFIRMED: 무관한 함수가 소유권 승인 결과를 바꾼다

최소 반례는 [owned String wrapper](repros/owned_string_wrapper_2026-09-30.pgy)다.
`Owned1`이 fresh String을 만들고, `Owned0`은 그 결과를 그대로 반환한다.
Main은 결과를 Array에 넣고 owned drop한다. 순수 계산과 값 전달에 해당하므로
`func`와 일반 값으로 작성하는 것이 자연스럽다.

[대조 변형](repros/owned_string_wrapper_padded_2026-09-30.pgy)은 동일한 소스 끝에
호출되지 않는 `func Unused() -> Int { return 7; }`만 추가한다.
기존 함수, Main, 호출 대상과 collection receipt의 identity를 바꾸지 않는다.

고정 번들로 관찰한 결과:

| Input | self-host source → MIR | public C | public LLVM |
|---|---|---|---|
| fresh String 직접 호출: Owned0만 있음 | PASS | 실행 PASS | 실행 PASS |
| Owned0 → Owned1 wrapper, 3 routines | PASS | 거부: readiness 26 | 거부: readiness 26 |
| 동일 wrapper, 선언 순서만 반대 | PASS | 거부: readiness 26 | 거부: readiness 26 |
| 동일 wrapper + Unused를 Main 뒤에 추가, 4 routines | PASS | 실행 PASS | 실행 PASS |

실행 PASS는 exit 0과 정확한 `worldview-owned-string-ready` 출력을 관찰했다.
LLVM positive leg에는 target-triple override warning이 있었다.

MIR에서 실패/성공 변형의 `owned-string-push` receipt는 둘 다
`receiver_binding_syntax_id=23`, `source_binding_syntax_id=1`이다.
push expression의 `Owned0` call target SyntaxNodeId도 둘 다 1이다.
달라지는 routine 수는 3과 4다. 단순 source identity 변동을 원인으로 추측하지 않는다.

직접 읽은 원인 경로:

1. `src/self_hosted/semantic/ast_owned_string_result_fact_owner.pgy`는
   exact callable identity를 대상으로 고정점 계산하여 wrapper의 결과를 승인한다.
2. `direct_mir_scalar_program_collection_ownership_transition_plan_readiness_owner.pgy`
   는 push의 callee body를 다시 판정한다.
3. `direct_mir_scalar_program_owned_string_result_fact_owner.pgy:58,127`은
   함수 진입과 expression/initializer 진입에서 모두 depth를 증가시키면서 한도를
   `ArrayLength(plan.routines.roles)`로 정한다.
4. 최소 반례는 local initializer에 도달할 때 depth 4가 필요하지만 routine은 3개다.
   무관한 함수가 하나 늘면 한도가 4가 되어 같은 계산이 승인된다.
5. `direct_mir_scalar_cfg_program_extension_readiness_owner.pgy:213`의
   collection transition 검사 실패는 코드 26이다. 최종 오류는 일반 plan identity
   오류로 렌더링되어 사용자에게 소유권 원인과 source 위치를 설명하지 못한다.

이것은 cap 초과가 아니라 의미적 불변조건의 위반이다. 사용하지 않는 코드 추가가
안전한 함수 합성의 승인 조건이 되어서는 안 된다. 한도를 단순히 크게 올리는
수정도 목적을 충족하지 않는다. exact producer identity에 결속된 증거와 cycle/
expression traversal의 실제 경계가 필요하다. 외부 MIR은 신뢰 경계에서 검증하고,
동일한 admitted 계획의 소비자는 승인된 사실을 읽어야 한다.

## F2 — CONFIRMED: native oracle의 같은 wrapper 판정은 선언 순서에 의존한다

같은 pinned native launcher에서 `--native-pipeline`으로 C/LLVM을 확인했다.

| Declaration order | native C | native LLVM |
|---|---|---|
| wrapper 먼저, fresh producer 나중 | MIR 거부 | MIR 거부 |
| fresh producer 먼저, wrapper 나중 | 실행 PASS | 실행 PASS |

앞선 순서는 semantic 0 errors 뒤
`MIR collection ownership transition is invalid ... stage=invalid-state-transition`
로 실패한다. 뒤 순서는 exit 0과 기대 출력을 관찰했다.

`src/semantic/type_checker_func_decl.c:394`에서
`semantic_collection_record_owned_string_result_summary`를 기록하고,
`src/semantic/collection_ownership_fact.c:158`의 body proof는 callee의 기존
`BODY_SUMMARY_RETURNS_OWNED_STRING`을 조회한다. native oracle과 Pergyra semantic
고정점 판정이 같은 callable 합성에 다른 결과를 내는 실행 반례다.
C oracle은 독립 비교 대상으로 유지하되 production 의미의 두 번째 권위로 남겨서는 안 된다.

### Reproduce with the pinned packet

저장소 루트에서 다음을 실행한다. wrapper는 exit 1/readiness 26,
wrapper_padded는 exit 0/기대 출력을 관찰하는 명령이다. 이 실패는 기대한 감사 반례다.

```powershell
$env:PGY_SELF_DRIVER_BIN = 'D:/PergyraLang/.tmp/worldview_attack_20260930_6d75/pgy-self-driver-pinned.exe'
foreach ($shape in @('wrapper', 'wrapper_padded')) {
    foreach ($backend in @('c', 'llvm')) {
        & ./.tmp/worldview_attack_20260930_6d75/pgy-pinned.exe `
            "docs/audits/repros/owned_string_${shape}_2026-09-30.pgy" `
            "--backend=$backend" -o ".tmp/worldview_attack_20260930_6d75/reproduce-$shape.$backend.exe" --run
        "shape=$shape backend=$backend exit=$LASTEXITCODE"
    }
}
```

영구 재현 파일 2개도 이 번들에서 C/LLVM 네 leg를 다시 실행했다.
expected exit와 diagnostic/output mismatch는 0이었다.
위 환경 설정은 실행하는 PowerShell 세션 범위의 선택이다.

## F3 — SOURCE OBSERVATION: 실행 world와 내부 fact 수명의 간격

`world.pgy:278`의 실제 멤버는 direct_mir/source_mir/source_llvm/source_c 네 route zone이다.
`DriverSourceCExecution.Compile`은 입수 승인, 산출물 commit, outcome을 갖는다.
`CompilePergyraCArtifact`의 단일 step intent도 게시 성공/실패라는 실제 목적을
묶으므로 action 수가 하나라는 이유로 없앨 대상이 아니다.

반면 `SourceUnit.Read`, `LexerStage.Scan`, `ParserStage.BuildAst`,
`SemanticStage.Check` 등의 선언된 stage action은 readiness를 반환한다.
현재 world의 실행 stage 멤버가 아니며 실제 revision 봉인을 수행하지 않는다.
키워드가 있다는 사실을 실행 세계관 완성으로 세지 않는다.

`driver_rung2_owner.pgy:482`의 source→C 경로는
source→MIR JSON→JSON admission으로 이어진다. 일반 C 소비자 경로에는 MIR→AST text,
expression graph, semantic 재분석, C emission이 남아 있다.
`canonical_mir_execution_owner.pgy:39`도 같은 종류의 재구성 경계를 갖는다.
직접 GraphPlan 대체 조각과 이 일반 경로의 도달 범위를 구별해야 한다.

현재 구조를 읽으면 목적과 실제 게시 경계는 보이지만, 동일 revision의 사실을
빌려 읽는 내부 단계 수명은 문자열과 재승인 경로 뒤에 가려져 있다.
P2b → P2a 순서로 이전할 이유가 소스에서 확인된다. 이번에 이전을 구현하지는 않았다.

## F4 — REVIEW CANDIDATES: 책임 분리는 의미 축으로

| Candidate | Observed shape | Judgment / prepared boundary |
|---|---|---|
| collection ownership verdict | 한 top-level 함수 span 529줄. 초기 출처, 대입, statement/call effect, transfer, 최종 row 발행 | 현재 P1에서 origin 분류·전이·receipt 발행을 구분해 검토. 타입/연산마다 새 권위로 쪼개지 않음 |
| EmitStmtList | span 642줄. lexical dispatch와 env, resource-init/exit cleanup을 함께 투영 | cleanup과 control-flow의 같은 불변조건은 유지. projection이 새 정책을 판정하는지부터 검토 |
| SelfMirDeclarationsFromAnalysis | span 595줄. 다수 병렬 배열을 같은 row로 유지 | S2 때 record/row 단위 구성 검토. cap을 맞추기 위한 열별 함수 분산은 위험 |
| enum payload provenance owner | 1,450줄, 30 top-level funcs, 등록 cap 1,450 | 큰 파일이라는 이유만으로 SRP 위반이라 판정하지 않음. active-variant identity와 flow join의 응집성부터 검토 |
| 작은 owner 65개 | 15줄 이하 owner 후보 | 안정된 kind identity, ABI spelling, boundary를 소유하는 작은 owner는 타당. wrapper 수만으로 제거하지 않음 |

span은 다음 top-level func 또는 파일 끝까지의 정적 후보 수치로, 주석/공백을 포함한다.
함수/파일 길이는 책임 혼합이나 성능 문제의 자동 증명이 아니다.

`direct_mir_*.pgy`는 tracked 984개, 95,094줄로 관찰했다.
target 문서의 S0 979 상한 제안보다 5개 많다. 이는 모양별 파일 증식의 확인 신호이며
984개 자체가 의미적 실패 증거는 아니다. 기존 collection_program_plan의 입력을
확장하는 방향이 타입/연산별 route를 추가하는 방향보다 검토 우선이다.

LSP document revision, debug session, formatter session도 source 수준에서 확인했다.
revision/session의 기존 owner를 소비해야 하며, 함수마다 zone을 추가할 이유가 되지 않는다.
그 영역의 live LSP, debugger, formatter 실행 회귀는 이번 감사에서 수행하지 않았다.

## Hot-path status

- 후보: source→MIR JSON→재색인, 일반 MIR→AST→semantic 재분석.
- 후보: owned String 고정점의 전체 signature 반복 및 선형 Contains.
- 후보: collection plan 발행·projection·emitter에서 반복되는 full Ready/digest 검사.
  각각이 외부 신뢰 경계인지 동일 admitted plan의 소비인지 먼저 구분한다.
- 부정 확인: collection verdict의 graph node loop는 `last_root + 1`을 사용한다.
  중첩 while만 보고 root마다 graph 전체를 재스캔한다고 주장하지 않는다.

16/48-function declaration-order 프로그램으로 source→MIR을 반복 측정했다.
48-function forward/reverse의 5회 중앙값은 각각 79.06/79.02 ms였지만 개별 값은
73~1,831 ms까지 흔들렸고 그동안 설치 binary도 바뀌었다.
이 숫자는 핫패스의 병목이나 개선율을 증명하지 않는다. 원시 탐색값으로만 남긴다.
다음 측정은 고정 번들과 같은 revision에서 owner별 pass/노드/복사 바이트를 기록한다.
일반 cache/query engine이나 worker를 추가할 근거는 아직 없다.

## Scan and supporting checks

tracked `src/scripts/tests/.github` 경로 8,341개를 목록화했다.
`src`의 C/H/Pergyra 후보 3,783개, 619,269줄을 크기·import·함수 span으로 스캔했다.
fixture/expected 디렉터리, `src/tests`, root `src/test_*`는 크기 후보에서 제외했다.
이 후보 집합은 Makefile production linkage 증명과 동일하지 않다.

| Check | Observed result |
|---|---|
| SoT authority edge | PASS: 95 authorities, 196 carriers; CLOSED 69 / BRIDGE 24 / ACTIVE 2; 96th refusal |
| Pergyra likeness | PASS; declared surface metric이며 실행 세계관 증명은 아님 |
| semantic .inc / worker boundary / Make source inventory | PASS; source contracts |
| semantic TU size | FAIL: 6 files over 599; collection_ownership_fact.c 865 포함 |
| backend implementation header size | FAIL: 6 headers over 600 |
| full component inventory | 완전한 terminal PASS 미관찰. checker mechanics PASS 뒤 bounded attempt exit 1, final inventory 없음 |
| skill validation | quick_validate PASS; 5 reference paths 존재 |

semantic/header cap 위반은 supporting maintenance debt로 남긴다.
그 수치를 없애기 위한 compiler 대규모 분할은 이번 요청의 세계관 판단을 대신하지 않는다.
초기 Bash login PATH에 Python이 없어 checker가 실패한 시도는 환경 문제로 분류했다.
UCRT64 Python 경로를 추가한 SoT 검사에서는 PASS를 관찰했다.

## Prepared work and limits

[작업 준비서](../agent_work_directives/pergyra_worldview_quality_preparation_2026-09-30.md)는
F1/F2를 P1의 다음 반례로 전달하고, 의미 소유권·revision·게시·target 투영·DX별
책임 경계를 기존 순서에 대응시킨다. authoring skill에는 worldview review 지침을
추가했다. compiler 구현, cap authority, registry 상태, CI, 설치 설정은 이 감사가 변경하지 않았다.

전체 parity matrix, fresh bootstrap/fixed point, remote CI, ASan/TSan,
owner별 시간·할당 계측은 실행하지 않았다. 결함을 발견하고 준비했다는 것이 결과다.
