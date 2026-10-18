# V10 capture allocation bounds: post-review-gap receipt

Observed 2026-10-18 KST on uncommitted compiler source atop the docs-only
checkpoint `4a5372b0c8dfbf30ed6891b03c4bf6c7fb37a613`. The compiler work derives
from `8404061d78f2d2bf9f2c325be1a67f83ab9904bf` and was validated in
`/root/pgy-nonowner-20261018.5t1W96`.

This record follows the earlier
[V10 receipt](2026-10-18_v10_capture_allocation_bounds.md). That candidate
proved count/capacity rejection but an independent read-only review found four
unexecuted lifecycle cases. This supplement identifies the repaired candidate;
the earlier record remains unchanged as history.

## Closed review gaps

1. Semantic capture boundary or row storage may be absent while its capacity is
   positive. Both shapes now execute as fail-closed imports before destination
   mutation, followed by a valid exact import.
2. An empty capture inventory now has an explicit imported state. A duplicate
   import refuses; clear resets the state; a later import succeeds.
3. MIR import is transactional after allocation starts. Any later failure
   clears boundary and row storage and returns the destination to the empty,
   restartable state instead of relying on its caller to destroy a partial MIR.
4. The semantic test checks that unused boundary and row capacity is zeroed.
   A focused sanitizer unit populates active and tail capacity, clears it by
   recorded capacities and runs with leak detection enabled.

The focused allocator unit compiles the production owner with a deterministic
replacement for its `calloc` calls. The first allocation succeeds and the row
allocation fails, so the actual mid-import cleanup branch is executed. This is
test-only linkage; production retains its single system allocator path.

## Candidate identity

| Artifact | SHA-256 |
|---|---|
| `bin/pgy` | `6b5b00baf5f21a28b351ce51273e1cea1f1be1fa9c56e34064465cb251c356d0` |
| `bin/test_air` | `d014758ff2cb88ce016bfba0cfe71db14fc6a430fe7eb1c54642adfa765fc2b1` |
| `bin/test_mir` | `2f1d170f4383afb2cbc317085ebb139049420d720ae6beeb166df8863263b720` |
| ASan+UBSan `test_air` | `a267df6ee35df6e8f873d793004e474f1da70c7f9ffde27002014df0e6ff9d33` |
| ASan+UBSan `test_mir` | `032354d12bae7de2f471ddc9fa71383f83ea6bda10ad938e77b02afaa80cb101` |

Hashes are recorded in
`/root/pgy-nonowner-20261018.5t1W96/.tmp/v10-post-gap-binaries.sha256`.
The candidate is not committed, published or an immutable review export.

## Observed gates

| Gate | Result | Receipt |
|---|---|---|
| focused allocation lifecycle, ASan+UBSan, leaks ON | PASS | `.tmp/v10-capture-allocation-lifecycle.log` |
| phase unit | 13,422 checks, 0 failures | `.tmp/v10-post-gap-phase.log` |
| release MIR | 683 passed, 0 failed | `.tmp/v10-post-gap-test-mir.log` |
| release AIR | 161 passed, 0 failed | `.tmp/v10-post-gap-test-air.log` |
| sanitized MIR, leaks OFF | 683 passed, 0 failed | `.tmp/v10-post-gap-asan-test-mir.log` |
| sanitized AIR, leaks OFF | 161 passed, 0 failed | `.tmp/v10-post-gap-asan-test-air.log` |

The requested four post-certificate mutations were re-executed in release and
sanitized AIR:

| Mutation | Binding | C | LLVM | Parallel | Outputs unchanged | Restore |
|---|---:|---:|---:|---:|---:|---:|
| instruction source identity | 0 | 0 | 0 | 0 | 1 | 1 |
| observability flag | 0 | 0 | 0 | 0 | 1 | 1 |
| boundary count above capacity | 0 | 0 | 0 | 0 | 1 | 1 |
| row count above capacity | 0 | 0 | 0 | 0 | 1 | 1 |

For the first two rows capture shape remains valid; for the size mismatches it
is invalid before traversal. Certificate readiness remains true in all four,
so the refusal is specifically the current MIR binding/storage check rather
than certificate withdrawal.

## Remaining boundary

- Semantic, transpile, RIR, DIR, 40-source sanitizer corpus, focused ReadFile
  runtime/parity and broad backend observations belong to the earlier V10
  receipt; they were not rerun after this transactional cleanup change.
- Fresh portable replay and independent review of this post-gap candidate are
  still open.
- Unchanged-canonical admission, bootstrap/gen2==gen3, installed C/LLVM driver,
  required published-SHA CI and whole-language memory safety remain
  unestablished.
