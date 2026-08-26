# Remaining Native Boundary Readiness Audit Directive

Status: `AUDIT COMPLETE`; this file coordinates read-only agent work. It is not a
numbered architecture document and owns no compiler fact, progress claim, or
successor implementation rung.

Base revision: `acdab822b7d1ce27c636f73392ebb1d7738bf08a`.

The implicit final `driver_run_pipeline` fallback is already deleted. Public
MIR/C/runtime-free LLVM compile paths and compiler-bearing package execution
are bounded `SUBSTITUTING`; bare unowned RIR/AIR/HIR diagnostics fail closed.
The remaining native entrypoints mix explicit compiler oracles with product
tools such as formatting, REPL, debugging, scaffolding, and package metadata.
This audit determines whether any one live C-owned production boundary already
has a complete Pergyra owner and executable falsifier.

## Shared objective card

- Objective: enumerate the remaining implicit native production boundaries and
  identify the smallest one whose complete observable behavior is already
  owned by typed, production-reachable Pergyra code.
- Priority: exact entrypoint and request identity; existing complete owner;
  missing-fact failure; old-path deletion; executable falsifier; product-value
  impact; then patch size and naming.
- Candidate fact owner: an existing Pergyra producer reached from an installed
  production root. Validators, fixture manifests, audit tools, native output,
  docs, and generated projections are evidence only.
- Last legitimate consumer: the public launcher, package dispatcher, or
  installed self-driver boundary named by the report.
- Forbidden fallback: native retry, native output parsing, source/AST/JSON
  reconstruction, fixture/name dispatch, `new ? old` dual reads, or promoting a
  parser/validator/tool into a producer.
- Integration gate: the primary task must reproduce one exact live bypass,
  name its existing complete Pergyra owner, and specify a focused positive plus
  missing-owner/unsupported-input negative before implementation begins.
  `NOT READY` is required when any observable behavior lacks that owner.

## Independent edit and safety boundary

- Each auditor edits only its assigned report under `docs/audits/`. Do not edit
  source, tests, Make/workflow files, registries, handoff, progress, dogfood,
  collaboration, `AGENTS.md`, this directive, or another report.
- Do not stage, commit, push, build DRV-2, or run full suites. Do not inspect,
  stage, delete, or rewrite the user-owned untracked `pgy-80135c2c/` directory.
- Read-only `rg`, bounded source reads, and existing installed/native CLI probes
  are allowed. Keep static commands under 60 seconds and probes under 5 minutes.
- Separate observed facts, inferences, and proposals. A report may conclude
  `READY`, `NOT READY`, or `NOT A COMPILER SUBSTITUTION TARGET`; none of those
  conclusions opens an implementation lease by itself.

## Launcher native-path census

Assigned report:
`docs/audits/2026-08-26_launcher_native_path_census.md`.

Trace every return reachable from `src/pgy_driver.c` before and after normal
argument admission. Classify it as installed Pergyra execution, explicit native
oracle, fail-closed boundary, implicit native compiler execution, or native
product tooling. Include package subcommand delegation rather than treating it
as one opaque return. Reconcile the result with the current hard-self-host
contract and name any stale statement. End with the exact remaining implicit
native compiler paths, if any, and the smallest candidate that already has a
complete Pergyra owner.

## Package manifest and lock readiness

Assigned report:
`docs/audits/2026-08-26_package_manifest_lock_readiness.md`.

Trace `pgy.toml` loading, entry/backend selection, existing-lock verification,
and lock publication. Compare every observable fact and mutation with existing
Pergyra package/module manifest code. Distinguish compiler execution—which is
already installed-self-hosted—from C-owned package metadata orchestration. End
with `READY`, `NOT READY`, or `NOT A COMPILER SUBSTITUTION TARGET`, the first
missing owner fact, and one exact falsifier for any future migration.

## Native product-surface readiness

Assigned report:
`docs/audits/2026-08-26_native_product_surface_readiness.md`.

Audit formatter, REPL, debugger, scaffold/new, and package init separately.
For each, record its public entrypoint, observable effect, current owner, any
existing production-reachable Pergyra equivalent, and whether replacing it
would count as compiler substitution or product-tool dogfood only. Do not
recommend implementing a new formatter, debugger, REPL, or template system
merely to eliminate C LOC. End with at most one `READY` candidate, otherwise
the first missing fact per surface.

## Integration ownership

The primary task alone reconciles reports against current source, selects or
rejects a successor rung, edits executable files, runs gates, commits, and
publishes. Parallel reports must not share files or infer progress from one
another. If no report finds an existing complete owner, the correct integrated
result is “no successor rung selected,” with the missing owner boundaries
recorded rather than manufactured.

## Integrated result

- The launcher census found exactly one remaining implicit native compiler
  execution: `pgy --repl -> repl_run -> driver_run_pipeline`.
- The complete REPL session is `NOT READY`; no Pergyra owner carries prompts,
  accumulated declarations, multiline admission, or session transitions.
- The narrower per-evaluation compile/run boundary is `READY`: the installed
  Pergyra source-C owner already owns that exact compiler request. Replacing
  only this call does not claim that the REPL product is Pergyra-owned.
- Package manifest/lock admission is `NOT A COMPILER SUBSTITUTION TARGET` and
  separately `NOT READY` for product-tool migration. Formatter, debugger,
  scaffold/new, and package init are also product-tool surfaces without
  complete production Pergyra owners.
- The primary task opened only the REPL compile/run substitution lease. No
  other audit finding became an implementation queue.
