# Architecture Boundary Cores

Status: `mechanized-model-boundary`

These models tighten the design philosophy without turning it into a
whole-language proof. They divide responsibilities that must not share one
source of truth.

## Owners

| Model | Owns | Does not prove |
|---|---|---|
| `DelegationBoundaryCore.v` | `SourceDeclaration` owns attributed purpose/requested capability; separate `EnforcementEvidence` owns trusted authority, complete mediation, and delegability; the relation owns static and retained runtime permits. | Actual human purpose, moral legitimacy, consent, compiler correctness, or runtime adequacy. |
| `LossCompositionCore.v` | Cumulative loss vectors and the conditions for a compiler-derived mechanism. | That current passes have supplied observational-equivalence or cost evidence. |
| `ResourceMachineBridge.v` | Explicit binding between logical resource authority and physical machine placement. | Concrete contact semantics, board/MMU truth, or backend lowering. |
| `MachineLayerCore.v` | Declared regions and explicit machine contact under authority, lease, mode, and hardware-adequacy witnesses. | That the declared hardware map equals the live machine or that an abstract event equals a concrete instruction. |

## Required Chain

```text
attributed declaration
  + separately owned delegability
  + separately owned trusted authority evidence
  + separately owned complete mediation
  -> static permit OR retained checked runtime permit

resource authority
  + explicit resource-to-machine projection
  + machine placement/contact evidence
  -> grounded machine contact

per-boundary loss vectors
  -> composed path loss
  -> VerifiedProjectionPlan budget decision
```

No arrow may be reversed. A capability does not establish delegability. A
resource id does not establish an address. An address does not establish
authority. A local loss budget does not establish a path budget. A source
declaration does not establish actual human intent.

## Evidence Labels

The Coq/Rocq files are mechanized theorems about these models. Their registration
in `formal_semantics_smoke.sh` and `ProofSpine.v` is executable proof-pack
evidence. Live C/LLVM/self-host implementation adequacy still requires owner
facts, negative gates, differential parity, and physical residue checks.

The boundary is intentional:

```text
mechanized model soundness != implementation adequacy
implementation parity != justified human delegation
complete proof spine != whole-language verification
```
