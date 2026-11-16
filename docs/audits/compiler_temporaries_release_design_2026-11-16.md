# D. 컴파일러 임시 값의 마지막 사용 해제 설계 (2026-11-16)

읽기 전용 조사와 설계다. 저장소는 바꾸지 않았다. 소스 줄 번호는 `origin/main`(`cd33a0031`) 기준이다.
측정은 WSL `Ubuntu-C-WSL`, 드라이버 `/root/tg-own/bin/pgy-self-driver`(main `f1456722d` 내용), 트리 `/root/tc-mem`에서 했다.
측정 도구는 WSL `/root/mem-tools/memD/`(`reach.c`, `reachrep.py`, `reachrun.sh`, `kind.py`, `lexcmp2.sh`), 원자료는 WSL `/root/tc-mem/outD/`에 있다.

## 1. 어느 emitter가 설치 드라이버를 만드는가

| 산출물 | 만드는 것 | emitter |
|---|---|---|
| `gen0.exe` (codegen oracle), `driver_oracle.c` | native `pgy --native-pipeline --emit-c` | native C backend |
| `gen1.exe`, `gen2.exe` (codegen seed) | `genN --source codegen/main.pgy` (`tests/self_hosted/parity/codegen_bootstrap.sh:8-11,367-392`) | self-host AST 소비 C emitter (`GenerateCFromTypedAstArtifact`, `codegen/run/codegen_run_owner.pgy:80-90`) |
| `bin/pgy-self-driver` (일반 빌드) | `gen2.exe --source driver_bootstrap_main.pgy` (`self_host_compiler_build.sh` "emitting DRV-2 from typed source artifact") | 같은 AST 소비 C emitter |
| `bin/pgy-self-driver` (CI, 고정점 채택) | `driver_seed --mir-json` → `driver_gen2.c` (`driver_bootstrap.sh:278-305`, ci.yml:289-294) | MIR→AST 텍스트 왕복 뒤 같은 AST 소비 C emitter (`GenerateCFromVerifiedSemanticArtifact`, `codegen/emission/program_entry_owner.pgy:10`) |
| `--mir-json-backend=c|llvm` | 직접 호출 | direct MIR emitter (`src/self_hosted/compiler/direct_mir_*`) |

- 설치 드라이버의 실행 메모리를 정하는 것은 **self-host AST 소비 C emitter**(`src/self_hosted/codegen/emission/`)의 정리 정책이다. 이 emitter는 자동 해제를 하지 않는다. `stmt_emit.pgy:256-262`의 scope cleanup은 zone lock, defer, focus 복구뿐이다. 방출된 C의 drop은 소스가 직접 부른 `ArrayDrop`류뿐이다.
- 설치 드라이버 자신의 C(`driver.c`, 29.0 MB)에는 `pgy_concat` 17,358곳, `pgy_tostr` 2,517곳, 배열 push 약 9,450곳이 있고 해제는 `free(` 195곳(런타임 본문 포함), `pgy_as_drop_owned` 69곳, `*_drop_storage` 244곳이다.
- native 정책은 gen0/gen1 씨앗과 oracle 증거에만 영향을 준다.
- V1에서 중요한 정책은 **direct MIR emitter**의 것이다. V1 범위가 "backend마다 일반 MIR emitter 하나, MIR→AST 왕복 삭제"이고, 자동 해제는 지금 그 emitter에만 있다. 다만 direct MIR은 semantic 크기 프로그램을 아직 받지 않는다(측정: `--mir-json-backend=c`가 `lexer` 76루틴은 받고 `semantic` 1,097루틴은 `program_readiness=26`으로 거부).

## 2. 지금 각 emitter가 해제하는 것과 근거 사실

**native C/LLVM**
- String 임시: 호출 인자로만 쓰이는 Concat 사슬과 String `Clone`만 함수 단위 region arena에 넣고 return 직전 `pgy_region_destroy`로 버린다(`region_escape_fact.c:181-236`, `transpiler_context.c:359-388`, `transpiler_mir_terminator_emit.c:220`). 근거는 `builtin_argument_retention_registry.def`의 "호출 동안만 빌림" 행. 루프 안 Concat도 함수가 끝날 때까지 남는다. `Substring`/`ToString` 결과와 `let s = Concat(..)`는 해제하지 않는다.
- Array: MIR `CLEANUP_EDGE` `local-collection-retire`(종류 1 String backing, 2 owned Strings, 3 스칼라 backing). `mir_loop_collection_cleanup.c:341-393`의 `private_uses` 증명을 통과한 지역만 대상이다(단일 정의, 지배 영역 안의 사용, 빌리는 builtin/읽기 전용 callee만, DROP receipt 없음).
- record, HashMap, owned String 지역: 해제 없음. drop glue가 없다.
- 요약 사실: `FunctionParamFlowSummaryEntry`(READ/WRITE/RELEASE/RETURN_ESCAPE/CALL_ESCAPE/CHANNEL_ESCAPE, `array_preserved`), `owned_string_result`(`function_param_publication_owner.h:161`), disposition `LIVE/RETIRED/RELEASED_MEMBER_MOVE/HANDED_OFF`(`collection_ownership_fact.h:99-112`).

**self-host direct MIR C/LLVM** (`src/self_hosted/compiler/`)
- `owned_storage_release_plan_owner.pgy:47-68`: 타입별 해제 모드(String, Array<Int/Bool>, 저장소 필드가 있는 record, Array<String>, HashMap, payload enum).
- `owned_local_lifetime_owner.pgy:87-449`: 지역 하나가 (정의 1개, 매개변수 아님, 모든 사용이 빌림 또는 readonly-ref/value-result/owner-handle carriage, return·owner-handle로 넘어가지 않음)일 때 모든 출구와 재정의 직전에 해제한다. 신선한 결과 판정(Concat, Clone, ToString, Substring, Trim, CharAtN, Join, ReadFile, 배열 리터럴, record 생성자, fresh-result 요약이 있는 callee)은 `owned_storage_result_recipe_owner.pgy:111-365`가 **emitter 안에서** 고정점으로 계산한다. MIR에는 실리지 않는다.
- `owned_expression_temporary_owner.pgy:19-59`: 부모가 관찰자(Concat, Clone, Length, IndexOf, Contains, ==, !=)이고 자식이 사용자 1명인 신선한 String이면 `pgy_retiring_string_observer_<kind>`로 해제한다. 사용자 함수 인자로 넘긴 신선한 String, Substring/ToString의 자식은 샌다.
- Array<String>: `collection_ownership_facts` 행과 instruction `collection_ownership_receipt`로 전이 상태를 만들고 `array_string_cleanup_policy_owner.pgy:60-143`이 drop 종류를 고른다. 전이 행이 없을 때 "legacy" 기본값으로 drop을 고르는 분기가 있다(조용한 대체 경로).

**self-host AST 소비 C emitter**: 자동 해제 없음(1절).

### 측정

**(a) 최고점 근처의 살아 있는 바이트를 도달 가능/불가능으로 나눔.** `reach.c`는 모든 블록의 (주소, 크기, 할당 지점)을 기록하고, 살아 있는 바이트가 새 고점을 지날 때 스택·레지스터·쓰기 가능 세그먼트에서 보수적으로 표시한다(블록 시작 주소만 포인터로 인정). 입력은 `semantic/main.pgy`(28,597줄, MIR JSON 41.1 MB).

| 경로 | 마지막 고점의 live | 도달 가능 | 도달 불가능(쓰레기) | 누계 할당 / 해제 |
|---|---:|---:|---:|---:|
| `--mir-json` (gen2 경로) | 768 MiB | 100 MiB | 667 MiB (86.9%) | 1,225 / 457 MiB |
| `--emit-mir-json-verified` | 512 MiB | 47 MiB | 464 MiB (90.8%) | 823 / 311 MiB |
| `--emit-c-artifact-verified` | 2,048 MiB | 170 MiB | 1,877 MiB (91.7%) | 2,836 / 788 MiB |

도달 가능한 양은 MIR JSON 바이트의 1.1~4.1배다. 나머지는 tracing GC라도 회수했을 쓰레기다.

**(b) 쓰레기의 종류** (MiB, 할당 지점의 런타임 함수로 분류)

| 종류 | `--mir-json` | `mir` | `c` |
|---|---:|---:|---:|
| String: Concat 결과 | 185 | 192 | 434 |
| String: Substring/CharAt/Trim 결과 | 69 | 34 | 167 |
| String: ToString/Join/Split/builder | 24 | 31 | 74 |
| String: 원소로 복사한 것 | 3 | 15 | 17 |
| 배열 성장 버퍼(런타임 push) | 15 | 12 | 43 |
| 사용자 루틴에 인라인된 할당 | 367 | 175 | 865 |
| 명시적 allocator (`pgy_alloc`, `JsonEmitCollection`) | 0 | 1 | 273 |

- "인라인된 할당"은 gcc가 push/버퍼 코드를 인라인한 곳이다. 가장 큰 곳: `MirRoutineGraphDistancesAvoiding`의 BFS `Array<Int>` 190(`--mir-json`과 `c`), `JsonEmitFieldRaw` 180·`JsonEscapeString…` 63·`JsonStringLiteral…` 62(`c` 전용, 문자열 조립), `JsonReadStringBounded` 56, `SemanticCallSpineViewFromGraph` 14~30, parser 표현식 행 20~30.
- `--mir-json`에서 String 결과가 쓰레기의 42%(281), 배열·record 버퍼가 57%(382, 그중 BFS 배열 하나가 190)다.
- `mir_lower/main.pgy`(`c`, 7.26 GB 시점의 기존 표본, 도달성 없음): `pgy_concat` 22.0%, `pgy_alloc` 15.1%, `JsonEmitFieldRaw` 9.8%, `pgy_substr` 6.2%, BFS 배열 2.7%. 누계 할당 9.75 GB, 해제 2.48 GB.
- Concat 지점의 모양(`driver.c`, grep 근사): 17,358곳 중 `x = Concat(x, …)` 누적 3,906줄, 다른 Concat/호출의 인자로 들어가는 안쪽 지점 약 7,200곳, `return Concat(…)` 347줄.

**(c) 지금의 direct MIR 해제 정책이 실제로 줄이는 양.** lexer 도구를 두 emitter로 빌드해 1.83 MB 입력을 돌렸다. 출력은 `cmp`로 같다.

| lexer 도구 | 할당 | 해제 | 끝까지 남은 것 | max RSS | 시간 |
|---|---:|---:|---:|---:|---:|
| AST emitter C | 181.0 MB | 42.3 MB (23%) | 138.8 MB | 151.5 MB | 3.6 s |
| direct MIR C | 193.5 MB | 98.4 MB (51%) | 95.1 MB | 128.6 MB | 7.3 s |

남은 바이트가 31% 줄었다. 시간이 2배인 이유는 조사하지 않았다(UNKNOWN).

**한계.** 도달성은 블록 시작 주소만 따라가는 보수적 근사이고, 마지막 고점 단계(128~256 MiB 간격)에서 잰 값이다. `mir_lower`와 DRV-2의 도달성은 재지 않았다(표 크기 때문에 6 GB 한도를 넘는다). 문자열 쓰레기 중 "임시 값"과 "죽은 배열·record가 들고 있던 것"의 비율은 재지 않았다.

## 3. 있는 사실과 없는 사실

| 필요한 사실 | 지금 | 빠진 것 |
|---|---|---|
| 값의 출처가 신선한가 | native `owned_string_result`; self-host는 emitter 안의 고정점, semantic은 Array<String>만 | MIR 시그니처의 결과 행 `fresh / lent(param i) / static / unproved`, 모든 타입 |
| 사용이 빌림인가 넘김인가 | `builtin_argument_retention_registry.def`(builtin), param flow summary(native), `carriage`(MIR param), 2026-11-16 결정의 형식 인자별 효과 집합(read/return/grow/keep/release/unproved, `.def` 예정) | builtin 결과·소비 행을 같은 `.def`에; 효과 집합 행의 MIR 게시(결정됨, 미착지) |
| 마지막 사용 | 없음(scope 출구와 재정의만) | MIR CFG 위의 경로별 liveness. 새 semantic 사실은 필요 없다 |
| 다른 저장소로 들어갔는가 | `collection_ownership_receipt`, disposition `handed-off`, 생성자 handoff | String과 record 필드 저장에 대한 같은 행 |
| 빌려준 결과가 살아 있는가 | getter loan 결정의 `lent` origin(S5b 예정) | owner 해제가 lent 결과의 마지막 사용 뒤라는 검사 |
| 타입별 해제 recipe | self-host `owned_storage_release_plan_owner`(record 한 겹) | native 쪽 없음; 중첩 payload, Array<record>, HashMap 값 |

## 4. 설계: MIR이 싣는 마지막 사용 해제

**목표.** 소유된 값마다 모든 경로에서 수명을 끝내는 지점이 정확히 하나이고, 그 지점은 마지막 접근 직후다. 증명하지 못한 값은 해제하지 않는다.

**결정하는 소유자 하나.** MIR 수준의 `value lifetime plan` 소유자(self-host `src/self_hosted/mir_lower/`, native `src/compiler/mir/`). 입력은 (1) 규칙 `.def` 한 파일(2026-11-16 결정의 hand-off 표에 builtin 결과 출처와 인자 소비 행을 더함. 지금의 `builtin_argument_retention_registry.def`를 흡수), (2) callee 요약 행, (3) MIR CFG와 SSA 이름. emitter는 결정하지 않는다.

**MIR에 싣는 형태.**
- 루틴 단위 `owned_value_facts` 행: `{value(binding_syntax_id 또는 표현식 임시 id), origin(fresh|call-result|moved-in|lent|static), recipe(해제 recipe 행 id), disposition(retired|handed-off|returned|lent|unproved)}`. 지금의 `collection_ownership_facts`를 String과 record로 넓힌 것이다.
- 해제 지점: 지금 있는 `cleanup` instruction(`local-collection-retire`, self-host 검증자 `collection_cleanup_fact_owner.pgy`가 이미 읽음)을 `value-retire`로 일반화한다. `{value, recipe, cause(last-use|redefinition|exit)}`를 instruction 뒤 또는 CFG edge에 둔다.
- 검증(두 front end 공통, MIR 소비 직전): 소유 값마다 정의에서 출구까지의 모든 경로에 수명 종료(retire, 넘김, return)가 정확히 하나, 종료 뒤 사용 없음, `lent` 값은 종료가 없고 owner의 종료보다 앞에서 끝남, `unproved`는 retire가 없음. 없거나 위조된 행은 거부한다. 진단은 값, 경로, 어긴 규칙을 적는다.
- 행이 없으면 누수이지 오류가 아니므로 native와 self-host가 다른 시점에 착지해도 안전하다. 두 쪽이 모두 낸 행은 같아야 한다.

**fail closed.** 사실이 없으면 `unproved`로 두고 해제하지 않는다. 공유를 반증하지 못한 자리는 기존 결정대로 whole `Clone` 또는 거부다. 누수 수는 census로 센다.

**문헌에서 가져오는 불변식과 전제**

| 출처 | 가져오는 것 | 전제와 차이 |
|---|---|---|
| Perceus [1], FBIP [2] | "garbage-free": 마지막 사용 직후 drop, 소비하는 callee로는 소유권을 넘김. 재사용: `x = Concat(x, y)`에서 `x`가 유일하고 여기서 죽으면 제자리 확장 | Perceus는 공유를 실행 중 참조 수로 처리한다. 여기는 참조 수가 없으므로 유일성을 정적으로 증명한 자리에만 적용하고 나머지는 Clone/거부 |
| Counting Immutable Beans [3] | 읽기만 하는 매개변수는 빌림, 소비하면 소유 | 효과 집합 표와 같은 구분. 추론은 요약 고정점 |
| Lobster [4] | 사용 지점마다 "소유를 원함/빌림"을 정하고 값마다 소유자 하나 | Lobster는 분석 실패 시 실행 중 RC로 돌아간다. 여기서는 복사 또는 거부 |
| ASAP [5] | 해제 기준은 scope 끝이 아니라 "이후 접근 없음" | ASAP는 공유 구조를 실행 중에 훑는 코드를 넣는다. 가져오지 않는다. 유일 소유가 증명된 값에만 적용 |
| Swift OSSA [6] | 소유 SSA 값은 모든 경로에서 수명 종료 사용이 정확히 하나, 빌림은 그 안에 중첩, verifier가 검사 | MIR이 SSA이고 넘김이 명시적이어야 함. 검증 규칙으로 가져온다 |
| Mojo [7] | 식 중간 임시 값도 마지막 사용 직후 파괴, 마지막 사용의 복사는 이동 | 저장소 안을 가리키는 값이 있으면 수명 연장 필요. 여기서는 `lent` 행 |
| Tofte–Talpin region [8] | 가져오지 않음 | native의 함수 단위 region은 루프에서 쌓인다. 대체 대상 |

**지울 옛 경로**
- self-host: `owned_local_lifetime_owner.pgy`의 판정 부분, `owned_storage_result_recipe_owner.pgy:277-365`의 fresh-result 고정점(MIR 시그니처 행으로 이동), `owned_expression_temporary_owner.pgy`와 `pgy_retiring_string_observer_*` 래퍼, `array_string_cleanup_policy_owner.pgy`의 전이 행 없는 legacy 기본값.
- native: `region_escape_fact.c` region 지점과 `pgy_region_string_concat`, `mir_loop_collection_cleanup.c`의 `private_uses`(새 소유자의 Array 판정으로 흡수).
- 컴파일러 소스의 수동 drop 호출(V1 "no manual drops").

**행동 음성 게이트**
1. ASan: 868개 lifetime fixture와 컴파일러 구성요소를 C(`-fsanitize=address`)와 LLVM IR `-O0`로 빌드해 실행. use-after-free와 double free가 0.
2. double-free witness: MIR JSON에서 retire 행 복제, `handed-off` 값에 retire 추가, `lent` 값에 retire 추가, owner retire를 lent 사용 앞으로 이동. 넷 다 검증자가 거부해야 한다. 검증을 끈 scratch 빌드에서는 ASan이 잡아야 한다.
3. legacy 기본값을 지운 뒤: 전이 행이 없는 Array<String>은 drop 없이 통과하고 census에 `unproved`로 잡힌다.
4. 출력 동일: gen2 == gen3, 구성요소 출력 `cmp`.
5. 메모리 상한(실행 게이트): `semantic/main.pgy --mir-json` peak RSS.

## 5. 이행 조각

기준선(semantic, `--mir-json`): 마지막 고점 live 768 MiB, 도달 가능 100 MiB, RSS 1,019 MiB. 아래 예상치는 2절 (b)의 종류별 쓰레기에서 계산한 추정이고 실행으로 확인한 값이 아니다. DRV-2 수치는 선형 외삽이다.

| 조각 | 내용 | semantic `--mir-json` live | DRV-2 gen2 MIR 경로 | DRV-2 한 프로세스 `c` |
|---|---|---:|---:|---:|
| 지금 | | 768 MiB | 10.4 GB | 약 23 GB |
| S0 | census: 소유 값 수를 disposition별로 출력. direct MIR이 DRV-2에서 거부하는 사유 전체 열거. 메모리 게이트 스크립트 | 변화 없음 | | |
| S1 | String: 신선한 String 임시(모든 빌림 사용), `x = Concat(x, …)`의 이전 값, 마지막 사용 뒤의 String 지역. `owned_value_facts`와 `value-retire` 행, 검증자, witness 게이트 | 490~560 MiB (String 쓰레기 281 중 210~280 회수) | 약 7 GB | 약 15~17 GB |
| S2 | 루틴 안에서 죽는 Array<스칼라>, Array<String>, 루프 반복마다 태어나는 배열 (BFS 배열 190, call spine 14 등) | 250~330 MiB | 약 3.5~4.5 GB | 약 9~11 GB |
| S3 | record recipe 중첩, Array<record>, 사용자 함수의 fresh 결과 행, 단계가 끝난 사실 묶음(`artifact`, `analysis`)의 마지막 접근 뒤 해제, handed-off 저장소의 callee 해제 | 150~200 MiB (도달 가능 100에 접근) | 약 2~2.7 GB | 약 3~5 GB |
| S4 | 제자리 재사용(누적 Concat 3,906줄), native region과 `private_uses` 삭제, 수동 drop 삭제 | 메모리보다 할당 횟수와 시간 | | |

- 목표(DRV-2 한 프로세스 16 GB 미만)는 **S2**에서 닿는다(추정 9~11 GB). S1만으로는 경계선이다. gen2 MIR 경로는 S1에서 이미 7 GB 근처다.
- 확인 방법: 조각마다 `mir_lower/main.pgy`의 `c` 경로가 8 GiB 안에서 완주하는지 본다(지금은 8,195 MiB 초과로 중단). S2 뒤 약 4 GiB 이하가 기대치다.
- 조각의 효과는 드라이버를 만드는 emitter가 retire 행을 소비해야 나타난다. 지금 그 emitter는 AST 소비 C emitter다(6절 결정 1).
- 조사 문서의 제안 B(JSON String 투영 제거)와 C(phi binding)는 이 설계와 독립이고 `c` 경로에서 `pgy_alloc` 273 MiB와 JSON 조립 약 305 MiB를 따로 없앤다.

## 6. 사용자 결정이 필요한 것

1. **retire 행의 첫 소비자**
   - A. AST 소비 C emitter가 문장 끝 단위로 소비: 설치 드라이버에 바로 효과. V1이 지울 왕복 경로에 투자.
   - B. direct MIR emitter만 소비: V1 방향과 같음. direct MIR이 컴파일러 크기를 받을 때까지 드라이버에는 효과 없음(지금 semantic 거부).
   - 권고: B. S0에서 direct MIR의 DRV-2 거부 사유 전체를 센 뒤, 거리가 멀면 A를 String 문장 끝 retire로만 한정해 임시로 둔다.
2. **한 변수에 신선한 값과 빌린 값(리터럴, 매개변수)이 섞일 때**
   - a. 빌린 정의 자리에 컴파일러가 복사를 넣어 소유로 통일(MIR에 기록). b. 실행 중 소유 플래그. c. 해제 안 함.
   - 권고: a. direct MIR의 `return_clones`가 같은 일을 이미 한다. b는 숨은 실행 상태다.
3. **handed-off 저장소**(2026-11-15 결정: 양쪽 다 해제 안 함)
   - a. S3에서 "callee가 소비하고 해제"로 다시 연다. b. 유지.
   - 권고: a. 결정문의 재개 조건(실제 프로그램이 저장소를 돌려받아야 함)을 컴파일러 자신이 충족한다. S0 census로 양을 먼저 잰다.
4. **native 착지 시점**
   - a. self-host가 먼저, native는 S3까지 따라옴(없는 행은 누수라 안전). b. 조각마다 동시에.
   - 권고: a. 규칙 `.def`는 처음부터 공유한다.
5. **메모리 상한을 CI 실행 게이트로 둘지**: 권고는 둔다(`semantic --mir-json` peak RSS, 조각마다 낮춤).

## 참고 문헌

1. Reinking, Xie, de Moura, Leijen. *Perceus: Garbage Free Reference Counting with Reuse.* PLDI 2021. https://www.microsoft.com/en-us/research/publication/perceus-garbage-free-reference-counting-with-reuse-2/ (확장판 MSR-TR-2020-42: https://microsoft.com/en-us/research/uploads/prod/2020/11/perceus-tr-v4.pdf)
2. Lorenzen, Leijen, Swierstra. *FP²: Fully in-Place Functional Programming.* ICFP 2023. https://doi.org/10.1145/3607840
3. Ullrich, de Moura. *Counting Immutable Beans: Reference Counting Optimized for Purely Functional Programming.* IFL 2019. https://arxiv.org/abs/1908.05647
4. van Oortmerssen. *Memory Management in Lobster.* 언어 문서, 2019~. https://aardappel.github.io/lobster/memory_management.html
5. Proust. *ASAP: As Static As Possible memory management.* University of Cambridge Computer Laboratory Technical Report UCAM-CL-TR-908, 2017. https://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-908.pdf
6. Swift project. *Ownership SSA* (`docs/SIL/Ownership.md`), SIL 문서. https://github.com/swiftlang/swift/blob/main/docs/SIL/Ownership.md ; Gottesman, *SIL Ownership Model Proposal (Refreshed)*, Swift Forums 2018. https://forums.swift.org/t/sil-ownership-model-proposal-refreshed/16872
7. Modular. *Mojo Manual: Death of a value.* https://docs.modular.com/mojo/manual/lifecycle/death
8. Tofte, Talpin. *Region-Based Memory Management.* Information and Computation 132(2), 1997. https://doi.org/10.1006/inco.1996.2613

[2], [3], [8]의 URL은 이번 조사에서 검색으로 다시 확인하지 않았다.
