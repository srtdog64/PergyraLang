#ifndef PERGYRA_MAP_STRING_PAYLOAD_VIEW_OWNER_H
#define PERGYRA_MAP_STRING_PAYLOAD_VIEW_OWNER_H

/* Liveness of a String MapGet returned from a HashMap<K, String>.
 *
 * The map owns each String value: MapSet replaces and MapRemove frees the
 * value a view points at, and an inout hand-off or a reassignment of the map
 * may do either. The identity is the map binding's storage source (a
 * SyntaxNodeId). A value that carries a view (the String, an alias of it, an
 * Array or struct built from it) is refused at its next use once that
 * storage changed; a later argument of the same call that changes it refuses
 * the view argument. A copy (Concat("", view), Clone) carries nothing. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;
typedef struct Type Type;

void semantic_map_payload_view_initialize_binding(Symbol *binding,
    ASTNode *initializer, SemanticContext *ctx);
void semantic_map_payload_view_record_assignment(Symbol *target,
    ASTNode *value, ASTNode *assignment, SemanticContext *ctx);
/* `receiver` changes the String values of the map typed `map_type` (NULL:
 * the type of the identifier `receiver` names). */
void semantic_map_payload_view_change(ASTNode *receiver, Type *map_type,
    SemanticContext *ctx);
bool semantic_map_payload_view_validate_use(Symbol *binding, ASTNode *use,
    SemanticContext *ctx);

/* One call's arguments in evaluation order: `begin` before the first, then
 * `argument` after each is checked, then `finish` once the callee's own
 * inout hand-offs are recorded. `finish` refuses a view argument whose map a
 * later argument or the callee changes, and releases the frame. */
typedef struct {
    uint32_t *sources;
    size_t   *marks;
    ASTNode **arguments;
    size_t    count;
    size_t    capacity;
} MapPayloadViewCallFrame;
void semantic_map_payload_view_call_begin(MapPayloadViewCallFrame *frame);
bool semantic_map_payload_view_call_argument(MapPayloadViewCallFrame *frame,
    ASTNode *argument, Type *argument_type, SemanticContext *ctx);
bool semantic_map_payload_view_call_finish(MapPayloadViewCallFrame *frame,
    SemanticContext *ctx);

#endif
