#include <stdio.h>
#include <zephyr.h>
#include <device.h>
#include <logging/log.h>
#include <sys/byteorder.h>

#include "app_cmd.h"
#include "app_uart.h"
#include "app_flash.h"
#include "int_dfu_helper.h"
#include "dfu_modul.h"

LOG_MODULE_REGISTER(cross_dfu_module, 3);

//Probably exists in util.h
#define CEIL_DIV(A, B) (((A) + (B) - 1) / (B))

static uint8_t    m_cmd_buffer[CMD_BUFFER_SIZE]; // 1024 + 8 Bytes Lang 

static uint32_t   m_img_size = 0;
static uint32_t   m_img_offset = 0;

static struct k_work wk_get_and_write_data;

//Reads and Sends FW Image Packets
//Gets Called in Write and Erase RSP
static void wk_get_and_write_data_handle(struct k_work* unused)
{
    static uint32_t buffer_len = 0;

    if(m_img_size - m_img_offset > IMG_BLOCK_SIZE)
        buffer_len = IMG_BLOCK_SIZE;
    else 
        buffer_len = m_img_size - m_img_offset;

    app_flash_read(m_img_offset,&m_cmd_buffer[8],buffer_len);

    sys_put_le32(m_img_offset, &m_cmd_buffer[0]);
    sys_put_le32(buffer_len, &m_cmd_buffer[4]);

    cmd_request_flash_write(m_cmd_buffer, buffer_len + 8);

    m_img_offset += buffer_len;

    LOG_INF("image(%d) %d/%d", buffer_len, m_img_offset, m_img_size);

}

//Entry Point for Serial DFU
void startdfuprocess(uint32_t img_size)
{
    uint32_t err;
    m_img_size=img_size;
    //Starts Update Process -> Rsponses call the worker function
    LOG_INF("Start DFU Process");
    err = cmd_request_flash_erase(0, CEIL_DIV(m_img_size, 0x1000));
     LOG_INF("Err %d",err);
}

/*---------------------------------------------------------------------*/
//cmds for requests
/*---------------------------------------------------------------------*/

/**@brief Request to write flash of nrf9160 device.
 *
 * @param p_data: pointer of data to be written, containing:
 *                address[4], length[4], data[length - 8].
 * @param length: length of data.
 */
uint32_t cmd_request_flash_write(uint8_t* p_data, uint16_t length)
{
    //LOG_INF(__func__);

    return app_cmd_request(CMD_OP_FLASH_WRITE_52, p_data, length);
}

/**@brief Request to read flash of nrf9160 device.
 *
 * @param address: Address of data to be read.
 * @param length: length of data.
 */
uint32_t cmd_request_flash_read(uint32_t address, uint16_t length)
{
    //LOG_INF(__func__);

    uint8_t p_data[8];
    sys_put_le32(address, &p_data[0]);
    sys_put_le32(length, &p_data[4]);

    return app_cmd_request(CMD_OP_FLASH_READ_52, p_data, sizeof(p_data));
}

/**@brief Request to erase flash pages of nrf9160 device.
 *
 * @param address: Address of data to be read.
 * @param count: count of pages, 1 page = 4096 bytes.
 */
uint32_t cmd_request_flash_erase(uint32_t address, uint16_t count)
{
    //LOG_INF(__func__);

    uint8_t p_data[8];
    sys_put_le32(address, &p_data[0]);
    sys_put_le32(count, &p_data[4]);

    return app_cmd_request(CMD_OP_FLASH_ERASE_52, p_data, sizeof(p_data));
}

/**@brief Request to indicate image transferring done.
 */
uint32_t cmd_request_flash_done(void)
{
    //LOG_INF(__func__);

    uint8_t p_data[4];
    sys_put_le32(m_img_size, &p_data[0]);

    return app_cmd_request(CMD_OP_FLASH_DONE_52, p_data, sizeof(p_data));
}
/*---------------------------------------------------------------------*/
//Responses
/*---------------------------------------------------------------------*/

/**@brief Callback function for flash erase response.
 *
 * @param p_rsp: response contains: "ok".
 */
static void rsp_cb_flash_erase(uint8_t* p_rsp, uint16_t rsp_len)
{
    //NRF_LOG_INFO(__func__);

    uint8_t p_ok[] = CMD_RSP_OK;

    if (memcmp(p_rsp, p_ok, sizeof(p_ok)) == 0)
    {
        k_work_submit(&wk_get_and_write_data);
    }
    else
    {
        LOG_ERR("Timeout");
    }
}

/**@brief Callback function for flash write response.
 *
 * @param p_rsp: response contains: "ok".
 */
static void rsp_cb_flash_write(uint8_t* p_rsp, uint16_t rsp_len)
{
    if (m_img_offset == m_img_size)
    {
        LOG_INF("image transfer done");
        m_img_offset = 0;

        cmd_request_flash_done();
    }
    else if (m_img_offset > 0)
    {
        k_work_submit(&wk_get_and_write_data);
    }
}

/**@brief Callback function for flash done response.
 *
 * @param p_rsp: response contains: "ok".
 */
static void rsp_cb_flash_done(uint8_t* p_rsp, uint16_t rsp_len)
{
    LOG_INF("rsp_flash_done");
}
/*---------------------------------------------------------------------------------*/
//Requests
/*---------------------------------------------------------------------------------*/


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

/**@brief Initializes CMD OP's and Worker Object
 */
void app_dfu_process_init(){
    /*Initialize CMD OPs*/
    //CMDOP52
    app_cmd_add(CMD_OP_FLASH_WRITE_52, NULL, rsp_cb_flash_write);
    app_cmd_add(CMD_OP_FLASH_ERASE_52, NULL, rsp_cb_flash_erase);
    app_cmd_add(CMD_OP_FLASH_DONE_52,  NULL, rsp_cb_flash_done);
    //CMDOP92
    app_cmd_add(CMD_OP_FLASH_WRITE_92, req_cb_flash_write, NULL);
    app_cmd_add(CMD_OP_FLASH_ERASE_92, req_cb_flash_erase, NULL);
    app_cmd_add(CMD_OP_FLASH_DONE_92,  req_cb_flash_done, NULL);
    //not in use
    app_cmd_add(CMD_OP_FLASH_READ_92,  req_cb_flash_read, NULL);
    app_cmd_add(CMD_OP_FLASH_CRC_92,   req_cb_flash_crc, NULL);
    app_cmd_add(CMD_OP_FLASH_START_92, req_cb_flash_start, NULL);

    /*Initialize Worker Funktions*/
    //init data getter worker
    k_work_init(&wk_get_and_write_data, wk_get_and_write_data_handle);
}


