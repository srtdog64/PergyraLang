# Documentation Facade

This page is the task-oriented entry point to PergyraLang documentation.
Numbered documents remain the canonical owners of design decisions, status,
and gates. The functional `Main.md` pages only provide reading order and
navigation; they must not restate mutable status as a second source of truth.

## Functional Areas

| Area | Start Here |
|---|---|
| Language surface and identity | [`language/Main.md`](language/Main.md) |
| Compiler, IR, ABI, and backends | [`compiler/Main.md`](compiler/Main.md) |
| Runtime and materialization | [`runtime/Main.md`](runtime/Main.md) |
| Async and parallel execution | [`concurrency/Main.md`](concurrency/Main.md) |
| Formal semantics and SoT ownership | [`semantics/Main.md`](semantics/Main.md) |
| Security, unsafe, and sandbox boundaries | [`security/Main.md`](security/Main.md) |
| Hard self-host and bootstrap | [`self_hosted/Main.md`](self_hosted/Main.md) |
| Standard library | [`stdlib/Main.md`](stdlib/Main.md) |
| Diagnostics, builds, performance, and contributor tools | [`tooling/Main.md`](tooling/Main.md) |
| Beta closure and release evidence | [`release/Main.md`](release/Main.md) |
| Checked external reviews | [`reviews/Main.md`](reviews/Main.md) |

## Index Roles

- [`INDEX.md`](INDEX.md) is the exhaustive curated index.
- This file and the functional `Main.md` pages are navigation facades.
- Numbered and named design documents own their respective contracts.
- Machine-readable manifests and smoke gates own executable inventories.

## Migration Rule

Each numbered or named design document lives in the area directory of the
facade that owns it; only the indexes, this facade, the Korean README and the
current-work handoff stay at the root. Keep a document's number and file name
when it moves. A move is allowed only together with its inbound links,
scripts, gates and ownership registries in the same change; otherwise add or
improve a facade link.
