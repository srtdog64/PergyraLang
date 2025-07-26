# Pergyra 구조화된 주석 시스템 설계

## 1. 기본 컨셉

Pergyra는 주석을 단순한 텍스트가 아닌 **구조화된 메타데이터**로 취급합니다.

## 2. 주석 문법

### 2.1 기본 구조화 주석

```pergyra
/// [What]: 사용자의 체력을 안전하게 관리하는 보안 슬롯
/// [Why]: 외부 메모리 조작 도구(치트엔진 등)로부터 보호 필요
/// [Alt]: 일반 변수 사용 시 메모리 해킹에 취약
/// [Next]: 추후 암호화 레벨을 동적으로 조정하는 기능 추가
class SecureHealth {
    private let (_hpSlot, _hpToken) = ClaimSecureSlot<Int>(SECURITY_LEVEL_HARDWARE)
}
```

### 2.2 메서드 레벨 주석

```pergyra
/// [What]: 플레이어가 데미지를 받아 체력 감소
/// [Why]: 토큰 검증을 통해 무단 체력 수정 방지
func TakeDamage(damage: Int) {
    let currentHp = Read(_hpSlot, _hpToken)
    Write(_hpSlot, currentHp - damage, _hpToken)
}
```

### 2.3 블록 레벨 주석

```pergyra
Parallel {
    /// [What]: 세 가지 독립적인 작업을 병렬 실행
    /// [Why]: 각 작업이 서로 의존성이 없어 동시 실행 가능
    ProcessA()
    ProcessB()
    ProcessC()
}
```

## 3. 언어 수준 지원

### 3.1 컴파일러 검증

```pergyra
@[require_docs(what, why)]  // What과 Why는 필수
class CriticalSystem {
    /// [What]: 시스템의 핵심 상태 관리
    // 컴파일 에러: [Why] 태그가 없음!
}
```

### 3.2 조건부 문서화

```pergyra
@[require_docs(what, why, alt: if_complex)]
func ComplexAlgorithm() {
    // 복잡도가 높으면 Alt 섹션도 필수
}
```

## 4. 구조화된 주석 타입

### 4.1 클래스/구조체용

```pergyra
/// [What]: 무엇을 표현/관리하는가
/// [Why]: 왜 이 설계를 선택했는가
/// [Invariants]: 유지되어야 할 불변조건
/// [Example]: 사용 예시
@[class_doc]
class BankAccount { }
```

### 4.2 함수/메서드용

```pergyra
/// [What]: 무엇을 하는가
/// [Why]: 왜 이 방식으로 구현했는가  
/// [Params]: 매개변수 설명
/// [Returns]: 반환값 설명
/// [Throws]: 예외 상황
@[func_doc]
func Transfer() { }
```

### 4.3 알고리즘/복잡한 로직용

```pergyra
/// [What]: 알고리즘 개요
/// [Why]: 선택 이유
/// [Alt]: 다른 알고리즘 옵션들
/// [Complexity]: 시간/공간 복잡도
/// [Next]: 최적화 가능성
@[algorithm_doc]
func QuickSort<T>() { }
```

## 5. 도구 통합

### 5.1 IDE 지원

```pergyra
// IDE에서 자동 완성
/// [What]: |커서|
/// [Why]: 
func NewFunction() { }
```

### 5.2 문서 생성

```bash
pergyra doc --format=markdown --output=docs/
# 구조화된 주석을 파싱하여 문서 자동 생성
```

### 5.3 린터 규칙

```yaml
# .pergyra-lint.yml
doc_rules:
  require_what: true
  require_why: true
  require_alt: 
    - for_complexity: "> O(n)"
    - for_loc: "> 50"
  require_next:
    - for_todo_comments: true
```

## 6. 실제 구현 예시

### 6.1 파서 확장

```c
// parser.c에 추가
typedef enum {
    DOC_TAG_WHAT,
    DOC_TAG_WHY,
    DOC_TAG_ALT,
    DOC_TAG_NEXT,
    DOC_TAG_PARAMS,
    DOC_TAG_RETURNS,
    DOC_TAG_THROWS,
    DOC_TAG_COMPLEXITY,
    DOC_TAG_INVARIANTS,
    DOC_TAG_EXAMPLE
} DocTagType;

typedef struct {
    DocTagType type;
    char* content;
} DocTag;

typedef struct {
    DocTag** tags;
    size_t tag_count;
    bool is_required[DOC_TAG_COUNT];
} StructuredComment;
```

### 6.2 렉서 토큰 추가

```c
// 구조화된 주석 토큰
TOKEN_DOC_COMMENT,      // ///
TOKEN_DOC_TAG_WHAT,     // [What]:
TOKEN_DOC_TAG_WHY,      // [Why]:
TOKEN_DOC_TAG_ALT,      // [Alt]:
TOKEN_DOC_TAG_NEXT,     // [Next]:
```

## 7. 고급 기능

### 7.1 조건부 문서화

```pergyra
/// [What]: 캐시 관리 시스템
/// [Why]: 성능 최적화
/// [Alt if BENCHMARK_MODE]: 
///   - LRU 캐시: 메모리 효율적이지만 느림
///   - FIFO 캐시: 빠르지만 히트율 낮음
/// [Next if PROFILE_DATA > threshold]: 
///   적응형 캐시 전략으로 전환
class AdaptiveCache<K, V> { }
```

### 7.2 문서 상속

```pergyra
/// [What]: 기본 컨테이너 인터페이스
/// [Why]: 모든 컬렉션의 공통 동작 정의
trait Container<T> {
    /// [inherit_docs]
    func Add(item: T)
}

class List<T> : Container<T> {
    /// [What]: @inherited + 리스트 끝에 추가
    /// [Why]: @inherited
    /// [Complexity]: O(1) amortized
    func Add(item: T) { }
}
```

### 7.3 문서 검증

```pergyra
@[test_docs]
func TestDocumentation() {
    // 문서의 예제 코드가 실제로 컴파일되는지 검증
    assert_doc_examples_compile(BankAccount)
    
    // 문서의 복잡도 표기가 실제와 일치하는지 검증
    assert_complexity_matches(QuickSort, "O(n log n)")
}
```

## 8. 실용적 접근

### 8.1 점진적 도입

```pergyra
// 레벨 1: 권장사항
@[doc_level(1)]
module BasicModule { }

// 레벨 2: What/Why 필수
@[doc_level(2)]
module ImportantModule { }

// 레벨 3: 전체 태그 필수
@[doc_level(3)]
module CriticalModule { }
```

### 8.2 도메인별 커스터마이징

```pergyra
// 게임 개발용
@[game_doc_tags(what, why, performance, memory)]
class GameObject { }

// 금융 시스템용
@[finance_doc_tags(what, why, compliance, audit_trail)]
class Transaction { }

// 임베디드용
@[embedded_doc_tags(what, why, memory_usage, power_consumption)]
class DeviceDriver { }
```

## 9. 구현 로드맵

1. **Phase 1**: 기본 구조화 주석 파싱
2. **Phase 2**: 컴파일러 검증 통합
3. **Phase 3**: IDE 플러그인 개발
4. **Phase 4**: 문서 생성 도구
5. **Phase 5**: 고급 기능 (조건부, 상속 등)

이런 구조화된 주석 시스템은 Pergyra를 단순한 프로그래밍 언어를 넘어 **자기 문서화 언어**로 만들 수 있습니다!
