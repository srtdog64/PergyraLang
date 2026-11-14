/* Only observe the two actual writer arguments. Other runtime frees retain
 * their ordinary behavior; this is not a second ownership implementation. */
#include <stdio.h>
#include <stdlib.h>

static void *watched[2];
static unsigned retired[2];
static unsigned empty[2];
static unsigned count;
static int active = -1;

static void pgy_json_verify(void)
{
    unsigned empty_frees = 0, text_frees = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (empty[i]) empty_frees += retired[i];
        else text_frees += retired[i];
    }
    if (count != 2 || active != -1 || empty_frees != 1 || text_frees != 1) {
        fprintf(stderr,
                "observed_fragment_free_mismatch watches=%u empty=%u text=%u\n",
                count, empty_frees, text_frees);
        _Exit(77);
    }
    puts("owned-fragment-free: empty=1 text=1");
}

void pgy_json_watch(void *pointer)
{
    if (pointer == NULL || active != -1 || count >= 2 ||
        (count == 1 && pointer == watched[0])) {
        fputs("invalid owned-fragment watch\n", stderr);
        _Exit(78);
    }
    if (count == 0 && atexit(pgy_json_verify) != 0) {
        fputs("observer atexit registration failed\n", stderr);
        _Exit(79);
    }
    watched[count] = pointer;
    empty[count] = ((const char *)pointer)[0] == '\0';
    active = (int)count;
    ++count;
}

void pgy_json_writer_returned(void)
{
    if (active < 0) {
        fputs("writer returned without an argument watch\n", stderr);
        _Exit(80);
    }
    active = -1;
}

void pgy_json_observed_free(void *pointer)
{
    // A later array backing may reuse a retired String's address. Count only
    // frees of this invocation's argument while the real writer is running.
    if (active >= 0 && pointer == watched[active]) ++retired[active];
    free(pointer);
}
