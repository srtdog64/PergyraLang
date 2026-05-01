static void
test_struct_sizes(void)
{
    printf("\n[struct_sizes]\n");

    TEST("PgySlot_Int size >= sizeof(int32_t) + sizeof(bool)");
    EXPECT(sizeof(PgySlot_Int) >= sizeof(int32_t) + sizeof(bool));

    TEST("PgySlot_Long size >= sizeof(int64_t) + sizeof(bool)");
    EXPECT(sizeof(PgySlot_Long) >= sizeof(int64_t) + sizeof(bool));

    TEST("PgySlot_Float size >= sizeof(float) + sizeof(bool)");
    EXPECT(sizeof(PgySlot_Float) >= sizeof(float) + sizeof(bool));

    TEST("PgySlot_Double size >= sizeof(double) + sizeof(bool)");
    EXPECT(sizeof(PgySlot_Double) >= sizeof(double) + sizeof(bool));

    TEST("PgySlot_Bool size >= sizeof(bool) + sizeof(bool)");
    EXPECT(sizeof(PgySlot_Bool) >= sizeof(bool) + sizeof(bool));

    TEST("PgySlot_String size >= sizeof(char*) + sizeof(bool)");
    EXPECT(sizeof(PgySlot_String) >= sizeof(char*) + sizeof(bool));
}

static void
test_struct_offsets(void)
{
    printf("\n[struct_offsets]\n");

    TEST("PgySlot_Int: value at offset 0");
    EXPECT(offsetof(PgySlot_Int, value) == 0);

    TEST("PgySlot_Int: occupied after value");
    EXPECT(offsetof(PgySlot_Int, occupied) >= sizeof(int32_t));

    TEST("PgySlot_String: value at offset 0");
    EXPECT(offsetof(PgySlot_String, value) == 0);

    TEST("PgySlot_Double: value at offset 0");
    EXPECT(offsetof(PgySlot_Double, value) == 0);
}

static void
test_secure_slot_sizes(void)
{
    printf("\n[secure_slot_sizes]\n");

    TEST("PgySecureSlot_Int includes token field (> PgySlot_Int)");
    EXPECT(sizeof(PgySecureSlot_Int) > sizeof(PgySlot_Int));

    TEST("PgySecureSlot_Int has uint64_t token field");
    EXPECT(sizeof(PgySecureSlot_Int) >= sizeof(int32_t) + sizeof(bool) + sizeof(uint64_t));

    TEST("PgySecureSlot_String includes token field");
    EXPECT(sizeof(PgySecureSlot_String) > sizeof(PgySlot_String));

    TEST("PgyToken_Int has id + can_write + can_read");
    EXPECT(sizeof(PgyToken_Int) >= sizeof(uint64_t) + 2 * sizeof(bool));
}

/* -----------------------------------------------------------------
 * B. Slot lifecycle tests
 * ----------------------------------------------------------------- */

static void
test_slot_lifecycle_int(void)
{
    printf("\n[slot_lifecycle_int]\n");

    TEST("claim: occupied == true");
    {
        PgySlot_Int s = pgy_claim_Int();
        EXPECT(s.occupied == true);
    }

    TEST("claim: value == 0 (zero-initialized)");
    {
        PgySlot_Int s = pgy_claim_Int();
        EXPECT(s.value == 0);
    }

    TEST("write: value updated correctly");
    {
        PgySlot_Int s = pgy_claim_Int();
        pgy_write_Int(&s, 42);
        EXPECT(s.value == 42);
    }

    TEST("read: returns written value");
    {
        PgySlot_Int s = pgy_claim_Int();
        pgy_write_Int(&s, 99);
        int32_t v = pgy_read_Int(&s);
        EXPECT(v == 99);
    }

    TEST("write overwrite: latest value wins");
    {
        PgySlot_Int s = pgy_claim_Int();
        pgy_write_Int(&s, 10);
        pgy_write_Int(&s, 20);
        pgy_write_Int(&s, 30);
        EXPECT(pgy_read_Int(&s) == 30);
    }

    TEST("release: occupied becomes false");
    {
        PgySlot_Int s = pgy_claim_Int();
        pgy_write_Int(&s, 42);
        pgy_release_Int(&s);
        EXPECT(s.occupied == false);
    }
}

static void
test_slot_lifecycle_string(void)
{
    printf("\n[slot_lifecycle_string]\n");

    TEST("String slot: write and read pointer");
    {
        PgySlot_String s = pgy_claim_String();
        pgy_write_String(&s, "Hello, Pergyra!");
        char *v = pgy_read_String(&s);
        EXPECT(strcmp(v, "Hello, Pergyra!") == 0);
        pgy_release_String(&s);
    }

    TEST("String slot: overwrite pointer");
    {
        PgySlot_String s = pgy_claim_String();
        pgy_write_String(&s, "first");
        pgy_write_String(&s, "second");
        EXPECT(strcmp(pgy_read_String(&s), "second") == 0);
        pgy_release_String(&s);
    }
}

static void
test_slot_lifecycle_all_types(void)
{
    printf("\n[slot_lifecycle_all_types]\n");

    TEST("Long slot: write/read int64_t");
    {
        PgySlot_Long s = pgy_claim_Long();
        pgy_write_Long(&s, 1234567890123LL);
        EXPECT(pgy_read_Long(&s) == 1234567890123LL);
        pgy_release_Long(&s);
    }

    TEST("Float slot: write/read float");
    {
        PgySlot_Float s = pgy_claim_Float();
        pgy_write_Float(&s, 3.14f);
        float diff = pgy_read_Float(&s) - 3.14f;
        EXPECT(diff > -0.001f && diff < 0.001f);
        pgy_release_Float(&s);
    }

    TEST("Double slot: write/read double");
    {
        PgySlot_Double s = pgy_claim_Double();
        pgy_write_Double(&s, 2.718281828);
        double diff = pgy_read_Double(&s) - 2.718281828;
        EXPECT(diff > -0.0001 && diff < 0.0001);
        pgy_release_Double(&s);
    }

    TEST("Bool slot: write/read bool");
    {
        PgySlot_Bool s = pgy_claim_Bool();
        pgy_write_Bool(&s, true);
        EXPECT(pgy_read_Bool(&s) == true);
        pgy_write_Bool(&s, false);
        EXPECT(pgy_read_Bool(&s) == false);
        pgy_release_Bool(&s);
    }
}

static void
test_slot_pin_views(void)
{
    printf("\n[slot_pin_views]\n");

    TEST("pin write view validates occupied slot and records write access");
    {
        PgySlot_Int s = pgy_claim_Int();
        PgyPinnedSlotView_Int view = pgy_pin_write_Int(&s);
        pgy_write_Int(&s, 123);
        pgy_unpin_Int(&view);
        EXPECT(s.value == 123 && view.active == false && view.can_write == true
               && view.slot == NULL);
        pgy_release_Int(&s);
    }

    TEST("pin read view records read-only access");
    {
        PgySlot_Int s = pgy_claim_Int();
        PgyPinnedSlotView_Int view = pgy_pin_read_Int(&s);
        EXPECT(view.active == true && view.can_write == false && view.slot == &s);
        pgy_unpin_Int(&view);
        pgy_release_Int(&s);
    }

    TEST("pin cleanup helper unpins active view and tolerates inactive view");
    {
        PgySlot_Int s = pgy_claim_Int();
        PgyPinnedSlotView_Int view = pgy_pin_write_Int(&s);
        pgy_unpin_cleanup_Int(&view);
        pgy_unpin_cleanup_Int(&view);
        EXPECT(view.active == false && view.slot == NULL);
        pgy_release_Int(&s);
    }

    EXPECT_PANIC("pin read after release triggers panic", {
        PgySlot_Int s = pgy_claim_Int();
        pgy_release_Int(&s);
        (void)pgy_pin_read_Int(&s);
    });

    EXPECT_PANIC("double unpin triggers invariant panic", {
        PgySlot_Int s = pgy_claim_Int();
        PgyPinnedSlotView_Int view = pgy_pin_write_Int(&s);
        pgy_unpin_Int(&view);
        pgy_unpin_Int(&view);
    });
}

/* -----------------------------------------------------------------
 * C. Secure slot tests
 * ----------------------------------------------------------------- */

static void
test_secure_slot_lifecycle(void)
{
    printf("\n[secure_slot_lifecycle]\n");

    TEST("secure claim: occupied + token generated");
    {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        EXPECT(s.occupied == true && tok.id != 0);
        pgy_secure_release_Int(&s, &tok);
    }

    TEST("secure write/read: correct value with valid token");
    {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        pgy_secure_write_Int(&s, 77, &tok);
        int32_t v = pgy_secure_read_Int(&s, &tok);
        EXPECT(v == 77);
        pgy_secure_release_Int(&s, &tok);
    }

    TEST("secure token: can_write and can_read are true by default");
    {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        EXPECT(tok.can_write == true && tok.can_read == true);
        pgy_secure_release_Int(&s, &tok);
    }

    TEST("secure release: occupied becomes false, token zeroed");
    {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        pgy_secure_release_Int(&s, &tok);
        EXPECT(s.occupied == false && s.token == 0);
    }
}

/* -----------------------------------------------------------------
 * D. Panic condition tests (use-after-release, double-release, bad token)
 * ----------------------------------------------------------------- */

static void
test_panic_conditions(void)
{
    printf("\n[panic_conditions]\n");

    EXPECT_PANIC("double release triggers panic", {
        PgySlot_Int s = pgy_claim_Int();
        pgy_release_Int(&s);
        pgy_release_Int(&s); /* should panic */
    });

    EXPECT_PANIC("write after release triggers panic", {
        PgySlot_Int s = pgy_claim_Int();
        pgy_release_Int(&s);
        pgy_write_Int(&s, 42); /* should panic */
    });

    EXPECT_PANIC("read after release triggers panic", {
        PgySlot_Int s = pgy_claim_Int();
        pgy_release_Int(&s);
        pgy_read_Int(&s); /* should panic */
    });

    EXPECT_PANIC("secure write with wrong token triggers panic", {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        PgyToken_Int bad_tok;
        bad_tok.id = 0xBADBADBADBADBADULL;
        bad_tok.can_write = true;
        bad_tok.can_read = true;
        pgy_secure_write_Int(&s, 42, &bad_tok); /* should panic */
    });

    EXPECT_PANIC("secure read with wrong token triggers panic", {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        pgy_secure_write_Int(&s, 10, &tok);
        PgyToken_Int bad_tok;
        bad_tok.id = 0xBADBADBADBADBADULL;
        bad_tok.can_read = true;
        pgy_secure_read_Int(&s, &bad_tok); /* should panic */
    });

    EXPECT_PANIC("secure write without can_write triggers panic", {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        tok.can_write = false; /* remove write permission */
        pgy_secure_write_Int(&s, 42, &tok); /* should panic */
    });

    EXPECT_PANIC("secure read without can_read triggers panic", {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        pgy_secure_write_Int(&s, 10, &tok);
        tok.can_read = false; /* remove read permission */
        pgy_secure_read_Int(&s, &tok); /* should panic */
    });

    EXPECT_PANIC("Result unwrap on Err stays hard-fail", {
        PgyResult_Int result = Err_Int("expected failure");
        (void)Unwrap_Int(result);
    });

    EXPECT_PANIC("Option unwrap on None stays hard-fail", {
        PgyOption_Int option = None_Int();
        (void)UnwrapOption_Int(option);
    });
}

/* -----------------------------------------------------------------
 * E. Multiple slot isolation test
 * ----------------------------------------------------------------- */

static void
test_slot_isolation(void)
{
    printf("\n[slot_isolation]\n");

    TEST("two Int slots are independent");
    {
        PgySlot_Int a = pgy_claim_Int();
        PgySlot_Int b = pgy_claim_Int();
        pgy_write_Int(&a, 100);
        pgy_write_Int(&b, 200);
        EXPECT(pgy_read_Int(&a) == 100 && pgy_read_Int(&b) == 200);
        pgy_release_Int(&a);
        pgy_release_Int(&b);
    }

    TEST("releasing one slot does not affect another");
    {
        PgySlot_Int a = pgy_claim_Int();
        PgySlot_Int b = pgy_claim_Int();
        pgy_write_Int(&a, 10);
        pgy_write_Int(&b, 20);
        pgy_release_Int(&a);
        EXPECT(a.occupied == false && b.occupied == true);
        EXPECT(pgy_read_Int(&b) == 20);
        pgy_release_Int(&b);
    }

    TEST("mixed type slots are independent");
    {
        PgySlot_Int    si = pgy_claim_Int();
        PgySlot_String ss = pgy_claim_String();
        PgySlot_Bool   sb = pgy_claim_Bool();
        pgy_write_Int(&si, 42);
        pgy_write_String(&ss, "test");
        pgy_write_Bool(&sb, true);
        EXPECT(pgy_read_Int(&si) == 42);
        EXPECT(strcmp(pgy_read_String(&ss), "test") == 0);
        EXPECT(pgy_read_Bool(&sb) == true);
        pgy_release_Int(&si);
        pgy_release_String(&ss);
        pgy_release_Bool(&sb);
    }
}

static void
test_secure_slot_pin_views(void)
{
    printf("\n[secure_slot_pin_views]\n");

    TEST("secure pin write validates token and records write access");
    {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        PgyPinnedSecureSlotView_Int view = pgy_secure_pin_write_Int(&s, &tok);
        bool pinned = view.active == true && view.can_write == true
                      && view.slot == &s && view.token == &tok;
        pgy_secure_unpin_Int(&view);
        EXPECT(pinned && view.active == false && view.slot == NULL
               && view.token == NULL);
        pgy_secure_release_Int(&s, &tok);
    }

    TEST("secure pin read rejects write capability");
    {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        PgyPinnedSecureSlotView_Int view = pgy_secure_pin_read_Int(&s, &tok);
        EXPECT(view.active == true && view.can_write == false
               && view.slot == &s && view.token == &tok);
        pgy_secure_unpin_Int(&view);
        pgy_secure_release_Int(&s, &tok);
    }

    TEST("secure pin cleanup helper unpins active view");
    {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        PgyPinnedSecureSlotView_Int view = pgy_secure_pin_write_Int(&s, &tok);
        pgy_secure_unpin_cleanup_Int(&view);
        pgy_secure_unpin_cleanup_Int(&view);
        EXPECT(view.active == false && view.slot == NULL && view.token == NULL);
        pgy_secure_release_Int(&s, &tok);
    }

    EXPECT_PANIC("secure pin with wrong token triggers panic", {
        PgyToken_Int tok;
        PgySecureSlot_Int s = pgy_claim_secure_Int(&tok);
        PgyToken_Int bad_tok;
        bad_tok.id = 0xBADBADBADBADBADULL;
        bad_tok.can_write = true;
        bad_tok.can_read = true;
        (void)pgy_secure_pin_write_Int(&s, &bad_tok);
    });
}

static void
test_allocator_features(void)
{
    printf("\n[allocator]\n");

    TEST("tracing allocator tracks allocation and free counts");
    {
        AllocatorTraceProbe probe = { pgy_allocator_tracing() };
        pgy_with_suppressed_stderr(allocator_trace_probe_run, &probe);
        EXPECT(probe.alloc.allocations == 1 && probe.alloc.deallocations == 1);
    }

    TEST("pool allocator serves allocations from fixed buffer");
    {
        PgyAllocator alloc = pgy_allocator_pool(256);
        void *a = pgy_alloc(&alloc, 32, 8);
        void *b = pgy_alloc(&alloc, 32, 8);
        EXPECT(a != NULL && b != NULL && a != b);
        pgy_allocator_destroy(&alloc);
    }
}

static void
test_rc_weak_features(void)
{
    printf("\n[rc_weak]\n");

    TEST("Rc clone increments strong count and preserves value");
    {
        PgyRc_Int rc = pgy_rc_new_Int(42);
        PgyRc_Int clone = pgy_rc_clone_Int(rc);
        EXPECT(rc.ctrl->strong_count == 2);
        EXPECT(*pgy_rc_get_Int(&clone) == 42);
        pgy_rc_drop_Int(&rc);
        pgy_rc_drop_Int(&clone);
    }

    TEST("Weak upgrade succeeds while strong refs are alive");
    {
        PgyRc_Int rc = pgy_rc_new_Int(7);
        PgyWeak_Int weak = pgy_rc_downgrade_Int(rc);
        PgyRc_Int upgraded = pgy_weak_upgrade_Int(weak);
        EXPECT(*pgy_rc_get_Int(&upgraded) == 7);
        pgy_rc_drop_Int(&rc);
        pgy_rc_drop_Int(&upgraded);
        pgy_weak_drop_Int(&weak);
    }

    EXPECT_PANIC("Weak upgrade after Rc drop triggers panic", {
        PgyRc_Int rc = pgy_rc_new_Int(9);
        PgyWeak_Int weak = pgy_rc_downgrade_Int(rc);
        pgy_rc_drop_Int(&rc);
        pgy_weak_upgrade_Int(weak);
    });
}

static void
test_box_array_features(void)
{
    printf("\n[box_array]\n");

    TEST("BoxArray uses single fused allocation layout");
    {
        BoxArrayTraceProbe probe;
        memset(&probe, 0, sizeof(probe));
        probe.alloc = pgy_allocator_tracing();
        pgy_with_suppressed_stderr(box_array_trace_probe_create, &probe);
        EXPECT(probe.arr->data == ((int32_t*)((char*)probe.arr + sizeof(PgyArray_Int))));
        EXPECT(probe.alloc.allocations == 1);
        pgy_with_suppressed_stderr(box_array_trace_probe_drop, &probe);
    }

    TEST("BoxArray stores values through embedded array");
    {
        PgyBoxArray_Int box = pgy_box_array_new_Int(4, NULL);
        PgyArray_Int *arr = pgy_box_array_get_Int(&box);
        pgy_array_push_Int(arr, 10);
        pgy_array_push_Int(arr, 20);
        EXPECT(pgy_array_get_Int(arr, 0) == 10);
        EXPECT(pgy_array_get_Int(arr, 1) == 20);
        pgy_box_array_drop_Int(&box);
    }

    EXPECT_PANIC("BoxArray get after drop triggers panic", {
        PgyBoxArray_Int box = pgy_box_array_new_Int(2, NULL);
        pgy_box_array_drop_Int(&box);
        (void)pgy_box_array_get_Int(&box);
    });

    TEST("BoxArray double drop is ignored safely");
    {
        PgyBoxArray_Int box = pgy_box_array_new_Int(2, NULL);
        pgy_box_array_drop_Int(&box);
        pgy_box_array_drop_Int(&box);
        EXPECT(box.ptr == NULL);
    }
}

static void
test_pointer_lifetime_guards(void)
{
    printf("\n[pointer_lifetime_guards]\n");

    TEST("Rc double drop is ignored safely");
    {
        PgyRc_Int rc = pgy_rc_new_Int(10);
        pgy_rc_drop_Int(&rc);
        pgy_rc_drop_Int(&rc);
        EXPECT(rc.ctrl == NULL);
    }

    EXPECT_PANIC("Rc get after drop triggers panic", {
        PgyRc_Int rc = pgy_rc_new_Int(11);
        pgy_rc_drop_Int(&rc);
        (void)pgy_rc_get_Int(&rc);
    });

    TEST("Weak double drop is ignored safely");
    {
        PgyRc_Int rc = pgy_rc_new_Int(12);
        PgyWeak_Int weak = pgy_rc_downgrade_Int(rc);
        pgy_weak_drop_Int(&weak);
        pgy_weak_drop_Int(&weak);
        EXPECT(weak.ctrl == NULL);
        pgy_rc_drop_Int(&rc);
    }

    EXPECT_PANIC("Weak upgrade after drop triggers panic", {
        PgyRc_Int rc = pgy_rc_new_Int(13);
        PgyWeak_Int weak = pgy_rc_downgrade_Int(rc);
        pgy_weak_drop_Int(&weak);
        pgy_weak_upgrade_Int(weak);
    });

    TEST("Pool allocator destroy is idempotent");
    {
        PgyAllocator alloc = pgy_allocator_pool(128);
        pgy_allocator_destroy(&alloc);
        pgy_allocator_destroy(&alloc);
        EXPECT(alloc.pool == NULL);
    }

    TEST("Channel destroy is idempotent");
    {
        PgyChannel_Int ch;
        pgy_channel_init_Int(&ch, 4);
        pgy_channel_destroy_Int(&ch);
        pgy_channel_destroy_Int(&ch);
        EXPECT(ch.buf == NULL && ch.cap == 0 && ch.count == 0);
    }

    TEST("Channel close after destroy is ignored safely");
    {
        PgyChannel_Int ch;
        pgy_channel_init_Int(&ch, 4);
        pgy_channel_destroy_Int(&ch);
        pgy_channel_close_Int(&ch);
        EXPECT(ch.buf == NULL && ch.cap == 0);
    }
}

/* -----------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------- */
