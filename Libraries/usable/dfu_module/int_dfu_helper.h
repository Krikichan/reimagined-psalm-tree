#ifndef INT_DFU_HELPER_H__
#define INT_DFU_HELPER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**@brief Request to get flash info of nrf52840 device.
 */
uint32_t cmd_request_flash_info(void);

/**@brief Request to write flash of nrf52840 device.
 *
 * @param p_data: pointer of data to be written, containing:
 *                address[4], length[4], data[length - 8].
 * @param length: length of data.
 */
uint32_t cmd_request_flash_write(uint8_t* p_data, uint16_t length);

/**@brief Request to read flash of nrf52840 device.
 *
 * @param address: Address of data to be read.
 * @param length: length of data.
 */
uint32_t cmd_request_flash_read(uint32_t address, uint16_t length);

/**@brief Request to erase flash pages of nrf52840 device.
 *
 * @param address: Address of data to be read.
 * @param count: count of pages, 1 page = 4096 bytes.
 */
uint32_t cmd_request_flash_erase(uint32_t address, uint16_t count);

/**@brief Request to indicate image transferring done.
 */
uint32_t cmd_request_flash_done(void);
 
 /**@ Callable Starting Serial DFU Process to nRF52 Target Chip
  * 
  * @param img_size: Downloaded Image Size in Bytes
  */
void startdfuprocess(uint32_t img_size);

/**@brief Initializes all necessary Resources for the DFU CROSS Update Capabilities
 */
void app_dfu_process_init();

#ifdef __cplusplus
}
#endif

#endif /* INT_DFU_HELPER_H__ */