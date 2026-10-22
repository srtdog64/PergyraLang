# Main and CI

Owner: GPT root lane. Checked 2026-10-22 KST during canonical/bootstrap
integration.

| Item | Observed value |
|---|---|
| Local main | `43413d0f4a2e1f90866d0306e2dc00a8bf59539c` |
| Integrated semantic handoffs | String crossing, Clone-as-read, collection-copy mutation, inout-exit copy, String-element copy and ArrayPop receiver rules are all on local main through `5c8e0ac57` |
| Root indexed-assignment integration | committed as `43413d0f4`; fresh compiler `/tmp/pgy-root-loan-20261022-b/bin/pgy`, SHA-256 `a7d232ce4652ab797653bed9b33db50cce2725529d3bd1e141bb0f070d649975` |
| Published main | `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe` |
| History relation | origin/main is an ancestor; local main is 193 commits ahead and 0 behind |
| Required CI | [run38860052641](https://github.com/srtdog64/PergyraLang/actions/runs/38860052641): failure on published main |
| Latest Platform full | [run39150389845](https://github.com/srtdog64/PergyraLang/actions/runs/39150389845): failure on the same published SHA |

The integrated candidate repairs native C/LLVM indexed-member places and
by-value method receiver ABI, and makes the final move owner hold an indexed
target's backing through the store, including the existing copy owner's exact
source/carrier field paths. Both former UAF programs refuse before artifact
publication; the read-only control and focused runtime gate pass.

No candidate was pushed. The integrated fixed census is 350 with exact refusal
parity against the preceding completed run, but
bootstrap, installed-driver C/LLVM, gen2 == gen3 and required exact-SHA CI
remain unestablished. Completed later Claude commits are reviewed before the
next fixed census; only completed commits are merged.

Unrelated work remains preserved and excluded: `docs/01_intent_first_design.md`
SHA-256 `100AFB338B73369DC0F2E73B248763065EE7B666C2BEEBEEDE25FB96ACC23867`
and the untracked 2026-10-18 indexed-String ownership-chain directive.
