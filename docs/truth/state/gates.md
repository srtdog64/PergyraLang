# Gates

Owner: GPT root lane. Checked 2026-11-08 KST. Current candidate evidence is
from `/root/pgy-v001-current-result-v14`; the older exact-candidate receipts
below do not describe the current source index.

## V.001 finish line

| Gate | Last observed result |
|---|---|
| Fixed canonical ownership admission | PASS: direct analyzer `body_ok=true`, empty diagnostic; seed and oracle both produced full MIR |
| Carrier positives | PASS: disjoint restored carrier and unrelated-loop member read |
| Carrier falsifiers | PASS: missing restore, moved/forwarded/root/deferred and mutator cases refused |
| Canonical MIR producer parity | PASS on current validation tree: 394,160,290 bytes, SHA-256 `1a525181...2f232` on both producers |
| Current gen2 emit | RED: installed diagnostic driver v66 refuses omitted-self role routine row 9143 before output |
| Current gen2 == gen3 | OPEN: not reached after the current source changes |
| Current installed C/MIR/LLVM driver | PARTIAL: build, ArrayDrop and owned-local-return gates pass; full canonical route is red |
| Older exact candidate | HISTORICAL: gen2==gen3 and installed contracts passed in `/root/pgy-v001-exact.WkZDds`, but its source graph differs from the current index |
| One-shot full make target | OPEN on the current candidate |
| Required CI at published candidate SHA | OPEN: candidate not pushed |

Historical green evidence cannot be attached to the current candidate. V.001
requires a fresh unbroken bootstrap, installed-driver execution and required
CI at the exact published SHA.
