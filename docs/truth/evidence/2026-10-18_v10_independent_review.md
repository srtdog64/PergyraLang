# V10 independent frozen review

Observed 2026-10-18 KST. The `Pergyra 컴파일러 레드팀 검증` chat reviewed
only the [immutable V10 export](2026-10-18_v10_frozen_review_export.md),
read-only, and returned **APPROVE for V10 capture allocation bounds only**.
It did not edit, build or execute the shared D checkout or WSL copy.

The reviewer rechecked 270/270 frozen source hashes, 135/135 evidence hashes,
224/224 final narrow inputs against frozen source, five portable binaries and
20 runtime inputs. It traced semantic allocation/capacity transfer and
release, MIR semantic preflight/exact allocation/single rollback/clear,
AIR pre-traversal guards, and the C/LLVM/parallel plan checks before output
mutation or plan allocation. It matched the 16 AIR fault restorations, the
allocation lifecycle unit, V9 ASan overflow prefix and V10 current-query
sanitizer refusal, and fresh portable missing-input/restoration receipts.

The review is source-and-receipt validation, not a new execution. Its scope
excludes simultaneous forged native pointers and capacity metadata, arbitrary
capacity-only damage with active count zero, and universal memory safety.
Canonical admission, bootstrap/gen2==gen3, installed C/LLVM driver and
published-SHA CI remain open; the independent review did not approve V1 or
mark any SoT row CLOSED.
