# Compiler Architecture Comparisons

Updated: 2026-08-26

이 디렉터리는 다른 컴파일러의 구현을 같은 기준으로 분해하고, 그 구현이 지키는
불변식 가운데 Pergyra에 필요한 것만 선별하기 위한 참고 자료다. 외부 구조를
Pergyra의 새 의미론 권위로 만들거나, 유명한 구현의 물리적 모양을 그대로 복제하는
문서가 아니다.

## Objective card

- **Objective:** 주요 컴파일러의 fact identity, IR 수명, 계산 스케줄, 무효화,
  검증, backend 및 bootstrap 경계를 비교하고 Pergyra 도입 후보를 판정한다.
- **Priority:** semantic identity와 one SoT > owner-directed fact > fallback 제거 >
  negative gate > 성능 > 관습적인 컴파일러 구조.
- **Fact owner:** 외부 구현 사실은 각 프로젝트의 공식 문서와 소스가 소유한다.
  Pergyra의 현재 사실은 현재 source, SoT registry, protocol/ABI registry와
  executable gate가 소유한다.
- **Last legitimate consumer:** `pergyra_adoption_map.md`. 개별 비교 문서는 판단의
  입력일 뿐 구현 순서나 완료 상태를 소유하지 않는다.
- **Forbidden fallback:** 이름이나 문법이 비슷하다는 이유로 구조를 이식하기,
  비교 문서를 Pergyra 의미론 권위로 읽기, 측정 없이 query/cache/dialect 체계를
  열기, 기존 owner 옆에 두 번째 권위를 두기.
- **Verification / falsifier:** 외부 사실 주장에는 공식 1차 자료가 있어야 한다.
  도입 제안은 Pergyra owner, 허용되는 consumer, 거부할 fallback과 확인 gate를
  지목해야 한다. 이 중 하나라도 없으면 `조건부` 또는 `보류`다.

## 비교 범위

| 문서 | 주로 보는 문제 | Pergyra와의 직접 접점 |
|---|---|---|
| [Clang + LLVM](llvm_clang.md) | source-faithful AST, Sema, analysis invalidation, backend | source diagnostics, MIR/DIR consumer discipline |
| [rustc](rustc.md) | stable identity, demand-driven query, incremental dependency, HIR/MIR | owner lookup, dependency and invalidation contract |
| [Swift](swift.md) | request evaluator, cycle, AST/SIL/LLVM lowering, ownership verifier | typed request, orchestration cycle, verifier |
| [GHC](ghc.md) | Core/STG/Cmm information lifetime, repeated typed rewrites | lowering loss ledger, semantic core preservation |
| [MLIR](mlir.md) | extensible operations, traits/interfaces, nested passes, verify-each | AIR verifier boundary, interface-shaped contracts |
| [Zig](zig.md) | ZIR/AIR granularity, interning, lazy discovery, job queue, bootstrap | self-host granularity, identity handles, codegen work ownership |
| [Pergyra adoption map](pergyra_adoption_map.md) | 채택·조건부·거부 판정 | 현재 owner와 다음 falsifier 연결 |

## 공통 분해 축

각 문서는 다음 질문에 답한다.

1. production entrypoint에서 backend까지 어떤 spine을 거치는가?
2. 선언, 타입, routine, analysis 결과의 identity는 무엇인가?
3. 각 IR은 무엇을 보존하며 어느 시점부터 무엇을 잃어도 되는가?
4. 계산은 순차 pass, demand-driven request/query, job graph 중 무엇으로 움직이는가?
5. 변환 뒤 어떤 분석을 보존하거나 무효화하는가?
6. 잘못된 중간 상태는 언제, 어떤 verifier/diagnostic으로 막히는가?
7. target, ABI, codegen과 frontend 의미론의 마지막 경계는 어디인가?
8. self-host, bootstrap, incremental build가 의미론 권위와 섞이지 않는가?
9. 얻는 성질을 위해 어떤 복잡성과 실패 모드를 감수하는가?
10. Pergyra가 가져와야 할 것은 구현 기법인가, 더 작은 불변식인가?

## 판정 언어

- **채택:** 현재 Pergyra owner와 맞고 기존 active rung에서 작은 계약 또는 gate로
  닫을 수 있다.
- **조건부:** 반복 비용이나 실제 누락 fact가 측정될 때만 검토한다.
- **거부:** Pergyra의 one SoT, fail-closed, DX 또는 self-host progress guard와
  충돌한다.
- **참고:** 좋은 성질은 분명하지만 현재 compiler rung과 직접 연결되지 않는다.

`채택`은 곧바로 구현하라는 작업 큐가 아니다. active self-host rung이 해당 seam에
도달했을 때 적용할 기본 판단이다.

## 증거와 갱신 규칙

- 원래 작성 기록은 upstream rolling documentation과 공식 source를
  2026-08-26에 확인했다고 명시한다. 이 날짜는 원 작성자의 조사 기록이며,
  2026-09-05 통합 검토에서 원전을 다시 검증했다는 뜻은 아니다. 이번 검토는
  로컬 참조와 Pergyra의 현재 owner/과거 수치 구분만 확인했다.
  upstream `main`/`master`는 고정 ABI가 아니므로 실제 도입 시에는 원전과
  링크를 다시 검증한다.
- 논문이나 발표는 보조 설명으로만 쓸 수 있다. 현재 구현 사실은 공식 source나
  maintainer 문서로 확인한다.
- 외부 컴파일러가 쓰는 명칭과 Pergyra 명칭을 동일시하지 않는다. 예를 들어 Zig의
  AIR와 Pergyra의 AIR은 같은 이름일 뿐 책임이 다르다.
- 비교에서 발견한 아이디어가 Pergyra registry 상태나 진척률을 변경하지 않는다.
  실제 consumer migration, missing-fact failure, old-path deletion과 negative gate가
  있어야만 해당 owner row를 닫을 수 있다.
