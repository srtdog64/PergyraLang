# C Oracle and Bootstrap Contract

Status: `supporting-bootstrap-contract`
Date: 2026-07-29

이 문서는 C oracle의 허용 역할과 bootstrap 수렴 조건을 소유한다. 현재
production 도달성과 `SURFACE` / `REACHABLE` / `SUBSTITUTING` 등급은
[`17_pergyra_native_dogfood_contract.md`](17_pergyra_native_dogfood_contract.md)가,
실행 순서와 fixed-point 계획은
[`../158_self_bootstrap_execution_blueprint.md`](../158_self_bootstrap_execution_blueprint.md)가
소유한다. 이 문서의 상태 문장이 현재 source, owner registry 또는 실행 gate와
다르면 후자가 우선하며 이 문서를 교정한다.

## 1. 결론

C oracle은 삭제 대상이 아니다. PergyraLang이 계속 더 안전하고 더 강한
의미를 제공하더라도, 그 의미가 실제 시스템에서 실행 가능한 C ABI와 runtime
protocol로 손실 없이 내려가는지 확인하는 비교군이다.

제거해야 하는 것은 C oracle 자체가 아니라 다음 두 권한이다.

- production semantic fact를 최종 결정하는 권한;
- Pergyra 경로가 실패했을 때 C 결과로 조용히 대체하는 fallback 권한.

따라서 장기 구조는 다음과 같다.

```text
Frozen C seed
  -> Stage 1 Pergyra compiler
       -> Stage 2 self-built compiler
            -> Stage 3 convergence check

C oracle
  = bootstrap seed + differential reference + recovery tool

Target production compiler
  = Pergyra semantic/MIR/admission owners
    -> C backend projection
    -> LLVM backend projection
```

여기서 C backend와 C oracle은 다르다. C backend는 최신 Pergyra 의미를 C로
투영하는 정식 backend이고 계속 유지한다. C oracle은 C로 작성된 이전 compiler
구현이며 bootstrap과 비교에 사용한다.

## 2. C가 증명하는 것과 증명하지 않는 것

C는 Pergyra 의미의 상한이 아니라 lowering witness다.

- 같은 관측 가능한 동작을 C ABI와 runtime protocol로 구현할 수 있는지 확인한다.
- ownership, identity, authority, lifecycle과 실패 구분이 lowering 뒤에도
  보존되는지 확인한다.
- C와 LLVM projection이 같은 target-neutral admitted fact를 소비하는지
  비교한다.
- 시간과 메모리 비용이 비정상적으로 증가하지 않는지 기준선을 제공한다.

C parity만으로 다음을 증명할 수는 없다.

- Pergyra semantic owner가 유일하다는 것;
- invalid state가 source에서 표현 불가능하다는 것;
- missing evidence가 output 전에 반드시 거부된다는 것;
- concurrency, authority와 lifetime 계약이 모든 경로에서 보존된다는 것.

이 보장은 Pergyra semantic fact, MIR carriage, machine admission과 negative
gate가 소유한다. Backend가 C source나 C oracle 결과에서 의미를 재추론하면
이중 권위다.

## 3. 허용되는 C oracle 사용

- 새 checkout 또는 새 platform에서 최초 Pergyra compiler를 만드는 Stage 0;
- native/self MIR 및 C/LLVM 결과의 differential comparison;
- bootstrap compiler가 손상됐을 때 복구하는 frozen tool;
- 성능, peak memory와 artifact 크기의 비교 기준;
- 아직 self path가 도달하지 못한 기능의 명시적인 oracle-only 진단.

## 4. 금지되는 C oracle 사용

- self semantic fact가 없을 때 C fact를 graft하는 것;
- self compilation 실패를 C 성공으로 바꾸는 implicit fallback;
- 같은 production run에서 C와 Pergyra가 하나의 의미를 각각 결정하는 것;
- name, ordinal, AST rescan 또는 output parity로 missing owner fact를 숨기는 것;
- C oracle이 통과했다는 이유로 Pergyra negative gate를 생략하는 것;
- 최신 언어 기능을 언제나 C compiler에 먼저 구현해 C를 영구적인 언어
  authority로 유지하는 것.

## 5. Bootstrap 완료 조건

완전한 bootstrap은 단순히 Pergyra source가 한 번 컴파일되는 상태가 아니다.
최소한 다음 조건이 필요하다.

1. Frozen C seed가 Stage 1 Pergyra compiler를 만든다.
2. Stage 1이 compiler source 전체를 처리해 Stage 2를 만든다.
3. Stage 2가 같은 source에서 Stage 3를 만든다.
4. Stage 2와 Stage 3의 차이가 deterministic artifact identity 또는 정의된
   semantic equivalence 안에서 수렴한다.
5. Production entrypoint가 C oracle을 호출하지 않는다.
6. C/LLVM backend가 같은 admitted MIR/plan을 소비한다.
7. Missing, unknown, stale, duplicate와 forged fact가 output 생성 전에 실패한다.
8. Oracle graft와 `self failure -> C fallback`이 structural gate로 금지된다.
9. Bootstrap peak memory와 시간 budget을 기록하고 반복 whole-graph
   validation을 한 번의 owner validation으로 줄인다.
10. Frozen seed, Stage 1, Stage 2와 Stage 3 artifact의 provenance를 기록한다.

## 6. 현재 판단

현재 bootstrap은 가능성 증명과 부분 실행 대체를 넘었지만 아직 완전 독립
closure는 아니다.

- Production `--mir-json-backend=c|llvm`의 direct-MIR slice는
  `PgyCompilerWorld -> DriverRung2DirectMirZone -> DriverRung2Execution`에
  실제로 도달한다.
- 입력 기능의 admitted binding-slot 및 typed intent-transition slice만 기존
  C-owned consumer를 대체해 bounded `SUBSTITUTING`이다.
- Source-to-C, source-to-MIR과 general MIR-to-C root는 아직 direct
  `CompileSourceTo*` / `CompileMirJsonToC*` orchestration을 사용한다. Compiler
  root의 canonical real-purpose `intent`도 아직 `SURFACE`다.
- 일부 canonical/oracle bridge, 완전한 LLVM self projection, 전체 role/domain
  runtime 의미, frozen seed provenance와 Stage 2/3 convergence는 열려 있다.
- 따라서 지금 C oracle을 삭제하면 비교 기준과 복구 seed를 동시에 잃는다.

부트스트래핑이 어려운 이유는 compiler LOC가 많아서만이 아니다. Compiler가
자기 parser, semantic identity, MIR wire, ABI와 artifact transaction을 자기
자신에게 적용할 때 어느 stage의 fact인지 섞이지 않아야 한다. 특히 다음이
어렵다.

- native/self source identity epoch 차이;
- 새 Pergyra feature를 구형 seed가 아직 이해하지 못하는 전이 기간;
- C와 LLVM의 layout 및 pointer ABI 동등성;
- bootstrap 중 generated compiler가 다시 전체 graph를 반복 검증하는 비용;
- 오류가 seed, Stage 1 producer, Stage 2 consumer 중 어디에서 생겼는지 구분하는
  diagnostics.

그러므로 당분간의 목표는 C oracle 삭제가 아니라 역할 축소다.

```text
target semantic authority: admitted Pergyra owner
remaining C-owned production paths: explicit migration debt
production fallback: forbidden
C oracle: frozen seed/reference/recovery
C and LLVM: equal backend projections of one admitted Pergyra fact graph
```

## 7. 작업 기억 규칙

이 문서는 C oracle과 bootstrap에 대한 지속 계약이다. 이후 작업에서는 다음을
기본 판단으로 사용한다.

- C oracle을 없애는 변경을 self-host 진척으로 세지 않는다.
- 실제 C-owned production path가 Pergyra path로 대체될 때만
  `SUBSTITUTING`으로 센다.
- C oracle과 결과가 다르면 어느 쪽이 권위인지 먼저 정하지 않고, Pergyra fact
  owner와 observable contract를 기준으로 원인을 분리한다.
- 새 기능은 Pergyra semantic owner에서 시작하고 C/LLVM은 그 fact를 투영한다.
- Bootstrap gate는 같은 compiler binary와 admitted graph를 재사용하며 불필요한
  whole-graph 재검증을 반복하지 않는다.
