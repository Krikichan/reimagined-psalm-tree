#ifndef INT_DFU_HELPER_H__
#define INT_DFU_HELPER_H__

#ifdef __cplusplus
extern "C" {
#endif

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