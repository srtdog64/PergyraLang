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

- [ ] **슬롯을 추상 자원 핸들로 일반화** — `Slot<T>`를 메모리 저장 상자보다 넓은 자원 핸들 개념으로 재정의. 장기적으로 `MemorySlot`, `DeviceSlot`, `SessionSlot`, `NetworkSlot`, `QubitSlot` 같은 자원 클래스로 확장 가능한 계약 정리
- [ ] **채널 의미론 강화** — 컨테이너/디바이스/원격 작업 간 비동기 제출, 대기, 수거, 후처리 흐름을 표현할 수 있도록 채널, 작업 그래프, 메시지 패싱 규칙 보강
- [ ] **effect/resource capability 표기 도입** — `local cpu`, `secure device`, `remote quantum backend`, `nondeterministic`, `collapse` 같은 제약을 타입 또는 효과 시스템으로 드러내는 설계 초안과 최소 구현
- [ ] **성능 목표를 orchestration overhead 중심으로 재정의** — 산술 microbenchmark보다 `submit`, `serialize`, `transfer`, `sync`, `retry/recovery` 비용을 줄이는 방향으로 벤치마크와 언어 목표 재정렬

## P2 — 배포 시작 시

- [ ] **문서-구현 동기화** — 테스트 수/기능 범위 일치, "지원/비지원" 명문화
- [ ] **SBOM (SPDX) + provenance (SLSA)** — 공급망 투명성
- [ ] **릴리스 아티팩트** — 서명된 바이너리, 체크섬, 설치 스크립트
- [ ] **3rd-party NOTICE** — OpenSSL/LLVM/pthread 라이선스 정리
