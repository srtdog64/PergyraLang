# Pergyra TODO (배포 준비)

## 완료 (P0 — 즉시 수정)

- [x] **`system()` 명령 주입 제거** — `_spawnvp`/`execvp`로 교체, 경로 검증 추가 (`pgy_path_is_safe`)
- [x] **AES-256 실구현** — XOR 가짜 암호를 FIPS 197 AES-256-CTR + HMAC-SHA256 인증으로 교체 (외부 의존성 없음)
- [x] **`auto __tmp` 제거** — `PGY_RESULT_TRY` 매크로에서 GCC 확장 `auto` 제거, C11 호환 (명시적 타입 파라미터)
- [x] **REPL 고정 파일명** — `_pgy_repl_tmp.*` → `TMPDIR/pgy_repl_{pid}.*` (PID 기반 유니크 경로)

## P1 — 다음 단계

- [ ] **CI (GitHub Actions) 구축** — Ubuntu + Windows 빌드 매트릭스, `make all && make test-all`, AddressSanitizer
- [ ] **CodeQL + secret scanning 활성화** — C/C++ 분석 모드, push protection
- [ ] **CHANGELOG.md + 버전 정책 수립** — SemVer, 릴리스 태깅 규칙
- [ ] **SECURITY.md** — 보안 취약점 제보 채널, 책임 있는 공개 정책

## P1.5 — 언어/컴파일러 보강

- [ ] **ability 기반 연산자 dispatch 고도화** — 현재는 `role/impl ability` 메서드에서 `operator_<suffix>_<Type>` alias를 합성해 C/LLVM이 정적으로 호출하는 방식. 장기적으로는 ability/vtable 기반의 직접 dispatch와 더 정교한 overload 우선순위 규칙이 필요
- [ ] **LLVM 연산자 오버로드 회귀 테스트 확장** — 현재 스모크는 `role IntMath for Int` 1건 중심. 비교 연산, 포함된 role, enum/custom type, namespace 경로까지 자동 테스트 확대

## P1.6 — 자원/오케스트레이션 방향 고정

- [ ] **Slot Protocol 고정** — 모든 슬롯이 공유하는 불변 계약(`Claim / Access / Mutate / Transfer / Release`)을 정의하고, `Slot<T> / SecureSlot<T> / QubitSlot`이 어디까지 지원하는지 현재 구현과 목표 구현을 명시
- [x] **Slot/View 계층 마감** — `Slot<T>`를 최소 소유 셀로 고정하고 `ReadView<T> / WriteView<T> / MoveToken<T>`를 권한 축소/이전 계층으로 정리
  - ~~현재 2단계~~: **완료** — `ReadView<T>` / `WriteView<T>` semantic type, non-owning release 금지, read/write 제한, `SecureSlot` source tracking, `MoveToken<T>` 재바인딩 transfer semantics, C lowering (pointer alias), LLVM lowering (pointer alias for view, structural copy for move)
  - 검증: semantic 158 passed, transpile 127 passed, 47 examples passed
  - 다음 단계: function/channel boundary에 view 규칙 연결, `MoveToken<T>` 재사용 진단 확대, LLVM secure-slot view 토큰 전파 (현재 시맨틱 레벨에서 차단됨)
- [ ] **슬롯을 추상 자원 핸들로 일반화** — `Slot<T>`를 메모리 저장 상자보다 넓은 자원 핸들 개념으로 재정의. 장기적으로 `MemorySlot`, `DeviceSlot`, `SessionSlot`, `NetworkSlot`, `QubitSlot` 같은 자원 클래스로 확장 가능한 계약 정리
- [ ] **채널 의미론 강화** — 컨테이너/디바이스/원격 작업 간 비동기 제출, 대기, 수거, 후처리 흐름을 표현할 수 있도록 채널, 작업 그래프, 메시지 패싱 규칙 보강
- [ ] **`Future<T>`를 transfer boundary로 고정** — `await Future<QubitSlot>`을 채널 `recv`와 같은 ownership 경계로 취급하고, anchored handle 금지 / fresh binding 권장 / inline use 제한 규칙 정리
- [ ] **effect/resource capability 표기 도입** — `local cpu`, `secure device`, `remote quantum backend`, `nondeterministic`, `collapse` 같은 제약을 타입 또는 효과 시스템으로 드러내는 설계 초안과 최소 구현
  - 현재 1단계: 함수 타입 메타데이터에 inferred effect mask(`secure`, `nondeterministic`, `collapse`) 저장 및 call graph 전파
  - 현재 1.5단계: `spawn/await/channel`에서 `remote`를 orchestration boundary effect로 추론, structured comment `@effects` metadata 병합, `RemoteFuture<T>`/`DeviceSlot<T>` semantic family 도입
  - 다음 단계: `DeviceSlot`/원격 자원과 연결된 `remote` 정밀화, 선언적 annotation 문법, effect mismatch 진단
- [ ] **성능 목표를 orchestration overhead 중심으로 재정의** — 산술 microbenchmark보다 `submit`, `serialize`, `transfer`, `sync`, `retry/recovery` 비용을 줄이는 방향으로 벤치마크와 언어 목표 재정렬

## P2 — 배포 시작 시

- [ ] **문서-구현 동기화** — 테스트 수/기능 범위 일치, "지원/비지원" 명문화
- [ ] **SBOM (SPDX) + provenance (SLSA)** — 공급망 투명성
- [ ] **릴리스 아티팩트** — 서명된 바이너리, 체크섬, 설치 스크립트
- [ ] **3rd-party NOTICE** — OpenSSL/LLVM/pthread 라이선스 정리
