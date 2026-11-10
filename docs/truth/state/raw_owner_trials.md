# Raw owner/View production optimization

Owner: [raw owner/View lane](../lanes/gpt_raw_owner_trials.md).
Observed 2026-11-07 KST at `60fd1e26d8ea3987efbcbfc65f59b892b234701a`
plus preserved shared dirty work. No staging, commit or push.

## Fixed executable boundary

Source: `/root/pgy-owner-production-20261107/candidate-r16`.
Official admitted-seed installer passed in 279.321 s; no native-built
replacement driver was installed.
Launcher: `a655563114dfc0d5810862669d01d4a14e944f71d8eb46d6fb96f511d469da3a`.
Driver: `74768fffba4276c6c7f9ea99c5277c1fd554fdafffc40943654b35b21a380711`.
The [final receipt](../evidence/2026-11-07_installed_array_retirement_r16.md)
owns full source/input/seed identities, scope and results.

## Last observed result

- Same eight unchanged inputs: installed C/LLVM compile and execute 16/16.
- Physical observation: 29,174 allocations = 29,174 frees; zero live blocks.
- Nine shuffled timing repetitions: all 16 ratios <= 1.3. C maximum 1.104494;
  LLVM maximum 1.150376, against the fastest of three raw C baselines.
- Existing installed route/index/arithmetic gates pass. Actual forged MIR
  refusals 46/46 plus two admitted controls; portable replay 48/48.
- Int width/arithmetic/bounds controls 10/10; runtime representation/cache/
  toolchain/explicit prebuilt controls 9/9.
- Additional source positives 23/24. Returned Array LLVM route is still
  unsupported, with the same refusal reproduced on installed r11/r13/r16.
- Supporting r15 native tests: 3,268 pass, two existing diagnostic-wording
  failures; currency verification 13,688 checks, zero failures.

## Patch and next integration obligation

Array provenance producer remains main-owned; shared Slice consumers were
promoted earlier. The final isolated delta connects indexed uses, foreign ABI,
Slice mutation, plain local Array retirement and general C route selection.
Int width facts and runtime-before-IR-optimization close the measured LLVM cost.
No manual drop, copy, annotation, per-workload route or weaker safety rule was
added to the eight inputs.

Package: `.tmp/owner-production-20261107/final-handoff/`.
Full r7-to-r16 patch: 64 files, isolated replay and byte equality pass.
Current integration patch: 61 files adapted to main's C emitter split; check,
isolated replay and function-body equality pass. See the
[verified split handoff](../evidence/2026-11-07_installed_array_main_split_verified.md).
Four fixtures and earlier native optimization dependencies are already shared.
Compiler source was not overwritten during final handoff packaging.

Main owns integration, 16 touched owner cap violations, full canonical input,
gen2 == gen3 and required CI on the integrated commit. Caps were not raised.
Its earlier canonical/fixed-point success is not evidence for this candidate.
This eight-input closure does not close all collection lifetimes or V1.
