#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(void)
{
    const char *mode = getenv("PGY_PUBLIC_DIAG_FIXTURE_MODE");

    if (mode != NULL && strcmp(mode, "missing") == 0)
        return 1;
    if (mode != NULL && strcmp(mode, "crosswired") == 0) {
        fputs("pgy.selfhost.other-diagnostic.v1\n[{\"wrong\":true}]\n",
              stdout);
        return 1;
    }
    fputs("pgy.selfhost.public-diagnostic.v1\n[x]\n", stdout);
    return 1;
}
