#include "slix.h"
#include <furi/furi.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/helpers/iso13239_crc.h>
#include <string.h>

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
