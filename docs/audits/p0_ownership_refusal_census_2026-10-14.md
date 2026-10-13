# P0 ownership refusal census (2026-10-14)

Status: `AUDIT COMPLETE`. This is an observation only and owns no semantics.
Base revision: `60b9d62bc` (origin/main), WSL Ubuntu, native `bin/pgy`.
Lane: Claude, investigation and verification
(`agent_work_directives/v1_scope_ownership_and_selfhost_generality_2026-10-14.md`).

## Question

How many ownership refusals stand between main and a green bootstrap? The
bootstrap stops at the first one: gen1 fails at
`bundle_verdict:borrow_boundary_escape:node=107421`
(`unproved_exclusive_member_mutation`, `Array<String>`). One first error is not
the remaining set.

## Method

1. Build gen0 from unchanged main:
   `pgy src/self_hosted/codegen/main.pgy --native-pipeline --backend=c`.
   Run it on the codegen source. It reproduces the CI failure exactly: node
   107421, same code and boundary.
2. **Find the reached owner.** The body fact rows (initializer, iteration,
   assignment and statement facts) all verify. The refusal comes later in the
   bundle assembly, from
   `SemanticAstCollectionOwnershipScanIntoVerdict`
   (`src/self_hosted/semantic/ast_collection_ownership_scan_owner.pgy`). That
   scan walks every expression root in order and returns at its first refusal.
3. **Instrument a scratch copy of that owner** (script below; never committed).
   - Each of its 26 `verdict = ...; return;` refusal sites now logs the
     refusal's code, node, expression node and message, then continues as if
     the site had passed.
   - The walk-stack `ArrayDrop`s just before such a refusal are removed.
   - The verdict still fails at the end if anything was logged.
   - gen0 is rebuilt from the patched owner. The owner file is then restored,
     so the input sources are byte-identical to main and node ids match the
     unpatched run.
4. Run the instrumented gen0 with unbuffered stdout on two inputs:
   - `src/self_hosted/codegen/main.pgy`, the gen1 input;
   - `src/self_hosted/compiler/driver_bootstrap_main.pgy`, the installed-driver
     input.

## Result

| Input | Refusals | Distinct nodes | Scan sites reached |
|---|---:|---:|---:|
| `codegen/main.pgy` (gen1) | 9 | 9 | 3 |
| `compiler/driver_bootstrap_main.pgy` (installed driver) | 97 | 93 | 9 |

The driver count matches the historical 97 named in
`compiler_closure_priorities_2026-10-14.md`. It is now measured on the current
main rather than carried forward.

Driver input by scan site and boundary:

| Count | Site | Boundary | Type |
|---:|---:|---|---|
| 27 | 12 | `unproved_exclusive_sink_entry` | nominal aggregate |
| 21 | 13 | `unproved_indexed_read_entry` | `Array<String>` |
| 9 | 10 | `owned_string_actual_unproved` | String |
| 9 | 13 | `unproved_formal_indexed_read_entry` | `Array<String>` |
| 7 | 1 | `unproved_exclusive_member_mutation` | `Array<String>` |
| 7 | 13 | `unproved_formal_element_use_entry` | `Array<String>` |
| 4 | 2 | `ArrayPush` (statement transition) | `Array<String>` |
| 4 | 13 | `unproved_shallow_mutation_entry` | `Array<String>` |
| 4 | 16 | `owned_string_drop` | borrowed or unknown `Array<String>` |
| 2 | 6 | `owned_element_escape` | String |
| 1 | 13 | `unproved_inout_copy_entry` | `Array<String>` |
| 1 | 24 | `aggregate_release_element_borrow` | String |
| 1 | 25 | `aggregate_release_incomplete` | `Array<String>` |

All 97 have code `borrow_boundary_escape`.

Each scan site is a numbered refusal point, in source order inside
`SemanticAstCollectionOwnershipScanIntoVerdict`. This is the check each site
runs:

| Site | Check |
|---:|---|
| 1 | `SemanticAstCollectionExclusiveStatementRootReady` |
| 2 | `SemanticAstCollectionStatementTransitionForSyntax` |
| 6 | `SemanticAstCollectionOwnedElementEscapeAtEvent` |
| 10 | `SemanticAstOwnedStringLiteralTransferCallArgumentVerdict` |
| 12 | `SemanticAstCollectionExclusiveArgumentBoundary` |
| 13 | `SemanticAstCollectionCallArgumentVerdict` |
| 16 | `SemanticAstCollectionBuiltinTransitionAtCompletion` |
| 24 | aggregate release element borrow |
| 25 | `SemanticAstCollectionAggregateReleasePlanFinalized` |

Site 13, the call-argument verdict, accounts for 42 of the 97 through its five
entry boundaries. Site 12, the exclusive argument boundary, accounts for 27.

The codegen input (gen1) has nine refusals:
- 6 × `unproved_exclusive_sink_entry`;
- 2 × `owned_element_escape`;
- 1 × `unproved_exclusive_member_mutation` (node 107421, the published CI
  failure).

The next refusal after 107421 is `unproved_exclusive_sink_entry` at node
107489.

## How to read it

- **Upper bound per site.** After a skipped refusal the scan keeps going on
  state the real owner never reaches, so some later rows may be cascades of
  earlier ones. The list of reached refusal kinds is exact. The per-kind counts
  are upper bounds.
- **Only this owner.** The bundle assembly runs more checks after the
  collection ownership verdict. Those were not reached, because the verdict
  still fails. A fix of all 97 can expose refusals from later owners, so this
  is the remaining set of the reached owner, not of the whole chain.
- **The gate order matters.** gen1 must pass the codegen input (9) before the
  driver input (97) is even compiled by a self-host generation.
- Node ids are artifact syntax ids of the stated input at `60b9d62bc`. They
  shift with any source edit before them. Map them with the P0 lane's existing
  node locator.

## Node lists (syntax node / expression node; -1 = statement-level)

### codegen/main.pgy

- site 12, `unproved_exclusive_sink_entry` (nominal aggregate), 6: 107489/366372, 120250/415532, 120259/415549, 127236/445301, 127360/445584, 127380/445653
- site 6, `owned_element_escape` (String), 2: 119645/413308, 119647/413314
- site 1, `unproved_exclusive_member_mutation` (Array<String>), 1: 107421/-1

### compiler/driver_bootstrap_main.pgy

- site 12, `unproved_exclusive_sink_entry` (nominal aggregate), 27: 114476/396705, 127237/445865, 127246/445882, 127389/446146, 127472/446378, 134998/477798, 135033/477966, 153150/521150, 154248/525602, 188566/682482, 188589/682618, 188594/682666, 188599/682688, 188602/682711, 188605/682724, 196373/718691, 196373/718691, 204633/757991, 204633/757991, 222903/844041, 223028/844334, 223055/844400, 223098/844562, 244429/980419, 246591/983378, 246613/983454, 247702/986893
- site 13, `unproved_indexed_read_entry` (Array<String>), 21: 154729/527194, 157704/545006, 168073/592336, 185065/667580, 187563/678669, 187583/678741, 187645/678918, 187679/679015, 187707/679076, 187775/679236, 187981/679896, 188025/679992, 188038/680038, 188044/680059, 188050/680082, 212714/794508, 212716/794532, 212740/794595, 212843/795024, 215493/805760, 215837/807129
- site 10, `owned_string_actual_unproved` (String), 9: 158274/547702, 158290/547740, 158321/547853, 158339/547913, 158633/549609, 158637/549651, 164171/579352, 222784/843776, 223133/844775
- site 13, `unproved_formal_indexed_read_entry` (Array<String>), 9: 159753/553683, 184789/666556, 184803/666618, 184815/666711, 184819/666750, 211130/788650, 224173/849000, 224173/849000, 224183/849073
- site 1, `unproved_exclusive_member_mutation` (Array<String>), 7: 114408/-1, 152727/-1, 152731/-1, 152739/-1, 152743/-1, 153178/-1, 153182/-1
- site 13, `unproved_formal_element_use_entry` (Array<String>), 7: 197572/723245, 197590/723344, 197634/723570, 197666/723744, 212866/795098, 215674/806548, 215715/806705
- site 2, `ArrayPush` (Array<String>), 4: 130906/-1, 212711/-1, 215485/-1, 215834/-1
- site 13, `unproved_shallow_mutation_entry` (Array<String>), 4: 153576/522892, 153576/522892, 188131/680307, 197520/722884
- site 16, `owned_string_drop` (borrowed or unknown Array<String>), 4: 156129/536364, 220663/832957, 222326/842069, 222363/842306
- site 6, `owned_element_escape` (String), 2: 126632/443641, 126634/443647
- site 13, `unproved_inout_copy_entry` (Array<String>), 1: 157702/544984
- site 24, `aggregate_release_element_borrow` (String), 1: 223029/-1
- site 25, `aggregate_release_incomplete` (Array<String>), 1: 222980/-1

## Reproduce

These are scratch steps on a clone at the base revision. They never touch the
shared tree.

1. Build `bin/pgy`.
2. Apply the transformation below to
   `src/self_hosted/semantic/ast_collection_ownership_scan_owner.pgy`.
3. Build gen0, then `git checkout` the owner file.
4. Run `stdbuf -o0 gen0 --source <input>`.
5. Collect the `[p0-census]` lines.

Each run takes about 190 s on the codegen input and about 715 s on the driver
input.

```python
import re, sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
fn = s.index("func SemanticAstCollectionOwnershipScanIntoVerdict(")
b0 = s.index("{\n", s.index(") -> Void", fn)) + 2
ok = s.index("    verdict = SemanticAstCollectionOwnershipVerdictOkWithFacts(", b0)
head, body, tail = s[:b0], s[b0:ok], s[ok:]
body = re.sub(r'ArrayDrop\(pending\);\s*ArrayDrop\(prefixes\);\s*(?=verdict\s*=)', '', body)
n = [0]
def repl(m):
    n[0] += 1; k = n[0]; v = f"p0v_{k}"
    return (f"let {v}: SemanticAstCollectionOwnershipVerdict = {m.group(1)};\n"
            f'Log(Concat("[p0-census] site={k} code=", Concat({v}.diagnostic_code, Concat(" node=", '
            f'Concat(ToString({v}.syntax_node_id), Concat(" expr=", Concat(ToString({v}.expression_node_id), '
            f'Concat(" root_slot=", Concat(ToString(root_slot), Concat(" msg=", {v}.diagnostic_message))))))))));\n'
            f"p0_census_count = p0_census_count + 1;\n"
            f"if p0_first_node < 0 {{ p0_first_node = {v}.syntax_node_id; }}")
body = re.sub(r'verdict\s*=\s*(.+?);\s*return;', repl, body, flags=re.S)
anchor = "    let local_count: Int = ArrayLength(locals.node_ids);\n"
body = body.replace(anchor, "    let p0_census_count: Int = 0;\n    let p0_first_node: Int = -1;\n" + anchor)
end = ('    if p0_census_count > 0 {\n        verdict = SemanticAstCollectionOwnershipVerdictError('
       '"borrow_boundary_escape", Concat("p0-census refusals=", ToString(p0_census_count)), '
       'p0_first_node, -1);\n        return;\n    }\n')
open(p, "w", encoding="utf-8", newline="\n").write(head + body + end + tail)
```
