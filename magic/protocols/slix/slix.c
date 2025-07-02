#include "slix.h"
#include <furi/furi.h>
#include <lib/nfc/protocols/iso15693_3/iso15693_3.h>
#include <lib/nfc/helpers/iso15693_crc.h>
#include <string.h>

void slix_build_inventory_request(BitBuffer* buf) {
    bit_buffer_reset(buf);
    uint8_t flags = Iso15693_3FlagInventory | Iso15693_3FlagSlot1 | Iso15693_3FlagDataRateHigh;
    bit_buffer_append_byte(buf, flags);
    bit_buffer_append_byte(buf, Iso15693_3CommandInventory);
    // No AFI, no mask
    bit_buffer_append_byte(buf, 0x00);
    iso15693_crc_append(buf);
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
