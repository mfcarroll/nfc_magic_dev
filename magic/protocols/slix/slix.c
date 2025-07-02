#include "slix.h"
#include <furi/furi.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/helpers/iso13239_crc.h>
#include <string.h>

// From NXP AN10787 ICODE UID format
#define SLIX_TYPE_SLIX_SLIX2      (0x01U)
#define SLIX_TYPE_SLIX_S          (0x02U)
#define SLIX_TYPE_SLIX_L          (0x03U)
#define SLIX_TYPE_INDICATOR_SLIX2 (0x01U)
#define SLIX_TYPE_INDICATOR_SLIX  (0x02U)

void slix_build_inventory_request(BitBuffer* buf) {
    bit_buffer_reset(buf);
    uint8_t flags = ISO15693_3_REQ_FLAG_INVENTORY_T5 | ISO15693_3_REQ_FLAG_T5_N_SLOTS_1 |
                    ISO15693_3_REQ_FLAG_DATA_RATE_HI;
    bit_buffer_append_byte(buf, flags);
    bit_buffer_append_byte(buf, ISO15693_3_CMD_INVENTORY);
    // No AFI, no mask
    bit_buffer_append_byte(buf, 0x00);
    iso13239_crc_append(Iso13239CrcTypeDefault, buf);
}

void slix_build_write_block_request(
    BitBuffer* buf,
    const uint8_t* uid,
    uint8_t block_num,
    const uint8_t* data) {
    bit_buffer_reset(buf);
    // Addressed, High data rate
    uint8_t flags = ISO15693_3_REQ_FLAG_T4_ADDRESSED | ISO15693_3_REQ_FLAG_DATA_RATE_HI;
    bit_buffer_append_byte(buf, flags);
    bit_buffer_append_byte(buf, ISO15693_3_CMD_WRITE_BLOCK);
    bit_buffer_append_bytes(buf, uid, SLIX_UID_LEN);
    bit_buffer_append_byte(buf, block_num);

    // Note: ISO15693 block size can vary. SLIX-S and SLIX2 are 4 bytes.
    // Assuming 4-byte blocks for this implementation.
    bit_buffer_append_bytes(buf, data, 4);

    iso13239_crc_append(Iso13239CrcTypeDefault, buf);
}

SlixData* slix_alloc() {
    SlixData* data = malloc(sizeof(SlixData));
    return data;
}

void slix_free(SlixData* data) {
    furi_assert(data);
    free(data);
}

void slix_reset(SlixData* data) {
    furi_assert(data);
    memset(data, 0, sizeof(SlixData));
}

void slix_copy(SlixData* dest, const SlixData* src) {
    furi_assert(dest);
    furi_assert(src);
    memcpy(dest, src, sizeof(SlixData));
}

SlixType slix_get_type(const SlixData* data) {
    furi_assert(data);

    SlixType type = SlixTypeUnknown;

    // UID is LSB-first from inventory response.
    // Manufacturer code is the 7th byte (index 6).
    if(data->uid[6] == SLIX_NXP_MANUFACTURER_CODE) {
        // ICODE type is the 6th byte (index 5).
        uint8_t icode_type = data->uid[5];
        if(icode_type == SLIX_TYPE_SLIX_S) {
            type = SlixTypeSlixS;
        } else if(icode_type == SLIX_TYPE_SLIX_L) {
            type = SlixTypeSlixL;
        } else if(icode_type == SLIX_TYPE_SLIX_SLIX2) {
            // Type indicator is bits 3 and 4 of the 5th byte (index 4).
            uint8_t type_indicator = (data->uid[4] >> 3) & 0x03;
            if(type_indicator == SLIX_TYPE_INDICATOR_SLIX) {
                type = SlixTypeSlix;
            } else if(type_indicator == SLIX_TYPE_INDICATOR_SLIX2) {
                type = SlixTypeSlix2;
            }
        }
    }

    return type;
}
