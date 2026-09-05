# Bootstrap artifact transport review — 2026-09-05

Status: AUDIT COMPLETE for the final source patch, under the input and
verification limits below. The real focused/integration execution verdicts
remain owned by the execution lane and primary, not by this source review.

Base: `b4d22cf2c6e68fcbd42a1ce4a44444de0e7899fa`, with the primary's existing
uncommitted work. This report follows
[Bootstrap Artifact Comparison Review](../agent_work_directives/bootstrap_artifact_comparison_review_2026-09-05.md).
This lane owns only this report. It does not own compiler semantics, the active
bootstrap rung, normalized-artifact policy, publication, or progress status.

## Existing owners and the proposed equivalence

`pgy_selfhost_normalize_text_artifact` in
`tests/self_hosted/parity/llvm_leg_helpers.sh:58` owns the current CR removal,
repo-root stripping from `source_module_path`, and trailing-empty-line policy.
The file-pair function at line 276 originally normalizes both inputs
unconditionally. It then invokes the Pergyra comparator with projection indices
`0`, `2` and the supplied artifact kind.

The actual comparator is
`src/self_hosted/tools/backend_output_comparator/main.pgy`, with its contract in
the adjacent `intent.md`. Its final checks remain material even for equal files:

- `Main` obtains projection identities through `CompilerHarnessProjectionOrExit`;
  invalid projection indices do not become guessed defaults.
- It checks file presence, reads each selected input, splits on newline, and
  compares ordered lines. Missing inputs produce the owned input-error result.
- `ComparatorSourceFields` checks the artifact kind and TestHarness/subprocess
  owner readiness. Raw byte equality is not artifact-kind admission.
- Its line comparison owns `ok` and the process failure result. The shell
  function must invoke it after either transport choice.

For completed, readable, unchanged text inputs `x` and `y`, byte equality
`x == y` implies equal ordered lines. Applying the same existing normalization
to both would also yield equal lines. Therefore passing equal originals can
preserve the comparator's parity verdict, provided the same artifact/projection
checks still run. When raw inputs differ, both must still pass through the
existing normalizer; this transport branch cannot introduce a new tolerance.

This argument is deliberately about the parity verdict, not byte-identical
comparison-report JSON. The original paths replace normalized-file paths in
`source` and subprocess argv. A raw file's trailing blank lines can also change
`expected_lines` and `actual_lines`, because `main.pgy:143` reads the original
text and performs its own split. Those counts must truthfully describe the
selected comparison inputs.

## Initial findings sent to primary

1. **Keep the owner after the identical branch.** A test with identical files
   and an unknown artifact kind must still fail in the real Pergyra comparator.
   `cmp` status 0 may select original paths, never return parity success.
2. **Do not let conditional function calls hide preparation errors.** Wrapping
   normalization in `if ! function ...` suppresses Bash `errexit` within that
   function. In the initial source, failure of
   `root_fwd="$(pgy_selfhost_root_forward_slash)"` can be followed by a
   successful pipeline. The root helper similarly assigns `windows_root`
   without an explicit failure return. Guard the existing assignments, the
   selected-path conversions, and normalization results explicitly; do not
   depend only on ambient `set -e`. The pipeline itself also requires its
   existing `pipefail` contract to expose a failed nonfinal stage.
3. **Raw transport has an input-path precondition.** The previous file-pair
   function converted only normalized output paths to repo-relative form. The
   direct branch converts original paths, so original inputs must themselves
   be within the supported repo-root path spelling. The inspected bootstrap
   callers use completed files in `BUILD_DIR` and satisfy this condition.
   Do not claim arbitrary external or relative original paths have unchanged
   admission across both branches.
4. **Byte comparison does not establish immutability.** The bootstrap producer
   functions finish in the foreground, check successful/nonempty output, then
   call comparison. Its only inspected background process prints heartbeats.
   Under that sequencing, no producer is intentionally still writing these
   files. A concurrent writer in a shared build directory would invalidate the
   equivalence premise; neither `cmp` nor direct `ReadFile` creates a snapshot.

Primary acknowledged findings 2–4 and stated that the existing preparation
assignments will get explicit return guards, with a root-conversion failure
case and comments distinguishing verdict equivalence from report metadata.
That interim acknowledgement was a proposed repair, not a PASS. The final
source verification below records what was subsequently observed.

## Scope and verification boundaries

- Read the comparison function, normalizer, root/path converters, comparator
  `main.pgy` and `intent.md`, projection/artifact owners, and bootstrap call sites.
  The full-MIR producer and gen2/gen3 calls retain explicit `mir_json` and
  `emitted_c` kinds; the comparison result is used as an admission gate, not
  as a byte-for-byte comparator-JSON fixture.
- The comparator parity parent separately owns exact JSON fixtures for its
  direct calls. Reusing that same executable for transport tests does not
  authorize changing those committed expectations.
- Verified the supplied existing comparator binary's SHA-256:
  `8F11050ADBDB56C9E14552369617E7BFDFB50A47ED4FD89231E4E29B2DDB21FC`, at
  `.tmp/self_hosted/driver/enum_receiver_integration_20260905/backend_output_comparator_38116.exe`.
  Hash verification is not a rebuild or proof that every current dirty source
  change is included in that binary.
- No compiler, full-MIR producer, bootstrap, or full test matrix was executed
  by this lane. No remote CI was queried; no Git writes, deletion, installation,
  or large normalized artifact creation was performed.
- Eliminating normalized files is not a constant-memory or zero-copy claim.
  The comparator still reads and splits both complete inputs in memory. The
  optimization under review avoids two disk artifacts and the corresponding
  normalization work only on the byte-identical branch.

## Final patch review

After the primary announced the patch, independently read the complete new
`text_artifact_file_comparison_owner.sh` and the scoped diffs in
`llvm_leg_helpers.sh` and `backend_output_comparator_parity.sh`.

### Observed repair

- `cmp` status is captured without losing it through `!`. Status 0 retains
  original paths; status 1 normalizes both sides using the existing function;
  every other status explicitly exits 1 with the comparison-error status in
  its diagnostic. Missing input is not treated as a content mismatch.
- Both normalizations have independent explicit failure guards. The two
  preparation assignments now have `|| return $?`, including the nested
  Windows-root conversion. Conditional callers cannot continue past those
  failed assignments to a successful later pipeline. This resolves initial
  finding 2 at the inspected preparation and caller boundaries.
- Both selected-path conversions now have `|| exit 1`. Their failure cannot
  leave an empty/default argument and continue into the comparator.
- Neither successful transport branch returns parity success. Both converge
  on the unchanged real Pergyra comparator invocation with projection indices
  `0`, `2` and the caller's artifact kind. Comparator failure still prints its
  captured output/error and fails the gate.
- The CR/provenance/trailing-empty-line pipeline itself is unchanged. Existing
  caller `pipefail` remains required for nonfinal pipeline-stage I/O failures;
  the inspected bootstrap and new focused gate enable it explicitly.
- The input-lifetime and JSON metadata qualifications from findings 3 and 4
  are now stated in the file-comparison function's comment. They are caller
  obligations, not newly implemented filesystem snapshots or locks.

No source-level parity bypass or new normalization tolerance was found in
this final diff under the documented completed, unchanged repo-root text-input
contract. This is not a claim that arbitrary concurrent/external inputs have
unchanged behavior.

### Focused gate review

The new gate copies and byte-checks the supplied existing comparator into the
normal binding path, then marks that build directory as already compiled. It
does not replace the Pergyra verdict with a shell stub or compile a new tool.
Its subject call is intentionally conditional, so the explicit failure guards
are tested without relying on implicit `errexit`.

The inspected assertions cover:

- identical LF, CRLF, no-final-LF, empty and trailing-blank inputs, with no
  normalized files and with the actual Pergyra schema, `ok`, source paths,
  artifact kind and both projection identities;
- an identical-input `emitted_c` case as well as the default `mir_json` kind;
- identical input with unknown artifact kind, requiring the actual Pergyra
  owner refusal rather than shell success;
- missing inputs on either side and an injected `cmp` status 7, requiring
  explicit failure before the comparator is reached;
- relative original paths on each side in turn, reaching the real path
  conversion function and requiring rejection before the comparator;
- normalized equality for CRLF/trailing blanks, absent final LF and the
  existing `source_module_path` provenance transformation;
- meaningful changed-line, interior-blank and other-field provenance drift,
  requiring a mismatch from the real owner;
- expected/actual normalizers that emit usable content and then fail, plus
  direct root-preparation and nested Windows-root-conversion failure, all
  requiring rejection before any comparator result.

The comparator report slot is reset to `comparator not reached` before each
case, so a stale previous green report cannot satisfy a transport-error case.
Raw cases precede the first normalized case, making their no-copy assertions
inspectable without deleting earlier evidence. The integration parent invokes
this gate using its already-built `ARG_BIN`; its existing exact JSON fixture
assertions remain unchanged.

The initial selected-path negative-coverage gap was sent to primary and is now
addressed in the gate source. In the follow-up diff, each original path is
converted to a relative spelling in turn while cwd remains `ROOT_DIR`, so
`cmp` still sees the existing identical files. The actual
`pgy_selfhost_path_relative_to_root` then refuses that spelling; the test requires
its diagnostic, exit 1, and the unreached sentinel. This checks both conversion
guards without substituting a fake converter. This lane inspected the cases
statically; independent execution remains with the other lane.

The follow-up also strengthens `ok` validation by matching the emitted
`{"schema":"pgy.selfhost.backend-output-comparator.v1","ok":...` prefix.
The nested `subprocess_plan.ok` can no longer satisfy a check intended for the
top-level comparison verdict. The added `emitted_c` case checks its known
artifact identity and raw transport; it is not a C compilation/execution claim.

### Independently observed checks and unrun work

With `C:/msys64/usr/bin/bash.exe` and the configured UCRT64/MSYS2/Git PATH, each
of these separate commands completed with exit 0:

```text
timeout 60s bash -n tests/self_hosted/parity/llvm_leg_helpers.sh
timeout 60s bash -n tests/self_hosted/parity/text_artifact_file_comparison_owner.sh
timeout 60s bash -n tests/self_hosted/parity/backend_output_comparator_parity.sh
```

Primary reported the initial old-function RED at `run.FEbn0y` and announced
the patched run `run.l1brYm` as running. This lane did not independently execute
either run and does not record the latter as PASS without a terminal result.
The other independent lane owns execution of the focused transport gate;
primary owns the integration gate and the one bounded comparison of the
already existing full-source MIR pair. No large MIR or normalized file was
created by this source review.

Primary subsequently reported `full-mir.cee6AL` PASS through the actual file-pair
function, with zero normalized copies and unchanged before/after SHA-256 values
(reported prefix `9C254AF4`). This is attributed primary evidence, not an
independent execution or large-file hash reread by this lane. It compares the
existing full-MIR pair; it is not a full bootstrap rerun. The follow-up changes
above complete this lane's requested static review.

## Reached component-placement ratchet follow-up

The primary subsequently reported an integration RED (`063cee`) from the
component inventory's previous blanket `cmp -s` prohibition on
`llvm_leg_helpers.sh`. That historical RED is primary evidence, not a full
inventory execution by this lane. The objective/directive now includes the
reached structural ratchet and its focused mechanics checks.

Independently reviewed only the new transport-related delta in
`tests/self_hosted_component_contract_smoke.sh` and
`tests/self_hosted_component_checker_smoke.sh`; unrelated dirty checker changes
were not modified.

### Observed structure and scope

- `check_artifact_comparison_transport_placement` selects the exact existing
  file-transport function. Inside it, the checker requires the current
  byte-precheck/status-capture spelling and the real comparator invocation
  with projections `0`, `2` and the supplied artifact kind.
- The function body must not contain the literal `return 0` or `exit 0`.
  Removing one selected body from the whole source leaves a residue in which
  the prior literal `cmp -s` ban still applies. This is a bounded exception
  for the named transport owner, not permission for foreign shell comparisons.
- The component inventory also requires the existing comparator parity parent
  to invoke `text_artifact_file_comparison_owner.sh` with `ARG_BIN`.
- The mechanics test extracts the actual checker functions. Every mutated
  synthetic snapshot clears both text and selected-function caches; a cached
  earlier valid body cannot satisfy a later negative. It then checks the
  actual current repository source in a separate subshell.

The new synthetic controls reject: a foreign `cmp -s`, a duplicate transport
body containing the same byte precheck, the wrong comparator projection/call
spelling, a missing byte precheck, literal early `return 0`, and a missing named
owner function. The valid synthetic control and current real source also pass.

These are source-inventory assertions, not shell control-flow proofs. Literal
presence alone does not prove that an invocation executes; absence of literal
`return 0`/`exit 0` does not rule out every possible early-success spelling.
The duplicate-body case rejects its remaining `cmp -s`, not every conceivable
duplicate function definition without that token. The existing 22-case
executable transport gate remains the owner of behavioral verdict preservation
and error-path evidence. No compiler-behavior claim is added to the component
inventory by this review.

### Independent execution

Executed only the permitted focused checker using the configured MSYS2 Bash:

```text
timeout 60s bash tests/self_hosted_component_checker_smoke.sh
```

Observed terminal exit 0 in 4.862 seconds:
`[component-checker] line caps, missing inputs, selected function identity and negative predicates: PASS`.
This includes the new six negative mechanics cases and actual current-source
placement check, as well as that focused script's retained earlier mechanics.
The script generated only its small owned scratch fixtures. This lane did not
run the full component inventory, compiler, bootstrap, executable 22-case gate,
or full matrix in this follow-up, and again edited only this report.
