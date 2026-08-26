# Launcher Native-Path Census — 2026-08-26

Status: **NOT READY**
Audited revision: `acdab822b7d1ce27c636f73392ebb1d7738bf08a`
Directive: `docs/agent_work_directives/remaining_native_boundary_readiness_audit_2026-08-26.md`

This is a read-only ownership audit. No build, executable probe, source edit, or
gate run was performed. The census follows the public returns in
`src/pgy_driver.c`, then expands the package dispatcher and the REPL call that
would otherwise hide a compiler execution.

## Objective card

- **Objective:** enumerate every launcher exit boundary and determine whether
  any remaining implicit native production execution already has a complete,
  production-reachable Pergyra owner and a focused falsifier.
- **Priority:** request identity, complete observable owner, missing-fact
  failure, old-path deletion, executable falsifier, then patch size.
- **Candidate owner:** an existing installed Pergyra producer, not a validator,
  native output parser, fixture projection, or document.
- **Last legitimate consumer:** the exact public launcher/package boundary.
- **Forbidden fallback:** native retry, `new ? old` dual execution, native
  output parsing, or reconstruction from source/AST/JSON.
- **Required gate:** one positive public invocation plus missing-owner and
  unsupported-input negatives that prove the native call is unreachable.

## Before normal argument admission

These branches run before `parse_args()` at `src/pgy_driver.c:194-232`.

| Public request | Reached return | Classification | Observed ownership |
| --- | --- | --- | --- |
| `pgy --self-driver ...` | `driver_run_self_host_command` | installed Pergyra execution, with fail-closed validation | Resolves and executes the sibling `pgy-self-driver`; bad arity, an unsupported internal mode, or a missing sibling returns failure and never enters `driver_run_pipeline`. |
| `pgy fmt FILE ...` | `driver_run_fmt_command` | native product tooling | The native formatter owns token-preserving formatting and optional file publication. It is not a compiler artifact target. |
| package-mode `pgy fmt ...` | `driver_run_pkg_command(..., "fmt", ...)` | native product tooling | Native manifest/lock selection followed by the same native formatter. |
| `pgy init [NAME]` | `driver_run_pkg_init` | native product tooling | Creates `pgy.toml`, optionally `main.pgy`, and a lock through C-owned package metadata code. |
| `pgy check/build/run/test/lint/prove/package/publish/install` | `driver_run_pkg_command` | expanded below | Compiler-bearing verbs use installed Pergyra artifacts by default; metadata, unsupported registry verbs, and explicit opt-out remain native/fail-closed. |
| `pgy debug ...` | `driver_run_debug_command` | native product tooling | Native debugger parsing/semantic inspection and AST walking are part of the debugger product; no Pergyra debugger owner is reached. |
| `pgy scaffold ...` | `driver_run_scaffold_command` | native product tooling | C-owned template/product mutation. |
| `pgy new ...` | allocation failure or `driver_run_scaffold_command(..., "project", ...)` | fail-closed boundary or native product tooling | Allocation failure returns `1`; success is only a spelling of the native scaffold product. |

An unrecognized first positional word is not silently treated as a command: it
continues into normal argument admission as the source path.

## Package subcommand expansion

`src/compiler/pkg.c:188-256` loads the C-owned package manifest, verifies the
existing lock for every verb except `package`, and then dispatches as follows.
The only `driver_run_pipeline` call in the file is guarded by the
launcher-supplied `native_pipeline` boolean at lines 82-84; that boolean comes
from the declared `PGY_NATIVE_PIPELINE` opt-out.

| Verb | Default compiler execution | Other observable boundary | Classification |
| --- | --- | --- | --- |
| `check` | one installed self-host MIR materialization | native manifest/lock read | installed Pergyra execution |
| `lint` | same installed MIR verification as current command semantics | native manifest/lock read | installed Pergyra execution; it does not claim a distinct Pergyra linter |
| `prove` | same installed MIR verification | native preflight message explicitly says “not a theorem” | installed Pergyra execution plus native product messaging |
| `package` | installed MIR verification (`package-check`) | native deterministic lock publication after success | installed Pergyra execution plus native package metadata mutation |
| `build` | installed C or LLVM artifact runner selected by the manifest | native host compile/link | installed Pergyra execution |
| `run` | installed C or LLVM artifact runner | native host compile/link and program execution | installed Pergyra execution |
| `test` | installed C or LLVM artifact runner for the test entry | native host compile/link and program execution | installed Pergyra execution |
| `fmt` | none | native manifest/lock selection and formatter | native product tooling |
| `install` | none | rejected before manifest load as out of beta | fail-closed boundary |
| `publish` | none | rejected after manifest/lock admission as out of beta | fail-closed boundary |
| unknown verb, extra unsupported arguments, missing/invalid manifest, invalid lock | none | explicit diagnostic/failure | fail-closed boundary |
| any compiler-bearing verb with `PGY_NATIVE_PIPELINE` truthy | `driver_run_pipeline` | declared harness/bootstrap opt-out | explicit native oracle, not fallback |

This agrees with the executable structural/behavioral assertions in
`tests/self_hosted/parity/package_commands_installed_self_host_owner.sh`: the
default package commands must not re-enter the native pipeline, and a missing
installed driver must not fall back.

## After normal argument admission

`parse_args()` prints help and exits `0` for `-h/--help`. Unknown options,
missing `-o`, unknown runtime values, a missing source outside the two
source-free modes, and an LLVM emit request in a non-LLVM build exit `1`.
Those are native CLI presentation or fail-closed admission boundaries, not
compiler executions. Its internal `find_driver_option` pointer/null returns and
`apply_driver_option` void guards do not themselves exit the launcher.

| Admitted request | Reached return | Classification | Observed ownership |
| --- | --- | --- | --- |
| `pgy --repl` | `repl_run()` | native product tooling **containing implicit native compiler execution** | `src/compiler/repl.c:145-149` synthesizes a temporary program and calls `driver_run_pipeline(&rf)` without an opt-in. |
| exact `--test-native-mir-json-oracle` | validation failure or `driver_run_pipeline` | fail-closed boundary or explicit native oracle | The flag is test-only, rejects combined/out-of-envelope modes, then requests the frozen native MIR oracle. |
| `--native-pipeline` or truthy `PGY_NATIVE_PIPELINE` | `driver_run_pipeline` | explicit native oracle | This decision precedes every installed selector and is the declared bootstrap/harness opt-out. |
| `--mir SOURCE` | `driver_run_self_host_mir_diagnostic_request` | installed Pergyra execution | Unsupported option combinations, missing owner/artifact, child failure, timeout, empty payload, or write failure fail closed. |
| exact `--machine-manifest-json` | `driver_write_self_host_machine_manifest` | installed Pergyra execution | The installed sibling verifies the installed companion; unsupported combinations or a missing sibling/companion fail closed. |
| exact `--tokens`, `--ast`, `--capability-manifest`, `--dir` | `driver_run_self_host_source_stdout` | installed Pergyra execution | The selection owner admits one exact mode; all other envelopes fail closed. |
| exact `--mir-json SOURCE` | `driver_run_self_host_mir_json` | installed Pergyra execution | Unsupported combinations fail before sibling execution. |
| runtime-free `--emit-llvm` to file or stdout | installed LLVM artifact/stdout owner | installed Pergyra execution | Pergyra materializes the LLVM artifact; native code only owns bounded workspace/publication or stdout transport. |
| `--emit-c` | `driver_run_self_host_c_emit_artifact` | installed Pergyra execution | The sibling owns semantic/codegen output; unsupported run/options fail closed. |
| plain C compile or run | `c_runner_execute_installed_self_host_c` | installed Pergyra execution | Pergyra owns the C artifact; native code owns host compile/link and optional execution. |
| plain LLVM compile or run | `llvm_runner_execute_installed_self_host_llvm` | installed Pergyra execution | Pergyra owns the runtime-free LLVM artifact; native code owns host compile/link and optional execution. |
| any remaining admitted request, including bare `--rir*`, `--air*`, or `--hir*` | `driver_emit_uninstalled_self_host_request_fail` | fail-closed boundary | The mode-specific rejection reports that no installed Pergyra fact owner exists; there is no final native retry. |

The static ratchet in
`tests/self_hosted/parity/public_native_ir_explicit_opt_in_owner.sh` expects
exactly two direct `return driver_run_pipeline(&flags)` statements in the
launcher: the exact test oracle and the explicit CLI/environment opt-out. It
also requires the final uninstalled-request rejection owner. The launcher
therefore has no third/default direct native compiler return.

## Reconciliation with the self-host contracts

The current statement in `docs/self_hosted/10_hard_self_host_contract.md` that
public C/LLVM compile/run and compiler-bearing package commands are bounded
`SUBSTITUTING` matches the current dispatch. Its classification of manifest/
lock parsing, scaffolding, formatting, and debugging as native product
boundaries also matches. The contract should not, however, be read as proof
that REPL compilation is installed-self-hosted: REPL is not named in that
closed list and its implementation still calls the native compiler.

Two older statements are stale as current-state summaries:

- `docs/self_hosted/17_pergyra_native_dogfood_contract.md:1436` says
  `dump/check/repl` are all open. Public MIR and source dump modes and package
  check are now installed-self-hosted; only the REPL portion remains open in
  this census. The similar historical sentence near line 762 also predates the
  later public/package substitutions.
- `src/self_hosted/PROGRESS.md:5270` says plain compile/run, package, and LLVM
  remain `OPEN`. It is an archived older checkpoint and conflicts with the
  active context plus the current hard-self-host contract if read as current.

## Exact residue and readiness conclusion

There is exactly **one implicit native compiler execution reachable from the
public launcher**:

```text
pgy --repl
  -> src/pgy_driver.c:235-236 repl_run()
  -> src/compiler/repl.c:145-149 driver_run_pipeline(&rf)
```

The package dispatch has no implicit native compiler path; its sole native
call requires `PGY_NATIVE_PIPELINE`. The two direct launcher native calls are
likewise explicit test/bootstrap oracles. Formatter, debugger, scaffold/new,
package init/metadata, help, and host compile/link/run are native product or
platform boundaries, not hidden compiler substitutions.

The smallest plausible candidate is the compiler-bearing interior of
`--repl`, because the general installed C compile/run path already owns source
to C artifact production and execution. That does **not** make the public REPL
boundary ready. No production-reachable typed Pergyra owner currently owns the
complete REPL request/receipt: accumulated declaration identity, multiline
admission, definition-versus-evaluation outcome, diagnostic/exit propagation,
and transient-artifact retirement. The existing C runner also requires a
launcher identity that `repl_run(void)` does not receive, and no REPL-focused
falsifier exists.

**Conclusion: NOT READY.** The first missing fact is a typed, production-
reachable **REPL session evaluation receipt** binding the exact accumulated
declarations and submitted input to one success/failure outcome and its
diagnostic/exit meaning. Until that fact has an existing Pergyra producer, the
whole public REPL cannot satisfy the directive's “complete observable
behavior” rule. Reusing the installed compiler for only the temporary program
would be a useful compiler-lane reroute, but it must not be reported as a
Pergyra-owned REPL.

A future focused falsifier would feed one declaration, one successful
evaluation, and one invalid evaluation through public `pgy --repl`; prove an
installed counting sibling is invoked exactly once per evaluation and never
for declaration admission; require missing sibling and unsupported input to
fail without native timing/output or a stale executable; and verify cleanup
and the exact user-visible exit/diagnostic contract. This gate is specified
only as missing evidence, not as completed work.
