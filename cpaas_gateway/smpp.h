#ifndef SMPP_H
#define SMPP_H

#include <stdint.h>

#define SMPP_PORT 2775

#define SMPP_BIND_TRANSCEIVER      0x00000009
#define SMPP_BIND_TRANSCEIVER_RESP 0x80000009

typedef struct
{
    uint32_t command_length;
    uint32_t command_id;
    uint32_t command_status;
    uint32_t sequence_number;
} SmppHeader;

void smpp_init(void);

int smpp_connect(const char *ip, int port);

int smpp_bind_transceiver(const char *system_id,
                          const char *password);

int smpp_submit_sm(const char *destination,
                   const char *message);

void smpp_disconnect(void);

#endif
