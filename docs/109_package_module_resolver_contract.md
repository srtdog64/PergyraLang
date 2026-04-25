# Package And Module Resolver Beta Contract

Status: beta-freeze-source-of-truth.

This document freezes the beta surface for package and module loading. The goal
is honest ecosystem readiness: local file modules and compiler-known stdlib
modules are stable; package registry behavior is not.

Executable gate: `make package-module-resolver-test-smoke`.

## Stable Module Surface

- File imports use `import "relative/path.pgy";`.
- Import paths are resolved relative to the importing file.
- Imported modules are normalized before merge.
- `namespace` and `export` define the public imported surface.
- If a module has explicit exports, non-exported top-level declarations remain
  hidden across the import boundary.
- If a module has no explicit exports, the current beta compatibility rule keeps
  declarations visible.
- Circular imports are rejected deterministically.
- Compiler-known stdlib modules use `use <module>;` and are frozen separately in
  `docs/108_stdlib_beta_freeze.md`.

## Stable Package Surface

Only manifest scaffolding is beta-stable:

```toml
[package]
name = "my-project"
version = "0.1.0"
pergyra = "1.0"
entry = "main.pgy"

[dependencies]

[dev-dependencies]
```

`pgy init <name>` creates this manifest and a `main.pgy` entry file when absent.

## Explicitly Out Of Beta

- `pgy install`.
- Dependency version solving.
- Lockfile format.
- Registry publishing or download.
- Package checksums, signatures, trust roots, and supply-chain integrity.
- remote imports and remote module imports.
- Logical `pgy.*` import syntax.
- SemVer compatibility enforcement for dependencies.

## Diagnostics Contract

- Missing file imports fail in `module_load`.
- Circular imports fail in `module_load` and mention `circular import detected`.
- Source-level package installation fails before normal compile argument parsing
  with `pgy install: package resolution and registry install are out-of-beta`.
- JSON diagnostics for module-load failures must remain parseable arrays.
