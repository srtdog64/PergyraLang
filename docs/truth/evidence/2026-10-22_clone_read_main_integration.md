# 2026-10-22: Clone-as-read main integration

Recorded by the GPT root lane after integrating Claude source commit
`64e69eb92` as `c295123ed` and its deep-Clone counterfactual record
`f0bf3e6a1` as `b1e2b532d` on local `main`. Nothing was pushed.

## Independent source validation

- Built `/tmp/pgy-p0-bin/pgy` from clean Claude HEAD `f0bf3e6a1`; SHA-256
  `3fb27b7dac7cde0ee80b2f170fa4c3db97b9e88239c5c264afcc77cc9e8bfaa5`.
- `function-param-flow-summary-test-smoke` passed in normal and full currency/
  worklist verification modes: 717.21 s, peak 254684 KiB. This exceeds the
  repository's 5-minute focused-gate budget; no timeout was raised.
- The fixed canonical census receipt remains the Claude record: 434 -> 383,
  with moves outside `direct_mir_*` 153 -> 102 and no new refusal kind.

## Root codegen-candidate join

Built local `main` plus the uncommitted indexed-place/receiver candidate as
`/tmp/pgy-PergyraLang-bin/pgy`; SHA-256
`ebb0b7f615e396674f0b6d4fe0f6273c64f8adc91221acec387342a5fe63bc7d`.

- Clone positive: C/LLVM compile and print `a41`.
- Shallow Clone growth negative: C/LLVM refuse at 26:40, no artifact.
- Array/member and receiver ABI gate: PASS in 14.23 s, peak 166668 KiB.
- Documentation quality: PASS, including truth size caps and generated
  post-selfhost validation manifest identity.
- Same-backing read-only assignment: C/LLVM compile and print `7`.
- `indexed_rhs_growth` and `indexed_index_growth`: RED; both C/LLVM routes
  still compile and publish artifacts. Prior sanitizer receipts show the
  resulting use-after-free. The semantic temporary-backing loan is not fixed.

Bootstrap, installed-driver execution, gen2 == gen3 and remote CI were not run
because the fixed census and the assignment safety falsifiers remain red.
