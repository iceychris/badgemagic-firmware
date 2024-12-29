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

extern int ani_fixed(bm_t *bm, uint16_t *fb);
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
 * We expect to always receive packets that are each 16 bytes long.
 */
static bStatus_t receive(uint8_t *val, uint16_t len)
{
    if (len != LEGACY_TRANSFER_WIDTH) {
        return ATT_ERR_INVALID_VALUE_SIZE;
    }

    // First packet
    if (transfer_state.counter == 0) {
        if (memcmp(val, "wang\0\0", 6)) {
            return ATT_ERR_INVALID_VALUE;
        }
        
        // Initialize new transfer
        transfer_state.data = malloc(sizeof(data_legacy_t));
        if (!transfer_state.data) {
            return ATT_ERR_INSUFFICIENT_RESOURCES;
        }
    }

    // Copy data safely
    if (transfer_state.data) {
        memcpy(transfer_state.data + transfer_state.counter * len, val, len);
    } else {
        return ATT_ERR_INSUFFICIENT_RESOURCES;
    }

    // Second packet - calculate total size
    if (transfer_state.counter == 1) {
        data_legacy_t *d = (data_legacy_t *)transfer_state.data;
		uint16_t sum_in_headers = bigendian16_sum(d->sizes, 8);
		if (sum_in_headers > 16) {
			return ATT_ERR_INVALID_VALUE;
		}
		transfer_state.total_size = sum_in_headers;
        transfer_state.data_len = LEGACY_HEADER_SIZE + LED_ROWS * transfer_state.total_size;
        
        // Reallocate buffer for full data
        uint8_t *new_data = realloc(transfer_state.data, transfer_state.data_len);
        if (!new_data) {
            free(transfer_state.data);
            transfer_state.data = NULL;
            transfer_state.counter = 0;
            return ATT_ERR_INSUFFICIENT_RESOURCES;
        }
        transfer_state.data = new_data;
    }

    // Check if transfer complete
    if (transfer_state.counter > 2 && 
        ((transfer_state.counter + 1) * LEGACY_TRANSFER_WIDTH) >= transfer_state.data_len) {
        
        // Process received data
        bm_t *bm = chunk2newbm(transfer_state.data + LEGACY_HEADER_SIZE, 
                              transfer_state.data_len - LEGACY_HEADER_SIZE);
        if (bm) {
            still(bm, fb, 0);
            free(bm->buf);
            free(bm);
        }

        // Clean up
        free(transfer_state.data);
        transfer_state.data = NULL;
        transfer_state.counter = 0;
        transfer_state.data_len = 0;
        transfer_state.total_size = 0;
        
        return SUCCESS;
    }

    transfer_state.counter++;
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