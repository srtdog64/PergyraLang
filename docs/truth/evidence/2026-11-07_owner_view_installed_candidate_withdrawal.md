# Frozen receipt: withdrawn installed candidate

Observed 2026-11-07 KST after the native production verification receipt.
The separate `installed-candidate-r5` snapshot added main's then-current
`ast_collection_ownership_member_transition_owner.pgy` candidate to native r4.
The candidate file SHA-256 was `9e16485db60165993ada4709cdfeb54f258d99f690c91386d82888d5e9e10f6d`.

Main subsequently reported that existing negative controls still require the
whole-root guard removed by this candidate. This lane did not independently
claim those negative controls passed. It withdrew the candidate rather than
admit an installation with incomplete safety evidence.

Only make's verified process group 8382, whose argv named this lane's exact
`build-r5` and `self-host-compiler`, received SIGTERM. The child returned -15
after 274.307 seconds; the wrapper collected its status. No installed driver
artifact was accepted. Source and logs remain for reproduction.

The validated r4 native snapshot and its optimization patch are unchanged;
they contain none of this semantic candidate. The installed-route obligation
remains open until main's revised exact-member evidence preserves the original
negative controls and the standard seed/installation workflow passes.

Evidence: `/root/pgy-owner-production-20261107/r5-withdrawal.json`,
`main-field-repair-r5.json`, `r5-build-results.json`, `r5-installed-build.log`.
