#ifndef INT_DFU_HELPER_H__
#define INT_DFU_HELPER_H__

#include <stdint.h>
#include "app_flash_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

void callback_init();

/**@brief Request to get flash info of nrf9160 device.
 */
uint32_t cmd_request_flash_info(void);

/**@brief Request to write flash of nrf9160 device.
 *
 * @param p_data: pointer of data to be written, containing:
 *                address[4], length[4], data[length - 8].
 * @param length: length of data.
 */
uint32_t cmd_request_flash_write(uint8_t* p_data, uint16_t length);

/**@brief Request to read flash of nrf9160 device.
 *
 * @param address: Address of data to be read.
 * @param length: length of data.
 */
uint32_t cmd_request_flash_read(uint32_t address, uint16_t length);

/**@brief Request to erase flash pages of nrf9160 device.
 *
 * @param address: Address of data to be read.
 * @param count: count of pages, 1 page = 4096 bytes.
 */
uint32_t cmd_request_flash_erase(uint32_t address, uint16_t count);

/**@brief Request to indicate image transferring done.
 */
uint32_t cmd_request_flash_done(void);
 
 
void startdfuprocess(uint32_t img_size);


#ifdef __cplusplus
}
#endif

#endif /* INT_DFU_HELPER_H__ */