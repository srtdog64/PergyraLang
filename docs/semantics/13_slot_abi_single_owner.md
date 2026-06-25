# 13. Slot ABI single-owner rule

Status: **ACTIVE.** Slot layout is no longer selected by build mode.

Companion to [[project_slot_safety_consistency]] and the ABI ownership smoke.

## 0. Rule

`Slot<T>` has one canonical physical owner:

```c
typedef struct {
    T value;
    bool occupied;
} PgySlot_T;
```

The `occupied` flag is part of the ABI. It is not a debug-only field, not a
release-mode alias, and not controlled by a whole-program macro. The runtime,
ABI spec, C backend, LLVM backend, and self-hosted verifier must all consume the
same checked layout.

## 1. Why raw same-name slots were retired

The former raw-slot design reused `PgySlot_*` names for a value-only layout.
That made one ABI name mean two layouts depending on compile flags. This is the
wrong failure mode: the source-level contract still says `Slot<T>`, but the
linked object may silently disagree about memory shape.

Pergyra's SoT rule forbids that. Build policy may change optimization, inlining,
and verifier strictness, but it must not change the physical meaning of an ABI
type name.

## 2. Future raw storage rule

If a value-only storage surface is reintroduced, it must be a distinct owner:

- distinct source surface or explicit unsafe/raw capability;
- distinct ABI type name;
- distinct MIR ABI row;
- distinct gate that proves no checked `Slot<T>` value crosses the boundary.

It must not be implemented as a macro that remaps `PgySlot_*`.

## 3. Gate

The ABI ownership smoke rejects same-name raw slot definitions and requires
`PgySlot_*` runtime sizes and field offsets to match `pgy_abi_slot_*`. This
prevents the old dual-layout path from returning as a compatibility branch.
