# Reached platform checker repair review — 2026-09-05

## Scope and authority

Base publication: `918a7c98e541771fab542713262b643d375d51c0`.
The primary reported macOS job `101307966639` in Platform full `33966692630`
reaching three harness failures. This lane reviewed and changed only the four
test files named in the publication-failure directive. This report is audit
evidence, not compiler semantics, self-host progress, or a CI-green declaration.

The existing memory-concurrency backend profile owns backend selection;
`check_match_pattern_consumer_placement` owns scan-input admission; the gate
subject declaration contract owns native-pipeline opt-out documentation.
No compiler source, fixture, golden, Makefile, workflow, or Git state was changed
by this lane. Other sessions' changes were preserved.

## Observed findings and repairs

### Structured-spawn profile propagation

- `scripts/ci_macos_steps.sh` explicitly supplies
  `PGY_MEMORY_CONCURRENCY_BACKENDS=c` to
  `memory-concurrency-model-test-smoke`. Its prerequisite
  `structured-spawn-lifecycle-test-smoke` previously ignored that input through
  five literal `for backend in c llvm` loops.
- The spawn gate now consumes that same profile; an unset profile still means
  `c llvm`. It accepts the existing backend names singly or together, including
  reversed order, and rejects empty, whitespace-only, unknown, or duplicate
  profiles before running the compiler. There is no platform inference,
  compiler-failure fallback, or quiet LLVM skip.
- All five backend loops use the admitted profile. The existing 19 source
  admission positives still emit C. Every selected backend executes each of
  the three owned-handle transfer positives and must match its unchanged exact
  stdout golden, with zero runtime stderr and successful bounded execution.
  Checking both outputs against the same golden preserves the old default
  C/LLVM parity requirement and also applies the golden to a C-only run.
- All existing semantic negative cases and diagnostic assertions remain.
- **Integration follow-up:** a recipe-local assignment in the parent does not
  configure its prerequisites. After this lane reported that seam, primary
  added propagation of the existing `MEMORY_CONCURRENCY_BACKENDS` value to the
  structured-spawn prerequisite recipe. This lane did not edit the Makefile.

### Placement scan input

- The original checker treated recursive grep exit 0/1 as a complete scan,
  without independently requiring its root to be a directory.
- A new focused mechanics case makes the scanner return no-match status 1 for
  an absent root. Before the owner repair, this produced an observed RED:
  `checker accepted filtered-missing-placement-root` (exit 1, 5.124 seconds,
  `.tmp/self_hosted/component_checker/run.2JNwCY`).
- The owner now requires `[[ -d "$SELF_HOST_DIR" ]]` before either scan. The
  original missing-root negative remains; added coverage rejects the filtered
  absent root and a non-directory operand, accepts a valid empty directory,
  and rejects scanner status 2 for an existing root. Existing same-basename,
  foreign-consumer, and statement-text-parse negatives remain.
- The focused checker then passed (exit 0, 7.549 seconds,
  `.tmp/self_hosted/component_checker/run.WC6MZN`). Other mechanics checks in
  that script, including transport-placement negatives, also ran.
- **Not established:** that Bash 3.2 itself caused the macOS failure. This host
  has not executed the failing macOS shell/grep pair. The observed defect is
  reliance on scanner status for root admission; the no-match injection
  demonstrates that defect without claiming to reproduce a particular BSD
  implementation. Native macOS confirmation belongs to the next CI run.

### Native concurrency-example subject

- `concurrency_examples_smoke.sh` already declared native execution but lacked
  the required `# Subject of this gate:` marker.
- Its preceding comment now names native concurrency execution and semantic
  rejection across C/LLVM, explicitly excluding self-host substitution.
  No example, expected output, compiler selection, or assertion changed.
- `tests/gate_subject_declaration_smoke.sh` passed under a 60-second timeout:
  56 native-subject gates declare a subject, with no self-host-subject opt-out.

## Additional local verification

- The actual spawn profile-selection block was evaluated independently:
  default `c llvm`; four accepted profiles (`c`, `llvm`, `c llvm`, `llvm c`);
  five rejected profiles (empty, blank, `wasm`, `c wasm`, `c c`). All rejected
  profiles returned exit 1. The check also required exactly five profile-driven
  loops and no remaining literal C/LLVM loop. PASS, 1.235 seconds.
- Each of the four changed shell files was separately checked with `bash -n`
  in that same successful static run. This is not a multi-file `bash -n` claim.
- Scoped `git diff --check` passed before executable verification.
- One C-only structured-spawn executable run passed with exit 0 under a
  300-second outer timeout, against the previously isolated native binary
  `.tmp/native_await_integration_20260905/pgy.exe`. Its pre-run SHA-256 was
  `2E0468AAE6521FE804C08ABE679EAF3A166CA1624C208774EE2142D3C870EDCE`.
  The post-run SHA-256 was identical. The terminal confirmed
  `affine completion handles are joined or explicitly transferred (backends: c)`.
  This exercises the retained 19 C source-admission positives, three C runtime
  transfer positives with exact goldens, and 18 C semantic negatives. The
  compiler was not rebuilt or replaced. It is an existing Windows native
  binary with a C-only test profile, not a native macOS C-only compiler build.

## Limits

The full component inventory, full memory/concurrency parent, all-platform
matrix, and a new source-revision compiler build were not run by this lane.
No remote CI or workflow operation was performed. Existing compiler evidence
tests the changed harness, not a newly built publication candidate. The default
C+LLVM profile has static selection evidence here; its complete executable
recheck is not claimed. Disk capacity was below 100 MiB; tests were restricted
to small focused runs and no existing evidence was removed.

## Read-only follow-up: materialized payload-free enum locals

Primary subsequently requested an independent review of the reached SEA
`CaptureFactFromBoundaryFact` failure and the new `enum_member_match_local`
fixture, mutator, and mixed-arity gate expansion. This follow-up changed only
this report. Compiler/gate implementation and executable verification remain
primary's responsibility.

### Observed ownership and remaining-consumer finding

- The initial value-type and local-inventory changes replace a payload-bearing
  subset check with `DirectMirScalarProgramReferencedEnumRow`. That existing
  lookup verifies fact readiness, digest/schema, unique identities, and exact
  type-name equality. It does not turn an unknown type name into `Int`.
- Value types retain the carried nominal spelling. Local inventory still uses
  `DirectMirScalarCfgSourceLocalTypeMatchesResolved`; its only non-exact pairs
  are the existing Option/Result unknown-type cases, not enum/Int equivalence.
- Independent source inspection found one remaining consumer:
  `DirectMirScalarCfgTypedPlanReady` still accepted only payload-bearing enums.
  `DirectMirScalarCfgGraphPlanReady` reaches that final typed check. This was
  reported immediately rather than interpreting the initial two edits as
  complete closure.
- The primary-generated artifact
  `.tmp/self_hosted/mixed_arity_enum/run.v6QKbS/enum-local.direct.c.out` was read
  and contains `program_readiness=0`. Primary separately reported that the SEA
  gate also reached this rejection with candidate `C5A5FFBD`. These are not
  independent executions by this lane and are not PASS evidence.
- Primary then changed the final typed predicate to the same referenced-enum
  lookup. This lane inspected that subsequent diff. It also inspected the
  follow-up local-emitter changes: C uses the payload-free enum ABI owner and
  accepts Int explicitly; LLVM admits only Int, Long, or an owned payload-free
  enum in its ordinal representation branch. Both reject an otherwise
  unrepresented local type instead of the former catch-all integer emission.

### Nominal confusion and output boundaries

- The existing definition admission supplies the resolved local type as the
  expression's expected type. Typed-expression admission rejects a different
  root type; final expression-row readiness repeats exact expected/root type
  equality. Logical-record expression readiness also ties a member's type to
  the declaration-owned field type. Those boundaries remain intact.
- The produced small fixture MIR was inspected: its single materialized local
  is `__pgy_match_12: Signal`, with one matching definition whose
  `abi_type_name` and `expr1` are both `Signal`. The mutator's fixture-shape
  assertions and coordinated local/definition rewrites therefore target the
  intended seam. Its three initial mutations cover an unknown nominal type,
  `Int` erasure, and deletion of the enum declaration.
- **Coverage proposal sent to primary:** also substitute a different declared
  and referenced payload-free enum, preferably with the same ordinal shape.
  That distinguishes exact nominal identity from mere membership in the enum
  table. It is a missing falsifier, not an observed compiler acceptance bug.
  Primary agreed to add it; execution of that extension was not observed here.
- The new negative loop requires driver exit 1, a `CODEGEN ERROR:` diagnostic,
  and absence of the requested output file for both backends. It never passes
  a mutated artifact to `compile_run`. Positive compilation failures and
  nonzero execution fail through explicit compile checks plus shell
  `errexit`/`pipefail`; positive stdout must match the exact golden.
- Existing harness limits were reported: `compile_run` does not assert empty
  runtime stderr, and the new negatives assert requested-file absence rather
  than absence of generated code on stdout. No unchecked execution of mutated
  output was found. These narrower assertions must not be described as a
  comprehensive absence-of-output proof.

No new compiler build, executable test, remote CI operation, or Git write was
performed by this review follow-up. Final current-candidate SEA/enum PASS and
the added declared-other-enum refusal remain primary's verification gate.

## Final integration review: preserved counterexample and refusal evidence

This later review was read-only except for this audit. The current compiler
diff, complete expanded enum gate, Option origin probe, enum-local fixture and
mutator, and the separately preserved variant-name counterexample were read.

### Current source observations

- `MirMatchBindingSourceRowAtOrigin` now returns `Option<Int>`: invalid input,
  missing identity, and duplicate identity return `None`; a unique row zero
  remains `Some(0)`. All three production callers found by a source search
  check presence before using the row. Existing index-column alignment and
  instruction/ordinal-to-local identity checks remain in place. The source
  probe contains 11 assertions against the imported production lookup, not a
  test-local implementation. Primary's reported seven Option mutation pairs
  are separate execution evidence, not a run by this reviewer.
- All three nominal-local consumers now query the existing sealed referenced-
  enum fact. Both local emitters explicitly admit their supported scalar or
  enum representation and reject the previous catch-all integer case. No new
  unchecked Option unwrap or guessed nominal-to-Int conversion was found in
  the reviewed diff.
- The active positive fixture now contains `Signal { Off, On }` and
  `OtherSignal { Disabled, Enabled }`, with the latter referenced by the
  `OtherMark` parameter. Its expected output is `22`, `0`, `3`. The general
  MIR route remains mandatory alongside direct/public C/LLVM routes; it was
  not skipped to obtain success.
- The separate audit
  `docs/audits/2026-09-05_enum_variant_name_reconstruction_counterexample.md`
  explicitly preserves the same-name `Off/On` source as OBSERVED OPEN, the
  compiler hash, original MIR, failure, two name-only consumers, and an exact
  future acceptance result. The retained
  `run.GtNf1M/enum-local.general.c` was independently read and contains the
  stated missing-variant-declaration error. Choosing distinct names for the
  bounded positive does not erase that finding or establish duplicate-name
  support. The OPEN audit remains necessary.

### Material harness finding: wrong rejection can satisfy the new negatives

The latest primary-generated `run.FJMmcN` artifacts were inspected. The positive
direct C and LLVM stdout files both contain `22`, `0`, `3`. However, all four
enum-local mutations, on both backends, have the same 73-byte diagnostic:

```text
CODEGEN ERROR: direct MIR three-routine structural shape is unsupported
```

The new loop's generic `CODEGEN ERROR:` assertion accepts these eight results.
It proves the requested files were refused, but does **not** prove that local
nominal identity or definition expected-type checks rejected the inputs.
This finding was sent to primary immediately; these results must not be counted
as eight reached nominal-local owner falsifiers.

Read-only inspection of the original and mutated MIR established the
confounder:

| MIR input | Signal declarations | Signal parameters | Signal instruction ABI references | Signal record fields |
| --- | ---: | ---: | ---: | ---: |
| Original | 1 | 0 | 1 | 1 |
| Unknown type | 1 | 0 | 0 | 1 |
| Int erasure | 1 | 0 | 0 | 1 |
| Other enum | 1 | 0 | 0 | 1 |
| Missing declaration | 0 | 0 | 1 | 1 |

The referenced-enum producer selects routine-signature and instruction-ABI
references. Thus the first three mutations also remove the only independent
`Signal` reference from that selection while retaining `Reading.signal`.
This can make the enum/record join decline before local type admission. The
terminal three-routine dispatcher then enters its structural classifier before
printing the existing scalar-route rejection receipt, obscuring the earlier
owner. The exact earlier receipt was not independently executed or observed.

**Proposed correction:** keep `Signal` independently referenced in an unchanged
signature/ABI site, retain an unmodified reserialized control, and require the
actual owned rejection stage for the identity mutations. Missing-declaration
rejection may legitimately occur earlier and must be described separately.
Do not weaken the local checks, add a native fallback, accept a generic route
failure as nominal proof, or delete the same-name OPEN counterexample.

The expanded shell gate separately passed `bash -n` within a 60-second timeout.
No executable suite, compiler build, Git operation, or workflow operation was
run by this final review. Primary-reported SEA C 35/35, LLVM 35/35 and missing-
term success are not substituted for the unresolved negative-stage evidence.

### Follow-up: negative-stage confounder resolved for the three local mutations

The later `run.MZKTxr` evidence was inspected without rerunning the compiler.
All four mutations retain one independent `Signal` formal parameter. Parsed
original/roundtrip JSON facts are equal, and the retained roundtrip C/LLVM
stdout files both contain `22`, `0`, `3`.

The unknown-type pair now reports `stage=local_inventory ordinal=1`; the Int-
erasure and declared-other-enum pairs report `stage=admitted-type` for the
`AST_LET_DECL`. These six results reach the intended local/definition admission
boundaries. Only declaration deletion still reports the generic three-routine
refusal; those two results remain pre-output refusal evidence, not local-type
proof. All eight requested output files were absent and stderr files empty.

The current shell assertions require those specific stages in addition to exit
1, `CODEGEN ERROR:`, and output absence, and its roundtrip control executes both
backends against the exact golden. Independent `bash -n` passed. This addresses
the six-case owner-reachability finding above; final execution of the tightened
gate remains primary's run, not an independent reviewer PASS. The same-name
variant finding and the intermediate `if`/exhaustive-return code-103 finding are
both explicitly preserved as OPEN in the separate counterexample audit.
