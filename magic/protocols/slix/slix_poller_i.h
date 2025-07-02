#pragma once

#include "slix_poller.h"
#include <nfc/protocols/nfc_generic_event.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TAG "SlixPoller"

#define SLIX_POLLER_MAX_BUFFER_SIZE (64U)
#define SLIX_POLLER_MAX_FWT         (60000U)

typedef enum {
    SlixPollerStateIdle,
    SlixPollerStateRequestMode,
    SlixPollerStateWipe,
    // Add other states like Write here
    SlixPollerStateSuccess,
    SlixPollerStateFail,

    SlixPollerStateNum,
} SlixPollerState;

typedef enum {
    SlixPollerSessionStateIdle,
    SlixPollerSessionStateStarted,
    SlixPollerSessionStateStopRequest,
} SlixPollerSessionState;

struct SlixPoller {
    Nfc* nfc;
    SlixData* slix_data;
    SlixPollerState state;
    SlixPollerSessionState session_state;

    uint16_t current_block;

    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;

    SlixPollerEvent slix_event;
    SlixPollerEventData slix_event_data;

    SlixPollerCallback callback;
    void* context;
};

#ifdef __cplusplus
}
#endif
