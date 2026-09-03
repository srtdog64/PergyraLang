# Package And Module Resolver Beta Contract

Status: beta-freeze-source-of-truth.

This document freezes the beta surface for package and module loading. The goal
is honest ecosystem readiness: local file modules, compiler-known stdlib
modules, and local manifest-driven package commands are stable; dependency
solving and package registry behavior are not.

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

## Seashell Manifest Surface

The local package/build/authority system is named **Seashell**. The CLI remains
`pgy`; Seashell is not a second executable. `pgy.toml` stays a TOML-shaped
human-authored declaration surface, but the beta accepts only the documented
Seashell TOML subset. Unsupported TOML constructs fail closed instead of being
silently ignored.
Plain anchor: pgy.toml stays TOML.

The source-of-truth chain is:

```text
pgy.toml (Seashell TOML-subset declaration)
  -> Seashell manifest graph
  -> pgy.lock deterministic package graph
  -> AIR/MIR effect, authority, capability, and ABI owner facts
```

TOML must not become a semantic shortcut. Backends consume owner facts, not raw manifest text.
The Seashell schema marker makes this explicit:

```toml
[seashell]
schema = "pgy.seashell.v1"
format = "toml-subset"
```

## Stable Package Surface

The beta package owner is local and deterministic. `pgy.toml` is the
source-of-truth for entry discovery and backend choice. Effect, authority, and
capability sections are reserved verifier declarations; in beta they must be
empty until their dedicated AIR/MIR checker owner consumes them:

Only manifest scaffolding is beta-stable for ecosystem/package distribution.
The local manifest-driven package commands below are stable compiler entry
points over local files; they are not dependency solving, registry, or remote
package claims.

```toml
[seashell]
schema = "pgy.seashell.v1"
format = "toml-subset"

[package]
name = "my-project"
version = "0.1.0"
pergyra = "1.0"
edition = "2026"

[targets.app]
main = "main.pgy"

[targets.test]
main = "main.pgy"

[dependencies]
# Dependency version solving is out-of-beta; use file imports for now.

[dev-dependencies]

[effects]
requires = []

[authority]
requires = []

[capabilities]
allow = []
deny = []

[build]
backend = "c"
deterministic = true
```

Supported Seashell v1 rules:

- `[seashell].schema` must be exactly `pgy.seashell.v1`.
- `[seashell].format` must be exactly `toml-subset`.
- `[build].backend` is a scalar string (`"c"` or `"llvm"`), not an array.
- Target paths must be forward-slash relative paths inside the package root.
- Unknown sections, unknown keys, duplicate sections, and duplicate keys fail
  closed.
- Non-empty `[effects]`, `[authority]`, or `[capabilities]` declarations fail
  closed until the verifier owner consumes those declarations as AIR/MIR facts.
- If `pgy.lock` exists, package commands verify it against `pgy.toml`; drift is
  rejected until `pgy package` refreshes the deterministic package graph.

Stable local commands:

- `pgy init <name>` creates `pgy.toml`, `pgy.lock`, and a `main.pgy` entry file
  when absent.
- `pgy new <project-dir>` creates a starter project plus `pgy.toml` and
  `pgy.lock`.
- `pgy check` validates the package through AIR/MIR without backend output.
- `pgy build` compiles the package entry.
- `pgy run` compiles and runs the package entry.
- `pgy test` compiles and runs `[targets.test].main` when present, otherwise the
  app entry.
- `pgy fmt --check|--write` checks or formats the package entry.
- `pgy lint` is the package verifier preflight: manifest load + AIR/MIR check.
- `pgy prove` is an evidence preflight, not a theorem. It proves only that the
  current package surface reaches the verifier boundary.
- `pgy package` writes the deterministic local `pgy.lock` artifact.

### Installed compiler ownership

Package subcommands are dispatched before the launcher's ordinary source-file
selector. They therefore must not assume that the later default selector will
delegate on their behalf. `check`, `lint`, `prove`, and the verification phase
of `package` publish one private verified MIR artifact through the installed
`pgy-self-driver`. `build`, `run`, and `test` consume the manifest backend once
and use the installed C or admitted default-runtime LLVM artifact runner. A missing sibling
driver fails closed; it never authorizes a native semantic/codegen retry.

`PGY_NATIVE_PIPELINE=1` is the explicit resolver/bootstrap test opt-out. It is
read by the launcher and passed into the package owner; the package owner does
not reread the environment or reinterpret failure as permission to fall back.
`init`, `new`, and `fmt` retain their existing scaffold/format responsibilities
and are not counted as compiler substitution evidence.

## Explicitly Out Of Beta

- `pgy install`.
- Dependency version solving.
- Registry publishing or download.
- Package checksums, signatures, trust roots, and supply-chain integrity.
- remote imports and remote module imports.
- Logical `pgy.*` import syntax.
- SemVer compatibility enforcement for dependencies.
- Native Pergyra manifest DSL. Seashell uses TOML until the self-hosted parser
  and manifest graph are strong enough to justify a Pergyra-native surface.

### Post-self-host BuildUnit reservation

독립 모듈 빌드는 self-host closure 이후의 작업이다. 현재 `[build]`는 package
entry와 backend를 고르는 beta surface일 뿐, Module별 독립 컴파일·interface
artifact·incremental invalidation을 약속하지 않는다. 이후의 고정 용어와
착수 순서는 `docs/202_module_authority_boundary_design.md` 7절이 소유한다.
물리 grouping은 Seashell 정책이 되고 `Module` 의미론이나 `world`/`zone`
수명에 합쳐지지 않는다.

## Diagnostics Contract

- Missing file imports fail in `module_load`.
- Circular imports fail in `module_load` and mention `circular import detected`.
- Source-level package installation fails before normal compile argument parsing
  with `pgy install: dependency version solving and registry install are out-of-beta`.
- `pgy publish` fails before registry I/O and points to `pgy package`.
- JSON diagnostics for module-load failures must remain parseable arrays.
