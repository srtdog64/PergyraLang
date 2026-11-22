# Self-host generality: default-route failures grouped by first error (2026-11-22)

Source: main `cc27d42c6` (WSL `/root/ts2` at `c6c7ff395` plus the change
that became `cc27d42c6`; release-profile installed driver built from it).
This is the baseline for the goal-2 measurement in
[measurements](../state/measurements.md).

## Method

- Input: 1,398 programs, fixed since 2026-11-19: every `tests/cases` and
  `examples` program that declares a `var`, plus the
  `src/self_hosted/codegen/fixture` and `src/self_hosted/mir_lower/fixture`
  programs.
- Each program is compiled twice with the LLVM backend: native
  (`pgy --native-pipeline --backend=llvm`) and the default route through the
  installed self-host driver (`pgy --backend=llvm`). A compiled program runs
  for at most 10 s with no input. A row compares compile status, whether the
  log has `CODEGEN ERROR`, exit status and the stdout SHA-1.
- Rows where native fails to compile are out of scope. For each remaining
  row that differs, the first error is normalized: numbers and local,
  routine, parameter and type-argument names become `N`; a self-host
  diagnostic keys on its `Code` and `surface`.

## Results

| Class | Programs |
|---|---:|
| Equal to native | 529 |
| Native refuses too | 295 |
| Self-host refuses, `CODEGEN ERROR` (direct MIR) | 331 |
| Self-host front end refuses (diagnostic) | 231 |
| Self-host other compile failure | 12 |
| Both run, output or exit differs | 0 |

The 574 non-equal rows fall into 118 first-error clusters. The largest 11
cover 50%, 32 cover 80% and 61 cover 90%; 66 clusters have one program.
Fixing a first error can expose a later one, so 118 is a lower bound.
Largest clusters: `borrow_boundary_escape` 107; scalar-program route seal
32; compiler `Clone` landings direct MIR cannot carry 26; scalar CFG
leaf-operand `AST_BINARY` 25; three-routine structural shape 24;
`ast_artifact_invalid` 19.

The same sweep on the 2026-11-21 driver (native table from 2026-11-19)
gave 529 equal and 576 non-equal in the same 118 clusters.

## `borrow_boundary_escape` (107 programs)

- Native C and LLVM builds of all 107 under AddressSanitizer
  (`/root/legs_lib.sh`, leak detection off): 214 runs, no memory error. So
  on the executed paths native is not unsafe; all 107 are the self-host
  checker being stricter than native. Paths a program does not execute are
  not covered.
- 103 are native checker fixtures under
  `tests/cases/function_param_flow_summary/`; 68 of them are `_landed_`
  (native admits them by inserting a compiler `Clone`). Four are ordinary
  programs: `bool_to_string_owned_drop`, `owned_string_pop`,
  `map_clone_deep`, `zone_shared_array_mutation`.
- Self-host boundary facts: `unproved_exclusive_member_mutation` 38,
  `owned_element_escape` 19, then 11 kinds with 2 to 6 programs each and
  one with 1.
- The planned route is the
  [one hand-off rule decision](../decisions/2026-11-16_one_handoff_rule_for_both_front_ends.md):
  S5a, S5c (compiler `Clone`s carried in direct MIR) and S6 are unlanded.

## Size of the per-shape route tree

`src/self_hosted/compiler/direct_mir/`: 817 tracked files, 74,935 lines
(`wc -l`) at `cc27d42c6`.
