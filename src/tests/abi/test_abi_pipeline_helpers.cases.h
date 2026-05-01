static char *
pgy_strdup_local(const char *text)
{
    size_t len;
    char *copy;

    if (text == NULL)
        return NULL;

    len = strlen(text);
    copy = (char *)malloc(len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static void
pgy_setenv_local(const char *name, const char *value)
{
    if (name == NULL || name[0] == '\0')
        return;
#ifdef _WIN32
    SetEnvironmentVariableA(name, value);
#else
    if (value == NULL)
        unsetenv(name);
    else
        setenv(name, value, 1);
#endif
}

static void
abi_expect(const char *name, bool cond)
{
    printf("  %-70s", name);
    if (cond) {
        printf("PASS\n");
        g_pass++;
    } else {
        printf("FAIL\n");
        g_fail++;
    }
}

static double
pgy_now_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
#endif
}

static const char *
pgy_test_tmpdir(void)
{
    const char *tmpdir = getenv("TMPDIR");

    if (tmpdir == NULL)
        tmpdir = getenv("TMP");
    if (tmpdir == NULL)
        tmpdir = getenv("TEMP");
#ifdef _WIN32
    if (tmpdir == NULL)
        tmpdir = ".";
#else
    if (tmpdir == NULL)
        tmpdir = "/tmp";
#endif
    return tmpdir;
}

static void
pgy_make_temp_paths(const char *stem,
                    char *source_path, size_t source_cap,
                    char *binary_path, size_t binary_cap,
                    char *capture_path, size_t capture_cap)
{
    const char *tmpdir = pgy_test_tmpdir();
    unsigned counter = ++g_temp_counter;
    unsigned pid = (unsigned)PGY_GETPID();

    snprintf(source_path, source_cap, "%s/%s_%u_%u.pgy",
             tmpdir, stem, pid, counter);
    snprintf(binary_path, binary_cap, "%s/%s_%u_%u%s",
             tmpdir, stem, pid, counter, PGY_EXEEXT);
    snprintf(capture_path, capture_cap, "%s/%s_%u_%u.out",
             tmpdir, stem, pid, counter);
}

static bool
write_text_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    size_t len;
    size_t written;

    if (fp == NULL)
        return false;

    len = strlen(text);
    written = fwrite(text, 1, len, fp);
    fclose(fp);
    return written == len;
}

static char *
read_text_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long size;
    char *buf;
    size_t read_len;

    if (fp == NULL)
        return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buf = (char *)malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }

    read_len = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    buf[read_len] = '\0';
    return buf;
}

static char *
normalize_newlines(const char *text)
{
    size_t len;
    char *buf;
    size_t read_i;
    size_t write_i = 0;

    if (text == NULL)
        return NULL;

    len = strlen(text);
    buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return NULL;

    for (read_i = 0; read_i < len; read_i++) {
        if (text[read_i] == '\r') {
            if ((read_i + 1) < len && text[read_i + 1] == '\n')
                continue;
            buf[write_i++] = '\n';
            continue;
        }
        buf[write_i++] = text[read_i];
    }

    buf[write_i] = '\0';
    return buf;
}

static int
capture_binary_output(const char *binary_path,
                      const char *capture_path,
                      double *run_seconds)
{
    int saved_stdout;
    FILE *capture;
    int rc;
    double start;
    double end;

    fflush(stdout);
    saved_stdout = PGY_DUP(PGY_FILENO(stdout));
    if (saved_stdout < 0)
        return -1;

    capture = fopen(capture_path, "wb");
    if (capture == NULL) {
        PGY_CLOSE(saved_stdout);
        return -1;
    }

    if (PGY_DUP2(PGY_FILENO(capture), PGY_FILENO(stdout)) < 0) {
        fclose(capture);
        PGY_CLOSE(saved_stdout);
        return -1;
    }

    start = pgy_now_seconds();
    rc = compiler_run_binary(binary_path, false);
    end = pgy_now_seconds();
    if (run_seconds != NULL)
        *run_seconds = end - start;

    fflush(stdout);
    PGY_DUP2(saved_stdout, PGY_FILENO(stdout));
    PGY_CLOSE(saved_stdout);
    fclose(capture);
    return rc;
}

static int
capture_driver_output(const DriverFlags *flags,
                      const char *capture_path,
                      DriverPhaseTimings *timings,
                      double *compile_seconds)
{
#ifdef _WIN32
    int saved_stdout;
    int saved_stderr;
    FILE *capture;
    int rc;
    double start;
    double end;

    fflush(stdout);
    fflush(stderr);
    saved_stdout = PGY_DUP(PGY_FILENO(stdout));
    saved_stderr = PGY_DUP(PGY_FILENO(stderr));
    if (saved_stdout < 0 || saved_stderr < 0) {
        if (saved_stdout >= 0)
            PGY_CLOSE(saved_stdout);
        if (saved_stderr >= 0)
            PGY_CLOSE(saved_stderr);
        return -1;
    }

    capture = fopen(capture_path, "wb");
    if (capture == NULL) {
        PGY_CLOSE(saved_stdout);
        PGY_CLOSE(saved_stderr);
        return -1;
    }

    if (PGY_DUP2(PGY_FILENO(capture), PGY_FILENO(stdout)) < 0
        || PGY_DUP2(PGY_FILENO(capture), PGY_FILENO(stderr)) < 0) {
        fclose(capture);
        PGY_CLOSE(saved_stdout);
        PGY_CLOSE(saved_stderr);
        return -1;
    }

    start = pgy_now_seconds();
    rc = driver_run_pipeline_timed(flags, timings);
    end = pgy_now_seconds();
    if (compile_seconds != NULL)
        *compile_seconds = end - start;

    fflush(stdout);
    fflush(stderr);
    PGY_DUP2(saved_stdout, PGY_FILENO(stdout));
    PGY_DUP2(saved_stderr, PGY_FILENO(stderr));
    PGY_CLOSE(saved_stdout);
    PGY_CLOSE(saved_stderr);
    fclose(capture);
    return rc;
#else
    const char *same_process_env = getenv("PGY_ABI_PIPELINE_SAME_PROCESS");
    if (same_process_env != NULL && same_process_env[0] != '\0'
        && strcmp(same_process_env, "0") != 0) {
        int saved_stdout;
        int saved_stderr;
        FILE *capture;
        int rc;
        double start;
        double end;

        fflush(stdout);
        fflush(stderr);
        saved_stdout = PGY_DUP(PGY_FILENO(stdout));
        saved_stderr = PGY_DUP(PGY_FILENO(stderr));
        if (saved_stdout < 0 || saved_stderr < 0) {
            if (saved_stdout >= 0)
                PGY_CLOSE(saved_stdout);
            if (saved_stderr >= 0)
                PGY_CLOSE(saved_stderr);
            return -1;
        }

        capture = fopen(capture_path, "wb");
        if (capture == NULL) {
            PGY_CLOSE(saved_stdout);
            PGY_CLOSE(saved_stderr);
            return -1;
        }

        if (PGY_DUP2(PGY_FILENO(capture), PGY_FILENO(stdout)) < 0
            || PGY_DUP2(PGY_FILENO(capture), PGY_FILENO(stderr)) < 0) {
            fclose(capture);
            PGY_CLOSE(saved_stdout);
            PGY_CLOSE(saved_stderr);
            return -1;
        }

        start = pgy_now_seconds();
        rc = driver_run_pipeline_timed(flags, timings);
        end = pgy_now_seconds();
        if (compile_seconds != NULL)
            *compile_seconds = end - start;

        fflush(stdout);
        fflush(stderr);
        PGY_DUP2(saved_stdout, PGY_FILENO(stdout));
        PGY_DUP2(saved_stderr, PGY_FILENO(stderr));
        PGY_CLOSE(saved_stdout);
        PGY_CLOSE(saved_stderr);
        fclose(capture);
        return rc;
    }

    FILE *capture;
    pid_t pid;
    int status = 0;
    double start;
    double end;

    if (timings != NULL)
        memset(timings, 0, sizeof(*timings));

    fflush(stdout);
    fflush(stderr);

    capture = fopen(capture_path, "wb");
    if (capture == NULL)
        return -1;

    start = pgy_now_seconds();
    pid = fork();
    if (pid < 0) {
        fclose(capture);
        return -1;
    }

    if (pid == 0) {
        int fd = PGY_FILENO(capture);
        int rc;

        if (PGY_DUP2(fd, PGY_FILENO(stdout)) < 0
            || PGY_DUP2(fd, PGY_FILENO(stderr)) < 0) {
            _exit(127);
        }
        fclose(capture);
        rc = driver_run_pipeline_timed(flags, NULL);
        fflush(stdout);
        fflush(stderr);
        _exit(rc);
    }

    fclose(capture);
    waitpid(pid, &status, 0);
    end = pgy_now_seconds();
    if (compile_seconds != NULL)
        *compile_seconds = end - start;

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
#endif
}

static void
abi_info(const char *name, const char *value)
{
    printf("  %-70s%s\n", name, value != NULL ? value : "");
}

static void
remove_if_exists(const char *path)
{
    if (path != NULL)
        remove(path);
}

static const char *
backend_name(BackendKind backend)
{
    return backend == BACKEND_LLVM ? "llvm" : "c";
}

static bool
abi_perf_mode_enabled(void)
{
    const char *value = getenv("PGY_ABI_PERF_MODE");

    return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

static void
print_phase_timings(const DriverPhaseTimings *timings)
{
    if (timings == NULL)
        return;

    printf("    phases: load=%.1fms sem=%.1fms hir=%.1fms dir=%.1fms dirv=%.1fms\n",
           timings->module_load * 1000.0, timings->semantic * 1000.0,
           timings->hir_lower * 1000.0, timings->dir_lower * 1000.0,
           timings->dir_validate * 1000.0);
    printf("            rir=%.1fms enrich=%.1fms rirv=%.1fms rir+dir=%.1fms mir=%.1fms mirv=%.1fms backend=%.1fms total=%.1fms\n",
           timings->rir_lower * 1000.0, timings->rir_enrich * 1000.0,
           timings->rir_validate * 1000.0, timings->rir_dir_validate * 1000.0,
           timings->mir_lower * 1000.0, timings->mir_validate * 1000.0,
           timings->backend * 1000.0, timings->total * 1000.0);
    printf("            backend_split: codegen=%.1fms compile=%.1fms link=%.1fms\n",
           timings->backend_codegen * 1000.0,
           timings->backend_native_compile * 1000.0,
           timings->backend_link * 1000.0);
}

static void
run_pipeline_case(const char *case_name,
                  const char *source,
                  const char *expected_output,
                  const char *expected_compile_output,
                  BackendKind backend,
                  bool enforce_thresholds,
                  double max_compile_seconds,
                  double max_run_seconds)
{
    const char *backend_label = backend_name(backend);
    char source_path[1024];
    char binary_path[1024];
    char compile_capture_path[1024];
    char capture_path[1024];
    DriverFlags flags;
    DriverPhaseTimings timings;
    double compile_seconds = 0.0;
    double run_seconds = 0.0;
    int compile_rc;
    int run_rc = -1;
    char *captured = NULL;
    char *normalized_captured = NULL;
    char *normalized_expected = NULL;
    char *compile_captured = NULL;
    char msg[256];

    if (g_case_filter != NULL && g_case_filter[0] != '\0'
        && strcmp(g_case_filter, case_name) != 0) {
        return;
    }
    if (g_case_start != NULL && g_case_start[0] != '\0' && !g_case_window_open) {
        if (strcmp(g_case_start, case_name) != 0)
            return;
        g_case_window_open = true;
    }
    if (g_backend_filter != NULL && g_backend_filter[0] != '\0'
        && strcmp(g_backend_filter, backend_label) != 0) {
        return;
    }

    pgy_make_temp_paths(case_name, source_path, sizeof(source_path),
                        binary_path, sizeof(binary_path),
                        capture_path, sizeof(capture_path));
    snprintf(compile_capture_path, sizeof(compile_capture_path), "%s",
             capture_path);
    if (strlen(compile_capture_path) + strlen(".compile")
        < sizeof(compile_capture_path)) {
        strcat(compile_capture_path, ".compile");
    }

    snprintf(msg, sizeof(msg), "%s/%s: wrote test source",
             backend_name(backend), case_name);
    abi_expect(msg, write_text_file(source_path, source));

    memset(&flags, 0, sizeof(flags));
    flags.source_path = source_path;
    flags.output_path = binary_path;
    flags.backend = backend;
    flags.opt_profile = PGY_OPT_RELEASE;

    memset(&timings, 0, sizeof(timings));
    compile_rc = capture_driver_output(&flags, compile_capture_path, &timings,
                                       &compile_seconds);

    snprintf(msg, sizeof(msg), "%s/%s: compiler pipeline succeeds",
             backend_name(backend), case_name);
    abi_expect(msg, compile_rc == 0);

    compile_captured = read_text_file(compile_capture_path);
    snprintf(msg, sizeof(msg), "%s/%s: expected compile diagnostics appear",
             backend_name(backend), case_name);
    abi_expect(msg, expected_compile_output == NULL
        || (compile_captured != NULL
            && strstr(compile_captured, expected_compile_output) != NULL));
    if (compile_rc != 0) {
        printf("    compile_rc=%d\n", compile_rc);
        if (compile_captured != NULL && compile_captured[0] != '\0')
            printf("    compile output:\n%s\n", compile_captured);
    }

    if (enforce_thresholds) {
        snprintf(msg, sizeof(msg), "%s/%s: compiler latency <= %.1fs",
                 backend_name(backend), case_name, max_compile_seconds);
        abi_expect(msg, compile_rc == 0 && compile_seconds <= max_compile_seconds);
    } else {
        snprintf(msg, sizeof(msg), "%s/%s: compiler latency benchmark-only",
                 backend_name(backend), case_name);
        abi_info(msg, "INFO");
    }

    if (compile_rc == 0) {
        run_rc = capture_binary_output(binary_path, capture_path, &run_seconds);

        snprintf(msg, sizeof(msg), "%s/%s: binary exits with code 0",
                 backend_name(backend), case_name);
        abi_expect(msg, run_rc == 0);

        if (enforce_thresholds) {
            snprintf(msg, sizeof(msg), "%s/%s: runtime <= %.1fs",
                     backend_name(backend), case_name, max_run_seconds);
            abi_expect(msg, run_rc == 0 && run_seconds <= max_run_seconds);
        } else {
            snprintf(msg, sizeof(msg), "%s/%s: runtime benchmark-only",
                     backend_name(backend), case_name);
            abi_info(msg, "INFO");
        }

        captured = read_text_file(capture_path);
        snprintf(msg, sizeof(msg), "%s/%s: expected output appears",
                 backend_name(backend), case_name);
        {
            bool output_ok;

            normalized_captured = normalize_newlines(captured);
            normalized_expected = normalize_newlines(expected_output);
            output_ok = normalized_captured != NULL
                && normalized_expected != NULL
                && strstr(normalized_captured, normalized_expected) != NULL;
            abi_expect(msg, output_ok);
            if (!output_ok && captured != NULL) {
                printf("    captured stdout:\n%s\n", captured);
            }
        }
    }

    printf("    metrics: compile=%.3fs run=%.3fs\n", compile_seconds, run_seconds);
    print_phase_timings(&timings);

    free(compile_captured);
    free(normalized_expected);
    free(normalized_captured);
    free(captured);
    if (compile_rc == 0 && run_rc == 0 && g_fail == 0) {
        remove_if_exists(compile_capture_path);
        remove_if_exists(capture_path);
        remove_if_exists(binary_path);
        remove_if_exists(source_path);
    }
    if (g_case_stop != NULL && g_case_stop[0] != '\0'
        && strcmp(g_case_stop, case_name) == 0) {
        g_case_window_open = false;
    }
}

static void
run_same_process_repeat_case(const char *case_name_prefix,
                             const char *source,
                             const char *expected_output,
                             const char *expected_compile_output,
                             BackendKind backend,
                             bool enforce_thresholds,
                             double max_compile_seconds,
                             double max_run_seconds,
                             int repeat_count)
{
    char *saved_same_process;

    if (repeat_count <= 0)
        return;

    saved_same_process = pgy_strdup_local(getenv("PGY_ABI_PIPELINE_SAME_PROCESS"));
    pgy_setenv_local("PGY_ABI_PIPELINE_SAME_PROCESS", "1");

    for (int i = 0; i < repeat_count; i++) {
        char repeated_case_name[256];

        snprintf(repeated_case_name, sizeof(repeated_case_name),
                 "%s_same_process_%d", case_name_prefix, i + 1);
        run_pipeline_case(repeated_case_name, source, expected_output,
                          expected_compile_output, backend,
                          enforce_thresholds, max_compile_seconds,
                          max_run_seconds);
    }

    pgy_setenv_local("PGY_ABI_PIPELINE_SAME_PROCESS", saved_same_process);
    free(saved_same_process);
}
