# 2026-11-16: native flow audit demand order (`41ab40e3f`)

Run by the Claude root lane on WSL `Ubuntu-C-WSL`. The fix tree is
`/root/te-flow` at `931d3d471`, which `41ab40e3f` squashes. Main-equivalent
binaries are in `/root/ag-selfhost`.

## The DRV-2 entry

Inputs: `driver_rung2_main.pgy` and `driver_bootstrap_main.pgy`, both in
`src/self_hosted/compiler/driver/`.

| Input | main | fix |
|---|---|---|
| `driver_rung2_main.pgy`, native C | 20 errors | 0 errors (68 MB of C, RSS 2.7 GB) |
| `driver_rung2_main.pgy`, native LLVM | 20 errors | 0 semantic errors, then aborted by local LLVM 18.1.3 InstCombine |
| `driver_bootstrap_main.pgy`, native C | 0 errors | 0 errors; only copy sites differ |

- The InstCombine abort also happens on main for the bootstrap entry. It
  is a local LLVM limitation: LLVM 18.1.3 ignores the
  `instcombine-no-verify-fixpoint` attribute (see `e1a9cba9e`).
- A scratch build that counted failed rows checks found 32274 on the DRV-2
  entry and 67377 on the bootstrap entry. All were at the
  declared-result frame site in the ALL audit.

## Native sanitizer sweep

Compared over the same 3996 program-legs:
- `tests/cases`: 2248 programs on native C;
- the 868 lifetime fixtures in `collection_field_lifetime/` and
  `aggregate_storage_transfer/`, on native C and native LLVM.

Each admitted program was built with ASan/UBSan and run for at most 10 s.

| | main | fix |
|---|---|---|
| native C admitted (of 3128) | 2329 | 2330 |
| native LLVM admitted (of 868) | 568 | 568 |
| ASan reports, C / LLVM | 3 / 2 | 3 / 2 |
| UBSan reports, C | 4 | 4 |

- The one verdict change is the new fixture
  `declared_result_unbuilt_intent_audit_valid`, which is not in the
  main tree.
- The ASan reports on both builds are stack overflows from endless
  recursion: `inout_cycle_drop_negative`, `inout_cycle_read_positive` and
  `function_param_flow_summary/main.pgy`.
- The UBSan reports on both builds are from the integer-boundary fixtures
  (`int_literal_signed_minimum` and three in `pgy_math_adversarial`).
- Two stdout differences are not regressions. One fixture prints a
  `DirWalk` listing, which is longer because of the two new fixtures. A
  parallel join prints its lines in a different order.
