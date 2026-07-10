# HIR Substitution Track

This layer owns the typed AST/HIR facts shared by parser, semantic analysis,
and code generation. `typed_ast_arena_owner.pgy` is the single owner of the
flat `AstArena` row contract. Codegen may project transitional AST text into
that arena, but it may not own or redeclare the arena shape.

`ast_text_scan_owner.pgy`, `ast_text_row_fact_owner.pgy`, and
`ast_text_inventory_owner.pgy` own the transitional compact-text input facts.
`ast_text_arena_projection_owner.pgy` constructs one `AstTreeArtifact` carrying
the text provenance, arena, and node count. Temporary `CodegenAstTextNode` rows
are consumed during construction and do not cross the artifact boundary.
Parser produces that artifact; codegen consumes it without rebuilding the
arena.

HIR lowering or validation must consume parser-owned facts and compare against
the C compiler before it counts toward substitution. Re-reading source text to
recover an already parsed semantic fact is not a valid self-host bridge.
