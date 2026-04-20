    if (node->data.func_decl.body != NULL) {
        for (size_t i = 0; i < param_count; i++) {
            FuncParam *param = node->data.func_decl.params[i];
            unsigned summary_mask;

            if (param == NULL || param->name == NULL || param->type == NULL)
                continue;
            if (param->mode != PARAM_MODE_REF)
                continue;
            if (semantic_classify_ownership_type(param_types[i], ctx)
                == OWNERSHIP_TYPE_COPY_ONLY)
                continue;

            summary_mask = slot_analyze_param_summary_in_program(
                node->data.func_decl.body, param->name, ctx->program_root);
            if ((summary_mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) {
                semantic_validate_borrowed_escape(
                    node, node, ctx, param_types[i], param->name,
                    OWNERSHIP_CONSUMER_RETURN, NULL, NULL, NULL,
                    false, NULL, NULL);
            }
            if ((summary_mask & SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE) != 0) {
                semantic_validate_borrowed_escape(
                    node, node, ctx, param_types[i], param->name,
                    OWNERSHIP_CONSUMER_CHANNEL_SEND, NULL, NULL, NULL,
                    false, NULL, NULL);
            }
            if ((summary_mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) {
                semantic_validate_borrowed_escape(
                    node, node, ctx, param_types[i], param->name,
                    OWNERSHIP_CONSUMER_HELPER_CALL, NULL,
                    node->data.func_decl.name != NULL
                        ? node->data.func_decl.name : "<anonymous>",
                    NULL, true, NULL,
                    semantic_classify_ownership_type(param_types[i], ctx)
                        == OWNERSHIP_TYPE_MOVE_ONLY ? "movable-resource"
                        : (semantic_classify_ownership_type(param_types[i], ctx)
                           == OWNERSHIP_TYPE_SUBJECT_IDENTITY ? "subject"
                           : "value"));
            }
        }
    }
