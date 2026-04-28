/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Function and resource effect mask helpers.
 */

#include "type_system.h"

uint32_t
type_function_effects(const Type *type)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return EFFECT_NONE;
    return type->data.function.effect_mask;
}

uint32_t
type_function_body_summary(const Type *type)
{
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return BODY_SUMMARY_NONE;
    return type->data.function.body_summary_mask;
}

uint32_t
type_effect_mask_closure(uint32_t mask)
{
    if ((mask & EFFECT_COLLAPSE) != 0)
        mask |= EFFECT_NONDETERMINISTIC;
    return mask;
}

uint32_t
type_effect_mask_join(uint32_t left, uint32_t right)
{
    return type_effect_mask_closure(left) | type_effect_mask_closure(right);
}

uint32_t
type_effect_mask_meet(uint32_t left, uint32_t right)
{
    return type_effect_mask_closure(left) & type_effect_mask_closure(right);
}

bool
type_effect_mask_requires_authority(uint32_t mask)
{
    uint32_t closed = type_effect_mask_closure(mask);
    return (closed & EFFECT_SECURE) != 0;
}

bool
type_effect_mask_touches_resource_boundary(uint32_t mask)
{
    uint32_t closed = type_effect_mask_closure(mask);
    return (closed & (EFFECT_SECURE | EFFECT_REMOTE | EFFECT_COLLAPSE)) != 0;
}

bool
type_effect_mask_has(uint32_t mask, uint32_t effect)
{
    uint32_t closed_mask = type_effect_mask_closure(mask);
    uint32_t closed_effect = type_effect_mask_closure(effect);
    return (closed_mask & closed_effect) == closed_effect;
}

bool
type_effect_mask_subsumes(uint32_t available, uint32_t required)
{
    uint32_t closed_available = type_effect_mask_closure(available);
    uint32_t closed_required = type_effect_mask_closure(required);
    return (closed_available & closed_required) == closed_required;
}

bool
type_effect_mask_conflicts(uint32_t left, uint32_t right)
{
    uint32_t closed_left = type_effect_mask_closure(left);
    uint32_t closed_right = type_effect_mask_closure(right);
    uint32_t boundary_mask = EFFECT_REMOTE | EFFECT_COLLAPSE | EFFECT_NONDETERMINISTIC;

    if ((closed_left & EFFECT_SECURE) != 0 && (closed_right & boundary_mask) != 0)
        return true;
    if ((closed_left & boundary_mask) != 0 && (closed_right & EFFECT_SECURE) != 0)
        return true;
    return false;
}

EffectMaskRelation
type_effect_mask_compare(uint32_t left, uint32_t right)
{
    uint32_t closed_left = type_effect_mask_closure(left);
    uint32_t closed_right = type_effect_mask_closure(right);
    bool left_has_right = (closed_left & closed_right) == closed_right;
    bool right_has_left = (closed_right & closed_left) == closed_left;

    if (left_has_right && right_has_left)
        return EFFECT_REL_EQUAL;
    if (left_has_right)
        return EFFECT_REL_SUPERSET;
    if (right_has_left)
        return EFFECT_REL_SUBSET;
    return EFFECT_REL_INCOMPARABLE;
}

