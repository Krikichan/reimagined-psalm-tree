#ifndef DFU_HELPER_H__
#define DFU_HELPER_H__

#include <stdint.h>
#include "../libs/app_flash_cmd.h"
#include "src/blecmds.h"

#ifdef __cplusplus
extern "C" {
#endif




void dfu_file_ready(uint8_t m_image_channel);

void module_init(void);

void nus_received_cb_handler(struct bt_conn *conn, const uint8_t *const p_write_data,uint16_t write_data_len);

uint32_t ble_send_req(uint8_t req);

uint32_t ble_send_req_conn(const struct bt_conn *conn,uint8_t req);

void self_update(void);

void self_info(void);

uint32_t cmd_request_flash_info(void);
uint32_t cmd_request_flash_write(uint8_t* p_data, uint16_t length);
uint32_t cmd_request_flash_read(uint32_t address, uint16_t length);
uint32_t cmd_request_flash_erase(uint32_t address, uint16_t count);
uint32_t cmd_request_flash_done(void);

#ifdef __cplusplus
}
#endif

#endif /* DFU_HELPER_H__ */
