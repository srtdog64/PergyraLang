#ifndef PERGYRA_RUNTIME_PGY_CHANNEL_H
#define PERGYRA_RUNTIME_PGY_CHANNEL_H

#define pgy_channel_send(channel, value) ((void)(channel), (void)(value))
#define pgy_channel_recv(channel, type) ((void)(channel), (type)0)

#endif
