#pragma once

#include <nfc/nfc.h>
#include <nfc/protocols/nfc_generic_event.h>
#include "slix.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SlixPollerEventTypeCardDetected,
    SlixPollerEventTypeRequestMode,
    // Add other events like RequestDataToWrite here
    SlixPollerEventTypeSuccess,
    SlixPollerEventTypeFail,
} SlixPollerEventType;

typedef enum {
    SlixPollerModeWipe,
    SlixPollerModeGetInfo,
    // Add other modes like Write here
} SlixPollerMode;

typedef struct {
    SlixPollerMode mode;
} SlixPollerEventDataRequestMode;

// Add other event data structs here

typedef union {
    SlixPollerEventDataRequestMode request_mode;
    // Add other event data unions here
} SlixPollerEventData;

typedef struct {
    SlixPollerEventType type;
    SlixPollerEventData* data;
} SlixPollerEvent;

typedef NfcCommand (*SlixPollerCallback)(SlixPollerEvent event, void* context);

typedef struct SlixPoller SlixPoller;

/**
 * @brief Detect a SLIX (ISO15693) magic card.
 *
 * @param nfc Nfc instance.
 * @param[out] slix_data A pointer to the SlixData instance to be filled.
 * @return true if a SLIX card was detected, false otherwise.
 */
bool slix_poller_detect(Nfc* nfc, SlixData* slix_data);

SlixPoller* slix_poller_alloc(Nfc* nfc);

void slix_poller_free(SlixPoller* instance);

void slix_poller_start(SlixPoller* instance, SlixPollerCallback callback, void* context);

void slix_poller_stop(SlixPoller* instance);

void slix_poller_set_data(SlixPoller* instance, const SlixData* data);

#ifdef __cplusplus
}
#endif
