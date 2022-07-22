#ifndef APP_FLASH_H__
#define APP_FLASH_H__

#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PM_MCUBOOT_SECONDARY_ID
#define APP_FLASH_BANK_ID			PM_MCUBOOT_SECONDARY_ID //Look it up under MCUBOOT images
#else
#define APP_FLASH_BANK_ID			FLASH_AREA_ID(image_1)	// 3: flash0 -> "image-1"
#endif // PM_MCUBOOT_SECONDARY_ID

/**@brief Get flash info for Secondary Image Slot
 *
 * @details Info structure: bank start address[4], page
 * count of the bank[4], the first blank page[4].
 * The first blank page is used to implement resume
 * from the break point.
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_si_slotinfo(uint8_t* p_data);

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
int app_flash_get_DevInf(uint8_t* buffer,uint16_t bufferlength);

/**@brief Read flash data
 *
 * @param[in] offset: offset from the bank start
 * @param[out] p_data: pointer of data
 * @param[in] length: length of data to be read
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_read(uint32_t offset, uint8_t* p_data, uint32_t length);

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
int app_flash_read_id(uint32_t offset, uint8_t* p_data, uint32_t length,uint8_t FLASH_ID);

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
int app_flash_write(uint32_t offset, uint8_t* p_data, uint32_t length);

/**@brief Erase flash pages
 *
 * @param[in] offset: offset from the bank start
 * @param[in] count: page count to be erased
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_erase_page(uint32_t offset, uint32_t count);

/**@brief Erase flash pages from the bank end
 *
 * @param[in] offset: offset from the bank start
 * @param[in] count: page count to be erased
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_erase_from_end(uint32_t count);

/**@brief Get crc value of flash data
 *
 * @param[in] offset: offset from the bank start
 * @param[in] length: length of data to be read
 * @param[out] crc32: pointer of crc value
 *
 * @return 0: success
 * @return neg: error
 */
int app_flash_crc(uint32_t offset, uint32_t length, uint32_t* crc32);

#ifdef __cplusplus
}
#endif

#endif /* APP_FLASH_H__ */
