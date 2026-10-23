# Ownership reuse slice

Owner: ownership-reuse lane, authorized 2026-10-24. Main integration and push
remain with the main worker; its state files are not edited by this lane.

- Base: `fdc661ff9bbc97033cc2a9615ab4ab3e0d7f5f99` on shared main.
- Plan and allowed scope: [ownership reuse](../../agent_work_directives/ownership_reuse_2026-10-24.md).
- Validation source: `/tmp/pgy-ownership-reuse.oBISPV` on Ubuntu-C-WSL, an
  export of the base plus only this lane's patch. Shared dirty work is excluded.
- Copy traversal: 2,670 checks PASS, including 64 four-binding graphs,
  field boundaries, cycles, row reallocation and both actual consumers.
- Unchanged parameter-flow regression: baseline PASS; copy candidate PASS
  both normally and with flow-currency/worklist replay enabled.
- Copy candidate binary SHA-256:
  `d5727069d54baaf773c643d7d4b35cb0bcdc3435fa0b62d34c729d5eb7221ef6`.
- Copy commit: `71b54aebc9d5ca61d6f5bbdfd033a3f2c8fb190e`.
- Region slice: the four LLVM files and receipt unit in the commit containing
  this state. The cumulative binary is identified in the [run record](../evidence/2026-10-24_ownership_decision_reuse.md).
- Region consumer faults: 115 checks PASS before/after; physical-release
  and cleanup-deletion controls PASS on C/LLVM before/after.
- Copy owner/consumer ASan+UBSan unit: 2,670 checks PASS; this instruments
  the three move/copy owners and the unit, not the whole compiler.
- Local skill reuse guidance and its validator: PASS. `.agents/` stays
  excluded by the existing Git rule; no skill files are force-added.
- Next owner: main worker combines its separate dirty changes, runs the
  canonical/bootstrap/installed-driver gates and publishes when ready.
  No installed binary replacement, V1 closure, push or CI claim here.
