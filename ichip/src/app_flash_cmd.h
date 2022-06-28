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
	
// app flash commands 92: 0x30 - 0x2F#
#define CMD_OP_FLASH_92        0x30
#define CMD_OP_FLASH_INFO_92   (CMD_OP_FLASH_92 + 1)
#define CMD_OP_FLASH_READ_92   (CMD_OP_FLASH_92 + 2)
#define CMD_OP_FLASH_WRITE_92  (CMD_OP_FLASH_92 + 3)
#define CMD_OP_FLASH_ERASE_92  (CMD_OP_FLASH_92 + 4)
#define CMD_OP_FLASH_CRC_92    (CMD_OP_FLASH_92 + 5)
#define CMD_OP_FLASH_START_92  (CMD_OP_FLASH_92 + 6)
#define CMD_OP_FLASH_DONE_92   (CMD_OP_FLASH_92 + 7)




/**@brief Register flash related commands
 *
 * @param cb: event callback
 */
void app_flash_cmd_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FLASH_CMD_H__ */
