#include "tests/semantic/test_semantic_helpers.cases.h"

#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define pgy_test_chdir _chdir
#define pgy_test_getpid _getpid
#define pgy_test_mkdir(path) _mkdir(path)
#define pgy_test_rmdir _rmdir
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define pgy_test_chdir chdir
#define pgy_test_getpid getpid
#define pgy_test_mkdir(path) mkdir(path, 0777)
#define pgy_test_rmdir rmdir
#endif

#ifndef PGY_PROJECT_ROOT
#define PGY_PROJECT_ROOT "."
#endif

static char g_test_semantic_workdir[1200];

static void
test_semantic_leave_isolated_cwd(void)
{
    if (g_test_semantic_workdir[0] == '\0')
        return;
    int chdir_result = pgy_test_chdir(PGY_PROJECT_ROOT);
    (void)chdir_result;
    (void)pgy_test_rmdir(g_test_semantic_workdir);
    g_test_semantic_workdir[0] = '\0';
}

static bool
test_semantic_enter_isolated_cwd(void)
{
    char tmp_root[1024];
    char work[1200];
    int pid = (int)pgy_test_getpid();
    long stamp = (long)time(NULL);

    snprintf(tmp_root, sizeof(tmp_root), "%s/.tmp", PGY_PROJECT_ROOT);
    if (pgy_test_mkdir(tmp_root) != 0 && errno != EEXIST)
        return false;

    for (int attempt = 0; attempt < 64; attempt++) {
        snprintf(work,
                 sizeof(work),
                 "%s/pgy-semantic-test.%d.%ld.%d",
                 tmp_root,
                 pid,
                 stamp,
                 attempt);
        if (pgy_test_mkdir(work) == 0) {
            if (pgy_test_chdir(work) == 0) {
                snprintf(g_test_semantic_workdir,
                         sizeof(g_test_semantic_workdir),
                         "%s",
                         work);
                atexit(test_semantic_leave_isolated_cwd);
                return true;
            }
            (void)pgy_test_rmdir(work);
            return false;
        }
        if (errno != EEXIST)
            return false;
    }

    return false;
}

/* -----------------------------------------------------------------
 * Test groups
 * ----------------------------------------------------------------- */


#include "tests/semantic/test_semantic_core_part_a.cases.h"
#include "tests/semantic/test_semantic_core_part_b_1.cases.h"
#include "tests/semantic/test_semantic_core_part_b_2.cases.h"
#include "tests/semantic/test_semantic_qubit_part_a.cases.h"
#include "tests/semantic/test_semantic_qubit_part_b_1.cases.h"
#include "tests/semantic/test_semantic_qubit_part_b_2.cases.h"
#include "tests/semantic/test_semantic_qubit_part_c_1.cases.h"
#include "tests/semantic/test_semantic_qubit_part_c_2.cases.h"
#include "tests/semantic/test_semantic_qubit_part_d_1.cases.h"
#include "tests/semantic/test_semantic_qubit_part_d_2.cases.h"
#include "tests/semantic/test_semantic_qubit_part_e.cases.h"
#include "tests/semantic/test_semantic_ownership_boundaries_part_a_1.cases.h"
#include "tests/semantic/test_semantic_ownership_boundaries_part_a_2.cases.h"
#include "tests/semantic/test_semantic_ownership_boundaries_part_b_1.cases.h"
#include "tests/semantic/test_semantic_ownership_boundaries_part_b_2.cases.h"
#include "tests/semantic/test_semantic_domain_part_a.cases.h"
#include "tests/semantic/test_semantic_domain_part_b_1.cases.h"
#include "tests/semantic/test_semantic_domain_part_b_2.cases.h"
#include "tests/semantic/test_semantic_domain_part_c.cases.h"
#include "tests/semantic/test_semantic_event_part_a.cases.h"
#include "tests/semantic/test_semantic_projection_diagnostics.cases.h"
#include "tests/semantic/test_semantic_intent_observability.cases.h"
#include "tests/semantic/test_semantic_intent_compression_part_a_1.cases.h"
#include "tests/semantic/test_semantic_intent_compression_part_a_2.cases.h"
#include "tests/semantic/test_semantic_b0_provenance.cases.h"

#include "tests/semantic/test_semantic_shared_domain_part_a_1.cases.h"
#include "tests/semantic/test_semantic_shared_domain_part_a_2.cases.h"
#include "tests/semantic/test_semantic_shared_domain_part_b.cases.h"
#include "tests/semantic/test_semantic_parallel_family.cases.h"
#include "tests/semantic/test_semantic_parallel_context.cases.h"
#include "tests/semantic/test_semantic_async_part_a_1.cases.h"
#include "tests/semantic/test_semantic_async_part_a_2.cases.h"
#include "tests/semantic/test_semantic_async_part_b.cases.h"
#include "tests/semantic/test_semantic_effects_part_a_1.cases.h"
#include "tests/semantic/test_semantic_effects_part_a_2.cases.h"
#include "tests/semantic/test_semantic_effects_part_b_1.cases.h"
#include "tests/semantic/test_semantic_effects_part_b_2.cases.h"
#include "tests/semantic/test_semantic_graph_part_a_1.cases.h"
#include "tests/semantic/test_semantic_graph_part_a_2.cases.h"
#include "tests/semantic/test_semantic_graph_part_b_1.cases.h"
#include "tests/semantic/test_semantic_graph_part_b_2.cases.h"
#include "tests/semantic/test_semantic_misc_a_part_a_1.cases.h"
#include "tests/semantic/test_semantic_misc_a_part_a_2.cases.h"
#include "tests/semantic/test_semantic_misc_a_part_a2.cases.h"
#include "tests/semantic/test_semantic_misc_a_part_b_1.cases.h"
#include "tests/semantic/test_semantic_misc_a_part_b_2.cases.h"
#include "tests/semantic/test_semantic_misc_b1_part_a_1.cases.h"
#include "tests/semantic/test_semantic_misc_b1_part_a_2.cases.h"
#include "tests/semantic/test_semantic_misc_b1_part_b.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_a_1.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_a_2.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_b_1.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_b_2.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_c_1.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_c_2.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_d_1.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_d_2.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_e_1.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_e_2.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_f_1.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_f_2.cases.h"
#include "tests/semantic/test_semantic_misc_b2_part_g.cases.h"


/* -----------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------- */

static void
test_semantic_ownership_boundaries(void)
{
    test_semantic_ownership_boundaries_part_a();
    test_semantic_ownership_boundaries_part_b();
}

int
main(void)
{
    printf("=== Pergyra Semantic Analyzer Test Suite ===\n");

    if (!test_semantic_enter_isolated_cwd()) {
        fprintf(stderr, "failed to enter isolated semantic test cwd\n");
        return 2;
    }

    type_system_init();

    test_type_system();
    test_symbol_table();
    test_type_checker_slot_rules();
    test_undefined_symbol();
    test_while_loop();
    test_arrays_and_enums();
    test_stdlib_and_io();
    test_qubit_slot_semantics();
    test_semantic_ownership_boundaries();
    test_quantum_extensions();
    test_match_stmt();
    test_event_semantics();
    test_projection_contract_diagnostics();
    test_b0_provenance_closure_diagnostics();
    test_intent_observability_semantics();
    test_intent_compression_semantics();
    test_ability_decl();
    test_role_decl();
    test_party_decl();
    test_roster_world_decl();
    test_extern_block();
    test_engine_collections();
    test_subject_class_ownership();
    test_shared_memory_features();
    test_parallel_family_semantics();
    test_parallel_context_semantics();
    test_parallel_execution_semantics();
    test_effect_inference();
    test_type_resolution_graph();
    test_misc_grammar_edges();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    type_system_cleanup();
    test_semantic_leave_isolated_cwd();
    return (g_fail > 0) ? 1 : 0;
}
