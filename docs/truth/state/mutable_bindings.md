# Mutable bindings and compound assignment

Owner: [binding implementation lane](../lanes/mutable_bindings.md).
Checked 2026-11-08 on `60fd1e26d8ea3987efbcbfc65f59b892b234701a`
plus preserved dirty work. This is separate from main's frozen V.001 candidate.

## Fixed executable rung

- Native: `/root/pgy-binding-20261108-okx3hp09/bin/pgy-native-gate-v12`
  in `Ubuntu-C-WSL`; SHA-256
  `b5e5a8db11c3b13d4864fc999fdbce705653d77cb1662f7f0001a4ca174db5d5`.
- Typed self-host probe: `/root/pgy-binding-slice-20261108/.tmp/semantic-binding-v4`;
  SHA-256 `53648359c041b41409c01161778b638bbcc40286403ad7c9d36f4730fed7180a`.
- Evidence root: `.tmp/compound-assignment-20261108/evidence/`.
  This is native execution plus an actual self-host body verifier, not an
  installed-driver, fixed-point or CI result.

## Last observed gates

- Native C/LLVM binding and compound cases: 148/148.
- Parser, semantic (3394/0), transpile (1035/0), registry projection,
  TextBuilder owner/diagnostic and disjoint parallel gates: PASS.
- Original nonowner/builder/allocator subset: 36/36 unchanged admissions,
  including pool use-after-destroy and discarded-transfer refusals.
- Actual String-copy/local-collection release: C ASan/UBSan and LLVM LSan PASS;
  deleting emitted cleanup is still detected. This is a bounded release claim.
- Six registry generators: drift checks and C/LLVM projection executions PASS.
- Typed self-host admission: 70/74; indexed RHS/index mutation now refuses,
  readonly/disjoint controls pass. Four other routes remain open below.

## Integration hold and next falsifiers

- Original 920-case admission census on v12 still has eight differences:
  five formerly conservative mutable-carrier refusals now admit; three using
  alias positives refuse immutable writes. Original expectations are unchanged.
- The five newly admitted inputs were run with v12: 8/10 C/LLVM sanitizer
  executions report leaks. Struct-field collection cleanup is not closed.
- `using` write authority cannot be enabled alone: the experimental native
  MIR route copied the value instead of changing the source. That permission
  experiment was reverted; declaration/alias identity must reach both emitters.
- Self-host remaining cases: nested assignment parser, event symbol admission,
  mutable lambda typing, String field copy-on-reuse. Full installed parity is open.
- Full v12 compilation finishes with 20 errors and 24 warnings; no C artifact.
  First failure: `world.pgy:333` / `CompileSourceToC`, required return-backing
  relation incomplete or stale. The two repaired retention rows do not close
  that failure. Native source/binary and the entire input are frozen in
  `allocator-retention-full-v1/`; GNU wall time 14:10.20, RSS 2,637,672 KiB.
- Do not update ownership goldens, stage this lane into the V.001 candidate,
  replace installed binaries or call the feature fully integrated on this evidence.

The [directive](../../agent_work_directives/mutable_bindings_and_compound_assignment_2026-11-08.md)
owns the closure plan; the [user decision](../decisions/2026-11-08_mutable_bindings_and_compound_assignment.md)
fixes the requested language contract. See the [run record](../evidence/2026-11-08_mutable_bindings_v12_verification.md).
