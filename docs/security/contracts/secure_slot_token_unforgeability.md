# Contract — Secure Slot Token Unforgeability (P0-1)

Last updated: 2026-04-26
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

## Source Files Governed

- `src/runtime/slot_security.c`
- `src/runtime/slot_security.h`
- `src/runtime/security_types.c`
- `src/runtime/security_types.h`
- `src/runtime/slot_manager.c` (interaction surface)

## Existing Regression Coverage

`make test-security` covers the following enumerated cases (currently
132/132 passing):

- Stale-generation read/write/pin/release rejection
- Stale `SlotIsValid` returning false
- Release-while-pinned rejection
- TTL cleanup skip while pinned
- Invalid (zero) secure-token rejection on read/write/pin/release
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

Operations the AI Validator may sequence:

- `ClaimSecureSlot<T>()` returning `(handle, token)`
- `PergyraSlotRead(handle, token)` / `Write` / `Release` / `Pin` / `Unpin`
- `PergyraGenerationBump(handle)` (test-only or implicit via Release/Reclaim)
- `PergyraSecureTokenRevoke(token)` (if API exists)
- `PergyraSecureTokenShare(token, target_zone)` (if API exists)
- `PergyraSlotTTLExpire(handle)` (test-only / time-skip)
- Concurrent operations across multiple zones / parallel tasks

Adversarial techniques to enumerate:

1. **Bit manipulation** — flip individual bits in a captured token,
   verify each flipped variant is rejected.
2. **Replay** — capture token at generation N, bump generation, replay
   token at generation N+1. Must reject.
3. **Cross-slot replay** — token from slot A used on slot B. Must reject.
4. **Generation overflow** — bump generation many times until counter
   wraps. Verify rejection still works.
5. **Concurrent revoke vs use** — token revoked on zone X while zone
   Y mid-operation. Verify atomicity.
6. **TTL race** — token used in window between expiration check and
   data return.
7. **Token compare timing** — does comparison short-circuit in a way
   that leaks token bits via timing? (side-channel — out of P0 scope
   but flag for P3-3.)
8. **Aliased zero-token** — zero token rejected, but is it *only* zero
   that's rejected, or any "default-looking" token?
9. **Token confusion across slot types** — `SecureSlot<Int>` token
   used on `SecureSlot<String>`. Type system should prevent at compile
   time, but runtime must reject if hand-crafted.
10. **Backend-divergent** — does C backend and LLVM backend handle
    forgery identically? (Use backend-compare as oracle.)

## Known Limits — Not Covered By This Audit

- Side-channel attacks (timing, cache, speculation) — P3-3.
- Hardware-induced bit flips (Rowhammer-class) — out of language scope.
- Concurrent interleavings deeper than the harness models — P3-2 with
  Loom-style tooling.
- Attacks via `unsafe` Pergyra code — when/if `unsafe` is added, that's
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
