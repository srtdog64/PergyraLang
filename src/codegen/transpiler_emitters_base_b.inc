/* C backend base emitter split into sub-1000 LOC include chunks.
 * Keep this shim for the existing transpiler include order. */
#include "transpiler_emitters_base_b_part_a.inc"
#include "transpiler_mir_func_emit.h"
#include "transpiler_parallel_capture.h"
#include "transpiler_func_class_flow_emit.h"
#include "transpiler_destructure_emit.h"
#include "transpiler_statement_dispatch.h"
#include "transpiler_block_intent_helpers.h"
#include "transpiler_emitters_base_b_part_d.inc"
