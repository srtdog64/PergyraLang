/*
 * Copyright (c) 2025 Pergyra Language Project
 * Channel convenience macros — real implementation in pgy_runtime.h
 * BSD 3-Clause License
 */

#ifndef PERGYRA_RUNTIME_PGY_CHANNEL_H
#define PERGYRA_RUNTIME_PGY_CHANNEL_H

/*
 * The actual thread-safe channels are defined by PGY_CHANNEL_DEFINE()
 * in pgy_runtime.h. These wrappers dispatch by channel lvalue type.
 */
#define pgy_channel_send(channel, value) \
    _Generic(&(channel), \
        PgyChannel_Int *: pgy_channel_send_Int, \
        PgyChannel_String *: pgy_channel_send_String \
    )(&(channel), (value))

#define pgy_channel_recv(channel, type) \
    ({ \
        type _pgy_value = (type)0; \
        (void)_Generic(&(channel), \
            PgyChannel_Int *: pgy_channel_recv_Int, \
            PgyChannel_String *: pgy_channel_recv_String \
        )(&(channel), &_pgy_value); \
        _pgy_value; \
    })

#endif
