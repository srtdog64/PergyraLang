# Public `ArrayDrop` contract

`ArrayDrop(values)` consumes one named owned `Array<T>` binding and releases its
backing allocation. It does not release resources owned by elements. It returns
`Void`; an empty array is a valid owner too. Later reads, mutations, moves, and a
second drop of that binding are invalid.

The admitted element frontier is numeric/Boolean values and recursively plain
value records or enums whose fields/payloads have no independent resource
lifetime. Unknown/generic element types, nested containers, handles, and
identity-bearing subjects are refused. Owning string elements use
`ArrayDropOwnedStrings`, not storage-only drop.

Default/ref/inout parameters are borrows, not release authority. A named local
must originate from an array literal and retain exclusive storage provenance,
or an `own` parameter must transfer that authority explicitly. General call
results remain refused until return ownership has an explicit carrier. A live
`Slice` prevents invalidating its backing storage. Aliased/escaped descriptors
and unproved field/element extraction fail closed. An inout handoff or a borrowed
call returning a resource-bearing result also ends unproved exclusive storage
provenance; naming a temporary alone does not manufacture ownership.

A non-escaping read-only borrow does not consume storage. An `own` argument
cannot share its binding with another argument of the same call, including a
read-only borrow; callee ordering is not proof of independent release authority.

`CompilerRetireArrayStorage` remains private to its exact registered compiler
lifetime owners. Public release does not weaken that caller registry.

The semantic owner admits release; C/LLVM lowering only implements that admitted
operation. Native, source-C, and direct-MIR evidence retain their actual
frontiers even when the focused public-pair gate passes. This contract does not claim automatic array
scope cleanup or completion of aggregate-formal deep-element ownership.

The direct-MIR execution lane retains its existing array representation
frontier: Int/Bool literals and empty logical-record arrays, with explicit own
Int/Bool descriptor forwarding. A source-level plain element verdict does not
add missing MIR literal, ABI, enum-array, or resource-return capabilities.
Source-C and native C/LLVM evidence are consequently listed separately.

Focused verification:

```sh
bash tests/self_hosted/parity/public_array_drop_owner_smoke.sh
PGY_BIN=<current-native> PGY_SELF_DRIVER_BIN=<current-driver> \
  bash tests/self_hosted/parity/public_array_drop.sh
```

The inventory gate checks release ID 146 without changing ArraySlice ID 139.
The executable gate owns positive behavior, diagnostic-specific negative
refusals, and preservation of prior output artifacts on rejection. These are
supporting release owners reached by `semantic.hashmap_collection_ownership`,
not another top-level source of truth or evidence that its ACTIVE seam is CLOSED.
