#include "slix_poller_i.h"
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/helpers/iso13239_crc.h>
#include <bit_lib/bit_lib.h>

#include <furi/furi.h>

#define TAG "SlixPoller"

static SlixPollerError slix_poller_process_nfc_error(NfcError error) {
    SlixPollerError ret = SlixPollerErrorNone;

    switch(error) {
    case NfcErrorNone:
        ret = SlixPollerErrorNone;
        break;
    case NfcErrorTimeout:
        ret = SlixPollerErrorTimeout;
        break;
    default:
        // Covers other NfcError types like communication errors
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
    uint8_t flags = ISO15693_3_REQ_FLAG_SUBCARRIER_1 | ISO15693_3_REQ_FLAG_DATA_RATE_HI |
                    ISO15693_3_REQ_FLAG_T4_ADDRESSED;
    bit_buffer_append_byte(instance->tx_buffer, flags);
    // Command
    bit_buffer_append_byte(instance->tx_buffer, ISO15693_3_CMD_WRITE_BLOCK);
    // UID
    bit_buffer_append_bytes(instance->tx_buffer, instance->slix_data->uid, SLIX_UID_LEN);
    // Block number
    bit_buffer_append_byte(instance->tx_buffer, block_num);
    // Data
    bit_buffer_append_bytes(instance->tx_buffer, data, SLIX_BLOCK_SIZE);
    // Append CRC
    iso13239_crc_append(Iso13239CrcTypeDefault, instance->tx_buffer);

    // Send request
    NfcError error = nfc_poller_trx(
        instance->nfc, instance->tx_buffer, instance->rx_buffer, SLIX_POLLER_MAX_FWT);

    SlixPollerError slix_error = slix_poller_process_nfc_error(error);

    if(slix_error == SlixPollerErrorNone) {
        // Check for card-level error response
        if(iso13239_crc_check(Iso13239CrcTypeDefault, instance->rx_buffer)) {
            iso13239_crc_trim(instance->rx_buffer);
            uint8_t resp_flags = bit_buffer_get_byte(instance->rx_buffer, 0);
            if(resp_flags & ISO15693_3_RESP_FLAG_ERROR) {
                slix_error = SlixPollerErrorProtocol;
            }
        } else {
            slix_error = SlixPollerErrorProtocol;
        }
    }

    return slix_error;
}

SlixPollerError slix_poller_get_nxp_system_info(SlixPoller* instance) {
    furi_assert(instance);
    bit_buffer_reset(instance->tx_buffer);

    // Flags: Addressed, High data rate
    uint8_t flags = ISO15693_3_REQ_FLAG_SUBCARRIER_1 | ISO15693_3_REQ_FLAG_DATA_RATE_HI |
                    ISO15693_3_REQ_FLAG_T4_ADDRESSED;
    bit_buffer_append_byte(instance->tx_buffer, flags);
    // Command
    bit_buffer_append_byte(instance->tx_buffer, SLIX_CMD_GET_NXP_SYSTEM_INFORMATION);
    // Manufacturer code for NXP
    bit_buffer_append_byte(instance->tx_buffer, SLIX_NXP_MANUFACTURER_CODE);
    // UID
    bit_buffer_append_bytes(instance->tx_buffer, instance->slix_data->uid, SLIX_UID_LEN);
    // Append CRC
    iso13239_crc_append(Iso13239CrcTypeDefault, instance->tx_buffer);

    // Send request
    NfcError error = nfc_poller_trx(
        instance->nfc, instance->tx_buffer, instance->rx_buffer, SLIX_POLLER_MAX_FWT);

    SlixPollerError slix_error = slix_poller_process_nfc_error(error);

    if(slix_error == SlixPollerErrorNone) {
        if(iso13239_crc_check(Iso13239CrcTypeDefault, instance->rx_buffer)) {
            iso13239_crc_trim(instance->rx_buffer);

            // Response format: flags(1) + data(8)
            if(bit_buffer_get_size_bytes(instance->rx_buffer) == 9) {
                const uint8_t* resp_data = bit_buffer_get_data(instance->rx_buffer);
                if(resp_data[0] & ISO15693_3_RESP_FLAG_ERROR) {
                    slix_error = SlixPollerErrorProtocol;
                } else {
                    // Parse response
                    instance->slix_data->slix_info.protection_pointer = resp_data[1];
                    instance->slix_data->slix_info.protection_condition = resp_data[2];

                    uint8_t lock_bits = resp_data[3];
                    instance->slix_data->iso15693_3_settings.lock_bits.afi =
                        (lock_bits & (1U << 0));
                    instance->slix_data->slix_info.lock_eas = (lock_bits & (1U << 1));
                    instance->slix_data->iso15693_3_settings.lock_bits.dsfid =
                        (lock_bits & (1U << 2));
                    instance->slix_data->slix_info.lock_ppl = (lock_bits & (1U << 3));

                    instance->slix_data->slix_info.feature_flags =
                        bit_lib_bytes_to_num_le(&resp_data[4], 4);
                }
            } else {
                slix_error = SlixPollerErrorProtocol;
            }
        } else {
            slix_error = SlixPollerErrorProtocol;
        }
    }

    return slix_error;
}

SlixPollerError slix_poller_get_system_info(SlixPoller* instance) {
    furi_assert(instance);
    bit_buffer_reset(instance->tx_buffer);

    // Flags: High data rate. Unaddressed command.
    uint8_t flags = ISO15693_3_REQ_FLAG_SUBCARRIER_1 | ISO15693_3_REQ_FLAG_DATA_RATE_HI;
    bit_buffer_append_byte(instance->tx_buffer, flags);
    // Command
    bit_buffer_append_byte(instance->tx_buffer, ISO15693_3_CMD_GET_SYS_INFO);
    // Append CRC
    iso13239_crc_append(Iso13239CrcTypeDefault, instance->tx_buffer);

    // Send request
    NfcError error = nfc_poller_trx(
        instance->nfc, instance->tx_buffer, instance->rx_buffer, SLIX_POLLER_MAX_FWT);

    SlixPollerError slix_error = slix_poller_process_nfc_error(error);

    if(slix_error == SlixPollerErrorNone) {
        if(iso13239_crc_check(Iso13239CrcTypeDefault, instance->rx_buffer)) {
            iso13239_crc_trim(instance->rx_buffer);

            // Response format: flags(1) + info_flags(1) + uid(8) + optional_data(...)
            const size_t min_resp_size = 1 + 1 + SLIX_UID_LEN;
            if(bit_buffer_get_size_bytes(instance->rx_buffer) >= min_resp_size) {
                const uint8_t* resp_data = bit_buffer_get_data(instance->rx_buffer);
                if(resp_data[0] & ISO15693_3_RESP_FLAG_ERROR) {
                    slix_error = SlixPollerErrorProtocol;
                } else {
                    // Parse response
                    Iso15693_3SystemInfo* info = &instance->slix_data->iso15693_3_info;
                    info->flags = resp_data[1];
                    const uint8_t* extra_data = &resp_data[1 + 1 + SLIX_UID_LEN];

                    if(info->flags & ISO15693_3_SYSINFO_FLAG_DSFID) {
                        info->dsfid = *extra_data++;
                    }
                    if(info->flags & ISO15693_3_SYSINFO_FLAG_AFI) {
                        info->afi = *extra_data++;
                    }
                    if(info->flags & ISO15693_3_SYSINFO_FLAG_MEMORY) {
                        info->block_count = *extra_data++ + 1;
                        info->block_size = (*extra_data++ & 0x1F) + 1;
                    }
                    if(info->flags & ISO15693_3_SYSINFO_FLAG_IC_REF) {
                        info->ic_ref = *extra_data++;
                    }
                }
            } else {
                slix_error = SlixPollerErrorProtocol;
            }
        } else {
            slix_error = SlixPollerErrorProtocol;
        }
    }

    return slix_error;
}

SlixPollerError slix_poller_select(SlixPoller* instance) {
    furi_assert(instance);
    bit_buffer_reset(instance->tx_buffer);

    // Flags: Addressed, High data rate
    uint8_t flags = ISO15693_3_REQ_FLAG_SUBCARRIER_1 | ISO15693_3_REQ_FLAG_DATA_RATE_HI |
                    ISO15693_3_REQ_FLAG_T4_ADDRESSED;
    bit_buffer_append_byte(instance->tx_buffer, flags);
    // Command
    bit_buffer_append_byte(instance->tx_buffer, ISO15693_3_CMD_SELECT);
    // UID
    bit_buffer_append_bytes(instance->tx_buffer, instance->slix_data->uid, SLIX_UID_LEN);
    // Append CRC
    iso13239_crc_append(Iso13239CrcTypeDefault, instance->tx_buffer);

    // Send request
    NfcError error = nfc_poller_trx(
        instance->nfc, instance->tx_buffer, instance->rx_buffer, SLIX_POLLER_MAX_FWT);

    SlixPollerError slix_error = slix_poller_process_nfc_error(error);

    if(slix_error == SlixPollerErrorNone) {
        if(iso13239_crc_check(Iso13239CrcTypeDefault, instance->rx_buffer)) {
            iso13239_crc_trim(instance->rx_buffer);
            // Response should be 1 byte (flags)
            if(bit_buffer_get_size_bytes(instance->rx_buffer) == 1) {
                uint8_t resp_flags = bit_buffer_get_byte(instance->rx_buffer, 0);
                if(resp_flags & ISO15693_3_RESP_FLAG_ERROR) {
                    slix_error = SlixPollerErrorProtocol;
                }
            } else {
                slix_error = SlixPollerErrorProtocol;
            }
        } else {
            slix_error = SlixPollerErrorProtocol;
        }
    }

    return slix_error;
}

SlixPollerError slix_poller_read_signature(SlixPoller* instance) {
    furi_assert(instance);
    bit_buffer_reset(instance->tx_buffer);

    // Flags: Addressed, High data rate
    uint8_t flags = ISO15693_3_REQ_FLAG_SUBCARRIER_1 | ISO15693_3_REQ_FLAG_DATA_RATE_HI |
                    ISO15693_3_REQ_FLAG_T4_ADDRESSED;
    bit_buffer_append_byte(instance->tx_buffer, flags);
    // Command
    bit_buffer_append_byte(instance->tx_buffer, SLIX_CMD_READ_SIGNATURE);
    // Manufacturer code for NXP
    bit_buffer_append_byte(instance->tx_buffer, SLIX_NXP_MANUFACTURER_CODE);
    // UID
    bit_buffer_append_bytes(instance->tx_buffer, instance->slix_data->uid, SLIX_UID_LEN);
    // Append CRC
    iso13239_crc_append(Iso13239CrcTypeDefault, instance->tx_buffer);

    // Send request (Signature read needs a longer timeout)
    NfcError error = nfc_poller_trx(
        instance->nfc, instance->tx_buffer, instance->rx_buffer, SLIX_POLLER_MAX_FWT * 2);

    SlixPollerError slix_error = slix_poller_process_nfc_error(error);

    if(slix_error == SlixPollerErrorNone) {
        if(iso13239_crc_check(Iso13239CrcTypeDefault, instance->rx_buffer)) {
            iso13239_crc_trim(instance->rx_buffer);

            // Response format: flags(1) + signature(32)
            if(bit_buffer_get_size_bytes(instance->rx_buffer) == 33) {
                const uint8_t* resp_data = bit_buffer_get_data(instance->rx_buffer);
                if(resp_data[0] & ISO15693_3_RESP_FLAG_ERROR) {
                    slix_error = SlixPollerErrorProtocol;
                } else {
                    // Parse response
                    memcpy(instance->slix_data->signature, &resp_data[1], SLIX_SIGNATURE_SIZE);
                }
            } else {
                slix_error = SlixPollerErrorProtocol;
            }
        } else {
            slix_error = SlixPollerErrorProtocol;
        }
    }

    return slix_error;
}

SlixPollerError slix_poller_inventory(SlixPoller* instance) {
    furi_assert(instance);
    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);

    // Build and send INVENTORY command
    slix_build_inventory_request(instance->tx_buffer);

    NfcError error = nfc_poller_trx(
        instance->nfc, instance->tx_buffer, instance->rx_buffer, ISO15693_3_FDT_POLL_FC * 2);

    SlixPollerError slix_error = slix_poller_process_nfc_error(error);
    if(slix_error != SlixPollerErrorNone) {
        return slix_error;
    }

    // A valid INVENTORY response is 12 bytes (flags, dsfid, uid, crc)
    if(bit_buffer_get_size_bytes(instance->rx_buffer) >= 12) {
        uint8_t flags = bit_buffer_get_byte(instance->rx_buffer, 0);
        if(!(flags & ISO15693_3_RESP_FLAG_ERROR)) {
            // Verify that the UID from inventory matches the one from the initial scan
            const uint8_t* uid_lsb = &bit_buffer_get_data(instance->rx_buffer)[2];
            if(memcmp(instance->slix_data->uid, uid_lsb, SLIX_UID_LEN) == 0) {
                return SlixPollerErrorNone;
            } else {
                FURI_LOG_W(TAG, "Inventory UID mismatch");
                return SlixPollerErrorProtocol;
            }
        }
    }

    return SlixPollerErrorProtocol;
}
