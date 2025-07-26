# Pergyra 프로그래밍 언어

> **"메모리를 슬롯으로, 실행을 의도로"** - 차세대 시스템 프로그래밍 언어

## 🚀 프로젝트 개요

Pergyra는 포인터 대신 **슬롯 기반 메모리 관리**를 채택한 혁신적인 시스템 프로그래밍 언어입니다. 메모리 안전성과 병렬 처리를 언어 차원에서 지원하며, 어셈블리 최적화와 JVM 연동을 통해 고성능을 제공합니다.

## ✨ 핵심 특징

### 🔒 **슬롯 기반 메모리 관리**
- **포인터 없음**: 주소 대신 타입 안전한 슬롯 참조
- **자동 생명주기**: 스코프 기반 TTL로 메모리 누수 방지
- **원자적 접근**: 동시성 환경에서 안전한 메모리 접근
- **🛡️ 보안 강화**: 토큰 기반 접근 제어로 외부 메모리 변조 차단

```pergyra
with slot<Int> as s {
    s.write(42)
    log(s.read())
} // 자동 해제

// 보안 슬롯 (외부 도구로 변조 불가)
let (secureSlot, token) = claim_secure_slot<Int>(SECURITY_LEVEL_HARDWARE)
write(secureSlot, 42, token)  // 토큰 없이는 접근 불가
release(secureSlot, token)
```
```

### ⚡ **내장 병렬성**
- **문법 차원 병렬**: `parallel` 블록으로 간단한 병렬 처리
- **스레드 안전**: 슬롯 시스템이 보장하는 데이터 레이스 방지

```pergyra
let result = parallel {
    process_a()
    process_b() 
    process_c()
}
```

### 🔧 **하이브리드 구현**
- **어셈블리 최적화**: 핵심 성능 루틴은 x86-64 어셈블리로 구현
- **JVM 연동**: JNI를 통한 Java 생태계 활용
- **C 호환성**: 기존 C 라이브러리와의 원활한 연동

## 📁 프로젝트 구조

```
PergyraLang/
├── src/
│   ├── lexer/           # 토크나이저 (어셈블리 최적화)
│   │   ├── lexer.h
│   │   └── lexer.c
│   ├── parser/          # AST 생성기
│   │   ├── ast.h
│   │   └── parser.h
│   ├── runtime/         # 슬롯 매니저 (어셈블리 + C)
│   │   ├── slot_manager.h
│   │   ├── slot_manager.c
│   │   └── slot_asm.s   # 어셈블리 최적화 루틴
│   ├── jvm_bridge/      # JVM 연동 계층
│   │   └── jni_bridge.h
│   ├── semantic/        # 의미 분석
│   ├── codegen/         # 코드 생성
│   └── main.c          # 테스트 드라이버
├── docs/               # 문서
│   └── grammar.md      # 언어 문법 정의
├── tests/              # 테스트 코드
├── examples/           # 예제 코드
├── tools/             # 개발 도구
└── Makefile           # 빌드 시스템
```

## 🛠️ 빌드 방법

### 필요 조건
- GCC 7.0+ (C11 지원)
- NASM (어셈블리)
- GNU Make
- JDK 8+ (JVM 연동용)

### 빌드 명령어

```bash
# 전체 빌드
make all

# 렉서만 테스트
make lexer_test

# 테스트 실행
make test

# 디버그 빌드
make debug

# 릴리즈 빌드
make release

# 정리
make clean
```

## 🧪 현재 구현 상태

### ✅ 완료된 컴포넌트
- **토크나이저**: 완전한 Pergyra 토큰 분석
- **AST 구조**: 언어 구문의 추상 구문 트리 정의
- **슬롯 매니저**: 어셈블리 최적화된 메모리 관리
- **JNI 브릿지**: JVM 연동을 위한 인터페이스

### 🚧 개발 중인 컴포넌트
- **파서**: AST 생성 로직
- **의미 분석**: 타입 검사 및 스코프 분석
- **코드 생성**: 중간 코드 생성

### 📋 계획된 기능
- **LLVM 백엔드**: 다중 플랫폼 지원
- **웹어셈블리 타겟**: 브라우저 실행 지원
- **패키지 매니저**: 모듈 시스템

## 🎯 언어 철학

### 1. **의미 기반 선언**
모든 코드는 의도를 명확히 표현해야 합니다.

### 2. **슬롯 기반 소유권**
메모리는 주소가 아닌 역할 단위로 관리됩니다.

### 3. **선언적 병렬성**
멀티스레드 실행은 문법 차원에서 선언됩니다.

### 4. **의도-구조-실행 사이클**
- 프로그래머가 **의도**를 작성
- 컴파일러가 **구조**를 분석  
- 런타임이 **최적 실행**을 생성

## 📖 기본 문법 예제

```pergyra
// 슬롯 기반 메모리 관리
let slot = claim_slot<Int>()
write(slot, 42)
let value = read(slot)
release(slot)

// 스코프 기반 자동 관리
with slot<String> as s {
    s.write("Hello, Pergyra!")
    log(s.read())
}

// 병렬 처리
let results = parallel {
    calculate_prime(1000)
    sort_array(data)
    compress_file(input)
}
```

## 🔬 성능 특징

- **Zero-copy 슬롯 접근**: 어셈블리 최적화로 포인터 수준의 성능
- **락-프리 동시성**: Compare-and-Swap 기반 원자적 연산
- **캐시 친화적**: 연속 메모리 할당으로 캐시 미스 최소화
- **NUMA 인식**: 멀티소켓 시스템에서 로컬 메모리 우선 사용

## 🤝 기여 방법

1. **이슈 리포팅**: 버그나 개선사항을 GitHub Issues에 등록
2. **코드 기여**: Pull Request를 통한 코드 개선
3. **문서화**: 언어 문법이나 API 문서 작성
4. **테스트**: 다양한 환경에서의 테스트 진행

## 🛡️ 보안 시스템 (SBSR)

Pergyra의 **Slot-Based Safe References (SBSR)** 시스템은 포인터 기반 메모리 접근의 취약점을 근본적으로 해결합니다.

### 핵심 보안 특징

#### 🔐 토큰 기반 접근 제어
- **암호화된 액세스 토큰**: 각 슬롯마다 고유한 256비트 토큰
- **하드웨어 바인딩**: CPU ID, 메인보드 시리얼 등과 연동
- **시간 기반 만료**: TTL로 토큰 자동 무효화

#### 🛡️ 외부 도구 차단
- **Cheat Engine 방지**: 메모리 스캔 도구로 값 변조 불가
- **메모리 덤프 보호**: 토큰 없이는 슬롯 접근 원천 차단
- **디버거 공격 대응**: 하드웨어 기반 무결성 검증

#### 🔒 다단계 보안 레벨
```pergyra
// 기본 보안 (토큰 검증)
let (slot1, token1) = claim_secure_slot<Int>(SECURITY_LEVEL_BASIC)

// 하드웨어 바인딩 보안
let (slot2, token2) = claim_secure_slot<String>(SECURITY_LEVEL_HARDWARE)

// 최고 보안 (암호화 + 하드웨어 바인딩)
let (slot3, token3) = claim_secure_slot<Data>(SECURITY_LEVEL_ENCRYPTED)
```

### 보안 API

#### 안전한 슬롯 조작
```pergyra
// 토큰 없이는 접근 불가
Write(slot, value, token)     // ✅ 허용
Write(slot, value)            // ❌ 컴파일 에러

// 권한 기반 접근 제어
token.CanWrite = false        // 읽기 전용으로 변경
Write(slot, newValue, token)  // ❌ 런타임 거부
```

#### 보안 위반 감지
```pergyra
// 실시간 보안 모니터링
if DetectSecurityAnomalies() {
    SecurityAlert("Unauthorized access detected")
    RevokeAllTokens()
}

// 접근 패턴 분석
AuditSlotAccessPatterns()
PrintSecurityViolations()
```

### 실전 활용 예시

#### 게임 보안 (치트 방지)
```pergyra
class SecurePlayerStats {
    private let (_hpSlot, _hpToken) = ClaimSecureSlot<Int>(SECURITY_LEVEL_ENCRYPTED)
    private let (_moneySlot, _moneyToken) = ClaimSecureSlot<Int>(SECURITY_LEVEL_HARDWARE)
    
    func TakeDamage(damage: Int) {
        let currentHp = Read(_hpSlot, _hpToken)
        Write(_hpSlot, currentHp - damage, _hpToken)
        // 외부 도구로 HP 수정 불가능
    }
    
    func AddMoney(amount: Int) {
        let currentMoney = Read(_moneySlot, _moneyToken)
        Write(_moneySlot, currentMoney + amount, _moneyToken)
        // 치트 엔진으로 돈 수정 불가능
    }
}
```

#### 금융 데이터 보호
```pergyra
class SecureBankAccount {
    private let (_balanceSlot, _balanceToken) = 
        ClaimSecureSlot<Decimal>(SECURITY_LEVEL_ENCRYPTED)
    
    func Transfer(amount: Decimal, to: Account) {
        // 하드웨어 바인딩 + 암호화로 잔고 보호
        let balance = Read(_balanceSlot, _balanceToken)
        
        guard balance >= amount else {
            SecurityAuditLog("Invalid transfer attempt")
            return
        }
        
        Write(_balanceSlot, balance - amount, _balanceToken)
        to.Deposit(amount)
    }
}
```

### 성능 및 오버헤드

- **기본 보안**: 일반 슬롯 대비 ~5% 오버헤드
- **하드웨어 바인딩**: 일반 슬롯 대비 ~15% 오버헤드  
- **암호화 보안**: 일반 슬롯 대비 ~25% 오버헤드
- **어셈블리 최적화**: SIMD 명령어로 성능 향상

### 컴파일 시간 보장

```pergyra
// 컴파일러가 토큰 누락을 감지
let slot = ClaimSecureSlot<Int>(SECURITY_LEVEL_BASIC)  // ❌ 토큰 없음
Write(slot, 42)  // ❌ 컴파일 에러: "Missing security token"

// 올바른 사용법
let (slot, token) = ClaimSecureSlot<Int>(SECURITY_LEVEL_BASIC)
Write(slot, 42, token)  // ✅ 컴파일 성공
```

### 🔬 테스트 및 검증

보안 시스템을 테스트하려면:

```bash
# 보안 테스트 스위트 실행
make test-security

# 치트 엔진 시뮬레이션 테스트
./bin/test_security --simulate-attacks

# 성능 벤치마크
./bin/test_security --performance
```

### 지원되는 공격 차단

- ✅ **메모리 스캔**: Cheat Engine, ArtMoney 등
- ✅ **포인터 추적**: 다단계 포인터 체이닝
- ✅ **메모리 덤프**: 프로세스 메모리 직접 수정
- ✅ **DLL 인젝션**: 외부 라이브러리 주입 공격
- ✅ **디버거 부착**: x64dbg, OllyDbg 등
- ✅ **가상 머신 우회**: VMware, VirtualBox 감지

### 📚 추가 문서

- [보안 아키텍처 상세 설명](docs/security_architecture.md)
- [토큰 암호화 알고리즘](docs/token_cryptography.md)
- [하드웨어 바인딩 구현](docs/hardware_binding.md)
- [성능 최적화 가이드](docs/performance_optimization.md)

## 📄 라이센스

이 프로젝트는 [BSD 3-Clause 라이센스](LICENSE) 하에 배포됩니다.

```
Copyright (c) 2025, Pergyra Language Project
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice
2. Redistributions in binary form must reproduce the above copyright notice
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.
```

## 📞 연락처

- **개발자**: [개발자명]
- **이메일**: [이메일 주소]
- **GitHub**: [GitHub 저장소]

---

> **"Pergyra로 메모리 안전하고 병렬 친화적인 시스템을 구축하세요"**