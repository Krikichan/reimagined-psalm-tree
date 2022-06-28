
#ifndef APP_FLASH_CMD_H__
#define APP_FLASH_CMD_H__

#include <zephyr.h>
#include "app_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

// app flash commands: 0x20 - 0x2F
#define CMD_OP_FLASH_52        0x20
#define CMD_OP_FLASH_INFO_52   (CMD_OP_FLASH_52 + 1)
#define CMD_OP_FLASH_READ_52   (CMD_OP_FLASH_52 + 2)
#define CMD_OP_FLASH_WRITE_52  (CMD_OP_FLASH_52 + 3)
#define CMD_OP_FLASH_ERASE_52  (CMD_OP_FLASH_52 + 4)
#define CMD_OP_FLASH_CRC_52    (CMD_OP_FLASH_52 + 5)
#define CMD_OP_FLASH_START_52  (CMD_OP_FLASH_52 + 6)
#define CMD_OP_FLASH_DONE_52   (CMD_OP_FLASH_52 + 7)
	
// app flash commands 91: 0x30 - 0x2F#
#define CMD_OP_FLASH_91        0x30
#define CMD_OP_FLASH_INFO_91   (CMD_OP_FLASH_91 + 1)
#define CMD_OP_FLASH_READ_91   (CMD_OP_FLASH_91 + 2)
#define CMD_OP_FLASH_WRITE_91  (CMD_OP_FLASH_91 + 3)
#define CMD_OP_FLASH_ERASE_91  (CMD_OP_FLASH_91 + 4)
#define CMD_OP_FLASH_CRC_91    (CMD_OP_FLASH_91 + 5)
#define CMD_OP_FLASH_START_91  (CMD_OP_FLASH_91 + 6)
#define CMD_OP_FLASH_DONE_91   (CMD_OP_FLASH_91 + 7)

typedef struct{
    rsp_cb_t rsp_cb_info;
    rsp_cb_t rsp_cb_write;
    rsp_cb_t rsp_cb_erase;
    rsp_cb_t rsp_cb_done;

}rsp_cb_struc_t;


/**@brief Register flash related commands
 *
 * @param cb: event callback
 */
void app_flash_cmd_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FLASH_CMD_H__ */
