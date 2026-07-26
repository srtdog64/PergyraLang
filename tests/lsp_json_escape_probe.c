#include "lsp/pgy_lsp_internal.h"

#include <stdio.h>
#include <string.h>

static int
expect_escape(const char *label, const char *input, size_t output_size,
              const char *expected)
{
    char actual[128];

    if (output_size > sizeof(actual)) {
        fprintf(stderr, "%s: invalid probe output size\n", label);
        return 1;
    }
    memset(actual, 0x7f, sizeof(actual));
    json_escape_copy(actual, output_size, input);
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n",
                label, expected, actual);
        return 1;
    }
    return 0;
}

int
main(void)
{
    static const char all_controls[] = {
        '"', '\\', '\b', '\f', '\n', '\r', '\t', 0x01, 0x1f, 'A', '\0'
    };
    static const char generic_control[] = { 0x01, '\0' };
    int failures = 0;

    failures += expect_escape(
        "json controls", all_controls, 128,
        "\\\"\\\\\\b\\f\\n\\r\\t\\u0001\\u001fA");
    failures += expect_escape("short named escape", "\n", 2, "");
    failures += expect_escape("exact named escape", "\n", 3, "\\n");
    failures += expect_escape("short unicode escape", generic_control, 6, "");
    failures += expect_escape(
        "exact unicode escape", generic_control, 7, "\\u0001");
    failures += expect_escape("prefix before truncated escape", "A\n", 3, "A");
    failures += expect_escape("plain truncation", "ABC", 3, "AB");

    return failures == 0 ? 0 : 1;
}
