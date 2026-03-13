# Compiler Modifications for Security-Aware Type System

## Overview
This document outlines the necessary compiler modifications to support the unified security-aware type system in Pergyra.

## 1. AST Modifications

### Add Security Level to Type Nodes
```c
// In ast.h, modify the type structure
struct {
    char* name;
    GenericParams* generic_args;
    SecurityModel security_level;  // NEW: Add security level
} type;
```

### Add Security Context to Function Declarations
```c
struct {
    // ... existing fields ...
    bool requires_secure_context;  // NEW: Security requirement
    SecurityModel minimum_security;  // NEW: Minimum security level
} func_decl;
```

## 2. Parser Modifications

### Parse Security Annotations
```pergyra
// Parse security level in type declarations
let slot = ClaimSlot<Int, ZeroCost>()
let secure = ClaimSlot<String, Hardware>()

// Parse security requirements in function signatures
func ProcessPayment<S: SecurityModel>(account: Slot<Account, S>)
    where S: Secure
```

### New Keywords
- Add `ZeroCost`, `Basic`, `Hardware`, `Encrypted` as type keywords
- Add `where S: Secure` constraint syntax

## 3. Semantic Analysis

### Type Checking with Security
```c
// In semantic analyzer
bool CheckSlotSecurity(ASTNode* slot_type, ASTNode* required_security) {
    SecurityModel slot_security = ExtractSecurityLevel(slot_type);
    SecurityModel required = ExtractSecurityLevel(required_security);
    
    // ZeroCost slots cannot be used where Secure is required
    if (required != SECURITY_MODEL_ZERO_COST && 
        slot_security == SECURITY_MODEL_ZERO_COST) {
        SemanticError("Cannot use insecure slot in secure context");
        return false;
    }
    
    return true;
}
```

### Effect System Integration
```c
// Check that functions using secure slots declare Security effect
bool ValidateSecurityEffects(ASTNode* func) {
    bool uses_secure_slots = CheckForSecureSlots(func->body);
    bool has_security_effect = HasEffect(func, "Security");
    
    if (uses_secure_slots && !has_security_effect) {
        SemanticError("Function using secure slots must declare 'with effects Security'");
        return false;
    }
    
    return true;
}
```

## 4. Code Generation

### Optimize for Security Level
```c
// In code generator
void GenerateSlotWrite(ASTNode* write_expr) {
    SecurityModel security = GetSlotSecurity(write_expr->slot);
    
    if (security == SECURITY_MODEL_ZERO_COST) {
        // Generate direct memory write
        EmitFastWrite(write_expr);
    } else {
        // Generate secure write with validation
        EmitSecureWrite(write_expr, security);
    }
}
```

### Remove Security Overhead in Performance Mode
```c
#ifdef PERGYRA_PERFORMANCE_MODE
    // Skip all security checks at compile time
    #define EMIT_SECURITY_CHECK(x) /* nothing */
#else
    #define EMIT_SECURITY_CHECK(x) EmitSecurityValidation(x)
#endif
```

## 5. Error Messages

### Clear Security Error Messages
```
Error: Cannot use insecure slot in secure context
  --> banking.pgy:45:10
   |
45 |     ProcessPayment(mySlot)  // mySlot is Slot<Account, ZeroCost>
   |                    ^^^^^^
   |
   = help: ProcessPayment requires Slot<Account, S> where S: Secure
   = help: Consider using ClaimSlot<Account, Basic>() instead

Error: Missing security effect declaration
  --> transfer.pgy:12:1
   |
12 | func TransferFunds(from: Slot<Money, Hardware>, to: Slot<Money, Hardware>)
   | ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   |
   = help: This function uses secure slots but doesn't declare 'with effects Security'
   = help: Add 'with effects Security' to the function signature
```

## 6. IDE Support

### Syntax Highlighting
- Highlight security levels differently (e.g., ZeroCost in gray, Hardware in red)
- Show security requirements in function tooltips

### Code Completion
- Suggest appropriate security levels based on context
- Warn when downgrading security

### Refactoring Tools
- "Upgrade to Secure Slot" refactoring
- "Add Security Effect" quick fix

## 7. Compilation Flags

### Security Mode Selection
```bash
# Compile for maximum security (default)
pergyra compile --security=production main.pgy

# Compile for maximum performance
pergyra compile --security=performance main.pgy

# Mixed mode with explicit boundaries
pergyra compile --security=mixed main.pgy
```

### Security Audit Mode
```bash
# Generate security audit report
pergyra audit --security-report main.pgy

# Output:
# Security Audit Report
# ====================
# Total slots: 145
# - Secure slots: 89 (61%)
# - Zero-cost slots: 56 (39%)
# 
# Security downgrades: 3
# - Line 234: User data downgraded for caching
# - Line 567: Temporary downgrade for performance
# - Line 891: Configuration downgraded after validation
#
# Potential issues: 1
# - Line 345: Mixing secure and insecure data in same structure
```

## Implementation Priority

1. **Phase 1**: AST and Parser modifications (Week 1-2)
2. **Phase 2**: Semantic analysis with security checking (Week 3-4)
3. **Phase 3**: Code generation optimizations (Week 5-6)
4. **Phase 4**: IDE support and tooling (Week 7-8)

## Testing Strategy

### Unit Tests
- Test security level inference
- Test security constraint validation
- Test error message generation

### Integration Tests
- Test mixed security mode applications
- Test security downgrade/upgrade
- Test performance optimizations

### Security Tests
- Attempt to bypass security through type casting
- Verify no security leaks in generated code
- Test audit trail generation
