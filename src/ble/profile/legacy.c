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

static bStatus_t receive(uint8_t *val, uint16_t len)
{
	static uint16_t c, data_len, n;
	static uint8_t *data;
	if (len != LEGACY_TRANSFER_WIDTH) {
		// write_error_code(c, data_len, len, ATT_ERR_INVALID_VALUE_SIZE);
		return ATT_ERR_INVALID_VALUE_SIZE;
	}

	if (c == 0) {
		if (memcmp(val, "wang\0\0", 6)) {
			// write_error_code(c, data_len, len, ATT_ERR_INVALID_VALUE);
			return ATT_ERR_INVALID_VALUE;
		} else {
			data = malloc(sizeof(data_legacy_t));
		}
	}

	memcpy(data + c * len, val, len);

	if (c == 1) {
		data_legacy_t *d = (data_legacy_t *)data;
		n = bigendian16_sum(d->sizes, 8);
		// fb[LED_COLS - 6] = d->sizes[0];
		// fb[LED_COLS - 5] = n;
		// DelayMs(65000u);
		data_len = LEGACY_HEADER_SIZE + LED_ROWS * n;
		data = realloc(data, data_len);
	}

	if (c > 2 && ((c+1) * LEGACY_TRANSFER_WIDTH) >= data_len) {
		// uint32_t r = data_flatSave(data, data_len);
		// reset_jump();

		bm_t *bm = chunk2newbm(data + LEGACY_HEADER_SIZE, data_len - LEGACY_HEADER_SIZE);
		bm->width = LED_COLS;

		still(bm, fb, 0);

		// (debug) write raw data directly to fb
		// fb[0] = data_len;
		// for(uint16_t i = 0; i < 43; i++) {
		// 	if (i < data_len) {
		// 		fb[i + 1] = data[i];
		// 	} else {
		// 		fb[i + 1] = 0;
		// 	}
		// }

		free(bm->buf);
		free(bm);

		free(data);
		data_len = 0;
		c = 0;
		return SUCCESS;
	}

	c++;
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