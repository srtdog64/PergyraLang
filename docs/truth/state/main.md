# Main and CI

Owner: GPT root lane. Checked 2026-10-23 KST after the MIR result-storage
ownership census.

| Item | Observed value |
|---|---|
| Latest verified source/census commit | `4450c83fb652489d4277aee65033534183867d5b` |
| Integrated semantic handoffs | Existing ownership handoffs plus MIR diagnostic-only error storage and retained MIR result-storage ownership are on local main through `4450c83fb` |
| Root indexed-assignment integration | committed as `43413d0f4`; fresh compiler `/tmp/pgy-root-loan-20261022-b/bin/pgy`, SHA-256 `a7d232ce4652ab797653bed9b33db50cce2725529d3bd1e141bb0f070d649975` |
| Published main | `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe` |
| History relation | origin/main is an ancestor; the verified source commit is 199 commits ahead and 0 behind |
| Required CI | [run38860052641](https://github.com/srtdog64/PergyraLang/actions/runs/38860052641): failure on published main |
| Latest Platform full | [run39150389845](https://github.com/srtdog64/PergyraLang/actions/runs/39150389845): failure on the same published SHA |

No candidate was pushed. The unchanged canonical fixed census is 319, down
from 332 by exactly 13 intended rows with no added refusal or hard census
error. The adopted change gives retained MIR output one owner; the rejected
broad-`ref` candidate is not in the tree. Bootstrap, installed-driver C/LLVM,
gen2 == gen3 and required exact-SHA CI remain unestablished because canonical
admission is still red. See the current [gate state](gates.md) and
[receipt](../evidence/2026-10-23_mir_result_storage_ownership_census.md).

Unrelated work remains preserved and excluded: `docs/01_intent_first_design.md`
SHA-256 `100AFB338B73369DC0F2E73B248763065EE7B666C2BEEBEEDE25FB96ACC23867`
and the untracked 2026-10-18 indexed-String ownership-chain directive.
