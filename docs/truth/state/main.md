# Main and CI

Owner: GPT root lane. Checked 2026-10-18 KST after local V10 checkpoint.

| Item | Observed value |
|---|---|
| Local main | `5be40a9c3`; [compiler checkpoint](../evidence/2026-10-18_v10_local_checkpoint.md) |
| Compiler base | `8404061d78f2d2bf9f2c325be1a67f83ab9904bf` |
| Compiler status | V10 bounds repaired and scoped frozen review APPROVE; canonical RED |
| Documentation integration | resolved and committed at `4a5372b0c`; compiler WIP excluded |
| Merge base | `7dda8a0c2`; main advanced 16 commits, so not fast-forward |
| Recovery | handoff-only stash `a4fdbab27a0500c6f6516615b52a56327ac75e6b` |
| Published main, last checked 2026-10-18 | `39a8fc1db0dc7162ba75ac7f9584f4cf096d8efe` |
| Required CI | [run38860052641](https://github.com/srtdog64/PergyraLang/actions/runs/38860052641): RED |

That run failed codegen-bootstrap, Windows and macOS. Required dependent
bootstrap, installed-driver/backend and Linux jobs were skipped, not passed.
The docs merge did not publish compiler changes or establish CI green.

The separate `docs/01_intent_first_design.md` change is preserved/excluded:
SHA256 `100AFB338B73369DC0F2E73B248763065EE7B666C2BEEBEEDE25FB96ACC23867`.
No known-red push. [Finish-line evidence](gates.md) still requires real
bootstrap and installed C/LLVM execution on one published SHA.
