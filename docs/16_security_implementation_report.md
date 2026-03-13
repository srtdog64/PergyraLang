# Pergyra 보안 시스템 구현 보고서

## 개요

이 문서는 Pergyra의 **SBSR (Slot-Based Safe References)** 보안 시스템 구현 범위를 정리한다.
현재 구현은 토큰 기반 접근 제어, 하드웨어 지문, 보안 이벤트 로깅, 보안 테스트 스위트를 포함한다.

## 핵심 보안 기능

### 1. 토큰 기반 접근 제어 (Token Capability System)
- **256비트 암호화 토큰**: 각 슬롯마다 고유한 AES-256 암호화 토큰
- **하드웨어 바인딩**: CPU ID, 메인보드 시리얼, MAC 주소와 연동
- **상수 시간 검증**: 타이밍 공격 방지를 위한 constant-time 비교
- **토큰 만료**: TTL 기반 자동 무효화

### 2. 하드웨어 지문(Hardware Fingerprinting)
```c
typedef struct {
    uint64_t cpuId;         // CPU 시리얼/특성 해시
    uint64_t boardId;       // 메인보드 시리얼 해시  
    uint64_t macAddress;    // 주 MAC 주소
    uint32_t platformHash;  // 플랫폼별 해시
    uint32_t checksum;      // 무결성 검증
} HardwareFingerprint;
```

### 3. 다단계 보안 레벨
- **BASIC**: 기본 토큰 검증 (~5% 오버헤드)
- **HARDWARE**: 하드웨어 바인딩 추가 (~15% 오버헤드)
- **ENCRYPTED**: 암호화 + 하드웨어 바인딩 (~25% 오버헤드)

## 대응 대상으로 가정한 접근 유형

### 직접 대응
- **Cheat Engine**: 메모리 스캐닝 및 값 변조
- **ArtMoney**: 게임 메모리 해킹 도구
- **메모리 덤프**: 프로세스 메모리 직접 수정
- **포인터 추적**: 다단계 포인터 체이닝
- **DLL 인젝션**: 외부 라이브러리 주입 공격

### 제한적 대응
- **디버거 부착**: 감지 가능하나 완전 차단은 어려움
- **가상머신 우회**: 하드웨어 지문으로 일부 대응

## 구현된 파일 구조

```
src/runtime/
├── slot_security.h         # 보안 시스템 헤더 (7.57KB)
├── slot_security.c         # 보안 시스템 구현 (17.87KB)
├── slot_manager.h          # 확장된 슬롯 매니저 헤더 
├── slot_manager.c          # 보안 기능 통합 슬롯 매니저 (36.19KB)

src/
├── test_security.c         # 포괄적 보안 테스트 스위트 (15.14KB)

example/
├── secure_slots.pgy        # 보안 슬롯 사용법 예제 (6.86KB)
```

## API 설계

### 보안 슬롯 생성
```c
// C 레벨 API
SlotError SlotClaimSecure(SlotManager *manager, TypeTag type, 
                         SecurityLevel level, SlotHandle *handle, 
                         TokenCapability *token);

// Pergyra 언어 레벨 API
let (secureSlot, token) = ClaimSecureSlot<Int>(SECURITY_LEVEL_HARDWARE);
```

### 토큰 기반 조작
```c
// 토큰 없이는 접근 불가
SlotError SlotWriteSecure(SlotManager *manager, const SlotHandle *handle,
                         const void *data, size_t dataSize, 
                         const TokenCapability *token);

// Pergyra 문법
Write(slot, value, token);  // 허용
Write(slot, value);         // 컴파일 에러
```

### 스코프 기반 자동 관리
```pergyra
with secure_slot<Int>(SECURITY_LEVEL_ENCRYPTED) as s {
    Write(s, 42);
    Log(Read(s));
} // 자동 해제 + 토큰 검증
```

## 테스트 시스템

### 8가지 테스트 카테고리
1. **보안 컨텍스트 생명주기**
2. **하드웨어 지문 생성/검증**
3. **토큰 생성/검증 작업**
4. **보안 슬롯 매니저 운영**
5. **보안 위반 감지**
6. **스코프 기반 슬롯 관리**
7. **Pergyra 언어 레벨 API**
8. **성능 및 스트레스 테스트**

### 실행 방법
```bash
# 보안 테스트 스위트 실행
make test-security

# 치트 엔진 시뮬레이션
./bin/test_security --simulate-attacks

# 성능 벤치마크
./bin/test_security --performance
```

## 성능 최적화

### 어셈블리 최적화 함수
```c
extern bool SecureCompareConstantTime(const void *a, const void *b, size_t size);
extern void SecureMemoryBarrier(void);
extern uint64_t SecureTimestamp(void);
```

### 메모리 보호
- **mlock()**: 토큰 페이지 스왑 방지
- **explicit_bzero()**: 안전한 메모리 소거
- **AEAD 암호화**: 인증된 암호화로 변조 감지

### 동시성 지원
- **락프리 토큰 검증**: Compare-and-Swap 최적화
- **스레드별 슬롯 풀**: 컨텐션 최소화
- **원자적 카운터**: 통계 수집 최적화

## 보안 모니터링

### 실시간 보안 감시
```c
// 이상 징후 탐지
bool SlotManagerDetectAnomalies(SlotManager *manager);

// 보안 이벤트 로깅
void SlotManagerLogSecurityEvent(SlotManager *manager, const char *event,
                                uint32_t slotId, const char *details);

// 통계 출력
void SlotManagerPrintSecurityStats(const SlotManager *manager);
```

### 감지 가능한 공격 패턴
- 과도한 보안 위반 시도
- 비정상적인 빠른 연속 접근
- 하드웨어 바인딩 불일치
- 토큰 재생 공격 시도

## 활용 예시

### 게임 보안 (치트 방지)
```pergyra
class SecurePlayerStats {
    private let (hpSlot, hpToken) = ClaimSecureSlot<Int>(SECURITY_LEVEL_ENCRYPTED);
    private let (moneySlot, moneyToken) = ClaimSecureSlot<Int>(SECURITY_LEVEL_HARDWARE);

    func TakeDamage(damage: Int) {
        let currentHp = Read(hpSlot, hpToken);
        Write(hpSlot, currentHp - damage, hpToken);
    }
}
```

### 금융 데이터 보호
```pergyra  
class SecureBankAccount {
    private let (balanceSlot, balanceToken) =
        ClaimSecureSlot<Decimal>(SECURITY_LEVEL_ENCRYPTED);

    func Transfer(amount: Decimal, to: Account) {
        let balance = Read(balanceSlot, balanceToken);
        // ... 안전한 이체 로직
    }
}
```

## 문서화

### README.md 업데이트
- 🛡️ 보안 시스템 소개 섹션 추가
- 토큰 기반 접근 제어 설명
- 실전 활용 예제 포함
- 지원되는 공격 차단 목록

### 빌드 시스템 통합
- OpenSSL 링크 추가 (`-lssl -lcrypto`)
- 보안 테스트 타겟 추가
- 의존성 관리 개선

## 구현 요약

### 현재 포함된 기능
1. **토큰 기반 보안 시스템**: 256비트 암호화 토큰
2. **하드웨어 바인딩**: CPU/보드/네트워크 지문
3. **다단계 보안 레벨**: BASIC/HARDWARE/ENCRYPTED
4. **크로스 플랫폼 지원**: Windows/Linux 하드웨어 감지
5. **성능 최적화**: 어셈블리 + 상수시간 알고리즘
6. **포괄적 테스트**: 8개 카테고리 테스트 스위트
7. **언어 통합**: Pergyra 문법에 자연스럽게 통합
8. **실시간 모니터링**: 보안 위반 감지 및 로깅

### 현재 문서의 범위
- 토큰이 없는 SecureSlot 접근은 언어 규칙과 런타임 검증의 대상이다.
- 하드웨어 지문과 암호화는 보안 레벨에 따라 선택적으로 적용된다.
- 공격 대응 범위는 구현과 테스트에 포함된 시나리오 기준으로만 해석해야 한다.
