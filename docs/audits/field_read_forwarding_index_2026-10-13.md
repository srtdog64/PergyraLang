# Exclusive field-read forwarding index: bounded unblocking receipt

Status: VERIFIED CANDIDATE — actual-owner parity, final permanent gate,
cold Make entry and scoped independent review pass; publication pending. Base:
`main @ 28d075016ca8a0e49c5e88f340916b0f158ec3c3`.
This is supporting execution evidence, not a semantic authority or SoT closure.

## Objective and whole-chain boundary

Unblock the fixed-input DRV-2 failure inventory within its existing300-second
and3072-MiB validation boundary. Priority: preserve owner identity and every
read fact/marker, preserve refusal and convergence behavior, remove the
repeated edge-times-all-keys operation, then measure this fixed input.

The existing fact owner is SemanticAstCollectionExclusiveFieldReads in
`src/self_hosted/semantic/ast_collection_exclusive_value_field_read_owner.pgy`.
Body admission populates formal bindings; this owner derives direct field
reads and propagates them along argument-forwarding edges. Exclusive-value
classification, family-read/growth and restoration consumers read this same
map; collection ownership and the body verdict remain the last admission
consumers. The map's generation, key identity and lifetime do not change.
No new owner, shared cache or alternative admission path is introduced.

Each forwarding round still snapshots MapKeys(reads), visits edges in their
original order, inserts absent source-prefix/suffix keys, and retains the
same changed/remaining/unconverged boundary. A local heads/tails/next index
groups that immutable snapshot by the complete prefix before the first colon.
Bucket links preserve original key order. Only the matching bucket is visited
for an edge; the suffix is copied unchanged, including *, !, @, @~ and paths.
The old edge-by-every-key search is removed, with no compatibility fallback.
Missing facts and unconverged markers keep their existing refusal behavior.

The shared file also has earlier unrelated import/signature WIP. Those edits
are explicitly excluded: the isolated baseline/candidate manifest has3164
files each, identical membership and exactly one differing path. Only its
forwarding span changes. Manifest SHA-256:
`C74752EE8EE2DAD9F89896F1CFA1A59663C544AB3080CE13DCBC4B6E303BA95D`.

## Observed verification

- Actual uninstrumented Pergyra-owner parity:
  `.tmp/field-read-index-parity/receipt-2bc68927bce04fb98215ac01c7f682f0`.
  Both analyzers build with the identified native C scaffold; four actual
  programs are parsed/analyzed, never emitted or executed. Nonempty ordered
  outputs are byte-identical (counts6/6/4/6): direct/transitive fields, nested
  path markers, whole-value wildcard and retaining markers. Source membership,
  source/probe and analyzer executable guards pass. Earlier header-only COUNT0
  results were vacuous and are excluded.
- A separate ordered differential model passes1381 checks, including250
  productive generated trials, prefix separation, later colons, cycles,
  duplicates, wildcard/retaining/path markers and round limits. This is model
  evidence, not production speed or self-host substitution progress.
- Designated independent review independently re-executes both retained
  actual-owner analyzers on all four sources and checks all3164 manifest
  entries, forwarding-span identity and the reached MapKeys/MapSet runtime.
  Scoped successful-execution key/order/fixed-point PASS; no production gate,
  source, index or refs changed by that review.
- Fixed full-input indexed diagnostic:
  `.tmp/field-focus-direct-8937781bcb5b4cd88630fc7893a1cb70`, original parser
  count127137/digest1216510375, build53.6s, analysis177.1s, sampled private peak
 1762.7MiB below the unchanged3072 cap. Forwarding completes10 rounds,
  535423 indexed-key visits,107680 edges and896447 matching-key visits.
  Source membership/bytes and executable guards pass. Baseline timed out
  before this owner returned; no general speedup ratio is established.
- Permanent gate on cold main-plus-only-this-unit tree
  `da6b1d2e12a7c8b03dcc253930313fb9bbf5eaa1` passes via the script and the actual
  Make entry `self-host-collection-field-read-summary-test-smoke`. Location:
  `C:/Users/user/AppData/Local/Temp/pgy-field-read-index-landing-5ab446e0391b421c97ef1d26b2df1e08/source`.
  Script receipt `.tmp/collection-field-read-summary.9B3Yr6` and pressure
  receipt `.tmp/root-cold-field-read-pressure-f29f5c2e31dd4ad491e3a4bd0f0d792f`
  show actual C/LLVM analyzer builds, four exact positive projections per
  backend, two existing source refusals per backend and six validator faults
  rejected. Script takes105.2s; native builds each retain13 warnings and zero
  errors. Targets are never emitted or executed. Guards pass. Native scaffold
  E1E56AB3F2EA076641C545F8D6C6E461752DE69145C37B535C5670A8A09F1A12 is
  explicitly reused; this is not installed-driver or full-WIP acceptance.
  Actual Make receipt `.tmp/root-cold-make-field-read-pressure` passes87.6s
  with `-o` naming that same frozen scaffold, preventing a rebuild/overwrite.
  Its wrapper-only sampled memory misses compiler descendants; it is not a
  complete memory measurement. The direct script has observed samples,
  also not an arbitrary-input peak-memory proof. Both keep the original300
  deadline; no validation allowance is raised.
- The gate's exact timeout routine is independently executed on a harmless
  test-owned parent/child. Receipt
  `.tmp/field-read-owned-timeout-9633eee7c0104a00b2cad5766c689b29` observes
  timeout refusal, termination of both owned processes, retained partial
  output/status and unchanged gate source. No unrelated process is stopped.
- Ordered diagnostic continuation receipt
  `.tmp/field-focus-direct-1c2c139f8d2341089958931475eee134` preserves live scratch
  and query order (no per-edge fact cloning). It maps node107235 to
  `MirIntentExecutionCaptureStep`'s call of CaptureCompensations and records
  that same sink refusal. Build54.7s/query175.6s, original300/cap3072 and
  input guards pass. It remains TAINTED/INCOMPLETE at node119391,
  owned_element_escape/String. This newly reached loan consumer is a separate
  ownership obligation, not evidence that descriptor recovery fixes it.
- Final gate generation SHA-256
  `21CB55583D96F4ED7FC3A5F4CCF55D84748CF38CA9F43FCFBC9EECB55A82C1AA`
  is cold-tested on tree59341fde01fde7e9847f1173403a3c0d36d9c572 at
  `C:/Users/user/AppData/Local/Temp/pgy-field-read-index-landing-3f733543129a4594b24fbf4d17637cb4/source`.
  Actual Make entry passes102.6s; receipt
  `.tmp/collection-field-read-summary.L70RX3`, pressure
  `.tmp/root-cold-make-field-read-pressure`. C and LLVM each build0 with13
  retained warnings, four projections and two source refusals; seven validator
  faults and all declared generation guards pass. This checks exact FORMAL
  inventory and READ sets/joins plus KEY order, not an external syntax-ID
  oracle or READ stream-order proof. Earlier six-fault receipts refer only to
  their earlier gate generation and are not relabeled as this result.
- Independent review found an exited-parent timeout gap in the original
  gate. The final Windows path assigns a blocked launcher to one owned Job
  before releasing its input token, then imports subprocess and starts the
  command. Timeout terminates the exact Job; its handle closes on every exit.
  Isolated/no-site launcher startup prevents cwd shadow imports/startup hooks
  before assignment. Both live-parent and exited-parent/pipe-holder paths
  are observed in `.tmp/field-read-owned-timeout-d9182fac44304b9d89b803739c936156`;
  the latter records launcher_rc0 and both record owned_job/cleanup PASS.
  The reviewer verifies the corrected Job/startup path and exact no-token/
  wrong-token refusal in the configured interpreter. This is not a claim
  about arbitrary compromised installations; no unrelated process is killed.
- A subsequent reached traversal finishes under explicitly TAINTED diagnostic
  continuations in
  `.tmp/field-focus-direct-0da8d4e4279d4f3fa464e65a1032f2a5`: build54.0s/query193.2s,
  total300/cap3072 and guards unchanged. Actual false guards are statement1,
  argument sinks6 and String elements2; alias/member-root false counts are0.
  Shared-issuer logs are excluded. This is completed reached execution under
  continuations, not original admission or an independent universal census.

## Limits and next falsifier

The full-input runner contains diagnostic bypasses. It remains TAINTED and
INCOMPLETE, refusing node107235 (unproved_exclusive_sink_entry, nominal
aggregate); it neither admits canonical input nor enumerates all dynamic
failures. The physical field-focus producer, recovery carrier, every continuing
exit consumer and installed-driver/CI acceptance remain OPEN.

The index allocates per-round maps, links and String prefixes. OOM can occur
earlier than with the baseline; this is an explicit resource-failure difference,
not a proved arbitrary-input equivalence. Heap live bytes and temporary
retirement were not measured. The fixed input stays under its existing cap.
No universal performance, memory-safety or GC-comparison claim follows.

The permanent gate must falsify missing/extra/duplicate reads, wrong formal
identity, raw key-order drift, empty summaries and changed refusal of existing
unsafe field exits/shared aliases. It must build actual C and LLVM analyzers,
guard source membership/bytes and executables, and never run the supplied
source programs. Cold main-plus-only-this-unit validation and scoped gate
review pass; publication remains pending. After that unit lands, return to the complete
recovery family and compiler-owned exit synthesis; do not open a separate
optimization track.
