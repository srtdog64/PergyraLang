#include "../../compiler/mir_hir_fact_transfer.h"
#include "../../compiler/hir_collection_owned_local_transition_adapter.h"

static void
test_mir_collection_ownership_validation(void)
{
    {
        HIRProgram *field_hir = NULL;
        RIRProgram *field_rir = NULL;
        MIRProgram *field_mir = NULL;
        const char *source =
            "struct FieldArtifact{text:String;}struct FieldRows{values:Array<Int>;}"
            "func FieldArtifactFrom(text:String)->FieldArtifact{let spare=FieldArtifact(\"other\");return FieldArtifact(text);}"
            "func FieldDigest(text:String,count:Int,ref left:FieldRows,ref right:FieldRows)->Int{"
            "return StringLength(text)+count+ArrayLength(left.values)+ArrayLength(right.values);}"
            "func FieldProbe()->Bool{let artifact=FieldArtifactFrom(\"literal\");let spare=FieldArtifact(\"other\");"
            "let unrelated:Array<String> = [];let data=FieldRows([1]);"
            "return FieldDigest(artifact.text,0,data,data)>0;}"
            "func FieldForeign()->Bool{return true;}"
            "func Main()->Void{if FieldProbe(){Log(\"ok\");}}";
        bool lowered = lower_mir_from_source(source, &field_hir, &field_rir, &field_mir);
        MIRRoutine *probe = lowered ? find_mir_routine_mut(field_mir, "FieldProbe", MIR_SCOPE_FUNCTION) : NULL;
        MIRRoutine *foreign = lowered ? find_mir_routine_mut(field_mir, "FieldForeign", MIR_SCOPE_FUNCTION) : NULL;
        MIRInstruction *returned = NULL, *foreign_return = NULL;
        MIRBasicBlock *return_block = NULL;
        for (size_t b = 0; probe != NULL && b < probe->block_count; b++)
            for (size_t i = 0; i < probe->blocks[b].instruction_count; i++)
                if (probe->blocks[b].instructions[i].kind == MIR_INST_RETURN) {
                    returned = &probe->blocks[b].instructions[i];
                    return_block = &probe->blocks[b];
                }
        for (size_t b = 0; foreign != NULL && b < foreign->block_count; b++)
            for (size_t i = 0; i < foreign->blocks[b].instruction_count; i++)
                if (foreign->blocks[b].instructions[i].kind == MIR_INST_RETURN)
                    foreign_return = &foreign->blocks[b].instructions[i];
        ASTNode *root = returned == NULL ? NULL : returned->expr0;
        ASTNode *sink = root != NULL && root->type == AST_BINARY ? ast_binary_left(root) : NULL;
        ASTNode *member = sink != NULL && sink->type == AST_CALL ? ast_call_argument(sink, 0) : NULL;
        PgyCollectionEffectView view = {0};
        bool control = lowered && probe != NULL && foreign_return != NULL && return_block != NULL &&
            sink != NULL && sink->type == AST_CALL && ast_call_arg_count(sink) == 4 &&
            member != NULL && member->type == AST_MEMBER_ACCESS &&
            semantic_collection_effect_snapshot_current(field_mir->collection_effect_snapshot,
                field_mir->collection_effect_source_program, &view) && view.source_use_count != 0 &&
            mir_validate_collection_effect_evidence(field_mir, NULL, NULL) && mir_validate(field_mir, NULL) &&
            mir_validate_emission_contract(probe, probe->has_cleanup_block, false, NULL);
        TEST("MIR nested selected-field source has an admitted immutable recipe");
        EXPECT(control);
        if (control) {
            MIRRoutine *producer = find_mir_routine_mut(field_mir, "FieldArtifactFrom", MIR_SCOPE_FUNCTION);
            MIRInstruction *definition = NULL, *producer_return = NULL;
            ASTNode *alternate_definition = NULL, *alternate_return = NULL;
            for (size_t b = 0; b < probe->block_count; b++)
                for (size_t i = 0; i < probe->blocks[b].instruction_count; i++) {
                    MIRInstruction *inst = &probe->blocks[b].instructions[i];
                    if (inst->kind != MIR_INST_DEF || inst->arg0 == NULL) continue;
                    if (strcmp(inst->arg0, "artifact") == 0) definition = inst;
                    if (strcmp(inst->arg0, "spare") == 0) alternate_definition = inst->expr0;
                }
            for (size_t b = 0; producer != NULL && b < producer->block_count; b++)
                for (size_t i = 0; i < producer->blocks[b].instruction_count; i++) {
                    MIRInstruction *inst = &producer->blocks[b].instructions[i];
                    if (inst->kind == MIR_INST_RETURN) producer_return = inst;
                    if (inst->kind == MIR_INST_DEF && inst->arg0 != NULL && strcmp(inst->arg0, "spare") == 0)
                        alternate_return = inst->expr0;
                }
            bool roles = definition != NULL && alternate_definition != NULL &&
                producer_return != NULL && alternate_return != NULL;
            TEST("MIR field recipe role substitutions have original same-caller alternatives");
            EXPECT(roles);
            for (unsigned role = 0; roles && role < 2; role++) {
                MIRInstruction *inst = role == 0 ? definition : producer_return;
                MIRRoutine *owner = role == 0 ? probe : producer;
                ASTNode *saved = inst->expr0;
                inst->expr0 = role == 0 ? alternate_definition : alternate_return;
                bool refused = !mir_validate_collection_effect_evidence(field_mir, owner, NULL) &&
                    !mir_validate(field_mir, NULL) &&
                    !mir_validate_emission_contract(owner, owner->has_cleanup_block, false, NULL);
                inst->expr0 = saved;
                TEST("MIR field recipe producer cannot become another original nominal call");
                EXPECT(refused && mir_validate(field_mir, NULL) &&
                    mir_validate_emission_contract(owner, owner->has_cleanup_block, false, NULL));
            }
            ASTNode unrelated_sink = *sink;
            unrelated_sink.stable_id = 0;
            TEST("MIR source hook binds both protected value and exact sink slot");
            EXPECT(!semantic_collection_effect_snapshot_source_use(field_mir->collection_effect_snapshot,
                    probe->source_syntax_id, &unrelated_sink, 0, member) &&
                !semantic_collection_effect_snapshot_source_use(field_mir->collection_effect_snapshot,
                    probe->source_syntax_id, sink, 1, member) &&
                !semantic_collection_effect_snapshot_source_use(field_mir->collection_effect_snapshot,
                    probe->source_syntax_id, sink, 0, ast_call_argument(sink, 1)) &&
                semantic_collection_effect_snapshot_source_use(field_mir->collection_effect_snapshot,
                    probe->source_syntax_id, sink, 0, member) &&
                semantic_collection_effect_snapshot_source_use(field_mir->collection_effect_snapshot,
                    probe->source_syntax_id, sink, 0, member));
            TEST("MIR pure field source classification is reusable");
            EXPECT(hir_collection_source_expression_current(field_mir->collection_effect_snapshot,
                    probe->hir_routine, root) &&
                hir_collection_source_expression_current(field_mir->collection_effect_snapshot,
                    probe->hir_routine, root));
            MIRInstruction saved_return = *returned;
            for (unsigned fault = 0; fault < 5; fault++) {
                ASTNode changed_root = *root, changed_sink = *sink, changed_member = *member;
                ASTNode *arguments[4];
                for (size_t a = 0; a < 4; a++) arguments[a] = ast_call_argument(sink, a);
                if (fault != 0) {
                    changed_root.data.binary.left = &changed_sink;
                    changed_sink.data.call.arguments = arguments;
                }
                if (fault == 2) {
                    arguments[0] = ast_call_argument(sink, 1);
                    arguments[1] = member;
                }
                if (fault == 3) changed_sink.data.call.arg_count = 0;
                if (fault == 4) arguments[0] = &changed_member;
                bool local_refused = fault == 0
                    ? !semantic_collection_effect_snapshot_source_root(field_mir->collection_effect_snapshot,
                        probe->source_syntax_id, &changed_root)
                    : fault == 4
                        ? !semantic_collection_effect_snapshot_source_value(field_mir->collection_effect_snapshot,
                            probe->source_syntax_id, &changed_member)
                        : !semantic_collection_effect_snapshot_source_use(field_mir->collection_effect_snapshot,
                            probe->source_syntax_id, fault == 1 ? &changed_sink : sink, 0,
                            fault == 3 ? NULL : arguments[0]);
                returned->expr0 = &changed_root;
                bool evidence_refused = !mir_validate_collection_effect_evidence(field_mir, probe, NULL);
                bool full_refused = !mir_validate(field_mir, NULL);
                bool emission_refused = !mir_validate_emission_contract(probe,
                    probe->has_cleanup_block, false, NULL);
                *returned = saved_return;
                TEST("MIR actual nested source operand cannot detach or change ordinal");
                if (!evidence_refused || !full_refused || !emission_refused)
                    printf("\n  nested source fault=%u evidence=%d full=%d emission=%d\n",
                        fault, evidence_refused, full_refused, emission_refused);
                EXPECT(local_refused && evidence_refused && full_refused && emission_refused &&
                    ast_binary_left(root) == sink && ast_call_argument(sink, 0) == member &&
                    mir_validate(field_mir, NULL) &&
                    mir_validate_emission_contract(probe, probe->has_cleanup_block, false, NULL));
            }
            MIRInstruction saved_foreign = *foreign_return;
            foreign_return->expr0 = root;
            bool foreign_refused = !mir_validate_collection_effect_evidence(field_mir, foreign, NULL) &&
                !mir_validate(field_mir, NULL) && !mir_validate_emission_contract(foreign,
                    foreign->has_cleanup_block, false, NULL);
            *foreign_return = saved_foreign;
            TEST("MIR protected nested source cannot migrate to another caller");
            EXPECT(foreign_refused && mir_validate(field_mir, NULL) &&
                mir_validate_emission_contract(foreign, foreign->has_cleanup_block, false, NULL));
            ASTNode cycle = *root;
            cycle.data.binary.left = &cycle;
            TEST("MIR pure source visitor refuses an active-path cycle");
            EXPECT(!hir_collection_source_expression_current(field_mir->collection_effect_snapshot,
                    foreign->hir_routine, &cycle) &&
                hir_collection_source_expression_current(field_mir->collection_effect_snapshot,
                    foreign->hir_routine, saved_foreign.expr0));
            /* This checks only lifetime evidence, not equivalence of return deletion. */
            size_t saved_count = return_block->instruction_count;
            return_block->instruction_count = (size_t)(returned - return_block->instructions);
            bool unused_allowed = mir_validate_collection_effect_evidence(field_mir, probe, NULL);
            return_block->instruction_count = saved_count;
            TEST("MIR pure source recipe need not have an executed consumer");
            EXPECT(unused_allowed && mir_validate(field_mir, NULL));
        }
        mir_destroy(field_mir);
        rir_destroy(field_rir);
        hir_destroy(field_hir);
    }
    {
        HIRProgram *risk_hir = NULL;
        RIRProgram *risk_rir = NULL;
        MIRProgram *risk_mir = NULL;
        const char *risk_source =
            "struct Arena{atoms:Array<String>;}"
            "func VariantAt(arena:Arena,index:Int)->Option<String>{"
            "if index<0||index>=ArrayLength(arena.atoms){return None;}"
            "return Some(arena.atoms[index]);}"
            "func NonownerRead(text:String)->Int{return StringLength(text);}"
            "func Main()->Void{let arena=Arena([\"red,blue\"]);"
            "let selected=VariantAt(arena,0);if IsSome(selected){"
            "let text=UnwrapOption(selected);let no_texts:Array<String> = [];"
            "var spare:Array<String> = [\"literal\"];spare=[\"other\"];"
            "var marker:Int=0;marker=1;"
            "ArrayPush(no_texts,\"literal\");Log(no_texts[0]);Log(NonownerRead(text));"
            "Log(ArrayLength(no_texts));Log(ArrayLength(spare));}}";
        bool lowered = lower_mir_from_source(risk_source, &risk_hir, &risk_rir, &risk_mir);
        MIRRoutine *main_routine = lowered
            ? find_mir_routine_mut(risk_mir, "Main", MIR_SCOPE_FUNCTION) : NULL;
        MIRInstruction *risk_def = NULL, *spare_def = NULL, *push = NULL;
        MIRInstruction *assignment = NULL, *spare_assignment = NULL;
        MIRBasicBlock *push_block = NULL;
        size_t push_index = 0;
        for (size_t b = 0; main_routine != NULL && b < main_routine->block_count; b++)
            for (size_t i = 0; i < main_routine->blocks[b].instruction_count; i++) {
                MIRInstruction *inst = &main_routine->blocks[b].instructions[i];
                if (inst->kind == MIR_INST_DEF && inst->arg0 != NULL) {
                    if (strcmp(inst->arg0, "no_texts") == 0) risk_def = inst;
                    if (strcmp(inst->arg0, "spare") == 0) spare_def = inst;
                }
                if (inst->ast != NULL && inst->ast->type == AST_ASSIGNMENT) {
                    ASTNode *target = ast_assignment_target(inst->ast);
                    const char *name = target == NULL ? NULL : ast_identifier_name(target);
                    if (name != NULL && strcmp(name, "marker") == 0) assignment = inst;
                    if (name != NULL && strcmp(name, "spare") == 0) spare_assignment = inst;
                }
                if (inst->has_collection_ownership_receipt &&
                    inst->collection_ownership_effect_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION) {
                    push = inst;
                    push_block = &main_routine->blocks[b];
                    push_index = i;
                }
            }
        PgyCollectionEffectView risk_view = {0};
        bool risk_control = main_routine != NULL && risk_def != NULL && spare_def != NULL &&
            risk_def->expr0 != NULL && spare_def->expr0 != NULL && push != NULL &&
            push_block != NULL && assignment != NULL && spare_assignment != NULL &&
            push->expr0 != NULL && push->expr0->type == AST_CALL &&
            semantic_collection_effect_snapshot_current(risk_mir->collection_effect_snapshot,
                risk_mir->collection_effect_source_program, &risk_view) &&
            risk_view.nonowner_risk_count == 1 &&
            mir_validate_collection_effect_evidence(risk_mir, NULL, NULL) &&
            mir_validate(risk_mir, NULL) &&
            mir_validate_emission_contract(main_routine,
                main_routine->has_cleanup_block, false, NULL);
        TEST("MIR non-owning empty risk retains its original whole-body evidence");
        EXPECT(risk_control);
        if (risk_control) {
            ASTNode *risk_receiver = ast_call_argument(push->expr0, 0);
            ASTNode *original_spare_rhs = spare_assignment->expr1;
            TEST("MIR unrelated typed Array assignment has an original RHS");
            EXPECT(risk_receiver != NULL && risk_receiver->type == AST_IDENTIFIER &&
                original_spare_rhs != NULL && original_spare_rhs->type == AST_ARRAY_LITERAL);
            if (risk_receiver != NULL && original_spare_rhs != NULL) {
                spare_assignment->expr1 = risk_receiver;
                bool evidence_refused = !mir_validate_collection_effect_evidence(
                    risk_mir, main_routine, NULL);
                bool full_refused = !mir_validate(risk_mir, NULL);
                bool emission_refused = !mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL);
                spare_assignment->expr1 = original_spare_rhs;
                TEST("MIR unrelated Array assignment cannot alias a nonowner risk RHS");
                EXPECT(evidence_refused && full_refused && emission_refused &&
                    mir_validate_collection_effect_evidence(risk_mir, main_routine, NULL) &&
                    mir_validate(risk_mir, NULL) &&
                    mir_validate_emission_contract(main_routine,
                        main_routine->has_cleanup_block, false, NULL));
            }
            ASTNode *main_ast = ast_program_statement(risk_mir->collection_effect_source_program, 3);
            ASTNode *branch = main_ast == NULL ? NULL :
                ast_block_statement(ast_func_body(main_ast), 2);
            ASTNode *risk_let = branch == NULL ? NULL :
                ast_block_statement(ast_if_then_branch(branch), 1);
            ASTNode *original = risk_let == NULL ? NULL : ast_let_initializer(risk_let);
            TEST("MIR risk definition identifies its original empty LET");
            EXPECT(risk_let != NULL && original == risk_def->expr0);
            if (risk_let != NULL && original == risk_def->expr0) {
                risk_def->expr0 = spare_def->expr0;
                risk_let->data.let_decl.initializer = spare_def->expr0;
                bool evidence_refused = !mir_validate_collection_effect_evidence(
                    risk_mir, main_routine, NULL);
                bool full_refused = !mir_validate(risk_mir, NULL);
                bool emission_refused = !mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL);
                risk_let->data.let_decl.initializer = original;
                risk_def->expr0 = original;
                TEST("MIR risk cannot rebind both AST and DEF to another original array");
                EXPECT(evidence_refused && full_refused && emission_refused &&
                    mir_validate(risk_mir, NULL) &&
                    mir_validate_emission_contract(main_routine,
                        main_routine->has_cleanup_block, false, NULL));
            }
            MIRInstruction saved_push = *push;
            ASTNode saved_call = *push->expr0;
            uint32_t forged_source = ast_node_stable_id(ast_call_argument(push->expr0, 1));
            push->collection_ownership_effect_kind = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
            push->collection_ownership_source_binding_id = forged_source;
            push->expr0->data.call.semantic_collection_effect_kind = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
            push->expr0->data.call.semantic_collection_source_binding_id = forged_source;
            bool evidence_refused = !mir_validate_collection_effect_evidence(
                risk_mir, main_routine, NULL);
            bool full_refused = !mir_validate(risk_mir, NULL);
            bool emission_refused = !mir_validate_emission_contract(main_routine,
                main_routine->has_cleanup_block, false, NULL);
            *saved_push.expr0 = saved_call;
            *push = saved_push;
            TEST("MIR risk cannot promote a shallow effect through coherent AST and MIR mutation");
            EXPECT(forged_source != 0 && evidence_refused && full_refused && emission_refused &&
                mir_validate(risk_mir, NULL) &&
                mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL));
            MIRBasicBlock saved_block = *push_block;
            size_t count = saved_block.instruction_count;
            MIRInstruction *inserted = calloc(count + 1, sizeof(*inserted));
            TEST("MIR extra nonowner alias operation has isolated instruction storage");
            EXPECT(inserted != NULL);
            if (inserted != NULL) {
                memcpy(inserted, saved_block.instructions, push_index * sizeof(*inserted));
                inserted[push_index] = *assignment;
                inserted[push_index].kind = MIR_INST_ASSIGN;
                inserted[push_index].expr0 = ast_call_argument(push->expr0, 0);
                inserted[push_index].expr1 = risk_def->expr0;
                memcpy(&inserted[push_index + 1], &saved_block.instructions[push_index],
                    (count - push_index) * sizeof(*inserted));
                push_block->instructions = inserted;
                push_block->instruction_count = count + 1;
                push_block->instruction_capacity = count + 1;
                bool evidence_refused = !mir_validate_collection_effect_evidence(
                    risk_mir, main_routine, NULL);
                bool full_refused = !mir_validate(risk_mir, NULL);
                bool emission_refused = !mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL);
                *push_block = saved_block;
                free(inserted);
                TEST("MIR unclaimed later risk assignment cannot hide behind old claims");
                EXPECT(evidence_refused && full_refused && emission_refused &&
                    mir_validate(risk_mir, NULL) &&
                    mir_validate_emission_contract(main_routine,
                        main_routine->has_cleanup_block, false, NULL));
            }
        }
        mir_destroy(risk_mir); rir_destroy(risk_rir); hir_destroy(risk_hir);
    }
    {
        HIRProgram *return_hir = NULL;
        RIRProgram *return_rir = NULL;
        MIRProgram *return_mir = NULL;
        const char *source =
            "struct Arena{atoms:Array<String>;}"
            "func VariantAt(arena:Arena,index:Int)->Option<String>{"
            "if index<0||index>=ArrayLength(arena.atoms){return None;}"
            "return Some(arena.atoms[index]);}"
            "func NonownerRead(text:String)->Int{return StringLength(text);}"
            "func Build()->Array<String>{let arena=Arena([\"red,blue\"]);"
            "let selected:Option<String> = VariantAt(arena,0);"
            "let text:String=UnwrapOption(selected);"
            "let no_texts:Array<String> = [];Log(NonownerRead(text));"
            "Log(ArrayLength(no_texts));return [\"fallback\"]; }"
            "func Main()->Void{let rows=Build();Log(ArrayLength(rows));}";
        bool lowered = lower_mir_from_source(source,
            &return_hir, &return_rir, &return_mir);
        MIRRoutine *build = lowered ? find_mir_routine_mut(return_mir,
            "Build", MIR_SCOPE_FUNCTION) : NULL;
        MIRInstruction *returned = NULL;
        for (size_t b = 0; build != NULL && b < build->block_count; b++)
            for (size_t i = 0; i < build->blocks[b].instruction_count; i++)
                if (build->blocks[b].instructions[i].kind == MIR_INST_RETURN)
                    returned = &build->blocks[b].instructions[i];
        const ASTNode *program = return_mir == NULL ? NULL :
            return_mir->collection_effect_source_program;
        ASTNode *build_ast = program == NULL ? NULL : ast_program_statement(program, 3);
        ASTNode *body = build_ast == NULL ? NULL : ast_func_body(build_ast);
        ASTNode *length_log = body == NULL ? NULL : ast_block_statement(body, 5);
        ASTNode *length_call = length_log == NULL ? NULL : ast_call_argument(length_log, 0);
        ASTNode *risk_identifier = length_call == NULL ? NULL : ast_call_argument(length_call, 0);
        PgyCollectionEffectView view = {0};
        bool control = build != NULL && returned != NULL && returned->expr0 != NULL &&
            returned->expr0->type == AST_ARRAY_LITERAL &&
            risk_identifier != NULL && risk_identifier->type == AST_IDENTIFIER &&
            semantic_collection_effect_snapshot_current(
                return_mir->collection_effect_snapshot, program, &view) &&
            view.nonowner_risk_count == 1 &&
            mir_validate_collection_effect_evidence(return_mir, build, NULL) &&
            mir_validate(return_mir, NULL) &&
            mir_validate_emission_contract(build,
                build->has_cleanup_block, false, NULL);
        TEST("MIR unrelated fallback Array return has one current nonowner risk");
        EXPECT(control);
        if (control) {
            ASTNode *fallback = returned->expr0;
            returned->expr0 = risk_identifier;
            bool evidence_refused = !mir_validate_collection_effect_evidence(
                return_mir, build, NULL);
            bool full_refused = !mir_validate(return_mir, NULL);
            bool emission_refused = !mir_validate_emission_contract(build,
                build->has_cleanup_block, false, NULL);
            returned->expr0 = fallback;
            TEST("MIR direct RETURN cannot reuse a nonowner risk identifier");
            EXPECT(evidence_refused && full_refused && emission_refused &&
                mir_validate_collection_effect_evidence(return_mir, build, NULL) &&
                mir_validate(return_mir, NULL) &&
                mir_validate_emission_contract(build,
                    build->has_cleanup_block, false, NULL));
        }
        mir_destroy(return_mir); rir_destroy(return_rir); hir_destroy(return_hir);
    }
    {
        HIRProgram *terminal_hir = NULL;
        RIRProgram *terminal_rir = NULL;
        MIRProgram *terminal_mir = NULL;
        const char *source =
            "struct Arena{atoms:Array<String>;}struct Stored{values:Array<String>;}"
            "func VariantAt(arena:Arena,index:Int)->Option<String>{"
            "if index<0||index>=ArrayLength(arena.atoms){return None;}"
            "return Some(arena.atoms[index]);}"
            "func NonownerRead(text:String)->Int{return StringLength(text);}"
            "func Build()->Stored{"
            "let arena:Arena=Arena([\"red,blue\"]);"
            "let selected:Option<String> = VariantAt(arena,0);"
            "let text:String=UnwrapOption(selected);"
            "let no_texts:Array<String> = [];"
            "let spare:Stored = Stored([]);"
            "Log(NonownerRead(text));"
            "return Stored(no_texts);}"
            "func Main()->Void{let stored=Build();Log(ArrayLength(stored.values));}";
        bool lowered = lower_mir_from_source(source,
            &terminal_hir, &terminal_rir, &terminal_mir);
        MIRRoutine *build = lowered ? find_mir_routine_mut(terminal_mir,
            "Build", MIR_SCOPE_FUNCTION) : NULL;
        const ASTNode *program = terminal_mir == NULL ? NULL :
            terminal_mir->collection_effect_source_program;
        ASTNode *build_ast = program == NULL ? NULL : ast_program_statement(program, 4);
        ASTNode *body = build_ast == NULL ? NULL : ast_func_body(build_ast);
        ASTNode *outer_return = body == NULL ? NULL : ast_block_statement(body, 6);
        MIRInstruction *spare_def = NULL, *statement_template = NULL, *terminal_return = NULL;
        MIRBasicBlock *terminal_block = NULL;
        size_t terminal_index = 0;
        for (size_t b = 0; build != NULL && b < build->block_count; b++)
            for (size_t i = 0; i < build->blocks[b].instruction_count; i++) {
                MIRInstruction *inst = &build->blocks[b].instructions[i];
                if (inst->kind == MIR_INST_DEF && inst->arg0 != NULL &&
                    strcmp(inst->arg0, "spare") == 0) spare_def = inst;
                if (inst->kind == MIR_INST_STMT && inst->expr0 != NULL)
                    statement_template = inst;
                if (inst->kind == MIR_INST_RETURN && inst->expr0 != NULL &&
                    inst->expr0->type == AST_CALL) {
                    terminal_return = inst;
                    terminal_block = &build->blocks[b];
                    terminal_index = i;
                }
            }
        PgyCollectionEffectView view = {0};
        bool snapshot_current = terminal_mir != NULL && program != NULL &&
            semantic_collection_effect_snapshot_current(
                terminal_mir->collection_effect_snapshot, program, &view);
        bool evidence_current = snapshot_current && build != NULL &&
            mir_validate_collection_effect_evidence(terminal_mir, build, NULL);
        bool full_current = evidence_current && mir_validate(terminal_mir, NULL);
        bool emission_current = full_current &&
            mir_validate_emission_contract(build,
                build->has_cleanup_block, false, NULL);
        bool control = build != NULL && body != NULL && outer_return != NULL &&
            outer_return->type == AST_RETURN && snapshot_current &&
            view.nonowner_risk_count == 1 && evidence_current && full_current &&
            emission_current && spare_def != NULL && spare_def->expr0 != NULL &&
            statement_template != NULL && terminal_return != NULL &&
            terminal_return->expr0 == ast_return_value(outer_return) &&
            terminal_block != NULL;
        TEST("MIR nonowner terminal nominal return has an admitted risk snapshot");
        if (!control) printf("\n  terminal control lowered=%d build=%d body=%d return=%d snapshot=%d risks=%zu evidence=%d full=%d emission=%d\n",
            (int)lowered, build != NULL, body != NULL,
            outer_return != NULL && outer_return->type == AST_RETURN,
            (int)snapshot_current, view.nonowner_risk_count,
            (int)evidence_current, (int)full_current, (int)emission_current);
        EXPECT(control);
        if (control) {
            ASTNode *protected_constructor = terminal_return->expr0;
            ASTNode *original_spare_initializer = spare_def->expr0;
            spare_def->expr0 = protected_constructor;
            bool def_evidence_refused = !mir_validate_collection_effect_evidence(
                terminal_mir, build, NULL);
            bool def_full_refused = !mir_validate(terminal_mir, NULL);
            bool def_emission_refused = !mir_validate_emission_contract(build,
                build->has_cleanup_block, false, NULL);
            spare_def->expr0 = original_spare_initializer;
            TEST("MIR nominal terminal constructor cannot become a separate local DEF value");
            EXPECT(def_evidence_refused && def_full_refused && def_emission_refused &&
                mir_validate_collection_effect_evidence(terminal_mir, build, NULL) &&
                mir_validate(terminal_mir, NULL) &&
                mir_validate_emission_contract(build,
                    build->has_cleanup_block, false, NULL));

            MIRBasicBlock saved_terminal_block = *terminal_block;
            size_t instruction_count = saved_terminal_block.instruction_count;
            MIRInstruction *with_extra_statement =
                calloc(instruction_count + 1, sizeof(*with_extra_statement));
            TEST("MIR nominal terminal non-return evaluation has isolated storage");
            EXPECT(with_extra_statement != NULL);
            if (with_extra_statement != NULL) {
                memcpy(with_extra_statement, saved_terminal_block.instructions,
                    terminal_index * sizeof(*with_extra_statement));
                with_extra_statement[terminal_index] = *statement_template;
                with_extra_statement[terminal_index].expr0 = protected_constructor;
                memcpy(&with_extra_statement[terminal_index + 1],
                    &saved_terminal_block.instructions[terminal_index],
                    (instruction_count - terminal_index) * sizeof(*with_extra_statement));
                terminal_block->instructions = with_extra_statement;
                terminal_block->instruction_count = instruction_count + 1;
                terminal_block->instruction_capacity = instruction_count + 1;
                bool stmt_evidence_refused = !mir_validate_collection_effect_evidence(
                    terminal_mir, build, NULL);
                bool stmt_full_refused = !mir_validate(terminal_mir, NULL);
                bool stmt_emission_refused = !mir_validate_emission_contract(build,
                    build->has_cleanup_block, false, NULL);
                *terminal_block = saved_terminal_block;
                free(with_extra_statement);
                TEST("MIR nominal terminal constructor cannot replay as a non-return STMT");
                EXPECT(stmt_evidence_refused && stmt_full_refused && stmt_emission_refused &&
                    mir_validate_collection_effect_evidence(terminal_mir, build, NULL) &&
                    mir_validate(terminal_mir, NULL) &&
                    mir_validate_emission_contract(build,
                        build->has_cleanup_block, false, NULL));
            }

            ASTNode *capture = ast_create_lambda_expression();
            ASTNode *capture_body = ast_create_block();
            ASTNode **captured_statements = calloc(1, sizeof(*captured_statements));
            TEST("MIR nested-capture mutant has isolated AST storage");
            EXPECT(capture != NULL && capture_body != NULL && captured_statements != NULL);
            if (capture != NULL && capture_body != NULL && captured_statements != NULL) {
                capture->stable_id = UINT32_MAX - 1;
                capture_body->stable_id = UINT32_MAX;
                captured_statements[0] = outer_return;
                capture_body->data.block.statements = captured_statements;
                capture_body->data.block.count = 1;
                capture_body->data.block.capacity = 1;
                capture->data.lambda_expr.body = capture_body;
                /* Original claims, terminal mapping, and MIR are unchanged.
                 * The live AST's original RETURN is now under a new, unclaimed
                 * lambda parent rather than Build's terminal statement. */
                body->data.block.statements[6] = capture;
                bool evidence_refused = !mir_validate_collection_effect_evidence(
                    terminal_mir, build, NULL);
                bool full_refused = !mir_validate(terminal_mir, NULL);
                bool emission_refused = !mir_validate_emission_contract(build,
                    build->has_cleanup_block, false, NULL);
                body->data.block.statements[6] = outer_return;
                captured_statements[0] = NULL;
                TEST("MIR unclaimed nested lambda cannot disguise a nominal terminal return");
                EXPECT(evidence_refused && full_refused && emission_refused &&
                    mir_validate_collection_effect_evidence(terminal_mir, build, NULL) &&
                    mir_validate(terminal_mir, NULL) &&
                    mir_validate_emission_contract(build,
                        build->has_cleanup_block, false, NULL));
                ast_destroy(capture);
            } else {
                if (capture != NULL && capture_body != NULL) {
                    capture->data.lambda_expr.body = capture_body;
                    ast_destroy(capture);
                } else {
                    ast_destroy(capture);
                    ast_destroy(capture_body);
                }
                free(captured_statements);
            }
        }
        mir_destroy(terminal_mir); rir_destroy(terminal_rir); hir_destroy(terminal_hir);
    }
    {
        HIRProgram *defs_hir = NULL;
        RIRProgram *defs_rir = NULL;
        MIRProgram *defs_mir = NULL;
        const char *source =
            "func MakeVariant()->String{return Substring(\"ready\",0,5);}"
            "func TrimReuse(value:String,trimmed:Bool)->String{"
            "if trimmed{return Substring(value,0,StringLength(value));}return value;}"
            "func Main()->Void{let variant:String=MakeVariant();"
            "let variant_name:String=variant;let names:Array<String> = [];"
            "let borrowed_rows:Array<String> = [\"seed\"];Log(borrowed_rows[0]);"
            "ArrayPush(names,variant_name);"
            "if StringLength(variant)>0{variant_name=TrimReuse(variant,true);}"
            "ArrayPush(names,variant_name);Log(names[0]);Log(names[1]);"
            "let foreign_text:String=\"left\";foreign_text=\"right\";Log(foreign_text);}";
        bool lowered = lower_mir_from_source(source, &defs_hir, &defs_rir, &defs_mir);
        MIRRoutine *main_routine = lowered ? find_mir_routine_mut(defs_mir,
            "Main", MIR_SCOPE_FUNCTION) : NULL;
        const ASTNode *program = defs_mir == NULL ? NULL :
            defs_mir->collection_effect_source_program;
        ASTNode *main_ast = program == NULL ? NULL : ast_program_statement(program, 2);
        ASTNode *body = main_ast == NULL ? NULL : ast_func_body(main_ast);
        ASTNode *birth_ast = body == NULL ? NULL : ast_block_statement(body, 1);
        ASTNode *decoy_log = body == NULL ? NULL : ast_block_statement(body, 4);
        ASTNode *borrowed_access = decoy_log == NULL ? NULL :
            ast_call_argument(decoy_log, 0);
        ASTNode *first_push = body == NULL ? NULL : ast_block_statement(body, 5);
        ASTNode *second_push = body == NULL ? NULL : ast_block_statement(body, 7);
        ASTNode *branch = body == NULL ? NULL : ast_block_statement(body, 6);
        ASTNode *assignment_ast = branch == NULL ? NULL :
            ast_block_statement(ast_if_then_branch(branch), 0);
        ASTNode *protected_target = assignment_ast == NULL ? NULL :
            ast_assignment_target(assignment_ast);
        ASTNode *foreign_assignment_ast = body == NULL ? NULL : ast_block_statement(body, 11);
        ASTNode *foreign_target = foreign_assignment_ast == NULL ? NULL :
            ast_assignment_target(foreign_assignment_ast);
        ASTNode *protected_use = second_push == NULL ? NULL :
            ast_call_argument(second_push, 1);
        MIRInstruction *birth_inst = NULL, *assignment = NULL, *second_push_inst = NULL;
        MIRBasicBlock *birth_block = NULL;
        size_t birth_index = 0, birth_count = 0;
        ASTNode **assignment_rhs_slot = NULL;
        size_t assignment_count = 0;
        ASTNode **foreign_rhs_slot = NULL;
        size_t foreign_count = 0;
        MIRBasicBlock *second_block = NULL;
        size_t second_index = 0;
        for (size_t b = 0; main_routine != NULL && b < main_routine->block_count; b++)
            for (size_t i = 0; i < main_routine->blocks[b].instruction_count; i++) {
                MIRInstruction *inst = &main_routine->blocks[b].instructions[i];
                if (inst->kind == MIR_INST_DEF && inst->ast == birth_ast) {
                    birth_inst = inst;
                    birth_block = &main_routine->blocks[b];
                    birth_index = i;
                    birth_count++;
                }
                if ((inst->kind == MIR_INST_ASSIGN || inst->kind == MIR_INST_DEF) &&
                    inst->ast == assignment_ast) {
                    assignment = inst;
                    assignment_rhs_slot = inst->kind == MIR_INST_DEF ?
                        &inst->expr0 : &inst->expr1;
                    assignment_count++;
                }
                if ((inst->kind == MIR_INST_ASSIGN || inst->kind == MIR_INST_DEF) &&
                    inst->ast == foreign_assignment_ast) {
                    foreign_rhs_slot = inst->kind == MIR_INST_DEF ?
                        &inst->expr0 : &inst->expr1;
                    foreign_count++;
                }
                if (inst->kind == MIR_INST_STMT && inst->expr0 == second_push) {
                    second_push_inst = inst;
                    second_block = &main_routine->blocks[b];
                    second_index = i;
                }
            }
        PgyCollectionEffectView view = {0};
        bool control = main_routine != NULL && birth_ast != NULL &&
            birth_ast->type == AST_LET_DECL && birth_count == 1 && birth_inst != NULL &&
            birth_block != NULL && birth_inst->expr0 == ast_let_initializer(birth_ast) &&
            borrowed_access != NULL &&
            borrowed_access->type == AST_ARRAY_ACCESS && first_push != NULL &&
            second_push != NULL && assignment_count == 1 &&
            assignment != NULL && assignment->kind == MIR_INST_DEF &&
            protected_target != NULL && assignment->expr1 == protected_target &&
            assignment_rhs_slot != NULL &&
            *assignment_rhs_slot == ast_assignment_value(assignment_ast) &&
            foreign_target != NULL && protected_use != NULL && foreign_count == 1 &&
            foreign_rhs_slot != NULL &&
            *foreign_rhs_slot == ast_assignment_value(foreign_assignment_ast) &&
            second_push_inst != NULL && second_block != NULL &&
            semantic_collection_effect_snapshot_current(defs_mir->collection_effect_snapshot,
                program, &view) && view.string_definition_count == 1 &&
            mir_validate_collection_effect_evidence(defs_mir, main_routine, NULL) &&
            mir_validate(defs_mir, NULL) &&
            mir_validate_emission_contract(main_routine,
                main_routine->has_cleanup_block, false, NULL);
        TEST("MIR all-def0 branch has one original assignment and two protected uses");
        EXPECT(control);
        if (control) {
            const PgyCollectionEffectSnapshot *snapshot = defs_mir->collection_effect_snapshot;
            uint32_t caller_id = main_routine->source_syntax_id;
            uint32_t birth_binding = ast_node_stable_id(birth_ast);
            uint32_t target_binding = ast_identifier_binding_syntax_id(protected_target);
            uint32_t foreign_binding = ast_identifier_binding_syntax_id(foreign_target);
            size_t birth_ordinal = SIZE_MAX, repeated_birth_ordinal = SIZE_MAX;
            size_t assignment_ordinal = SIZE_MAX, repeated_assignment_ordinal = SIZE_MAX;
            bool birth_admitted = semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, birth_binding, true, birth_ast, birth_ast,
                ast_let_initializer(birth_ast), &birth_ordinal);
            bool birth_reused = semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, birth_binding, true, birth_ast, birth_ast,
                ast_let_initializer(birth_ast), &repeated_birth_ordinal);
            bool assignment_admitted = semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, target_binding, false, assignment_ast, protected_target,
                ast_assignment_value(assignment_ast), &assignment_ordinal);
            bool assignment_reused = semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, target_binding, false, assignment_ast, protected_target,
                ast_assignment_value(assignment_ast), &repeated_assignment_ordinal);
            TEST("MIR all-def0 exact birth and assignment proofs remain reusable");
            EXPECT(view.string_write_count == 2 && birth_binding != 0 && target_binding != 0 &&
                birth_admitted && birth_reused && assignment_admitted && assignment_reused &&
                birth_ordinal < view.string_write_count &&
                assignment_ordinal < view.string_write_count &&
                birth_ordinal != assignment_ordinal &&
                birth_ordinal == repeated_birth_ordinal &&
                assignment_ordinal == repeated_assignment_ordinal);

            size_t missing_birth_ordinal = SIZE_MAX, wrong_birth_ordinal = SIZE_MAX;
            size_t missing_assignment_ordinal = SIZE_MAX, wrong_assignment_ordinal = SIZE_MAX;
            bool missing_birth_refused = !semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, birth_binding, true, NULL, birth_ast,
                ast_let_initializer(birth_ast), &missing_birth_ordinal);
            bool wrong_birth_refused = !semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, birth_binding, true, foreign_assignment_ast, birth_ast,
                ast_let_initializer(birth_ast), &wrong_birth_ordinal);
            bool missing_assignment_refused = !semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, target_binding, false, NULL, protected_target,
                ast_assignment_value(assignment_ast), &missing_assignment_ordinal);
            bool wrong_assignment_refused = !semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, target_binding, false, foreign_assignment_ast, protected_target,
                ast_assignment_value(assignment_ast), &wrong_assignment_ordinal);
            TEST("MIR all-def0 selected writes require their original source statements");
            EXPECT(missing_birth_refused && wrong_birth_refused && missing_assignment_refused &&
                wrong_assignment_refused && missing_birth_ordinal == SIZE_MAX &&
                wrong_birth_ordinal == SIZE_MAX && missing_assignment_ordinal == SIZE_MAX &&
                wrong_assignment_ordinal == SIZE_MAX);

            size_t foreign_ordinal = SIZE_MAX, protected_rhs_ordinal = SIZE_MAX;
            bool foreign_role_admitted = foreign_binding != 0 &&
                semantic_collection_effect_snapshot_source_definition(snapshot, caller_id,
                    foreign_binding, false, foreign_assignment_ast, foreign_target,
                    ast_assignment_value(foreign_assignment_ast), &foreign_ordinal);
            bool protected_rhs_refused = !semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, foreign_binding, false, foreign_assignment_ast,
                foreign_target, protected_use, &protected_rhs_ordinal);
            bool bare_evaluation_refused = !semantic_collection_effect_snapshot_source_evaluation(
                snapshot, caller_id, protected_use);
            TEST("MIR all-def0 foreign String assignment has a real admitted role");
            EXPECT(foreign_role_admitted && foreign_ordinal == SIZE_MAX);
            TEST("MIR all-def0 protected String rejects a foreign assignment RHS role");
            EXPECT(protected_rhs_refused && protected_rhs_ordinal == SIZE_MAX);
            TEST("MIR all-def0 protected String rejects a bare evaluation role");
            EXPECT(bare_evaluation_refused);
            ASTNode *saved_foreign_rhs = *foreign_rhs_slot;
            *foreign_rhs_slot = protected_use;
            bool foreign_evidence_refused = !mir_validate_collection_effect_evidence(
                defs_mir, main_routine, NULL);
            bool foreign_full_refused = !mir_validate(defs_mir, NULL);
            bool foreign_emission_refused = !mir_validate_emission_contract(main_routine,
                main_routine->has_cleanup_block, false, NULL);
            *foreign_rhs_slot = saved_foreign_rhs;
            bool foreign_evidence_restored = mir_validate_collection_effect_evidence(
                defs_mir, main_routine, NULL);
            bool foreign_full_restored = mir_validate(defs_mir, NULL);
            bool foreign_emission_restored = mir_validate_emission_contract(main_routine,
                main_routine->has_cleanup_block, false, NULL);
            TEST("MIR all-def0 foreign RHS substitution rejects collection evidence");
            EXPECT(foreign_evidence_refused);
            TEST("MIR all-def0 foreign RHS substitution rejects full validation");
            EXPECT(foreign_full_refused);
            TEST("MIR all-def0 foreign RHS substitution rejects emission");
            EXPECT(foreign_emission_refused);
            TEST("MIR all-def0 foreign RHS restores the original admitted role");
            EXPECT(foreign_evidence_restored && foreign_full_restored &&
                foreign_emission_restored);

            MIRInstruction saved_assignment = *assignment;
            assignment->kind = MIR_INST_ASSIGN;
            assignment->expr0 = protected_target;
            assignment->expr1 = ast_assignment_value(assignment_ast);
            bool target_evaluation_refused = !semantic_collection_effect_snapshot_source_evaluation(
                snapshot, caller_id, protected_target);
            size_t target_ordinal = SIZE_MAX;
            bool target_definition_admitted = semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, target_binding, false, assignment_ast,
                protected_target, assignment->expr1, &target_ordinal);
            bool assign_layout_owner_admitted = mir_validate_collection_effect_evidence(
                defs_mir, main_routine, NULL);
            assignment->expr1 = borrowed_access;
            bool assign_layout_wrong_rhs_refused = !mir_validate_collection_effect_evidence(
                defs_mir, main_routine, NULL);
            *assignment = saved_assignment;
            bool assign_layout_owner_restored = mir_validate_collection_effect_evidence(
                defs_mir, main_routine, NULL);
            TEST("MIR all-def0 ASSIGN target role accepts the original protected place");
            EXPECT(target_evaluation_refused && target_definition_admitted &&
                target_ordinal == assignment_ordinal && assign_layout_owner_admitted);
            TEST("MIR all-def0 ASSIGN target role cannot admit an array-derived RHS");
            EXPECT(assign_layout_wrong_rhs_refused);
            TEST("MIR all-def0 ASSIGN layout restores its original DEF owner evidence");
            EXPECT(assign_layout_owner_restored);

            ASTNode *saved_rhs = *assignment_rhs_slot;
            *assignment_rhs_slot = borrowed_access;
            bool evidence_refused = !mir_validate_collection_effect_evidence(
                defs_mir, main_routine, NULL);
            bool full_refused = !mir_validate(defs_mir, NULL);
            bool emission_refused = !mir_validate_emission_contract(main_routine,
                main_routine->has_cleanup_block, false, NULL);
            *assignment_rhs_slot = saved_rhs;
            TEST("MIR all-def0 actual assignment RHS cannot become array-derived");
            EXPECT(evidence_refused && full_refused && emission_refused &&
                mir_validate_collection_effect_evidence(defs_mir, main_routine, NULL) &&
                mir_validate(defs_mir, NULL) &&
                mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL));

            ASTNode *saved_sink = second_push_inst->expr0;
            second_push_inst->expr0 = first_push;
            bool pure_pair_reuse = mir_validate_collection_effect_evidence(
                defs_mir, main_routine, NULL) && mir_validate(defs_mir, NULL) &&
                mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL);
            second_push_inst->expr0 = saved_sink;
            TEST("MIR all-def0 complete pure source-use pair remains reusable");
            EXPECT(pure_pair_reuse);

            ASTNode changed_sink = *second_push;
            ASTNode *changed_args[2] = {ast_call_argument(second_push, 0),
                ast_call_argument(first_push, 1)};
            changed_sink.data.call.arguments = changed_args;
            bool changed_slot_refused = !semantic_collection_effect_snapshot_source_use(
                defs_mir->collection_effect_snapshot, main_routine->source_syntax_id,
                second_push, 1, changed_args[1]);
            second_push_inst->expr0 = &changed_sink;
            evidence_refused = !mir_validate_collection_effect_evidence(
                defs_mir, main_routine, NULL);
            full_refused = !mir_validate(defs_mir, NULL);
            emission_refused = !mir_validate_emission_contract(main_routine,
                main_routine->has_cleanup_block, false, NULL);
            second_push_inst->expr0 = saved_sink;
            TEST("MIR all-def0 selected sink cannot adopt a foreign actual");
            EXPECT(changed_slot_refused && evidence_refused && full_refused && emission_refused &&
                mir_validate_collection_effect_evidence(defs_mir, main_routine, NULL) &&
                mir_validate(defs_mir, NULL) &&
                mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL));

            MIRBasicBlock saved_block = *second_block;
            size_t count = saved_block.instruction_count;
            MIRInstruction *inserted = calloc(count + 1, sizeof(*inserted));
            TEST("MIR all-def0 extra write has isolated instruction storage");
            EXPECT(inserted != NULL);
            if (inserted != NULL) {
                memcpy(inserted, saved_block.instructions, second_index * sizeof(*inserted));
                inserted[second_index] = *assignment;
                memcpy(&inserted[second_index + 1],
                    &saved_block.instructions[second_index],
                    (count - second_index) * sizeof(*inserted));
                second_block->instructions = inserted;
                second_block->instruction_count = count + 1;
                second_block->instruction_capacity = count + 1;
                bool duplicate_evidence_refused = !mir_validate_collection_effect_evidence(
                    defs_mir, main_routine, NULL);
                bool duplicate_full_refused = !mir_validate(defs_mir, NULL);
                bool duplicate_emission_refused = !mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL);
                *second_block = saved_block;
                bool duplicate_evidence_restored = mir_validate_collection_effect_evidence(
                    defs_mir, main_routine, NULL);
                bool duplicate_full_restored = mir_validate(defs_mir, NULL);
                bool duplicate_emission_restored = mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL);
                TEST("MIR all-def0 exact duplicate assignment rejects collection evidence");
                EXPECT(duplicate_evidence_refused);
                TEST("MIR all-def0 exact duplicate assignment rejects full validation");
                EXPECT(duplicate_full_refused);
                TEST("MIR all-def0 exact duplicate assignment rejects emission");
                EXPECT(duplicate_emission_refused);
                TEST("MIR all-def0 exact duplicate assignment restores the original block");
                EXPECT(duplicate_evidence_restored && duplicate_full_restored &&
                    duplicate_emission_restored);

                ASTNode **inserted_rhs_slot = inserted[second_index].kind == MIR_INST_DEF ?
                    &inserted[second_index].expr0 : &inserted[second_index].expr1;
                *inserted_rhs_slot = borrowed_access;
                second_block->instructions = inserted;
                second_block->instruction_count = count + 1;
                second_block->instruction_capacity = count + 1;
                evidence_refused = !mir_validate_collection_effect_evidence(
                    defs_mir, main_routine, NULL);
                full_refused = !mir_validate(defs_mir, NULL);
                emission_refused = !mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL);
                *second_block = saved_block;
                free(inserted);
                TEST("MIR all-def0 rejects an unclaimed later array-derived write");
                EXPECT(evidence_refused && full_refused && emission_refused &&
                    mir_validate_collection_effect_evidence(defs_mir, main_routine, NULL) &&
                    mir_validate(defs_mir, NULL) &&
                    mir_validate_emission_contract(main_routine,
                        main_routine->has_cleanup_block, false, NULL));
            }

            MIRBasicBlock saved_birth_block = *birth_block;
            size_t birth_instruction_count = saved_birth_block.instruction_count;
            MIRInstruction *birth_inserted = calloc(birth_instruction_count + 1,
                sizeof(*birth_inserted));
            TEST("MIR all-def0 duplicate birth has isolated instruction storage");
            EXPECT(birth_inserted != NULL);
            if (birth_inserted != NULL) {
                memcpy(birth_inserted, saved_birth_block.instructions,
                    (birth_index + 1) * sizeof(*birth_inserted));
                birth_inserted[birth_index + 1] = *birth_inst;
                memcpy(&birth_inserted[birth_index + 2],
                    &saved_birth_block.instructions[birth_index + 1],
                    (birth_instruction_count - birth_index - 1) * sizeof(*birth_inserted));
                birth_block->instructions = birth_inserted;
                birth_block->instruction_count = birth_instruction_count + 1;
                birth_block->instruction_capacity = birth_instruction_count + 1;
                bool birth_duplicate_evidence_refused = !mir_validate_collection_effect_evidence(
                    defs_mir, main_routine, NULL);
                bool birth_duplicate_full_refused = !mir_validate(defs_mir, NULL);
                bool birth_duplicate_emission_refused = !mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL);
                *birth_block = saved_birth_block;
                free(birth_inserted);
                TEST("MIR all-def0 exact duplicate birth rejects collection evidence");
                EXPECT(birth_duplicate_evidence_refused);
                TEST("MIR all-def0 exact duplicate birth rejects full validation");
                EXPECT(birth_duplicate_full_refused);
                TEST("MIR all-def0 exact duplicate birth rejects emission");
                EXPECT(birth_duplicate_emission_refused);
                TEST("MIR all-def0 exact duplicate birth restores the original block");
                EXPECT(mir_validate_collection_effect_evidence(defs_mir, main_routine, NULL) &&
                    mir_validate(defs_mir, NULL) &&
                    mir_validate_emission_contract(main_routine,
                        main_routine->has_cleanup_block, false, NULL));
            }
        }
        mir_destroy(defs_mir); rir_destroy(defs_rir); hir_destroy(defs_hir);
    }
    {
        HIRProgram *return_hir = NULL;
        RIRProgram *return_rir = NULL;
        MIRProgram *return_mir = NULL;
        const char *source =
            "func MakeVariant()->String{return Substring(\"ready\",0,5);}"
            "func TrimReuse(value:String)->String{return Substring(value,0,StringLength(value));}"
            "func BuildText()->String{let variant:String=MakeVariant();"
            "let variant_name:String=variant;let names:Array<String> = [];"
            "ArrayPush(names,variant_name);variant_name=TrimReuse(variant);"
            "ArrayPush(names,variant_name);return \"steady\";}"
            "func Main()->Void{Log(BuildText());}";
        bool lowered = lower_mir_from_source(source, &return_hir, &return_rir, &return_mir);
        MIRRoutine *build = lowered ? find_mir_routine_mut(return_mir,
            "BuildText", MIR_SCOPE_FUNCTION) : NULL;
        const ASTNode *program = return_mir == NULL ? NULL :
            return_mir->collection_effect_source_program;
        ASTNode *build_ast = program == NULL ? NULL : ast_program_statement(program, 2);
        ASTNode *body = build_ast == NULL ? NULL : ast_func_body(build_ast);
        ASTNode *push = body == NULL ? NULL : ast_block_statement(body, 5);
        ASTNode *protected_use = push == NULL ? NULL : ast_call_argument(push, 1);
        ASTNode *return_statement = body == NULL ? NULL : ast_block_statement(body, 6);
        ASTNode *original_value = return_statement == NULL ? NULL :
            ast_return_value(return_statement);
        MIRInstruction *returned = NULL;
        size_t return_count = 0;
        for (size_t b = 0; build != NULL && b < build->block_count; b++)
            for (size_t i = 0; i < build->blocks[b].instruction_count; i++) {
                MIRInstruction *inst = &build->blocks[b].instructions[i];
                if (inst->kind == MIR_INST_RETURN && inst->expr0 == original_value) {
                    returned = inst;
                    return_count++;
                }
            }
        PgyCollectionEffectView view = {0};
        bool control = build != NULL && build_ast != NULL && protected_use != NULL &&
            return_statement != NULL && return_statement->type == AST_RETURN &&
            original_value != NULL && return_count == 1 && returned != NULL &&
            returned->expr0 == original_value &&
            semantic_collection_effect_snapshot_current(return_mir->collection_effect_snapshot,
                program, &view) && view.string_definition_count == 1 &&
            semantic_collection_effect_snapshot_source_return(return_mir->collection_effect_snapshot,
                build->source_syntax_id, return_statement, original_value) &&
            mir_validate_collection_effect_evidence(return_mir, build, NULL) &&
            mir_validate(return_mir, NULL) &&
            mir_validate_emission_contract(build, build->has_cleanup_block, false, NULL);
        TEST("MIR all-def0 has an admitted same-caller String RETURN control");
        EXPECT(control);
        if (control) {
            bool direct_refused = !semantic_collection_effect_snapshot_source_return(
                return_mir->collection_effect_snapshot, build->source_syntax_id,
                return_statement, protected_use);
            ASTNode *saved = returned->expr0;
            returned->expr0 = protected_use;
            bool evidence_refused = !mir_validate_collection_effect_evidence(return_mir, build, NULL);
            bool full_refused = !mir_validate(return_mir, NULL);
            bool emission_refused = !mir_validate_emission_contract(build,
                build->has_cleanup_block, false, NULL);
            returned->expr0 = saved;
            bool direct_restored = semantic_collection_effect_snapshot_source_return(
                return_mir->collection_effect_snapshot, build->source_syntax_id,
                return_statement, original_value);
            bool evidence_restored = mir_validate_collection_effect_evidence(
                return_mir, build, NULL);
            bool full_restored = mir_validate(return_mir, NULL);
            bool emission_restored = mir_validate_emission_contract(build,
                build->has_cleanup_block, false, NULL);
            TEST("MIR all-def0 protected use rejects the original RETURN role directly");
            EXPECT(direct_refused);
            TEST("MIR all-def0 protected RETURN substitution rejects collection evidence");
            EXPECT(evidence_refused);
            TEST("MIR all-def0 protected RETURN substitution rejects full validation");
            EXPECT(full_refused);
            TEST("MIR all-def0 protected RETURN substitution rejects emission");
            EXPECT(emission_refused);
            TEST("MIR all-def0 RETURN restores the original admitted role");
            EXPECT(direct_restored && evidence_restored && full_restored && emission_restored);
        }
        mir_destroy(return_mir); rir_destroy(return_rir); hir_destroy(return_hir);
    }
    {
        HIRProgram *option_hir = NULL;
        RIRProgram *option_rir = NULL;
        MIRProgram *option_mir = NULL;
        const char *source =
            "func Choose(value:String,kind:Int)->Option<String>{"
            "if kind==0{return None;}"
            "if kind==1{return Some(Substring(value,0,StringLength(value)));}"
            "return Some(\"literal\");}"
            "func Main()->Void{let rows:Array<String> = [];"
            "let candidate:Option<String> = Choose(\"ready\",1);"
            "let alternate:Option<String> = Some(\"other\");"
            "var foreign:Option<String> = None;foreign=Some(\"other\");"
            "Log(IsSome(alternate));"
            "if IsSome(candidate){let text:String=UnwrapOption(candidate);"
            "let liveRisk:Array<String> = [Concat(\"he\",\"ap\")];"
            "ArrayPush(rows,text);Log(rows[0]);Log(liveRisk[0]);}}";
        bool lowered = lower_mir_from_source(source, &option_hir, &option_rir, &option_mir);
        MIRRoutine *main_routine = lowered ? find_mir_routine_mut(option_mir,
            "Main", MIR_SCOPE_FUNCTION) : NULL;
        MIRRoutine *choose_routine = lowered ? find_mir_routine_mut(option_mir,
            "Choose", MIR_SCOPE_FUNCTION) : NULL;
        const ASTNode *program = option_mir == NULL ? NULL :
            option_mir->collection_effect_source_program;
        ASTNode *choose_ast = program == NULL ? NULL : ast_program_statement(program, 0);
        ASTNode *main_ast = program == NULL ? NULL : ast_program_statement(program, 1);
        ASTNode *choose_body = choose_ast == NULL ? NULL : ast_func_body(choose_ast);
        ASTNode *none_branch = choose_body == NULL ? NULL : ast_block_statement(choose_body, 0);
        ASTNode *some_branch = choose_body == NULL ? NULL : ast_block_statement(choose_body, 1);
        ASTNode *none_statement = none_branch == NULL ? NULL :
            ast_block_statement(ast_if_then_branch(none_branch), 0);
        ASTNode *some_statement = some_branch == NULL ? NULL :
            ast_block_statement(ast_if_then_branch(some_branch), 0);
        ASTNode *none_value = none_statement == NULL ? NULL : ast_return_value(none_statement);
        ASTNode *some_value = some_statement == NULL ? NULL : ast_return_value(some_statement);
        ASTNode *body = main_ast == NULL ? NULL : ast_func_body(main_ast);
        ASTNode *carrier = body == NULL ? NULL : ast_block_statement(body, 1);
        ASTNode *alternate = body == NULL ? NULL : ast_block_statement(body, 2);
        ASTNode *foreign_assignment = body == NULL ? NULL : ast_block_statement(body, 4);
        ASTNode *alternate_log = body == NULL ? NULL : ast_block_statement(body, 5);
        ASTNode *is_some_alternate = alternate_log == NULL ? NULL :
            ast_call_argument(alternate_log, 0);
        ASTNode *alternate_use = is_some_alternate == NULL ? NULL :
            ast_call_argument(is_some_alternate, 0);
        ASTNode *branch = body == NULL ? NULL : ast_block_statement(body, 6);
        ASTNode *inside = branch == NULL ? NULL : ast_if_then_branch(branch);
        ASTNode *definition = inside == NULL ? NULL : ast_block_statement(inside, 0);
        ASTNode *unwrap = definition == NULL ? NULL : ast_let_initializer(definition);
        ASTNode *carrier_use = unwrap == NULL ? NULL : ast_call_argument(unwrap, 0);
        ASTNode *push = inside == NULL ? NULL : ast_block_statement(inside, 2);
        ASTNode *text_use = push == NULL ? NULL : ast_call_argument(push, 1);
        MIRInstruction *carrier_inst = NULL, *text_inst = NULL, *foreign_inst = NULL;
        MIRInstruction *push_inst = NULL, *some_return_inst = NULL;
        MIRBasicBlock *carrier_block = NULL;
        size_t carrier_index = 0;
        ASTNode **foreign_rhs_slot = NULL;
        for (size_t b = 0; main_routine != NULL && b < main_routine->block_count; b++)
            for (size_t i = 0; i < main_routine->blocks[b].instruction_count; i++) {
                MIRInstruction *inst = &main_routine->blocks[b].instructions[i];
                if (inst->kind == MIR_INST_DEF && inst->ast == carrier) {
                    carrier_inst = inst;
                    carrier_block = &main_routine->blocks[b];
                    carrier_index = i;
                }
                if (inst->kind == MIR_INST_DEF && inst->ast == definition) text_inst = inst;
                if ((inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_ASSIGN) &&
                    inst->ast == foreign_assignment) {
                    foreign_inst = inst;
                    foreign_rhs_slot = inst->kind == MIR_INST_DEF ? &inst->expr0 : &inst->expr1;
                }
                if (inst->kind == MIR_INST_STMT && inst->expr0 == push) push_inst = inst;
            }
        for (size_t b = 0; choose_routine != NULL && b < choose_routine->block_count; b++)
            for (size_t i = 0; i < choose_routine->blocks[b].instruction_count; i++) {
                MIRInstruction *inst = &choose_routine->blocks[b].instructions[i];
                if (inst->kind == MIR_INST_RETURN && inst->expr0 == some_value)
                    some_return_inst = inst;
            }
        PgyCollectionEffectView view = {0};
        bool control = lowered && main_routine != NULL && choose_routine != NULL &&
            carrier != NULL && alternate != NULL && foreign_inst != NULL &&
            foreign_rhs_slot != NULL && *foreign_rhs_slot == ast_assignment_value(foreign_assignment) &&
            alternate_use != NULL && none_value != NULL && some_value != NULL &&
            unwrap != NULL && carrier_use != NULL && text_use != NULL &&
            carrier_inst != NULL && carrier_block != NULL &&
            carrier_inst->expr0 == ast_let_initializer(carrier) &&
            text_inst != NULL && text_inst->expr0 == unwrap &&
            push_inst != NULL && some_return_inst != NULL &&
            semantic_collection_effect_snapshot_current(option_mir->collection_effect_snapshot,
                program, &view) && view.string_definition_count != 0 &&
            mir_validate_collection_effect_evidence(option_mir, main_routine, NULL) &&
            mir_validate(option_mir, NULL) &&
            mir_validate_emission_contract(main_routine,
                main_routine->has_cleanup_block, false, NULL);
        TEST("MIR Option payload has an admitted original carrier and Some/None producer");
        EXPECT(control);
        if (control) {
            const PgyCollectionEffectSnapshot *snapshot = option_mir->collection_effect_snapshot;
            uint32_t caller_id = main_routine->source_syntax_id;
            size_t carrier_ordinal = SIZE_MAX, foreign_ordinal = SIZE_MAX;
            bool carrier_birth_current = semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, ast_node_stable_id(carrier), true,
                carrier, carrier, ast_let_initializer(carrier), &carrier_ordinal);
            bool foreign_birth_refused = !semantic_collection_effect_snapshot_source_definition(
                snapshot, caller_id, ast_node_stable_id(carrier), true,
                carrier, carrier, ast_let_initializer(alternate), &foreign_ordinal);
            bool some_return_current = semantic_collection_effect_snapshot_source_return(
                snapshot, choose_routine->source_syntax_id, some_statement, some_value);
            bool none_crosswire_refused = !semantic_collection_effect_snapshot_source_return(
                snapshot, choose_routine->source_syntax_id, some_statement, none_value);
            TEST("MIR Option source owner binds exact carrier birth and Some RETURN roles");
            EXPECT(carrier_birth_current && carrier_ordinal < view.string_write_count &&
                foreign_birth_refused && foreign_ordinal == SIZE_MAX &&
                some_return_current && none_crosswire_refused);
            bool pure_once = semantic_collection_effect_snapshot_source_use(snapshot,
                caller_id, push, 1, text_use);
            bool pure_again = semantic_collection_effect_snapshot_source_use(snapshot,
                caller_id, push, 1, text_use);
            TEST("MIR Option payload complete pure call-actual proof is reusable");
            EXPECT(pure_once && pure_again);

            ASTNode *saved_carrier_rhs = carrier_inst->expr0;
            carrier_inst->expr0 = ast_let_initializer(alternate);
            bool carrier_evidence_refused = !mir_validate_collection_effect_evidence(
                option_mir, main_routine, NULL);
            bool carrier_full_refused = !mir_validate(option_mir, NULL);
            bool carrier_emission_refused = !mir_validate_emission_contract(main_routine,
                main_routine->has_cleanup_block, false, NULL);
            carrier_inst->expr0 = saved_carrier_rhs;
            TEST("MIR Option carrier DEF cannot adopt another Option initializer");
            EXPECT(carrier_evidence_refused && carrier_full_refused && carrier_emission_refused &&
                mir_validate_collection_effect_evidence(option_mir, main_routine, NULL));

            ASTNode changed_unwrap = *unwrap;
            ASTNode *changed_actuals[1] = {alternate_use};
            changed_unwrap.data.call.arguments = changed_actuals;
            ASTNode *saved_unwrap = text_inst->expr0;
            text_inst->expr0 = &changed_unwrap;
            bool actual_evidence_refused = !mir_validate_collection_effect_evidence(
                option_mir, main_routine, NULL);
            bool actual_full_refused = !mir_validate(option_mir, NULL);
            bool actual_emission_refused = !mir_validate_emission_contract(main_routine,
                main_routine->has_cleanup_block, false, NULL);
            text_inst->expr0 = saved_unwrap;
            TEST("MIR UnwrapOption cannot adopt a foreign original Option actual");
            EXPECT(actual_evidence_refused && actual_full_refused && actual_emission_refused &&
                mir_validate_collection_effect_evidence(option_mir, main_routine, NULL));

            ASTNode *saved_some = some_return_inst->expr0;
            some_return_inst->expr0 = none_value;
            bool return_evidence_refused = !mir_validate_collection_effect_evidence(
                option_mir, choose_routine, NULL);
            bool return_full_refused = !mir_validate(option_mir, NULL);
            bool return_emission_refused = !mir_validate_emission_contract(choose_routine,
                choose_routine->has_cleanup_block, false, NULL);
            some_return_inst->expr0 = saved_some;
            TEST("MIR Some RETURN cannot borrow another RETURN's None issuer");
            EXPECT(return_evidence_refused && return_full_refused && return_emission_refused &&
                mir_validate_collection_effect_evidence(option_mir, choose_routine, NULL));

            ASTNode *saved_foreign_rhs = *foreign_rhs_slot;
            *foreign_rhs_slot = carrier_use;
            bool alias_evidence_refused = !mir_validate_collection_effect_evidence(
                option_mir, main_routine, NULL);
            bool alias_full_refused = !mir_validate(option_mir, NULL);
            bool alias_emission_refused = !mir_validate_emission_contract(main_routine,
                main_routine->has_cleanup_block, false, NULL);
            *foreign_rhs_slot = saved_foreign_rhs;
            TEST("MIR Option carrier cannot become a foreign alias assignment RHS");
            EXPECT(alias_evidence_refused && alias_full_refused && alias_emission_refused &&
                mir_validate_collection_effect_evidence(option_mir, main_routine, NULL));

            MIRBasicBlock saved_block = *carrier_block;
            size_t count = saved_block.instruction_count;
            MIRInstruction *duplicated = calloc(count + 1, sizeof(*duplicated));
            TEST("MIR Option carrier duplicate birth has isolated storage");
            EXPECT(duplicated != NULL);
            if (duplicated != NULL) {
                memcpy(duplicated, saved_block.instructions,
                    (carrier_index + 1) * sizeof(*duplicated));
                duplicated[carrier_index + 1] = *carrier_inst;
                memcpy(&duplicated[carrier_index + 2],
                    &saved_block.instructions[carrier_index + 1],
                    (count - carrier_index - 1) * sizeof(*duplicated));
                carrier_block->instructions = duplicated;
                carrier_block->instruction_count = count + 1;
                carrier_block->instruction_capacity = count + 1;
                bool duplicate_evidence_refused = !mir_validate_collection_effect_evidence(
                    option_mir, main_routine, NULL);
                bool duplicate_full_refused = !mir_validate(option_mir, NULL);
                bool duplicate_emission_refused = !mir_validate_emission_contract(main_routine,
                    main_routine->has_cleanup_block, false, NULL);
                *carrier_block = saved_block;
                free(duplicated);
                TEST("MIR Option static carrier birth cannot occur twice");
                EXPECT(duplicate_evidence_refused && duplicate_full_refused &&
                    duplicate_emission_refused &&
                    mir_validate_collection_effect_evidence(option_mir, main_routine, NULL) &&
                    mir_validate(option_mir, NULL) &&
                    mir_validate_emission_contract(main_routine,
                        main_routine->has_cleanup_block, false, NULL));
            }
        }
        mir_destroy(option_mir); rir_destroy(option_rir); hir_destroy(option_hir);
    }
    {
        HIRProgram *literal_hir = NULL;
        RIRProgram *literal_rir = NULL;
        MIRProgram *literal_mir = NULL;
        const char *source =
            "func LiteralCodes()->Array<String>{let codes:Array<String> = [\"alpha\",\"beta\"];return codes;}"
            "func FindCode(message:String)->Option<String>{let codes:Array<String> = LiteralCodes();"
            "var i:Int=0;while i<ArrayLength(codes){if StringContains(message,codes[i]){"
            "return Some(codes[i]);}i=i+1;}return None;}"
            "func Main()->Void{let risk:Array<String> = [Concat(\"he\",\"ap\")];"
            "let found:Option<String> = FindCode(\"alpha\");let rows:Array<String> = [];"
            "if IsSome(found){let text:String=UnwrapOption(found);ArrayPush(rows,text);"
            "Log(rows[0]);}Log(risk[0]);}";
        bool lowered = lower_mir_from_source(source, &literal_hir, &literal_rir, &literal_mir);
        MIRRoutine *producer_routine = lowered ? find_mir_routine_mut(literal_mir,
            "LiteralCodes", MIR_SCOPE_FUNCTION) : NULL;
        MIRRoutine *finder_routine = lowered ? find_mir_routine_mut(literal_mir,
            "FindCode", MIR_SCOPE_FUNCTION) : NULL;
        MIRRoutine *main_routine = lowered ? find_mir_routine_mut(literal_mir,
            "Main", MIR_SCOPE_FUNCTION) : NULL;
        const ASTNode *program = literal_mir == NULL ? NULL :
            literal_mir->collection_effect_source_program;
        ASTNode *producer_ast = program == NULL ? NULL : ast_program_statement(program, 0);
        ASTNode *producer_body = producer_ast == NULL ? NULL : ast_func_body(producer_ast);
        ASTNode *producer_birth = producer_body == NULL ? NULL : ast_block_statement(producer_body, 0);
        ASTNode *producer_return = producer_body == NULL ? NULL : ast_block_statement(producer_body, 1);
        ASTNode *finder_ast = program == NULL ? NULL : ast_program_statement(program, 1);
        ASTNode *finder_body = finder_ast == NULL ? NULL : ast_func_body(finder_ast);
        ASTNode *carrier = finder_body == NULL ? NULL : ast_block_statement(finder_body, 0);
        ASTNode *loop = finder_body == NULL ? NULL : ast_block_statement(finder_body, 2);
        ASTNode *branch = loop == NULL ? NULL : ast_block_statement(ast_while_body(loop), 0);
        ASTNode *condition = branch == NULL ? NULL : ast_if_condition(branch);
        ASTNode *other_string = condition == NULL ? NULL : ast_call_argument(condition, 0);
        ASTNode *some_statement = branch == NULL ? NULL :
            ast_block_statement(ast_if_then_branch(branch), 0);
        ASTNode *some = some_statement == NULL ? NULL : ast_return_value(some_statement);
        ASTNode *element = some == NULL ? NULL : ast_call_argument(some, 0);
        ASTNode *main_ast = program == NULL ? NULL : ast_program_statement(program, 2);
        ASTNode *main_body = main_ast == NULL ? NULL : ast_func_body(main_ast);
        ASTNode *risk = main_body == NULL ? NULL : ast_block_statement(main_body, 0);
        ASTNode *main_branch = main_body == NULL ? NULL : ast_block_statement(main_body, 3);
        ASTNode *inside = main_branch == NULL ? NULL : ast_if_then_branch(main_branch);
        ASTNode *push = inside == NULL ? NULL : ast_block_statement(inside, 1);
        MIRInstruction *producer_birth_inst = NULL, *producer_return_inst = NULL;
        MIRInstruction *carrier_inst = NULL, *some_return_inst = NULL, *push_inst = NULL;
        MIRBasicBlock *carrier_block = NULL;
        size_t carrier_index = 0;
        for (size_t b = 0; producer_routine != NULL && b < producer_routine->block_count; b++)
            for (size_t i = 0; i < producer_routine->blocks[b].instruction_count; i++) {
                MIRInstruction *inst = &producer_routine->blocks[b].instructions[i];
                if (inst->kind == MIR_INST_DEF && inst->ast == producer_birth) producer_birth_inst = inst;
                if (inst->kind == MIR_INST_RETURN && inst->expr0 == ast_return_value(producer_return))
                    producer_return_inst = inst;
            }
        for (size_t b = 0; finder_routine != NULL && b < finder_routine->block_count; b++)
            for (size_t i = 0; i < finder_routine->blocks[b].instruction_count; i++) {
                MIRInstruction *inst = &finder_routine->blocks[b].instructions[i];
                if (inst->kind == MIR_INST_DEF && inst->ast == carrier) {
                    carrier_inst = inst;
                    carrier_block = &finder_routine->blocks[b];
                    carrier_index = i;
                }
                if (inst->kind == MIR_INST_RETURN && inst->expr0 == some) some_return_inst = inst;
            }
        for (size_t b = 0; main_routine != NULL && b < main_routine->block_count; b++)
            for (size_t i = 0; i < main_routine->blocks[b].instruction_count; i++)
                if (main_routine->blocks[b].instructions[i].kind == MIR_INST_STMT &&
                    main_routine->blocks[b].instructions[i].expr0 == push)
                    push_inst = &main_routine->blocks[b].instructions[i];
        PgyCollectionEffectView view = {0};
        bool control = lowered && producer_routine != NULL && finder_routine != NULL &&
            main_routine != NULL && producer_birth != NULL && producer_return != NULL &&
            carrier != NULL && some != NULL && element != NULL && other_string != NULL &&
            risk != NULL && push != NULL && producer_birth_inst != NULL &&
            producer_return_inst != NULL && carrier_inst != NULL && carrier_block != NULL &&
            some_return_inst != NULL && push_inst != NULL &&
            semantic_collection_effect_snapshot_current(literal_mir->collection_effect_snapshot,
                program, &view) && view.permanent_element_count != 0 &&
            mir_validate_collection_effect_evidence(literal_mir, NULL, NULL) &&
            mir_validate(literal_mir, NULL) &&
            mir_validate_emission_contract(producer_routine,
                producer_routine->has_cleanup_block, false, NULL) &&
            mir_validate_emission_contract(finder_routine,
                finder_routine->has_cleanup_block, false, NULL);
        TEST("MIR permanent literal Array element has an admitted Some source receipt");
        EXPECT(control);
        if (control) {
            const PgyCollectionEffectSnapshot *snapshot = literal_mir->collection_effect_snapshot;
            uint32_t caller_id = finder_routine->source_syntax_id;
            bool exact_use = semantic_collection_effect_snapshot_source_use(snapshot,
                caller_id, some, 0, element);
            bool reused_use = semantic_collection_effect_snapshot_source_use(snapshot,
                caller_id, some, 0, element);
            bool foreign_use = !semantic_collection_effect_snapshot_source_use(snapshot,
                caller_id, some, 0, other_string);
            bool exact_return = semantic_collection_effect_snapshot_source_return(snapshot,
                producer_routine->source_syntax_id, producer_return,
                ast_return_value(producer_return));
            bool crosswired_return = !semantic_collection_effect_snapshot_source_return(snapshot,
                producer_routine->source_syntax_id, producer_return, ast_let_initializer(risk));
            TEST("MIR literal element proof is reusable only at the exact Some slot and producer RETURN");
            EXPECT(exact_use && reused_use && foreign_use && exact_return && crosswired_return);

            ASTNode *saved_carrier = carrier_inst->expr0;
            carrier_inst->expr0 = ast_let_initializer(risk);
            bool carrier_evidence = !mir_validate_collection_effect_evidence(literal_mir,
                finder_routine, NULL);
            bool carrier_full = !mir_validate(literal_mir, NULL);
            bool carrier_emit = !mir_validate_emission_contract(finder_routine,
                finder_routine->has_cleanup_block, false, NULL);
            carrier_inst->expr0 = saved_carrier;
            TEST("MIR permanent literal carrier DEF rejects a foreign Array result");
            EXPECT(carrier_evidence && carrier_full && carrier_emit &&
                mir_validate_collection_effect_evidence(literal_mir, finder_routine, NULL));

            ASTNode *saved_birth = producer_birth_inst->expr0;
            producer_birth_inst->expr0 = ast_let_initializer(risk);
            bool birth_evidence = !mir_validate_collection_effect_evidence(literal_mir,
                producer_routine, NULL);
            bool birth_full = !mir_validate(literal_mir, NULL);
            bool birth_emit = !mir_validate_emission_contract(producer_routine,
                producer_routine->has_cleanup_block, false, NULL);
            producer_birth_inst->expr0 = saved_birth;
            TEST("MIR literal producer DEF cannot adopt a mixed String Array");
            EXPECT(birth_evidence && birth_full && birth_emit &&
                mir_validate_collection_effect_evidence(literal_mir, producer_routine, NULL));

            ASTNode *saved_producer_return = producer_return_inst->expr0;
            producer_return_inst->expr0 = ast_let_initializer(risk);
            bool return_evidence = !mir_validate_collection_effect_evidence(literal_mir,
                producer_routine, NULL);
            bool return_full = !mir_validate(literal_mir, NULL);
            bool return_emit = !mir_validate_emission_contract(producer_routine,
                producer_routine->has_cleanup_block, false, NULL);
            producer_return_inst->expr0 = saved_producer_return;
            TEST("MIR literal producer RETURN cannot change its selected Array source");
            EXPECT(return_evidence && return_full && return_emit &&
                mir_validate_collection_effect_evidence(literal_mir, producer_routine, NULL));

            ASTNode changed_some = *some;
            ASTNode *changed_arguments[1] = {other_string};
            changed_some.data.call.arguments = changed_arguments;
            ASTNode *saved_some = some_return_inst->expr0;
            some_return_inst->expr0 = &changed_some;
            bool some_evidence = !mir_validate_collection_effect_evidence(literal_mir,
                finder_routine, NULL);
            bool some_full = !mir_validate(literal_mir, NULL);
            bool some_emit = !mir_validate_emission_contract(finder_routine,
                finder_routine->has_cleanup_block, false, NULL);
            some_return_inst->expr0 = saved_some;
            TEST("MIR Some RETURN cannot substitute another payload actual");
            EXPECT(some_evidence && some_full && some_emit &&
                mir_validate_collection_effect_evidence(literal_mir, finder_routine, NULL));

            MIRBasicBlock saved_block = *carrier_block;
            size_t count = saved_block.instruction_count;
            MIRInstruction *duplicated = calloc(count + 1, sizeof(*duplicated));
            TEST("MIR literal Array carrier duplicate has isolated instruction storage");
            EXPECT(duplicated != NULL);
            if (duplicated != NULL) {
                memcpy(duplicated, saved_block.instructions,
                    (carrier_index + 1) * sizeof(*duplicated));
                duplicated[carrier_index + 1] = *carrier_inst;
                memcpy(&duplicated[carrier_index + 2],
                    &saved_block.instructions[carrier_index + 1],
                    (count - carrier_index - 1) * sizeof(*duplicated));
                carrier_block->instructions = duplicated;
                carrier_block->instruction_count = count + 1;
                carrier_block->instruction_capacity = count + 1;
                bool duplicate_evidence = !mir_validate_collection_effect_evidence(literal_mir,
                    finder_routine, NULL);
                bool duplicate_full = !mir_validate(literal_mir, NULL);
                bool duplicate_emit = !mir_validate_emission_contract(finder_routine,
                    finder_routine->has_cleanup_block, false, NULL);
                *carrier_block = saved_block;
                free(duplicated);
                TEST("MIR permanent literal Array carrier cannot be born twice");
                EXPECT(duplicate_evidence && duplicate_full && duplicate_emit &&
                    mir_validate_collection_effect_evidence(literal_mir, finder_routine, NULL) &&
                    mir_validate(literal_mir, NULL));
            }
        }
        mir_destroy(literal_mir); rir_destroy(literal_rir); hir_destroy(literal_hir);
    }
    {
        HIRProgram *alias_hir = NULL;
        RIRProgram *alias_rir = NULL;
        MIRProgram *alias_mir = NULL;
        const char *source =
            "func AliasOptional(value:String,found:Bool)->Option<String>{"
            "if !found{return None;}"
            "let payload:String=Substring(value,0,StringLength(value));"
            "return Some(payload);}"
            "func Main()->Void{let rows:Array<String> = [];"
            "let candidate:Option<String> = AliasOptional(\"ready\",true);"
            "if IsSome(candidate){let text:String=UnwrapOption(candidate);"
            "let liveRisk:Array<String> = [Concat(\"he\",\"ap\")];"
            "ArrayPush(rows,text);Log(rows[0]);Log(liveRisk[0]);}}";
        bool lowered = lower_mir_from_source(source, &alias_hir, &alias_rir, &alias_mir);
        MIRRoutine *producer = lowered ? find_mir_routine_mut(alias_mir,
            "AliasOptional", MIR_SCOPE_FUNCTION) : NULL;
        const ASTNode *program = alias_mir == NULL ? NULL :
            alias_mir->collection_effect_source_program;
        ASTNode *producer_ast = program == NULL ? NULL : ast_program_statement(program, 0);
        ASTNode *producer_body = producer_ast == NULL ? NULL : ast_func_body(producer_ast);
        ASTNode *payload = producer_body == NULL ? NULL : ast_block_statement(producer_body, 1);
        ASTNode *initializer = payload == NULL ? NULL : ast_let_initializer(payload);
        ASTNode *borrowed = initializer == NULL ? NULL : ast_call_argument(initializer, 0);
        ASTNode *return_statement = producer_body == NULL ? NULL :
            ast_block_statement(producer_body, 2);
        ASTNode *some = return_statement == NULL ? NULL : ast_return_value(return_statement);
        MIRInstruction *birth = NULL, *returned = NULL;
        MIRBasicBlock *birth_block = NULL;
        size_t birth_index = 0;
        for (size_t b = 0; producer != NULL && b < producer->block_count; b++)
            for (size_t i = 0; i < producer->blocks[b].instruction_count; i++) {
                MIRInstruction *inst = &producer->blocks[b].instructions[i];
                if (inst->kind == MIR_INST_DEF && inst->ast == payload) {
                    birth = inst;
                    birth_block = &producer->blocks[b];
                    birth_index = i;
                }
                if (inst->kind == MIR_INST_RETURN && inst->expr0 == some) returned = inst;
            }
        PgyCollectionEffectView view = {0};
        bool control = lowered && producer != NULL && payload != NULL &&
            initializer != NULL && borrowed != NULL && some != NULL &&
            birth != NULL && birth_block != NULL && birth->expr0 == initializer &&
            returned != NULL &&
            semantic_collection_effect_snapshot_current(alias_mir->collection_effect_snapshot,
                program, &view) && view.string_definition_count != 0 &&
            mir_validate_collection_effect_evidence(alias_mir, producer, NULL) &&
            mir_validate(alias_mir, NULL) &&
            mir_validate_emission_contract(producer, producer->has_cleanup_block, false, NULL);
        TEST("MIR Option local Substring payload alias has one admitted source birth");
        EXPECT(control);
        if (control) {
            const PgyCollectionEffectSnapshot *snapshot = alias_mir->collection_effect_snapshot;
            size_t ordinal = SIZE_MAX, repeated = SIZE_MAX, wrong = SIZE_MAX;
            uint32_t binding = ast_node_stable_id(payload);
            bool birth_current = semantic_collection_effect_snapshot_source_definition(
                snapshot, producer->source_syntax_id, binding, true,
                payload, payload, initializer, &ordinal);
            bool birth_reused = semantic_collection_effect_snapshot_source_definition(
                snapshot, producer->source_syntax_id, binding, true,
                payload, payload, initializer, &repeated);
            bool borrowed_refused = !semantic_collection_effect_snapshot_source_definition(
                snapshot, producer->source_syntax_id, binding, true,
                payload, payload, borrowed, &wrong);
            TEST("MIR Option payload alias birth is exact and its source query is reusable");
            EXPECT(birth_current && birth_reused && ordinal < view.string_write_count &&
                ordinal == repeated && borrowed_refused && wrong == SIZE_MAX);

            ASTNode *saved_rhs = birth->expr0;
            birth->expr0 = borrowed;
            bool rhs_evidence_refused = !mir_validate_collection_effect_evidence(
                alias_mir, producer, NULL);
            bool rhs_full_refused = !mir_validate(alias_mir, NULL);
            bool rhs_emission_refused = !mir_validate_emission_contract(producer,
                producer->has_cleanup_block, false, NULL);
            birth->expr0 = saved_rhs;
            TEST("MIR Option alias DEF RHS cannot become a borrowed parameter");
            EXPECT(rhs_evidence_refused && rhs_full_refused && rhs_emission_refused &&
                mir_validate_collection_effect_evidence(alias_mir, producer, NULL));

            ASTNode changed_some = *some;
            ASTNode *changed_args[1] = {borrowed};
            changed_some.data.call.arguments = changed_args;
            ASTNode *saved_return = returned->expr0;
            returned->expr0 = &changed_some;
            bool return_evidence_refused = !mir_validate_collection_effect_evidence(
                alias_mir, producer, NULL);
            bool return_full_refused = !mir_validate(alias_mir, NULL);
            bool return_emission_refused = !mir_validate_emission_contract(producer,
                producer->has_cleanup_block, false, NULL);
            returned->expr0 = saved_return;
            TEST("MIR Option Some RETURN cannot substitute a borrowed payload");
            EXPECT(return_evidence_refused && return_full_refused &&
                return_emission_refused &&
                mir_validate_collection_effect_evidence(alias_mir, producer, NULL));

            MIRBasicBlock saved_block = *birth_block;
            size_t count = saved_block.instruction_count;
            MIRInstruction *duplicated = calloc(count + 1, sizeof(*duplicated));
            TEST("MIR Option payload alias duplicate birth has isolated storage");
            EXPECT(duplicated != NULL);
            if (duplicated != NULL) {
                memcpy(duplicated, saved_block.instructions,
                    (birth_index + 1) * sizeof(*duplicated));
                duplicated[birth_index + 1] = *birth;
                memcpy(&duplicated[birth_index + 2],
                    &saved_block.instructions[birth_index + 1],
                    (count - birth_index - 1) * sizeof(*duplicated));
                birth_block->instructions = duplicated;
                birth_block->instruction_count = count + 1;
                birth_block->instruction_capacity = count + 1;
                bool duplicate_evidence_refused = !mir_validate_collection_effect_evidence(
                    alias_mir, producer, NULL);
                bool duplicate_full_refused = !mir_validate(alias_mir, NULL);
                bool duplicate_emission_refused = !mir_validate_emission_contract(producer,
                    producer->has_cleanup_block, false, NULL);
                *birth_block = saved_block;
                free(duplicated);
                TEST("MIR Option payload alias cannot issue two static births");
                EXPECT(duplicate_evidence_refused && duplicate_full_refused &&
                    duplicate_emission_refused &&
                    mir_validate_collection_effect_evidence(alias_mir, producer, NULL) &&
                    mir_validate(alias_mir, NULL) &&
                    mir_validate_emission_contract(producer,
                        producer->has_cleanup_block, false, NULL));
            }
        }
        mir_destroy(alias_mir); rir_destroy(alias_rir); hir_destroy(alias_hir);
    }
    HIRProgram *borrow_hir = NULL;
    RIRProgram *borrow_rir = NULL;
    MIRProgram *borrow_mir = NULL;
    MIRRoutine *borrow_routine = NULL;
    MIRInstruction *borrow_push = NULL;
    MIRCollectionOwnershipFact *borrow_fact = NULL;
    if (lower_mir_from_source("struct TextNode{text:String;}struct Arena{rows:Array<String>;risk:Array<String>;}"
            "func Project(nodes:Array<TextNode>)->Arena{let risk:Array<String> = [];"
            "let rows:Array<String> = [\"a\"];ArrayPush(rows,nodes[0].text);return Arena(rows,risk);}"
            "func Main()->Void{let nodes:Array<TextNode> = [TextNode(\"name\")];"
            "let arena=Project(nodes);Log(arena.rows[1]);}", &borrow_hir, &borrow_rir, &borrow_mir))
        borrow_routine = find_mir_routine_mut(borrow_mir, "Project", MIR_SCOPE_FUNCTION);
    if (borrow_routine != NULL) {
        for (size_t f = 0; f < borrow_routine->collection_ownership_fact_count; f++)
            if (borrow_routine->collection_ownership_facts[f].origin == PGY_COLLECTION_ORIGIN_BORROWED_LITERAL)
                borrow_fact = &borrow_routine->collection_ownership_facts[f];
        for (size_t b = 0; b < borrow_routine->block_count; b++)
            for (size_t i = 0; i < borrow_routine->blocks[b].instruction_count; i++) {
                MIRInstruction *inst = &borrow_routine->blocks[b].instructions[i];
                if (inst->has_collection_ownership_receipt) borrow_push = inst;
            }
    }
    bool borrow_control = borrow_push != NULL && borrow_fact != NULL
        && mir_validate(borrow_mir, NULL)
        && mir_validate_collection_ownership_transitions(borrow_routine, NULL);
    TEST("MIR borrowed literal shallow effect is not owned cleanup authority");
    if (!borrow_control) printf("\n  borrowed fixture routine=%d fact=%d receipt=%d\n",
        borrow_routine != NULL, borrow_fact != NULL, borrow_push != NULL);
    EXPECT(borrow_control);
    if (borrow_control) {
        MIRInstruction saved_inst = *borrow_push;
        MIRCollectionOwnershipFact saved_fact = *borrow_fact;
        ASTNode saved_call = *borrow_push->expr0;
        for (unsigned fault = 0; fault < 13; fault++) {
            if (fault == 0) borrow_push->collection_ownership_effect_kind = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
            if (fault == 1) borrow_push->collection_ownership_effect_kind = PGY_COLLECTION_EFFECT_DROP;
            if (fault == 2) borrow_push->collection_ownership_source_binding_id++;
            if (fault == 3) borrow_push->expr0->data.call.semantic_collection_effect_kind = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
            if (fault == 4) borrow_push->expr0->data.call.semantic_collection_receiver_binding_id++;
            if (fault == 5) borrow_push->expr0->data.call.semantic_collection_source_binding_id++;
            if (fault == 6) borrow_push->expr0->data.call.semantic_collection_effect_set = false;
            if (fault == 7) borrow_fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
            if (fault == 8) borrow_fact->element_ownership = PGY_STRING_ARRAY_OWNED_ELEMENTS;
            if (fault == 9) borrow_fact->function_syntax_id++;
            if (fault == 10) borrow_push->has_collection_ownership_receipt = false;
            if (fault == 11) borrow_push->collection_ownership_receiver_binding_id++;
            if (fault == 12) borrow_fact->origin_syntax_id = 0;
            bool refused = !mir_validate_collection_ownership_transitions(borrow_routine, NULL);
            *saved_inst.expr0 = saved_call;
            *borrow_push = saved_inst; *borrow_fact = saved_fact;
            TEST("MIR borrowed effect refuses tuple drift or owned/drop forgery");
            if (!refused) printf("\n  borrowed effect fault=%u not refused\n", fault);
            EXPECT(refused && mir_validate_collection_ownership_transitions(borrow_routine, NULL));
        }
    }
    mir_destroy(borrow_mir); rir_destroy(borrow_rir); hir_destroy(borrow_hir);
    {
        HIRProgram *fresh_hir = NULL;
        RIRProgram *fresh_rir = NULL;
        MIRProgram *fresh_mir = NULL;
        MIRRoutine *fresh_routine = NULL;
        MIRInstruction *push = NULL;
        MIRBasicBlock *push_block = NULL;
        size_t push_index = 0;
        const char *fresh_source =
            "struct TextNode{text:String;}struct Arena{rows:Array<String>;}"
            "func Fresh(node:TextNode)->String{return Concat(\"fresh\",\" value\");}"
            "func Project(nodes:Array<TextNode>)->Arena{let rows:Array<String> = [];"
            "ArrayPush(rows,Fresh(nodes[0]));return Arena(rows);}"
            "func Main()->Void{let nodes:Array<TextNode> = [TextNode(\"name\")];"
            "let arena=Project(nodes);Log(arena.rows[0]);}";
        if (lower_mir_from_source(fresh_source, &fresh_hir, &fresh_rir, &fresh_mir))
            fresh_routine = find_mir_routine_mut(fresh_mir, "Project", MIR_SCOPE_FUNCTION);
        for (size_t b = 0; fresh_routine != NULL && b < fresh_routine->block_count; b++)
            for (size_t i = 0; i < fresh_routine->blocks[b].instruction_count; i++) {
                MIRInstruction *inst = &fresh_routine->blocks[b].instructions[i];
                if (inst->has_collection_ownership_receipt &&
                    inst->collection_ownership_effect_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION) {
                    push = inst;
                    push_block = &fresh_routine->blocks[b];
                    push_index = i;
                }
            }
        uint32_t fresh_id = push == NULL ? 0 :
            ast_call_semantic_callee_decl_id(ast_call_argument(push->expr0, 1));
        bool control = push != NULL && fresh_id != 0 && mir_validate(fresh_mir, NULL);
        TEST("MIR independent String push retains its semantic SHALLOW effect");
        EXPECT(control);
        if (control) {
            MIRInstruction saved = *push;
            ASTNode saved_call = *push->expr0;
            for (unsigned fault = 0; fault < 7; fault++) {
                if (fault == 0) {
                    push->collection_ownership_effect_kind = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
                    push->collection_ownership_source_binding_id = fresh_id;
                }
                if (fault == 1) push->expr0->data.call.semantic_collection_effect_kind = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
                if (fault == 2) push->expr0->data.call.semantic_collection_receiver_binding_id++;
                if (fault == 3) push->expr0->data.call.semantic_collection_source_binding_id++;
                if (fault == 4) push->expr0->data.call.semantic_collection_effect_set = false;
                if (fault == 5) push->has_collection_ownership_receipt = false;
                if (fault == 6) push->collection_ownership_receiver_binding_id++;
                bool refused = !mir_validate_collection_ownership_transitions(fresh_routine, NULL);
                *saved.expr0 = saved_call; *push = saved;
                TEST("MIR independent push cannot promote SHALLOW into owned cleanup");
                if (!refused) printf("\n  empty independent effect fault=%u not refused\n", fault);
                EXPECT(refused && mir_validate_collection_ownership_transitions(fresh_routine, NULL));
            }
            bool emission_control = mir_validate_emission_contract(
                fresh_routine, fresh_routine->has_cleanup_block, false, NULL);
            TEST("MIR private String effect reaches the routine emission contract");
            EXPECT(emission_control);
            if (emission_control) {
                for (unsigned fault = 0; fault < 5; fault++) {
                    if (fault == 0) {
                        push->has_collection_ownership_receipt = false;
                        push->expr0->data.call.semantic_collection_effect_set = false;
                    }
                    if (fault == 1) {
                        push->collection_ownership_effect_kind =
                            PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
                        push->collection_ownership_source_binding_id = fresh_id;
                        push->expr0->data.call.semantic_collection_effect_kind =
                            PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
                        push->expr0->data.call.semantic_collection_source_binding_id =
                            fresh_id;
                    }
                    if (fault == 2) {
                        push->collection_ownership_receiver_binding_id++;
                        push->expr0->data.call.semantic_collection_receiver_binding_id++;
                    }
                    if (fault == 3) {
                        push->collection_ownership_source_binding_id++;
                        push->expr0->data.call.semantic_collection_source_binding_id++;
                    }
                    if (fault == 4) {
                        push->collection_ownership_effect_kind = PGY_COLLECTION_EFFECT_DROP;
                        push->expr0->data.call.semantic_collection_effect_kind =
                            PGY_COLLECTION_EFFECT_DROP;
                    }
                    char *validation_error = NULL;
                    bool refused = !mir_validate(fresh_mir, &validation_error)
                        && validation_error != NULL;
                    free(validation_error);
                    validation_error = NULL;
                    bool emission_refused = !mir_validate_emission_contract(
                        fresh_routine, fresh_routine->has_cleanup_block, false,
                        &validation_error) && validation_error != NULL;
                    free(validation_error);
                    *saved.expr0 = saved_call;
                    *push = saved;
                    TEST("MIR private String effect rejects coherent receipt and AST forgery");
                    if (!refused || !emission_refused)
                        printf("\n  coherent private effect fault=%u full=%d emission=%d\n",
                            fault, refused, emission_refused);
                    EXPECT(refused && emission_refused && mir_validate(fresh_mir, NULL)
                        && mir_validate_emission_contract(fresh_routine,
                            fresh_routine->has_cleanup_block, false, NULL));
                }
                MIRProgram saved_program = *fresh_mir;
                MIRRoutine *saved_routines = malloc(
                    fresh_mir->routine_count * sizeof(*saved_routines));
                TEST("MIR private effect erasure has isolated routine snapshots");
                EXPECT(saved_routines != NULL);
                if (saved_routines != NULL) {
                    memcpy(saved_routines, fresh_mir->routines,
                        fresh_mir->routine_count * sizeof(*saved_routines));
                    MIRInstruction saved_push = *push;
                    ASTNode saved_push_call = *push->expr0;
                    fresh_mir->collection_effect_snapshot = NULL;
                    fresh_mir->collection_effect_source_program = NULL;
                    fresh_mir->has_collection_ownership_facts = false;
                    for (size_t r = 0; r < fresh_mir->routine_count; r++) {
                        MIRRoutine *routine = &fresh_mir->routines[r];
                        routine->hir_routine = NULL;
                        routine->collection_ownership_facts = NULL;
                        routine->collection_ownership_fact_count = 0;
                        routine->collection_ownership_fact_capacity = 0;
                    }
                    push->has_collection_ownership_receipt = false;
                    push->collection_ownership_effect_kind = 0;
                    push->collection_ownership_receiver_binding_id = 0;
                    push->collection_ownership_source_binding_id = 0;
                    push->expr0->data.call.semantic_collection_effect_set = false;
                    push->expr0->data.call.semantic_collection_effect_kind = 0;
                    push->expr0->data.call.semantic_collection_receiver_binding_id = 0;
                    push->expr0->data.call.semantic_collection_source_binding_id = 0;
                    char *validation_error = NULL;
                    bool evidence_refused = !mir_validate_collection_effect_evidence(
                        fresh_mir, NULL, &validation_error) && validation_error != NULL;
                    free(validation_error);
                    validation_error = NULL;
                    bool full_refused = !mir_validate(fresh_mir,
                        &validation_error) && validation_error != NULL;
                    free(validation_error);
                    validation_error = NULL;
                    bool emission_refused = !mir_validate_emission_contract(
                        fresh_routine, fresh_routine->has_cleanup_block, false,
                        &validation_error) && validation_error != NULL;
                    free(validation_error);
                    *saved_push.expr0 = saved_push_call;
                    *push = saved_push;
                    memcpy(fresh_mir->routines, saved_routines,
                        fresh_mir->routine_count * sizeof(*saved_routines));
                    *fresh_mir = saved_program;
                    free(saved_routines);
                    bool restored = mir_validate_collection_effect_evidence(
                        fresh_mir, NULL, NULL) && mir_validate(fresh_mir, NULL)
                        && mir_validate_emission_contract(fresh_routine,
                            fresh_routine->has_cleanup_block, false, NULL);
                    TEST("MIR private effect rejects coherent whole-family erasure");
                    if (!evidence_refused || !full_refused || !emission_refused ||
                        !restored)
                        printf("\n  whole-family erasure evidence=%d full=%d emission=%d restored=%d\n",
                            evidence_refused, full_refused, emission_refused,
                            restored);
                    EXPECT(evidence_refused && full_refused && emission_refused &&
                        restored);
                }
                if (push_block != NULL) {
                    MIRBasicBlock saved_block = *push_block;
                    size_t count = saved_block.instruction_count;
                    MIRInstruction *changed = malloc((count + 1) * sizeof(*changed));
                    TEST("MIR private effect operation mutations have isolated storage");
                    EXPECT(changed != NULL);
                    if (changed != NULL) {
                        uint32_t saved_source_id = fresh_routine->source_syntax_id;
                        for (unsigned fault = 0; fault < 3; fault++) {
                            memcpy(changed, saved_block.instructions,
                                count * sizeof(*changed));
                            push_block->instructions = changed;
                            push_block->instruction_capacity = count + 1;
                            if (fault == 0 || fault == 2) {
                                memmove(&changed[push_index],
                                    &changed[push_index + 1],
                                    (count - push_index - 1) * sizeof(*changed));
                                push_block->instruction_count = count - 1;
                                if (fault == 2)
                                    fresh_routine->source_syntax_id = UINT32_MAX;
                            } else {
                                memmove(&changed[push_index + 2],
                                    &changed[push_index + 1],
                                    (count - push_index - 1) * sizeof(*changed));
                                changed[push_index + 1] = changed[push_index];
                                push_block->instruction_count = count + 1;
                            }
                            char *validation_error = NULL;
                            bool refused = !mir_validate(fresh_mir,
                                &validation_error) && validation_error != NULL;
                            free(validation_error);
                            validation_error = NULL;
                            bool emission_refused =
                                !mir_validate_emission_contract(fresh_routine,
                                    fresh_routine->has_cleanup_block, false,
                                    &validation_error) && validation_error != NULL;
                            free(validation_error);
                            *push_block = saved_block;
                            fresh_routine->source_syntax_id = saved_source_id;
                            TEST("MIR private effect needs one instruction in its exact caller");
                            if (!refused || !emission_refused)
                                printf("\n  private effect operation fault=%u full=%d emission=%d\n",
                                    fault, refused, emission_refused);
                            EXPECT(refused && emission_refused &&
                                mir_validate(fresh_mir, NULL) &&
                                mir_validate_emission_contract(fresh_routine,
                                    fresh_routine->has_cleanup_block, false,
                                    NULL));
                        }
                        free(changed);
                    }
                }
                Lexer *foreign_lexer = lexer_create(
                    "func Foreign() -> Void { return; }");
                Parser *foreign_parser = parser_create(foreign_lexer);
                ASTNode *foreign_program = parser_parse_program(foreign_parser);
                TEST("MIR private effect foreign program fault has an AST owner");
                EXPECT(foreign_program != NULL &&
                    !parser_has_error(foreign_parser));
                if (foreign_program != NULL &&
                    !parser_has_error(foreign_parser)) {
                    PgyCollectionEffectSnapshot *saved_snapshot =
                        fresh_mir->collection_effect_snapshot;
                    const ASTNode *saved_program =
                        fresh_mir->collection_effect_source_program;
                    MIRRoutine saved_routine = *fresh_routine;
                    for (unsigned fault = 0; fault < 7; fault++) {
                        if (fault == 0)
                            fresh_mir->collection_effect_snapshot = NULL;
                        if (fault == 1)
                            fresh_mir->collection_effect_source_program = NULL;
                        if (fault == 2)
                            fresh_mir->collection_effect_source_program =
                                foreign_program;
                        if (fault == 3) fresh_routine->source_syntax_id++;
                        if (fault == 4) fresh_routine->hir_routine = NULL;
                        if (fault == 5) fresh_routine->program = NULL;
                        if (fault == 6) fresh_routine->id = UINT32_MAX;
                        char *validation_error = NULL;
                        bool refused = !mir_validate(fresh_mir,
                            &validation_error) && validation_error != NULL;
                        free(validation_error);
                        validation_error = NULL;
                        bool emission_refused =
                            !mir_validate_emission_contract(fresh_routine,
                                fresh_routine->has_cleanup_block, false,
                                &validation_error) && validation_error != NULL;
                        free(validation_error);
                        fresh_mir->collection_effect_snapshot = saved_snapshot;
                        fresh_mir->collection_effect_source_program =
                            saved_program;
                        *fresh_routine = saved_routine;
                        TEST("MIR private effect requires its retained owner and exact routine");
                        if (!refused || !emission_refused)
                            printf("\n  private effect owner fault=%u full=%d emission=%d\n",
                                fault, refused, emission_refused);
                        EXPECT(refused && emission_refused &&
                            mir_validate(fresh_mir, NULL) &&
                            mir_validate_emission_contract(fresh_routine,
                                fresh_routine->has_cleanup_block, false,
                                NULL));
                    }
                }
                ast_destroy(foreign_program);
                parser_destroy(foreign_parser);
                lexer_destroy(foreign_lexer);
                char **view_param_names = calloc(
                    fresh_routine->param_count > 0 ? fresh_routine->param_count : 1,
                    sizeof(*view_param_names));
                TEST("MIR emission view has isolated ABI pointer storage");
                EXPECT(view_param_names != NULL);
                if (view_param_names != NULL) {
                    char view_name[] = "ProjectEmissionView";
                    for (size_t p = 0; p < fresh_routine->param_count; p++)
                        view_param_names[p] = fresh_routine->param_type_names[p];
                    MIRRoutine view = *fresh_routine;
                    view.name = view_name;
                    view.param_type_names = view_param_names;
                    bool view_control = mir_validate_emission_contract(
                        &view, view.has_cleanup_block, false, NULL);
                    TEST("MIR emission accepts a canonical-backed ABI/name view");
                    EXPECT(view_control);
                    if (view_control) {
                        MIRSourceLocalType detached_local = {0};
                        for (unsigned fault = 0; fault < 8; fault++) {
                            MIRRoutine damaged = view;
                            MIRCollectionOwnershipFact *copied_facts = NULL;
                            if (fault == 0) damaged.source_syntax_id = UINT32_MAX;
                            if (fault == 1) damaged.program = NULL;
                            if (fault == 2) damaged.hir_routine = NULL;
                            if (fault == 3) damaged.ast = NULL;
                            if (fault == 4) {
                                copied_facts = malloc(damaged.collection_ownership_fact_count *
                                    sizeof(*copied_facts));
                                if (copied_facts != NULL) {
                                    memcpy(copied_facts, damaged.collection_ownership_facts,
                                        damaged.collection_ownership_fact_count * sizeof(*copied_facts));
                                    damaged.collection_ownership_facts = copied_facts;
                                }
                            }
                            if (fault == 5) damaged.source_local_types =
                                damaged.source_local_types == NULL ? &detached_local : NULL;
                            if (fault == 6) damaged.source_local_type_count++;
                            if (fault == 7) damaged.source_local_type_capacity++;
                            char *validation_error = NULL;
                            bool refused = (fault != 4 || copied_facts != NULL) &&
                                !mir_validate_emission_contract(&damaged,
                                    damaged.has_cleanup_block, false,
                                    &validation_error) && validation_error != NULL;
                            free(validation_error);
                            free(copied_facts);
                            TEST("MIR emission view cannot replace canonical routine evidence");
                            if (!refused)
                                printf("\n  private effect view identity fault=%u not refused\n", fault);
                            EXPECT(refused && mir_validate(fresh_mir, NULL) &&
                                mir_validate_emission_contract(&view,
                                    view.has_cleanup_block, false, NULL));
                        }
                        if (push_block != NULL) {
                            MIRBasicBlock *detached_blocks = malloc(
                                fresh_routine->block_count * sizeof(*detached_blocks));
                            size_t count = push_block->instruction_count;
                            MIRInstruction *detached_instructions = malloc(
                                (count + 1) * sizeof(*detached_instructions));
                            TEST("MIR emission view mutation has detached block storage");
                            EXPECT(detached_blocks != NULL && detached_instructions != NULL);
                            if (detached_blocks != NULL && detached_instructions != NULL) {
                                size_t block_index = (size_t)(push_block - fresh_routine->blocks);
                                for (unsigned fault = 0; fault < 4; fault++) {
                                    memcpy(detached_blocks, fresh_routine->blocks,
                                        fresh_routine->block_count * sizeof(*detached_blocks));
                                    memcpy(detached_instructions, push_block->instructions,
                                        count * sizeof(*detached_instructions));
                                    detached_blocks[block_index].instructions = detached_instructions;
                                    detached_blocks[block_index].instruction_capacity = count + 1;
                                    ASTNode call_copy = *push->expr0;
                                    if (fault == 1)
                                        detached_instructions[push_index].has_collection_ownership_receipt = false;
                                    if (fault == 2) {
                                        memmove(&detached_instructions[push_index + 2],
                                            &detached_instructions[push_index + 1],
                                            (count - push_index - 1) * sizeof(*detached_instructions));
                                        detached_instructions[push_index + 1] =
                                            detached_instructions[push_index];
                                        detached_blocks[block_index].instruction_count = count + 1;
                                    }
                                    if (fault == 3) {
                                        detached_instructions[push_index].collection_ownership_effect_kind =
                                            PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
                                        detached_instructions[push_index].collection_ownership_source_binding_id =
                                            fresh_id;
                                        call_copy.data.call.semantic_collection_effect_kind =
                                            PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
                                        call_copy.data.call.semantic_collection_source_binding_id = fresh_id;
                                        detached_instructions[push_index].expr0 = &call_copy;
                                        detached_instructions[push_index].ast = &call_copy;
                                    }
                                    MIRRoutine damaged = view;
                                    damaged.blocks = detached_blocks;
                                    char *validation_error = NULL;
                                    bool refused = !mir_validate_emission_contract(&damaged,
                                        damaged.has_cleanup_block, false,
                                        &validation_error) && validation_error != NULL;
                                    free(validation_error);
                                    TEST("MIR emission view rejects detached or forged blocks");
                                    if (!refused)
                                        printf("\n  private effect detached view fault=%u not refused\n", fault);
                                    EXPECT(refused && mir_validate(fresh_mir, NULL) &&
                                        mir_validate_emission_contract(&view,
                                            view.has_cleanup_block, false, NULL));
                                }
                            }
                            free(detached_instructions);
                            free(detached_blocks);
                        }
                    }
                    free(view_param_names);
                }
            }
        }
        mir_destroy(fresh_mir); rir_destroy(fresh_rir); hir_destroy(fresh_hir);
    }
    {
        const char *transport_source =
            "struct TextNode{text:String;}struct Arena{rows:Array<String>;risk:Array<String>;}"
            "func Fresh(node:TextNode)->String{return Concat(\"fresh\",\" value\");}"
            "func Project(nodes:Array<TextNode>)->Arena{let risk:Array<String> = [];"
            "let rows:Array<String> = [\"a\"];ArrayPush(rows,Fresh(nodes[0]));return Arena(rows,risk);}"
            "func Main()->Void{let nodes:Array<TextNode> = [TextNode(\"name\")];"
            "let arena=Project(nodes);Log(arena.rows[1]);}";
        Lexer *lexer = lexer_create(transport_source);
        Parser *parser = parser_create(lexer);
        ASTNode *ast = parser_parse_program(parser);
        SemanticResult *sem = parser_has_error(parser) ? NULL : semantic_analyze(ast);
        ASTNode *project = ast == NULL ? NULL : ast_program_statement(ast, 3);
        ASTNode *body = project == NULL ? NULL : ast_func_body(project);
        ASTNode *call = body == NULL ? NULL : ast_block_statement(body, 2);
        char *error = NULL;
        HIRSemanticProjectionFailure failure = HIR_SEMANTIC_PROJECTION_NONE;
        HIRProgram *accepted = sem == NULL || !sem->success ? NULL :
            hir_lower_with_semantic_facts(sem, &failure, &error);
        DIRProgram *accepted_dir = accepted == NULL ? NULL :
            dir_lower_with_hir_facts(ast, accepted, &error);
        RIRProgram *accepted_rir = accepted == NULL ? NULL : rir_lower(ast, &error);
        if (accepted_rir != NULL)
            (void)rir_enrich_with_hir_flow(accepted_rir, accepted, &error);
        MIRLowerRequest request;
        mir_lower_request_init(&request, accepted, accepted_rir, sem);
        mir_lower_request_bind_dir(&request, accepted_dir);
        MIRProgram *accepted_mir = accepted == NULL || accepted_rir == NULL ||
            accepted_dir == NULL ? NULL : mir_lower(&request, &error);
        bool control = accepted != NULL && call != NULL && call->type == AST_CALL &&
            accepted_mir != NULL && hir_collection_effect_snapshot_current(sem, accepted, NULL);
        TEST("HIR retains admitted private effects after semantic context teardown");
        if (!control) printf("\n  semantic=%d HIR=%d call=%d failure=%d error=%s\n",
            sem != NULL && sem->success, accepted != NULL, call != NULL,
            (int)failure, error != NULL ? error : "none");
        EXPECT(control);
        free(error); error = NULL;
        if (control) {
            ASTNode saved_call = *call;
            PgyCollectionEffectSnapshot *saved_snapshot = sem->collection_effect_snapshot;
            for (unsigned fault = 0; fault < 5; fault++) {
                if (fault == 0) call->data.call.semantic_collection_effect_set = false;
                if (fault == 1) call->data.call.semantic_collection_effect_kind = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
                if (fault == 2) call->data.call.semantic_collection_receiver_binding_id++;
                if (fault == 3) call->data.call.semantic_collection_source_binding_id++;
                if (fault == 4) sem->collection_effect_snapshot = NULL;
                HIRProgram *rejected = hir_lower_with_semantic_facts(sem, &failure, &error);
                bool refused = rejected == NULL &&
                    failure == HIR_SEMANTIC_PROJECTION_COLLECTION_OWNERSHIP && error != NULL;
                bool request_refused = !hir_collection_effect_snapshot_current(sem, accepted, NULL);
                hir_destroy(rejected); free(error); error = NULL;
                MIRProgram *rejected_mir = mir_lower(&request, &error);
                request_refused = request_refused && rejected_mir == NULL && error != NULL;
                mir_destroy(rejected_mir); free(error); error = NULL;
                *call = saved_call; sem->collection_effect_snapshot = saved_snapshot;
                TEST("HIR refuses deleted or rewritten borrowed independent effects before MIR");
                if (!refused || !request_refused) printf("\n  transport fault=%u HIR=%d request=%d\n",
                    fault, refused, request_refused);
                EXPECT(refused && request_refused && hir_collection_effect_snapshot_current(sem, accepted, NULL));
            }
            HIRRoutine *actual = NULL;
            for (size_t r = 0; r < accepted->routine_count; r++)
                if (accepted->routines[r].ast == project) actual = &accepted->routines[r];
            bool member = actual != NULL;
            TEST("HIR expected effect caller is present in the lowered routine owner");
            EXPECT(member);
            if (member) {
                HIRRoutine saved_routine = *actual;
                for (unsigned fault = 0; fault < 6; fault++) {
                    if (fault == 0) actual->body = NULL;
                    if (fault == 1) actual->source_syntax_id++;
                    if (fault == 2) actual->ast = body;
                    if (fault == 3) actual->has_cfg = false;
                    if (fault == 4) actual->cfg.blocks = NULL;
                    if (fault == 5) actual->cfg.block_capacity = 0;
                    bool refused = !hir_collection_effect_snapshot_current(sem, accepted, NULL);
                    MIRProgram *rejected_mir = mir_lower(&request, &error);
                    refused = refused && rejected_mir == NULL && error != NULL;
                    mir_destroy(rejected_mir); free(error); error = NULL;
                    *actual = saved_routine;
                    TEST("MIR request cannot use a detached expected caller body");
                    EXPECT(refused && hir_collection_effect_snapshot_current(sem, accepted, NULL));
                }
                HIRBasicBlock *effect_block = NULL;
                size_t effect_statement = 0, occurrences = 0;
                for (size_t b = 0; b < actual->cfg.block_count; b++)
                    for (size_t s = 0; s < actual->cfg.blocks[b].statement_count; s++)
                        if (actual->cfg.blocks[b].statements[s] == call) {
                            effect_block = &actual->cfg.blocks[b];
                            effect_statement = s; occurrences++;
                        }
                TEST("HIR private effect has one actual CFG statement witness");
                EXPECT(occurrences == 1 && effect_block != NULL);
                if (occurrences == 1 && effect_block != NULL) {
                    HIRBasicBlock saved_block = *effect_block;
                    size_t count = effect_block->statement_count;
                    ASTNode **changed = malloc((count + 1) * sizeof(*changed));
                    TEST("HIR effect omission mutation has isolated statement storage");
                    EXPECT(changed != NULL);
                    if (changed != NULL) {
                        for (unsigned fault = 0; fault < 3; fault++) {
                            memcpy(changed, saved_block.statements, count * sizeof(*changed));
                            effect_block->statements = changed;
                            if (fault == 0) {
                                memmove(&changed[effect_statement], &changed[effect_statement + 1],
                                    (count - effect_statement - 1) * sizeof(*changed));
                                effect_block->statement_count = count - 1;
                            }
                            if (fault == 1) changed[effect_statement] = ast_block_statement(body, 0);
                            if (fault == 2) {
                                changed[count] = call;
                                effect_block->statement_count = count + 1;
                                effect_block->statement_capacity = count + 1;
                            }
                            bool refused = !hir_collection_effect_snapshot_current(sem, accepted, NULL);
                            MIRProgram *rejected_mir = mir_lower(&request, &error);
                            refused = refused && rejected_mir == NULL && error != NULL;
                            mir_destroy(rejected_mir); free(error); error = NULL;
                            *effect_block = saved_block;
                            TEST("MIR request refuses erased, replaced or duplicated CFG effects");
                            if (!refused) printf("\n  actual CFG effect fault=%u not refused\n", fault);
                            EXPECT(refused && hir_collection_effect_snapshot_current(sem, accepted, NULL));
                        }
                        free(changed);
                    }
                }
            }
        }
        mir_destroy(accepted_mir); rir_destroy(accepted_rir); dir_destroy(accepted_dir);
        hir_destroy(accepted);
        TEST("semantic effect ledger survives MIR-first destruction");
        EXPECT(sem != NULL && sem->success &&
            semantic_collection_effect_snapshot_current(
                sem->collection_effect_snapshot, sem->annotated_ast, NULL));
        semantic_result_destroy(sem);
        ast_destroy(ast); parser_destroy(parser); lexer_destroy(lexer);
    }
    {
        const char *source =
            "struct TextNode{text:String;}struct Arena{rows:Array<String>;}"
            "func Fresh(node:TextNode)->String{return Concat(\"fresh\",\" value\");}"
            "func Project(nodes:Array<TextNode>)->Arena{let rows:Array<String> = [];"
            "ArrayPush(rows,Fresh(nodes[0]));return Arena(rows);}"
            "func Main()->Void{let nodes:Array<TextNode> = [TextNode(\"name\")];"
            "let arena=Project(nodes);Log(arena.rows[0]);}";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *ast = parser_parse_program(parser);
        SemanticResult *sem = parser_has_error(parser) ? NULL :
            semantic_analyze(ast);
        char *error = NULL;
        HIRProgram *hir = sem == NULL || !sem->success ? NULL :
            hir_lower_with_semantic_facts(sem, NULL, &error);
        DIRProgram *dir = hir == NULL ? NULL :
            dir_lower_with_hir_facts(ast, hir, &error);
        RIRProgram *rir = hir == NULL ? NULL : rir_lower(ast, &error);
        if (hir != NULL && rir != NULL)
            (void)rir_enrich_with_hir_flow(rir, hir, &error);
        MIRProgram *mir = NULL;
        if (hir != NULL && dir != NULL && rir != NULL &&
            dir_validate(dir, &error)) {
            MIRLowerRequest request;
            mir_lower_request_init(&request, hir, rir, sem);
            mir_lower_request_bind_dir(&request, dir);
            mir = mir_lower(&request, &error);
        }
        bool control = sem != NULL && sem->success && mir != NULL &&
            mir->collection_effect_snapshot == sem->collection_effect_snapshot &&
            mir_validate(mir, NULL);
        TEST("MIR owns a reference to the semantic effect ledger");
        if (!control) printf("\n  semantic-first lifetime error=%s\n",
            error != NULL ? error : "none");
        EXPECT(control);
        free(error);
        /* The ledger is independently retained, but existing IR Type borrows
         * still require their original registry until all IRs are destroyed. */
        TypeRegistry *borrowed_types = sem == NULL ? NULL : sem->owned_types;
        if (sem != NULL) sem->owned_types = NULL;
        semantic_result_destroy(sem);
        sem = NULL;
        TEST("MIR private effect evidence survives semantic result destruction");
        EXPECT(control && mir_validate(mir, NULL));
        mir_destroy(mir); rir_destroy(rir); dir_destroy(dir);
        hir_destroy(hir); ast_destroy(ast);
        type_registry_destroy(borrowed_types);
        parser_destroy(parser); lexer_destroy(lexer);
    }
    const char *source =
        "func Main() -> Void {\n"
        "    let values: HashMap<String, Int> = MapNew();\n"
        "    MapSet(values, \"owned\", 1);\n"
        "    let keys: Array<String> = MapKeys(values);\n"
        "    ArrayDropOwnedStrings(keys);\n"
        "}\n";
    const char *empty_source =
        "func Main() -> Void {\n"
        "    let values: Array<String> = [];\n"
        "    Log(ArrayLength(values));\n"
        "}\n";
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    MIRRoutine *routine = NULL;
    MIRCollectionOwnershipFact *fact = NULL;
    MIRSourceLocalType *target = NULL;
    char *error = NULL;
    bool baseline = false;
    bool rejects_function = false;
    bool rejects_binding = false;
    bool rejects_origin_id = false;
    bool rejects_source = false;
    bool rejects_ownership = false;
    bool rejects_disposition = false;
    bool rejects_origin = false;
    bool rejects_unknown_owned = false;
    bool rejects_retired_unknown = false;
    bool rejects_target_type = false;
    bool rejects_storage_mismatch = false;
    bool empty_baseline = false;
    bool rejects_empty_owned = false;

    if (lower_mir_from_source(source, &hir, &rir, &mir))
        routine = find_mir_routine_mut(mir, "Main", MIR_SCOPE_FUNCTION);
    if (routine != NULL && routine->collection_ownership_fact_count == 1) {
        fact = &routine->collection_ownership_facts[0];
        for (size_t i = 0; i < routine->source_local_type_count; i++) {
            if (routine->source_local_types[i].binding_syntax_id
                == fact->binding_syntax_id) {
                target = &routine->source_local_types[i];
                break;
            }
        }
    }
    if (fact != NULL && target != NULL) {
        uint32_t u32;
        PgyStringArrayOwnership ownership;
        PgyCollectionDisposition disposition;
        PgyCollectionOrigin origin;
        const char *type_name;
        size_t count;

        baseline = mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL;

        u32 = fact->function_syntax_id;
        fact->function_syntax_id = u32 + 1;
        rejects_function = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->function_syntax_id = u32;

        u32 = fact->binding_syntax_id;
        fact->binding_syntax_id = 0;
        rejects_binding = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->binding_syntax_id = u32;

        u32 = fact->origin_syntax_id;
        fact->origin_syntax_id = 0;
        rejects_origin_id = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->origin_syntax_id = u32;

        u32 = fact->source_binding_syntax_id;
        fact->source_binding_syntax_id = fact->binding_syntax_id;
        rejects_source = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->source_binding_syntax_id = u32;

        ownership = fact->element_ownership;
        fact->element_ownership = PGY_STRING_ARRAY_BORROWED_ELEMENTS;
        rejects_ownership = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->element_ownership = ownership;

        disposition = fact->disposition;
        fact->disposition = (PgyCollectionDisposition)99;
        rejects_disposition = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->disposition = disposition;

        origin = fact->origin;
        fact->origin = (PgyCollectionOrigin)99;
        rejects_origin = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; fact->origin = origin;

        origin = fact->origin;
        ownership = fact->element_ownership;
        disposition = fact->disposition;
        fact->origin = PGY_COLLECTION_ORIGIN_UNKNOWN;
        fact->element_ownership = PGY_STRING_ARRAY_OWNED_ELEMENTS;
        fact->disposition = PGY_COLLECTION_DISPOSITION_LIVE;
        rejects_unknown_owned =
            !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL;
        fact->element_ownership = PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN;
        fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
        rejects_retired_unknown =
            !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL;
        fact->origin = origin;
        fact->element_ownership = ownership;
        fact->disposition = disposition;

        type_name = target->type_name;
        target->type_name = "Array<Int>";
        rejects_target_type = !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL; target->type_name = (char *)type_name;

        count = routine->collection_ownership_fact_count;
        routine->collection_ownership_fact_count = 0;
        rejects_storage_mismatch =
            !mir_validate_collection_ownership_facts(routine, &error);
        free(error); error = NULL;
        routine->collection_ownership_fact_count = count;
    }

    {
        HIRProgram *empty_hir = NULL;
        RIRProgram *empty_rir = NULL;
        MIRProgram *empty_mir = NULL;
        MIRRoutine *empty_routine = NULL;
        MIRCollectionOwnershipFact *empty_fact = NULL;

        if (lower_mir_from_source(
                empty_source, &empty_hir, &empty_rir, &empty_mir))
            empty_routine = find_mir_routine_mut(
                empty_mir, "Main", MIR_SCOPE_FUNCTION);
        if (empty_routine != NULL
            && empty_routine->collection_ownership_fact_count == 1) {
            empty_fact = &empty_routine->collection_ownership_facts[0];
            empty_baseline =
                empty_fact->element_ownership
                    == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
                && empty_fact->origin == PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
                && empty_fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
                && empty_fact->source_binding_syntax_id == 0
                && mir_validate_collection_ownership_facts(
                    empty_routine, &error);
            free(error); error = NULL;
            if (empty_baseline) {
                empty_fact->element_ownership =
                    PGY_STRING_ARRAY_OWNED_ELEMENTS;
                rejects_empty_owned =
                    !mir_validate_collection_ownership_facts(
                        empty_routine, &error);
                free(error); error = NULL;
            }
        }
        mir_destroy(empty_mir);
        rir_destroy(empty_rir);
        hir_destroy(empty_hir);
    }

    TEST("MIR collection ownership carrier rejects in-memory mutations");
    EXPECT(baseline && rejects_function && rejects_binding &&
           rejects_origin_id && rejects_source && rejects_ownership &&
           rejects_disposition && rejects_origin &&
           rejects_unknown_owned && rejects_retired_unknown &&
           rejects_target_type && empty_baseline && rejects_empty_owned &&
           rejects_storage_mismatch);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);

    /* The same lexical binding is born at DEF on every retired loop entry;
     * routine entry and a join alone must never invent a live allocation. */
    for (unsigned loop_local = 0; loop_local < 2; loop_local++) {
        const char *birth_source = loop_local
            ? "func Main()->Void{var i:Int=0;while i<2{let names:Array<String> = [];"
              "ArrayPushOwnedString(names,\"entry\");ArrayDropOwnedStrings(names);i=i+1;}}"
            : "func Main()->Void{let names:Array<String> = [];"
              "ArrayPushOwnedString(names,\"entry\");ArrayDropOwnedStrings(names);}";
        HIRProgram *birth_hir = NULL;
        RIRProgram *birth_rir = NULL;
        MIRProgram *birth_mir = NULL;
        MIRRoutine *birth = NULL;
        if (lower_mir_from_source(birth_source, &birth_hir, &birth_rir, &birth_mir))
            birth = find_mir_routine_mut(birth_mir, "Main", MIR_SCOPE_FUNCTION);
        MIRCollectionOwnershipFact *birth_fact = NULL;
        MIRSourceLocalType *birth_type = NULL;
        MIRBasicBlock *body = NULL;
        size_t definition = SIZE_MAX, push = SIZE_MAX, drop = SIZE_MAX;
        if (birth != NULL && birth->collection_ownership_fact_count == 1) {
            birth_fact = &birth->collection_ownership_facts[0];
            for (size_t i = 0; i < birth->source_local_type_count; i++)
                if (birth->source_local_types[i].binding_syntax_id == birth_fact->binding_syntax_id)
                    birth_type = &birth->source_local_types[i];
            for (size_t b = 0; b < birth->block_count; b++) {
                MIRBasicBlock *candidate = &birth->blocks[b];
                for (size_t i = 0; i < candidate->instruction_count; i++) {
                    MIRInstruction *inst = &candidate->instructions[i];
                    if (inst->kind == MIR_INST_DEF && inst->binding_syntax_id == birth_fact->binding_syntax_id) {
                        body = candidate; definition = i;
                    }
                }
            }
            for (size_t i = 0; body != NULL && i < body->instruction_count; i++) {
                MIRInstruction *inst = &body->instructions[i];
                if (!inst->has_collection_ownership_receipt) continue;
                if (inst->collection_ownership_effect_kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH) push = i;
                if (inst->collection_ownership_effect_kind == PGY_COLLECTION_EFFECT_DROP) drop = i;
            }
        }
        bool birth_control = birth_fact != NULL && birth_type != NULL && body != NULL
            && definition < push && push < drop && drop < body->instruction_count
            && mir_validate(birth_mir, NULL)
            && mir_validate_collection_ownership_transitions(birth, NULL);
        TEST(loop_local ? "MIR exact loop-local DEF restarts only a retired generation"
                        : "MIR routine-local empty lifetime starts at its actual DEF");
        if (!birth_control)
            printf("\n  loop=%u birth fact=%d type=%d body=%d def=%zu push=%zu drop=%zu\n",
                loop_local, birth_fact != NULL, birth_type != NULL, body != NULL, definition, push, drop);
        EXPECT(birth_control);
        if (birth_control) {
            MIRInstruction *def = &body->instructions[definition];
            MIRInstruction saved_def = *def;
            MIRSourceLocalType saved_type = *birth_type;
            MIRCollectionOwnershipFact saved_fact = *birth_fact;
            ASTNode saved_literal = *def->expr0;
            ASTNode *elements[] = {ast_call_argument(body->instructions[push].expr0, 1)};
            for (unsigned fault = 0; fault < 11; fault++) {
                if (fault == 0) def->kind = MIR_INST_STMT;
                if (fault == 1) def->expr0 = NULL;
                if (fault == 2) def->source_stable_id++;
                if (fault == 3) def->binding_syntax_id++;
                if (fault == 4) birth_fact->origin_syntax_id++;
                if (fault == 5) birth_type->type_name = "Array<Int>";
                if (fault == 6) def->requires_source_local_decl_emit = false;
                if (fault == 7) {
                    def->expr0->data.array_literal.count = 1;
                    def->expr0->data.array_literal.elements = elements;
                }
                if (fault == 8) def->expr0->stable_id++;
                if (fault == 9) def->ast = NULL;
                if (fault == 10) def->expr1 = NULL;
                bool refused = !mir_validate_collection_ownership_transitions(birth, NULL);
                *saved_def.expr0 = saved_literal;
                *def = saved_def; *birth_type = saved_type; *birth_fact = saved_fact;
                TEST("MIR empty birth refuses missing or crosswired DEF evidence");
                if (!refused) printf("\n  loop=%u DEF fault=%u not refused\n", loop_local, fault);
                EXPECT(refused && mir_validate_collection_ownership_transitions(birth, NULL));
            }
            size_t count = body->instruction_count;
            MIRInstruction *original = body->instructions;
            MIRInstruction *changed = malloc((count + 1) * sizeof(*changed));
            TEST("MIR lifetime faults allocate a separate instruction view");
            EXPECT(changed != NULL);
            if (changed != NULL) {
                for (unsigned fault = 0; fault < 3; fault++) {
                    memcpy(changed, original, count * sizeof(*changed));
                    body->instructions = changed;
                    if (fault == 0) {
                        changed[definition] = original[push];
                        changed[push] = original[definition];
                    } else {
                        size_t after = fault == 1 ? drop : push;
                        memmove(changed + after + 2, changed + after + 1,
                            (count - after - 1) * sizeof(*changed));
                        changed[after + 1] = original[fault == 1 ? push : definition];
                        body->instruction_count = count + 1;
                    }
                    bool refused = !mir_validate_collection_ownership_transitions(birth, NULL);
                    body->instructions = original; body->instruction_count = count;
                    TEST("MIR rejects pre-birth push, post-drop push and live overwrite");
                    if (!refused) printf("\n  loop=%u ordering fault=%u not refused\n", loop_local, fault);
                    EXPECT(refused && mir_validate_collection_ownership_transitions(birth, NULL));
                }
                free(changed);
            }
        }
        mir_destroy(birth_mir); rir_destroy(birth_rir); hir_destroy(birth_hir);
    }

    /* One real typed binding move, with final destination lifetime preserved.
     * This does not grant an unsupported second-hop move or revive the source. */
    for (unsigned dropped = 0; dropped < 2; dropped++) {
        const char *move_source = dropped
            ? "func Main() -> Void { let values: HashMap<String,Int> = MapNew();"
              " MapSet(values,\"owned\",1); let keys: Array<String> = MapKeys(values);"
              " let alias: Array<String> = keys; ArrayDropOwnedStrings(alias); }"
            : "func Main() -> Void { let values: HashMap<String,Int> = MapNew();"
              " MapSet(values,\"owned\",1); let keys: Array<String> = MapKeys(values);"
              " let alias: Array<String> = keys; Log(ArrayLength(alias)); }";
        HIRProgram *move_hir = NULL;
        RIRProgram *move_rir = NULL;
        MIRProgram *move_mir = NULL;
        MIRRoutine *move = NULL;
        if (lower_mir_from_source(move_source, &move_hir, &move_rir, &move_mir))
            move = find_mir_routine_mut(move_mir, "Main", MIR_SCOPE_FUNCTION);
        MIRCollectionOwnershipFact *src = NULL, *dst = NULL;
        MIRSourceLocalType *dst_type = NULL;
        MIRInstruction *def = NULL;
        if (move != NULL && move->collection_ownership_fact_count == 2) {
            src = &move->collection_ownership_facts[0];
            dst = &move->collection_ownership_facts[1];
            for (size_t i = 0; i < move->source_local_type_count; i++)
                if (move->source_local_types[i].binding_syntax_id == dst->binding_syntax_id)
                    dst_type = &move->source_local_types[i];
            for (size_t b = 0; b < move->block_count; b++)
                for (size_t i = 0; i < move->blocks[b].instruction_count; i++)
                    if (move->blocks[b].instructions[i].binding_syntax_id == dst->binding_syntax_id)
                        def = &move->blocks[b].instructions[i];
        }
        bool control = src != NULL && dst != NULL && dst_type != NULL && def != NULL
            && src->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS
            && src->disposition == PGY_COLLECTION_DISPOSITION_RETIRED
            && dst->origin == PGY_COLLECTION_ORIGIN_BINDING
            && dst->source_binding_syntax_id == src->binding_syntax_id
            && dst->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
            && dst->disposition == (dropped ? PGY_COLLECTION_DISPOSITION_RETIRED
                                           : PGY_COLLECTION_DISPOSITION_LIVE)
            && mir_validate_collection_ownership_facts(move, NULL);
        TEST(dropped ? "MIR moved MapKeys destination remains retired after deep drop"
                     : "MIR moved MapKeys destination remains live before deep drop");
        EXPECT(control);
        if (control) {
            char *full_error = NULL;
            bool full_control = mir_validate(move_mir, &full_error);
            free(full_error); full_error = NULL;
            TEST("MIR full typed binding-move program is valid before corruption");
            EXPECT(full_control);
            MIRSourceLocalType *birth_type = NULL;
            for (size_t i = 0; i < move->source_local_type_count; i++)
                if (move->source_local_types[i].binding_syntax_id == src->binding_syntax_id)
                    birth_type = &move->source_local_types[i];
            TEST("MIR MapKeys birth retains actual initializer result evidence");
            EXPECT(birth_type != NULL && birth_type->initializer_call_syntax_id == src->origin_syntax_id
                && birth_type->initializer_collection_producer == 1
                && birth_type->initializer_call_value_type_name != NULL
                && strcmp(birth_type->initializer_call_value_type_name, "Array<String>") == 0);
            if (full_control && birth_type != NULL) {
                for (unsigned birth_fault = 0; birth_fault < 6; birth_fault++) {
                    MIRSourceLocalType saved_birth = *birth_type;
                    bool birth_control = mir_validate(move_mir, NULL);
                    if (birth_fault == 0) birth_type->initializer_call_syntax_id = 0;
                    if (birth_fault == 1) birth_type->initializer_call_syntax_id++;
                    if (birth_fault == 2) birth_type->initializer_collection_producer = 0;
                    if (birth_fault == 3) birth_type->initializer_collection_producer = 2;
                    if (birth_fault == 4) birth_type->initializer_call_value_type_name = NULL;
                    if (birth_fault == 5) birth_type->initializer_call_value_type_name = "Array<Int>";
                    bool birth_rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                    free(full_error); full_error = NULL;
                    *birth_type = saved_birth;
                    TEST("MIR full validation rejects missing or stale actual birth evidence");
                    if (!birth_rejected) printf("\n  dropped=%u birth fault=%u not refused\n", dropped, birth_fault);
                    EXPECT(birth_control && birth_rejected && mir_validate(move_mir, NULL));
                }
            }
            if (full_control) {
                PgyCollectionDisposition saved_status = dst->disposition;
                dst->disposition = dropped ? PGY_COLLECTION_DISPOSITION_LIVE
                                           : PGY_COLLECTION_DISPOSITION_RETIRED;
                bool rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                free(full_error); full_error = NULL;
                dst->disposition = saved_status;
                TEST("MIR full validation rejects final disposition flip");
                EXPECT(rejected && mir_validate(move_mir, NULL));
                bool checked_orphan = false;
                for (size_t b = 0; b < move->block_count && !checked_orphan; b++) {
                    for (size_t i = 0; i < move->blocks[b].instruction_count; i++) {
                        MIRInstruction *candidate = &move->blocks[b].instructions[i];
                        if (candidate->has_collection_ownership_receipt || candidate->kind != MIR_INST_DEF) continue;
                        bool orphan_control = mir_validate(move_mir, NULL);
                        MIRInstruction saved_candidate = *candidate;
                        candidate->has_collection_ownership_receipt = true;
                        candidate->collection_ownership_effect_kind = PGY_COLLECTION_EFFECT_DROP;
                        candidate->collection_ownership_receiver_binding_id = dst->binding_syntax_id;
                        candidate->collection_ownership_source_binding_id = 0;
                        bool orphan_rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                        free(full_error); full_error = NULL;
                        *candidate = saved_candidate;
                        TEST("MIR full validation rejects orphan DROP receipt on actual DEF");
                        EXPECT(orphan_control && orphan_rejected && mir_validate(move_mir, NULL));
                        checked_orphan = true; break;
                    }
                }
                TEST("MIR typed control contains an operation for orphan receipt fault");
                EXPECT(checked_orphan);
            }
            if (full_control && dropped) {
                size_t matched_drop = 0;
                for (size_t b = 0; b < move->block_count; b++) {
                    MIRBasicBlock *block = &move->blocks[b];
                    for (size_t i = 0; i < block->instruction_count; i++) {
                        ASTNode *call = block->instructions[i].expr0;
                        ASTNode *callee = call != NULL && call->type == AST_CALL ? ast_call_callee(call) : NULL;
                        if (callee == NULL || callee->type != AST_IDENTIFIER ||
                            strcmp(ast_identifier_name(callee), "ArrayDropOwnedStrings") != 0) continue;
                        matched_drop++;
                        size_t count = block->instruction_count;
                        MIRInstruction *original = block->instructions;
                        MIRInstruction *backup = malloc(count * sizeof(*backup));
                        MIRInstruction *duplicate = malloc((count + 1) * sizeof(*duplicate));
                        bool allocated = backup != NULL && duplicate != NULL;
                        TEST("MIR deep-drop mutation scratch storage exists");
                        EXPECT(allocated);
                        if (!allocated) { free(backup); free(duplicate); continue; }
                        memcpy(backup, original, count * sizeof(*backup));
                        for (unsigned receipt_fault = 0; receipt_fault < 4; receipt_fault++) {
                            bool receipt_control = mir_validate(move_mir, NULL);
                            MIRInstruction saved_drop = original[i];
                            if (receipt_fault == 0) original[i].has_collection_ownership_receipt = false;
                            if (receipt_fault == 1) original[i].collection_ownership_effect_kind = 99;
                            if (receipt_fault == 2) original[i].collection_ownership_receiver_binding_id = src->binding_syntax_id;
                            if (receipt_fault == 3) original[i].collection_ownership_source_binding_id = src->binding_syntax_id;
                            bool receipt_rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                            free(full_error); full_error = NULL;
                            original[i] = saved_drop;
                            TEST("MIR full validation rejects missing or malformed deep-drop receipt");
                            if (!receipt_rejected) printf("\n  drop receipt fault=%u not refused\n", receipt_fault);
                            EXPECT(receipt_control && receipt_rejected && mir_validate(move_mir, NULL));
                        }
                        bool before_delete = mir_validate(move_mir, NULL);
                        memmove(&original[i], &original[i + 1], (count - i - 1) * sizeof(*original));
                        block->instruction_count = count - 1;
                        bool deletion_rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                        free(full_error); full_error = NULL;
                        memcpy(original, backup, count * sizeof(*original));
                        block->instruction_count = count;
                        TEST("MIR full validation rejects deleted drop with retired final fact");
                        EXPECT(before_delete && deletion_rejected && mir_validate(move_mir, NULL));
                        bool before_duplicate = mir_validate(move_mir, NULL);
                        memcpy(duplicate, backup, (i + 1) * sizeof(*duplicate));
                        duplicate[i + 1] = backup[i];
                        memcpy(&duplicate[i + 2], &backup[i + 1], (count - i - 1) * sizeof(*duplicate));
                        block->instructions = duplicate; block->instruction_count = count + 1;
                        bool duplication_rejected = !mir_validate(move_mir, &full_error) && full_error != NULL;
                        free(full_error); full_error = NULL;
                        block->instructions = original; block->instruction_count = count;
                        TEST("MIR full validation rejects duplicated deep drop");
                        EXPECT(before_duplicate && duplication_rejected && mir_validate(move_mir, NULL));
                        free(duplicate); free(backup);
                    }
                }
                TEST("MIR typed binding-move has exactly one deep-drop operation");
                EXPECT(matched_drop == 1);
            }
            for (unsigned fault = 0; fault < 19; fault++) {
                MIRCollectionOwnershipFact saved_src = *src, saved_dst = *dst;
                MIRInstruction saved_def = *def;
                const char *saved_type = dst_type->type_name;
                size_t saved_count = move->collection_ownership_fact_count;
                switch (fault) {
                    case 0: dst->source_binding_syntax_id = 0; break;
                    case 1: dst->source_binding_syntax_id = UINT32_MAX; break;
                    case 2: dst->source_binding_syntax_id = dst->binding_syntax_id; break;
                    case 3: src->disposition = PGY_COLLECTION_DISPOSITION_LIVE; break;
                    case 4: dst->element_ownership = PGY_STRING_ARRAY_BORROWED_ELEMENTS; break;
                    case 5: dst->element_ownership = (PgyStringArrayOwnership)99; break;
                    case 6: dst->disposition = (PgyCollectionDisposition)99; break;
                    case 7: dst->function_syntax_id++; break;
                    case 8: dst->origin_syntax_id = 0; break;
                    case 9: dst->binding_syntax_id = 0; break;
                    case 10: dst->origin = (PgyCollectionOrigin)99; break;
                    case 11: *src = saved_dst; *dst = saved_src; break; /* source order */
                    case 12: *dst = saved_src; break; /* duplicate identity */
                    case 13: dst_type->type_name = "Array<Int>"; break;
                    case 14: def->kind = MIR_INST_STMT; break;
                    case 15: def->binding_syntax_id = 0; break;
                    case 16: move->collection_ownership_fact_count = 1; *src = saved_dst; break;
                    case 17: def->expr0 = NULL; break;
                    case 18: def->use_count = 0; break;
                }
                char *fault_error = NULL;
                bool refused = !mir_validate_collection_ownership_facts(move, &fault_error)
                    && fault_error != NULL;
                free(fault_error);
                *src = saved_src; *dst = saved_dst; *def = saved_def;
                dst_type->type_name = (char *)saved_type;
                move->collection_ownership_fact_count = saved_count;
                TEST("MIR binding-move malformed lineage refuses without lifetime rewrite");
                if (!refused) printf("\n  binding-move dropped=%u fault=%u not refused\n", dropped, fault);
                EXPECT(refused && mir_validate_collection_ownership_facts(move, NULL));
            }
        }
        mir_destroy(move_mir); rir_destroy(move_rir); hir_destroy(move_hir);
    }

    {
        const char *clone_source =
            "func Main() -> Void { let values: Array<String> = [\"borrowed\"];"
            " let cloned: Array<String> = Clone(values); ArrayDropOwnedStrings(cloned); }";
        HIRProgram *clone_hir = NULL;
        RIRProgram *clone_rir = NULL;
        MIRProgram *clone_mir = NULL;
        MIRRoutine *clone_routine = NULL;
        MIRSourceLocalType *clone_type = NULL;
        MIRCollectionOwnershipFact *clone_fact = NULL;
        bool clone_lowered = lower_mir_from_source(clone_source, &clone_hir, &clone_rir, &clone_mir);
        if (clone_lowered) clone_routine = find_mir_routine_mut(clone_mir, "Main", MIR_SCOPE_FUNCTION);
        if (clone_routine != NULL) {
            for (size_t i = 0; i < clone_routine->collection_ownership_fact_count; i++)
                if (clone_routine->collection_ownership_facts[i].origin == PGY_COLLECTION_ORIGIN_CLONE)
                    clone_fact = &clone_routine->collection_ownership_facts[i];
            for (size_t i = 0; clone_fact != NULL && i < clone_routine->source_local_type_count; i++)
                if (clone_routine->source_local_types[i].binding_syntax_id == clone_fact->binding_syntax_id)
                    clone_type = &clone_routine->source_local_types[i];
        }
        bool clone_control = clone_type != NULL && clone_fact != NULL
            && clone_type->initializer_call_syntax_id > 0
            && clone_type->initializer_call_syntax_id == clone_fact->origin_syntax_id
            && clone_type->initializer_collection_producer == 2
            && clone_type->initializer_call_value_type_name != NULL
            && strcmp(clone_type->initializer_call_value_type_name, "Array<String>") == 0
            && mir_validate(clone_mir, NULL);
        TEST("MIR full Clone deep-drop retains canonical actual result witness");
        EXPECT(clone_control);
        if (clone_control) for (unsigned fault = 0; fault < 3; fault++) {
            MIRSourceLocalType saved = *clone_type;
            bool before = mir_validate(clone_mir, NULL);
            if (fault == 0) clone_type->initializer_call_syntax_id = 0;
            if (fault == 1) clone_type->initializer_collection_producer = 0;
            if (fault == 2) clone_type->initializer_call_value_type_name = NULL;
            char *clone_error = NULL;
            bool rejected = !mir_validate(clone_mir, &clone_error) && clone_error != NULL;
            free(clone_error);
            *clone_type = saved;
            TEST("MIR full Clone rejects missing retained initializer witness");
            EXPECT(before && rejected && mir_validate(clone_mir, NULL));
        }
        mir_destroy(clone_mir); rir_destroy(clone_rir); hir_destroy(clone_hir);

        /* The missing canonical AST seal is rejected at HIR before MIR can
         * capture an owned birth. Do not mutate a retired AST flag and pretend
         * it is the native private MIR carrier's authority. */
        Lexer *clone_lexer = lexer_create(clone_source);
        Parser *clone_parser = parser_create(clone_lexer);
        ASTNode *clone_ast = parser_parse_program(clone_parser);
        SemanticResult *clone_sem = semantic_analyze(clone_ast);
        ASTNode *clone_call = NULL;
        if (!parser_has_error(clone_parser) && clone_sem != NULL && clone_sem->success) {
            ASTNode *main_decl = ast_program_statement(clone_sem->annotated_ast, 0);
            clone_call = ast_let_initializer(ast_block_statement(ast_func_body(main_decl), 1));
        }
        HIRSemanticProjectionFailure clone_failure = HIR_SEMANTIC_PROJECTION_NONE;
        char *clone_error = NULL;
        HIRProgram *canonical_hir = clone_call != NULL
            ? hir_lower_with_semantic_facts(clone_sem, &clone_failure, &clone_error) : NULL;
        bool canonical_control = canonical_hir != NULL && clone_error == NULL
            && clone_failure == HIR_SEMANTIC_PROJECTION_NONE
            && ast_call_semantic_callee_is_stdlib(clone_call);
        hir_destroy(canonical_hir); free(clone_error); clone_error = NULL;
        bool canonical_refused = false;
        if (canonical_control) {
            bool saved_flag = clone_call->data.call.semantic_callee_is_stdlib;
            clone_call->data.call.semantic_callee_is_stdlib = false;
            canonical_hir = hir_lower_with_semantic_facts(clone_sem, &clone_failure, &clone_error);
            canonical_refused = canonical_hir == NULL && clone_error != NULL
                && clone_failure == HIR_SEMANTIC_PROJECTION_COLLECTION_OWNERSHIP;
            clone_call->data.call.semantic_callee_is_stdlib = saved_flag;
            hir_destroy(canonical_hir); free(clone_error);
        }
        TEST("HIR to MIR boundary refuses missing canonical Clone seal");
        EXPECT(canonical_control && canonical_refused);
        semantic_result_destroy(clone_sem); ast_destroy(clone_ast);
        parser_destroy(clone_parser); lexer_destroy(clone_lexer);
    }

    hir = NULL;
    rir = NULL;
    mir = NULL;
    bool formal_control = lower_mir_from_source(
        "struct Holder { rows: Array<String>; }"
        "func Inspect(own values: Holder, ref other: Holder) -> Void {"
        " let alias: Array<String> = values.rows; Log(ArrayLength(alias)); }"
        " func Main() -> Void {}", &hir, &rir, &mir);
    routine = formal_control
        ? find_mir_routine_mut(mir, "Inspect", MIR_SCOPE_FUNCTION) : NULL;
    bool duplicate_formal = false;
    if (routine != NULL && routine->param_count == 2) {
        formal_control &= routine->collection_ownership_fact_count == 1
            && mir_validate_collection_ownership_facts(routine, NULL);
        FuncParam *second = routine->params[1];
        routine->params[1] = routine->params[0];
        duplicate_formal = !mir_validate_collection_ownership_facts(routine,
            &error) && error != NULL;
        routine->params[1] = second;
        free(error);
    }
    TEST("MIR collection source type refuses duplicate formal identity");
    if (!formal_control || !duplicate_formal)
        printf("\n  formal identity fixture: control=%d rows=%zu params=%zu duplicate_refused=%d\n",
            (int)formal_control,
            routine != NULL ? routine->collection_ownership_fact_count : 0,
            routine != NULL ? routine->param_count : 0, (int)duplicate_formal);
    EXPECT(formal_control && duplicate_formal);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
}
