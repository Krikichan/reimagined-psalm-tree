#include <stdio.h>
#include <zephyr.h>
#include <device.h>
#include <logging/log.h>
#include <sys/byteorder.h>

#include "app_cmd.h"
#include "app_uart.h"
#include "app_flash_cmd.h"
#include "app_flash.h"
#include "int_dfu_helper.h"

LOG_MODULE_REGISTER(dfu_helper, 3);

//Probably exists in util.h
#define CEIL_DIV(A, B) (((A) + (B) - 1) / (B))

#define IMG_PACKET_SIZE          128
#define PKTS_PER_BLOCK           8
#define IMG_BLOCK_SIZE           (IMG_PACKET_SIZE * PKTS_PER_BLOCK)

#define CMD_WRITE_ADDR_SIZE      4
#define CMD_WRITE_LEN_SIZE       4
#define CMD_WRITE_HEADER_SIZE    (CMD_WRITE_ADDR_SIZE + CMD_WRITE_LEN_SIZE)
#define CMD_BUFFER_SIZE          (IMG_BLOCK_SIZE + CMD_WRITE_HEADER_SIZE)       // address + length + image block data

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
//Responses
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

//Helper for init
void callback_init(){
    
    app_cmd_add(CMD_OP_FLASH_WRITE_52, NULL, rsp_cb_flash_write);
    app_cmd_add(CMD_OP_FLASH_ERASE_52, NULL, rsp_cb_flash_erase);
    app_cmd_add(CMD_OP_FLASH_DONE_52,  NULL, rsp_cb_flash_done);

    //init worker
    k_work_init(&wk_get_and_write_data, wk_get_and_write_data_handle);
}
