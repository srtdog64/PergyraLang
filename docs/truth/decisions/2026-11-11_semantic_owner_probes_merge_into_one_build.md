# Probes that import the semantic owners merge into one build per job (2026-11-11)

Decided by the user on 2026-11-11.

- The contract gates whose probes import the self-host semantic body owner
  or the direct-MIR admission owner get their units from one combined
  probe program per import set, selected by mode, instead of one probe
  program each.
- A make target builds each combined probe once per backend per job.
  Gates reuse it only through receipts naming the exact compiler, probe
  source and imported sources by hash. A missing or different receipt
  fails the gate; the gate never rebuilds it.
- That build gets a time limit set from its measured duration, recorded
  with the measurement when it lands. The 120 s limit of each separate
  probe build goes away with the separate builds.
- Every negative the separate probes checked is kept as a mode of the
  combined probe.

Reason: on 2026-11-11 the contracts shard's step 4 built 14 such probes,
each spending about 140 to 150 s in native analysis of the same imported
owners, so the per-probe 120 s limits could not be met and the builds
together exceeded the shard's 30 minute job limit.
