# Comment-excluding source-size gates

Date: 2026-09-30 (Asia/Seoul)
Base HEAD: `7a9fe09d8293170c4ce676de6464486cd25ca348`, dirty/moving tree.
This is developer-tool evidence, not compiler semantics or self-host progress.

## Outcome and metric owner

The old source-size caps used physical `wc -l`/awk records, including comments;
some consumers also lost an unterminated last record. The host owner is now
`scripts/source_size_count.py`. Existing manifests and call sites still own
numeric limits; no limit was increased for this metric change.

- Count LF physical records, including an unterminated last record.
- Exclude nonblank lexical comment-only records. Inline code+comment counts1.
- Blank records still count1, including blank records inside block comments.
- String, Python docstring and shell heredoc payloads count; shebangs count.
- C/header/Pergyra comments respect quoted literals; Python uses stdlib tokenize.
- Explicit plain-data suffixes retain records; unknown dialects fail closed.
- Every duplicate cap is checked, including a later tighter cap. Batch file
  measurements are cached once, not cap decisions. `--total` emits no sum if
  any input fails; repeated requested files contribute repeatedly to the sum.

Current hash-pinned host owner:
`4FBA796B30173110977C5369517DBDB65B9F8B3D2619E672C690F585F3F4B6CF`.
The size-only Pergyra compatibility owner is `lib/source_size.pgy`, SHA
`D68A596FA01E055E7B886E9338DB071C936102E39E4F4CB85ACDB0B0C4AF7E08`.
Both production C/header tools use it; `TextScan.CountLines` remains unchanged
for examples/text inventory consumers. This is pure computation, not a new
world/subject/intent compiler stage or a SoT registry closure claim.

## Consumers and ratchets

Component batched caps, shared owner-size policy, production/header/backend/
semantic-TU caps, direct focused owner caps and aggregate family caps use the
host owner. Output occurrence, row inventory, profile and progress counts were
not changed. Unsupported/unreadable inputs propagate failure before numeric
comparison; they must not become an empty-string arithmetic0. Aggregate family
measurements use explicit guards or `--total`, not a sum of possibly empty
command substitutions.

The actual component mechanics checker runs the fast host lexical tests.
Both existing production-size Makefile targets run the full99-CLI suite and actual SourceSize owner
probe. Synthetic over-cap production fixtures now contain code, not701/1001
comment-only records; their existing expected over-cap sizes remain unchanged.
The Pergyra runner defaults to the normal compiler path and accepts PGY_BIN;
the temporary private launcher is evidence for this run, not a permanent API.

## Observed focused verification

Latest acceptance supersedes the older7232 packet below. A concurrent change
of unknown provenance added shell escaped-LF physical-record preservation and
quoted punctuation heredoc delimiters at10:41:24; its tests changed at10:41:44.
Root and test agent did not author those edits, did not revert them, and instead
revalidated the exact current files. Host31 methods/99 CLI subprocesses PASS
exit0,31.963s, evidence `.tmp/self_hosted/source_size_count.wwrvba3m/`, host
before/after4FBA796B identical. Current test SHA
`158E503283526D42E5C210B23B06B65988D99255DA4805F4327FDC6FD5398C52`
also stayed unchanged. Current component mechanics checker exit0 with14
lexical tests PASS (0.003s), logs `comment-component-checker-moving-final.*`.

The actual unchanged Pergyra owner/fixture/runner were rebuilt and rerun against
the current host:24 positives/11 refusals/2 caps PASS exit0, evidence
`.tmp/self_hosted/source_size_c_owner.vUAQSO/`. Native emission0 errors/0 warnings,
GCC stderr empty; source-before/after manifests match current host4FBA796B,
ownerD68A596F and private launcherF06E9EB7. C artifact SHA9429BBE1... unchanged;
new executable SHA
`0F315CB8F003A85CDBF695EC39119722EA0149D7B943B13A511305DCB770C214`.
This moving-tree refresh does not establish who authored the concurrent edits
or make whole component/production targets green.

### Earlier root-owned7232 focused packet (historical evidence)

Host suite:30 methods /99 real CLI subprocesses, PASS exit0,27.654s. Evidence
`.tmp/self_hosted/source_size_count.xhhqvpfy/`, before/after host hash identical.
Test SHA
`032B782379C1D4E9EEF7E18C35782732A2C96210A7EEA4909DF2DA00F5E92607`.
Includes600 code records plus900 comments accepted,601 code records refused,
inline comments, literal payloads, EOF/CRLF/BOM/NUL, invalid/missing input,
duplicate caps and all-or-nothing totals. NUL is tested in the host, not claimed
for Pergyra ReadFile/String representation.

Actual Pergyra owner:24 fixed positive goldens,11 lexical refusals and2 cap
branches, PASS exit0. Evidence `.tmp/self_hosted/source_size_c_owner.jdIfBT/`.
Fixture SHA`CC5F01EF98FB5D256949273202B39C0990A6DE2163193458565FCC58057B85B3`;
runner SHA`61C4B150A78923CDCA4115A0742C54D8D3AB89D728CC2E5129235197956729C6`.
Native emission0 errors/0 warnings, GCC stderr empty, before/after source and
private launcher hashes identical. C artifact SHA
`9429BBE1F8831417E01597599C56E279F30E949E8A74C63147C7F18B38FBF3DA`;
executable SHA
`1C35D64AAF7390EAD8149A7CE815E1F0E98F0DB8E0C6487C2A05245AA1ACBA83`.
This is actual owner execution, not copied counting logic or parser-only proof.

Root's actual self-host owner-policy scan:1880 source files measured, exit0,
empty counter/policy stderr. Logs
`.tmp/owned_result_reachable_20260930/comment-owner-{counts,policy}-final.*`.
Reviewer measured293 unique existing literal shell cap targets, exit0; all4
literal600-line shell targets included. That read-only snapshot predates the
C-only hardening; dynamic paths and all repository shell grammar are not claimed.
Changed shell scripts passed bash-n; git diff--check passed.

Root's earlier component mechanics checker exited0, with13 fast lexical tests
PASS in0.002s. Logs
`.tmp/owned_result_reachable_20260930/comment-component-checker-budgeted.*`.
Root independently replayed the native probe:600+900 comments exit0/count600,
601 exit1 and exact cap refusal, prefixed spliced-end exit1 and exact unsupported
diagnostic. The full component inventory run bounded by60s exited1 after the
mechanics PASS, with no final inventory verdict; logs `comment-full-component.*`.
It is not a full pass, and no hidden source-check error is inferred from the
incomplete output.

## Actual production findings, not green repo-wide size evidence

Root freshly emitted/compiled/executed both actual Pergyra tools. Native
emissions0 errors/0 warnings; both GCC stderr files empty; source hashes matched.
Evidence `.tmp/self_hosted/source_size_tools_final.4qodmi/`. Header tool scanned
752 headers and refused4; C tool scanned1076 files and refused5, all exit1.
The shell header gate independently reported the same4 sizes. These9 source
files have no current git diff and were not edited by this task.
Root's host production-C scan also reported the same5 sizes, log
`.tmp/owned_result_reachable_20260930/comment-production-c-final.out`.

| Existing source owner | Comment-excluding size | Existing cap |
| --- | ---: | ---: |
| src/runtime/pgy_runtime_builtin_hashmap_inline.h | 642 | 600 |
| src/runtime/pgy_runtime_lib_raw_map_exports.h | 656 | 600 |
| src/runtime/pgy_runtime_lib_raw_map_key_exports.h | 772 | 600 |
| src/runtime/pgy_runtime_map_string_inline.h | 880 | 600 |
| src/codegen/llvm_expr_call_dispatch.c | 701 | 699 |
| src/codegen/transpiler_expr_call_member_emit.c | 812 | 699 |
| src/compiler/mir_branch_source_facts.c | 877 | 699 |
| src/compiler/mir_json_dump.c | 711 | 699 |
| src/semantic/collection_ownership_fact.c | 839 | 699 |

## Limits and retained falsifiers

Shell case/esac inside command substitution is explicitly refused: pattern')'
requires grammar ownership not provided by this bounded counter. Ordinary
quoted nested expansions conservatively retain payloads; exact full-shell
comment elimination is not claimed. A valid complex component checker source
itself is refused by the bounded shell lexer; it is not a capped source target.

C phase-2 spliced comment delimiters are explicitly refused, not guessed as
extra code/comment records. Ordinary continued line comments and quoted
spliced payloads are tested. Full C preprocessing is not claimed. Pergyra tool
compatibility is verified on fixed ASCII C/header goldens and the reached real
production inventories, not all Unicode whitespace/String byte representations.

Retained failures: initial heredoc EOF#word and nested quote under-count;
valid nested case pattern refusal hardening; BOM-only Pergyra off-by-one;
Windows test TSV CRLF; host block.find skipping the prefixed spliced-end guard.
The latter failed `.tmp/self_hosted/source_size_count.b6gplnzm/` and
`.tmp/self_hosted/source_size_c_owner.Cw9dtd/`; bytewise block scanning fixed it.
Root's intermediate integrated checker failure6mbgsnul is retained too.

The owner-size policy mechanics runner previously timed out at the static60s
budget; that whole checker is not claimed green from its reached partial cases.
The initial all99-CLI hook in the static component checker also exhausted60s
in this environment (exit1/incomplete result rather than success). It was moved
to the focused production-size targets; the static hook runs lexical tests only.
Full component inventory, complete production Makefile targets, C/LLVM tool
matrix, installed driver/fixed-point and CI are not closed by this packet.
Existing production oversizes are a separate owner-directed split task; do not
raise caps or open unrelated compiler-stage tracks to make this metric green.

## Reproduce the focused checks

```bash
export PATH=/ucrt64/bin:/usr/bin:$PATH
python3 tests/source_size_count_test.py -v
timeout 60 bash tests/self_hosted_component_checker_smoke.sh
PGY_BIN="$PWD/.tmp/owned_result_reachable_20260930/native-bin/pgy.exe" \
  PGY_SELFHOST_CC=gcc bash tests/self_hosted/parity/source_size_c_owner_probe.sh
```

The private compiler in the observed run has SHA
`F06E9EB7F464CA5ADC4906FBA31F3953667EC6FCBE6262B41721622032DE3660`.
No install, commit, push or user-local vision/repro/concurrent bootstrap rewrite.
