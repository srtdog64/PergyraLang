/* Source admission only: rejected programs are never executed. */
static void
test_callable_capability_inference(void)
{
    static const struct {
        const char *name;
        const char *source;
        const char *diagnostic;
    } cases[] = {
        {"effect inference preserves existing transitive parallel refusal",
         "func Touch() -> Void with effects secure { }"
         "func Relay() -> Void { Touch(); }"
         "func Main() -> Void { parallel { Relay(); } }",
         "Parallel context does not permit calling secure-effect function 'Relay'"},
        {"inferred forward effects reach the caller",
         "func Main() -> Void with effects local { Log(Later()); }"
         "func Later() -> Int { return Now(); }",
         "Function 'Main' is missing declared effects: nondeterministic"},
        {"formal call effects reach the caller",
         "func Clock(x: Int) -> Int { return x + Now(); }"
         "func Invoke(f: func(Int) -> Int) -> Int { return f(1); }"
         "func Main() -> Void with effects local { Log(Invoke(Clock)); }",
         "Function 'Main' is missing declared effects: nondeterministic"},
        {"inferred forward capability reaches the caller",
         "func Main() -> Void with caps random { Log(Later()); }"
         "func Later() -> Int { return Now(); }",
         "Function 'Main' is missing declared capabilities: clock"},
        {"returned callable retains its authority",
         "func Clock(x: Int) -> Int { return x + Now(); }"
         "func Pick() -> func(Int) -> Int { return Clock; }"
         "func Main() -> Void with caps random { let f = Pick(); Log(f(1)); }",
         "Function 'Main' is missing declared capabilities: clock"},
        {"returned formal is instantiated separately at each call",
         "func Clock(x: Int) -> Int { return x + Now(); }"
         "func Pure(x: Int) -> Int { return x + 1; }"
         "func Pick(f: func(Int) -> Int) -> func(Int) -> Int { return f; }"
         "func A() -> Int with caps clock { let f = Pick(Clock); return f(1); }"
         "func B() -> Int with caps random { let f = Pick(Pure); return f(1); }"
         "func Main() -> Void { Log(A()); Log(B()); }", NULL},
        {"callable alias retains its authority",
         "func Clock(x: Int) -> Int { return x + Now(); }"
         "func Main() -> Void with caps random { let f = Clock; let g = f; Log(g(1)); }",
         "Function 'Main' is missing declared capabilities: clock"},
        {"unused lambda does not consume its body authority",
         "func Main() -> Void with caps random {"
         "let f: func(Int) -> Int = (x: Int) => x + Now(); Log(1); }", NULL},
        {"invoked lambda consumes its body authority",
         "func Main() -> Void with caps random {"
         "let f: func(Int) -> Int = (x: Int) => x + Now(); Log(f(1)); }",
         "Function 'Main' is missing declared capabilities: clock"},
        {"uninvoked event does not consume subscriber authority",
         "event Tick(value: Int);"
         "func Handler(x: Int) -> Void { Log(Now()); }"
         "func Main() -> Void with caps random { Tick += Handler; Log(1); }", NULL},
        {"event invocation includes registered handler authority",
         "event Tick(value: Int);"
         "func Handler(x: Int) -> Void { Log(Now()); }"
         "func Main() -> Void with caps random { Tick += Handler; Tick(1); }",
         "Function 'Main' is missing declared capabilities: clock"},
        {"method invocation includes the resolved method authority",
         "class Clock { func Read(self) -> Int { return Now(); } }"
         "func Main() -> Void with caps random { let clock = Clock(); Log(clock.Read()); }",
         "Function 'Main' is missing declared capabilities: clock"},
        {"unsupported mutable callable provenance is not pure",
         "func Pure(x: Int) -> Int { return x; }"
         "func Main() -> Void { let mut f = Pure; f = Pure; Log(f(1)); }",
         "Closed callable capability use has unresolved value provenance"},
        {"unused abstract ability does not claim a closed dynamic call",
         "ability Clockable { func Read(self) -> Int; }"
         "func Main() -> Void with caps random { Log(1); }", NULL},
        {"dynamic ability invocation includes implementation authority",
         "subject Device { let value: Int; } ability Clockable { func Read(self) -> Int; }"
         "role Clock for Device { impl Clockable { func Read(self) -> Int { return Now(); } } }"
         "party Panel { dyn role slot device: Clockable; }"
         "func Main() -> Void with caps random { let p = Panel(); bind p.device = Clock; Log(p.device.Read()); }",
         "Function 'Main' is missing declared capabilities: clock"},
        {"dynamic ability invocation includes implementation effects",
         "subject Device { let value: Int; } ability Clockable { func Read(self) -> Int; }"
         "role Clock for Device { impl Clockable { func Read(self) -> Int { return Now(); } } }"
         "party Panel { dyn role slot device: Clockable; }"
         "func Main() -> Void with effects local { let p = Panel(); bind p.device = Clock; Log(p.device.Read()); }",
         "Function 'Main' is missing declared effects: nondeterministic"},
        {"dynamic ability does not pick a pure implementation by bind order",
         "subject Device { let value: Int; } ability Clockable { func Read(self) -> Int; }"
         "role Clock for Device { impl Clockable { func Read(self) -> Int { return Now(); } } }"
         "role Pure for Device { impl Clockable { func Read(self) -> Int { return 1; } } }"
         "party Panel { dyn role slot device: Clockable; }"
         "func Main() -> Void with caps random { let p = Panel(); bind p.device = Pure; Log(p.device.Read()); }",
         "Function 'Main' is missing declared capabilities: clock"},
        {"dynamic ability callback keeps the supplied callable authority",
         "func Clock(x: Int) -> Int { return Now(); }"
         "subject Device { let value: Int; } ability Callable { func Apply(self, f: func(Int) -> Int) -> Int; }"
         "role Invoke for Device { impl Callable { func Apply(self, f: func(Int) -> Int) -> Int { return f(1); } } }"
         "party Panel { dyn role slot device: Callable; }"
         "func Main() -> Void with caps random { let p = Panel(); bind p.device = Invoke; Log(p.device.Apply(Clock)); }",
         "Function 'Main' is missing declared capabilities: clock"},
        {"dynamic ability with sufficient capability remains admitted",
         "subject Device { let value: Int; } ability Clockable { func Read(self) -> Int; }"
         "role Clock for Device { impl Clockable { func Read(self) -> Int { return Now(); } } }"
         "party Panel { dyn role slot device: Clockable; }"
         "func Main() -> Void with caps clock { let p = Panel(); bind p.device = Clock; Log(p.device.Read()); }", NULL},
        {"party rejects absent ability implementation before callable sealing",
         "ability Clockable { func Read(self) -> Int; } party Panel { dyn role slot device: Clockable; }"
         "func Main() -> Void { let p = Panel(); Log(p.device.Read()); }",
         "Party role slot 'device' requires ability 'Clockable', but no subject-bound role implements it"},
        {"constructor as callback constructs a value without invoking a body",
         "enum Value { Item(Int) }"
         "func Build(f: func(Int) -> Value) -> Value { return f(1); }"
         "func Main() -> Void with caps random { let value = Build(Item); }", NULL},
        {"recursive local lambdas reuse finite capability identities",
         "func Pure(x: Int) -> Int { return x; }"
         "func Repeat(f: func(Int) -> Int, n: Int) -> Int {"
         "let next: func(Int) -> Int = (x: Int) => x + 1;"
         "if n > 0 { return Repeat(next, n - 1); } return f(1); }"
         "func Main() -> Void with caps random { Log(Repeat(Pure, 3)); }", NULL},
        {"capture owner still forbids captured callable storage",
         "func Wrap(operation: func(Int) -> Int) -> Int {"
         "let f: func(Int) -> Int = (x: Int) => operation(x); return f(1); }",
         "Lambda capture of local 'operation' is not supported"}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        TEST(cases[i].name);
        Lexer *lexer = lexer_create(cases[i].source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && (cases[i].diagnostic == NULL
            ? result->error_count == 0
            : result->error_count > 0
              && ctx_has_diagnostic_substring_from_result(result, cases[i].diagnostic)));
        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
