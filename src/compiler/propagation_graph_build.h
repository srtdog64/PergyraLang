#ifndef PERGYRA_PROPAGATION_GRAPH_BUILD_H
#define PERGYRA_PROPAGATION_GRAPH_BUILD_H

#include <stdbool.h>

#include "mir.h"
#include "propagation_graph.h"
#include "../parser/ast.h"

bool propagation_graph_build_from_zone_mir(PropagationGraph *g,
                                            const MIRProgram *mir,
                                            const char *zone_name);
bool propagation_graph_build_from_world(PropagationGraph *g, const ASTNode *world);
bool propagation_graph_build_from_world_header(
    PropagationGraph *g,
    const MIRDeclHeader *header);

#endif /* PERGYRA_PROPAGATION_GRAPH_BUILD_H */
