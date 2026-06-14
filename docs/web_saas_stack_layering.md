# Web/SaaS Stack Layering (Design Note)

Status: design direction. Records where the missing SaaS pieces (data
persistence, auth/session, networking, build/deploy) belong relative to the
language, and the Pergyra-specific principle that keeps them native rather than
bolted on.

## Context

The WASM dashboard (examples/reactive_dom_demo/web) showed the two SaaS pieces
that already work: business logic compiled to WASM (dash.wasm, 428 bytes,
computes every KPI) and reactive UI state (zone/slot/projection, rung-1). The
remaining pieces are data persistence, auth/session/multi-user state,
networking/HTTP, and the build/deploy pipeline.

These are not all flat libraries. They split across three tiers, and because
Pergyra has a capability and domain model, some belong partly in the language,
not in a library.

## Three tiers

Tier A, toolchain (pgy itself, not a library). Build, bundle, dev server, wasm
packaging, route map generation. This is the compiler and CLI growing a web
target, the way wasm-pack or go build are not libraries. A target such as
`pgy build --target web` would emit the wasm bundle, an HTML shell, and a route
map. Only the runtime side of routing is a library or a language pattern.

Tier B, capability-bound host bindings (libraries the language governs). HTTP
and fetch, DB and storage, sockets. These are I/O and cross the capability
boundary. They are libraries, one set per target (browser wasm uses fetch,
native uses sockets), and access to them is gated by the language's authority
and scoped-unsafe system. They plug in through runtime profiles (capability 10):
runtime-none is pure compute with no I/O (dash.wasm is exactly this),
runtime-default is full, and a future runtime-web would carry fetch and DOM.
Runtime profiles are the architecturally correct home for I/O libraries.

Tier C, domain-shaped with library mechanism. Auth, identity, session, and
multi-user state. The shape already lives in the language: authority, party,
role, and capability are identity, capability, and access control, and shared
multi-user state is an extension of zone and world to distributed or shared
reactive state. A library supplies only the mechanism, token crypto and session
store bindings. Auth is therefore not pulled out wholesale; the language gives
the shape and the library gives the transport and crypto.

## The Pergyra-specific principle

I/O libraries should be capability and slot shaped, not generic FFI. A database
connection is a slot held under an authority; an HTTP client is a capability
that is granted. Two consequences follow.

The library feels native to the language rather than bolted on.

The security story comes for free: capability-bound I/O means the language
bounds what generated code may touch. This is a differentiator, and it matters
more in an era where code is AI-authored.

Exposing fetch() as plain FFI would make Pergyra look like every other language
at the I/O layer. Shaping I/O as capabilities pushes the language's identity all
the way down into the I/O layer.

## Summary table

| Piece | Home | Form |
|-------|------|------|
| build, bundle, deploy | toolchain (pgy) | not a library |
| HTTP, DB, sockets | library + runtime profile | capability-shaped, language-gated |
| routing | library or pattern | intent/zone based |
| auth, session, multi-user | language shape + library mechanism | authority/role/zone plus crypto lib |

## Principle to hold

Keep the pure core language unaware of I/O (runtime-none is the proof) and make
every I/O path pass through a capability gate. That keeps the core clean, keeps
the libraries native, and turns the capability model into the SaaS security
story rather than an afterthought.
