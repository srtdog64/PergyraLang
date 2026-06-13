# Contract - Secure Slot Token Unforgeability (P0-1)

Last updated: 2026-06-13
Audit priority: P0 (must audit before beta freeze)

## Invariant Statement

> **For any `SecureSlotHandle` `h` bound to authority token `t_real`,
> there is no operation sequence over the runtime API that causes
> `PergyraSlotRead(h, t_fake)` or `PergyraSlotWrite(h, t_fake, _)` to
> succeed when `t_fake != t_real`, except when `t_fake` is obtained
> through a legitimate token transfer/share path that the runtime
> records.**

Equivalent informal phrasing:

> *Tokens cannot be forged. Reads and writes succeed only with the
> legitimate token. The set of legitimate tokens for a slot is exactly
> the set the runtime issued for that slot's current generation.*

## Scope Note

This contract covers two related but distinct implementation surfaces:

- Generated `PgySecureSlot_*` ABI: `src/runtime/pgy_abi_spec.h`,
  `src/runtime/pgy_runtime_slot_macros.h`, and
  `src/runtime/pgy_runtime_lib_secure_slot_exports.h` use
  `PgyToken_* { id, can_write, can_read }` and hard-fail or return typed
  `PgyRuntimeSlotStatus` on wrong-token access.
- `SlotManager` secure runtime API: `src/runtime/slot_security.h`,
  `src/runtime/slot_security.c`, `src/runtime/slot_security_crypto.c`,
  `src/runtime/slot_security_sealed_payload.c`, and
  `src/runtime/slot_manager_secure_ops.c` use `TokenCapability`,
  `SecureToken`, hardware fingerprinting, sealed payload policy, and crypto
  provider bindings. Windows routes random generation, SHA-256, HMAC-SHA256,
  and AES primitives through CNG/BCrypt; non-Windows routes them through
  OpenSSL EVP/HMAC/RAND. The runtime owns CTR counter composition and token
  authentication layout, not self-contained AES or SHA implementations.

`SECURITY_LEVEL_*` is not yet the generated language ABI's storage-policy
selector. Treat BASIC/HARDWARE/ENCRYPTED policy claims as `SlotManager`
runtime API claims until a named lowering owner wires source-level syntax to
that layer.

## Source Files Governed

- `src/runtime/slot_security.c`
- `src/runtime/slot_security_crypto.c`
- `src/runtime/slot_security_sealed_payload.c`
- `src/runtime/slot_security.h`
- `src/runtime/slot_manager.c` (interaction surface)
- `src/runtime/pgy_abi_spec.h`
- `src/runtime/pgy_runtime_slot_macros.h`
- `src/runtime/pgy_runtime_lib_secure_slot_exports.h`

## Existing Regression Coverage

`make test-security` covers the following enumerated cases (currently
182/182 passing):

- Stale-generation read/write/pin/release rejection
- Stale `SlotIsValid` returning false
- Zero slot id and slot id wrap tombstone before ABA reuse
- Tampered pinned-view generation unpin rejection
- Double-unpin rejection
- Release-while-pinned rejection
- TTL cleanup skip while pinned
- Invalid (zero) secure-token rejection on read/write/pin/release
- Invalid `SecurityLevel` rejection at context, token, and secure-claim entry
  points
- Capability metadata tamper and expired token rejection
- Token-refresh replay rejection for the old token
- Stored encrypted token tamper rejection before read/write/release/pin can
  treat a capability as live
- SHA-256 and AES-256-CTR/HMAC known-answer vectors, including authentication
  tag tamper rejection
- Sealed payload provider AES/HMAC coverage: generation-bound MAC rejection,
  primary provider auth-tag tamper recovery through a verified shadow copy,
  policy tamper rejection, and cross-slot sealed-payload transplant rejection
- Security portability smoke checks that the crypto owner uses standard
  provider APIs and does not reintroduce the removed self-contained AES/SHA
  implementation path
- Revoked-token rejection
- Raw secure-slot release rejection
- Concurrent secure-write rejection
- Release-after-unpin rejection

`runtime-panic-abi-test-smoke` covers exported entrypoints (C and
LLVM-linkable):

- Forged zero-token read/write/release rejection on inline C
  entrypoints
- Same on exported C/LLVM-linkable secure-slot entrypoints

These together are the **enumerated** test surface. The audit's job is
to find counterexamples *outside* this enumeration.

## Adversarial Input Shape

Use `../02_red_team_threat_model.md` as the red-team source of truth for
attacker tiers, kill-chain families, and claim limits. This contract lists the
P0-1 operation families that must be enumerated for token unforgeability.

Operations the AI Validator may sequence:

- `ClaimSecureSlot<T>()` returning `(handle, token)`
- `PergyraSlotRead(handle, token)` / `Write` / `Release` / `Pin` / `Unpin`
- `PergyraFreshClaim(handle)` until zero-id / max-id tombstone
- `PergyraSecureTokenRevoke(token)` (if API exists)
- `PergyraSecureTokenShare(token, target_zone)` (if API exists)
- `PergyraSlotTTLExpire(handle)` (test-only / time-skip)
- Concurrent operations across multiple zones / parallel tasks

Adversarial techniques to enumerate:

1. **Bit manipulation** - flip individual bits in a captured token,
   verify each flipped variant is rejected.
2. **Replay** - capture token at generation N, bump generation, replay
   token at generation N+1. Must reject.
3. **Cross-slot replay** - token from slot A used on slot B. Must reject.
4. **Id-space exhaustion** - force the next slot id to the zero sentinel
   or max-id tombstone boundary. Verify claim rejects before stale handles
   can alias a reused id.
5. **Concurrent revoke vs use** - token revoked on zone X while zone
   Y mid-operation. Verify atomicity.
6. **TTL race** - token used in window between expiration check and
   data return.
7. **Token compare timing** - does comparison short-circuit in a way
   that leaks token bits via timing? (side-channel - out of P0 scope
   but flag for P3-3.)
8. **Aliased zero-token** - zero token rejected, but is it *only* zero
   that's rejected, or any "default-looking" token?
9. **Token confusion across slot types** - `SecureSlot<Int>` token
   used on `SecureSlot<String>`. Type system should prevent at compile
   time, but runtime must reject if hand-crafted.
10. **Backend-divergent** - does C backend and LLVM backend handle
    forgery identically? (Use backend-compare as oracle.)
11. **Stored authority tamper** - mutate `entry->writeToken` or
    `entry->tokenGeneration` while presenting an otherwise valid capability.
    Must reject before read/write/release/pin treats the capability as live.
12. **Capability metadata tamper** - mutate permissions, expiry, level, or
    transfer fields without reissuing the token. Must reject.
13. **Provider drift** - remove the required platform crypto provider or
    reintroduce custom AES/SHA/RNG code. Must fail preflight or smoke.
14. **Sealed payload transplant** - copy ciphertext, nonce, auth tag, and MAC
    from slot A to slot B. Must reject because the HMAC binds slot id,
    generation, policy, shadow flag, provider auth tag, and payload bytes.
15. **Sealed payload policy tamper** - flip the stored sealed payload policy
    after a valid write. Must reject before returning ciphertext as plaintext
    or accepting a mismatched shadow policy.

## Known Limits - Not Covered By This Audit

- Side-channel attacks (timing, cache, speculation) - P3-3.
- Hardware-induced bit flips (Rowhammer-class) - out of language scope.
- Concurrent interleavings deeper than the harness models - P3-2 with
  Loom-style tooling.
- Attacks via `unsafe` Pergyra code - when/if `unsafe` is added, that's
  a separate audit target.

## Pass Criterion

The audit passes when the AI Validator reports:

- Each adversarial technique above produces only rejected operations
  (no successful read/write with non-legitimate token).
- AI explicitly enumerates and reports exhaustion of each technique
  family.
- Backend-compare confirms C and LLVM behave identically on each
  rejected case.

The audit does **not** pass merely because no counterexample was
found. Exhaustion of named families must be explicit.

## On Failure

Each confirmed counterexample becomes a finding in `findings/` with:

- Reproducer Pergyra source or C harness program.
- Expected behavior (rejection per invariant).
- Actual behavior (acceptance / wrong data / wrong panic class).
- Severity classification (silent-wrong-data > wrong-panic-class >
  loud-but-recoverable).
- Regression test added to `tests/cases/security/` referenced by
  finding ID.
- Fix commit referenced.

## Audit History

(append-only)

| Date | Tool | Audit log | Findings |
|---|---|---|---|
| (none yet) | | | |
