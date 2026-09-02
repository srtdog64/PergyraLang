# Codegen Seed Prebuild Receipt — 2026-09-03

Status: `IMPLEMENTATION CANDIDATE — LOCAL GREEN, EXACT CI PENDING`

Exact base: `2b4cf40115e848b0077e759419254390bf83729e` on
`origin/main`.

This directive coordinates one reached compiler-scale performance repair. It
does not own compiler semantics, relax the codegen fixed point, add a general
cache architecture, or claim self-host substitution or SoT progress.

## Objective card

- Objective: make the phony codegen seed prerequisite reject an unchanged
  repeated oracle/gen1/gen2 build before compiler-scale emission while keeping
  the current seed artifacts content-bound and runnable.
- Priority order: current self-host source-graph identity; native compiler and
  C toolchain identity; exact gen2 C/binary output receipt; explicit rebuild on
  changed or damaged evidence; then wall time.
- Fact owner: `codegen_bootstrap_seed_receipt_owner.sh` owns one conservative
  prebuild key and one exact output receipt. It reuses the existing source-graph,
  runtime-header, and compiler fingerprint functions rather than recreating
  those facts.
- Last legitimate consumer: the seed-only branch of
  `codegen_bootstrap.sh`, reached by the phony
  `self-host-codegen-bootstrap-seed-test-smoke` prerequisite.
- Forbidden fallback: timestamps; output existence alone; a key that includes
  the newly relinked PE binary as an input fact; accepting a changed gen2 C or
  binary; skipping runnable validation; weakening full `gen2 == gen3`; or
  starting parallel compiler-scale builds.
- Verification gate: the focused receipt gate must admit identical inputs,
  reject source/toolchain/owner/output mutations, and prove a repeated real
  seed-only target returns before `building oracle tool`. A subsequent
  installed-driver call must reuse before DRV-2 emission.

## Fresh falsifier

- With no semantic or source change, a direct installed-driver build reused in
  12 seconds when the seed prerequisite was bypassed.
- Running the ordinary seed-only prerequisite then took about 227 seconds.
  `gen2.c` remained byte-identical at
  `3F311DDB3CAF040558EFA00FB8AA21DEB7D7D68FD5636D507835F07F66E9CB00`,
  while the relinked Windows `gen2.exe` changed from `850BB5FD...` to
  `A4A468CE...`.
- Because the driver installer conservatively hashes that executable, the
  otherwise unchanged seed invalidates the installed-driver prebuild key and
  causes a second compiler-scale DRV-2 emission. The defect is therefore the
  missing seed prebuild receipt, not an installer semantic fallback.

## Scope, ownership, and budget

- Allowed edits: the seed receipt owner and focused gate; the seed-only
  bootstrap branch; Make/CI/static ratchets; and current coordination/progress
  documents.
- Forbidden overlap: parser/semantic/MIR/backend behavior, installed-driver
  receipt weakening, general query/cache work, unrelated SoT rows, and the
  protected untracked paths.
- Integration owner: the primary task owns implementation, commit/push, and
  exact-head CI interpretation. No parallel implementation scope is open.
- Allowed commands: Bash syntax, focused synthetic receipt gate, two sequential
  seed-only calls, one installed-driver reuse call, static contracts, and one
  exact-head CI run. The static budget is 60 seconds and the focused integration
  budget is five minutes after the first receipt-producing run.
- Output classification: observations and an implementation candidate only.
  Completion changes neither `88/183`, `CLOSED=55 BRIDGE=32 ACTIVE=1`, the nine
  blockers, nor the 83% project forecast.

## Local implementation evidence

- The seed owner binds the conservative self-host source graph, native compiler,
  alias-normalized C compiler identity, runtime headers, compile profile/flags,
  and all scripts that can change seed construction. A separate exact output
  receipt binds both `gen2.c` and `gen2.exe`; changed or non-runnable output
  cannot pass on file existence alone.
- The focused synthetic gate admits identical inputs and rejects source,
  compiler, bootstrap-owner, C output, binary output, and schema mutations.
  Build-source inventory pins the focused gate into both Linux CI step lists;
  hard and component contracts pin the owner, early consumer, output recorder,
  schemas, marker, and line bounds.
- The final first receipt-producing seed run took 307.2 seconds. Its repeated
  seed-only invocation took 20.9 seconds, printed
  `reusing fingerprinted gen2 seed before oracle build`, and never printed the
  oracle build marker. The bound C SHA-256 is `3F311DDB...E9CB00`; the exact
  binary SHA-256 is `3BDDFD...A69108`.
- After the expected one-time DRV-2 invalidation, the first installation took
  304.3 seconds. The immediately repeated complete `self-host-compiler` target
  reused both seed and driver before emission in 32.5 seconds.
- Fresh installed DRV-2 SHA-256 `8EB168AD...E6AC` passes the complete installed
  CLI aggregate in 209.9 seconds. The captured DAG contains one seed reuse, one
  driver reuse, and zero driver emissions. Focused receipt, build-source
  inventory, hard, documentation, and broad component gates are green.
- The expanded preparation contract passed through its source/MIR owner suite,
  then stopped only because this host has no Rocq/Coq executable. The repository
  explicitly requires `PGY_ALLOW_MISSING_COQ=1` for a declared local skip; no
  prover result is claimed, and the exact CI Rocq 9 job remains load-bearing.
- Full `gen2 == gen3` and exact-head platform CI remain the publication
  falsifiers. No semantic owner, SoT row, blocker count, or project percentage
  changes before those pass.
