# Red-Team Threat Model

Last updated: 2026-06-14

This document deepens the security audit workflow from the attacker's side.
It is not a claim that Pergyra is exploit-proof. It defines the attacker
families the beta security story must survive, the trust boundaries each
family tries to break, and the evidence required before claims are allowed in
beta-facing docs.

## Source Of Truth

Current beta-facing security has two separate surfaces:

- Generated `SecureSlot<T>` ABI: `PgySecureSlot_*` plus `PgyToken_* {
  id, can_write, can_read }`. This is the stable language/backend surface.
- `SlotManager` secure runtime API: `TokenCapability`, `SecureToken`,
  `SecurityLevel`, `HardwareFingerprint`, encrypted stored tokens, and sealed
  payload policy. This is runtime-manager evidence, not yet the generated
  language storage-policy selector.

Red-team audits must keep those surfaces separate. A break in one surface is
not automatically a break in the other, and a pass in one surface is not
automatically evidence for the other.

## Attacker Tiers

| Tier | Attacker | Capabilities | Must Not Be Trusted |
|---|---|---|---|
| A0 | Source-level caller | Can sequence public Pergyra APIs in any valid order | Caller discipline, intended API order |
| A1 | ABI-level caller | Can call exported C runtime entrypoints with hand-built structs | Struct field honesty, enum validity, zero/default values |
| A2 | Same-process memory mutator | Can flip writable runtime memory after capture | Stored token bytes, capability fields, sealed payload bytes |
| A3 | Scheduler attacker | Can race refresh/revoke/read/write/pin/unpin across threads | Check-then-use windows, lock ordering assumptions |
| A4 | Toolchain/platform attacker | Can build on minimal or unusual supported toolchains | Optional crypto providers, missing preflights, unsupported platform fallback |
| A5 | Host drift attacker | Can change or virtualize host identity over time | Stable hardware fingerprint availability |

The beta runtime should primarily defend A0 through A3 for current in-process
contracts. A4 is a build policy contract. A5 is an availability and binding
contract, not a promise that host hardware identity is globally unspoofable.

## Kill-Chain Families

### 1. Capture Then Mutate Capability

Goal: turn a legitimate capability into a broader or different authority.

Attack steps:

1. Claim a secure slot and capture `(handle, TokenCapability)`.
2. Mutate one field at a time: `slotId`, `level`, `expiryTime`,
   `canRead`, `canWrite`, `canTransfer`, `token.generation`, `token.checksum`,
   and each byte of `token.tokenData`.
3. Attempt read, write, release, pin, refresh, and revoke where applicable.

Required defense:

- `TokenValidate` must reject metadata tamper before any data operation.
- Capability integrity must be keyed by `SecurityContext.masterKey`.
- Crypto failure must fail closed. There must be no weak checksum fallback.

Current evidence:

- `make test-security` covers capability metadata tamper and expired token
  rejection.
- `security-portability-contract-test-smoke` gates provider HMAC use in the
  `TokenCapability` integrity path.

### 2. Capture Then Mutate Stored Runtime State

Goal: keep a valid presented capability but corrupt the slot's stored runtime
authority record.

Attack steps:

1. Claim a secure slot and capture a valid `TokenCapability`.
2. Mutate the slot's encrypted stored token (`entry->writeToken`), stored
   token generation, security level, or sealed payload.
3. Attempt read, write, release, pin, and refresh with the still-valid
   presented capability.

Required defense:

- `SlotValidateToken` must decrypt the stored token and compare it with the
  presented token using `TokenCompareSecure`.
- Generation equality alone is not authority.
- Stored token decrypt/auth failure must be treated as token validation
  failure.

Current evidence:

- `make test-security` covers stored encrypted token tamper rejection.
- `security-portability-contract-test-smoke` gates `TokenDecrypt` plus
  `TokenCompareSecure` in `SlotValidateToken`.

### 3. Replay Across Time

Goal: reuse a token that was valid before a generation, refresh, revoke, or TTL
transition.

Attack steps:

1. Capture a token at generation N.
2. Refresh or revoke the token, or release and recycle the slot.
3. Replay the old token against read/write/release/pin.
4. Repeat with operations racing across threads.

Required defense:

- Stored token generation and decrypted stored token must both match.
- Revocation must wipe stored authority before later validation can succeed.
- TTL expiration must be checked before data access.
- Race tests must cover the check-then-use boundary.

Current evidence:

- `make test-security` covers refresh replay, revoke rejection, stale handle
  rejection, and TTL cleanup while pinned.
- Deeper revoke/read/write interleavings remain an audit target.

### 4. Cross-Boundary Confusion

Goal: move a token across slot IDs, slot types, generated ABI surfaces, or
runtime-manager API surfaces.

Attack steps:

1. Use a token issued for slot A on slot B.
2. Use a `SecureSlot<Int>`-shaped token on a different payload type.
3. Mix generated `PgyToken_*` assumptions with `TokenCapability` assumptions.
4. Compare C backend and LLVM backend behavior on each rejected case.

Required defense:

- Token integrity must bind slot ID, level, generation, permissions, expiry,
  and hardware fingerprint summary.
- Runtime must reject forged ABI-level structs even when the source type
  checker would normally prevent them.
- Docs must not treat generated `SecureSlot<T>` storage policy as already
  wired to `SECURITY_LEVEL_*`.

Current evidence:

- `secure_slot_token_unforgeability.md` keeps generated ABI and `SlotManager`
  runtime API scopes separate.
- Backend-divergent rejection remains an audit target.

### 5. Toolchain And Provider Drift

Goal: force the build onto a weaker crypto path or silently skip the security
suite.

Attack steps:

1. Build on Windows without OpenSSL and on non-Windows without OpenSSL.
2. Build on Windows with CNG/BCrypt unavailable or not linked.
3. Search for reintroduced custom AES/SHA/RNG paths.
4. Run `make test-security` and verify the suite executes, not merely
   compiles.

Required defense:

- Windows uses CNG/BCrypt; non-Windows uses OpenSSL EVP/HMAC/RAND.
- Missing providers must fail in `check-security-toolchain`.
- No self-contained AES/SHA/RNG path may reappear without a new audit target.

Current evidence:

- `check-security-toolchain` compiles provider preflight fixtures.
- `security-portability-contract-test-smoke` forbids removed custom crypto
  identifiers.

### 6. Host Identity Drift

Goal: break hardware-bound tokens through VM migration, host rename, MAC
change, board ID loss, or unsupported platform fallback.

Attack steps:

1. Generate HARDWARE token on host identity H1.
2. Change individual identity inputs or run under a platform without a
   fingerprint provider.
3. Validate the token again.

Required defense:

- Unsupported platforms fail closed for hardware fingerprint generation.
- HARDWARE validation compares current fingerprint with context fingerprint.
- Docs describe hardware binding as runtime-manager evidence, not an
  unspoofable machine identity guarantee.

Current evidence:

- `slot_security_fingerprint.c` returns `SECURITY_ERROR_UNSUPPORTED_PLATFORM`
  outside currently supported fingerprint providers.
- Full VM migration and host-drift tests remain audit work.

## Defense Matrix

| Boundary | Required Owner | Red-Team Question | Current Evidence |
|---|---|---|---|
| Presented capability integrity | `TokenValidate` | Can field mutation keep authority? | HMAC-backed checksum, tamper tests |
| Stored authority integrity | `SlotValidateToken` | Can stored token mutation be ignored? | decrypt plus constant-time compare |
| Payload integrity | `SecureSealedPayloadOpen` | Can payload bytes or policy be changed silently or transplanted? | provider AES plus HMAC-bound primary/shadow tests |
| Replay and revocation | `SlotRefreshToken`, `SlotRevokeToken` | Can old token still act? | refresh/revoke tests |
| Provider policy | `check-security-toolchain` | Can build fall back to custom crypto? | provider preflight and smoke |
| Scope split | docs/security contracts | Are claims scoped to the real surface? | source-of-truth split gates |

## Required New Audit Families

These are not all implementation TODOs. They are attack families that should
become either regression tests, backend-compare cases, or explicit out-of-scope
findings.

1. Concurrent revoke/read/write/pin race with deterministic scheduler control.
2. Cross-slot and cross-type hand-built ABI tokens on both C and LLVM paths.
3. VM/host identity drift matrix for HARDWARE tokens.
4. Sealed payload nonce reuse stress under repeated writes.
5. SecurityContext lifecycle misuse: destroy, reinitialize, and stale pointer
   attempts through public APIs.
6. Toolchain matrix: Windows CNG, Linux OpenSSL, macOS/OpenSSL or explicit
   unsupported status.
7. Timing budget smoke for token compare path that detects obvious
   short-circuit reintroduction.

## Claim Rules

- Do not claim "memory safe" from SecureSlot token discipline. It is a runtime
  authority boundary, not a memory safety proof.
- Do not claim hardware binding is unspoofable. Claim only that supported
  platforms compare the current runtime fingerprint against the context
  fingerprint.
- Do not claim generated `SecureSlot<T>` uses BASIC/HARDWARE/ENCRYPTED storage
  policy until a named lowering owner connects `SECURITY_LEVEL_*` syntax to
  the runtime-manager policy.
- Do not claim a green audit proves security. It only reports that named
  attack families were exhausted without a counterexample.

## Beta Exit Bar

Before beta freeze, the red-team bar for P0-1 is:

- `make test-security` passes and includes presented capability tamper, stored
  authority tamper, refresh replay, revoke rejection, stale handle rejection,
  provider KATs, sealed payload corruption, policy tamper, auth-tag tamper,
  generation mismatch, and cross-slot transplant rejection.
- `security-portability-contract-test-smoke` gates provider crypto, no custom
  AES/SHA/RNG reintroduction, stored-token compare, and scoped docs wording.
- At least one audit log under `docs/security/audits/` records the exhausted
  P0-1 families and links any confirmed findings.
