# Package manifest and lock readiness audit

- Audited revision: `acdab822b7d1ce27c636f73392ebb1d7738bf08a`
- Scope: `pgy.toml` admission, entry/backend selection, existing-lock
  verification, and `pgy.lock` publication
- Classification: **NOT A COMPILER SUBSTITUTION TARGET**
- Product-tool migration readiness: **NOT READY**

## Decision

Package compilation is already separated from package metadata ownership.
After the C package layer admits an entry and backend, default `check`, `lint`,
`prove`, `package`, `build`, `run`, and `test` consume the installed Pergyra
MIR/C/LLVM owners. The remaining C boundary parses a human-authored TOML subset,
chooses package targets, verifies or replaces a local lockfile, and reports
package-tool diagnostics. Replacing that boundary would be Pergyra product-tool
dogfood, not another native compiler substitution.

It is also not ready for such a product migration. No production-reachable
Pergyra owner parses `pgy.toml`, owns the complete admitted Seashell manifest
graph, verifies `pgy.lock`, or publishes the lock. The existing Pergyra module
manifest resolver reads an unrelated JSON roadmap manifest and is a standalone
validator rather than a package producer.

The first missing owner fact is therefore a **typed, admitted Pergyra Seashell
manifest graph** that preserves schema/format identity, package identity,
app/test entry identity, backend selection, deterministic policy, reserved
empty declarations, duplicate/unknown rejection, and package-relative path
admission. A deterministic lock-graph receipt and safe publication outcome are
downstream missing facts; they cannot be created soundly before the manifest
admission fact exists.

## Observations

### 1. Public entrypoint and command split

- `src/pgy_driver.c:191-212` dispatches `fmt`, `init`, `check`, `build`, `run`,
  `test`, `lint`, `prove`, `package`, `publish`, and `install` before ordinary
  source argument admission. The launcher reads `PGY_NATIVE_PIPELINE` once and
  passes the resulting explicit boolean to `driver_run_pkg_command`; the
  package owner does not reinterpret a compiler failure as permission to retry.
- `driver_run_pkg_command` rejects supported-command extra arguments (except
  package `fmt`) before reading metadata, then loads `pgy.toml`
  (`src/compiler/pkg.c:188-213`). Every command except the lock-refreshing
  `package` verifies an existing `pgy.lock` before any command action
  (`src/compiler/pkg.c:214-216`).
- Command mapping is explicit at `src/compiler/pkg.c:218-251`:
  `check`/`lint`/`prove` use the app entry for verification; `build`/`run` use
  the app entry; `test` requests the test entry; `package` verifies the app
  entry and only then writes the lock. `publish` and `install` remain
  fail-closed out-of-beta product surfaces.

### 2. `pgy.toml` fact and failure inventory

The current complete owner is native `PgyPackageManifest`, whose stored fields
are `seashell_schema`, `seashell_format`, package `name`/`version`, optional
`pergyra`/`edition`, app `entry`, optional `test_entry`, backend plus presence
bit, and deterministic value plus presence bit
(`src/compiler/pkg_manifest.h:8-22`).

The native C TOML-subset admission has these observable rules:

- The section vocabulary is exactly `seashell`, `package`, `targets.app`,
  `targets.test`, `dependencies`, `dev-dependencies`, `effects`, `authority`,
  `capabilities`, and `build` (`src/compiler/pkg_manifest.c:141-169`).
- `[seashell]` owns scalar `schema` and `format`; `[package]` owns `name`,
  `version`, `pergyra`, and `edition`; app/test targets own `main`; `[build]`
  owns scalar `backend` and Boolean `deterministic`
  (`src/compiler/pkg_manifest.c:311-343,395-410`). Backend accepts only `c` or
  `llvm`, and an array is rejected explicitly
  (`src/compiler/pkg_manifest.c:245-270`).
- Dependency and dev-dependency entries are rejected as out-of-beta.
  `effects.requires`, `authority.requires`, and
  `capabilities.allow`/`deny` are accepted only as empty arrays
  (`src/compiler/pkg_manifest.c:344-393`). Unknown keys fail closed.
- Duplicate known keys fail through one seen-bit owner
  (`src/compiler/pkg_manifest.c:171-183`); duplicate/unknown sections and
  malformed lines fail with a line-numbered diagnostic
  (`src/compiler/pkg_manifest.c:459-503`). Comments outside strings are
  stripped, while string escapes are unsupported
  (`src/compiler/pkg_manifest.c:80-103`).
- `schema` must equal `pgy.seashell.v1`, `format` must equal `toml-subset`, and
  `name`, `version`, app entry, and backend are required
  (`src/compiler/pkg_manifest.c:506-525`). Package names accept only
  alphanumeric, `.`, `_`, and `-`; target paths must be nonempty forward-slash
  relative paths without absolute, drive, empty, `.` or `..` segments
  (`src/compiler/pkg_manifest.c:526-537`, `273-296`).
- `deterministic` defaults to `true` when omitted
  (`src/compiler/pkg_manifest.c:447-452`). `pergyra`, `edition`, reserved empty
  declaration sections, and a test target are not required. Test selection uses
  `test_entry` when present and otherwise intentionally falls back to the app
  entry (`src/compiler/pkg_manifest.c:559-571`).

The contract accurately describes the authority chain as `pgy.toml ->
Seashell manifest graph -> pgy.lock -> AIR/MIR facts` and prohibits raw TOML as
a backend shortcut (`docs/109_package_module_resolver_contract.md:35-44`). It
also freezes the entry/backend, fail-closed declaration, lock-drift and command
behavior (`docs/109_package_module_resolver_contract.md:53-130`).

### 3. Entry/backend selection versus compiler execution

- `pkg_driver_flags` projects the admitted entry and manifest backend into
  `DriverFlags`, with release optimization and the normal runtime/diagnostic
  defaults (`src/compiler/pkg.c:24-41`). The `manifest == NULL ? C` expression
  is not reached by the current loaded-manifest caller; successful load already
  requires `backend_set`. It must not become a migration fallback.
- `pkg_run_entry` derives the app/test entry exactly once
  (`src/compiler/pkg.c:64-81`). Only the explicit native opt-out calls
  `driver_run_pipeline`; default verification calls the installed self-host MIR
  producer, LLVM selection calls the installed self-host LLVM runner, and C
  selection calls the installed self-host C runner
  (`src/compiler/pkg.c:82-93`). Success prints `pgy <verb>: <entry> ok`.
- The existing executable gate observes the exact installed-owner request
  counts for C and LLVM, rejects a missing installed sibling without native
  retry, and proves a failed package verification leaves the old lock unchanged
  (`tests/self_hosted/parity/package_commands_installed_self_host_owner.sh:48-88,90-172`).
  Consequently the compiler-bearing half is already bounded `SUBSTITUTING`;
  moving metadata parsing does not replace another C semantic/codegen path.
- Current owner documents say the same: package manifest/lock parsing remains
  C-owned orchestration while semantic verification and backend artifact
  production no longer use the native compiler by default
  (`docs/self_hosted/09_selfhost_status.md:781-793`), and classify the parser as
  a native/fail-closed product boundary
  (`docs/self_hosted/10_hard_self_host_contract.md:199-210`).

### 4. Existing-lock verification

The C lock view contains schema, manifest schema, package name/version, source,
entry, backend and deterministic policy (`src/compiler/pkg_lock.c:17-30`). Its
observable behavior is:

- Absence of `pgy.lock` is accepted; only an existing file is parsed and
  compared (`src/compiler/pkg_lock.c:325-335`).
- The accepted section kinds are `[seashell]` and exactly one `[[package]]`
  entry carrying all fields above. Unknown sections/keys, malformed rows,
  multiple package entries, or missing required values fail
  (`src/compiler/pkg_lock.c:178-284`). Unlike manifest admission, this parser
  does not maintain a `seashell`-section seen bit or duplicate-key seen bits;
  repeated `[seashell]` tables and recognized keys are accepted, with the last
  value winning. That is current C behavior, not evidence of a typed Pergyra
  owner.
- Verification compares lock schema `pgy.seashell.lock.v1`, manifest schema,
  name, version, exact local source `path:.`, app entry, backend, and
  deterministic value (`src/compiler/pkg_lock.c:340-348`). Drift reports the
  refresh diagnostic and fails (`src/compiler/pkg_lock.c:349-354`). Changes to
  `test_entry`, `pergyra`, or `edition` are not lock identity today because
  those fields are not serialized.

### 5. Lock publication and mutation boundary

- `pgy package` deliberately skips existing-lock verification so it can refresh
  drift, but calls the installed verification owner first; only a zero result
  reaches `pgy_package_manifest_write_lock`
  (`src/compiler/pkg.c:214-216,240-244`). The existing parity negative confirms
  invalid source cannot replace the prior lock
  (`tests/self_hosted/parity/package_commands_installed_self_host_owner.sh:70-88`).
- Publication writes fixed `pgy.lock` content: generated-file comments,
  `[seashell]` schema and manifest schema, then one `[[package]]` row containing
  name, version, `source = "path:."`, app entry, backend and deterministic
  value. It emits `pgy package: wrote pgy.lock for '<name>'`
  (`src/compiler/pkg_lock.c:286-322`).
- The mutation is a direct `fopen("pgy.lock", "wb")` truncate/write, and
  `fprintf`/`fclose` results are not checked. Thus an I/O failure can leave a
  partial lock while still printing success, and process interruption has no
  publication receipt or recovery boundary. A future Pergyra product migration
  needs an explicit write outcome and safe replace policy; byte parity must not
  be used to retain this unowned failure behavior.

## Comparison with existing Pergyra manifest code

`src/self_hosted/tools/module_manifest_resolver/main.pgy` is not a partial
Seashell owner:

- It reads `docs/language_module_manifest.json`, not `pgy.toml`, through the
  shared JSON fact table
  (`src/self_hosted/tools/module_manifest_resolver/main.pgy:81-104`).
- It counts roadmap module objects and validates only `layer`, `status`, and
  `beta_blocker` field coverage
  (`src/self_hosted/tools/module_manifest_resolver/main.pgy:104-136`). It owns
  neither TOML admission nor package entry/backend/deterministic identity, and
  it cannot read or publish `pgy.lock`.
- Its own contract calls it the first Pergyra-origin gate on a JSON freeze
  surface and explicitly excludes deeper dependency/schema work
  (`src/self_hosted/tools/module_manifest_resolver/intent.md:3-18,62-92`). It is
  a standalone validator/tool, not imported by the installed package execution
  root.

A scoped search of current self-hosted `.pgy` sources finds no `pgy.toml`,
`pgy.lock`, `Seashell`, TOML parser, `PackageManifest`, or `PackageLock`
producer. Internal compiler path manifests and fixture manifests own different
facts and cannot be promoted into this product boundary.

## Inference

There is no remaining implicit native compiler execution in this track. The
remaining native code owns beta package UX, metadata admission and filesystem
mutation around an already substituted compiler. It may be worthwhile future
product dogfood, especially to obtain typed publication failure, but eliminating
these C files would not count as hard self-host compiler replacement progress.

Because the complete Pergyra manifest admission fact does not exist, this track
must not open a migration lease. The correct integrated result is **NOT A
COMPILER SUBSTITUTION TARGET**, with product migration separately **NOT READY**.

## Exact future falsifier (proposal, not completed work)

If package-tool dogfood is explicitly opened later, use one focused fixture
whose `pgy.toml` selects app `main.pgy`, test `tests.pgy`, backend `llvm`, and
`deterministic = false`:

1. The new installed Pergyra metadata owner must return the typed manifest
   admission, and public package selection must invoke the LLVM installed owner
   for app commands and `tests.pgy` for `pgy test` without reading raw TOML a
   second time.
2. With a pre-existing otherwise-valid `pgy.lock` whose backend is changed to
   `c`, `pgy check` must exit non-zero before any installed compiler invocation,
   leave stdout and the lock bytes unchanged, and emit the owned
   `pgy.lock drift detected` diagnostic. This single mutation falsifies guessed
   backend defaults, lock-after-compile ordering, last-reader dual authority,
   and silent lock refresh.
3. After a successful `pgy package`, the new lock must byte-match the declared
   deterministic schema projection; a forced publication failure must return
   non-zero and preserve the prior lock. Native parsing/output may remain only
   an explicit oracle during migration and must never be retried.

No build, test, staging, commit, or push was performed for this audit.
