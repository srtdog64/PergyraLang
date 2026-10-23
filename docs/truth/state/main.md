# Main and CI

Owner: GPT root lane. Checked 2026-10-23 KST after the exact inout-member
release/restoration slice.

| Item | Observed value |
|---|---|
| Latest verified source commit | `f90dd715736068312b74f35fa499a81a78f35802` |
| Integrated local main | exact current-inout member release/restoration bridge through `f90dd7157` |
| Exact-source native compiler | built; SHA-256 `8ed9f3af3b182abcaf3ab3e954110ad1c662435b0cff005e20a0dcc5a5966e7e` |
| Published main | `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe` |
| History relation | origin/main is an ancestor; the verified source commit is local and unpublished |
| Required CI | [run38860052641](https://github.com/srtdog64/PergyraLang/actions/runs/38860052641): failure on published main |
| Latest Platform full | [run39150389845](https://github.com/srtdog64/PergyraLang/actions/runs/39150389845): failure on the same published SHA |

No candidate was pushed. The unchanged canonical still fails before MIR; the
next reached blocker is the opaque String non-retention boundary at line 236.
Bootstrap, installed-driver C/LLVM, gen2 == gen3 and required exact-SHA CI
remain unestablished. See [gate state](gates.md) and the current
[receipt](../evidence/2026-10-23_inout_member_release_restore.md).

Unrelated work remains preserved and excluded: `docs/01_intent_first_design.md`
SHA-256 `100AFB338B73369DC0F2E73B248763065EE7B666C2BEEBEEDE25FB96ACC23867`
and the untracked 2026-10-18 indexed-String ownership-chain directive.
