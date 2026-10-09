# Ownership cutover: second checkpoint baseline

Checkpoint: `5651c916c87e030cc3ef579180abdfbf1814d1cc`. All other writers
stopped per the latest user confirmation. Thirty text inputs committed locally;
untracked `gmon.out` preserved. Native Git tracked/index diff zero before and
after the gates; no production source or official binary changed during them.

Logs: `.tmp/ownership-cutover-2026-10-09/p0-5651c916-7aeb6d7b3bc44252ba1e80bf2027449b/`.

| Executed boundary | Observed result |
|---|---|
| Full formal semantics | PASS: 69 proof files, 77 compiled modules plus approval consumer; only two approved Slot abstractions |
| Native lexer/parser/semantic/transpile/memory-layout/HIR/DIR/RIR/AIR/MIR units | All ten PASS |
| Document, boundary, SoT, protocol, progress, profile, evidence lifetime, source inventory, gate reachability | PASS; profile initially lacked Git in the child PATH, corrected run PASS |
| Windows pressure contract | PASS: 21 executable cases and synthetic identity controls |
| Native clock/scalar ABI on C/LLVM | PASS |
| Likeness | RED: sentinel 73 > 20 |
| Component inventory | INCOMPLETE: 60 s timeout after its checker self-test passed |
| Native MIR integration | RED: reaches match-binding gate, refuses missing same-source isolated self-host driver |
| Complete driver source-to-MIR under 3072 MiB | RED: receipt exit 88, 131209 ms, sampled private 3114.3 MiB (3.041 GiB) |

Full-driver executable SHA-256:
`8b26d1d82cf94481ff76a26eaa83b151e6f38c4f3b2099dc4c5c7e17d24af754`.
The pressure receipt binds all 10,559 tracked files and actual command/probe;
all before/after hashes unchanged, capture complete. These endpoint hashes are
not continuous immutability; sampled OS private bytes are not heap counters.
Peak-live/cumulative heap counters remain UNMEASURED.

An initial scratch invocation lacked cwd; its corrected invocation ran.
The scratch list also misspelled the evidence-lifetime filename; the actual
existing gate was then run separately and passed. Both failed launch logs are
retained. Neither was treated as a semantic failure or hidden in a PASS.

This is an executed RED baseline for the reached gates, not the complete P0
platform/installed/fixed-point/sanitizer matrix or current remote CI evidence.
The missing same-source driver and memory refusal block those dependent routes;
the stale official installed pair is not a substitute. Next: migrate the already
falsified value-admission/memo repair behind the existing resource-flow owner,
then rerun the same full semantic input. No automatic-drop activation, P1
closure, new SoT CLOSED row, push or official installation follows.
