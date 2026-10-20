# 2026-10-20: canonical census after call results became paths of a formal

Recorded by the Claude lane on branch `claude/p0-native-checker` (local
commits, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** a call result that holds a formal's storage is a
  path of it, and a loan's carriers must stay unchanged
  ([decision](../decisions/2026-10-20_call_result_holding_formal_storage_is_its_path.md)).
- **Wall:** 810 s for four analyses (3.98 GB peak).

| Run | Refusals |
|---|---|
| before (`c60564db0`) | 2532 |
| results tracked, a local of a result an alias | 3112 |
| a local of a result is that result | 2868 |
| an Option of a String or copy-only value holds no storage (final) | 2479 |

- Final run: 448 refusals cleared and 395 appeared. The rounds: 4377,
  2885, 2571, 2479.
- Cleared: read-and-return constructors of a formal's fields
  (`DirectMirCfgPlanWithDigest` 42, `DirectMirCfgCertificateWithDigest`
  49) now lend, and callees that pass such results on are proved.
- Appeared: 135 uses after `SemanticAstIntentTransitionFactsFromArtifact`,
  35 after `SemanticAstLocalBindingFactsFromArtifact`, and 90 after the
  `TypedAstArena*Text` getters. They are not yet classified as real
  changes of shared storage or as proof imprecision.
- The intermediate rows show what each part of the rule costs: following
  results without treating their locals as results refused every
  `let x = Getter(formal); return x`, and treating `Option<String>` as
  storage refused every reassigned getter result.
