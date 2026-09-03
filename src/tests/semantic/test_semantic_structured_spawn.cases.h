static bool
structured_spawn_source_matches(const char *source, bool expect_lifecycle_error)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticResult *result = NULL;
    bool matched = false;

    if (!parser_has_error(parser))
        result = semantic_analyze(program);

    if (result != NULL) {
        bool has_lifecycle_error = result->error_count > 0
            && ctx_has_diagnostic_code_from_result(
                result, PGY_CODE_SEM_TASK_LIFECYCLE);
        matched = expect_lifecycle_error
            ? has_lifecycle_error
            : result->error_count == 0;
        if (!matched) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                Diagnostic *diag = result->diagnostics[i];
                if (diag != NULL && diag->message != NULL)
                    fprintf(stderr, "structured-spawn diagnostic: %s\n",
                            diag->message);
            }
        }
    }

    semantic_result_destroy(result);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return matched;
}

static bool
structured_spawn_source_is_rejected(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticResult *result = NULL;
    bool rejected = false;

    if (!parser_has_error(parser))
        result = semantic_analyze(program);
    rejected = result != NULL && result->error_count > 0;

    semantic_result_destroy(result);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return rejected;
}

static void
test_structured_spawn_lifecycle(void)
{
    printf("\n[structured_spawn_lifecycle]\n");

    TEST("joined spawn retires before function fallthrough");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  let value: Int = await pending;\n"
        "  Log(value);\n"
        "}\n", false));

    TEST("cancel request followed by await retires spawn");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  Log(Cancel(pending));\n"
        "  let value: Int = await pending;\n"
        "  Log(value);\n"
        "}\n", false));

    TEST("immediate await owns anonymous spawn handle");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let value: Int = await spawn Worker();\n"
        "  Log(value);\n"
        "}\n", false));

    TEST("both if branches retire the same spawn");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main(flag: Bool) -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  if flag { Log(await pending); } else { Log(await pending); }\n"
        "}\n", false));

    TEST("one parallel arm may retire an outer spawn because every arm runs");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  parallel {\n"
        "    { Log(await pending); }\n"
        "    { Log(2); }\n"
        "  }\n"
        "}\n", false));

    TEST("static true if excludes its impossible fallthrough path");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  if true { Log(await pending); }\n"
        "}\n", false));

    TEST("static false if excludes its impossible then path");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  if false { Log(0); } else { Log(await pending); }\n"
        "}\n", false));

    TEST("static unreachable return does not create a lifecycle exit");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  if false { return; }\n"
        "  Log(await pending);\n"
        "}\n", false));

    TEST("static unreachable break does not create a loop exit");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  while true { if false { break; } }\n"
        "}\n", false));

    TEST("known single-iteration range excludes zero-iteration state");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  for i in 0..1 { Log(i + await pending); }\n"
        "}\n", false));

    TEST("known single-iteration continue reaches the loop exit state");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  for i in 0..1 { Log(i + await pending); continue; }\n"
        "}\n", false));

    TEST("known zero-iteration range does not consume the body state");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  for i in 0..0 { Log(i + await pending); }\n"
        "  Log(await pending);\n"
        "}\n", false));

    TEST("static true while with break has no zero-iteration state");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  while true { Log(await pending); break; }\n"
        "}\n", false));

    TEST("static match excludes non-selected literal arms");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  match 1 { case 1: Log(await pending); default: Log(0); }\n"
        "}\n", false));

    TEST("static match ignores return exits in impossible arms");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  match 1 {\n"
        "    case 2: return;\n"
        "    case 1: Log(await pending);\n"
        "    default: return;\n"
        "  }\n"
        "}\n", false));

    TEST("static match ignores loop control in impossible arms");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  while true {\n"
        "    match 1 { case 2: break; case 1: continue; default: break; }\n"
        "  }\n"
        "}\n", false));

    TEST("two parallel arms cannot retire the same spawn handle");
    EXPECT(structured_spawn_source_is_rejected(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  parallel {\n"
        "    { Log(await pending); }\n"
        "    { Log(await pending); }\n"
        "  }\n"
        "}\n"));

    TEST("own Future transfer retires the caller handle");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func RetireTask(own task: Future<Int>) -> Void { Log(await task); }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  RetireTask(pending);\n"
        "}\n", false));

    TEST("own RemoteFuture transfer retires the caller handle");
    EXPECT(structured_spawn_source_matches(
        "async func RetireRemote(own task: RemoteFuture<Int>) -> Void {\n"
        "  let result: Result<Int> = await task;\n"
        "  Log(Unwrap(result));\n"
        "}\n"
        "async func Main() -> Void {\n"
        "  let dev: DeviceSlot<Int> = ClaimDeviceSlot();\n"
        "  DeviceWrite(dev, 11);\n"
        "  let pending: RemoteFuture<Int> = SubmitDeviceRead(dev);\n"
        "  RetireRemote(pending);\n"
        "  ReleaseDeviceSlot(dev);\n"
        "}\n", false));

    TEST("same ABI parameter name preserves distinct Future kinds");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 3; }\n"
        "async func RetireLocal(own task: Future<Int>) -> Void { Log(await task); }\n"
        "async func RetireRemote(own task: RemoteFuture<Int>) -> Void {\n"
        "  let result: Result<Int> = await task; Log(Unwrap(result));\n"
        "}\n"
        "async func Main() -> Void {\n"
        "  let local_pending: Future<Int> = spawn Worker();\n"
        "  let dev: DeviceSlot<Int> = ClaimDeviceSlot();\n"
        "  DeviceWrite(dev, 11);\n"
        "  let remote_pending: RemoteFuture<Int> = SubmitDeviceRead(dev);\n"
        "  RetireLocal(local_pending); RetireRemote(remote_pending); ReleaseDeviceSlot(dev);\n"
        "}\n", false));

    TEST("own Future parameter must retire before callee fallthrough");
    EXPECT(structured_spawn_source_matches(
        "async func DropTask(own task: Future<Int>) -> Void { Log(0); }\n",
        true));

    TEST("own RemoteFuture parameter must retire before callee fallthrough");
    EXPECT(structured_spawn_source_matches(
        "async func DropRemote(own task: RemoteFuture<Int>) -> Void { Log(0); }\n",
        true));

    TEST("own Future transfer rejects caller reuse");
    EXPECT(structured_spawn_source_is_rejected(
        "async func Worker() -> Int { return 1; }\n"
        "async func RetireTask(own task: Future<Int>) -> Void { Log(await task); }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  RetireTask(pending);\n"
        "  Log(await pending);\n"
        "}\n"));

    TEST("Cancel does not mask use after own transfer with a signature error");
    EXPECT(structured_spawn_source_is_rejected(
        "async func Worker() -> Int { return 1; }\n"
        "async func RetireTask(own task: Future<Int>) -> Void { Log(await task); }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  RetireTask(pending);\n"
        "  Log(Cancel(pending));\n"
        "}\n"));

    TEST("second own transfer does not add a boundary mismatch cascade");
    EXPECT(structured_spawn_source_is_rejected(
        "async func Worker() -> Int { return 1; }\n"
        "async func RetireTask(own task: Future<Int>) -> Void { Log(await task); }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  RetireTask(pending); RetireTask(pending);\n"
        "}\n"));

    TEST("repeated divergent Future use emits no type cascade");
    EXPECT(structured_spawn_source_is_rejected(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main(flag: Bool) -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  if flag { Log(await pending); }\n"
        "  Log(await pending); Log(Cancel(pending));\n"
        "}\n"));

    TEST("live spawn at function fallthrough is rejected");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  Log(Cancel(pending));\n"
        "}\n", true));

    TEST("live spawn at explicit return is rejected");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main(flag: Bool) -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  if flag { return; }\n"
        "  Log(await pending);\n"
        "}\n", true));

    TEST("live spawn cannot leave its lexical block");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  if true { let pending: Future<Int> = spawn Worker(); }\n"
        "}\n", true));

    TEST("one-sided branch await leaves divergent lifecycle");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main(flag: Bool) -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  if flag { Log(await pending); }\n"
        "}\n", true));

    TEST("bare spawn expression has no lifecycle owner");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void { spawn Worker(); }\n", true));

    TEST("mutable Future binding is rejected");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let mut pending: Future<Int> = spawn Worker();\n"
        "  Log(await pending);\n"
        "}\n", true));

    TEST("Future parameter requires explicit own transfer");
    EXPECT(structured_spawn_source_matches(
        "async func RetireTask(task: Future<Int>) -> Void { Log(await task); }\n",
        true));

    TEST("Future return boundary is rejected");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Leak() -> Future<Int> { return spawn Worker(); }\n",
        true));

    TEST("Future let alias cannot hide ownership transfer");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  let alias: Future<Int> = pending;\n"
        "  Log(await alias);\n"
        "}\n", true));

    TEST("loop-local spawn joined before body exit is valid");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  let mut running: Bool = true;\n"
        "  while running {\n"
        "    let pending: Future<Int> = spawn Worker();\n"
        "    Log(await pending);\n"
        "    running = false;\n"
        "  }\n"
        "}\n", false));

    TEST("loop body cannot break with a live spawn");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main() -> Void {\n"
        "  while true {\n"
        "    let pending: Future<Int> = spawn Worker();\n"
        "    break;\n"
        "  }\n"
        "}\n", true));

    TEST("zero-iteration loop cannot conditionally retire outer spawn");
    EXPECT(structured_spawn_source_matches(
        "async func Worker() -> Int { return 1; }\n"
        "async func Main(flag: Bool) -> Void {\n"
        "  let pending: Future<Int> = spawn Worker();\n"
        "  while flag { Log(await pending); break; }\n"
        "}\n", true));
}
