#ifndef PGY_CALLABLE_CAPABILITY_EQUATIONS_INTERNAL_H
#define PGY_CALLABLE_CAPABILITY_EQUATIONS_INTERNAL_H
#include "callable_capability_inference.h"

/* One semantic-arena equation store shared by collection, dispatch projection
 * and the fixed-point seal. Consumers never reconstruct it from source bodies. */
typedef struct CapabilityInstance CapabilityInstance;
typedef enum { CAP_UNKNOWN, CAP_DECL, CAP_FORMAL, CAP_BINDING, CAP_RESULT, CAP_CONSTRUCTOR } CapabilityTargetKind;
typedef struct {
    CapabilityTargetKind kind;
    uint32_t id;
} CapabilityTarget;
/* Callable captures are rejected by the capture owner. Scalar copy captures
 * do not affect a lambda's capability identity; interning declaration/actual
 * tuples therefore remains finite even when recursion creates local lambdas. */
typedef struct CapabilityCall {
    ASTNode *site;
    CapabilityTarget callee;
    CapabilityTarget *actuals;
    size_t count;
    uint32_t event_id;
    struct CapabilityCall *next;
} CapabilityCall;
typedef struct CapabilityBinding {
    uint32_t id;
    CapabilityTarget value;
    struct CapabilityBinding *next;
} CapabilityBinding;
typedef struct CapabilityDispatch {
    uint32_t signature_id, implementation_id;
    ASTNode *site;
    struct CapabilityDispatch *next;
} CapabilityDispatch;

struct CallableCapabilityRoutine {
    ASTNode *decl;
    uint32_t id;
    uint32_t *formals;
    size_t count;
    uint32_t direct_mask;
    uint32_t declared_mask;
    uint32_t direct_effects;
    uint32_t declared_effects;
    bool has_effect_contract;
    bool abstract_dispatch;
    Type *type;
    CapabilityTarget result;
    bool has_result;
    CapabilityCall *calls;
    CapabilityInstance *instances;
    struct CallableCapabilityRoutine *next;
};
typedef struct CapabilityParent {
    CapabilityInstance *instance;
    struct CapabilityParent *next;
} CapabilityParent;
struct CapabilityInstance {
    CallableCapabilityRoutine *routine;
    CapabilityTarget *actuals;
    CapabilityInstance *next;
    CapabilityInstance *routine_next;
    CapabilityInstance *queue_next;
    CapabilityParent *parents;
    uint32_t used;
    uint32_t used_effects;
    bool closed;
    bool deferred;
    bool queued;
};
struct CallableCapabilityStore {
    CallableCapabilityRoutine *routines;
    CallableCapabilityRoutine **index;
    size_t count;
    size_t equation_count;
    CapabilityBinding *bindings;
    CapabilityCall *subscriptions;
    CapabilityDispatch *dispatches;
    CapabilityInstance *instances;
    CapabilityInstance *last_instance;
    bool failed;
};


void *callable_capability_allocate_equation(SemanticContext *ctx, size_t bytes);
CallableCapabilityRoutine *callable_capability_find_routine(struct CallableCapabilityStore *store, uint32_t id);
bool callable_capability_seal_dispatch(SemanticContext *ctx);
#endif
