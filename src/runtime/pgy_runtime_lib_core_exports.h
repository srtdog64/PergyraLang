#ifndef PGY_RUNTIME_LIB_CORE_EXPORTS_H
#define PGY_RUNTIME_LIB_CORE_EXPORTS_H

#include "../common/string_compat.h"
#include "pgy_runtime_host_clock.h"

void pgy_log_int(int32_t v)    { printf("%d\n", v); }
void pgy_log_long(int64_t v)   { printf("%lld\n", (long long)v); }
void pgy_log_float(float v)    { printf("%f\n", v); }
void pgy_log_double(double v)  { printf("%lf\n", v); }
void pgy_log_bool(bool v)      { printf("%s\n", v ? "true" : "false"); }

void
pgy_log_string(const char *v)
{
    size_t len;

    if (v == NULL)
        v = "(null)";

    fputs(v, stdout);
    len = strlen(v);
    if (len == 0 || v[len - 1] != '\n')
        fputc('\n', stdout);
    fflush(stdout);
}

void
pgy_log_banner(const char *v)
{
    pgy_log_string(v);
}

int64_t
pgy_now_ms(void)
{
    pgy_cap_require_export(PGY_CAP_CLOCK, "now-ms");
    return pgy_host_monotonic_time(1000);
}

void
pgy_sleep_ms(int32_t ms)
{
    if (ms <= 0)
        return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)((ms % 1000) * 1000000L);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
#endif
}

char *
pgy_int_to_string(int32_t v)
{
    char *buf = pergyra_strdup_printf("%d", v);
    if (buf == NULL) {
        char *fallback = (char *)malloc(2);
        if (fallback != NULL) {
            fallback[0] = '0';
            fallback[1] = '\0';
        }
        return fallback;
    }
    return buf;
}

char *
pgy_long_to_string(int64_t v)
{
    char *buf = pergyra_strdup_printf("%lld", (long long)v);
    if (buf == NULL) {
        char *fallback = (char *)malloc(2);
        if (fallback != NULL) {
            fallback[0] = '0';
            fallback[1] = '\0';
        }
        return fallback;
    }
    return buf;
}

char *
pgy_float_to_string(float v)
{
    char *buf = pergyra_strdup_printf("%g", (double)v);
    if (buf == NULL) {
        char *fallback = (char *)malloc(4);
        if (fallback != NULL) {
            fallback[0] = '0';
            fallback[1] = '.';
            fallback[2] = '0';
            fallback[3] = '\0';
        }
        return fallback;
    }
    return buf;
}

char *
pgy_double_to_string(double v)
{
    char *buf = pergyra_strdup_printf("%g", v);
    if (buf == NULL) {
        char *fallback = (char *)malloc(4);
        if (fallback != NULL) {
            fallback[0] = '0';
            fallback[1] = '.';
            fallback[2] = '0';
            fallback[3] = '\0';
        }
        return fallback;
    }
    return buf;
}

#endif /* PGY_RUNTIME_LIB_CORE_EXPORTS_H */
