# Bug: outer-variable call-assignment inside `if` is dropped when followed by `let` then another `if`

Status: open. Found by dogfooding the self-hosted AIR graph consumer tools
(`air_graph_node_count_integrity`, `air_graph_ref_live`): a `findings` string
assigned inside an `if` came out empty even though the branch ran.

## Symptom

An assignment to an *outer-scope* variable performed inside an `if` block, where
the right-hand side is a **function call**, is silently lost when that `if` block
is followed by a `let` declaration and then another `if` block. The value reverts
to the variable's initial value.

## Minimal repro

```pergyra
func Make() -> String { return "X"; }

func Main() -> Void {
    let f: String = "";
    let cond: Bool = true;
    if cond {
        f = Make();        // call-result assigned to outer `f`
    }
    let lit2: String = "t";  // (2) intervening `let`
    if cond {
        lit2 = "u";          // (3) a second `if`
    }
    Log(Concat(Concat("f=", f), Concat(" lit2=", lit2)));
}
```

Observed: `f= lit2=u` (f is empty, the wrong result).
Expected: `f=X lit2=u`.

The second `if`'s assignment (`lit2 = "u"`) survives; the first `if`'s
call-assignment (`f = Make()`) is the one dropped.

## Trigger is the conjunction of three conditions

All three are required; removing any one makes it pass:

- The first `if` assigns a **function-call result** to an outer variable.
  (A literal first assignment, e.g. `f = "b"`, passes -- see variant C.)
- A `let` declaration sits **between** the two `if` blocks.
  (No intervening `let` passes -- see variant F.)
- A **second `if`** follows that `let`.
  (No second `if` passes -- see variant A.)

Passing variants confirmed:

- A: `if { f = Make(); } let other; <use f>` (no second if) -> `f=X`. OK.
- C: `if { f = "b"; } if { g = "d"; }` (literal first, no intervening let) -> OK.
- D: distinct conditions, both literal -> OK.
- E: single `if` assigning two vars -> OK.
- F: `if { f = Make(); } if { g = "d"; }` (no intervening let) -> `f=X`. OK.
- G: single `if { f = Make(); }` -> `f=X`. OK.

Failing:

- B (the repro above): all three conditions present -> `f` dropped.

The first three (A, C–E, F, G) all compile and run correctly; only B drops the
assignment. So this is not a parser issue -- it is a lowering/SSA-or-block-scope
defect specific to threading a call-result write out of the first `if` when an
intervening `let` plus a later `if` reshape the CFG.

## Impact

Common shape. Any builder that does `if cond { x = make_thing(); } ... if cond {
y = ...; }` is exposed. In the AIR consumer tools it silently emptied the
`findings[]` array on the negative path while `ok:false` and the exit code stayed
correct, so the failure was invisible except by inspecting the JSON.

## Workaround (verified)

Compute the conditional value with an unconditional assignment from a helper
whose body does the branch as a `return`, so there is no `if { outer = call() }`:

```pergyra
func FindingsFor(cond: Bool) -> String {
    if cond {
        return Make();
    }
    return "";
}
// ...
let f: String = FindingsFor(cond);   // unconditional; no in-if outer assignment
```

Verified: this prints `f=X lit2=u`. The two self-hosted tools above were switched
to this form.

## Likely area

Codegen / MIR lowering of consecutive `if` blocks separated by a `let`, around
the writeback or phi of a value defined by a call inside the first block.
Probably adjacent to the active block-scope work in the semantic checker and the
MIR CFG lowering.
