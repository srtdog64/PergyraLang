#include "mir.h"

void
mir_lower_request_init(MIRLowerRequest *request,
                        const HIRProgram *hir,
                        const RIRProgram *rir,
                        const SemanticResult *semantic)
{
    if (request == NULL)
        return;
    request->protocol_id = PGY_MIR_LOWER_PROTOCOL_ID;
    request->protocol_version = PGY_MIR_LOWER_PROTOCOL_VERSION;
    request->hir = hir;
    request->dir = NULL;
    request->rir = rir;
    request->semantic = semantic;
}

void
mir_lower_request_bind_dir(MIRLowerRequest *request, const DIRProgram *dir)
{
    if (request != NULL)
        request->dir = dir;
}
