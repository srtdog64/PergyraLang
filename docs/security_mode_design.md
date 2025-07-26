# Pergyra Security Mode Design: Unified Type-Safe Approach

## Overview

This document outlines the design for supporting both secure and zero-cost modes in Pergyra while maintaining type safety and preventing security vulnerabilities at compile time.

## Core Design: Security as a Type Parameter

### 1. Security Level Types

```pergyra
// Base trait for all security models
trait SecurityModel {
    const IS_SECURE: Bool
    const NEEDS_TOKEN: Bool
}

// Concrete security levels
struct Insecure : SecurityModel {
    const IS_SECURE = false
    const NEEDS_TOKEN = false
}

struct SecureBasic : SecurityModel {
    const IS_SECURE = true
    const NEEDS_TOKEN = true
}

struct SecureHardware : SecurityModel {
    const IS_SECURE = true
    const NEEDS_TOKEN = true
}

struct SecureEncrypted : SecurityModel {
    const IS_SECURE = true
    const NEEDS_TOKEN = true
}

// Type alias for convenience
type ZeroCost = Insecure
type Basic = SecureBasic
type Hardware = SecureHardware
type Encrypted = SecureEncrypted
```

### 2. Unified Slot Type

```pergyra
// Single slot type with security as type parameter
type Slot<T, S: SecurityModel = ZeroCost> = {
    id: SlotId,
    _phantom: PhantomData<(T, S)>
}

// Token is only required for secure slots
type SecurityToken<S: SecurityModel> = 
    if S.NEEDS_TOKEN then Token else Unit
```

### 3. Unified API with Compile-Time Security

```pergyra
// Single claim function with security level inference
func ClaimSlot<T, S: SecurityModel = ZeroCost>() -> Slot<T, S> {
    match S {
        ZeroCost => runtime.claim_fast<T>(),
        Basic => runtime.claim_secure<T>(SECURITY_LEVEL_BASIC),
        Hardware => runtime.claim_secure<T>(SECURITY_LEVEL_HARDWARE),
        Encrypted => runtime.claim_secure<T>(SECURITY_LEVEL_ENCRYPTED)
    }
}

// Write function that adapts based on security level
func Write<T, S: SecurityModel>(
    slot: Slot<T, S>, 
    value: T,
    token: SecurityToken<S> = default
) {
    match S {
        ZeroCost => runtime.write_fast(slot.id, value),
        _ => runtime.write_secure(slot.id, value, token)
    }
}

// Read function with same pattern
func Read<T, S: SecurityModel>(
    slot: Slot<T, S>,
    token: SecurityToken<S> = default
) -> T {
    match S {
        ZeroCost => runtime.read_fast(slot.id),
        _ => runtime.read_secure(slot.id, token)
    }
}
```

## Preventing Security Contamination

### 1. Explicit Conversion with Security Downgrade

```pergyra
// Conversion must be explicit and logged
func DowngradeSlot<T>(
    secure: Slot<T, impl Secure>,
    token: SecurityToken<_>
) -> Slot<T, ZeroCost> 
    with effects Security, Audit
{
    // Log security downgrade for audit
    Audit.Log("Security downgrade", secure.id)
    
    // Create new insecure slot with copied data
    let insecure = ClaimSlot<T, ZeroCost>()
    Write(insecure, Read(secure, token))
    
    // Release secure slot
    Release(secure, token)
    
    return insecure
}

// Upgrade is always safe
func UpgradeSlot<T, S: Secure>(
    insecure: Slot<T, ZeroCost>
) -> (Slot<T, S>, SecurityToken<S>) {
    let (secure, token) = ClaimSecureSlot<T, S>()
    Write(secure, Read(insecure), token)
    Release(insecure)
    return (secure, token)
}
```

### 2. Function Security Requirements

```pergyra
// Functions can specify minimum security requirements
func ProcessPayment<S: SecurityModel>(
    account: Slot<BankAccount, S>
) -> Result<Receipt, Error>
    where S: Secure  // Compile-time requirement!
{
    // This function only accepts secure slots
}

// Functions can be generic over security
func CacheData<T, S: SecurityModel>(
    data: Slot<T, S>,
    token: SecurityToken<S> = default
) {
    // Works with both secure and insecure slots
}
```

## Effect System Integration

### 1. Security Effects

```pergyra
// Define security effects
effect Security {
    func RequireToken<S: Secure>() -> SecurityToken<S>
    func ValidateAccess<T, S>(slot: Slot<T, S>, token: SecurityToken<S>)
}

// Functions using secure slots must declare security effects
func TransferFunds(
    from: Slot<Account, Hardware>,
    to: Slot<Account, Hardware>,
    amount: Decimal
) -> Result<TransferReceipt, Error>
    with effects Security, IO, Audit
{
    let token = Security.RequireToken<Hardware>()
    
    let fromBalance = Read(from, token)
    if fromBalance < amount {
        return .Err(InsufficientFunds)
    }
    
    Write(from, fromBalance - amount, token)
    let toBalance = Read(to, token)
    Write(to, toBalance + amount, token)
    
    return .Ok(TransferReceipt { ... })
}
```

### 2. Compile-Time Security Context

```pergyra
// Security contexts prevent accidental mixing
@[security_context(Secure)]
module BankingCore {
    // All slots in this module default to Hardware security
    type DefaultSecurity = Hardware
    
    // Attempting to use ZeroCost slots here causes compile error
}

@[security_context(Performance)]
module GameEngine {
    // All slots default to ZeroCost
    type DefaultSecurity = ZeroCost
    
    // Can still explicitly use secure slots when needed
}
```

## Migration Strategy

### 1. Backwards Compatibility

```pergyra
// Old API can be implemented as aliases
func ClaimSecureSlot<T>() -> (Slot<T, Basic>, SecurityToken<Basic>) {
    let slot = ClaimSlot<T, Basic>()
    let token = Security.RequireToken<Basic>()
    return (slot, token)
}

// Gradual migration path
#[deprecated("Use ClaimSlot<T, ZeroCost>() instead")]
func claim_slot<T>() -> Slot<T, ZeroCost> {
    ClaimSlot<T, ZeroCost>()
}
```

### 2. Compiler Warnings and Migrations

```pergyra
// Compiler can suggest security upgrades
#[suggest_security_upgrade]
func ProcessUserData(data: Slot<UserInfo, ZeroCost>) {
    // Compiler warning: "Consider using Slot<UserInfo, Basic> for user data"
}
```

## Performance Optimizations

### 1. Zero-Cost Abstraction

```pergyra
// Security checks are eliminated at compile time for ZeroCost
@[inline(always)]
func Write<T>(slot: Slot<T, ZeroCost>, value: T) {
    // Direct memory write - no security overhead
    unsafe { *slot.get_ptr() = value }
}

// Security version has full checks
@[inline(never)]  // Prevent inlining for security
func Write<T, S: Secure>(slot: Slot<T, S>, value: T, token: SecurityToken<S>) {
    validate_token(token)
    check_access_rights(slot, token)
    audit_write(slot, token)
    encrypt_if_needed(S, value)
    unsafe { *slot.get_ptr() = value }
}
```

### 2. Compile-Time Mode Selection

```pergyra
// Global compilation mode
#[cfg(security = "production")]
type DefaultSlotSecurity = Hardware

#[cfg(security = "performance")]
type DefaultSlotSecurity = ZeroCost

// Use default in generic code
func GenericAlgorithm<T>() {
    let slot = ClaimSlot<T, DefaultSlotSecurity>()
    // ...
}
```

## Example: Mixed-Mode Application

```pergyra
// High-security module for financial transactions
module FinancialCore {
    type AccountSlot = Slot<Account, Hardware>
    
    actor Bank {
        private _accounts: Map<String, AccountSlot>
        
        public func Transfer(
            from: String, 
            to: String, 
            amount: Decimal
        ) -> Result<(), Error>
            with effects Security, Audit
        {
            // All operations require hardware security tokens
        }
    }
}

// High-performance module for game physics
module PhysicsEngine {
    type VectorSlot = Slot<Vector3, ZeroCost>
    
    func SimulatePhysics(
        positions: Array<VectorSlot>,
        velocities: Array<VectorSlot>,
        deltaTime: Float
    ) {
        // Zero security overhead for maximum performance
        Parallel {
            for i in 0..positions.Length {
                let pos = Read(positions[i])
                let vel = Read(velocities[i])
                Write(positions[i], pos + vel * deltaTime)
            }
        }
    }
}

// Bridge module that safely converts between modes
module GameEconomy {
    // Convert game achievements to real money (requires security upgrade)
    func RedeemAchievements(
        achievements: Slot<Achievements, ZeroCost>
    ) -> Result<Money, Error>
        with effects Security, Audit
    {
        // Explicitly upgrade to secure mode
        let (secureAchievements, token) = UpgradeSlot<Achievements, Hardware>(achievements)
        
        // Now process with full security
        let value = CalculateValue(Read(secureAchievements, token))
        
        // ... process payment ...
    }
}
```

## Benefits of This Approach

1. **Single Language**: One Pergyra, not two dialects
2. **Type Safety**: Security requirements enforced at compile time
3. **Zero Cost**: No runtime overhead for performance mode
4. **Explicit Boundaries**: Clear, auditable security transitions
5. **Gradual Migration**: Existing code can be migrated incrementally
6. **Flexibility**: Mix security levels within same application

## Implementation Phases

### Phase 1: Type System Enhancement
- Add security type parameters to Slot
- Implement compile-time security checking

### Phase 2: Runtime Optimization
- Separate fast paths for ZeroCost mode
- Remove security overhead from performance path

### Phase 3: Effect System Integration
- Add Security effect
- Implement compile-time effect checking

### Phase 4: Tooling Support
- IDE warnings for security downgrades
- Migration tools for existing code
- Security audit reports

## Conclusion

This design achieves the goal of supporting both maximum security and zero-cost performance modes while maintaining Pergyra's core value of safety. The type system ensures that security properties are preserved at compile time, preventing the mixing of secure and insecure data without explicit, auditable conversions.
