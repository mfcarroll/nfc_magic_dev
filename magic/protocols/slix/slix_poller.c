#include "slix_poller_i.h"

#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <furi/furi.h>

#define SLIX_POLLER_THREAD_FLAG_DETECTED (1U << 0)

// For this example, we assume a SLIX-S card with 32 blocks.
// A more robust implementation would first use GET_SYSTEM_INFORMATION
// to determine the card's memory size.
#define SLIX_TOTAL_BLOCKS (32)

typedef NfcCommand (*SlixPollerStateHandler)(SlixPoller* instance);

static const uint8_t slix_empty_block[4] = {0x00, 0x00, 0x00, 0x00};

typedef struct {
    Nfc* nfc;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;
    FuriThreadId thread_id;
    bool detected;
    SlixData* slix_data;
} SlixPollerDetectContext;

static SlixPollerError slix_poller_process_error(NfcError error) {
    SlixPollerError ret = SlixPollerErrorNone;

    if(error == NfcErrorNone) {
        ret = SlixPollerErrorNone;
    } else if(error == NfcErrorTimeout) {
        ret = SlixPollerErrorTimeout;
    } else {
        ret = SlixPollerErrorProtocol;
    }
    return ret;
}

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
    instance->nfc = nfc;
    instance->slix_data = slix_alloc();

    instance->tx_buffer = bit_buffer_alloc(SLIX_POLLER_MAX_BUFFER_SIZE);
    instance->rx_buffer = bit_buffer_alloc(SLIX_POLLER_MAX_BUFFER_SIZE);

    instance->slix_event.data = &instance->slix_event_data;

    return instance;
}

void slix_poller_free(SlixPoller* instance) {
    furi_assert(instance);

    slix_free(instance->slix_data);
    bit_buffer_free(instance->tx_buffer);
    bit_buffer_free(instance->rx_buffer);

    free(instance);
}

static void slix_poller_reset(SlixPoller* instance) {
    instance->state = SlixPollerStateIdle;
    instance->session_state = SlixPollerSessionStateIdle;
    instance->current_block = 0;
}

NfcCommand slix_poller_idle_handler(SlixPoller* instance) {
    slix_poller_reset(instance);

    instance->slix_event.type = SlixPollerEventTypeCardDetected;
    NfcCommand command = instance->callback(instance->slix_event, instance->context);

    // The UID must be set by the caller after detection and before starting the poller
    furi_assert(instance->slix_data->uid[0] != 0);

    instance->state = SlixPollerStateRequestMode;

    return command;
}

NfcCommand slix_poller_request_mode_handler(SlixPoller* instance) {
    instance->slix_event.type = SlixPollerEventTypeRequestMode;
    NfcCommand command = instance->callback(instance->slix_event, instance->context);

    if(instance->slix_event_data.request_mode.mode == SlixPollerModeWipe) {
        instance->state = SlixPollerStateWipe;
    } else {
        // Other modes would be handled here
        instance->state = SlixPollerStateFail;
    }

    return command;
}

NfcCommand slix_poller_wipe_handler(SlixPoller* instance) {
    NfcCommand command = NfcCommandContinue;

    if(instance->current_block < SLIX_TOTAL_BLOCKS) {
        slix_build_write_block_request(
            instance->tx_buffer,
            instance->slix_data->uid,
            instance->current_block,
            slix_empty_block);

        NfcError error = nfc_poller_trx(
            instance->nfc, instance->tx_buffer, instance->rx_buffer, SLIX_POLLER_MAX_FWT);

        if(error != NfcErrorNone) {
            FURI_LOG_E(
                TAG,
                "Failed to write block %u: %d",
                instance->current_block,
                slix_poller_process_error(error));
            instance->state = SlixPollerStateFail;
        } else {
            instance->current_block++;
        }
    } else {
        instance->state = SlixPollerStateSuccess;
    }

    return command;
}

NfcCommand slix_poller_success_handler(SlixPoller* instance) {
    instance->slix_event.type = SlixPollerEventTypeSuccess;
    NfcCommand command = instance->callback(instance->slix_event, instance->context);
    instance->state = SlixPollerStateIdle;
    return command;
}

NfcCommand slix_poller_fail_handler(SlixPoller* instance) {
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

static NfcCommand slix_poller_callback(NfcEvent event, void* context) {
    furi_assert(context);
    SlixPoller* instance = context;
    NfcCommand command = NfcCommandContinue;

    if(event.type == NfcEventTypePollerReady) {
        if(instance->state < SlixPollerStateNum) {
            command = slix_poller_state_handlers[instance->state](instance);
        }
    } else if(event.type == NfcEventTypeFieldOff) {
        // Handle card removal if needed
    }

    if(instance->session_state == SlixPollerSessionStateStopRequest) {
        command = NfcCommandStop;
    }

    return command;
}

void slix_poller_start(
    SlixPoller* instance,
    const SlixData* slix_data,
    SlixPollerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    furi_assert(slix_data);

    instance->callback = callback;
    instance->context = context;

    // Copy data from the provided structure to the poller instance.
    slix_copy(instance->slix_data, slix_data);

    nfc_config(instance->nfc, NfcModePoller, NfcTechIso15693);
    nfc_set_guard_time_us(instance->nfc, ISO15693_3_GUARD_TIME_US);
    nfc_set_fdt_poll_fc(instance->nfc, ISO15693_3_FDT_POLL_FC);

    instance->session_state = SlixPollerSessionStateStarted;
    nfc_start(instance->nfc, slix_poller_callback, instance);
}

void slix_poller_stop(SlixPoller* instance) {
    furi_assert(instance);

    instance->session_state = SlixPollerSessionStateStopRequest;
    nfc_stop(instance->nfc);
    slix_poller_reset(instance);
}
