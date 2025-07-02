#include "slix_poller.h"
#include "slix.h"

#include <furi/furi.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>

#define TAG "SlixPoller"

#define SLIX_POLLER_THREAD_FLAG_DETECTED (1U << 0)
#define SLIX_POLLER_MAX_BUFFER_SIZE (32) // Inventory response is 12 bytes

typedef struct {
  Nfc *nfc;
  BitBuffer *tx_buffer;
  BitBuffer *rx_buffer;
  FuriThreadId thread_id;
  bool detected;
  SlixData *slix_data;
} SlixPollerDetectContext;

static NfcCommand slix_poller_detect_callback(NfcEvent event, void *context) {
  furi_assert(context);

  NfcCommand command = NfcCommandStop;
  SlixPollerDetectContext *slix_poller_detect_ctx = context;

  if (event.type == NfcEventTypePollerReady) {
    do {
      // Build and send INVENTORY command
      slix_build_inventory_request(slix_poller_detect_ctx->tx_buffer);

      NfcError error = nfc_poller_trx(
          slix_poller_detect_ctx->nfc, slix_poller_detect_ctx->tx_buffer,
          slix_poller_detect_ctx->rx_buffer,
          ISO15693_3_FDT_POLL_FC * 2); // A bit more than standard FWT

      if (error != NfcErrorNone) {
        FURI_LOG_D(TAG, "INVENTORY trx error: %d", error);
        break;
      }

      // A valid INVENTORY response is 12 bytes (flags, dsfid, uid, crc)
      // We check for a response of at least that size with no error flags.
      if (bit_buffer_get_size_bytes(slix_poller_detect_ctx->rx_buffer) >= 12) {
        uint8_t flags =
            bit_buffer_get_byte(slix_poller_detect_ctx->rx_buffer, 0);
        if (!(flags & ISO15693_3_RESP_FLAG_ERROR)) {
          slix_poller_detect_ctx->detected = true;
          // The UID starts at the 3rd byte (index 2) of the INVENTORY response.
          const uint8_t *uid_lsb =
              &bit_buffer_get_data(slix_poller_detect_ctx->rx_buffer)[2];
          memcpy(slix_poller_detect_ctx->slix_data->uid, uid_lsb, SLIX_UID_LEN);
        }
      }

    } while (false);
  }
  furi_thread_flags_set(slix_poller_detect_ctx->thread_id,
                        SLIX_POLLER_THREAD_FLAG_DETECTED);

  return command;
}

bool slix_poller_detect(Nfc *nfc, SlixData *slix_data) {
  furi_assert(nfc);

  nfc_config(nfc, NfcModePoller, NfcTechIso15693);
  nfc_set_guard_time_us(nfc, ISO15693_3_GUARD_TIME_US);
  nfc_set_fdt_poll_fc(nfc, ISO15693_3_FDT_POLL_FC);

  SlixPollerDetectContext slix_poller_detect_ctx = {
      .nfc = nfc,
      .tx_buffer = bit_buffer_alloc(SLIX_POLLER_MAX_BUFFER_SIZE),
      .rx_buffer = bit_buffer_alloc(SLIX_POLLER_MAX_BUFFER_SIZE),
      .thread_id = furi_thread_get_current_id(),
      .detected = false,
      .slix_data = slix_data,
  };

  nfc_start(nfc, slix_poller_detect_callback, &slix_poller_detect_ctx);

  // Wait for detection to finish, with a timeout
  furi_thread_flags_wait(SLIX_POLLER_THREAD_FLAG_DETECTED, FuriFlagWaitAny,
                         200);
  nfc_stop(nfc);

  bit_buffer_free(slix_poller_detect_ctx.tx_buffer);
  bit_buffer_free(slix_poller_detect_ctx.rx_buffer);

  return slix_poller_detect_ctx.detected;
}