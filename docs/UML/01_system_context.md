# 01. 시스템 문맥과 저장소 경계

이 그림은 Pergyra 소스가 사용자 요청에서 실행 artifact가 되기까지의 큰
경계를 보여 준다. 세부 의미와 최신 상태는 각 링크의 owner가 소유한다.

## 전체 구성 요소

```mermaid
flowchart TB
    User["사용자 / 빌드 도구"]
    Source[".pgy source와 import graph"]
    Editor["Editor / LSP / grammar tooling"]

    subgraph Repo["PergyraLang repository"]
        Launcher["CURRENT: pgy launcher<br/>src/pgy_driver.c"]
        Selector["CURRENT: route selection owner<br/>driver_self_host_selection_owner.c"]

        subgraph Native["Native C compiler path"]
            NativeFront["module loader + parser + semantic"]
            NativeIR["HIR / DIR / RIR / AIR / MIR"]
            NativePlan["verified region / projection plans"]
        end

        subgraph SelfHost["Installed self-host path"]
            SelfDriver["pgy-self-driver"]
            World["CURRENT: PgyCompilerWorld<br/>4 executable route zones"]
            SelfFacts["self-host parser / semantic / MIR facts"]
            GeneralEmit["V1 TARGET: general MIR emitters"]
        end

        CEmit["C artifact emission"]
        LlvmEmit["LLVM artifact emission"]
        Runtime["Pergyra runtime<br/>src/runtime"]
        Diagnostics["diagnostics / dumps / LSP facts"]
    end

    Host["Host C/LLVM toolchain and linker"]
    Binary["native binary / run output"]
    Gates["behavioral gates, parity, bootstrap, CI"]
    Rocq["PROOF: bounded Rocq models"]

    User --> Launcher
    Source --> Launcher
    Source --> Editor
    Editor --> Diagnostics
    Launcher --> Selector
    Selector -->|"default plain compile routes"| SelfDriver
    Selector -->|"explicit native pipeline and native-owned modes"| NativeFront

    NativeFront --> NativeIR --> NativePlan
    SelfDriver --> World --> SelfFacts --> GeneralEmit

    NativePlan --> CEmit
    NativePlan --> LlvmEmit
    GeneralEmit --> CEmit
    GeneralEmit --> LlvmEmit
    CEmit --> Host
    LlvmEmit --> Host
    Runtime --> Host
    Host --> Binary

    NativeFront --> Diagnostics
    SelfFacts --> Diagnostics
    Gates -. "execute and compare" .-> Launcher
    Gates -. "installed-driver and artifact parity" .-> SelfDriver
    Rocq -. "model obligations; not implementation status" .-> NativeIR
```

## 저장소 디렉터리 책임

```mermaid
flowchart LR
    Src["src/"] --> Lexer["lexer/<br/>token facts"]
    Src --> Parser["parser/<br/>AST and syntax identity"]
    Src --> Semantic["semantic/<br/>types, diagnostics, ownership admission"]
    Src --> Compiler["compiler/<br/>HIR, DIR, RIR, AIR, MIR, plans, driver"]
    Src --> Codegen["codegen/<br/>C and LLVM mechanical emission"]
    Src --> RuntimeDir["runtime/<br/>linked runtime operations"]
    Src --> SelfHosted["self_hosted/<br/>Pergyra compiler and tools"]
    Src --> Lsp["lsp/<br/>language-server boundary"]

    Tests["tests/"] --> Gates2["behavioral, negative, parity, bootstrap gates"]
    Docs["docs/"] --> Contracts["contracts, registry, truth, navigation"]
    Grammar["grammar/ + editors/"] --> Surface["surface tooling projections"]
    Workflows[".github/workflows/"] --> CI["exact-job orchestration"]
```

디렉터리가 owner를 자동으로 결정하지는 않는다. 예를 들어 backend 파일이
타입·소유권·ABI 결정을 다시 만들면, 디렉터리가 맞더라도 SoT는 중복된다.

## 경계를 바꿀 때

- launcher 경로를 바꾸면 self-host 선택 owner와 installed-driver gate를 함께
  확인한다.
- IR/ABI 사실을 바꾸면 C와 LLVM을 각각 고치는 것이 아니라 그 사실 owner와
  두 backend 소비를 함께 추적한다.
- runtime symbol을 추가하면 compiler-side runtime-call ABI와 실제 runtime 구현,
  link 경계를 한 사슬로 본다.
- LSP나 dump 출력이 production compilation과 같은 사실을 필요로 하면 semantic
  owner를 재사용한다. 출력 경로가 별도 의미 owner가 되면 안 된다.

## 권위 링크

- 네이티브 실행 흐름: [02_native_compiler_pipeline.md](02_native_compiler_pipeline.md)
- self-host world: [03_self_host_world.md](03_self_host_world.md)
- 현재/목표 논리 계층: [180번 문서](../180_compiler_logical_spine_handles_gates.md)
- 최신 상태: [truth/state](../truth/state/)

