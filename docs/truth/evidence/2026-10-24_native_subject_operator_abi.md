# Native temporary subject / free operator ABI receipt

Date: 2026-10-24. This is local native-backend evidence, not a published-commit
CI, self-host, ownership-cleanup or whole-language safety verdict.

## Source and executable provenance

The fixed base is `7313e9dfc6e45ffab10e7d5f665e576af4623f3d`. Validation used
the existing private export `/tmp/pgy-operator-reuse.qAlfP0` in
`Ubuntu-C-WSL`, with LLVM 18. Only committed base files and this lane's edits
were copied into it. The shared checkout's unrelated dirty ownership/self-host
and main truth files were neither edited nor incorporated into these binaries.
In particular, the later dirty `formal_string_store_landing_owner.c` is not
part of this receipt's input. No additional Git worktree was created.

| Executable | Source | SHA-256 |
|---|---|---|
| `bin/pgy-subject-baseline-fresh` | Fixed base, rebuilt | `7e8911c1e76878bfb18674f3534a6418b3f436d1cb8b3cb42bd73cb158e22141` |
| `bin/pgy-subject-boundary-slice` | Common policy slice, committed as `ce7af5fa06317322098e042bc2637a1761d2ba0b` | `789973f5c57c4d74d68e76576d416e1f2a2b7588f6d6f4b25ecbd238538d0fa9` |
| `bin/pgy-subject-final` | Preceding commit plus the five operator source/header edits committed with this receipt | `a645c24e4b520cc122ca8da332a2795ef692cc8d7b9664286697707c649731b9` |
| `bin/pgy-subject-copy-mutant` | Final source with only C address preparation changed to address a copy | `6ad3be328d2efb76410bfb81977c022d0cf5b0cffd2a4bac9135f1a7358112f8` |

Archived source paths were explicitly touched and rebuilt when switching
between slices: archive timestamps can predate existing objects. The
intermediate binary still fails the operator temporary test while its ordinary
constructor call passes; it is not the final binary relabelled as an earlier
slice. The fresh baseline rebuild reproduced its original binary hash.

## Observed defect and correction

`subject Box { var value: Int; }` with
`operator_add_Box(a: Box, b: Int)` uses a pointer parameter. In the baseline,
`Box(40) + 2` supplies a value: C compilation fails and LLVM's verifier rejects
the argument/signature mismatch. A role with the same operator also caused
LLVM's forward declaration to install a value-receiver signature before the
admitted free routine's pointer signature.

The correction reuses existing MIR parameter carriage and call lowering:

- The shared constructor check consumes the MIR nominal header and call
  identity. C keeps the constructor compound literal alive through the call;
  LLVM materializes the proven fresh value in function storage.
- C captures an existing subject's address before evaluating the next operand.
  LLVM prepares nominal arguments through its ordinary-call boundary owner.
  Neither makes a copy of an existing subject to obtain an address.
- LLVM retains an already evaluated primitive operand. This preserves
  left-to-right, single evaluation and the actual type of nested results.
- A free routine owns its LLVM declaration before any role bridge. Role-only
  value-receiver behavior remains unchanged.
- Invalid inout temporaries get a structured backend refusal, not a guessed
  pointer. Subject return escape still gets `PGY_SEM_ANCHORED_HANDLE_COPY`.

The semantic checker and ownership analysis were not changed. This does not
close overload-target MIR identity, all subject call forms or resource-field
cleanup. The [plan](../../agent_work_directives/subject_operator_argument_abi_2026-10-24.md)
fixes the complete bounded chain and excluded work.

## Executed gates

| Gate | Observed result |
|---|---|
| Fresh baseline, final operator corpus | 27 failures; 31 runtime and 24 rejection controls passed |
| Common-policy slice, earlier operator corpus | 24 exact-output executions and 18 semantic rejections passed, C + LLVM |
| Common-policy ordinary constructor and local-identity controls | 4 exact-output executions passed, C + LLVM |
| Final `operator-overload-rules-test-smoke` | 56 exact-output executions and 26 diagnostic rejections passed, C + LLVM |
| Operator vocabulary unit | 9,132 checks passed |
| `test-semantic` | 3,240 passed, 0 failed |
| `test-transpile` | 1,020 passed, 0 failed; includes missing header, mismatched target and shadowed-call refusals |
| Final emitted-C ASan + UBSan | 28 exact-output executions and 13 diagnostic rejections passed |
| Copy mutation | Detected: `subject_local_identity` produced `42\n40\n`, not required `42\n42\n` |
| Existing targeted backend comparison | `subject_inout_action_receiver` and `operator_overload` passed; constructor-store case blocked below |
| Makefile source inventory | Passed |

The runtime runner has independent expected outputs. Added coverage includes
fresh/local/parameter/self/member operands, both-subject and RHS-subject calls,
readonly constructors, inout forwarding/local storage, left-to-right effects,
nested results, ordinary-call controls, free-over-role precedence and semantic
or ABI refusals. The mutation changes generated behavior; no source/doc text
presence test was added.

Sanitizers instrument the emitted C programs, not the compiler. The wrapper
uses clang-18 with ASan, UBSan, use-after-scope checks and no recovery;
`detect_stack_use_after_return=1` is enabled. A retained positive executable
exports `__asan_init` and executes with output `42`. Compiler diagnostics still
include an existing emitted assignment `-Wunused-value` warning; it was not
suppressed and there were no sanitizer findings in these runs.

## Non-green boundaries retained

`native-compiler-boundary-regression-test-smoke` passed its operator, match
single-evaluation, C callable macro hygiene and imported-private-nominal parts,
then failed at `native-string-result-control-ownership-test-smoke`:

```text
ArrayDropOwnedStrings cannot release 'fields': shallow mutation withdrew its owned String-element premise
```

The unchanged baseline fails with the same message. No test was disabled and
no ownership premise was relaxed. Separately,
`subject_param_constructor_store` fails before code generation with
`required return backing relation is incomplete or stale`; baseline and final
both reproduce it. The default backend-comparison inventory also has existing
unregistered cases; the three-case check above used its documented explicit
case arguments, not a successful full inventory run.

The production-C-size aggregate gate reached and passed its owner probe
(35 goldens + 2 cap branches), then stopped because the private export has no
installed self-host driver. Its subsequent header-size target did not run.
This partial probe is not reported as the aggregate gate passing.
The build-pressure contract also reports its Windows execution receipt as not
run on this Linux host; native Windows runtime validation was not performed.

Run logs and binaries remain in the private export (`subject-final-*`,
`subject-slice1-*`, `subject-baseline-*`). Publication, combined dirty-tree
verification and required exact-SHA CI belong to the main worker. This lane
made only local commits and did not push, replace an installed compiler, run
bootstrap, or claim V1/SoT closure.
