# DRV-2 fixed MIR admission census

Observed 2026-11-28 KST. Baseline `e6097ba08096d3ab1f38ea499b373c9149b8fde2`.
The frozen 418,865,243-byte MIR from the 2026-11-17 driver program has
SHA-256 `e7767d3059972797baf41654ececd912bfb91a0755547afea6336e6af8bbdf19`.
The baseline installed driver (`c1687a7d720c26b1936c20b139eb76c847c4c831528aac659c7aefa2f6ff964a`)
refused its ordinary LLVM emission at collection transition ordinal 1438
(`SemanticAstSignatureTypeTextShape`). Peak RSS was 4,086,356 KiB.

## Diagnostic coverage

The scratch executable (`784586e5656f211b2075b08ff2592aff9be41ca7b6095aed515c42e06178473e`)
checked each callable in isolation using the GraphPlan routine admission
owner. Its driver stops before seal and emission. A continuous run reached
ordinal 4,500 before the existing timeout. Completed windows `[2500, 5500)`,
`[5500, 7500)` and `[7500, 9623)` cover the rest of the same input. The
14 GiB address-space and 1,800 s per-process bounds were unchanged. The
overlap agreed, with 9,623 distinct ordinals covered across all runs. The
window runs exited 42 at the diagnostic boundary. No C or LLVM artifact was
emitted by the census.

The combined JSON with routine names, stages and available transition
checks is `/root/mg-main/.tmp/selfhost-memory-20261117/drv2-1128/combined-census-receipt.json`
in `Ubuntu-C-WSL`, SHA-256
`9eff913999cf2b380ba32abf8058244136a4d28617d8df8ad7e38312712b4c20`.

| First reached check | Diagnostic candidates |
| --- | ---: |
| Collection ownership transition | 22 |
| Match binding `LocalRef` | 15 |
| Instruction kind | 13 |
| Builtin call | 9 |
| Indexed assignment | 6 |
| Direct call | 5 |
| Statement | 4 |
| Graph use completeness | 3 |
| Admitted type, inputs, leaf operand, local inventory | 2 each |
| Literal uses, result definition | 1 each |
| Total | 87 |

Ordinal 1205, `SemanticAstExpressionFunctionTableFactsRelease`, passed in
the ordinary run before 1438. The isolated diagnostic disagreed at that
ordinal, leaving 86 candidates. These counts represent the first check reached
per routine. The order of later refusals in full-program admission and
failures masked by a first refusal remain open.

The [return transfer](2026-11-28_direct_mir_owned_result_return.md) and
[readonly carriage](2026-11-28_record_array_readonly_carriage.md) runs
use a new publication from the full source. Its 357,038,615-byte MIR is a
different source generation. Compare later consumer revisions on that
publication byte for byte when locating the next refusal.

## Ordinary full-input admission on the newer publication

At `222cdaef198e1ab522cd38c1311188f9425dce1e`, the release installed driver
from the readonly carriage run consumed the same publication on both
backends. MIR SHA-256
`f0b420ecf94ba21ef3d5eff33534b5bc0c7437b19440ee1631f98562a4f0ca7e`.
Both passed callable-envelope admission and refused the collection ownership
transition at ordinal 1403 (`SemanticExpressionGraphScalarTypeContractReady`).
Both exited 1 without output under the existing process bound.

| Backend | Peak RSS (KiB) | Elapsed (GNU time) |
| --- | ---: | ---: |
| C | 3,833,748 | 12:20.11 |
| LLVM | 3,832,932 | 10:32.34 |

The owning receipts are `full-c.receipt.json` and `full-llvm.receipt.json` in
`/root/drv2-1128/borrow-verified/.tmp/drv2-review/` (Ubuntu-C-WSL).
