#ifndef PGY_RUNTIME_MEDIA_STUB_H
#define PGY_RUNTIME_MEDIA_STUB_H

/*
 * Capability-gated media host API -- headless stub backend.
 *
 * The render/audio/input capabilities (PGY_CAP_RENDER/AUDIO/INPUT) need a real
 * backend (canvas/WebGL/WebAudio), which lands with the browser/WASM target.
 * Until then these stubs give the capability model a complete surface: every
 * media entry point is gated through the same fail-closed boundary as the
 * ambient ops, so a manifest that omits RENDER literally cannot draw, today.
 * The backend is headless (it only records call counts), so the GATE is
 * testable now and the API shape is fixed for the future backend to implement.
 *
 * These are static-inline (C-output) twins; they call the external capability
 * gate. When media gets surface syntax + codegen, the external twins + LLVM
 * decls follow the same dual pattern as the ambient ops.
 */

#include "pgy_runtime_capability.h"

#include <stdint.h>

/* Headless backend state: call counters, so the gate + API are observable
 * without a real device. */
static inline uint32_t *
pgy_media_counter(int which)
{
    static uint32_t counters[3]; /* 0:render 1:audio 2:input */
    if (which < 0 || which > 2)
        return &counters[0];
    return &counters[which];
}

/* RENDER */
static inline void
pgy_render_clear(int32_t rgba)
{
    pgy_cap_require_export(PGY_CAP_RENDER, "render-clear");
    (void)rgba;
    (*pgy_media_counter(0))++;
}

static inline void
pgy_render_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t rgba)
{
    pgy_cap_require_export(PGY_CAP_RENDER, "render-fill-rect");
    (void)x; (void)y; (void)w; (void)h; (void)rgba;
    (*pgy_media_counter(0))++;
}

/* AUDIO */
static inline void
pgy_audio_play_tone(int32_t freq_hz, int32_t ms)
{
    pgy_cap_require_export(PGY_CAP_AUDIO, "audio-play-tone");
    (void)freq_hz; (void)ms;
    (*pgy_media_counter(1))++;
}

/* INPUT */
static inline int32_t
pgy_input_poll_key(void)
{
    pgy_cap_require_export(PGY_CAP_INPUT, "input-poll-key");
    (*pgy_media_counter(2))++;
    return 0; /* headless: no key */
}

static inline uint32_t
pgy_media_call_count(int which)
{
    return *pgy_media_counter(which);
}

#endif /* PGY_RUNTIME_MEDIA_STUB_H */
