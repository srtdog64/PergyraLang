# Pergyra Role/Interface System Design

## Overview
Pergyra의 혁신적인 역할 기반 조합 시스템으로, 기존의 trait 상속을 대체하는 더 단순하고 안전한 패러다임입니다.

## Core Concepts

### 1. **struct** (데이터 컨테이너)
- 순수하게 데이터만 정의
- 메서드 없음
- 객체의 '정체성' 표현

```pergyra
struct Player
{
    _healthSlot: SecureSlot<Int>
    _token: Token
    name: String
}
```

### 2. **ability** (행위의 명세)
- 객체가 무엇을 할 수 있는지 정의
- `require` 키워드로 필요한 데이터 필드 명시
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

### 3. **role** (행위의 구체적 구현)
- 특정 struct가 특정 ability를 어떻게 수행하는지 구현
- 데이터와 행위를 연결하는 '접착제'

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

### 4. **include** (역할 조합)
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

// 몬스터 정의
struct Monster
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

// 게임 오브젝트에 병렬 역할 조합
role GameObject for Entity
{
    include role ParallelRenderer<Entity>
    include role ParallelPhysics<Entity>
}
```

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
