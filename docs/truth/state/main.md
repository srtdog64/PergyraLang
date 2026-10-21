# Main and CI

Owner: GPT root lane. Checked 2026-10-22 KST after native consumer integration.

| Item | Observed value |
|---|---|
| Compiler source checkpoint | `f25b7bb6e286944c99c89c0dd8a53527f9c2564a` |
| Integrated sources | MIR consumer `cc560937b` and whole Claude loan/copy fix `c877a46e4` |
| Compiler status | scoped native connection and named UAF repair verified; [receipts and open integration](../evidence/2026-10-22_array_preservation_native_consumer_integration.md) |
| Published main, verified 2026-10-22 | `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe` |
| History relation | published main is an ancestor of the compiler checkpoint; 178 local commits ahead, 0 behind at that checkpoint |
| Required CI | [run38860052641](https://github.com/srtdog64/PergyraLang/actions/runs/38860052641): failure |
| Latest Platform full | [run39150389845](https://github.com/srtdog64/PergyraLang/actions/runs/39150389845): failure on the same published SHA |

This state update follows the compiler checkpoint without changing its
source. No candidate CI was triggered and nothing known red was pushed.
Bootstrap, installed-driver C/LLVM and gen2 == gen3 remain unestablished on
the current source. See [gates](gates.md), not the historical handoff cards.

The published Array verdict is already consumed by native MIR; it is not a
pending producer handoff. The last independent census remains Claude-owned
in [blocker](blocker.md). Later exploratory work on that lane is not merged
without a completed commit handoff. No new branch/worktree was created.

Remaining unrelated work is preserved/excluded: the separate
`docs/01_intent_first_design.md` change,
SHA256 `100AFB338B73369DC0F2E73B248763065EE7B666C2BEEBEEDE25FB96ACC23867`.
The untracked 2026-10-18 indexed-String ownership-chain directive is also
preserved and excluded. No compiler source/test WIP remains after the
checkpoint; resolve live status when resuming.
