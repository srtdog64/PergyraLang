# Main and CI

Owner: GPT root lane. Checked 2026-10-23 KST after the AST graph-atom prebind
slice.

| Item | Observed value |
|---|---|
| Latest verified source/census commit | `4716f9539b84841c507010bd919a427cfb73cc86` |
| Integrated local main | retained routine/transition ownership plus AST graph-atom prebinding through `4716f9539` |
| Exact-source native compiler | not built; the fixed source-reading census is still red |
| Published main | `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe` |
| History relation | origin/main is an ancestor; the verified source commit is local and unpublished |
| Required CI | [run38860052641](https://github.com/srtdog64/PergyraLang/actions/runs/38860052641): failure on published main |
| Latest Platform full | [run39150389845](https://github.com/srtdog64/PergyraLang/actions/runs/39150389845): failure on the same published SHA |

No candidate was pushed. The unchanged canonical fixed census is 309 and the
next mapped family is owned-String release. Bootstrap,
installed-driver C/LLVM, gen2 == gen3 and required exact-SHA CI remain
unestablished. See [gate state](gates.md) and the current
[receipt](../evidence/2026-10-23_ast_graph_atom_prebind_census.md).

Unrelated work remains preserved and excluded: `docs/01_intent_first_design.md`
SHA-256 `100AFB338B73369DC0F2E73B248763065EE7B666C2BEEBEEDE25FB96ACC23867`
and the untracked 2026-10-18 indexed-String ownership-chain directive.
