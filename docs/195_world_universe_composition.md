# World Composition and Dynamic Module Boundary

Status: `proposed, out-of-beta`
Date: 2026-07-21

This document records the architecture decision for composing modules that
define Pergyra Worlds. It is a design decision and a wiring plan, not a claim
that dynamic World loading is implemented today.

The authoritative owners remain the existing source-of-truth registry,
`docs/192_protocol_abi_api_registry.md`, the Seashell package contract, and
the MIR/AIR owners. This document must not become a second ABI or Gate SoT.

## Decision

A **module** and a **World** are related but are not required to be the same
thing:

```text
Module (source/package and visibility boundary)
  -> zero or more World definitions
       -> Zone, subject, authority, intent, and local state

WorldGraph / UniverseManifest (static composition boundary)
  -> World-to-World imports, exports, protocols, ABI, and capabilities

UniverseInstance (optional runtime composition root)
  -> loaded World instances and their explicit handoff ports
```

If a program contains only one World, a runtime Universe is unnecessary. If
multiple Worlds are composed in one process, a composition owner is necessary
for dependency order, stable World identity, protocol compatibility, and
cross-World capability/handoff policy.

`Universe` should therefore not be introduced as a new general-purpose
language keyword yet. The first implementation should be a verified
`WorldGraph`/`UniverseManifest`; a `UniverseInstance` is added only when the
runtime needs to host more than one World.

## Why a DLL is not itself a World

Other languages commonly put a module behind a shared library and load it with
the operating system. That solves **code discovery and relocation**, not
semantic compatibility. A DLL loaded with `LoadLibrary`/`dlopen` does not prove
that it has the right World protocol, layout, ownership, or authority model.

For Pergyra, the boundary must be split:

```text
source module -> MIR -> AIR local verification
             -> World export manifest
World manifests -> WorldGraph/AIR composition verification
                -> C/LLVM artifact
artifact + descriptor -> runtime loader -> WorldInstance
```

The current runtime DLL path handling in the repository concerns finding the
compiler's C/LLVM runtime libraries on Windows. It is not a World plugin
contract. The stable module surface is still file imports and local Seashell
manifests; registry installation and remote module loading remain out of beta.

## Proposed World artifact contract

The exact encoding is still open, but every loadable World artifact must have
one versioned descriptor, either as a sidecar manifest or a platform-neutral
metadata section. The descriptor must identify at least:

| Fact | Owner/role |
|---|---|
| `world_id`, `module_id`, version | stable composition identity |
| protocol IDs and versions | protocol registry and compatibility policy |
| exported/imported port IDs | WorldGraph edge resolution |
| signature/layout/target digest | ABI admission |
| required capabilities and effects | AIR composition admission |
| lifecycle and handoff shape | runtime attachment boundary |

An external DLL must be wrapped by an adapter that supplies this descriptor.
The loader must not infer a World from exported symbol names or arbitrary
function pointers.

The eventual native entry surface may be C-compatible, for example a
versioned descriptor query plus attach/detach operations. Those names are
proposed only; they are not an implemented ABI:

```c
const PgyWorldDescriptorV1 *pgy_world_descriptor_v1(void);
PgyWorldStatus pgy_world_attach_v1(const PgyWorldHostV1 *host,
                                   PgyWorldHandle *out_world);
PgyWorldStatus pgy_world_detach_v1(PgyWorldHandle world);
```

No raw Pergyra `String`, `Array`, `Slice`, `Slot`, or allocator-owned object
may cross a DLL boundary by layout accident. Cross-boundary values must use a
declared wire/layout row, an opaque handle with an explicit owner, or an
adapter-owned copy. C and LLVM emit the same descriptor facts, but each
backend may use its own platform implementation for the loader and call
stubs.

## Load sequence and failure boundary

The runtime loader is allowed to load code only after the static contract has
been checked:

1. Seashell resolves the artifact path and declared dependency graph.
2. The loader obtains the versioned World descriptor.
3. The descriptor is checked against the WorldGraph/AIR composition plan.
4. Protocol/version, target, layout digest, capability, and lifecycle checks
   are performed.
5. Only then is the World attached and handed explicit ports.

The following are hard failures, not compatibility fallbacks:

- missing or malformed descriptor;
- unknown protocol or unsupported version;
- World/module identity collision;
- signature, layout, target, or calling-convention mismatch;
- undeclared import, export, capability, or handoff edge;
- missing lifecycle operation;
- direct symbol lookup that bypasses the verified descriptor.

`GetProcAddress`/`dlsym` may be used internally by the loader to find the
versioned descriptor entrypoint, but it must never become a second semantic
resolver.

## Beta-first implementation order

Dynamic loading must not block the MIR-only ABI-first backend or self-host
rungs. The implementation order is:

1. Define a two-World static `WorldGraph` fixture with one exported port and
   one explicit handoff edge.
2. Carry the World export/import and ABI facts through MIR and AIR.
3. Emit and compare C/LLVM composition descriptors from the same verified
   plan.
4. Link the two Worlds statically and gate missing-fact, version-mismatch,
   undeclared-edge, and layout-mismatch failures.
5. Only after that, replace static linking with a DLL/shared-library loader
   that consumes the identical descriptor and composition plan.

This makes DLL loading an artifact substitution, not a new semantic path. A
World compiled into the executable and the same World loaded from a DLL must
be admitted by the same manifest, AIR certificate, protocol policy, and ABI
rows.

## Ownership and SoT boundary

| Fact family | Single legitimate owner | Last consumer |
|---|---|---|
| module path/import/export | Seashell/module resolver | package graph and MIR admission |
| local World semantics | semantic/MIR World facts | AIR local verification |
| cross-World composition | future WorldGraph/UniverseManifest owner | AIR composition verifier |
| protocol and ABI compatibility | existing protocol/ABI owner registry | C/LLVM projection and runtime loader |
| OS artifact loading and lifetime | runtime loader | `UniverseInstance` |

The manifest is a projection of these owners, not an additional authority.
The loader must fail when a required owner fact is absent; it must not reopen
AST, source text, or a DLL's untyped symbol table to guess the answer.

## Cross-World intent priority

The question "which World intent runs first?" cannot be answered by exposing
one global scheduler priority. A local scheduler owns execution order inside a
World, while a cross-World handoff is a distributed protocol edge. The
composition layer therefore owns a **partial order and admission policy**, not
a global queue.

The ordering rules are:

1. Causal and dependency order wins. An intent cannot overtake its declared
   predecessor or a required protocol/ownership transition.
2. Authority and capability admission wins over priority. A `critical` intent
   without the required authority is rejected; it is never promoted merely
   because its number is larger.
3. Expiry/deadline is checked before normal scheduling. An expired intent is
   rejected or routed to its declared compensation path.
4. Priority class is compared only among ready intents on the same dependency
   frontier. It is a scheduling hint, not a semantic override.
5. Ties use a deterministic stable key such as
   `(causal_epoch, origin_world_id, intent_id, sequence)`. Hash-map iteration,
   arrival timing, and backend choice must not decide the result.

An eventual cross-World envelope may carry these facts:

```text
intent_id | origin_world | target_port | protocol_id/version
causal_parent | dependency_epoch | priority_class
deadline/ttl | idempotency_key | authority/capability evidence
```

These are proposed protocol facts, not a released wire schema. The numeric
priority is interpreted by the destination World only after the composition
edge and evidence have been admitted. The source World cannot use it to
mutate the destination's local state directly.

When two intents conflict over the same resource, priority alone must not pick
the winner. The resource-owning World or an explicitly declared conflict
protocol decides; the losing intent receives a typed rejection, retry, or
compensation outcome. A global coordinator may provide a total order for a
specific protocol, but that coordinator is an explicit System/WorldGraph
component, not an implicit Universe scheduler.

This keeps the ownership split precise:

| Decision | Owner |
|---|---|
| intent body/step order inside one World | intent semantic/MIR/AIR facts |
| cross-World edge and causal prerequisites | WorldGraph composition owner |
| capability and authority admission | AIR boundary evidence and World owner |
| local ready-queue execution | destination World runtime scheduler |
| retry, idempotency, and compensation | protocol/World contract |

The C and LLVM projections must consume the same admitted ordering facts. They
may implement queues differently, but neither backend may invent a global
priority or reorder a verified causal edge.

## Required future gate

The first executable gate should cover one valid case and the following
negative cases, including cross-World priority:

- two Worlds with a valid declared port and matching protocol version;
- missing descriptor;
- undeclared import/export;
- protocol or ABI version mismatch;
- C/LLVM descriptor or layout digest drift;
- a causal predecessor that cannot be overtaken by a higher-priority intent;
- an unauthorized `critical` intent and a deterministic tie between ready
  intents.

Until that gate exists, this document remains `proposed, out-of-beta`. It must
not be cited as evidence that Pergyra already supports loading arbitrary World
DLLs.

Related contracts:

- `docs/109_package_module_resolver_contract.md`
- `docs/192_protocol_abi_api_registry.md`
- `docs/semantics/00_proof_contract.md`
- `docs/semantics/01_intent_world_zone.md`
- `docs/193_mir_only_abi_first_backend_closure.md`
