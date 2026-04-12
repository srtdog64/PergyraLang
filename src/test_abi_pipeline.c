/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * test_abi_pipeline.c — ABI Pipeline Integration Test
 *
 * PURPOSE:
 *   Validate the ABI contract through the real compiler pipeline:
 *
 *     source -> semantic -> HIR -> DIR -> RIR -> MIR -> backend -> binary
 *
 *   This complements test_abi_spec.c:
 *   - test_abi_spec.c checks physical layout and runtime type agreement
 *   - test_abi_pipeline.c checks that the compiler emits binaries that
 *     produce correct values and meet a minimal runtime performance floor
 *
 * BUILD: make test-abi
 * RUN:   ./bin/test_abi_pipeline
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <windows.h>
#define PGY_DUP _dup
#define PGY_DUP2 _dup2
#define PGY_CLOSE _close
#define PGY_FILENO _fileno
#define PGY_GETPID _getpid
#define PGY_EXEEXT ".exe"
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define PGY_DUP dup
#define PGY_DUP2 dup2
#define PGY_CLOSE close
#define PGY_FILENO fileno
#define PGY_GETPID getpid
#define PGY_EXEEXT ""
#endif

#include "compiler/driver_app.h"
#include "compiler/compiler.h"

static int g_pass = 0;
static int g_fail = 0;
static unsigned g_temp_counter = 0;

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
}

int
main(void)
{
    bool perf_mode = abi_perf_mode_enabled();
    static const char *projection_source =
        "object PlayerView {\n"
        "    hp: Int;\n"
        "    ready: Bool;\n"
        "}\n"
        "tobject PlayerPacket {\n"
        "    hp: Int;\n"
        "    mp: Int;\n"
        "}\n"
        "subject Player {\n"
        "    let hp: Int;\n"
        "    let mp: Int;\n"
        "    let ready: Bool;\n"
        "}\n"
        "func Main() -> Void {\n"
        "    let player: Player = Player(42, 7, true);\n"
        "    let view: PlayerView = ToObject(PlayerView, player);\n"
        "    let packet: PlayerPacket = ToTObject(PlayerPacket, player);\n"
        "    let maybe: Option<Int> = Some(packet.hp + packet.mp);\n"
        "    Log(view.hp);\n"
        "    Log(view.ready);\n"
        "    Log(packet.hp);\n"
        "    Log(packet.mp);\n"
        "    match maybe {\n"
        "        case .Some(v):\n"
        "            Log(v);\n"
        "        case .None:\n"
        "            Log(-1);\n"
        "    }\n"
        "}\n";
    static const char *projection_expected =
        "42\n"
        "true\n"
        "42\n"
        "7\n"
        "49\n";
    static const char *zone_projection_expected =
        "42\n"
        "true\n"
        "42\n"
        "7\n";
    static const char *zone_projection_source =
        "object PlayerView {\n"
        "    hp: Int;\n"
        "    ready: Bool;\n"
        "}\n"
        "tobject PlayerPacket {\n"
        "    hp: Int;\n"
        "    mp: Int;\n"
        "}\n"
        "subject Player {\n"
        "    let hp: Int;\n"
        "    let mp: Int;\n"
        "    let ready: Bool;\n"
        "}\n"
        "zone ArenaZone {\n"
        "    subject slot player: Player\n"
        "    object slot view: PlayerView\n"
        "    tobject slot packet: PlayerPacket\n"
        "    authority player\n"
        "    bind view from player by player\n"
        "    bind packet from player by player\n"
        "    func Snapshot() -> Void {\n"
        "        Log(view.hp);\n"
        "        Log(view.ready);\n"
        "        Log(packet.hp);\n"
        "        Log(packet.mp);\n"
        "    }\n"
        "}\n"
        "func Main() -> Void {\n"
        "    let zone: ArenaZone = ArenaZone(Player(42, 7, true));\n"
        "    zone.Snapshot();\n"
        "}\n";
    static const char *intent_source =
        "subject Buyer {\n"
        "    let hp: Int;\n"
        "    action Verify(self) -> Void { }\n"
        "}\n"
        "zone CheckoutZone {\n"
        "    subject slot buyer: Buyer\n"
        "}\n"
        "intent Charge(checkout: CheckoutZone, buyer: Buyer) {\n"
        "    step verify {\n"
        "        where: CheckoutZone;\n"
        "        using: checkout;\n"
        "        who: buyer;\n"
        "        on: buyer.Verify();\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n"
        "func Main() -> Void {\n"
        "    let buyer: Buyer = Buyer(1);\n"
        "    let checkout: CheckoutZone = CheckoutZone(buyer);\n"
        "    let ok: Bool = Charge(checkout, buyer);\n"
        "    Log(ok);\n"
        "    Log(ToString(IntentLastStepCount()));\n"
        "    Log(ToString(IntentHistoryCount()));\n"
        "    Log(ToString(IntentLastFailed()));\n"
        "    Log(ToString(IntentRecentCount()));\n"
        "    Log(IntentRecentName(0));\n"
        "    Log(ToString(IntentRecentStepCount(0)));\n"
        "    Log(ToString(IntentRecentFailed(0)));\n"
        "}\n";
    static const char *intent_expected =
        "true\n"
        "1\n"
        "1\n"
        "false\n"
        "1\n"
        "Charge\n"
        "1\n"
        "false\n";
    static const char *loop_source =
        "func Spin(limit: Int) -> Int {\n"
        "    let i: Int = 0;\n"
        "    let acc: Int = 0;\n"
        "    while i < limit {\n"
        "        acc = acc + 1;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return acc;\n"
        "}\n"
        "func Main() -> Void {\n"
        "    Log(Spin(2000000));\n"
        "}\n";
    static const char *loop_expected = "2000000\n";
    static const char *projection_medium_source =
        "object PlayerViewA { hp: Int; mp: Int; ready: Bool; x: Int; y: Int; }\n"
        "object PlayerViewB { hp: Int; mp: Int; ready: Bool; x: Int; y: Int; }\n"
        "tobject PlayerPacketA { hp: Int; mp: Int; x: Int; y: Int; }\n"
        "tobject PlayerPacketB { hp: Int; mp: Int; x: Int; y: Int; }\n"
        "subject Player {\n"
        "    let hp: Int;\n"
        "    let mp: Int;\n"
        "    let ready: Bool;\n"
        "    let x: Int;\n"
        "    let y: Int;\n"
        "}\n"
        "zone ArenaZone {\n"
        "    subject slot player: Player\n"
        "    object slot viewA: PlayerViewA\n"
        "    object slot viewB: PlayerViewB\n"
        "    tobject slot packetA: PlayerPacketA\n"
        "    tobject slot packetB: PlayerPacketB\n"
        "    authority player\n"
        "    bind viewA from player by player\n"
        "    bind viewB from player by player\n"
        "    bind packetA from player by player\n"
        "    bind packetB from player by player\n"
        "    func Snapshot() -> Void {\n"
        "        Log(viewA.hp);\n"
        "        Log(viewB.ready);\n"
        "        Log(packetA.mp);\n"
        "        Log(packetB.y);\n"
        "    }\n"
        "}\n"
        "func Main() -> Void {\n"
        "    let zone: ArenaZone = ArenaZone(Player(42, 7, true, 9, 11));\n"
        "    zone.Snapshot();\n"
        "}\n";
    static const char *projection_medium_expected =
        "42\n"
        "true\n"
        "7\n"
        "11\n";
    static const char *intent_medium_source =
        "subject Buyer {\n"
        "    let hp: Int;\n"
        "    action Verify(self) -> Void { }\n"
        "    action Reserve(self) -> Void { }\n"
        "    action Confirm(self) -> Void { }\n"
        "}\n"
        "zone CheckoutZone {\n"
        "    subject slot buyer: Buyer\n"
        "}\n"
        "intent Checkout(checkout: CheckoutZone, buyer: Buyer) {\n"
        "    step verify {\n"
        "        where: CheckoutZone;\n"
        "        using: checkout;\n"
        "        who: buyer;\n"
        "        on: buyer.Verify();\n"
        "        expect: true;\n"
        "    }\n"
        "    step reserve {\n"
        "        where: CheckoutZone;\n"
        "        using: checkout;\n"
        "        who: buyer;\n"
        "        on: buyer.Reserve();\n"
        "        expect: true;\n"
        "    }\n"
        "    step confirm {\n"
        "        where: CheckoutZone;\n"
        "        using: checkout;\n"
        "        who: buyer;\n"
        "        on: buyer.Confirm();\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n"
        "func Main() -> Void {\n"
        "    let buyer: Buyer = Buyer(1);\n"
        "    let checkout: CheckoutZone = CheckoutZone(buyer);\n"
        "    let ok: Bool = Checkout(checkout, buyer);\n"
        "    Log(ok);\n"
        "    Log(ToString(IntentLastStepCount()));\n"
        "    Log(ToString(IntentHistoryCount()));\n"
        "}\n";
    static const char *intent_medium_expected =
        "true\n"
        "3\n"
        "3\n";
    static const char *rollback_medium_source =
        "subject Buyer {\n"
        "    let hp: Int;\n"
        "    action Reserve(self) -> Void { }\n"
        "    action Charge(self) -> Void { }\n"
        "    action RollbackReserve(self) -> Void { }\n"
        "    action RollbackCharge(self) -> Void { }\n"
        "}\n"
        "zone CheckoutZone {\n"
        "    subject slot buyer: Buyer\n"
        "}\n"
        "intent Purchase(checkout: CheckoutZone, buyer: Buyer) {\n"
        "    rollback: full;\n"
        "    step reserve {\n"
        "        where: CheckoutZone;\n"
        "        using: checkout;\n"
        "        who: buyer;\n"
        "        on: buyer.Reserve();\n"
        "        compensate: buyer.RollbackReserve();\n"
        "        expect: true;\n"
        "    }\n"
        "    step charge {\n"
        "        where: CheckoutZone;\n"
        "        using: checkout;\n"
        "        who: buyer;\n"
        "        on: buyer.Charge();\n"
        "        compensate: buyer.RollbackCharge();\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n"
        "func Main() -> Void {\n"
        "    let buyer: Buyer = Buyer(1);\n"
        "    let checkout: CheckoutZone = CheckoutZone(buyer);\n"
        "    let ok: Bool = Purchase(checkout, buyer);\n"
        "    Log(ok);\n"
        "    Log(ToString(IntentLastStepCount()));\n"
        "    Log(ToString(IntentHistoryCount()));\n"
        "}\n";
    static const char *rollback_medium_expected =
        "true\n"
        "2\n"
        "2\n";
    static const char *transfer_medium_source =
        "subject Buyer {\n"
        "    let hp: Int;\n"
        "    action Promote(self) -> Void { }\n"
        "    action Finalize(self) -> Void { }\n"
        "}\n"
        "zone CartZone {\n"
        "    subject slot buyer: Buyer\n"
        "}\n"
        "zone PaymentZone {\n"
        "    subject slot buyer: Buyer\n"
        "}\n"
        "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
        "    step promote {\n"
        "        where: PaymentZone;\n"
        "        using: payment;\n"
        "        transfer: cart -> payment;\n"
        "        who: buyer;\n"
        "        on: buyer.Promote();\n"
        "        expect: true;\n"
        "    }\n"
        "    step finalize {\n"
        "        where: PaymentZone;\n"
        "        using: payment;\n"
        "        who: buyer;\n"
        "        on: buyer.Finalize();\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n"
        "func Main() -> Void {\n"
        "    let buyer: Buyer = Buyer(1);\n"
        "    let cart: CartZone = CartZone(buyer);\n"
        "    let payment: PaymentZone = PaymentZone(buyer);\n"
        "    let ok: Bool = Checkout(cart, payment, buyer);\n"
        "    Log(ok);\n"
        "    Log(ToString(IntentLastStepCount()));\n"
        "    Log(ToString(IntentHistoryCount()));\n"
        "}\n";
    static const char *transfer_medium_expected =
        "true\n"
        "2\n"
        "2\n";

    printf("=== Pergyra ABI Pipeline Integration ===\n");
    printf("This test validates end-to-end compiler output, runtime values,\n");
    printf("and a minimal runtime performance floor.\n\n");
    if (perf_mode)
        printf("Benchmark mode: hard upper bounds disabled, medium workloads enabled.\n\n");

    printf("[C backend]\n");
    run_pipeline_case("projection_abi", projection_source, projection_expected,
                      "0 error(s), 2 warning(s)",
                      BACKEND_C, !perf_mode, 30.0, 5.0);
    run_pipeline_case("zone_projection_abi", zone_projection_source, zone_projection_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_C, !perf_mode, 30.0, 5.0);
    run_pipeline_case("intent_trace_abi", intent_source, intent_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_C, !perf_mode, 30.0, 5.0);
    run_pipeline_case("runtime_floor", loop_source, loop_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_C, !perf_mode, 30.0, 5.0);
    if (perf_mode) {
        run_pipeline_case("projection_medium", projection_medium_source, projection_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_C, false, 60.0, 10.0);
        run_pipeline_case("intent_medium", intent_medium_source, intent_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_C, false, 60.0, 10.0);
        run_pipeline_case("rollback_medium", rollback_medium_source, rollback_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_C, false, 60.0, 10.0);
        run_pipeline_case("transfer_medium", transfer_medium_source, transfer_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_C, false, 60.0, 10.0);
    }

#ifdef PGY_LLVM_ENABLED
    printf("\n[LLVM backend]\n");
    run_pipeline_case("projection_abi", projection_source, projection_expected,
                      "0 error(s), 2 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("zone_projection_abi", zone_projection_source, zone_projection_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("intent_trace_abi", intent_source, intent_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    run_pipeline_case("runtime_floor", loop_source, loop_expected,
                      "0 error(s), 0 warning(s)",
                      BACKEND_LLVM, !perf_mode, 45.0, 5.0);
    if (perf_mode) {
        run_pipeline_case("projection_medium", projection_medium_source, projection_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_LLVM, false, 90.0, 10.0);
        run_pipeline_case("intent_medium", intent_medium_source, intent_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_LLVM, false, 90.0, 10.0);
        run_pipeline_case("rollback_medium", rollback_medium_source, rollback_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_LLVM, false, 90.0, 10.0);
        run_pipeline_case("transfer_medium", transfer_medium_source, transfer_medium_expected,
                          "0 error(s), 0 warning(s)",
                          BACKEND_LLVM, false, 90.0, 10.0);
    }
#endif

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
