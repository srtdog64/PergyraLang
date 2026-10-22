# Main and CI

Owner: GPT root lane. Checked 2026-10-22 KST during canonical/bootstrap
integration.

| Item | Observed value |
|---|---|
| Local main | `b1e2b532d8f5382cf03110fb1fd9c19cb8eb5a0f` |
| Integrated semantic handoffs | String crossing `316e8a30c` -> `0d77e1562`; Clone-as-read `64e69eb92` -> `c295123ed`; deep-Clone counterfactual record `f0bf3e6a1` -> `b1e2b532d` |
| Root codegen candidate | uncommitted working-tree build `/tmp/pgy-root-loan-20261022-a/bin/pgy`, SHA-256 `64c9cac91c53dcb47e873ceb0fd3739146d5778ed6095de82e5bd006845a711b` |
| Published main | `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe` |
| History relation | origin/main is an ancestor; local main is 187 commits ahead and 0 behind |
| Required CI | [run38860052641](https://github.com/srtdog64/PergyraLang/actions/runs/38860052641): failure on published main |
| Latest Platform full | [run39150389845](https://github.com/srtdog64/PergyraLang/actions/runs/39150389845): failure on the same published SHA |

The candidate includes the integrated String-crossing and Clone-as-read rules,
repairs native C/LLVM indexed-member places and by-value method receiver ABI,
and makes the final move owner hold an indexed target's backing through the
store. Both former UAF programs now refuse before C/LLVM artifact publication;
the read-only control and focused runtime gate pass. The exact plan and receipt
are in the directive and
`docs/truth/evidence/2026-10-22_indexed_assignment_backing_loan.md`.

No candidate was pushed. The last integrated fixed census improved from 434 to 383, but
bootstrap, installed-driver C/LLVM, gen2 == gen3 and required exact-SHA CI
remain unestablished. Completed later Claude commits are reviewed before the
next fixed census; only completed commits are merged.

Unrelated work remains preserved and excluded: `docs/01_intent_first_design.md`
SHA-256 `100AFB338B73369DC0F2E73B248763065EE7B666C2BEEBEEDE25FB96ACC23867`
and the untracked 2026-10-18 indexed-String ownership-chain directive.
