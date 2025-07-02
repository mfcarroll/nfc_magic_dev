#include "slix_poller_i.h"
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_i.h>

#include <furi/furi.h>

#define TAG "SlixPoller"

static SlixPollerError slix_poller_process_error(Iso15693_3Error error) {
    SlixPollerError ret = SlixPollerErrorNone;

    switch(error) {
    case Iso15693_3ErrorNone:
        ret = SlixPollerErrorNone;
        break;
    case Iso15693_3ErrorNotPresent:
    case Iso15693_3ErrorTimeout:
        ret = SlixPollerErrorTimeout;
        break;
    default:
        ret = SlixPollerErrorProtocol;
        break;
    }

    return ret;
}

SlixPollerError
    slix_poller_write_block(SlixPoller* instance, uint8_t block_num, const uint8_t* data) {
    furi_assert(instance);
    bit_buffer_reset(instance->tx_buffer);

    // Flags: Addressed, High data rate
    uint8_t flags = ISO15693_3_REQ_FLAG_T4_ADDRESSED | ISO15693_3_REQ_FLAG_DATA_RATE_HI;
    bit_buffer_append_byte(instance->tx_buffer, flags);
    // Command
    bit_buffer_append_byte(instance->tx_buffer, ISO15693_3_CMD_WRITE_BLOCK);
    // Block number
    bit_buffer_append_byte(instance->tx_buffer, block_num);
    // Data
    bit_buffer_append_bytes(instance->tx_buffer, data, SLIX_BLOCK_SIZE);

    // Send request
    Iso15693_3Error error = iso15693_3_poller_send_frame(
        instance->iso15693_3_poller, instance->tx_buffer, instance->rx_buffer, SLIX_POLLER_MAX_FWT);

    if(error == Iso15693_3ErrorNone) {
        // Check for card-level error response
        iso15693_3_error_response_parse(&error, instance->rx_buffer);
    }

    return slix_poller_process_error(error);
}
