#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t allocations, fail_at;
static void *test_malloc(size_t size)
{
    allocations++;
    return fail_at != 0 && allocations == fail_at ? NULL : malloc(size);
}
static void *test_realloc(void *ptr, size_t size) { return realloc(ptr, size); }
static void test_free(void *ptr) { free(ptr); }

/* Same production-TU injection seam as hashmap_string_raw_storage_runtime.c.
   Allocation failure is a runtime panic, never a fabricated successful copy. */
#define malloc(size) test_malloc(size)
#define realloc(ptr, size) test_realloc((ptr), (size))
#define free(ptr) test_free(ptr)
#define PGY_LLVM_ENABLED
#include "runtime/pgy_runtime_lib.c"
#undef malloc
#undef realloc
#undef free

static void retire_ints(PgyArray_Array_Int *outer)
{
    for (size_t i = 0; i < outer->length; i++) free(outer->data[i].data);
    free(outer->data);
    *outer = (PgyArray_Array_Int){0};
}
static void retire_strings(PgyArray_Array_String *outer)
{
    for (size_t i = 0; i < outer->length; i++) {
        for (size_t j = 0; j < outer->data[i].length; j++) free(outer->data[i].data[j]);
        free(outer->data[i].data);
    }
    free(outer->data);
    *outer = (PgyArray_Array_String){0};
}
static PgyArray_Array_Int int_source(void)
{
    PgyArray_Int row = pgy_array_new_Int(2), empty = pgy_array_new_Int(0);
    pgy_array_push_Int(&row, 7); pgy_array_push_Int(&row, 8);
    PgyArray_Array_Int outer = pgy_array_new_Array_Int(2);
    pgy_array_push_Array_Int(&outer, row); pgy_array_push_Array_Int(&outer, empty);
    return outer;
}
static PgyArray_Array_String string_source(void)
{
    PgyArray_String row = pgy_array_new_String(2);
    pgy_array_push_String(&row, pgy_runtime_strdup_export("alpha"));
    pgy_array_push_String(&row, pgy_runtime_strdup_export("beta"));
    PgyArray_Array_String outer = pgy_array_new_Array_String(1);
    pgy_array_push_Array_String(&outer, row);
    return outer;
}
static bool independent_ints(void)
{
    PgyArray_Array_Int source = int_source();
    PgySlice_Array_Int view = {source.data, source.length};
    PgyArray_Array_Int cloned = pgy_array_clone_Array_Int(&source);
    PgyArray_Array_Int sliced = pgy_slice_copy_Array_Int(&view);
    PgyArray_Array_Int fixed = {0};
    pgy_array_clone_Array_Int_into(&source, &fixed);
    /* Stop BEFORE mutation/free if the old shallow-copy implementation aliases. */
    if (source.length != 2 || source.data == NULL || source.data[0].length != 2 ||
        source.data[0].data == NULL || source.data[0].data[0] != 7 || source.data[0].data[1] != 8 ||
        cloned.length != 2 || sliced.length != 2 || cloned.data == NULL || sliced.data == NULL ||
        cloned.data[0].length != 2 || sliced.data[0].length != 2 ||
        cloned.data[0].data == NULL || sliced.data[0].data == NULL || cloned.data == source.data ||
        sliced.data == source.data || cloned.data == sliced.data ||
        cloned.data[0].data == source.data[0].data || sliced.data[0].data == source.data[0].data ||
        cloned.data[0].data == sliced.data[0].data || cloned.data[1].length != 0 ||
        sliced.data[1].length != 0 || fixed.length != 2 || fixed.data == NULL ||
        fixed.data[0].length != 2 || fixed.data[0].data == NULL ||
        fixed.data == source.data || fixed.data == cloned.data || fixed.data == sliced.data ||
        fixed.data[0].data == source.data[0].data || fixed.data[0].data == cloned.data[0].data ||
        fixed.data[0].data == sliced.data[0].data || fixed.data[1].length != 0) return false;
    source.data[0].data[0] = 99;
    if (cloned.data[0].data[0] != 7 || sliced.data[0].data[0] != 7 || fixed.data[0].data[0] != 7) return false;
    retire_ints(&source);
    if (cloned.data[0].data[1] != 8 || sliced.data[0].data[1] != 8 || fixed.data[0].data[1] != 8) return false;
    retire_ints(&cloned);
    bool ready = sliced.data[0].data[0] == 7;
    retire_ints(&sliced);
    ready = ready && fixed.data[0].data[0] == 7;
    retire_ints(&fixed);
    return ready;
}
static bool independent_strings(void)
{
    PgyArray_Array_String source = string_source();
    PgySlice_Array_String view = {source.data, source.length};
    PgyArray_Array_String cloned = pgy_array_clone_Array_String(&source);
    PgyArray_Array_String sliced = pgy_slice_copy_Array_String(&view);
    PgyArray_Array_String fixed = {0};
    pgy_array_clone_Array_String_into(&source, &fixed);
    if (source.length != 1 || source.data == NULL || source.data[0].length != 2 ||
        source.data[0].data == NULL || source.data[0].data[0] == NULL || source.data[0].data[1] == NULL ||
        strcmp(source.data[0].data[0], "alpha") != 0 || strcmp(source.data[0].data[1], "beta") != 0 ||
        cloned.length != 1 || sliced.length != 1 || cloned.data == NULL || sliced.data == NULL ||
        cloned.data[0].data == NULL || sliced.data[0].data == NULL || cloned.data == source.data ||
        sliced.data == source.data || cloned.data == sliced.data ||
        cloned.data[0].data == source.data[0].data || sliced.data[0].data == source.data[0].data ||
        cloned.data[0].data == sliced.data[0].data ||
        cloned.data[0].length != 2 || sliced.data[0].length != 2 || fixed.length != 1 ||
        fixed.data == NULL || fixed.data[0].data == NULL || fixed.data[0].length != 2 ||
        fixed.data == source.data || fixed.data == cloned.data || fixed.data == sliced.data ||
        fixed.data[0].data == source.data[0].data || fixed.data[0].data == cloned.data[0].data ||
        fixed.data[0].data == sliced.data[0].data) return false;
    for (size_t i = 0; i < 2; i++) {
        char *original = source.data[0].data[i], *a = cloned.data[0].data[i], *b = sliced.data[0].data[i];
        char *f = fixed.data[0].data[i];
        if (a == NULL || b == NULL || f == NULL || a == original || b == original || a == b ||
            f == original || f == a || f == b || strcmp(f, original) != 0 ||
            strcmp(a, original) != 0 || strcmp(b, original) != 0) return false;
    }
    source.data[0].data[0][0] = 'X';
    if (strcmp(cloned.data[0].data[0], "alpha") != 0 || strcmp(sliced.data[0].data[0], "alpha") != 0 ||
        strcmp(fixed.data[0].data[0], "alpha") != 0) return false;
    retire_strings(&source);
    if (strcmp(cloned.data[0].data[1], "beta") != 0 || strcmp(sliced.data[0].data[1], "beta") != 0 ||
        strcmp(fixed.data[0].data[1], "beta") != 0) return false;
    retire_strings(&cloned);
    bool ready = strcmp(sliced.data[0].data[0], "alpha") == 0;
    retire_strings(&sliced);
    ready = ready && strcmp(fixed.data[0].data[0], "alpha") == 0;
    retire_strings(&fixed);
    return ready;
}
static bool empty_copies(void)
{
    PgyArray_Array_Int ints = pgy_array_new_Array_Int(0);
    PgyArray_Array_String strings = pgy_array_new_Array_String(0);
    PgySlice_Array_Int int_view = {NULL, 0};
    PgySlice_Array_String string_view = {NULL, 0};
    PgyArray_Array_Int a = pgy_array_clone_Array_Int(&ints), b = pgy_slice_copy_Array_Int(&int_view);
    PgyArray_Array_String c = pgy_array_clone_Array_String(&strings), d = pgy_slice_copy_Array_String(&string_view);
    return a.length == 0 && b.length == 0 && c.length == 0 && d.length == 0 &&
        a.data == NULL && b.data == NULL && c.data == NULL && d.data == NULL;
}
int main(int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "twin-source-null-int") == 0) {
            PgyArray_Array_Int output = {0};
            pgy_array_clone_Array_Int_into(NULL, &output);
        } else if (strcmp(argv[1], "twin-source-null-string") == 0) {
            PgyArray_Array_String output = {0};
            pgy_array_clone_Array_String_into(NULL, &output);
        } else if (strcmp(argv[1], "twin-null-int") == 0 || strcmp(argv[1], "twin-alias-int") == 0) {
            PgyArray_Array_Int source = int_source();
            pgy_array_clone_Array_Int_into(&source, strcmp(argv[1], "twin-null-int") == 0 ? NULL : &source);
        } else if (strcmp(argv[1], "twin-null-string") == 0 || strcmp(argv[1], "twin-alias-string") == 0) {
            PgyArray_Array_String source = string_source();
            pgy_array_clone_Array_String_into(&source, strcmp(argv[1], "twin-null-string") == 0 ? NULL : &source);
        } else return 2;
        fputs("invalid clone-into boundary returned a copy\n", stderr);
        return 3;
    }
    if (argc == 3) {
        const char *mode = argv[1];
        size_t offset = (size_t)strtoul(argv[2], NULL, 10);
        if (offset < 1 || offset > 4) return 2;
        if (strcmp(mode, "oom-int-clone") == 0 || strcmp(mode, "oom-int-slice") == 0) {
            PgyArray_Array_Int source = int_source();
            PgySlice_Array_Int view = {source.data, source.length};
            fail_at = allocations + offset;
            if (strcmp(mode, "oom-int-clone") == 0) (void)pgy_array_clone_Array_Int(&source);
            else (void)pgy_slice_copy_Array_Int(&view);
        } else if (strcmp(mode, "oom-string-clone") == 0 || strcmp(mode, "oom-string-slice") == 0) {
            PgyArray_Array_String source = string_source();
            PgySlice_Array_String view = {source.data, source.length};
            fail_at = allocations + offset;
            if (strcmp(mode, "oom-string-clone") == 0) (void)pgy_array_clone_Array_String(&source);
            else (void)pgy_slice_copy_Array_String(&view);
        } else return 2;
        fputs("injected allocation failure returned a copy\n", stderr);
        return 3;
    }
    if (argc != 1) return 2;
    if (!independent_ints() || !independent_strings() || !empty_copies()) {
        fputs("nested copy independence failed before unsafe retirement\n", stderr);
        return 1;
    }
    puts("nested array Clone/SliceCopy independent backing: PASS");
    return 0;
}
