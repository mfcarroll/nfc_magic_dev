#pragma once

#include <nfc/nfc.h>
#include "slix.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Detect a SLIX (ISO15693) magic card.
 *
 * @param nfc Nfc instance.
 * @param[out] slix_data A pointer to the SlixData instance to be filled.
 * @return true if a SLIX card was detected, false otherwise.
 */
bool slix_poller_detect(Nfc* nfc, SlixData* slix_data);

#ifdef __cplusplus
}
#endif
