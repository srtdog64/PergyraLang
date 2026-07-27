#ifndef PGY_RUNTIME_ARTIFACT_TRANSACTION_CORE_H
#define PGY_RUNTIME_ARTIFACT_TRANSACTION_CORE_H

/*
 * Compiler artifact transaction owner shared by the C-inline and LLVM-linked
 * runtime twins.  A transaction writes only to an exclusive sibling temp file;
 * the final path changes only after checked write/flush/close and one atomic
 * replace.  This guarantees atomic visibility, not crash durability (there is
 * deliberately no file/directory fsync claim here).
 */

#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PGY_COMPILER_ARTIFACT_TXN_API
#define PGY_COMPILER_ARTIFACT_TXN_API static inline
#endif

#define PGY_COMPILER_ARTIFACT_TXN_LIMIT 64
#define PGY_COMPILER_ARTIFACT_TXN_SLOT_BITS 6
#define PGY_COMPILER_ARTIFACT_TXN_SLOT_MASK 63
#define PGY_COMPILER_ARTIFACT_TXN_HANDLE_TAG 0x40000000u
#define PGY_COMPILER_ARTIFACT_TXN_GENERATION_MAX 0x00ffffffu

typedef enum PgyCompilerArtifactCommitStatus {
    PGY_COMPILER_ARTIFACT_COMMIT_OK = 0,
    PGY_COMPILER_ARTIFACT_COMMIT_INVALID_HANDLE = 1,
    PGY_COMPILER_ARTIFACT_COMMIT_WRITE_FAILED = 2,
    PGY_COMPILER_ARTIFACT_COMMIT_FLUSH_FAILED = 3,
    PGY_COMPILER_ARTIFACT_COMMIT_CLOSE_FAILED = 4,
    PGY_COMPILER_ARTIFACT_COMMIT_PUBLISH_FAILED = 5,
    PGY_COMPILER_ARTIFACT_COMMIT_CLEANUP_FAILED = 6
} PgyCompilerArtifactCommitStatus;

typedef struct PgyCompilerArtifactTransaction {
    FILE *stream;
    char *final_path;
    char *temp_path;
    uint32_t generation;
    PgyCompilerArtifactCommitStatus failure;
    bool cleanup_pending;
    bool active;
} PgyCompilerArtifactTransaction;

static PgyCompilerArtifactTransaction
    pgy_compiler_artifact_transactions[PGY_COMPILER_ARTIFACT_TXN_LIMIT];
static pthread_mutex_t pgy_compiler_artifact_transactions_mutex =
    PTHREAD_MUTEX_INITIALIZER;
static uint64_t pgy_compiler_artifact_temp_nonce = 0;
static uint32_t pgy_compiler_artifact_next_generation = 1;

static bool
pgy_compiler_artifact_fault_is(const char *stage)
{
#ifdef PGY_RUNTIME_ARTIFACT_TESTING
    const char *fault = getenv("PGY_ARTIFACT_TXN_FAULT");
    return fault != NULL && stage != NULL && strcmp(fault, stage) == 0;
#else
    (void)stage;
    return false;
#endif
}

static uint64_t
pgy_compiler_artifact_process_id(void)
{
#ifdef _WIN32
    return (uint64_t)GetCurrentProcessId();
#else
    return (uint64_t)getpid();
#endif
}

static FILE *
pgy_compiler_artifact_open_exclusive(const char *path)
{
#ifdef _WIN32
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL
                                  | FILE_FLAG_OPEN_REPARSE_POINT,
                              NULL);
    BY_HANDLE_FILE_INFORMATION information;
    int descriptor;
    FILE *stream;

    if (file == INVALID_HANDLE_VALUE)
        return NULL;
    if (!GetFileInformationByHandle(file, &information)
        || (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        CloseHandle(file);
        (void)DeleteFileA(path);
        return NULL;
    }
    descriptor = _open_osfhandle((intptr_t)file, _O_BINARY | _O_WRONLY);
    if (descriptor < 0) {
        CloseHandle(file);
        (void)DeleteFileA(path);
        return NULL;
    }
    stream = _fdopen(descriptor, "wb");
    if (stream == NULL) {
        _close(descriptor);
        (void)DeleteFileA(path);
        return NULL;
    }
    return stream;
#else
    int flags = O_WRONLY | O_CREAT | O_EXCL;
    int descriptor;
    FILE *stream;

#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
    descriptor = open(path, flags, 0666);
    if (descriptor < 0)
        return NULL;
    stream = fdopen(descriptor, "wb");
    if (stream == NULL) {
        close(descriptor);
        (void)remove(path);
        return NULL;
    }
    return stream;
#endif
}

static bool
pgy_compiler_artifact_publish(const char *temp_path, const char *final_path)
{
#ifdef _WIN32
    return MoveFileExA(temp_path, final_path, MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return rename(temp_path, final_path) == 0;
#endif
}

static bool
pgy_compiler_artifact_entry_release(PgyCompilerArtifactTransaction *entry,
                                    bool remove_temp)
{
    if (entry == NULL)
        return false;
    if (remove_temp && entry->temp_path != NULL
        && (pgy_compiler_artifact_fault_is("cleanup")
            || (remove(entry->temp_path) != 0 && errno != ENOENT))) {
        entry->failure = PGY_COMPILER_ARTIFACT_COMMIT_CLEANUP_FAILED;
        entry->cleanup_pending = true;
        return false;
    }
    free(entry->temp_path);
    free(entry->final_path);
    entry->stream = NULL;
    entry->temp_path = NULL;
    entry->final_path = NULL;
    entry->failure = PGY_COMPILER_ARTIFACT_COMMIT_OK;
    entry->cleanup_pending = false;
    entry->active = false;
    return true;
}

static PgyCompilerArtifactTransaction *
pgy_compiler_artifact_entry_for_handle(int32_t handle)
{
    uint32_t raw;
    uint32_t slot;
    uint32_t generation;
    PgyCompilerArtifactTransaction *entry;

    if (handle < 0)
        return NULL;
    raw = (uint32_t)handle;
    if ((raw & PGY_COMPILER_ARTIFACT_TXN_HANDLE_TAG) == 0)
        return NULL;
    slot = raw & PGY_COMPILER_ARTIFACT_TXN_SLOT_MASK;
    generation = (raw & ~PGY_COMPILER_ARTIFACT_TXN_HANDLE_TAG)
        >> PGY_COMPILER_ARTIFACT_TXN_SLOT_BITS;
    if (slot >= PGY_COMPILER_ARTIFACT_TXN_LIMIT || generation == 0)
        return NULL;
    entry = &pgy_compiler_artifact_transactions[slot];
    if (!entry->active || entry->generation != generation)
        return NULL;
    return entry;
}

PGY_COMPILER_ARTIFACT_TXN_API int32_t
pgy_compiler_artifact_begin(const char *path)
{
    char *resolved;
    PgyCompilerArtifactTransaction *entry = NULL;
    FILE *stream = NULL;
    char *temp_path = NULL;
    uint32_t slot = 0;
    uint32_t generation;

    pgy_cap_require_export(PGY_CAP_IO_WRITE, "compiler-artifact-begin");
    resolved = pgy_runtime_resolve_file_path(path, true);
    if (resolved == NULL)
        return -1;

    pthread_mutex_lock(&pgy_compiler_artifact_transactions_mutex);
    for (slot = 0; slot < PGY_COMPILER_ARTIFACT_TXN_LIMIT; ++slot) {
        if (!pgy_compiler_artifact_transactions[slot].active) {
            entry = &pgy_compiler_artifact_transactions[slot];
            break;
        }
    }
    if (entry == NULL) {
        pthread_mutex_unlock(&pgy_compiler_artifact_transactions_mutex);
        free(resolved);
        return -1;
    }

    for (unsigned attempt = 0; attempt < 64 && stream == NULL; ++attempt) {
        size_t capacity = strlen(resolved) + 96;
        int written;

        free(temp_path);
        temp_path = (char *)malloc(capacity);
        if (temp_path == NULL)
            break;
        pgy_compiler_artifact_temp_nonce++;
        written = snprintf(temp_path, capacity, "%s.pgy-tmp-%llu-%llu",
            resolved,
            (unsigned long long)pgy_compiler_artifact_process_id(),
            (unsigned long long)pgy_compiler_artifact_temp_nonce);
        if (written < 0 || (size_t)written >= capacity)
            break;
        if (!pgy_compiler_artifact_fault_is("open"))
            stream = pgy_compiler_artifact_open_exclusive(temp_path);
    }
    if (stream == NULL) {
        pthread_mutex_unlock(&pgy_compiler_artifact_transactions_mutex);
        free(temp_path);
        free(resolved);
        return -1;
    }

    generation = pgy_compiler_artifact_next_generation++;
    if (generation == 0 || generation > PGY_COMPILER_ARTIFACT_TXN_GENERATION_MAX) {
        generation = 1;
        pgy_compiler_artifact_next_generation = 2;
    }
    entry->stream = stream;
    entry->final_path = resolved;
    entry->temp_path = temp_path;
    entry->generation = generation;
    entry->failure = PGY_COMPILER_ARTIFACT_COMMIT_OK;
    entry->cleanup_pending = false;
    entry->active = true;
    pthread_mutex_unlock(&pgy_compiler_artifact_transactions_mutex);
    return (int32_t)(PGY_COMPILER_ARTIFACT_TXN_HANDLE_TAG
        | (generation << PGY_COMPILER_ARTIFACT_TXN_SLOT_BITS) | slot);
}

PGY_COMPILER_ARTIFACT_TXN_API bool
pgy_compiler_artifact_write(int32_t handle, const char *data)
{
    PgyCompilerArtifactTransaction *entry;
    size_t length;
    size_t written;
    bool ok = false;

    pgy_cap_require_export(PGY_CAP_IO_WRITE, "compiler-artifact-write");
    pthread_mutex_lock(&pgy_compiler_artifact_transactions_mutex);
    entry = pgy_compiler_artifact_entry_for_handle(handle);
    if (entry == NULL)
        goto done;
    if (entry->failure != PGY_COMPILER_ARTIFACT_COMMIT_OK)
        goto done;
    if (pgy_compiler_artifact_fault_is("write")) {
        entry->failure = PGY_COMPILER_ARTIFACT_COMMIT_WRITE_FAILED;
        goto done;
    }
    if (data == NULL) {
        ok = true;
        goto done;
    }
    length = strlen(data);
    written = fwrite(data, 1, length, entry->stream);
    if (written != length) {
        entry->failure = PGY_COMPILER_ARTIFACT_COMMIT_WRITE_FAILED;
        goto done;
    }
    ok = true;

done:
    pthread_mutex_unlock(&pgy_compiler_artifact_transactions_mutex);
    return ok;
}

PGY_COMPILER_ARTIFACT_TXN_API int32_t
pgy_compiler_artifact_commit(int32_t handle)
{
    PgyCompilerArtifactTransaction *entry;
    PgyCompilerArtifactCommitStatus status;

    pgy_cap_require_export(PGY_CAP_IO_WRITE, "compiler-artifact-commit");
    pthread_mutex_lock(&pgy_compiler_artifact_transactions_mutex);
    entry = pgy_compiler_artifact_entry_for_handle(handle);
    if (entry == NULL) {
        pthread_mutex_unlock(&pgy_compiler_artifact_transactions_mutex);
        return PGY_COMPILER_ARTIFACT_COMMIT_INVALID_HANDLE;
    }
    if (entry->cleanup_pending) {
        bool cleaned = pgy_compiler_artifact_entry_release(entry, true);
        pthread_mutex_unlock(&pgy_compiler_artifact_transactions_mutex);
        (void)cleaned;
        return PGY_COMPILER_ARTIFACT_COMMIT_CLEANUP_FAILED;
    }

    status = entry->failure;
    if (status == PGY_COMPILER_ARTIFACT_COMMIT_OK
        && (pgy_compiler_artifact_fault_is("flush")
            || fflush(entry->stream) != 0)) {
        status = PGY_COMPILER_ARTIFACT_COMMIT_FLUSH_FAILED;
    }
    if (pgy_compiler_artifact_fault_is("close")) {
        (void)fclose(entry->stream);
        status = status == PGY_COMPILER_ARTIFACT_COMMIT_OK
            ? PGY_COMPILER_ARTIFACT_COMMIT_CLOSE_FAILED : status;
    } else if (fclose(entry->stream) != 0
               && status == PGY_COMPILER_ARTIFACT_COMMIT_OK) {
        status = PGY_COMPILER_ARTIFACT_COMMIT_CLOSE_FAILED;
    }
    entry->stream = NULL;

    if (status == PGY_COMPILER_ARTIFACT_COMMIT_OK) {
        if (pgy_compiler_artifact_fault_is("publish")
            || pgy_compiler_artifact_fault_is("cleanup")
            || !pgy_compiler_artifact_publish(
                entry->temp_path, entry->final_path)) {
            status = PGY_COMPILER_ARTIFACT_COMMIT_PUBLISH_FAILED;
        }
    }

    if (!pgy_compiler_artifact_entry_release(
            entry, status != PGY_COMPILER_ARTIFACT_COMMIT_OK)) {
        status = PGY_COMPILER_ARTIFACT_COMMIT_CLEANUP_FAILED;
    }
    pthread_mutex_unlock(&pgy_compiler_artifact_transactions_mutex);
    return (int32_t)status;
}

PGY_COMPILER_ARTIFACT_TXN_API int32_t
pgy_compiler_artifact_abort(int32_t handle)
{
    PgyCompilerArtifactTransaction *entry;
    PgyCompilerArtifactCommitStatus status = PGY_COMPILER_ARTIFACT_COMMIT_OK;

    pthread_mutex_lock(&pgy_compiler_artifact_transactions_mutex);
    entry = pgy_compiler_artifact_entry_for_handle(handle);
    if (entry == NULL) {
        status = PGY_COMPILER_ARTIFACT_COMMIT_INVALID_HANDLE;
    } else {
        if (entry->stream != NULL && fclose(entry->stream) != 0)
            status = PGY_COMPILER_ARTIFACT_COMMIT_CLOSE_FAILED;
        entry->stream = NULL;
        if (!pgy_compiler_artifact_entry_release(entry, true))
            status = PGY_COMPILER_ARTIFACT_COMMIT_CLEANUP_FAILED;
    }
    pthread_mutex_unlock(&pgy_compiler_artifact_transactions_mutex);
    return (int32_t)status;
}

#undef PGY_COMPILER_ARTIFACT_TXN_API

#endif /* PGY_RUNTIME_ARTIFACT_TRANSACTION_CORE_H */
