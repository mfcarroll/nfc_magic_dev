#pragma once

#include "slix_poller.h"
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TAG                         "SlixPoller"
#define SLIX_POLLER_MAX_BUFFER_SIZE (64U)
#define SLIX_POLLER_MAX_FWT         (60000U)

typedef enum {
    SlixPollerErrorNone,
    SlixPollerErrorTimeout,
    SlixPollerErrorProtocol,
} SlixPollerError;

typedef enum {
    SlixPollerStateIdle,
    SlixPollerStateRequestMode,
    SlixPollerStateWipe,
    // Add other states like Write here
    SlixPollerStateSuccess,
    SlixPollerStateFail,

    SlixPollerStateNum,
} SlixPollerState;

struct SlixPoller {
    NfcPoller* poller;
    Iso15693_3Poller* iso15693_3_poller;
    SlixPollerState state;

    SlixData* slix_data;

    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    uint16_t current_block;

    SlixPollerEvent slix_event;
    SlixPollerEventData slix_event_data;

    SlixPollerCallback callback;
    void* context;
};

SlixPollerError
    slix_poller_write_block(SlixPoller* instance, uint8_t block_num, const uint8_t* data);

#ifdef __cplusplus
}
#endif
