# Unsafe Capability Scope Contract

`unsafe { ... }` is not a beta-stable permission to do every unsafe operation.
It is only the current lexical boundary marker. The stable direction is a
scoped unsafe capability model: an unsafe operation must name the capability it
needs, and the compiler must verify that the operation stays inside that
lexical scope.

## Rule

Unsafe is a scoped capability, not a mode bit.

Future raw/system-tier syntax must follow this shape:

```pergyra
unsafe(raw) {
    let p = SlotRawPointer(slot);
}

unsafe(ffi) {
    ExternUncheckedCall();
}
```

The exact spelling can still change before freeze, but the contract cannot:
plain `unsafe { ... }` must not become a universal escape hatch.

## Capability Buckets

The initial buckets are deliberately narrow:

| Capability | Allows | Does not allow |
|---|---|---|
| `raw` | raw pointer escape, pointer arithmetic, raw slot address access | unchecked FFI, thread races, authority bypass |
| `ffi` | unchecked foreign ABI calls and layout assumptions | raw Slot escape unless `raw` is also present |
| `layout` | representation punning, packed layout assumptions | pointer lifetime extension |
| `runtime` | runtime bypass hooks, low-level scheduler/runtime hooks | raw pointer escape by itself |
| `concurrency` | explicitly unsafe atomics/thread handoff surfaces | crossing `await`, `spawn`, or `parallel` without evidence |

If an operation needs more than one capability, the source must say so. For
example, an MMIO write through a raw pointer is both `raw` and `ffi`/`layout`
depending on the ABI boundary.

## Non-Transitive Boundary

An unsafe capability scope is lexical and non-transitive:

- it does not automatically apply to called functions;
- it does not cross `await`, `spawn`, `parallel`, or channel/world handoff;
- it does not disable `Result`/panic contracts;
- it does not bypass Slot generation/token validation;
- it must appear as AIR evidence so LSP, CI, and audits can answer why the
  unsafe operation was accepted.

## Diagnostics

Diagnostics should request the missing capability, not generic unsafety:

- Bad: `use unsafe`
- Good: `operation requires unsafe(raw)`
- Good: `raw pointer escaped unsafe(raw) scope`
- Good: `unsafe(concurrency) cannot cross spawn boundary`

This preserves Pergyra's layered safety model. Static verification rejects
unsafe transitions that are visible at compile time; runtime validation still
guards dynamic handles, generations, tokens, and authority state.

## Current Beta State

Today:

- `unsafe { ... }` parses, type-checks, lowers, and participates in CFG/HIR/AIR
  boundary traversal.
- `SlotRawPointer(...)` rejects with `PGY_SEM_RAW_ESCAPE_UNSTABLE`.
- Pin/Lease views remain the stable hot-path answer for repeated Slot access.

Before raw/system-tier escape is accepted, the compiler must add scoped
capability syntax, semantic gates, AIR evidence, ABI lowering rules, backend
parity tests, and diagnostics.
