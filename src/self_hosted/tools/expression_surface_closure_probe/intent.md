# Expression Surface Closure Probe -- Intent / Contract

**Status:** focused executable closure proof.

## Intent

Prove that parser-owned expression topology and semantic overlays are the only
supported expression facts consumed by semantic admission. The probe executes
the parser graph contracts, serialized parser boundary, typed-binding and
surface contracts, and the fail-closed type contracts for unsupported nodes,
nominal call comparisons, and payload-enum tag comparisons.

## Input Contract

The probe has no external input. `main.pgy` imports the production parser and
semantic contract owners directly. It must not reconstruct an expression from
payload text or import a codegen text-rewrite path.

## Output Contract

Success emits exactly:

```
expression-surface-closure=parser-owned-fail-closed
```

Any failed owner contract emits one owned diagnostic line and exits nonzero.

## Oracle

`tests/self_hosted/parity/expression_surface_closure_probe_parity.sh` compiles and runs
the probe through C and LLVM, compares their output artifacts, rejects an
unowned graph type, and ratchets every retired expression text-rewrite owner.

## Not In Scope

- Adding new expression syntax or backend lowering.
- Treating parser acceptance alone as semantic support.
- Reopening compact payload text as a compatibility fallback.
