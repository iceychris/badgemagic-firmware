#include "utils.h"

#include "../../data.h"
#include "../../power.h"
#include "../../leddrv.h"

static const uint16_t ServiceUUID = 0xFEE0;
static const gattAttrType_t service = {2, (uint8_t *)&ServiceUUID};

static const uint16_t RxCharUUID = 0xFEE1;
static uint8 RxCharProps = GATT_PROP_WRITE;
static uint8 RxCharVal[16];

extern volatile uint16_t fb[LED_COLS];

static gattAttribute_t attr_table[] = {
	ATTR_DECLAR(primaryServiceUUID, 2, GATT_PERMIT_READ, &service),

	CHAR_DECLAR(&RxCharProps),
	CHAR_VAL_DECLAR(&RxCharUUID, 2, GATT_PERMIT_WRITE, RxCharVal),
};

extern void still(bm_t *bm, uint16_t *fb, int frame);

void write_error_code(uint16_t c, uint16_t data_len, uint16_t len, uint8_t code)
{
	// write last columns
	fb[LED_COLS - 4] = c;
	fb[LED_COLS - 3] = data_len;
	fb[LED_COLS - 2] = len;
	fb[LED_COLS - 1] = code;
}

// Add structure to hold transfer state
typedef struct {
    uint16_t counter;
    uint16_t data_len;
    uint16_t total_size;
    uint8_t *data;
} transfer_state_t;

// Make transfer state per connection
static transfer_state_t transfer_state = {0};

/**
 * New Protocol:
 * - 1 byte sequence number (valid values are 0-5)
 * - 16 bytes payload to write at offset into fb
 */
static bStatus_t receive(uint8_t *val, uint16_t len)
{
    if (len != 17) {
        return ATT_ERR_INSUFFICIENT_KEY_SIZE;
    }

    uint8_t sequence_number = val[0];
    if (sequence_number > 5) {
        return ATT_ERR_INSUFFICIENT_KEY_SIZE;
    }

    uint16_t offset = sequence_number * 16;
    uint16_t bytes_to_write = 16;
    if (sequence_number == 5) {
        bytes_to_write = 8;
    }

    memcpy(fb + offset / 2, val + 1, bytes_to_write);
    return SUCCESS;
}

static bStatus_t write_handler(uint16 connHandle, gattAttribute_t *pAttr,
				uint8 *pValue, uint16 len, uint16 offset, uint8 method)
{
	if(gattPermitAuthorWrite(pAttr->permissions)) {
		return ATT_ERR_INSUFFICIENT_AUTHOR;
	}

	uint16_t uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);
	if(uuid == RxCharUUID) {
		return receive(pValue, len);
	}
	return ATT_ERR_ATTR_NOT_FOUND;
}

gattServiceCBs_t service_handlers = {
	NULL,
	write_handler,
	NULL
};

int legacy_registerService()
{
	uint8 status = SUCCESS;

	status = GATTServApp_RegisterService(attr_table,
				GATT_NUM_ATTRS(attr_table),
				GATT_MAX_ENCRYPT_KEY_SIZE,
				&service_handlers);
	return (status);
}