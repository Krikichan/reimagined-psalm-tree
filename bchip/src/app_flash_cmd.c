#include <zephyr.h>
#include <device.h>
#include <sys/byteorder.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(cmd_flash, 3);

#include "app_cmd.h"
#include "app_flash.h"
#include "app_flash_cmd.h"

/*---------------------------------------------------------------------------------*/
//Requests
/*---------------------------------------------------------------------------------*/

/**@brief Callback of flash info request
 *
 * @details Request: none.
 * Response: {bank address[4], page count of the bank[4],
 * first blank page[4]} or none
 *
 * @param[in] p_req     Pointer of request data
 * @param[in] req_len   Length of request data
 * @param[in] respond   Callback function of responding
 *
 * @return 0 success
 * @return neg error
 */
static int req_cb_flash_info(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond)
{
    LOG_DBG("%s", __func__);

    int   rc;
    uint8_t  p_rsp[12];
    uint16_t rsp_len = 12;

    rc = app_flash_si_slotinfo(p_rsp);

    LOG_DBG("add: %08x, cnt: %d, first: %x", \
            sys_get_le32(&p_rsp[0]),         \
            sys_get_le32(&p_rsp[4]),         \
            sys_get_le32(&p_rsp[8]));

    if (rc == 0) {
        respond(p_rsp, rsp_len);
    }
    else {
        respond(NULL, 0);
    }

    return rc;
}

/**@brief Callback of flash read request
 *
 * @details Request: address offset[4], data length[4]
 * Response: read data or none
 *
 * @param[in] p_req     Pointer of request data
 * @param[in] req_len   Length of request data
 * @param[in] respond   Callback function of responding
 *
 * @return 0 success
 * @return neg error
 */
static int req_cb_flash_read(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond)
{
    LOG_DBG("%s", __func__);

    int   rc;
    uint8_t* p_rsp;
    uint16_t rsp_len;

    uint32_t offset = sys_get_le32(&p_req[0]);
    uint32_t length = sys_get_le32(&p_req[4]);

    LOG_INF("read offset: %x, length: %d", offset, length);

    rsp_len = length;

    p_rsp = k_malloc(rsp_len);

    if (p_rsp != NULL) {
        rc = app_flash_read(offset, p_rsp, length);
        if (rc == 0) {
            respond(p_rsp, rsp_len);
            k_free(p_rsp);
        }
        else {
            respond(NULL, 0);
        }
    }
    else {
        rc = -1;
        respond(NULL, 0);
    }

    return rc;
}

/**@brief Callback of flash write request
 *
 * @details Request: address offset[4], data length[4],
 * data[N]. Response: "ok" or none
 *
 * @param[in] p_req     Pointer of request data
 * @param[in] req_len   Length of request data
 * @param[in] respond   Callback function of responding
 *
 * @return 0 success
 * @return neg error
 */
static int req_cb_flash_write(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond)
{
    LOG_DBG("%s", __func__);

    int rc = 0;

    uint32_t offset = sys_get_le32(&p_req[0]);
    uint32_t length = sys_get_le32(&p_req[4]);
    uint8_t* p_data = &p_req[8];

    LOG_INF("write offset: %x, length: %d", offset, length);

    rc = app_flash_write(offset, p_data, length);
    if (rc == 0) {
        respond("ok", 2);
    }
    else {
        respond(NULL, 0);
    }

    return rc;
}

/**@brief Callback of flash erase request
 *
 * @details Request: address offset[4], page count[4]
 * Response: "ok" or none
 *
 * @param[in] p_req     Pointer of request data
 * @param[in] req_len   Length of request data
 * @param[in] respond   Callback function of responding
 *
 * @return 0 success
 * @return neg error
 */
static int req_cb_flash_erase(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond)
{
    LOG_DBG("%s", __func__);

    int rc;
    uint32_t offset = sys_get_le32(&p_req[0]);
    uint32_t count = sys_get_le32(&p_req[4]);

    rc = app_flash_erase_page(offset, count);
    if (rc == 0) {
        respond("ok", 2);
    }
    else {
        respond(NULL, 0);
    }

    LOG_INF("erase offset: %x, count: %d", offset, count);

    return rc;
}

/**@brief Callback of flash crc request
 *
 * @details Request: address offset[4], data length[4]
 * Response: crc32[4] or none
 *
 * @param[in] p_req     Pointer of request data
 * @param[in] req_len   Length of request data
 * @param[in] respond   Callback function of responding
 *
 * @return 0 success
 * @return neg error
 */
static int req_cb_flash_crc(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond)
{
    LOG_DBG("%s", __func__);

    int rc;
    uint8_t  p_rsp[4];
    uint16_t rsp_len = 4;

    uint32_t crc32;
    uint32_t offset = sys_get_le32(&p_req[0]);
    uint32_t length = sys_get_le32(&p_req[4]);

    LOG_INF("crc offset: %x, length: %d", offset, length);

    rc = app_flash_crc(offset, length, &crc32);
    if (rc == 0) {
        sys_put_le32(crc32, p_rsp);
        respond(p_rsp, rsp_len);
    }
    else {
        respond(NULL, 0);
    }

    return rc;
}

/**@brief Callback of flash start request
 *
 * @details Request: none
 * Response: "ok"
 *
 * @param[in] p_req     Pointer of request data
 * @param[in] req_len   Length of request data
 * @param[in] respond   Callback function of responding
 *
 * @return 0 success
 * @return neg error
 */
static int req_cb_flash_start(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond)
{
    LOG_DBG("%s", __func__);

    respond("ok", 2);

    return 0;
}

/**@brief Callback of flash done request
 *
 * @details Request: none
 * Response: "ok"
 *
 * @param[in] p_req     Pointer of request data
 * @param[in] req_len   Length of request data
 * @param[in] respond   Callback function of responding
 *
 * @return 0 success
 * @return neg error
 */
static int req_cb_flash_done(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond){
    LOG_DBG("%s", __func__);

    respond("ok", 2);

    return 0;
}

/**@brief Register flash related commands
 *
 * @param cb: event callback function
 */
void app_flash_cmd_init(void)
{
    app_cmd_add(CMD_OP_FLASH_INFO_52,  req_cb_flash_info,NULL);
    app_cmd_add(CMD_OP_FLASH_WRITE_52, req_cb_flash_write, NULL);
    app_cmd_add(CMD_OP_FLASH_ERASE_52, req_cb_flash_erase, NULL);
    app_cmd_add(CMD_OP_FLASH_DONE_52, req_cb_flash_done, NULL);

    app_cmd_add(CMD_OP_FLASH_READ_52,  req_cb_flash_read, NULL);
    app_cmd_add(CMD_OP_FLASH_CRC_52,   req_cb_flash_crc, NULL);
    app_cmd_add(CMD_OP_FLASH_START_52, req_cb_flash_start, NULL);

}
