# Member-receiver collection mutation counterexample

Revision: `c71ea7fc81e35b98d9760180a097772a2d9d6a77` (published main/origin/main).
Status: the bounded direct-MIR scalar CFG program route does not admit a
collection mutation whose receiver is a logical-record member. The compiler
source now stays inside the admitted surface; the route itself is unchanged.
This audit is evidence, not an ABI owner, completion score or implementation plan.

## Observed refusal

`self_host_parity.yml` run `34661105326` on exact `c71ea7fc`, step
`Build the release pair and run exhaustive self-host parity`:

```text
[self-host-parity:assignment-projection] backend=llvm compile failed
CODEGEN ERROR: direct MIR scalar CFG program routine admission stage is invalid:
stage=statement ordinal=1886 block=4 row=22668 source=AST_CALL
```

The same row reproduces locally, byte-identical. Global instruction row 22668 is
`CodegenGenericSpecializationInstantiateCalls` block 4 instruction 0 in
`src/self_hosted/codegen/input/generic_specialization_codegen_view_owner.pgy`:

```pergyra
ArrayPush(templates.syntax_ids, signatures.function_node_ids[template]);
```

The C leg compiles, runs and matches expected output before the LLVM leg starts.
The refusal is LLVM-route admission only, not a semantic or producer defect.

## Reduced counterexample

`.tmp/probe/mpush.pgy`, fourteen lines, no compiler owner imported:

```pergyra
struct Bag { let xs: Array<Int>; let ys: Array<Int>; }
func BagEmpty() -> Bag { let ints: Array<Int> = []; let other: Array<Int> = []; return Bag(ints, other); }
func Main() -> Void {
    let bag: Bag = BagEmpty();
    let i: Int = 0;
    while i < 3 { ArrayPush(bag.xs, i); ArrayPush(bag.ys, i * 2); i = i + 1; }
    Log(ToString(ArrayLength(bag.xs)));
    Log(ToString(bag.ys[2]));
}
```

`--emit-source-llvm-ir-verified` refuses it at `ordinal=0 block=2 row=7`. The same
program with two plain `Array<Int>` locals pushed and then handed to `Bag(...)`
emits a 7,160-byte artifact. The difference is the receiver spelling alone.

## Why the route cannot resolve the member today

The producer projects one instruction-primary LocalRef per collection statement
and that ref names the base binding only. All three pushes into distinct fields
of one record share `declaration:45687:0`, so the member identity is absent from
the wire:

```text
22668 local_ref=declaration:45687:0 ArrayPush(templates.syntax_ids, ...)
22669 local_ref=declaration:45687:0 ArrayPush(templates.generic_starts, ...)
22670 local_ref=declaration:45687:0 ArrayPush(templates.generic_counts, ...)
```

`DirectMirScalarProgramArrayMutationTargetFromOwners` therefore resolves the base
local, reads its type `GenericInstanceTemplates`, matches no admitted array type
and refuses. `expr0_graph` carries the pushed value and `expr1_graph` carries the
`ArraySet` index, so neither slot holds the receiver. Recovering the member from
`expr0` text is forbidden.

The adjacent `record.field[i] = value` rung already solves the same problem for
assignments: it reads the target graph from `expr1_graph`, resolves the member
with `DirectMirScalarProgramLogicalRecordMemberPathFromGraphRoot`, and keeps the
target address in `secondary_expression_rows`. A future rung for push/set needs
the equivalent producer-owned receiver fact, then one GEP in
`DirectMirScalarProgramLlvmArrayMutation`; the runtime helpers already take a
`ptr`.

## Decision on this revision

The route is bounded `SUBSTITUTING` by construction, and the compiler source is
expected to stay inside it. Of 713 collection statements the probe reaches, 701
already push into plain locals; the 12 outliers were all in one routine, while
`CodegenGenericSpecializationFactsFromSemantic` twenty lines above it already
uses the plain-local idiom. That routine now accumulates plain locals and
constructs `GenericInstanceTemplates` and `CodegenGenericSpecializationFacts`
once, which is the spelling its own file already used.

The frontier stays open: `src/self_hosted/` still holds member-receiver
collection mutations outside this probe's closure, and the next probe that
reaches one will refuse at the same owner.
