# Main and CI

Owner: GPT root lane. Checked 2026-10-23 KST after the retained MIR routine and
DIR transition-storage slice.

| Item | Observed value |
|---|---|
| Latest verified source/census commit | `674fb590302db1f7997c262e873c5fbf971692b2` |
| Integrated local main | String physical-release work through `cefd474e0`, cleanup-boundary record `b633e8907`, and retained routine/transition ownership `674fb5903` |
| Current native compiler | exact `674fb5903` tree, SHA-256 `cb0363a4e9889e14fe20b8ff214fcaf8918a2f061fe80e356079ce648e110aba` |
| Published main | `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe` |
| History relation | origin/main is an ancestor; local main is 205 commits ahead and 0 behind |
| Required CI | [run38860052641](https://github.com/srtdog64/PergyraLang/actions/runs/38860052641): failure on published main |
| Latest Platform full | [run39150389845](https://github.com/srtdog64/PergyraLang/actions/runs/39150389845): failure on the same published SHA |

No candidate was pushed. The unchanged canonical fixed census is 310 and the
first remaining refusal is escaped HIR `atom_table` storage. Bootstrap,
installed-driver C/LLVM, gen2 == gen3 and required exact-SHA CI remain
unestablished. See [gate state](gates.md) and the current
[receipt](../evidence/2026-10-23_mir_routine_and_transition_storage_census.md).

Unrelated work remains preserved and excluded: `docs/01_intent_first_design.md`
SHA-256 `100AFB338B73369DC0F2E73B248763065EE7B666C2BEEBEEDE25FB96ACC23867`
and the untracked 2026-10-18 indexed-String ownership-chain directive.
