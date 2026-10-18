# Current blocker

Owner: GPT root lane. Checked 2026-10-18 KST; revision in [main](main.md).

The unchanged fixed and current canonical programs were rerun on final V10.
Both still refuse at `diagnostic_owner.pgy:573:39`,
`ArrayPush(codes, found[j])`, with MIR0. See [root replay](../evidence/2026-10-18_v10_root_replay.md).

## Closed prerequisite, active next seam

The V10 allocation repair has a bounded independent
[APPROVE](../evidence/2026-10-18_v10_independent_review.md) and local
[checkpoint](../evidence/2026-10-18_v10_local_checkpoint.md). Its safety
boundary does not admit the canonical program.

The next reached chain begins with `SemanticDiagnosticCallCodesFromContent`
producing `found: Array<String>`, then `found[j]` enters `ArrayPush(codes, ...)`.
The compiler says the indexed String cannot cross this opaque boundary without
current non-retention proof. Survey the typed String element source, the
`ArrayPush` call contract, the receiving array's cleanup owner and C/LLVM
last consumers together before changing code. Keep owned and borrowed
element cases distinguishable and preserve existing negative gates.

The default strict backend comparison, forced 975-case inventory and self-host
parity manifest have broader current-branch failures recorded in [gates](gates.md).
Canonical admission, bootstrap, installed-driver, CI-green, SoT-closed and
whole-language memory-safety claims remain open.
