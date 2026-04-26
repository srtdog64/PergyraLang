static char *
read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    size_t read_len;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    read_len = fread(buf, 1, (size_t)len, f);
    buf[read_len] = '\0';
    fclose(f);
    return buf;
}

static bool
source_is_parseable(const char *source)
{
    Lexer *lexer;
    Parser *parser;
    ASTNode *program;
    bool ok;

    if (source == NULL)
        return false;

    lexer = lexer_create(source);
    if (lexer == NULL)
        return false;
    parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        return false;
    }

    program = parser_parse_program(parser);
    ok = !parser_has_error(parser);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return ok;
}

static char *
read_stream(FILE *f)
{
    long len;
    size_t read_len;
    char *buf;

    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0)
        return NULL;
    len = ftell(f);
    if (len < 0)
        return NULL;
    if (fseek(f, 0, SEEK_SET) != 0)
        return NULL;
    buf = malloc((size_t)len + 1);
    if (buf == NULL)
        return NULL;
    read_len = fread(buf, 1, (size_t)len, f);
    buf[read_len] = '\0';
    return buf;
}

static char *
format_source_to_string(const char *source)
{
    FILE *tmp;
    char *result;

    if (source == NULL)
        return NULL;
    tmp = tmpfile();
    if (tmp == NULL)
        return NULL;
    if (!format_source_to_stream(source, tmp)) {
        fclose(tmp);
        return NULL;
    }
    fflush(tmp);
    result = read_stream(tmp);
    fclose(tmp);
    return result;
}
