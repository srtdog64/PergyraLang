# SecureSlot v2

## Goal

`SecureSlot` is no longer treated as "a normal slot with extra XOR/checksum code inside the manager".
The runtime is split into two layers:

- `Slot`: plain storage, fast path, no sealed-memory policy
- `SecureSlot`: token validation + sealed storage + optional shadow recovery

## Design

### Slot Layer

`src/runtime/slot_manager.c`

- Owns slot lifecycle, handles, TTL, scope, and concurrency checks
- Stores plain payloads for normal slots in `dataBlockRef`
- Does not implement masking, MAC generation, or shadow-copy verification itself

### Secure Storage Layer

`src/runtime/slot_security.c`

- Defines `SecureSlotPolicy`
- Defines `SecureSealedPayload`
- Seals payloads with:
  - per-payload nonce
  - keyed in-memory obfuscation
  - keyed MAC over sealed bytes and metadata
  - optional shadow copy
- Opens payloads with:
  - primary MAC verification
  - shadow verification fallback
  - automatic primary restore from shadow on successful recovery

## Policy

`SecurityPolicyForLevel()` currently maps levels as follows:

- `SECURITY_LEVEL_BASIC`: sealed storage, obfuscation enabled, no shadow copy
- `SECURITY_LEVEL_HARDWARE`: sealed storage, obfuscation enabled, shadow copy enabled
- `SECURITY_LEVEL_ENCRYPTED`: sealed storage, obfuscation enabled, shadow copy enabled

## Why This Is Better

- `slot_manager` no longer owns security-specific storage details
- normal slot performance is isolated from secure slot policy
- secure storage can evolve without rewriting the core slot allocator
- integrity checking moved from fixed checksum logic to keyed MAC validation

## Current Limits

- shadow storage is still in-process memory, not a separate process or trusted boundary
- the current MAC/obfuscation scheme is runtime-local and not a substitute for server authority
- `SECURITY_LEVEL_HARDWARE` and `SECURITY_LEVEL_ENCRYPTED` currently differ more in policy intent than in storage backend

## Next Candidates

- separate secure arena for shadow pages
- keyed BLAKE3 or HMAC backend instead of the current SHA-256 material build
- background verifier for long-lived secure slots
- editor/runtime visibility controls so secure payload internals are not exposed through public slot metadata
