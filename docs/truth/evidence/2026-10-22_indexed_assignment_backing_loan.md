# 2026-10-22: indexed assignment holds its backing through the store

Recorded by the GPT root lane on local `main` at `b1e2b532d` plus the
uncommitted native place/receiver and final-move-owner candidate. Nothing was
pushed.

## Contract and falsifiers

An indexed assignment evaluates its target receiver and indices before its
right-hand side. Once the target element address is captured, the backing is
lent until the final store. A call in a later index or the right-hand side may
read that backing, but it may not write, grow, release or transfer it.

- `indexed_target_rhs_growth_invalid.pgy`: refused before C or LLVM artifact
  publication with `PGY_SEM_BORROW_ESCAPE` at 12:27.
- `indexed_target_index_growth_invalid.pgy`: refused before either artifact
  with the same diagnostic at 20:17.
- `indexed_target_readonly_valid.pgy`: C and LLVM both print `7`.

The two invalid programs had previously compiled on both backends; the frozen
ASan counterexamples then reported heap-use-after-free. The final move owner now
consumes the existing typed call publication, final parameter-effect summary
and stdlib collection identity. It does not rescan a callee body or create a
second effect owner.

## Observed gates

Fresh compiler: `/tmp/pgy-root-loan-20261022-a/bin/pgy`, SHA-256
`64c9cac91c53dcb47e873ceb0fd3739146d5778ed6095de82e5bd006845a711b`.

| Gate | Result |
|---|---|
| Native compiler build | PASS, 113.61 s, peak 175828 KiB |
| Array/member mutation C/LLVM gate | PASS, 8.9 s |
| Semantic unit suite | 3240 passed, 0 failed; 20.03 s, peak 271032 KiB |
| Transpiler unit suite | 1014 passed, 0 failed |
| Function parameter flow integration | PASS, 393.30 s, peak 254592 KiB |
| Documentation quality and manifest | PASS after the truth state size correction |
| `git diff --check` | PASS |

The flow integration exceeded the repository's five-minute focused budget.
The fixed canonical census, bootstrap, installed driver, fixed point and remote
CI are separate OPEN finish-line gates.
