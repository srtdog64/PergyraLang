/* Generic call-site binding (registry row mir.generic_specialization): the
 * checker binds each generic parameter structurally, refuses a conflicting
 * or missing binding with a code, and seals the binding on the call in MIR
 * type grammar. `sealed` lists the sealed type arguments, '|'-joined, of the
 * call that initializes let statement `statement` of function `host`. */
static bool
generic_call_binding_sealed_matches(ASTNode *program, const char *host,
                                    size_t statement, const char *sealed)
{
    char joined[256];
    size_t used = 0;
    ASTNode *call = NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *decl = ast_program_statement(program, i);
        const char *name = decl != NULL && decl->type == AST_FUNC_DECL
            ? ast_declaration_name(decl) : NULL;
        if (name != NULL && strcmp(name, host) == 0)
            call = ast_let_initializer(
                ast_block_statement(ast_func_body(decl), statement));
    }
    if (call == NULL || call->type != AST_CALL)
        return false;
    joined[0] = '\0';
    for (size_t i = 0; i < ast_call_semantic_generic_arg_count(call); i++) {
        int written = snprintf(joined + used, sizeof(joined) - used, "%s%s",
            i > 0 ? "|" : "", ast_call_semantic_generic_arg_type_name(call, i));
        if (written < 0 || (size_t)written >= sizeof(joined) - used)
            return false;
        used += (size_t)written;
    }
    return strcmp(joined, sealed) == 0;
}

static void
test_generic_call_binding(void)
{
    static const struct {
        const char *name;
        const char *source;
        const char *code;       /* NULL: accepted */
        const char *diagnostic;
        const char *host;
        size_t statement;
        const char *sealed;
    } cases[] = {
        {"Option<T> parameter binds T from Some(5)",
         "func TakeOpt<T>(o: Option<T>) -> Int { return 0; }"
         "func Main() -> Void { let n: Int = TakeOpt(Some(5)); }",
         NULL, NULL, "Main", 0, "Int"},
        {"Result<T, E> parameter binds both parameters",
         "func Good<T, E>(r: Result<T, E>) -> Bool { return IsOk(r); }"
         "func Main() -> Void { let r: Result<Int, String> = Ok(3); let b: Bool = Good(r); }",
         NULL, NULL, "Main", 1, "Int|String"},
        {"a constructed binding is sealed in MIR type grammar",
         "func Has<T>(o: Option<T>) -> Bool { return IsSome(o); }"
         "func Main() -> Void { let r: Result<Int, String> = Ok(2); let b: Bool = Has(Some(r)); }",
         NULL, NULL, "Main", 1, "Result<Int,String>"},
        {"Array<T> parameter binds from an Array<Option<Int>> argument",
         "func Count<T>(xs: Array<T>) -> Int { return ArrayLength(xs); }"
         "func Main() -> Void { let xs: Array<Option<Int>> = [Some(1)]; let n: Int = Count(xs); }",
         NULL, NULL, "Main", 1, "Option<Int>"},
        {"generic struct parameter binds T",
         "struct Crate<T> { let item: T; }"
         "func Open<T>(c: Crate<T>) -> T { return c.item; }"
         "func Main() -> Void { let c: Crate<Int> = Crate(4); let n: Int = Open(c); }",
         NULL, NULL, "Main", 1, "Int"},
        {"tuple parameter binds each element parameter",
         "func Fst<A, B>(p: (A, B)) -> Int { return 1; }"
         "func Main() -> Void { let p: (Int, String) = (1, \"x\"); let n: Int = Fst(p); }",
         NULL, NULL, "Main", 1, "Int|String"},
        {"a generic caller forwards its own formal",
         "func Count<T>(xs: Array<T>) -> Int { return ArrayLength(xs); }"
         "func Relay<U>(xs: Array<U>) -> Int { let n: Int = Count(xs); return n; }",
         NULL, NULL, "Relay", 0, "U"},
        {"explicit type argument is sealed",
         "func Keep<T>(v: T) -> T { return v; }"
         "func Main() -> Void { let n: Int = Keep<Int>(7); }",
         NULL, NULL, "Main", 0, "Int"},
        {"default type argument is sealed",
         "func Make<T = Int>(n: Int) -> Int { return n; }"
         "func Main() -> Void { let n: Int = Make(3); }",
         NULL, NULL, "Main", 0, "Int"},
        {"where-clause is checked against a nested binding",
         "ability Sortable {} subject Card { let value: Int; }"
         "role CardContract for Card { impl ability Sortable {} }"
         "func Peek<T>(o: Option<T>) -> Int where T: Sortable { return 1; }"
         "func Main() -> Void { let n: Int = Peek(Some(Card(4))); }",
         NULL, NULL, "Main", 0, "Card"},
        {"nested where-clause bound failure is reported against the binding",
         "ability Sortable {} subject Card { let value: Int; }"
         "func Peek<T>(o: Option<T>) -> Int where T: Sortable { return 1; }"
         "func Main() -> Void { let n: Int = Peek(Some(Card(4))); }",
         NULL, "does not satisfy constraint 'Sortable'", NULL, 0, NULL},
        {"nested conflicting binding is refused",
         "func Both<T>(a: Option<T>, b: T) -> Int { return 1; }"
         "func Main() -> Void { let n: Int = Both(Some(1), \"x\"); }",
         PGY_CODE_SEM_TYPE_MISMATCH,
         "binds generic parameter 'T' to both 'Int' (argument 1) and 'String' (argument 2)",
         NULL, 0, NULL},
        {"conflict across two constructed parameters is refused",
         "func Join<T>(xs: Array<T>, extra: Option<T>) -> Int { return 1; }"
         "func Main() -> Void { let xs: Array<Int> = [1]; let n: Int = Join(xs, Some(\"x\")); }",
         PGY_CODE_SEM_TYPE_MISMATCH,
         "binds generic parameter 'T' to both 'Int' (argument 1) and 'String' (argument 2)",
         NULL, 0, NULL},
        {"argument conflicting with an explicit type argument is refused",
         "func Keep<T>(v: T) -> T { return v; }"
         "func Main() -> Void { let n: Int = Keep<Int>(\"seven\"); }",
         PGY_CODE_SEM_TYPE_MISMATCH,
         "binds generic parameter 'T' to 'Int' by its explicit type argument, but argument 1 gives it 'String'",
         NULL, 0, NULL},
        {"parameter no argument fixes is refused",
         "func TakeOpt<T>(o: Option<T>) -> Int { return 0; }"
         "func Main() -> Void { let n: Int = TakeOpt(None); }",
         PGY_CODE_SEM_INFER_GENERIC,
         "cannot infer generic parameter 'T' from its arguments", NULL, 0, NULL},
        {"more explicit type arguments than parameters are refused",
         "func Keep<T>(v: T) -> T { return v; }"
         "func Main() -> Void { let n: Int = Keep<Int, Bool>(7); }",
         PGY_CODE_SEM_INFER_GENERIC,
         "supplies 2 type argument(s), but 'Keep' declares 1 generic parameter(s)",
         NULL, 0, NULL},
        {"a callable cannot instantiate a generic parameter",
         "func Id<T>(x: T) -> T { return x; }"
         "func Two(n: Int) -> Int { return n; }"
         "func Main() -> Void { let f = Id(Two); }",
         PGY_CODE_SEM_INFER_GENERIC, "which cannot instantiate it", NULL, 0, NULL},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        TEST(cases[i].name);
        Lexer *lexer = lexer_create(cases[i].source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        bool accepted = result != NULL && result->error_count == 0;

        EXPECT(!parser_has_error(parser));
        if (cases[i].diagnostic == NULL) {
            EXPECT(accepted && generic_call_binding_sealed_matches(program,
                cases[i].host, cases[i].statement, cases[i].sealed));
        } else {
            EXPECT(!accepted
                && ctx_has_diagnostic_substring_from_result(result,
                    cases[i].diagnostic)
                && (cases[i].code == NULL
                    || ctx_has_diagnostic_code_from_result(result,
                        cases[i].code)));
        }
        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
