#pragma once

#include <stdint.h>
#include <toolbox/bit_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SLIX_UID_LEN 8

typedef struct {
  uint8_t uid[SLIX_UID_LEN];
} SlixData;

void slix_build_inventory_request(BitBuffer *buf);

SlixData *slix_alloc(void);

void slix_free(SlixData *data);

void slix_reset(SlixData *data);

void slix_copy(SlixData *dest, const SlixData *src);

#ifdef __cplusplus
}
#endif