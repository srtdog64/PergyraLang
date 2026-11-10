# V.001 canonical, fixed-point and installed-driver evidence

Observed 2026-11-07 KST by the GPT root lane. This is a run record for the
isolated source candidate `/root/pgy-v001-exact.WkZDds`; it is not a published
revision and does not close required CI.

## Pinned inputs

- Local Git base: `60fd1e26d8ea3987efbcbfc65f59b892b234701a`.
- Native compiler SHA-256:
  `89088ba82c1e571b50d3e65834bb47ab220a97c3e78761e36336ceec22c7c234`.
- Pergyra codegen seed SHA-256:
  `954bd3a7905dbbd42bb7fd438d1b76ff101d55df8754dc6b6d10e2f6097a6f7e`.
- Canonical input: `src/self_hosted/compiler/driver_bootstrap_main.pgy`.

## Ownership admission and MIR producer parity

- The exact final analyzer returned `body_ok=true`, an empty body diagnostic
  and exit 0: 1,197.06 s, maximum RSS 9,507,424 KiB.
- Seed and native-oracle full MIR processes exited 0 under the 14 GiB
  address-space bulkhead in 138.753 s and 197.767 s.
- Both MIR artifacts are 389,274,587 bytes and SHA-256
  `55ab513e5e75dc681ce0bb99fbc379b97328ab7809f38dc0f367c0ee663abb1c`.

## Fixed point

- gen2 C: 228,603 lines, SHA-256
  `b470e72850ce96037ff28cdc1d1e1e65910858f7783b747bad75b3c090a6e013`.
- gen2 emit passed in 207.11 s at 10,351,924 KiB maximum RSS.
- host C compilation passed in 129.64 s at 1,618,692 KiB maximum RSS.
- the generated driver's bounded MIR output was byte-equal to the seed output.
- gen3 emit passed in 206.58 s at 10,351,632 KiB maximum RSS.
- gen2 C and gen3 C are byte-equal. The installed fixed-point driver SHA-256 is
  `0bf758c3f31237daa04cc3988becd472f8445dc4c5e41566da5527e7d9ef9ac1`.

## Installed execution

- `self-host-installed-driver-cli-mode-test-smoke`: PASS in 60.64 s. This
  includes C/MIR/LLVM publication, typed diagnostics and binary refusals.
- legacy/composite/nested intent LLVM and source-C execution gates: PASS in
  97.34 s.
- bootstrap policy corpus: PASS, 3/3 in-subset programs, self emission equal
  to oracle emission and runtime equal to each native golden.

## Limit

The one-shot full make invocation was wrapped in a local 2,400-second external
limit and reached gen2 emission before termination. The exact remaining stages
were continued in dependency order from byte-equal MIR and passed. Therefore
the fixed point is established locally, but the unbroken 60-minute GitHub job
and every other required job remain OPEN until the exact candidate is pushed.
Raw/View, store isolation and broader ownership precision were excluded as the
next milestone.
