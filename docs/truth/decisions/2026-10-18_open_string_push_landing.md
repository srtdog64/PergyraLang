# OPEN 2026-10-18: copy or move a String the proof route cannot alias

Asked by: the Claude lane. Waiting for the user.

## The question

The user chose GPT's proof route for String elements over "copy wherever a
proof fails". The Claude lane has since proved the cases a proof can
settle: a String passed to a declared callee that only reads it (330 of
the 767 canonical sites). What remains cannot be settled by any proof: a
String element of a local Array goes into a value that outlives that
Array. The first CI step's manifest program has this shape:

```
let entries: Array<String> = DirWalk(root);
let rows: Array<String> = [];
... ArrayPush(rows, entries[i]); ...
return rows;          // entries dies here; rows keeps its elements
```

GPT's plan (`docs/agent_work_directives/indexed_string_array_push_ownership_chain_2026-10-18.md`,
untracked in `D:\PergyraLang`) asks for one compiler-owned contract per
such site. It names two: copy, or move.

| | Copy landing | Move landing |
|---|---|---|
| What happens | the push stores an owned copy (`ArrayPushOwnedString`) | the element leaves `entries`; its slot is cleared so `entries`' release skips it |
| Proof needed | none beyond the site | the moved slot is never read again |
| Runtime | exists on C and LLVM | new slot-take operation on both backends |
| Cost | one allocation per pushed String | none |
| Status | built and tested on `claude/p0-push-copy-landing` | not started |

Canonical sites of this kind: 249 `ArrayPush`, 124 constructor fields,
46 plain stores, 19 `ArraySet` (census at `2902cf449`).

## Recommendation

Copy for pushes now (the runtime and both backends exist; MIR shows the
selected `ArrayPushOwnedString`, so the choice stays inspectable), and
the same contract for constructor fields and stores. Move can replace a
copy later where the allocation shows up in a measurement.

## What changes if copy is chosen

`array_result_local_owned_element_container_retention_invalid` (push
`rows[0]` into `copies`, deep-drop `rows`, read `copies[0]`) is refused
today as a use-after-free. Under a copy it is safe and is admitted.
