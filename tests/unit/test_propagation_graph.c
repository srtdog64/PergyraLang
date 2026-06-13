#include "propagation_graph.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("[FAIL] %s\n", msg); failures++; } else { printf("[ok] %s\n", msg); } } while (0)

static size_t N(PropagationGraph *g, const char *s) { return propagation_graph_intern_node(g, s); }

int main(void)
{
    /* 1. Linear chain A->B->C->D */
    {
        PropagationGraph *g = propagation_graph_create();
        propagation_graph_add_edge(g, N(g,"A"), N(g,"B"));
        propagation_graph_add_edge(g, N(g,"B"), N(g,"C"));
        propagation_graph_add_edge(g, N(g,"C"), N(g,"D"));
        CHECK(propagation_graph_schedule(g), "chain: schedule ok");
        CHECK(!g->has_cycle, "chain: acyclic");
        CHECK(g->chain_depth == 4, "chain: depth=4");
        CHECK(g->pass_limit == 4, "chain: pass_limit=4 (was crude count)");
        CHECK(g->order_count == 4 && strcmp(g->node_names[g->order[0]],"A")==0
              && strcmp(g->node_names[g->order[3]],"D")==0, "chain: order A..D");
        propagation_graph_destroy(g);
    }
    /* 2. Cycle A->B->C->A */
    {
        PropagationGraph *g = propagation_graph_create();
        propagation_graph_add_edge(g, N(g,"A"), N(g,"B"));
        propagation_graph_add_edge(g, N(g,"B"), N(g,"C"));
        propagation_graph_add_edge(g, N(g,"C"), N(g,"A"));
        CHECK(propagation_graph_schedule(g), "cycle: schedule ok");
        CHECK(g->has_cycle, "cycle: detected");
        CHECK(g->max_scc_size == 3, "cycle: max_scc_size=3 (fixpoint width)");
        CHECK(g->scc_count == 1, "cycle: one SCC");
        CHECK(g->pass_limit == 3, "cycle: pass_limit=3");
        propagation_graph_destroy(g);
    }
    /* 3. Diamond A->B,A->C,B->D,C->D */
    {
        PropagationGraph *g = propagation_graph_create();
        size_t a=N(g,"A"), b=N(g,"B"), c=N(g,"C"), d=N(g,"D");
        propagation_graph_add_edge(g,a,b); propagation_graph_add_edge(g,a,c);
        propagation_graph_add_edge(g,b,d); propagation_graph_add_edge(g,c,d);
        CHECK(propagation_graph_schedule(g), "diamond: schedule ok");
        CHECK(!g->has_cycle, "diamond: acyclic");
        CHECK(g->chain_depth == 3, "diamond: depth=3 (A,B|C,D)");
        CHECK(g->pass_limit == 3, "diamond: pass_limit=3");
        /* A must precede B,C,D; D must be last */
        size_t posA=99,posD=99;
        for (size_t i=0;i<g->order_count;i++){
            if(strcmp(g->node_names[g->order[i]],"A")==0)posA=i;
            if(strcmp(g->node_names[g->order[i]],"D")==0)posD=i;
        }
        CHECK(posA==0 && posD==3, "diamond: A first, D last");
        propagation_graph_destroy(g);
    }
    /* 4. Transitive multi-layer: world state S3 <- S2 <- S1 (input chains) plus extra cluster cycle */
    {
        PropagationGraph *g = propagation_graph_create();
        propagation_graph_add_edge(g, N(g,"S1"), N(g,"S2"));
        propagation_graph_add_edge(g, N(g,"S2"), N(g,"S3"));
        /* embedded cycle X<->Y feeding S3 */
        propagation_graph_add_edge(g, N(g,"X"), N(g,"Y"));
        propagation_graph_add_edge(g, N(g,"Y"), N(g,"X"));
        propagation_graph_add_edge(g, N(g,"Y"), N(g,"S3"));
        CHECK(propagation_graph_schedule(g), "transitive: schedule ok");
        CHECK(g->has_cycle && g->max_scc_size==2, "transitive: X<->Y cluster size 2");
        /* longest weighted path: X/Y(2) -> S3(1) = 3 ; S1->S2->S3 = 3 ; take max=3 */
        CHECK(g->pass_limit == 3, "transitive: pass_limit=3 (SCC-weighted)");
        propagation_graph_destroy(g);
    }
    /* 5. Empty */
    {
        PropagationGraph *g = propagation_graph_create();
        CHECK(propagation_graph_schedule(g), "empty: schedule ok");
        CHECK(g->pass_limit==0 && !g->has_cycle, "empty: zero/acyclic");
        propagation_graph_destroy(g);
    }
    printf("\n=== propagation graph: %d failures ===\n", failures);
    return failures ? 1 : 0;
}
