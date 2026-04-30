/* transpiler_emitters_base_a split into sub-1000 LOC include chunks.
 * Keep this shim for the existing include order. */
#include "transpiler_mir_emit_decls.h"
#include "transpiler_let_slot_emit.h"
#include "transpiler_let_box_emit.h"
#include "transpiler_let_emit.h"
#include "transpiler_mir_pending_uses.h"
#include "transpiler_mir_cfg_control_emit.h"
#include "transpiler_mir_pin_emit.h"
#include "transpiler_mir_block_emit.h"
#include "transpiler_mir_ssa_contract.h"
#include "transpiler_mir_emission_contract.h"
#include "transpiler_mir_resource_hook_emit.h"
