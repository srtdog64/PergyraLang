# LLVM Pointer Parameter Contracts

Status: `carriage projection landed; pointer-attribute projection withdrawn`.

Pergyra keeps source ownership and parameter carriage first-class through MIR,
but those facts do not by themselves prove LLVM pointer contracts.

## Closed distinction

`MIRParamCarriageRow` owns physical call representation:

- value
- readonly reference
- value-result
- owner handle
- direct or indirect passage

LLVM attributes such as `noalias`, `readonly`, `captures`, `nofree`, `nonnull`,
and `dereferenceable` belong to a separate proof surface. They describe every
way the pointer and its reachable storage can be observed, not only the source
parameter spelling.

For example, an owner handle does not prove that no global, callback, interior
pointer, runtime registry, or concurrent task can reach the same storage. A
read-only parameter proves that this source binding cannot write through the
borrow; it does not automatically prove LLVM's whole-call memory contract.

## Current rule

`src/codegen/llvm_decl.c` consumes MIR carriage for ABI shape only. It emits no
pointer parameter optimizer attribute from carriage. Unknown proof means no
attribute.

C/LLVM output parity is not a soundness proof for LLVM attributes. The C
backend does not assert the same LLVM undefined-behavior contract, and an
unsound attribute may remain dormant outside a triggering optimization.

## Re-entry gate

Pointer attributes may return only after a MIR-owned pointer-contract row has:

- one field per independent promise;
- a proof source and boundary identity;
- a verifier that rejects missing or contradictory evidence;
- O0/O2/O3/LTO hidden-alias and capture regressions;
- optimized-IR inspection or bounded refinement validation.

The old carriage-to-`noalias`/`readonly` mapping and its benchmark remain useful
as performance hypotheses, not as accepted language guarantees.
