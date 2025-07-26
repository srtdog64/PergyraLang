# Pergyra 언어 문법 정의

## 1. 핵심 철학
- **Slot-based Memory Management**: 포인터 대신 슬롯으로 메모리 관리
- **의미 기반 선언**: 모든 코드는 의미 단위로 구성
- **내장 병렬성**: 언어 문법 차원에서 병렬 실행 지원
- **타입 안전성**: 컴파일 타임 타입 검증

## 2. 기본 문법

### 2.1 슬롯 관리
```pergyra
let slot = claim_slot<Type>()
write(slot, value)
let val = read(slot)
release(slot)
```

### 2.2 스코프 기반 슬롯
```pergyra
with slot<Type> as s {
    s.write(42)
    log(s.read())
}
```

### 2.3 병렬 실행
```pergyra
let result = parallel {
    A()
    B()
    C()
}
```

## 3. 토큰 정의

### 3.1 키워드 (Keywords)
- `let`: 변수/슬롯 선언
- `claim_slot`: 슬롯 할당
- `write`: 슬롯에 값 쓰기
- `read`: 슬롯에서 값 읽기
- `release`: 슬롯 해제
- `with`: 스코프 기반 슬롯 선언
- `as`: 슬롯 별칭
- `parallel`: 병렬 실행 블록
- `log`: 출력 함수

### 3.2 연산자 (Operators)
- `=`: 할당
- `()`: 함수 호출/그룹핑
- `{}`: 블록
- `<>`: 타입 매개변수
- `.`: 멤버 접근
- `;`: 문장 종료

### 3.3 리터럴 (Literals)
- 정수: `42`, `-10`, `0`
- 문자열: `"hello"`, `'world'`
- 부울: `true`, `false`

### 3.4 식별자 (Identifiers)
- 변수명: `variable_name`, `slotA`
- 함수명: `function_name`, `process_data`
- 타입명: `Type`, `Int`, `String`, `Vector`

## 4. 타입 시스템

### 4.1 기본 타입
- `Int`: 32비트 정수
- `Long`: 64비트 정수  
- `Float`: 32비트 부동소수점
- `Double`: 64비트 부동소수점
- `String`: 문자열
- `Bool`: 부울값

### 4.2 슬롯 타입
- `slot<Type>`: 타입이 지정된 슬롯
- 예: `slot<Int>`, `slot<String>`

## 5. 메모리 모델

### 5.1 슬롯 테이블 구조
```
SlotId | TypeTag | Occupied | DataBlockRef | TTL
```

### 5.2 생명주기
- **스코프 기반**: `with` 블록 종료 시 자동 해제
- **명시적 해제**: `release()` 호출
- **TTL 기반**: 시간 초과 시 자동 해제
