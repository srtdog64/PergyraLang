# Web Target and DOM Ownership Thesis (TODO)

Status: exploratory. This is a strategy note and a staged TODO, not a committed
plan. It records why a DOM-owning web target is the differentiated bet for
Pergyra and the smallest experiment that proves or kills the thesis.

## The thesis

WASM and DOM ownership are not competing choices. They sit at different layers.
WASM is the execution substrate: where code runs and who owns memory. DOM
ownership is the paradigm built on top: whether Pergyra's domain model becomes
the UI model itself. A DOM-owning model still runs on a substrate (WASM
preferred, a thin JS glue acceptable as a first step).

The differentiation lives entirely in DOM ownership. Compiling to WASM alone
makes Pergyra "another systems language that runs on the web," competing with
Rust/WASM and AssemblyScript. Owning the DOM makes Pergyra something with no
direct peer, because the domain primitives map onto reactive UI:

- zone maps to a reactive scope and render boundary (isolation and ownership)
- slot maps to owned reactive state and a DOM handle
- the transitive frontier propagation graph maps to the reactivity engine
- intent maps to orchestration of UI updates
- capability maps to controlled DOM access

## Why this is reachable for Pergyra specifically

The reactivity engine that a DOM-owning model needs already exists. The
transitive frontier propagation graph (the world/zone scheduler) is a reactive
dependency graph, the same shape SolidJS signals and Svelte runes use. Most
projects would have to build that core first. Pergyra already has it. That is
the unfair advantage that makes this thesis concrete rather than aspirational.

## The proving demo (smallest experiment)

One page proves the whole thesis: change a slot inside a zone, and the DOM
updates reactively, driven by the propagation graph rather than manual DOM
writes.

Progress (rung-0, done). examples/reactive_dom_demo/main.pgy runs on both the
C and LLVM backends. A zone declares source state (slot counter) and a derived
view (slot view) bound by `refresh view from counter`, and a render function
turns the view into DOM text, emitting `<div id="app">count: 7</div>`. The
declaration and render path compile and run.

Key finding from rung-0. Pergyra reactivity is intent-orchestrated, not
imperative-eager. Writing a slot does not eagerly recompute the bound view; the
refresh fires when an intent step syncs the zone (`on: zone.Sync(n)` inside an
intent step). This is not a limitation for the web thesis, it is the right
shape: it maps directly onto the web event cycle, DOM event then intent then
zone Sync then projection refresh then DOM update. The reactive trigger is an
intent step, which is exactly how a reactive web framework routes a user event
into a state update. The DOM binding should therefore hook into the intent and
projection-ready path, not into raw slot writes.

TODO for the demo:

- [ ] Pick the substrate for the demo. Default to a thin JS glue layer first
      (Pergyra owns reactivity and state, JS only performs DOM calls), because
      pure WASM cannot own DOM operations today without crossing the JS
      boundary. Migrate the substrate to WASM via the existing LLVM backend once
      the binding shape is proven.
- [ ] Define a minimal binding surface: create element, set text or attribute,
      append child. Nothing more for the demo.
- [x] Map one zone to a render boundary and one slot to a text node (rung-0:
      zone CounterZone, slot view rendered to a div).
- [x] Drive the refresh so the view updates via the propagation graph after a
      slot write (rung-1, examples/reactive_dom_demo/rung1.pgy, verified on C
      and LLVM). A world method Bump() mutates only the source slot; the view
      refreshes through the zone projection (`state appViewReady: zone app
      projection view`) and the render reflects it, count 0 then 1 then 2. The
      view is never assigned by hand, so reactivity is Pergyra-owned.
- [x] Run the reactive logic in WebAssembly (rung-2, verified). The reactive
      demo was compiled to wasm32-wasi with the zig-bundled clang and run on
      node's WASI; it printed the same DOM snapshots as the native build,
      count 0 then 1 then 2. The reactive core (Bump and the _sync propagation
      functions) ran unchanged in WASM. The only porting needed was a tiny
      ucontext shim for the unused coroutine scheduler include. See
      examples/reactive_dom_demo/web/wasm_build.sh, ucontext_shim.h,
      run_wasi.mjs.
- [ ] Move the render to a real DOM node in the browser. The compute side is
      proven in WASM; remaining is to instantiate the same wasm in a page over a
      browser WASI shim and route a DOM click into a WebBump entry point added
      to the demo. host.html sketches the interactive wiring.
- [ ] Write the page in Pergyra, compile it through the chosen substrate, and
      confirm the reactive update path runs end to end.

If the demo works, the direction is proven and the framework surface
(components, events, routing) can follow. If it does not, the lesson is cheap.

## Rung-2 substrate scope (measured)

The WASM substrate is far smaller than the raw symbol count suggests. The
rung-1 demo's LLVM IR (pgy --emit-llvm on rung1.pgy) declares 484 runtime
symbols but actually calls only ten functions, and only five of those are
runtime, not user code:

- Reactive core, compiles to WASM as-is: CounterApp_Bump, CounterApp_sync,
  CounterZone_sync, RenderDom, Main. The two `_sync` functions are the
  compiled propagation graph, the reactivity itself, and they are pure logic
  needing no porting.
- Runtime porting surface, all small and mostly pure: StringConcat,
  pgy_int_to_string, pgy_args_init, and the panic export are pure and shim
  trivially. pgy_log_string is the only I/O call, and in the browser it maps
  to a DOM write.

So a first in-browser reactive demo does not require porting the runtime. It
requires compiling the reactive functions to WASM (the LLVM path) and providing
roughly five tiny shims, of which only one, the output call, is bound to the
DOM. The toolchain to lower the IR to wasm32 (clang or llc with a wasm target,
or emscripten/wasi-sdk) is not present in the current build sandbox, so the
wasm build itself must run in an environment that has it. The reactive code is
already in hand from rung-1.

## Substrate decision (deferred until after the demo)

- WASM via LLVM backend: preserves memory and ownership, the natural long-term
  substrate, reachable from the existing LLVM path. DOM access still crosses a
  JS boundary today, so DOM ownership is partial at the substrate level.
- JS as glue, not master: Pergyra owns reactivity, state, and ownership; JS
  performs only DOM calls. Fastest path to DOM proximity for the demo; intended
  as a starting point, not the end state.
- Plain JS as compile target: rejected. It erases Pergyra's primitives (no
  ownership, GC-managed, no isolation), turning the language into one of many
  transpile-to-JS languages and dissolving the differentiation at runtime.

## Scope risk (read before committing)

DOM ownership is not one backend. It is a UI framework, a render model, and a
reactivity binding, which is closer in scope to building React than to adding a
target. Carrying a self-hosting compiler and a web UI framework at once is a
very large surface for a small team. The mitigation is the staged demo above:
prove the single reactive page first, expand the framework surface only after
the thesis is validated, and keep each step independently demonstrable.

## Connection to the AI-orchestration angle

intent as the orchestration spine, plus capability-controlled DOM access, is
timely for AI-authored and AI-orchestrated interfaces. A DOM-owning Pergyra
where intent drives UI and capability bounds what generated code may touch is a
distinctive position for the AI era, and worth keeping in view when shaping the
framework surface after the demo.
