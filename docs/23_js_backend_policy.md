# Pergyra JavaScript Backend Policy

Last updated: 2026-05-10

Status: beta+1 / historical design note. Direct `.pgy -> JS` backend work is not
part of the beta closure path, and it is not the beta or first dogfood path.

## Beta Position

The beta dogfood path is:

```text
Pergyra -> C backend --emit-c -> optional Emscripten/WebGL bridge
```

This path validates host-bridge viability only. It must not add WebGL, DOM, or
JavaScript-specific syntax to the core language. WebGL is not promoted to core language
surface by this bridge.

Reserved post-beta ecosystem modules:

- `pgy.render.webgl`
- `pgy.render.skia`
- `pgy.accel.spray`

Native LLVM wasm and a direct JavaScript backend remain beta+1 or later.

## Core Rule

JavaScript backend pressure must not reshape the core language around JS
`class`, `extends`, prototype chains, or parent-call semantics.

Pergyra's core model stays:

- `subject` is an identity-bearing host.
- `class` is passive nominal structure.
- `struct` is value-shaped data.
- `object` and `tobject` are projection/transfer surfaces.
- `ability` and `role` are contract/dispatch surfaces, not JS interface sugar.
- `relation`, `effect`, `zone`, and `world` lower through explicit state and
  orchestration objects, not inheritance.

## Lowering Direction

If a direct JS backend becomes necessary after beta, it should lower Pergyra
concepts into a small set of runtime shapes:

- records for value-like structures
- cell objects for identity-bearing hosts
- method bundles or dispatch tables for role/ability implementation
- mailbox/task wrappers for async subject or participant execution
- explicit state objects for relation/effect/zone/world synchronization

The backend may use JS `class` internally where convenient, but that is an
implementation detail. It is not the Pergyra semantic model.

## Interop Boundary

JS interop belongs behind an explicit external boundary, for example:

```pergyra
extern js class HTMLElement;
extern js func setTimeout(cb: JsFn, ms: Int) -> JsHandle;
```

This kind of surface is a host interop layer, not a reason to make Pergyra
inheritance-driven.

## UI IR Track

Before a direct JS backend, prefer a shared UI/scene IR that can be consumed by
web, native, and mobile targets:

- `Window`
- `Scene`
- `Node`
- `Layout`
- `DrawCommand`
- `InputEvent`
- `ProjectionBinding`
- `DirtyScope`

In that model, `subject` is a projection source, not a DOM node. `zone` and
`world` own dirty-sync and lifecycle contracts.

## Decision

Direct JavaScript backend work is not a beta blocker. If dogfood evidence later
shows the C/Emscripten bridge is insufficient, the direct backend can be
reopened as beta+1 work without changing the core language surface.
