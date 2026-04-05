# Pergyra Role/Interface System Design

## Overview
이 문서는 Pergyra의 역할 기반 조합 시스템을 정리한다.
핵심은 다음 네 층이다.

- `struct`: 최소 값 타입. 복사/비교가 자연스러운 데이터
- `subject`: ability를 수행하는 주체 타입. 상태와 identity를 가진다
- `class`: 값으로 들고 쓰는 도구/사물 타입. 메서드는 가질 수 있지만 identity/zone/action 의미는 붙지 않는다
- `ability`: subject가 수행할 수 있는 행위 계약
- `role`: 특정 subject가 ability를 어떻게 수행하는지 바인딩하는 계층

즉 Pergyra에서 행위의 중심은 `subject + ability + role` 조합이고,
`struct`는 그 아래에서 쓰이는 값 타입이다.

## Current Implementation Surface (2026-04-03)

현재 파서/시맨틱/백엔드에서 지원하는 문법은 다음 형태다.

```pergyra
ability Damageable {
    require health: Int;
    func TakeDamage(amount: Int) -> Void;
}

role PlayerDamageable for Player {
    impl ability Damageable {
        func TakeDamage(amount: Int) -> Void {
            Log(amount);
        }
    }
}

role MonsterCombat for Monster {
    include role BasicDamageable;
    override func TakeDamage(amount: Int) -> Void {
        Log(amount);
    }
}
```

지원되는 요소:
- `ability` + `require`
- `role ... for Type`
- `impl ability ...`
- `include role ...`
- `override func ...`

아래 **Core Concepts** 섹션은 설계 방향 설명이며, 일부 예시는 현재 문법과 다를 수 있다.
특히 `&mut self`, `impl Trait`, `Result<(), Error>` 같은 표기는 현재 stable current surface라기보다 장기 설계 메모에 가깝다.

## Core Concepts (Design Notes)

### 1. **struct** (최소 값 타입)
- 순수 데이터
- 복사/비교 가능
- identity 없음
- 배열 원소, 설정값, 좌표, 상태 스냅샷 같은 값 표현에 적합

```pergyra
struct Vec3
{
    x: Float
    y: Float
    z: Float
}
```

### 2. **subject** (주체 타입)
- 상태를 가진 객체
- identity를 가진다
- plain value보다 "살아있는 객체"에 가깝다
- ability의 실제 수행 주체가 된다
- subject의 행위는 항상 `self` 객체 셀 위에서 실행된다고 본다

현재 stable surface는 `subject`와 `class`를 구분한다. 아래 예시는 주체 타입을 설명하므로 `subject`를 쓰는 편이 맞다.

```pergyra
subject Player
{
    _healthSlot: SecureSlot<Int>
    _token: Token
    name: String
}
```

### 3. **ability** (행위의 명세)
- 객체가 무엇을 할 수 있는지 정의
- 단순 인터페이스보다 강하다
- `self` 객체가 어떤 자원 셀과 규율 위에서 움직이는지 전제한다
- `require` 키워드로 ability 수행에 필요한 자원/필드 명시
- 기본 구현 제공 가능

```pergyra
ability Damageable
{
    require _healthSlot: SecureSlot<Int>
    require _token: Token
    
    func TakeDamage(&mut self, amount: Int)
    func Heal(&mut self, amount: Int)
    
    // 기본 구현
    func IsAlive(&self) -> Bool
    {
        return Read(self._healthSlot, self._token) > 0
    }
}

ability Attackable
{
    require _attackPower: Int
    
    func Attack(&self, target: &mut impl Damageable) -> Int
}
```

### 4. **role** (행위의 구체적 구현)
- 특정 subject가 특정 ability를 어떻게 수행하는지 구현
- subject와 ability를 연결하는 접착 계층
- 같은 subject가 여러 role을 통해 다른 ability 조합을 가질 수 있다
- role은 타입이 아니라 "그 subject가 어떤 자격으로 행동하는가"를 나타낸다

```pergyra
role PlayerDamageable for Player
{
    impl ability Damageable
    {
        func TakeDamage(&mut self, amount: Int)
        {
            let current = Read(self._healthSlot, self._token)
            Write(self._healthSlot, Max(0, current - amount), self._token)
        }
        
        func Heal(&mut self, amount: Int)
        {
            let current = Read(self._healthSlot, self._token)
            Write(self._healthSlot, Min(100, current + amount), self._token)
        }
    }
}
```

### 5. **include** (역할 조합)
- 기존 역할을 재사용하고 확장
- 다중 include 지원
- override로 재정의 가능

```pergyra
// 재사용 가능한 제네릭 역할
role BasicDamageable<T> for T where T has _healthSlot: Slot<Int>
{
    impl ability Damageable
    {
        func TakeDamage(&mut self, amount: Int)
        {
            let current = Read(self._healthSlot)
            Write(self._healthSlot, current - amount)
        }
    }
}

// 몬스터 객체 정의
class Monster
{
    _healthSlot: Slot<Int>
    monsterType: String
}

// 역할 조합
role MonsterCombat for Monster
{
    // 기본 역할 포함
    include role BasicDamageable<Monster>
    
    // 추가 능력 구현
    impl ability Attackable
    {
        func Attack(&self, target: &mut impl Damageable) -> Int
        {
            target.TakeDamage(10)
            return 10
        }
    }
    
    // 포함된 역할의 메서드 재정의
    override func TakeDamage(&mut self, amount: Int)
    {
        Log("Monster takes damage!")
        super.TakeDamage(amount)
    }
}
```

## Security Integration

### subject와 ability의 관계

Pergyra에서 `ability`는 모든 타입에 무차별적으로 붙는 일반 인터페이스가 아니다.
기본 방향은 다음과 같다.

- `struct`는 값 타입이다
- `subject`는 ability를 수행하는 주체 타입이다
- role은 subject에 ability를 바인딩한다
- ability의 `require`는 그 subject가 점유하거나 접근 가능한 자원 셀을 전제한다
- `object`는 이 subject가 transfer/view 문맥에서 수동적으로 해석된 모습일 뿐, 별도 코어 타입은 아니다

즉 `ability`는 "메서드 목록"보다
"이 객체가 어떤 자원 상태와 규율 위에서 어떤 행위를 수행할 수 있는가"에 더 가깝다.

### 보안 레벨과 역할 통합

```pergyra
// 보안 능력
secure ability SecureTransferable
{
    require _balanceSlot: SecureSlot<Decimal>
    require _token: Token
    
    func Transfer(&mut self, to: &mut impl SecureTransferable, amount: Decimal) -> Result<(), Error>
}

// 일반 전송 능력
ability Transferable
{
    require _balanceSlot: Slot<Decimal>
    
    func Transfer(&mut self, to: &mut impl Transferable, amount: Decimal) -> Result<(), Error>
}

// 타입 시스템이 보안 불일치를 방지
struct BankAccount
{
    _balanceSlot: SecureSlot<Decimal>
    _token: Token
}

// 컴파일 에러: SecureTransferable은 일반 Slot으로 구현 불가
// role InvalidRole for BankAccount
// {
//     impl ability Transferable  // Error: BankAccount has SecureSlot, not Slot
// }
```

## Parallel Composition

```pergyra
// 병렬 처리 가능한 역할 정의
role ParallelRenderer<T> for T where T: Renderable
{
    parallel on (gpuFiber)
    {
        let mesh = Read(self._meshSlot)
        let texture = Read(self._textureSlot)
        await GpuScheduler.SubmitDrawCall(mesh, texture)
    }
}

role ParallelPhysics<T> for T where T: Physics
{
    parallel on (cpuFiber)
    {
        let transform = Read(self._transformSlot)
        let rigidbody = Read(self._rigidbodySlot)
        let newTransform = await CpuScheduler.CalculatePhysics(transform, rigidbody)
        Write(self._transformSlot, newTransform)
    }
}

// 게임 유닛 subject에 병렬 역할 조합
role GameUnit for WorldUnit
{
    include role ParallelRenderer<WorldUnit>
    include role ParallelPhysics<WorldUnit>
}
```

## 요약

- `struct`는 복사 가능한 값이다
- `subject`는 ability를 수행하는 주체다
- `ability`는 객체 셀 위의 행위 계약이다
- `role`은 subject와 ability를 묶는 바인딩이다
- `include`는 role 재사용 수단이다

## Implementation Plan

### Phase 1: AST Extensions
1. `AST_ABILITY_DECL` 노드 타입 추가
2. `AST_ROLE_DECL` 노드 타입 추가
3. `AST_INCLUDE_STMT` 노드 타입 추가
4. `AST_REQUIRE_FIELD` 노드 타입 추가

### Phase 2: Parser Updates
1. `ability` 키워드 파싱
2. `role` 키워드 파싱
3. `include` 문 파싱
4. `require` 필드 파싱

### Phase 3: Type System Integration
1. Ability를 타입으로 취급
2. Role 바인딩 검증
3. Include 충돌 해결

### Phase 4: Runtime Support
1. 가상 테이블 생성
2. 동적 디스패치 지원
3. Role 기반 최적화
