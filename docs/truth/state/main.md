# Main and CI

Owner: GPT root lane. Checked 2026-11-08 KST.

| Item | Observed value |
|---|---|
| Local main | `60fd1e26d8ea3987efbcbfc65f59b892b234701a` plus preserved dirty work |
| Published main | `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe` |
| Current source candidate | preserved dirty V.001 index plus unstaged later-milestone work; no candidate commit yet |
| Current validation root | `/root/pgy-v001-current-result-v14`, isolated and not a Git checkout |
| Current installed diagnostic driver | `.tmp/v001-compiler-build-v66/pgy-self-driver`; canonical gen2 emit is red |
| Older exact candidate | `/root/pgy-v001-exact.WkZDds`; historical only because its source graph differs |
| Required CI | OPEN: no V.001 candidate SHA has been pushed yet |

The current candidate has no fresh fixed-point or full installed-driver proof.
Raw/View, `let mut`, store isolation and wider SoT work remain the next
milestone and must not enter the V.001 commit. Commit, push and all required
jobs at that exact SHA remain mandatory.
