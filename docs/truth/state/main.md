# Main and CI

Owner: GPT root lane. Checked 2026-10-17 21:30 KST.

| Item | Value |
|---|---|
| Published `origin/main` | `39a8fc1db` (2026-10-16 15:13) |
| Required CI at that SHA | run `38860052641`: RED |
| Local `main` in `D:\PergyraLang` | `7dda8a0c2` (2026-10-17 20:59), 48 commits not pushed |
| Other branches | `claude/docs-truth` (this folder), not merged |

CI `38860052641` at `39a8fc1db`:
- failed: `self-host-codegen-bootstrap-linux`, `build-windows`,
  `build-macos-c-only`;
- passed: `classify-changes`, `tsan-linux`, `formal-proofs-rocq9`;
- skipped, so not passed: `self-host-bootstrap-linux`,
  `backend-compare-toolchain-linux`, `backend-compare-linux`,
  `build-linux`, `sanitizers-linux`, `build-linux-self-host-contracts`.

Push policy: no known-red push. Required CI is green only on one
published SHA, with the bootstrap and installed-driver jobs actually run.
