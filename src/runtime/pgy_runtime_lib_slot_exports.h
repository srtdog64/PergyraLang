#ifndef PGY_RUNTIME_LIB_SLOT_EXPORTS_H
#define PGY_RUNTIME_LIB_SLOT_EXPORTS_H

/* LLVM-linkable primitive slot exports. */

PgySlot_Double pgy_claim_Double(void)
{
    PgySlot_Double s;
    s.value = 0.0;
    s.claimed = true;
    return s;
}

void pgy_write_Double(PgySlot_Double *s, double v)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Double slot write");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE);
    }
    s->value = v;
}

double pgy_read_Double(PgySlot_Double *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Double slot read");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ);
    }
    return s->value;
}

void pgy_release_Double(PgySlot_Double *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Double slot release");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE,
                          PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_SLOT);
    }
    s->value = 0.0;
    s->claimed = false;
}

PgySlot_Bool pgy_claim_Bool(void)
{
    PgySlot_Bool s;
    s.value = false;
    s.claimed = true;
    return s;
}

void pgy_write_Bool(PgySlot_Bool *s, bool v)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Bool slot write");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE);
    }
    s->value = v;
}

bool pgy_read_Bool(PgySlot_Bool *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Bool slot read");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ);
    }
    return s->value;
}

void pgy_release_Bool(PgySlot_Bool *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Bool slot release");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE,
                          PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_SLOT);
    }
    s->value = false;
    s->claimed = false;
}

PgySlot_String pgy_claim_String(void)
{
    PgySlot_String s;
    s.value = NULL;
    s.claimed = true;
    return s;
}

void pgy_write_String(PgySlot_String *s, char *v)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null String slot write");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE);
    }
    s->value = v;
}

char *pgy_read_String(PgySlot_String *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null String slot read");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ);
    }
    return s->value;
}

void pgy_release_String(PgySlot_String *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null String slot release");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE,
                          PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_SLOT);
    }
    s->value = NULL;
    s->claimed = false;
}

#endif /* PGY_RUNTIME_LIB_SLOT_EXPORTS_H */
