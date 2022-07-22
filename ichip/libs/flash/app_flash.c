#include <zephyr.h>
#include <device.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <sys/crc.h>
#include <sys/byteorder.h>
#include <sys/util.h>
#include <string.h>
#include "app_flash.h"

/**@brief Get flash info
 *
 * @details Info structure: bank start address[4], page
 * count of the bank[4], the first blank page[4].
 * The first blank page is used to implement resume
 * from the break point.
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_si_slotinfo(uint8_t* p_data)
{
	int rc;
	const struct flash_area* fa;
	uint32_t bank_addr;
	uint32_t page_count;
	uint32_t first_blank;

	uint32_t p_read;
	bool  found;

	rc = flash_area_open(APP_FLASH_BANK_ID, &fa);
	if (rc) {
		return rc;
	}

	bank_addr = fa->fa_off;
	page_count = fa->fa_size / 0x1000;

	flash_area_close(fa);

	found = false;
	p_read = bank_addr;
	while (p_read < fa->fa_size) {
		for (uint32_t i = 0; i < 4096; i++) {
			if (((uint8_t*)p_read)[i] != 0xFF) {
				found = true;
				break;
			}
		}

		if (!found) {
			break;
		}

		p_read += 4096;
	}

	first_blank = MAX(bank_addr, (p_read - 4096));

	sys_put_le32(bank_addr, &p_data[0]);
	sys_put_le32(page_count, &p_data[4]);
	sys_put_le32(first_blank, &p_data[8]);

	return rc;
}

/**@brief Read flash data
 *
 * @param[in] offset: offset from the bank start
 * @param[out] p_data: pointer of data
 * @param[in] length: length of data to be read
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_read(uint32_t offset, uint8_t* p_data, uint32_t length)
{
	int rc;
	const struct flash_area* fa;

	rc = flash_area_open(APP_FLASH_BANK_ID, &fa);
	if (rc) {
		return rc;
	}

    // TODO: here should check the boundary of flash

	rc = flash_area_read(fa, offset, p_data, length);
	if (rc) {
		return rc;
	}

	flash_area_close(fa);

	return rc;
}

/**@brief Read flash data with Bank ID
 *
 * @param[in] offset: offset from the bank start
 * @param[out] p_data: pointer of data
 * @param[in] length: length of data to be read
 * @param[in] FLASH_ID: ID of Flash to be read
 * 
 * @return 0: success
 * @return neg: error
 */
int app_flash_read_id(uint32_t offset, uint8_t* p_data, uint32_t length,uint8_t FLASH_ID)
{
	int rc;
	const struct flash_area* fa;

	rc = flash_area_open(FLASH_ID, &fa);
	if (rc) {
		return rc;
	}

    // TODO: here should check the boundary of flash

	rc = flash_area_read(fa, offset, p_data, length);
	if (rc) {
		return rc;
	}

	flash_area_close(fa);

	return rc;
}

/**@brief Get Available Space for MCUBOOTSECONDARY Slot in Bytes
 *
 * @return >= 0: success
 * @return neg: error
 */
static uint32_t app_flash_get_availablesize(){

	int rc;
	const struct flash_area* fa;
	uint32_t size;

	rc = flash_area_open(APP_FLASH_BANK_ID, &fa);
	if (rc) {
		return rc;
	}

	size = fa->fa_size;

	flash_area_close(fa);

	return size;
}

/**@brief Retrieves Current Firmware Version
 *
 *
 * @param[in] minor_fw: pointer to byte for holding minor version
 * @param[in] major_fw: pointer to byte for holding major version
 *
 * @return 0: success
 * @return neg: error
 */
static int app_flash_fwnum(uint8_t* minor_fw, uint8_t* major_fw)
{
	int rc;
	
	uint8_t Data_bata[2];

	rc = app_flash_read_id(20,Data_bata,2,FLASH_AREA_ID(image_0));//4+4+2+2+4+4 20 21 MCUBOOT Image Header Layout

	*minor_fw=Data_bata[0];
	*major_fw=Data_bata[1];

	return rc;
}

/**@brief Retrieves Current Firmware Version and Available Space in the Secondary Image Slot
 *
 * @details buffer has to be at least 6 Bytes Long 0th Major 1st Minor 2-6 Available Space
 *
 * @param[in] buffer: buffer for read data
 * @param[in] bufferlength: length of buffer
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_get_DevInf(uint8_t* buffer,uint16_t bufferlength){
	int rc = 0;
	
	if(bufferlength < 6){ //6 -> major minor version = 2 Byte + 4 byte for Available Space
		return -1;
	}

	//Retrieve FW Version
	rc = app_flash_fwnum(&buffer[0],&buffer[1]);
	sys_put_le32(app_flash_get_availablesize(),&buffer[2]);

	return rc;
}


/**@brief Write flash data
 *
 * @details If length is not word aligned, it will fill
 * 0xFF as end padding
 *
 * @param[in] offset: offset from the bank start
 * @param[in] p_data: pointer of data
 * @param[in] length: length of data to be read
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_write(uint32_t offset, uint8_t* p_data, uint32_t length)
{
	int rc;
	const struct flash_area* fa;
	uint8_t  p_last_word[4] = {0xFF, 0xFF, 0xFF, 0xFF};
	uint32_t word_align_len;

	rc = flash_area_open(APP_FLASH_BANK_ID, &fa);
	if (rc) {
		return rc;
	}

	if (length % 4 != 0) {
		word_align_len = length / 4 * 4;
	}
	else {
		word_align_len = length;
	}

	rc = flash_area_write(fa, offset, p_data, word_align_len);
	if (rc != 0) {
		return rc;
	}

	if (word_align_len < length) {
		memcpy(p_last_word, &p_data[word_align_len], (length - word_align_len));
		rc = flash_area_write(fa, offset + word_align_len, p_last_word, 4);
		if (rc) {
			return rc;
		}
	}

	flash_area_close(fa);

	return rc;
}

/**@brief Erase flash pages
 *
 * @param[in] offset: offset from the bank start
 * @param[in] count: page count to be erased
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_erase_page(uint32_t offset, uint32_t count)
{
	int rc;
	const struct flash_area* fa;
	uint32_t byte_len;

	rc = flash_area_open(APP_FLASH_BANK_ID, &fa);
	if (rc) {
		return rc;
	}

	byte_len = MIN(count * 0x1000, fa->fa_size);

	rc = flash_area_erase(fa, offset, byte_len);

	flash_area_close(fa);

	return rc;
}

/**@brief Erase flash pages from the bank end
 *
 * @param[in] offset: offset from the bank start
 * @param[in] count: page count to be erased
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_erase_from_end(uint32_t count)
{
	int rc;
	const struct flash_area* fa;
	uint32_t byte_len;
    uint32_t offset;

	rc = flash_area_open(APP_FLASH_BANK_ID, &fa);
	if (rc) {
		return rc;
	}

	byte_len = MIN(count * 0x1000, fa->fa_size);
    offset = fa->fa_size - byte_len;

	rc = flash_area_erase(fa, offset, byte_len);

	flash_area_close(fa);

	return rc;
}

/**@brief Get crc value of flash data
 *
 * @param[in] offset: offset from the bank start
 * @param[in] length: length of data to be read
 * @param[out] crc32: pointer of crc value
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_crc(uint32_t offset, uint32_t length, uint32_t* crc32)
{
	int rc;
	const struct flash_area* fa;
	uint8_t *bank_addr;
	uint32_t step = 1024;
	uint32_t crc_val = 0;
	uint32_t i;

	rc = flash_area_open(APP_FLASH_BANK_ID, &fa);
	if (rc) {
		return rc;
	}

	bank_addr = (uint8_t *)(fa->fa_off);

	if (length <= step) {
		crc_val = crc32_ieee(bank_addr, length);
	}
	else {
		crc_val = 0;

		for (i = 0; i < length / step; i++) {
			crc_val = crc32_ieee_update(crc_val, bank_addr + i * step, step);
		}
		if (length % step != 0) {
			crc_val = crc32_ieee_update(crc_val, bank_addr + i * step, length % step);
		}
	}
	*crc32 = crc_val;

	flash_area_close(fa);

	return rc;
}

