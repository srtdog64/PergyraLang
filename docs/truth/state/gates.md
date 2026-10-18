# Gates

Owner: GPT root lane. Observed 2026-10-18 KST on an uncommitted V10 candidate
based on `8404061d78f2d2bf9f2c325be1a67f83ab9904bf`.

## Finish line (OPEN)

| Gate | Last result |
|---|---|
| Full unchanged fixed/current canonical | RED:18435 errors/16 warnings/MIR0 (YSfo5x) |
| Current bootstrap and gen2 == gen3 | not established on current main |
| Installed current C/LLVM driver | not established on current main |
| Required CI, all required jobs executed at one published SHA | see [main](main.md) |

The earlier `b8e66a387` driver/fixed point is branch-specific
[history](../evidence/2026-10-16_b8e66a387_fixed_point.md), not current-main proof.

## Scoped regression evidence, not closure

Build copy: `/root/pgy-nonowner-20261018.5t1W96`.
Full receipt: [V10 capture allocation bounds](../evidence/2026-10-18_v10_capture_allocation_bounds.md).

| Receipt | Observed result |
|---|---|
| Candidate binaries | pgy `0779ca03...afb2c`; AIR `6a8560de...7a5a`; MIR `2d57e8b...7a5a` |
| Unit families | phase13422/0, MIR682/0, AIR161/0, semantic3230/0, transpile1014/0, RIR44/0, DIR15/0 |
| Four representative post-certificate mutations | instruction identity, observability flag, boundary count and row count all refuse and restore |
| Focused runtime/parity | ReadFile C/LLVM, capture projection C/LLVM, binding and four IR-flow gates PASS |
| Sanitizers | AIR, MIR and 40-source compile corpus PASS under ASan+UBSan; leaks OFF |

## Broader current-branch gates

| Gate | Last result |
|---|---|
| Default strict backend compare | RED: ABI precheck 186 passed/4 failed across two LLVM fixtures |
| Explicit all discovered backend cases | RED: 895/975 passed; default registry also omits three fixture directories |
| Production-header gate | size/header portions PASS; self-host parity manifest RED on opaque String boundary |
| Fresh portable V10 replay and independent immutable review | not run |

The previous V9 size-mismatch sanitizer failures are repaired in this
candidate. The broader failures above are not attributed wholesale to V10.
