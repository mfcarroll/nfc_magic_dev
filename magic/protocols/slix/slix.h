#pragma once

#include <stdint.h>
#include <toolbox/bit_buffer.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SLIX_UID_LEN        8
#define SLIX_BLOCK_SIZE     4
#define SLIX_SIGNATURE_SIZE 32

#define SLIX_NXP_MANUFACTURER_CODE          (0x04U)
#define SLIX_CMD_GET_NXP_SYSTEM_INFORMATION (0xABU)
#define SLIX_CMD_READ_SIGNATURE             (0xBDU)

typedef enum {
    SlixTypeSlix,
    SlixTypeSlixS,
    SlixTypeSlixL,
    SlixTypeSlix2,
    SlixTypeCount,
    SlixTypeUnknown,
} SlixType;

typedef uint8_t SlixSignature[SLIX_SIGNATURE_SIZE];

typedef struct {
    uint8_t protection_pointer;
    uint8_t protection_condition;
    bool lock_eas;
    bool lock_ppl;
    uint32_t feature_flags;
} SlixSystemInfo;

typedef struct {
    uint8_t uid[SLIX_UID_LEN];
    SlixType type;
    Iso15693_3Settings iso15693_3_settings;
    Iso15693_3SystemInfo iso15693_3_info;
    SlixSystemInfo slix_info;
    SlixSignature signature;
    bool signature_read;
} SlixData;

void slix_build_inventory_request(BitBuffer* buf);

SlixData* slix_alloc(void);

void slix_free(SlixData* data);

void slix_reset(SlixData* data);

void slix_copy(SlixData* dest, const SlixData* src);

SlixType slix_get_type(const SlixData* data);

#ifdef __cplusplus
}
#endif
