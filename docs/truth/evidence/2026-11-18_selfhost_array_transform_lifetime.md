# Typed array transform and bounded lifetime evidence

Observed 2026-11-18 KST on local main parent `b2fd8d424f33720fcd857bb811842b28c824e050`.
This is the next reached direct-MIR prerequisite in the existing
[owned-value lifetime plan](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md),
not completion of S1/S2 or of one-process DRV-2 self-compilation.

The fixed full MIR contains an Int and a String `ArrayReverse` call. The
generic transform expression keeps identity 136; its existing owner now
selects an exact typed runtime ABI from the admitted argument. It preserves
old Int ABI IDs and appends String/Bool IDs 275/276. The signature, node
admission, C and LLVM projection, and declaration consumers use that fact;
missing type, wrong arity or forged identity refuses. Reversal changes
elements in place and returns the same descriptor; it grants no allocation,
copy or release right. The existing String sharing consumer follows the
checked alias to its retaining use. The owned-local lifetime consumer permits
an observed/discarded root, but cannot grant cleanup through an unproved
binding, constructor, call or return. The final guard excludes a graph with a
return carrier or local definition from that observation.

## Exact local generation

- Edit checkout `D:\PergyraLang`, validation `/root/mg-main` in
  `Ubuntu-C-WSL`; all 2,840 normalized production `.pgy` files agree with
  map SHA-256 `55aef9b95ab6539099d9cd18595591e16aec97f207ebd512a06b692ce0fa1b26`.
- Native seed: `97f33eab89e7e264ea37c6f9239b886e4129e639e3dc337c556e205216443c48`.
  Actual Pergyra codegen gen2 seed: `ea6c8e08b8bf8d3ff143768be2ac269b339847d6345c27d464a989fd8b2fc1c8`.
  Installed v5 driver: `b39bd3523df9333068f1b91e9cf3e130b49c3ee044402075a2e64c244e656552`.
  Its emitted C: `69c1ea5c7a773fe54f91a492f39c3ffaf25b449846c3cd526817f04abb0326a4`.
- Frozen complete MIR input remains
  `e7767d3059972797baf41654ececd912bfb91a0755547afea6336e6af8bbdf19`.
  WSL receipts and logs are under `.tmp/selfhost-memory-20261117/array-reverse-*v5*`.

## Executed checks

- Pergyra-seeded installed-driver build PASS within unchanged 14 GiB / 1,800 s
  owner; the native compiler did not compile the driver source as a shortcut.
- v5 effect execution gate: ten C/LLVM direct-MIR runs PASS, four forged
  String/Bool graph ABI refusals and eight retaining String alias refusals PASS
  with no output artifact. Evidence directory
  `.tmp/self_hosted/effect-builtin-execution.HrLlMC`.
- Isolated automatic Int/Bool/String retirement: both C and LLVM instrumented
  at `-O0`, zero ASan/LSan leaks. Removing actual frees triggers LSan;
  duplicating a non-null scope-end free triggers double-free. An instrumented
  LLVM volatile store after free triggers heap-use-after-free. All three
  mutants exit 86. A returned Int/Bool record remains readable under ASan in
  both backends; its cleanup is unproved, so leak checking is disabled only
  for that separate retention witness. This is not a leak-free claim for all
  transform programs. `heap.results.json` records each exit and 11 instrumented
  definitions per LLVM module.
- Existing Array<String> cleanup gate PASS: refusals 18+4+6+2; existing
  admitted C/LLVM, native-MIR, binding and nested-return cases preserved.
  `array-reverse-cleanup-v5.log`.
- The unchanged ABI projection and native normalized fixture passed locally
  on v4: runtime-row C/LLVM artifact parity and 30 positive/negative checks;
  public effect admission/execution passed 100 checks. v5 changed only the
  additional conservative owned-local observation guard, and its reached
  execution/cleanup gates above were rerun. Component caps, generated registry
  and docs gates are recorded in [current gates](../state/gates.md).

## Full-input limit

The v5 installed driver ran `--emit-c-artifact-verified` on the complete
`driver_rung2_main.pgy` in one process. Unchanged 14 GiB address-space /
1,800 s boundary: explicit `[PGY PANIC] ... class=oom`, exit 134,
14,077,584 KiB peak RSS, drained process group and no C artifact.
`array-reverse-fullsource-v5.{log,receipt.json}`. GNU and wrapper elapsed
clocks disagree; no wall-time or memory-improvement claim follows.

The earlier v2 frozen-MIR consumer stops at ownership transition routine
1437, `SemanticAstSignatureTypeTextChildren`, with no artifact. A diagnostic
run found five transition failures and then an unrelated statement refusal at
routine 1672; it did not finish the failure census and emitted no accepted
program. Neither direct-MIR full admission, value-retire S1/S2, full driver
fixed point nor candidate CI is proved by this slice.
