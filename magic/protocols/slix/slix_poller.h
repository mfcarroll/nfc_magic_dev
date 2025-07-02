#pragma once

#include <nfc/nfc.h>
#include <nfc/protocols/nfc_generic_event.h>
#include "slix.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SlixPollerErrorNone,
    SlixPollerErrorTimeout,
    SlixPollerErrorNotPresent,
    SlixPollerErrorProtocol,
} SlixPollerError;

typedef enum {
    SlixPollerEventTypeCardDetected,
    SlixPollerEventTypeRequestMode,
    SlixPollerEventTypeSuccess,
    SlixPollerEventTypeFail,
} SlixPollerEventType;

typedef enum {
    SlixPollerModeWipe,
    // Other modes like Write can be added here
} SlixPollerMode;

typedef struct {
    SlixPollerMode mode;
} SlixPollerEventDataRequestMode;

typedef union {
    SlixPollerEventDataRequestMode request_mode;
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

/**
 * @brief Allocate a SlixPoller instance.
 * @param nfc Nfc instance.
 * @return SlixPoller* pointer to the allocated instance.
 */
SlixPoller* slix_poller_alloc(Nfc* nfc);

/**
 * @brief Free a SlixPoller instance.
 * @param instance Pointer to the SlixPoller instance to free.
 */
void slix_poller_free(SlixPoller* instance);

void slix_poller_start(
    SlixPoller* instance,
    const SlixData* slix_data,
    SlixPollerCallback callback,
    void* context);

void slix_poller_stop(SlixPoller* instance);

#ifdef __cplusplus
}
#endif
