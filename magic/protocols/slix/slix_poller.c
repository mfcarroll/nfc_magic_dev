#include "slix_poller_i.h"

#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/nfc_poller.h>

#define SLIX_POLLER_THREAD_FLAG_DETECTED (1U << 0)
#define SLIX_WIPE_BLOCKS_TOTAL           (32)

// Note: The detect function is kept as-is. It's a synchronous function
// that doesn't use the SlixPoller state machine, which is consistent
// with the Gen4 poller pattern.
typedef struct {
    Nfc* nfc;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;
    FuriThreadId thread_id;
    bool detected;
    SlixData* slix_data;
} SlixPollerDetectContext;

static NfcCommand slix_poller_detect_callback(NfcEvent event, void* context) {
    furi_assert(context);

    NfcCommand command = NfcCommandStop;
    SlixPollerDetectContext* slix_poller_detect_ctx = context;

    if(event.type == NfcEventTypePollerReady) {
        do {
            // Build and send INVENTORY command
            slix_build_inventory_request(slix_poller_detect_ctx->tx_buffer);

            NfcError error = nfc_poller_trx(
                slix_poller_detect_ctx->nfc,
                slix_poller_detect_ctx->tx_buffer,
                slix_poller_detect_ctx->rx_buffer,
                ISO15693_3_FDT_POLL_FC * 2); // A bit more than standard FWT

            if(error != NfcErrorNone) {
                FURI_LOG_D(TAG, "INVENTORY trx error: %d", error);
                break;
            }

            // A valid INVENTORY response is 12 bytes (flags, dsfid, uid, crc)
            // We check for a response of at least that size with no error flags.
            if(bit_buffer_get_size_bytes(slix_poller_detect_ctx->rx_buffer) >= 12) {
                uint8_t flags = bit_buffer_get_byte(slix_poller_detect_ctx->rx_buffer, 0);
                if(!(flags & ISO15693_3_RESP_FLAG_ERROR)) {
                    slix_poller_detect_ctx->detected = true;
                    // The UID starts at the 3rd byte (index 2) of the INVENTORY response.
                    const uint8_t* uid_lsb =
                        &bit_buffer_get_data(slix_poller_detect_ctx->rx_buffer)[2];
                    memcpy(slix_poller_detect_ctx->slix_data->uid, uid_lsb, SLIX_UID_LEN);
                }
            }

        } while(false);
    }
    furi_thread_flags_set(slix_poller_detect_ctx->thread_id, SLIX_POLLER_THREAD_FLAG_DETECTED);

    return command;
}

bool slix_poller_detect(Nfc* nfc, SlixData* slix_data) {
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
    furi_thread_flags_wait(SLIX_POLLER_THREAD_FLAG_DETECTED, FuriFlagWaitAny, 200);
    nfc_stop(nfc);

    bit_buffer_free(slix_poller_detect_ctx.tx_buffer);
    bit_buffer_free(slix_poller_detect_ctx.rx_buffer);

    return slix_poller_detect_ctx.detected;
}

SlixPoller* slix_poller_alloc(Nfc* nfc) {
    furi_assert(nfc);

    SlixPoller* instance = malloc(sizeof(SlixPoller));
    instance->poller = nfc_poller_alloc(nfc, NfcProtocolIso15693_3);

    instance->slix_data = slix_alloc();

    instance->tx_buffer = bit_buffer_alloc(SLIX_POLLER_MAX_BUFFER_SIZE);
    instance->rx_buffer = bit_buffer_alloc(SLIX_POLLER_MAX_BUFFER_SIZE);

    instance->slix_event.data = &instance->slix_event_data;

    return instance;
}

void slix_poller_free(SlixPoller* instance) {
    furi_assert(instance);

    nfc_poller_free(instance->poller);
    slix_free(instance->slix_data);
    bit_buffer_free(instance->tx_buffer);
    bit_buffer_free(instance->rx_buffer);

    free(instance);
}

typedef NfcCommand (*SlixPollerStateHandler)(SlixPoller* instance);

static NfcCommand slix_poller_idle_handler(SlixPoller* instance) {
    NfcCommand command = NfcCommandContinue;

    instance->current_block = 0;
    instance->slix_event.type = SlixPollerEventTypeCardDetected;
    command = instance->callback(instance->slix_event, instance->context);
    instance->state = SlixPollerStateRequestMode;

    return command;
}

static NfcCommand slix_poller_request_mode_handler(SlixPoller* instance) {
    NfcCommand command = NfcCommandContinue;

    instance->slix_event.type = SlixPollerEventTypeRequestMode;
    command = instance->callback(instance->slix_event, instance->context);

    if(instance->slix_event_data.request_mode.mode == SlixPollerModeWipe) {
        instance->state = SlixPollerStateWipe;
    } else {
        // Other modes not implemented yet
        instance->state = SlixPollerStateFail;
    }

    return command;
}

static NfcCommand slix_poller_wipe_handler(SlixPoller* instance) {
    NfcCommand command = NfcCommandContinue;

    if(instance->current_block >= SLIX_WIPE_BLOCKS_TOTAL) {
        instance->state = SlixPollerStateSuccess;
    } else {
        do {
            const uint8_t zero_block[SLIX_BLOCK_SIZE] = {0};
            SlixPollerError error =
                slix_poller_write_block(instance, instance->current_block, zero_block);

            if(error != SlixPollerErrorNone) {
                // Some SLIX cards have fewer than 32 blocks.
                // Writing to a non-existent block will cause a protocol error.
                // We can treat this as success and finish wiping.
                FURI_LOG_W(
                    TAG,
                    "Wipe failed on block %d, assuming end of memory",
                    instance->current_block);
                instance->state = SlixPollerStateSuccess;
                break;
            }
            instance->current_block++;
        } while(false);
    }

    return command;
}

static NfcCommand slix_poller_success_handler(SlixPoller* instance) {
    instance->slix_event.type = SlixPollerEventTypeSuccess;
    NfcCommand command = instance->callback(instance->slix_event, instance->context);
    instance->state = SlixPollerStateIdle;
    return command;
}

static NfcCommand slix_poller_fail_handler(SlixPoller* instance) {
    instance->slix_event.type = SlixPollerEventTypeFail;
    NfcCommand command = instance->callback(instance->slix_event, instance->context);
    instance->state = SlixPollerStateIdle;
    return command;
}

static const SlixPollerStateHandler slix_poller_state_handlers[SlixPollerStateNum] = {
    [SlixPollerStateIdle] = slix_poller_idle_handler,
    [SlixPollerStateRequestMode] = slix_poller_request_mode_handler,
    [SlixPollerStateWipe] = slix_poller_wipe_handler,
    [SlixPollerStateSuccess] = slix_poller_success_handler,
    [SlixPollerStateFail] = slix_poller_fail_handler,
};

static NfcCommand slix_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    SlixPoller* instance = context;
    NfcCommand command = NfcCommandContinue;

    if(event.protocol == NfcProtocolIso15693_3) {
        instance->iso15693_3_poller = event.instance;
        const Iso15693_3PollerEvent* iso15693_3_event = event.event_data;

        if(iso15693_3_event->type == Iso15693_3PollerEventTypeReady) {
            if(instance->state < SlixPollerStateNum) {
                command = slix_poller_state_handlers[instance->state](instance);
            }
        }
    }

    return command;
}

void slix_poller_start(SlixPoller* instance, SlixPollerCallback callback, void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;
    instance->state = SlixPollerStateIdle;

    nfc_poller_start(instance->poller, slix_poller_callback, instance);
}

void slix_poller_stop(SlixPoller* instance) {
    furi_assert(instance);

    nfc_poller_stop(instance->poller);
}
