#pragma once

#include <stdint.h>
#include <toolbox/bit_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SLIX_UID_LEN 8

typedef struct {
    uint8_t uid[SLIX_UID_LEN];
    // Other SLIX-specific data can be added here
} SlixData;

void slix_build_inventory_request(BitBuffer* buf);

/**
 * @brief Build a Write Single Block request.
 * @param buf The buffer to build the request in.
 * @param uid The UID of the card to write to.
 * @param block_num The block number to write.
 * @param data A pointer to the data to be written (must be 4 bytes).
 */
void slix_build_write_block_request(
    BitBuffer* buf,
    const uint8_t* uid,
    uint8_t block_num,
    const uint8_t* data);

SlixData* slix_alloc(void);

void slix_free(SlixData* data);

void slix_reset(SlixData* data);

void slix_copy(SlixData* dest, const SlixData* src);

#ifdef __cplusplus
}
#endif
