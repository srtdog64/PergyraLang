# Main and CI

Owner: GPT root lane. Checked 2026-10-21 KST after the native loop candidate.

| Item | Observed value |
|---|---|
| Compiler source checkpoint | `c9513d5b27f4038e3a548109fc57e19254208177` |
| Integrated source parent | `fa66a2fb231bb6b5e1f15e0ffa8a2f175f89194d`, including Claude `914b3bbb0` |
| Compiler status | native loop-cleanup candidate; [evidence and remaining boundaries](../evidence/2026-10-21_native_mir_loop_local_cleanup.md) |
| Published main, verified 2026-10-21 | `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe` |
| History relation | published main is an ancestor of the compiler checkpoint; 167 local commits ahead, 0 behind at that checkpoint |
| Required CI | [run38860052641](https://github.com/srtdog64/PergyraLang/actions/runs/38860052641): failure |
| Latest Platform full | [run39150389845](https://github.com/srtdog64/PergyraLang/actions/runs/39150389845): failure on the same published SHA |

This state update follows the compiler checkpoint without changing its
source. No candidate CI was triggered and nothing known red was pushed.
Bootstrap, installed-driver C/LLVM and gen2 == gen3 remain unestablished on
the current source. See [gates](gates.md), not the historical handoff cards.

Remaining unrelated work is preserved/excluded: the separate
`docs/01_intent_first_design.md` change,
SHA256 `100AFB338B73369DC0F2E73B248763065EE7B666C2BEEBEEDE25FB96ACC23867`.
The untracked 2026-10-18 indexed-String ownership-chain directive is also
preserved and excluded. No compiler source/test WIP remains after the
checkpoint; resolve live status when resuming.
