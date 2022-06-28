#ifndef APP_FLASH_H__
#define APP_FLASH_H__

#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PM_MCUBOOT_SECONDARY_ID
#define APP_FLASH_BANK_ID			PM_MCUBOOT_SECONDARY_ID
#else
#define APP_FLASH_BANK_ID			FLASH_AREA_ID(image_1)		// 3: flash0 -> "image-1"
#endif // PM_MCUBOOT_SECONDARY_ID

//Reads Bank Address Page Count and First Blank Space
int app_flash_si_slotinfo(uint8_t* p_data);

//Retrieves Fw Version of the Device and Available Space in the Upgradeable Slot PM_MCUBOOOT_SECONDARY
int app_flash_get_DevInf(uint8_t* buffer,uint16_t bufferlength);

//Read 
int app_flash_read(uint32_t offset, uint8_t* p_data, uint32_t length);
int app_flash_read_id(uint32_t offset, uint8_t* p_data, uint32_t length,uint8_t FLASH_ID);

//Writes to Secondary Flash Area
int app_flash_write(uint32_t offset, uint8_t* p_data, uint32_t length);

//Not Really Used Currently
int app_flash_erase_page(uint32_t offset, uint32_t count);
int app_flash_erase_from_end(uint32_t count);
int app_flash_crc(uint32_t offset, uint32_t length, uint32_t* crc32);

#ifdef __cplusplus
}
#endif

#endif /* APP_FLASH_H__ */
