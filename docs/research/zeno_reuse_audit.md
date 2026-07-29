# Zeno 재사용 감사 — Pergyra stdlib/library/compiler 채택 후보

Updated: 2026-07-30 (Asia/Seoul)

Status: research inventory. 이 문서는 채택 후보와 falsifier를 정리하지만
semantic SoT나 완료 원장은 아니다. 실제 채택 상태는
`docs/201_insere_zeno_lineage_and_library_adoption.md`, Pergyra source, owner
registry와 executable gate가 소유한다.

## 0. 결론

Zeno는 충분히 재사용할 가치가 있다. 다만 가치의 중심은 TypeScript 코드나
`DataView` API가 아니라 다음 네 가지 설계 불변식이다.

1. layout을 한 normalized fact owner가 소유하고 analyzer, validator, emitter,
   runtime projection, inspect와 diff가 그 사실을 소비한다.
2. 경계에서 plan/receipt를 한 번 검증한 뒤 hot path는 같은 그래프를 다시
   검증하거나 재구성하지 않는다.
3. canonical graph와 반복 스캔용 rebuildable numeric index를 구분한다.
4. compiler failure, memory-boundary failure, unchecked hot path와 application
   concurrency policy를 서로 다른 책임으로 둔다.

Pergyra에 바로 맞는 것은 1·2이고, 3은 현재 program graph 통합과 메모리 압력의
좋은 후속 후보다. 반대로 Zeno의 TypeScript AST frontend, `ArrayBuffer`/`DataView`
표면, `pointer32`, shared writer와 renderer 예제는 그대로 가져오면 Pergyra의
기존 MIR ABI, ownership, authority와 concurrency owner를 두 번째로 만든다.

가장 먼저 구현할 새 slice는 **기존 MIR ABI tuple에서만 파생되는 layout
inspect/diff**다. 새 layout IR·새 hash·새 byte runtime은 만들지 않는다. 현재 진행
중인 semantic-admission executable rung이 먼저 안정화되어야 하며, 이 연구 때문에
그 active rung을 갈아끼우지 않는다.

## 1. 감사 objective card

- Objective: `F:/zeno`의 실제 구현 자산을 Pergyra의 stdlib, official library,
  compiler-internal/tooling, reject/defer로 분류하고 첫 bounded slice를 정한다.
- Priority: Pergyra semantic identity와 one SoT, owner-directed fact, repeated proof
  제거, explicit failure, executable gate, 그 다음 이식량과 편의성.
- Fact owner: Zeno는 provenance/falsifier다. 채택된 의미의 최종 owner는 기존
  Pergyra MIR ABI, Slot, diagnostic, graph와 runtime owner여야 한다.
- Last consumer: 후보마다 실제 compiler artifact, FFI/binary boundary 또는
  official-library workload를 별도로 지정한다. fixture만 있으면 `REACHABLE` 이상을
  주장하지 않는다.
- Forbidden fallback: TypeScript source 복사, 두 번째 layout hash/offset table,
  implicit little-endian, receipt 없는 raw projection, hot-loop 재검증, canonical
  graph와 numeric index의 이중 권위.
- Verification: 후보별 positive/negative gate와 C/LLVM parity. self-host progress는
  실제 C-owned production path가 삭제될 때만 `SUBSTITUTING`이다.

## 2. 관찰 snapshot과 검증 범위

감사 대상은 `F:/zeno` working tree다.

- HEAD: `2865f670f711460488db05d29f3bcc0d42d92bfa`.
- Branch: `main`, `origin/main`보다 1 commit 앞섬.
- 기존 dirty 파일: `package.json`, `package-lock.json`, `llms.txt`, 두 WebGL example
  package manifest. 이 감사에서 Zeno 파일은 수정하지 않았다.
- 규모: `packages` 85 files / 73 code files / 약 14,836 TS·MJS lines,
  `tests` 39 files / 약 6,226 lines, `examples` 64 files, `docs` 53 files.
- License: `F:/zeno/LICENSE`, MIT, Copyright (c) 2026 Exornea.

관찰한 실행 증거:

```text
npm test -- --reporter=dot
  26 test files passed
  176 tests passed

npm run package:policy
  package policy check passed

npm run version:check
  version check passed: 2.9.3
```

`npm run release:check`, browser smoke와 benchmark matrix는 이번 감사에서 실행하지
않았다. 따라서 README의 ns/record 수치나 renderer benchmark를 Pergyra 성능
보증으로 가져오지 않는다. 현 테스트 PASS는 Zeno 구현의 내부 일관성 증거이고,
Pergyra port의 correctness/performance 증거가 아니다.

## 3. Pergyra 철학으로 번역하는 법

Zeno의 package/class 모양을 복사하지 않고 경계 의미를 다음처럼 번역한다.

| Zeno 책임 | Pergyra canonical shape | 이유 |
| --- | --- | --- |
| layout row, pack plan, diff row, diagnostic evidence | `struct` + pure `func` | identity나 authority가 없는 계산 fact다. |
| 같은 실행 안에서 backing fact를 읽는 projection view | 필요할 때만 `object` + `refresh` | local read projection이며 외부 transfer가 아니다. |
| source lifecycle에서 분리해 파일/API/world 경계로 넘기는 manifest/receipt | 필요할 때만 method-free `tobject` + `publish` | detached immutable transfer다. live slot, borrow, authority를 싣지 않는다. |
| reusable mutable table/arena 내부 상태 | 실제 장기 owner가 생기면 subject-owned `vessel` | 일반 mutable DTO나 전역 helper로 만들지 않는다. |
| allocation, resize, publish, commit/reject | 실제 identity-bearing owner가 있을 때만 `subject.action` | 순수 pack/diff를 ceremonial action으로 감싸지 않는다. |
| byte arena, file, FFI, shared-memory lifetime | 실제 resource frontier가 있을 때만 `zone` | namespace를 zone으로 부르지 않는다. |
| compiler artifact publish 같은 현실 목적 | coordination/authority/effect/failure/trace가 한 목적에 결속될 때만 `intent` | 함수 여러 개를 묶었다는 이유로 intent를 만들지 않는다. |

따라서 Zeno-derived code의 대부분이 `struct`/`func`로 남는 것이 정상이다.
`tobject`는 layout row 자체의 대체물이 아니라, 검증된 manifest나 receipt가 source
lifecycle을 떠나는 순간의 해법이다.

## 4. package와 자산별 판정

| Zeno 영역 | 핵심 자산 | Pergyra 판정 | 배치 | 난이도 |
| --- | --- | --- | --- | --- |
| `@exornea/zeno-types` | TS branded ABI marker | 이식 금지 | 없음 | - |
| `@exornea/zeno-schema` | field offset/size/align/endian을 묶은 Layout IR | 구조 원칙만 채택; 새 IR 금지 | existing MIR ABI owner | 중 |
| analyzer/lowering | restricted `.zeno.ts` AST → Layout IR | 이식 금지 | 없음 | 매우 높음 |
| validator | overlap, alignment, descriptor, inline-cycle fail-closed | falsifier/negative corpus 채택 | compiler internal | 중 |
| emitter layering | assembly와 responsibility emitter 분리 | 구조 원칙 채택 | self-host codegen owners | 낮음 |
| capability-derived emission | 필요한 runtime import만 파생 | 이미 대응 owner 존재; 중복 금지 | existing codegen usage owner | 완료/유지 |
| layout manifest/inspect/diff | change review와 breaking classification | **우선 채택** | self-host tooling | 중 |
| measurement diagnostics | observed/required resolution과 phase 구분 | 기존 loss/diagnostic owner에 결합 | compiler internal | 중 |
| generated source map | generated member → source field provenance | 원칙 채택, TS AST 알고리즘은 금지 | compiler tooling | 중~높음 |
| scalar/range runtime | width/endian/range/alignment checks | L0 owner가 필요할 때만 재표현 | runtime/core | 높음 |
| fixed byte/ASCII predicates | decode 전 byte predicate | safe `ByteView` 이후 후보 | L1 official library | 중 |
| cursor projection views | checked default + explicit unchecked | 언어 lifetime 증거 전에는 보류 | runtime/library | 높음 |
| `Span32`/`Vector32`/`pointer32` | relocatable dynamic descriptor | 현 ABI에 직접 이식 금지 | 없음 | 매우 높음 |
| optional frame header | magic/version/endian/layout/payload preflight | real binary consumer가 생기면 후보 | official library/internal | 중 |
| `ArenaEpoch`/generation handle | stale reuse rejection | 핵심 invariant는 이미 채택됨 | Slot + SnapshotTicket | 구현됨/부분 |
| dynamic/shared writer | tail arena, Atomics publication | 이식 금지; Pergyra resource/concurrency owner와 충돌 | 없음 | 매우 높음 |
| fixed record table | grow/reuse, caller-owned output | safe byte storage + workload 뒤 후보 | official library | 높음 |
| pack/histogram plan | validate once, fused scan, output capacity | 알고리즘/경계 원칙 채택 | official library/compiler internal | 중~높음 |
| diagram graph index bench | interned rows + degree/CSR adjacency | memory profile가 증명하면 compiler read index 후보 | compiler internal | 중~높음 |
| tests/release policy | property, allocation, API inventory, doc drift | gate 패턴 적극 채택 | tests/tooling | 낮음 |
| WebGL/renderer examples | workload witnesses | fixture/provenance만 유지 | research only | - |

## 5. 고가치 후보 상세

### 5.1 P0 원칙 — admission은 한 번, hot consumer 재증명은 0번

근거:

- `F:/zeno/packages/buffers/src/pack-f32.ts:10`의 plan 생성은 stride와 모든
  field offset을 한 번 검증한다.
- `F:/zeno/packages/buffers/src/pack-f32.ts:70`의 plan consumer는 그 immutable
  plan을 반복 루프에서 사용한다.
- `F:/zeno/docs/reference/runtime-boundary.md`는 checked boundary와 unchecked
  hot loop를 명시적으로 분리한다.
- `F:/zeno/tests/runtime/allocation.test.ts:53`은 100,000 record scalar/cursor
  scan의 retained heap budget을 별도 gate로 둔다.

이 패턴은 현재 Pergyra의 semantic-admission 작업을 직접 지지한다.

```text
whole semantic artifact
  -> deep validation exactly once
  -> immutable admitted facts/receipt
  -> emission consumes admitted facts
  -> no AST reconstruction, no repeated deep Ready
```

새 generic `ValidatedPlan` helper를 만들지는 않는다. 현재 owner인
`SemanticAstArtifactAdmissionReady`와 admitted emission seam이 해당 fact family의
구체적 계약을 소유해야 한다. 검증 횟수와 재구성 횟수를 executable counter 또는
negative call-edge gate로 잠그는 것이 채택의 전부다.

### 5.2 P1 — MIR ABI layout inspect/diff

Zeno 근거:

- `F:/zeno/packages/schema/src/index.ts:40`의 field row는 name, offset,
  byteLength, alignment를 함께 운반한다.
- `F:/zeno/packages/schema/src/index.ts:204`의 struct row는 byteLength,
  alignment, endianness와 fields를 결속한다.
- `F:/zeno/packages/compiler/src/layout-manifest.ts:60`은 validated layout에서만
  manifest를 projection한다.
- `F:/zeno/packages/compiler/src/layout-manifest.ts:105`는 두 manifest를 비교한다.
- `F:/zeno/packages/compiler/src/layout-manifest.ts:155` 이후는 size, alignment,
  endian, offset와 field shape 변화를 breaking으로 분류한다.
- `F:/zeno/tests/compiler/layout-manifest.test.ts:69`는 offset 이동과 struct size
  변경이 breaking이고 추가 field가 version routing 없이는 안전하지 않음을
  실행한다.

Pergyra에는 이미 다음 owner가 있다.

- `D:/PergyraLang/src/self_hosted/mir_lower/abi_layout_fact_owner.pgy`:
  `MirAbiLayoutRowCapture`, exact tuple validation과
  `MirAbiLayoutIdFromCapture`.
- `D:/PergyraLang/src/self_hosted/compiler/abi_layout_row_manifest.pgy`:
  runnable ABI row projection.
- `D:/PergyraLang/src/self_hosted/lib/binary_projection_preflight_owner.pgy`:
  generation/layout/endian admission.

따라서 새 `PgyLayoutIR`, 새 offset calculator, Zeno FNV hash를 만들지 않는다.
기존 capture의 exact tuple을 inspect/diff 가능한 구조로 projection한다. 현재 layout
ID가 28-bit이므로 diff correctness를 ID equality 하나에 맡기지 않는다. 기존
validation session처럼 exact tuple을 비교하고 ID는 identity/lookup fact로만 쓴다.

Objective card:

- Objective: 두 target-scoped MIR ABI capture set의 변경을 `same`, `breaking`,
  `requires_version_route`로 분류한다.
- Fact owner: `MirAbiLayoutRowCapture`와 현재 target/endian owner.
- Last consumer: ABI change review tool과 실제 binary/FFI artifact admission.
- Forbidden fallback: AST/source type 재탐색, C spelling에서 size 추측, ID-only
  equality, implicit endian, backend별 local layout table.
- Output shape: process 내부 비교는 `struct AbiLayoutDiff`; 파일/API 경계에
  publish할 때만 immutable `tobject AbiLayoutDiffReceipt` 후보.

최소 falsifier:

1. exact tuple은 `same`.
2. 같은 type/field name의 offset, size, alignment 또는 endian mutation은
   `breaking`.
3. field/type 추가는 explicit version route가 없으면 success가 아니라
   `requires_version_route`.
4. 같은 layout ID라도 raw exact tuple이 다르면 collision/drift로 fail-closed.
5. missing target/endian/field row와 duplicate type/field는 서로 구별되는 failure.
6. diff consumer 안에 layout 계산, AST scan 또는 `SelfMirJsonStaticAbiLayout`
   호출이 없어야 한다.
7. C/LLVM tool output이 byte-equal이어야 한다.

### 5.3 P1 — resolution/loss-aware diagnostics

근거:

- `F:/zeno/packages/compiler/src/measurement.ts:1`은 관측 resolution layer를
  순서 있는 vocabulary로 둔다.
- `F:/zeno/packages/compiler/src/measurement.ts:19`은
  `InsufficientResolution`, `UnsupportedAtPhase`, `AmbiguousLayout`,
  `DuplicateDefinition`, `LayoutInvariantViolation`을 구분한다.
- `F:/zeno/packages/compiler/src/diagnostics.ts:19`은 source-derived와
  IR-derived provenance를 분리하며 fake line 0을 만들지 않는다.

아이디어는 가치가 높지만 enum과 Result 구현을 그대로 복사하면 안 된다.
Pergyra에는 이미 diagnostic code owner와
`docs/semantics/loss_contract_manifest.md`가 있다. 다음 필드를 기존 diagnostic
fact에 연결하는 것이 맞다.

- loss-contract row identity;
- observed stage와 required stage;
- compilation phase;
- source span 또는 IR-derived evidence identity;
- precise diagnostic code와 broader failure class.

첫 negative는 initializer/assignment/statement unresolved fixture가 source AST를
다시 읽거나 line 0을 만들지 않고 같은 owner fact를 소비하는지 확인한다. 이
작업도 production self-host driver가 소비하기 전에는 `SURFACE` 또는
`REACHABLE`이다.

### 5.4 P1/P2 — canonical graph 뒤의 rebuildable numeric index

근거:

- `F:/zeno/docs/llm/README.md:94`는 canonical editor graph를 object/JSON에
  남기고 numeric index를 derived read model로 제한한다.
- `F:/zeno/packages/bench/diagram-graph-index.mjs:258`은 fixed node/edge rows를
  만든다.
- `F:/zeno/packages/bench/diagram-graph-index.mjs:369`는 row를 한 번 스캔해
  degree fact를 만든다.
- `F:/zeno/packages/bench/diagram-graph-index.mjs:421`과 `:437`은 prefix sum과
  cursor fill로 CSR-style adjacency를 구성한다.
- 같은 benchmark의 methodological note는 index build cost를 제외했으며 한 번만
  읽을 때는 rebuild cost가 지배한다고 명시한다.

Pergyra 적용은 “모든 graph를 하나의 의미 없는 binary graph로 합친다”가 아니다.
canonical identity는 SoT registry의 각 fact family owner에 남긴다. 하나의 program
epoch/root가 expression, CFG, domain topology 등의 typed view를 조직하고, 반복
탐색이 측정된 consumer에만 compact index를 한 번 파생한다.

후보 shape:

```text
canonical graph owner + graph epoch/digest
  -> one admitted numeric row index
  -> typed views: expression / CFG / domain / reachability
  -> repeated consumer lookup
```

index는 `object` read projection 후보이며, detached artifact로 publish될 때만
`tobject`가 된다. 문자열 이름, source provenance, authority와 semantic kind를
numeric row에서 역산하지 않는다.

채택 조건:

- profile이 같은 graph epoch에 대한 반복 scan/reconstruction을 보여야 한다;
- build count 1, consumer reconstruction count 0;
- index가 graph id/digest와 node/edge count를 결속해야 한다;
- canonical owner와 다른 결과가 나오면 index가 폐기되고 fail-closed해야 한다;
- 2.9MB/5.1MB compiler artifact에서 시간과 peak memory가 실제로 개선되어야 한다.

이 조건 전에는 Zeno graph benchmark는 좋은 falsifier일 뿐 library 기능이 아니다.

### 5.5 P2 — fixed record table과 pack/histogram official library

근거:

- `F:/zeno/packages/buffers/src/table.ts:11`은 row byte length와 capacity만 아는
  재사용 table이다.
- `F:/zeno/packages/buffers/src/pack-uint.ts:17`과
  `pack-f32.ts:10`은 reusable immutable plan을 만든다.
- `F:/zeno/packages/buffers/src/pack-mixed.ts:13`은 한 source pass에서 두 output
  stream을 만들되 capacity를 fail-closed한다.
- `F:/zeno/packages/buffers/src/histogram.ts:3`은 caller-owned buckets를 쓴다.
- `F:/zeno/tests/buffers.test.ts:28`은 reuse, endian fallback, short output,
  overflow와 invalid bucket을 함께 검증한다.

이 알고리즘은 renderer 전용이 아니며 compiler table, telemetry, device batch에도
쓸 수 있다. 그러나 현재 Pergyra stdlib에는 안전한 general byte view, explicit
endianness와 lifetime-pinned backing storage가 없다. 지금 `stdlib/binary_table.pgy`
같은 API를 만들면 String/Array 내부 layout이나 generated C pointer가 공개
semantic contract가 될 위험이 크다.

승격 순서:

1. 실제 workload가 fixed-row pack/histogram을 요구한다.
2. L0가 checked `ByteView`/range/alignment/endian contract를 소유한다.
3. L1/official library는 caller-owned output과 immutable plan만 노출한다.
4. unchecked loop는 preflight receipt 없이는 호출할 수 없게 한다.
5. C/LLVM parity, short buffer/output, integer overflow, wrong endian, stale backing
   negative와 allocation/peak-memory budget을 둔다.

`FixedRecordTable`의 growable storage를 `parallel`/`async` 경계로 raw pointer 전달하는
것은 금지한다. concurrent publication이 필요하면 library table이 아니라 별도
zone/subject action과 pinned/copy/channel boundary가 소유한다.

### 5.6 P2 — generated source provenance

`F:/zeno/packages/compiler/src/source-map.ts:27`과
`F:/zeno/tests/compiler/source-map.test.ts:15`는 generated accessor를 schema field로
되돌리고 absolute workspace path 누출과 substring-based line matching을 거부한다.

Pergyra가 채택할 것은 다음 불변식이다.

- generated output span은 stable source/stage identity를 운반한다;
- source path는 artifact boundary에서 canonical/relative form으로 투영한다;
- emitted text substring을 다시 찾아 provenance를 복구하지 않는다.

Zeno 구현처럼 emitted source를 TypeScript AST로 다시 parse하는 알고리즘은
Pergyra에 맞지 않는다. Pergyra emitter가 text span을 생성하는 순간 기존
`SourceUnitId`/`SyntaxNodeId`/MIR identity에서 mapping row를 함께 발행해야 한다.
`docs/180_compiler_logical_spine_handles_gates.md`의 source/AST stable handle이 아직
`PARTIAL`이므로 그 owner가 먼저 닫혀야 한다.

### 5.7 즉시 채택할 수 있는 test/release 패턴

새 runtime 기능 없이도 다음 방법론은 가치가 있다.

- property negative: `F:/zeno/tests/runtime/property.test.ts:14`는 128-run
  arbitrary payload/descriptor round-trip과 malformed range rejection을 분리한다.
- allocation ratchet: `F:/zeno/tests/runtime/allocation.test.ts:53`은 checksum과
  retained-heap budget을 함께 본다.
- public surface inventory: `F:/zeno/tests/public-api.test.ts:8`은 root export
  surface를 snapshot한다.
- documentation drift: `F:/zeno/tests/compiler/schema-grammar-doc-drift.test.ts:95`
  는 문서의 supported/rejected 예제를 실제 analyzer에 넣는다.
- layer policy: `F:/zeno/tests/layer-model.test.ts:41`은 renderer/domain 개념이 core
  API로 새는 것을 거부한다.
- package/release policy: `F:/zeno/scripts/package-policy-check.mjs`와
  `version-check.mjs`는 package metadata/version drift를 fail-closed한다.

Pergyra에는 이미 inventory, docs UTF-8, language example, ABI shape, component와
negative call-edge gate가 많다. 별도 test framework를 들이지 않고 기존 shell/C/
Pergyra fixture에 이 패턴을 추가한다.

## 6. stdlib vs official library vs compiler-internal

### stdlib/core에 지금 넣지 않는 것

- raw byte buffer, `DataView`, typed-array와 pointer API;
- generic layout descriptor/hash;
- shared writer/Atomics;
- renderer pack API;
- schema compiler와 별도 IDL.

이들은 모든 프로그램이 지불해야 할 universal primitive가 아니거나 현재 L0
semantic contract가 없다.

### future official library 후보

- checked byte view와 explicit endian이 생긴 뒤의 fixed record plan,
  pack/histogram;
- 실제 file/network/FFI consumer가 견인하는 optional binary frame envelope;
- safe byte view 위의 ASCII equality/prefix/contains/hash predicates.

각 모듈은 실제 workload, namespace, docs inventory와 C/LLVM gate가 생긴 뒤에만
`active`다. 그 전에는 proposal이며 `stdlib/` sketch 파일을 미리 만들지 않는다.

### compiler-internal/tooling 우선 후보

- current MIR ABI tuple inspect/diff;
- loss-contract-aware diagnostic evidence;
- stable source provenance mapping;
- measured repeated scan을 제거하는 epoch-bound compact graph index;
- fixed-shape memory/validation-count pressure gates.

이 배치가 Zeno의 장점을 가장 빨리 살리면서 public language/runtime ABI를
성급하게 늘리지 않는 경로다.

## 7. 명시적 do-not-port 목록

- `.zeno.ts` grammar, TS brand type와 TypeScript AST analyzer/lowering;
- Zeno Layout IR, FNV layout hash나 field offset table을 두 번째 SoT로 복사;
- `DataView`, `ArrayBuffer`, `TypedArray` 이름을 Pergyra semantic core에 노출;
- `Span32`, `Vector32`, `pointer32` ABI와 mutable rebase view;
- `SharedArrayBuffer`, Atomics, shared tail arena와 JS worker publication;
- implicit `littleEndian = true` default;
- runtime hot loop마다 `Result` object를 만들거나, 반대로 untrusted boundary에서
  unchecked read를 허용하는 것;
- renderer, WebGL/WebGPU, scene, ECS, asset-loading behavior;
- graph index를 canonical compiler/program graph 또는 저장 포맷으로 승격;
- schema evolution을 optional field/default/vtable 없이 흉내 내는 것;
- Zeno benchmark 수치를 Pergyra 성능 약속으로 인용하는 것.

## 8. 라이선스와 외부 의존성

Zeno는 MIT이고 Pergyra는 BSD-3-Clause다. 라이선스는 호환 가능하지만 Zeno code의
substantial expression을 복사하면 MIT copyright/permission notice를 source 또는
배포 자료에 보존해야 한다. 이번 계획은 원칙과 불변식을 Pergyra owner에 맞춰
재구현하는 방식을 우선한다. 실제 코드 복사가 생기면 별도 provenance/notice
변경을 같은 commit에 포함해야 한다.

package dependency 관찰:

- buffers/schema/types는 external runtime dependency가 없다.
- runtime은 같은 version의 `@exornea/zeno-types`만 의존한다.
- compiler는 같은 version의 `@exornea/zeno-schema`와 TypeScript `^5.9.0`에
  의존한다.
- fast-check, FlatBuffers, Playwright, Vitest 등은 root dev/benchmark dependency다.

Pergyra port에는 이 npm dependency를 추가하지 않는다. 필요한 알고리즘은 현재
C/Pergyra runtime과 test harness로 검증한다. FlatBuffers fixture도 비교 oracle일
뿐 production dependency가 아니다.

## 9. 권장 실행 순서

1. **현재 active rung 완료**: semantic artifact deep analysis 1회, admitted emission
   reconstruction 0회와 3,072MB pressure gate를 먼저 닫는다. Zeno-derived plan
   원칙은 이 rung의 근거이지 별도 helper commit이 아니다.
2. **ABI diff 최소 slice**: 기존 `MirAbiLayoutRowCapture`에서 exact tuple inspect와
   breaking classifier를 만들고 same/offset/endian/duplicate/collision negatives를
   C/LLVM으로 실행한다.
3. **diagnostic 결합 slice**: 기존 diagnostic fact에 loss row와 observed/required
   stage를 붙인다. 새 taxonomy를 만들지 않는다.
4. **graph profile**: program graph의 epoch당 build/scan/reconstruction 횟수와 peak
   memory를 계측한다. 반복 scan이 입증될 때만 CSR-style read index를 만든다.
5. **real binary workload 대기**: FFI/compiler artifact/device batch가 fixed-row
   packing을 요구할 때 checked byte substrate와 official library를 함께 설계한다.
6. **source provenance**: source/AST stable handle seam이 닫힌 뒤 emission-time
   mapping row를 추가한다.

## 10. 첫 ABI diff slice의 완료 정의

다음이 모두 있어야 첫 새 slice를 완료라고 부를 수 있다.

- fact owner: existing MIR ABI capture와 target/endian fact;
- one projection owner: exact tuple에서 manifest/diff를 만들며 layout을 계산하지
  않음;
- explicit failure: malformed, duplicate, missing target/endian, collision/drift가
  구별됨;
- old/forbidden path gate: AST scan, local offset calculation, new hash와 implicit
  endian을 거부;
- executable C/LLVM parity;
- mutation corpus: offset, size, alignment, endian, field removal/addition, duplicate,
  same-ID/different-tuple;
- inspect output은 presentation이고 capture/diff fact가 authority임;
- production consumer가 붙기 전 evidence grade는 `REACHABLE`, hard self-host
  `SUBSTITUTING`으로 세지 않음.

## 11. 핵심 source evidence map

### Zeno

- `F:/zeno/README.md` — product boundary와 projection-first identity.
- `F:/zeno/llms.txt` — package/layer map과 claim boundary.
- `F:/zeno/packages/schema/src/index.ts` — normalized Layout IR.
- `F:/zeno/packages/compiler/src/analyzer.ts` — restricted frontend admission.
- `F:/zeno/packages/compiler/src/validator.ts` — layout invariant negatives.
- `F:/zeno/packages/compiler/src/layout-manifest.ts` — inspect/diff projection.
- `F:/zeno/packages/compiler/src/measurement.ts` — resolution/phase evidence.
- `F:/zeno/packages/compiler/src/diagnostics.ts` — source vs IR-derived provenance.
- `F:/zeno/packages/compiler/src/source-map.ts` — generated provenance witness.
- `F:/zeno/packages/runtime/src/lifetime.ts` — epoch/generation stale rejection.
- `F:/zeno/packages/runtime/src/frame.ts` — optional boundary preflight.
- `F:/zeno/packages/runtime/src/range.ts` — memory boundary checks.
- `F:/zeno/packages/buffers/src/table.ts` — fixed row storage reuse.
- `F:/zeno/packages/buffers/src/pack-f32.ts`, `pack-uint.ts`, `pack-mixed.ts` —
  validate-once plan과 fused output.
- `F:/zeno/packages/bench/diagram-graph-index.mjs` — rebuildable numeric graph index
  witness와 제한.
- `F:/zeno/tests` — 176 observed passing tests.

### Current Pergyra owners

- `D:/PergyraLang/docs/201_insere_zeno_lineage_and_library_adoption.md` — canonical
  provenance/adoption contract.
- `D:/PergyraLang/src/self_hosted/mir_lower/abi_layout_fact_owner.pgy` — MIR ABI
  exact tuple와 identity owner.
- `D:/PergyraLang/src/self_hosted/compiler/abi_layout_row_manifest.pgy` — existing
  runnable row projection.
- `D:/PergyraLang/src/self_hosted/lib/snapshot_ticket.pgy` — generation/layout/endian
  snapshot ticket.
- `D:/PergyraLang/src/self_hosted/lib/binary_projection_preflight_owner.pgy` —
  projection admission receipt.
- `D:/PergyraLang/docs/200_object_to_action_boundary_patterns.md` —
  struct/object/tobject/vessel/subject/action/zone/intent 경계.
- `D:/PergyraLang/docs/semantics/sot_owner_spine_registry.md` — graph와 compiler fact
  family owner registry.
- `D:/PergyraLang/docs/180_compiler_logical_spine_handles_gates.md` — stable
  source/stage handle 상태.

## 12. 최종 판정

Zeno는 Pergyra에 가져올 것이 많은 프로젝트다. 특히 지금 겪은 3GB/20GB 계열
문제와 정확히 맞닿는 교훈은 **검증 가능한 plan을 경계에서 한 번 만들고, 이후
consumer가 같은 program fact를 다시 스캔·재구성하지 않게 하는 것**이다.

다만 Zeno 전체를 stdlib로 옮기는 것은 잘못된 방향이다. 가장 좋은 채택은
Pergyra의 기존 ABI/Slot/graph/diagnostic owner를 강화하고, layout diff와 memory
ratchet을 실행 가능하게 만드는 것이다. byte projection library는 실제 workload와
안전한 backing lifetime contract가 생긴 뒤에만 열어야 한다.
