# 해체 정리 후속: 루트 세대와 저장된 지역 링크 검사

검토일: 2026-10-08 KST. 상태: **모델 범위 구현·집중/전체 통합 PASS**.
기준 HEAD: `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, 공유 dirty main.
계획: [전체 사슬·수정 범위](../agent_work_directives/ownership_teardown_root_epoch_2026-10-08.md).
앞선 [국소 갱신 감사](ownership_teardown_redteam_locality_2026-10-08.md)의
지적 중 현재 canonical 모델에서 닫을 수 있는 루트 재사용과 지역 읽기를 고쳤다.

## 수정한 사슬

`OwnershipTeardown.v`의 St가 루트 세대를 소유한다. 루트에 할당, 재부착,
루트 해제는 모두 같은 `root_resolves`를 통해 생존과 현재 세대를 요구한다.
`OpRootDrop`은 이제 `(id,epoch)`를 받고, 정확·고유 unit 은퇴와 같은
변환에서 해당 epoch만 증가시킨다. `RootNew` 및 비은퇴 연산은 기존 epoch를
보존한다. 예전 raw-id 연산이나 새 검사가 실패하면 예전 경로를 쓰는 fallback은 없다.

모든 St 생성자, invariant/재사용 증명, 정확 unit 존재 증명과 실행 증거를
옮겼다. `root_drop_always_succeeds`는 현재 세대를 사용하는 unit의 존재를
유지한다. `step_root_handles_resolve`는 세 종류 연산 전부를 검사하고,
`root_gen_mono_steps`와 `dropped_root_identity_dead_forever/never_acts`는
임의의 재선언·후속 실행에서도 옛 루트가 되살아나지 않음을 보인다.

노드의 저장된 지역 Link에는 `resolve_node`를 추가했다. 세대와 생존을
확인해 현재 immutable Node 값 또는 None을 반환한다. 반환 계약은
`resolve_node_some/none`으로 기존 resolves와 연결했고, 은퇴 이후 모든
후속 실행에서 None이라는 정리도 있다. `check_root` 역시 생존·세대
predicate와 iff로 연결한다. 이 검사들은 권한을 발급하거나 물리 loan을
잡아 주지 않으며, caller가 임의로 준 unit을 검증하는 전체 실행기도 아니다.

독립 typed 소비자에서 예전 루트 반례를 삭제하고 같은 재선언·노드 재사용
입력으로 현재 epoch 성공, 옛 epoch 할당·재부착·해제 거부, 미래 epoch 거부,
죽은 현재 루트 거부, 무관한 루트 정체성 보존을 증명했다. 노드 세대와 루트
세대도 별개의 정체성이다. 해제 권한이 없는 기존 forest Step 반례는 남겼다.

## 실행 비용 때문에 필요한 한 번의 수정

최초 fresh 추출의 고정 64회 root/node 재사용 검증은 98,304 field 및
16,384 teardown 사례를 끝낸 뒤에도 93초 CPU를 사용하며 실행 중이었다.
실제 추출 코드에서 phase1이 같은 `h src`를 liveness와 generation 조회에
각각 재평가했다. 이전 functional heap이 겹쳐진 입력이라 반복 작업이 커졌다.
해당 검증의 정확한 실행 경로·부모 PID를 확인하고 그 프로세스만 중단했다.
공유 WSL 프로세스 이름으로 종료하지 않았고 다른 레인은 중단하지 않았다.
중단은 FAIL/불완전 실행이며 메모리 안전 반례나 성공으로 세지 않는다.

phase1과 checked node read가 immutable slot을 한 번 읽고 그 값을 쓰도록
바꿨다. 모델 의미와 unit, 64회 입력, 시간 예산은 그대로다. 새 cache,
worker, 복사 경로나 안전 조건 완화는 없다. fresh 추출에서 같은 64회와
20,608개 정체성 검사가 완료됐고, source-slot 호출 횟수 oracle도 각 조회가
한 번임을 검사한다. 동일 값에 추가 읽기를 끼워 넣은 변이는 실제로 거부된다.
추가 전체 self-test 관측은 wall 0.10초/user CPU 0.09초/max RSS 5,212 KiB다
(`read-profile.log`, OCaml 4.14.1/O2). 중단된 실행과의 정밀 배율은 계산하지
않는다. 함수형 모델/OCaml의 비용 개선이지 native allocator나 GC 비교가 아니다.

## 관측한 검증과 남은 경계

집중 `tests/ownership_teardown_redteam_smoke.sh` PASS:

- fresh Rocq 9.3.0/rocqchk 3개 모듈, 가정 0, admit/unsafe 없음;
- 98,304 field 갱신, 16,384 teardown, 16 parent 사례, 고유성 4개;
- 64회 root/node 재사용, 20,608개 실제 checked-read 사례;
- 실제 규칙/검사 변이 11개: unique, ancestor, incoming, children,
  root alloc/attach/drop, epoch 증가/재선언 보존, node read, root check;
- 불필요한 복사와 반복 읽기 observer 변이 2개, 잘못된 CLI 거부;
- 동일 512/4096/16384 입력의 국소/scan cost 관측 15행. 큰 행의 필터·append,
  quadratic uniqueness observer의 제한은 여전히 남는다.

전체 fresh kernel PASS: canonical owner 64개와 permanent 소비자 7개,
합계 71개 및 approval export/binding 소비자. 전체 가정은 기존
`SlotCalculus.MaxSlotId`, `SlotCalculus.verify_token` 두 개뿐이다.
기존 mask/deprecation/extraction 경고와 default indices theory 의존은 로그에
보존했다. formal registration도 64개 owner를 확인하고 동일 71개 snapshot을
재검사했다. 재검사를 142개 독립 증명으로 세지 않는다. kernel refusal
self-test, 문서 품질·evidence-lifecycle 등록, scoped UTF-8/링크/공백,
Bash 문법·Make 진입점·formal CI 배선도 PASS다. CI wiring은 있으나
remote CI는 실행하지 않았다.

첫 formal 진입은 toolchain wrapper 없이 실행해 stable Rocq 없음으로 거부됐다.
SKIP으로 우회하지 않고 admitted wrapper로 다시 실행해 통과했다.

**OPEN:** caller의 affine 해제 권한과 실제 발급 경로, 안정된 loan/pin,
root/arena domain binding, 유한 세대 고갈, exact/unique indexed unit 발급과
비용 상한, 물리 배치·성장·해제 합성, finalizer/reentrancy와 동시성.
특히 반환 Node는 immutable 모델 값이지 살아 있는 raw pointer가 아니다.
doc 28과 실제 compiler ownership/DX implementation hold는 유지한다.
사용자 문법, compiler/C/LLVM/설치본, GUI readiness, SoT 상태는 바꾸지 않았다.
stage/commit/push도 하지 않았다.

관측물: `.tmp/ownership-teardown-root-epoch-2026-10-08/`의
`environment.log`, `kernel-extraction.log`, `controls.log`, `cost.jsonl`,
`mutation-*.log`, `redteam.log`, fresh `extracted/`.
중단된 판은 `repeated-read-{interrupted,environment}.log`와
`repeated-read-before.ml`로 분리 보존한다. 예전 감사의 receipt는 덮어쓰지 않았다.

## 최종 소스와 인계 경계

- canonical owner: `f260e3bb628e0c6f26bc597da7f14ac167a416aa85a3982c9b75f999bd05c98a`
- typed red team: `5d92cebe505837228093a53697343054796e72dcdd21a1e4dc6a66e5fafd700e`
- extraction 계약: `3f7ce538c90d4987458aef89dff35f5e6c11d91f5eb5fb3dc713d6acb994756d`
- OCaml observer: `932d3ab09d7a8a1a8cdb09ab99fe32e3d5e4f54721f2de9afc0d7f0695bde102`
- 집중 gate: `f23724d7e3136f450ef426be97d00fb01bfcd4615fdb54d944137dd4aeb151d3`

`kernel-full.log`, `formal.log`, `kernel-selftest.log`, `documentation.log`,
`lifecycle.log`, `read-profile.log`와 scoped validation을 추가로 보존했다.
검사한 71개 파일의 SHA를 현재 파일과 다시 대조해 모두 일치했다.
Windows Git 관측은 dirty 305개(시작 303개), staged paths 0개,
HEAD 변경 없음이다. 새 directive/audit 외의 기존 공유 변경을 버리지 않았다.
다음 반증 입력은 실제 authority issuer와 loan/pin을 가진 release 경계에서
남의 노드 정리/활성 접근 중 정리를 거부하면서 정당한 스코프 종료는 수행하는
것이다. 단순 Bool 권한 조건이나 Slot.s_val 해석을 발급 증명으로 대체하지 않는다.
