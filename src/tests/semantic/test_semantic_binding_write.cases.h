static void
test_binding_write_permissions(void)
{
    const char *accepted[] = {
        "func Main() -> Void { var x: Int = 1; x = 2; Log(x); }",
        "func Main() -> Void { let mut x: Int = 1; x = 2; Log(x); }",
        "func Main() -> Void { var xs: Array<Int> = [1]; xs[0] = 2; Log(xs[0]); }",
        "func Set(inout x: Int) -> Void { x = 2; } func Main() -> Void { var x: Int = 1; Set(x); Log(x); }",
        "func Set(inout x: Int) -> Void { x = 2; } func Main() -> Void { let mut x: Int = 1; Set(x); Log(x); }",
        "func Main() -> Void { var x: Int = 1; x += 2; x -= 1; x *= 3; x /= 2; x %= 2; Log(x); }",
        "func Main() -> Void { let mut x: Int = 1; x += 2; Log(x); }",
        "func Main() -> Void { var xs: Array<Int> = [1]; xs[0] += 2; Log(xs[0]); }",
        "func Main() -> Void { var xs: Array<Int> = [1]; ArrayPush(xs, 2); Log(xs[1]); }",
        "struct Point { x: Int; } func Main() -> Void { var p: Point = Point(1); p.x += 2; Log(p.x); }",
        "func Main() -> Void { var xs: Array<Int> = [1]; var s: Slice<Int> = xs.Slice(0, 1); s[0] += 2; Log(s[0]); }",
        "func Main() -> Void { let x: Int = 1; if true { var y: Int = 2; y += x; Log(y); } Log(x); }",
        "func Main() -> Void { var xs: Array<Int> = [1, 2]; var s: Slice<Int> = xs.Slice(0, 2); var t: Slice<Int> = s.Slice(0, 1); t[0] += 1; Log(xs[0]); }",
        "func Main() -> Void { var a: Array<Int> = [1]; var b: Array<Int> = [2]; var s: Slice<Int> = a.Slice(0, 1); s = b.Slice(0, 1); s[0] += 1; Log(b[0]); }",
    };
    const char *rejected[] = {
        "func Main() -> Void { let x: Int = 1; x = 2; }",
        "func Main() -> Void { let x: Int = 1; if true { x = 2; } }",
        "func Main() -> Void { let x: Int = 1; while false { x = 2; } }",
        "func Set(inout x: Int) -> Void { x = 2; } func Main() -> Void { let x: Int = 1; Set(x); }",
        "func Forward(inout x: Int) -> Void { Set(x); } func Set(inout x: Int) -> Void { x = 2; } func Main() -> Void { let x: Int = 1; Forward(x); }",
        "func Main() -> Void { let (x, y) = (1, 2); x = y; }",
        "func Main() -> Void { let x: Int = 1; x += 2; }",
        "func Main() -> Void { let x: Int = 1; if false { x -= 2; } }",
        "func Main() -> Void { let x: Int = 1; for i in 0..0 { x *= 2; } }",
        "func Main() -> Void { var x: Int = 1; if true { let x: Int = 2; x += 1; } }",
        "func Main() -> Void { let xs: Array<Int> = [1]; xs[0] = 2; }",
        "func Main() -> Void { let xs: Array<Int> = [1]; ArrayPush(xs, 2); }",
        "func Main() -> Void { let xs: Array<Int> = [1]; ArraySet(xs, 0, 2); }",
        "func Main() -> Void { let xs: Array<Int> = [1]; ArrayPop(xs); }",
        "func Main() -> Void { let xs: List<Int> = ListNew(); ListPush(xs, 2); }",
        "func Main() -> Void { let xs: HashMap<Int, Int> = MapNew(); MapSet(xs, 1, 2); }",
        "struct Point { x: Int; } func Main() -> Void { let p: Point = Point(1); p.x += 2; }",
        "func Main() -> Void { var xs: Array<Int> = [1]; let s: Slice<Int> = xs.Slice(0, 1); s[0] += 2; }",
        "func Main() -> Void { let xs: Array<Int> = [1]; var s: Slice<Int> = xs.Slice(0, 1); s[0] += 2; }",
        "func Main() -> Void { let xs: Array<Int> = [1]; let s: Slice<Int> = xs.Slice(0, 1); var alias: Slice<Int> = s; alias[0] += 2; }",
        "func Main() -> Void { var xs: Array<Int> = [1]; let s: Slice<Int> = xs.Slice(0, 1); var alias: Slice<Int> = s; alias[0] += 2; }",
        "func Main() -> Void { var a: Array<Int> = [1]; let b: Array<Int> = [2]; var s: Slice<Int> = a.Slice(0, 1); s = b.Slice(0, 1); s[0] += 1; }",
        "func Main() -> Void { var a: Array<Int> = [1]; let b: Array<Int> = [2]; var s: Slice<Int> = a.Slice(0, 1); if true { s = b.Slice(0, 1); } s[0] += 1; }",
        "func Main() -> Void { var a: Array<Int> = [1]; let b: Array<Int> = [2]; var s: Slice<Int> = a.Slice(0, 1); var n: Int = 0; while n < 1 { s = b.Slice(0, 1); n += 1; } s[0] += 1; }",
        "func Main() -> Void { let xs: Array<Int> = [1, 2]; var s: Slice<Int> = xs.Slice(0, 2); var t: Slice<Int> = s.Slice(0, 1); t[0] += 1; }",
    };
    for (size_t group = 0; group < 2; ++group) {
        const char **sources = group == 0 ? accepted : rejected;
        size_t count = group == 0 ? sizeof(accepted) / sizeof(accepted[0])
                                 : sizeof(rejected) / sizeof(rejected[0]);
        for (size_t i = 0; i < count; ++i) {
            TEST(group == 0 ? "mutable binding or interior write admitted"
                            : "immutable binding replacement refused");
            Lexer *lexer = lexer_create(sources[i]);
            Parser *parser = parser_create(lexer);
            ASTNode *program = parser_parse_program(parser);
            SemanticResult *result = semantic_analyze(program);
            EXPECT(!parser_has_error(parser));
            EXPECT(result != NULL);
            if (group == 0) {
                EXPECT(result != NULL && result->error_count == 0);
            } else {
                EXPECT(ctx_has_diagnostic_code_from_result(
                    result, PGY_CODE_SEM_IMMUTABLE_BINDING_WRITE));
            }
            semantic_result_destroy(result);
            ast_destroy(program);
            parser_destroy(parser);
            lexer_destroy(lexer);
        }
    }
}
