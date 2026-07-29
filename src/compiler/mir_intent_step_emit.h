#ifndef PGY_MIR_INTENT_STEP_EMIT_H
#define PGY_MIR_INTENT_STEP_EMIT_H

#include "dir.h"
#include "mir.h"

bool mir_append_intent_stmt(MIRRoutine *routine,
                            MIRBasicBlock *block,
                            const char *name,
                            const char *slot_anchor,
                            const char *arg0,
                            const char *arg1,
                            ASTNode *ast);
bool mir_append_intent_step_facts(MIRRoutine *routine,
                                  MIRBasicBlock *block,
                                  const DIRIntentInfo *dir_intent,
                                  ASTNode *step,
                                  size_t step_index);

#endif /* PGY_MIR_INTENT_STEP_EMIT_H */
