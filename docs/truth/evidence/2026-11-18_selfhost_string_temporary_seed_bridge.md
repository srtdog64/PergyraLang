# String temporary retirement: seed bridge

Observed 2026-11-18 KST on parent `9049e547e`, with the changes carried by
this commit. D: is the editing authority; WSL `/root/mg-main` is a mirrored
validation tree, not another published Git revision.

## Scope and artifact identity

The MIR-layer temporary plan grants a release only to a fresh, independent
String operand after a synchronous, nonretaining builtin call. Canonical
identity, concrete types, the sealed retention row and single use are
required. The C materializer consumes the checked plan and preserves
evaluation order, receiver handling, short circuiting and the call result.
The result-origin contracts live in the existing call hand-off registry.

This is the decision-authorized temporary AST-consuming seed step. It is
not full S1, persisted `owned_value_facts`/`value-retire`, an OSSA verifier,
native cleanup, array cleanup or cleanup of locals/user-call results. The
direct backend's sharing guard and its `alias_read_after` falsifier remain.

SHA-256 identities (normalized production `.pgy` map, 2,840 files):

| Artifact | SHA-256 |
|---|---|
| Production source map | `759fd4442d601ad461dd94129bf83e04495f76ee4334153a4b1bb435f7056bbb` |
| Native seed | `97f33eab89e7e264ea37c6f9239b886e4129e639e3dc337c556e205216443c48` |
| Pergyra-built codegen | `42a3051588240eb65fed61939491627994b521a760c340a12c687952ba55eb10` |
| Installed driver | `de016bb78169a2146855133a38059137b1c6d8f2ffd119ea57b750c179e40527` |
| Installed driver C | `7fbfa8eb24ae016bdd4a7e3854e650fcba0c549bf18fac9c104609f88392c1c0` |
| Codegen gen2 C == gen3 C, 124,083 lines | `4b834fcb0f092848ef2d3a434dfdf190d2ca36f3129df4686406b91830a1737d` |

## Executed local checks

Logs live under `.tmp/selfhost-memory-20261117/` in the validation tree.

- `string-retire-bootstrap-final.log`, `string-retire-installed-build.log`:
  actual fixed point and installed-driver build passed. The native build
  reported zero errors and 17 existing unreachable-statement warnings.
- `string-retire-gate-installed-generation.log`: the new gate passed with
  the Pergyra-built AST emitter, without direct-MIR route substitution.
  Receipt: `.tmp/self_hosted/string-retirement.LZeZZ9/receipt.json`.
  The 4,096-iteration fixture has equal native/Pergyra runtime output and
  zero ASan/LSan findings at `-O0`. Borrowed/static values and user-retained
  results are read after calls with ASan enabled; their unproved roots are
  explicitly outside the leak-freedom claim.
- Plan mutations reject a missing, duplicate, relocated or wrong-recipe
  retire, a declared namesake, multiple use and an unsealed retention row.
  Five unchecked C-artifact mutations actually expose a leak, double free,
  premature free, static free and hand-off use-after-free. These runtime
  falsifiers are distinct from the plan checks. A generated-name collision
  probe preserves the later operand's value.
- `string-retire-registry-final.log`: C/Pergyra hand-off decisions and fresh
  String result queries agree; malformed result rows are refused.
- `string-retire-component-final.log`, `string-retire-keywords-final.log`:
  component contracts, unchanged existing caps and generator drift passed.
  The call-order owner is 178 code lines under its existing 200-line cap.
- `string-retire-roles.log`: installed C/LLVM role behavior and 46 refusal
  pairs passed. `string-retire-allocation-final.log`: all 22 allocation/OOM
  and unchecked-memory controls passed.
- `string-retire-nominal.log`: six executions and eight pre-emission
  refusals passed. The optional qualified-kind and snapshot observations
  are recorded separately below; they are not silently counted as green.

## Complete fixed memory inputs

`string-retire-memory.log`, work directory
`.tmp/self_hosted/selfhost-memory.FWUOMj`: all five owned-process receipts
were checked; no address-space, timeout, input or RSS allowance was raised.

| Input/path | Peak KiB | Existing ceiling KiB | Result |
|---|---:|---:|---|
| Complete semantic MIR consumer | 777,420 | 819,200 | PASS |
| Complete MIR-lower source-to-C | 5,248,516 | 8,388,608 | PASS |

Source-to-C and source-to-MIR-to-C emit equal C bytes, SHA-256
`0981cafd21631c81fdba22086e47724d5be953672dbc1da7e19ea04902d3b505`.
These are the current production inputs, not a frozen-source speedup claim.

**Full DRV-2 remains red.** `string-retire-full-drv2.{log,time,receipt.json}`
records the complete one-process source-to-C compile under unchanged
14 GiB `RLIMIT_AS` / 1,800 seconds. It reaches 14,072,968 KiB RSS and exits
134 with an explicit allocation-failure panic; the process group is drained.
No completed C artifact or 16 GB success is claimed. The main MIR lifetime
and array work remains necessary.

## Optional observations and remaining boundary

The optional `collection_borrowed_descriptor_read.sh` stops at three pre-existing
source caps (125/115, 414/410, 47/45); those production owners and that gate
were unchanged by this slice. Its result is red, not a semantic verdict.
The qualified-kind runtime probe fails both here and against the archived
parent's Pergyra source; its last manual graph mutation does not change
the copied graph. The unique-use snapshot probe fails an unchanged `let`
counter reassignment at compile time. These remain red, not acceptance
evidence for this bridge. The local-ref snapshot compiles and runs with
`MIR LOCAL REF SNAPSHOT PASS`.

Compiler-selected Clone transport still needs an executable reconciliation:
the separate ledger contains selected rows, while expression-node JSON does
not carry the graph overlay. This bridge does not grant freshness from that
overlay. Local checks above are not exact-SHA CI; publication belongs to
[main state](../state/main.md).
