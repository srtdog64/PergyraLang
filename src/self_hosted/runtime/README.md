# Runtime Track

This directory is the runtime self-host owner surface, not a copy of C-side
`src/runtime/`. Runtime self-hosting is split into two different claims.

## Native Kernel Boundary

The native runtime kernel remains C. It owns the pieces that cannot be safely
implemented by generated Pergyra code without creating a bootstrap cycle:

- allocator entry points and ABI layout;
- OS file/process/time/thread primitives;
- mutex, condvar, atomics, and scheduler state;
- panic/exit ABI exports;
- Slot/SecureSlot/Pin handle tables and synchronization;
- C/LLVM-linkable runtime export symbols.

Replacing this layer in Pergyra would mean the generated program depends on a
runtime that itself needs the generated runtime to execute. That is not a valid
first hard-self-host step.

## Portable Policy Boundary

Portable runtime policy can move to Pergyra. Good candidates are pure or
deterministic layers that can run beside the C implementation and compare
outputs:

- runtime contract checkers;
- deterministic path/string/manifest policy;
- JSON/schema validators;
- portable collection algorithms that only consume stable runtime primitives;
- generated-runtime inventory and ABI-shape auditors.

Those are self-host work packages because they prove Pergyra can own runtime
contracts without replacing the native kernel.

## Counting Rule

`src/runtime/` remains 0% compiler-internal substitution until a Pergyra-written
runtime component is linked into generated programs as part of the runtime
surface. Runtime-adjacent Pergyra tools count as soft self-host evidence, not a
full runtime replacement.
