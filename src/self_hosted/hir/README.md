# HIR Substitution Track

This layer owns the typed AST/HIR facts shared by parser, semantic analysis,
and code generation. `typed_ast_arena_owner.pgy` is the single owner of the
flat `AstArena` row contract. Codegen may project transitional AST text into
that arena, but it may not own or redeclare the arena shape.

HIR lowering or validation must consume parser-owned facts and compare against
the C compiler before it counts toward substitution. Re-reading source text to
recover an already parsed semantic fact is not a valid self-host bridge.
