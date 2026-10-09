# Ownership cutover candidate: exact-SHA CI repair

Status: `ACTIVE / CANDIDATE PUSHED; CI NOT GREEN`.
Base: `13fdf36b8fb914a282ce0f6c38b9d9bb57d119fa`, branch
`codex/ownership-clean-cutover-2026-10-09`. The user explicitly authorized
commit, push and CI repair. This does not activate incomplete automatic
cleanup, install the candidate officially, or authorize a main merge.
Other writers remain stopped; one shared checkout, no parallel edit lanes.
Untracked `gmon.out` is preserved and excluded.

## Objective card and reached chain

- Objective: run the complete existing CI on an identified candidate SHA and
  repair its observed failures without changing safety or validation scope.
- Priority: exact input/artifact identity, required executable and kernel
  checks, explicit refusal, then build portability and patch size.
- Owners: `ci_change_scope_owner.sh` owns full/Markdown classification;
  `ci.yml` owns artifact dependencies. `rocq_toolchain_owner.sh` and the
  installer own the admitted stable proof toolchain. Compiler meaning stays
  with its existing typed fact owners and doc 27.
- Chain: full dispatch -> native/codegen fixed point -> receipt-bound DRV-2
  -> the same installed pair for Linux contracts, backend shards and
  sanitizers; independent Windows/macOS/runtime and stable Rocq kernel jobs.
  Every reached failure is checked against that SHA, not an older main run.
- Last consumers: all existing CI jobs, fresh kernel checker and negative
  toolchain tests. A locally green slice is not remote CI or ownership closure.
- Forbidden: skipping a red job, enlarging budgets/caps, accepting an old
  artifact, compiler/backend fallback, manual source ownership ceremony, or
  changing the classification to Markdown-only. P1 dependencies remain.
- Integration owner: GPT. Gate: the full `CI` workflow on the pushed SHA.
  Falsifiers: version/stdlib/kernel mismatch, an unadmitted seed or driver,
  a failed existing test, and any required full job not executing.
- Budgets: static 60 s, focused 300 s, integration 1800 s at the unchanged
  memory ceiling. Actual remote job limits remain unchanged.

## First observed failure: fresh Rocq switch bootstrap

Run `37866547046`, job `113614585641`, fails before any proof is compiled.
The checksum-pinned opam succeeds, but fresh `ocaml-system` switch creation
cannot admit the bootstrap image's opam-managed OCaml as a system compiler.
The solver reports an unavailable `ocaml-system` invariant. Do not describe
this as a theorem failure or weaken Rocq 9.3.0 admission.

Bounded change: the existing installer creates a fresh project switch using
an explicitly versioned, source-built `ocaml-base-compiler`, independent of
the image's active switch. Existing installed switches are preserved; there
is no retry or system-compiler fallback. The same installer still pins opam,
Rocq core/runtime and Stdlib; the same runner executes fresh kernel checks.
Validate shell syntax, an isolated fresh-prefix installation, existing
version/kernel refusal tests and the full remote proof job before claiming
the bootstrap repaired. Outputs here are candidates and observations only.

## Second observed failure: macOS bounded test execution

The same run's macOS job `113614585476` passes native unit batteries and
three boundary regressions, then fails with `timeout: command not found`
in `string_result_control_ownership_smoke.sh`. This is a runner dependency,
not an ownership verdict. Its existing 45 s compilation and 10 s execution
budgets must stay bounded.

Route all three subprocess sites through the existing
`portable_process_helpers.sh` owner (GNU timeout, macOS gtimeout or bounded
Perl execution). Keep separate stdout/stderr evidence, every expected output,
three deep-drop negatives per backend, exact semantic refusal status and
absence-of-artifact checks. No unbounded branch or new timeout implementation.
Validate C/LLVM on the identified native candidate and force the existing
Perl route before rerunning macOS CI. Kernel/compiler meaning is unchanged.

Observed local validation: shell syntax and existing Rocq stable-version,
Stdlib and checker refusals PASS. The unchanged native candidate
`262768f6...919b0` passes String-result C/LLVM success/negative paths and the
forced Perl C route. Documentation quality PASS. The fresh isolated OCaml
4.14.2 / Rocq core/runtime 9.3.0 / Stdlib 9.2.0 installation completed with
exit 0 under a new project prefix, independent of the existing WSL switch.
Its fresh corpus kernel check PASS: 78 modules plus approval binding consumer,
only the two existing approved Slot abstractions. Version/Stdlib/checker
refusal self-test also PASS with that fresh toolchain. Source UTF-8 PASS.
This is local bootstrap/kernel evidence, not remote CI success. The original
candidate's Windows, TSan and codegen fixed-point jobs have passed; its DRV-2
and downstream jobs still need observation.
