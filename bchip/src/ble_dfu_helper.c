#include <stdio.h>
#include <zephyr.h>
#include <device.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <sys/reboot.h>

#include <errno.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/services/nus.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <dfu/mcuboot.h>
#include <dfu/dfu_target.h>

#include <dk_buttons_and_leds.h>

#include "app_cmd.h"
#include "app_uart.h"
#include "app_flash_cmd.h"
#include "app_flash.h"
#include "blecmds.h"


#include "ble_dfu_helper.h"

LOG_MODULE_REGISTER(bl_dfu_helper, 3);

#define CEIL_DIV(A, B) (((A) + (B) - 1) / (B)) //This might already be in a util or common header

#define UPDATE_NRF52				0x00
#define UPDATE_NRF91				0x01
#define UPDATE_BOTH 				0x02
#define UPDATE_NOMORE               0x03

static uint8_t more_updates = UPDATE_NOMORE; 

static uint8_t    m_cmd_buffer[CMD_BUFFER_SIZE];

//Not Necessary could be different its just easier to understand this way

static uint8_t  m_cur_majorfw91;
static uint8_t	m_cur_minorfw91;
static uint32_t m_available_size91;

static uint8_t  m_cur_majorfw52;
static uint8_t	m_cur_minorfw52;
static uint32_t m_available_size52;


static uint32_t   cur_img_offset = 0;
static uint8_t    cur_image_file_type;
static uint32_t   cur_image_file_size;


static struct k_work wk_self_update;
static struct k_work wk_self_info;
static struct k_work wk_update_mcuboot_flag;
static struct bt_conn *current_conn;


uint32_t ble_send_req(uint8_t req);

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
    return app_cmd_request(CMD_OP_FLASH_WRITE_91, p_data, length);
}

/**@brief Request to read flash of nrf9160 device.
 *
 * @param address: Address of data to be read.
 * @param length: length of data.
 */
uint32_t cmd_request_flash_read(uint32_t address, uint16_t length)
{
    uint8_t p_data[8];
    sys_put_le32(address, &p_data[0]);
    sys_put_le32(length, &p_data[4]);

    return app_cmd_request(CMD_OP_FLASH_READ_91, p_data, sizeof(p_data));
}

/**@brief Request to erase flash pages of nrf9160 device.
 *
 * @param address: Address of data to be read.
 * @param count: count of pages, 1 page = 4096 bytes.
 */
uint32_t cmd_request_flash_erase(uint32_t address, uint16_t count)
{
    uint8_t p_data[8];
    sys_put_le32(address, &p_data[0]);
    sys_put_le32(count, &p_data[4]);

    return app_cmd_request(CMD_OP_FLASH_ERASE_91, p_data, sizeof(p_data));
}

/**@brief Request to indicate image transferring done.
 */
uint32_t cmd_request_flash_done(void)
{
 

    uint8_t p_data[4];
    sys_put_le32(cur_image_file_size, &p_data[0]);

    return app_cmd_request(CMD_OP_FLASH_DONE_91, p_data, sizeof(p_data));
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
    LOG_INF("Erase Rsp");

    uint8_t p_ok[] = CMD_RSP_OK;

    if (memcmp(p_rsp, p_ok, sizeof(p_ok)) == 0)
    {
        ble_send_req(REQ_GET_IMG_DATA);
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

    if (cur_img_offset == cur_image_file_size)
    {
        cur_img_offset = 0;
        ble_send_req(REQ_TX_SUCCESS);
        cmd_request_flash_done();
    }
    else if (cur_img_offset > 0)
    {
       ble_send_req(REQ_GET_IMG_DATA);
    }
}

/**@brief Callback function for flash done response.
 *
 * @param p_rsp: response contains: "ok".
 */
static void rsp_cb_flash_done(uint8_t* p_rsp, uint16_t rsp_len)
{
    //Check if we need to Update ourself
    
    if (more_updates != UPDATE_NRF52){
        ble_send_req(REQ_UPDATE_SELF);
        LOG_INF("Start self Udpate");
    }
    LOG_INF("In Flash Done");
}



/*---------------------------------------------------------------------*/
//Self Update 
/*---------------------------------------------------------------------*/


static void get_self_info(){

    int err;
    uint8_t nrf52buffer[6];

    err = app_flash_get_DevInf(nrf52buffer,6);  

    if(err != 0){
        LOG_INF("Error Dev Info get %d",err);
    }

    m_cur_majorfw52 = nrf52buffer[0];
    m_cur_minorfw52 = nrf52buffer[1];
    m_available_size52 = sys_get_le32(&nrf52buffer[2]);
}

static void wk_self_update_handler(struct k_work* unused){

    int err = 0;

    uint32_t offset = sys_get_le32(&m_cmd_buffer[0]);
    uint32_t cur_bufferlen = sys_get_le32(&m_cmd_buffer[4]);

    uint8_t* p_data = &m_cmd_buffer[8];

    err = app_flash_write(offset,p_data,cur_bufferlen);
    
    if(err < 0){
        LOG_ERR("Problem bei Write %d",err);
        return;
    }

    if (cur_img_offset == cur_image_file_size){

        ble_send_req(REQ_TX_SUCCESS);
        dfu_file_ready(IMAGE_FROM_BLUETOOTH);
        cur_img_offset = 0;
    }else{
        ble_send_req(REQ_GET_IMG_DATA);
    }
}

/**
 * @brief Handles getting Flash Info 
 * Worker Object so one doesnt Block CB for to Long
 * @param unused .
 */
static void wk_self_info_handler(struct k_work* unused){
    int err;
    if(m_available_size52 < cur_image_file_size){
        LOG_ERR("Not enough Space for Image available Size: %x required Size: %x",m_available_size52,cur_image_file_size);
        return;
    }

    err = app_flash_erase_page(0, cur_image_file_size);

    if(err !=0){
        LOG_ERR("Error during Erase: %d",err);
    }
    LOG_INF("Start Receiving Data Packages");
    ble_send_req(REQ_GET_IMG_DATA);
}


/*---------------------------------------------------------------------*/
//Bluetooth Helper
/*---------------------------------------------------------------------*/
//Sends DevInfo to Bluetooth Central
void send_deviceinfo(void){
    uint8_t buf[13] ;// RSP Code91 FW Major Minor 2 Byte Av Size 4 -> 6 +52
    // FW 91 major minor FW 52 major minor Av 91 Av 52
    buf[0] = RSP_DEVINF;
    buf[1] = m_cur_majorfw91;
    buf[2] = m_cur_minorfw91;
    buf[3] = m_cur_majorfw52;
    buf[4] = m_cur_minorfw52;
    sys_put_le32(m_available_size91,&buf[5]);
    sys_put_le32(m_available_size52,&buf[9]);

    //bt_nus_send(current_conn,buf,13);
}
/**@brief Notify a request through ble notification to NUS central.
 */
uint32_t ble_send_req_conn(const struct bt_conn *conn, uint8_t req)
{
    const uint8_t data[1] = {req};
    uint16_t len = 1;
    //return bt_nus_send(conn,data,len);

}

/**@brief Notify a request through ble notification to NUS central.
 */
uint32_t ble_send_req(uint8_t req)
{
    const uint8_t data[1] = {req};
    uint16_t len = 1;
    //return bt_nus_send(current_conn,data,len);
}


/**@brief A dummy callback function */
void dfu_target_cb_dummy(enum dfu_target_evt_id evt_id) {;}

/**@brief k_work handler for updating mcuboot flag */ //-> Verlagern in DFU Target UART ist somewhat useless hier
static void wk_update_mcuboot_flag_handler(struct k_work* unused)
{
    uint8_t dummybuf[8];
    if (cur_image_file_type == IMAGE_TYPE_NRF52) {
	    dfu_target_mcuboot_set_buf(dummybuf, 8);
        dfu_target_init(DFU_TARGET_IMAGE_TYPE_MCUBOOT, 0, dfu_target_cb_dummy); 
        dfu_target_done(true);
    }
    else {
        LOG_ERR("Inavlid image file type %c",cur_image_file_type);
    }
}

static void retrieve_dev_info(){

    if(m_available_size52 != -1){
        get_self_info();
    }

    if(m_available_size91 != -1){
        app_cmd_request(CMD_OP_DEVINF91,NULL,0);
        //In Response Send Retrieved Data
    }else{
       send_deviceinfo(); 
    }
}

//Toodls write new function that gets called for image data and logik seperatly
void nus_received_cb_handler(struct bt_conn *conn, const uint8_t *const p_write_data,uint16_t write_data_len)
{   
    current_conn = conn; //Super Unsafe :D
    
    static uint32_t buffer_len = 0;
    
    uint8_t  ble_data_flag = p_write_data[0];
    
    const uint8_t* p_img_data = &(p_write_data[1]);
    uint16_t img_data_len = write_data_len - 1;

    /* START_DFU content: flag[1] */
    switch(ble_data_flag){
        case REQ_START_DFU:{//First Call Second is Send Fw Data
            LOG_INF("In Start_DFU");
            retrieve_dev_info();
            break;
        }
        case REQ_GET_IMG_INFO:{//Third Call
            cur_image_file_size = sys_get_le32(&p_img_data[0]);
            more_updates = p_img_data[4];

            LOG_INF("Image file size: %d Image Type Received: %d", cur_image_file_size,cur_image_file_type);

            if(more_updates == UPDATE_NRF91){
                cur_image_file_type = IMAGE_TYPE_NRF91;
                cmd_request_flash_erase(0, CEIL_DIV(cur_image_file_size, 0x1000));
            }else if (more_updates == UPDATE_NRF52){
                cur_image_file_type = IMAGE_TYPE_NRF52;
                k_work_submit(&wk_self_info); // Start Receiving in Function
            
            }
        }
        case REQ_GET_IMG_DATA:{
            // Reserve 8 bytes to store address[4] and img_data_len[4]
            memcpy(&m_cmd_buffer[8 + buffer_len], p_img_data, img_data_len);
            buffer_len += img_data_len;

            if (buffer_len == IMG_BLOCK_SIZE || buffer_len == cur_image_file_size - cur_img_offset){//IMG_BLOCK_SIZE 1024

                sys_put_le32(cur_img_offset, &m_cmd_buffer[0]);
                sys_put_le32(buffer_len, &m_cmd_buffer[4]);

                if(cur_image_file_type == IMAGE_TYPE_NRF91){
                    cmd_request_flash_write(m_cmd_buffer,buffer_len+8);
                }else{
                    k_work_submit(&wk_self_update);
                }

                cur_img_offset += buffer_len;

                LOG_INF("Send %d/%d", cur_img_offset, cur_image_file_size);

                buffer_len = 0;

            }else{
                ble_send_req(REQ_GET_IMG_DATA);
            }
        }
    }
}

/**@brief Handler for DFU file is downloaded or received */

//TODO: Rewrite so it shows that File Ready and lights up different LED

void dfu_file_ready(uint8_t cur_image_channel)
{
    if (cur_image_channel == IMAGE_FROM_BLUETOOTH) {
            cur_image_file_type = IMAGE_TYPE_NRF52;
            dk_set_leds(DK_LED4);
            k_work_submit(&wk_update_mcuboot_flag);
            LOG_INF("Image received over Bluetooth \nPress Button 2 to do Reboot");
    }       
    else if (cur_image_channel == IMAGE_FROM_SERIAL) {
            dk_set_leds(DK_LED4);
            cur_image_file_type = IMAGE_TYPE_NRF52;

            k_work_submit(&wk_update_mcuboot_flag);
            LOG_INF("Image received over Uart \nPress Button 2 to do Reboot");
    }
    else {
        // Should not come here
        LOG_ERR("Invalid image channel %c",cur_image_channel);
    }
}

//size 4 8 die bytes 0 minor fw 1 major fw num
int req_dev_fwinfo52(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond)
{
    //Read fw num + available size
    LOG_INF("In FW Info");
    uint8_t nrf52buffer[6];
    app_flash_get_DevInf(nrf52buffer,6);
    respond(nrf52buffer,6);

    return 0;
}

//size 4 8 die bytes 0 minor fw 1 major fw num
void rsp_dev_fwinfo91(uint8_t* p_rsp, uint16_t rsp_len)
{
    m_available_size91 = sys_get_le32(&p_rsp[4]);
    m_cur_majorfw91 = p_rsp[1];
    m_cur_minorfw91 = p_rsp[0];

    LOG_INF("nRF91:Available Flashspace:%x FWNum%x.%x",m_available_size91,m_cur_majorfw91,m_cur_minorfw91);
    
    if(m_available_size91 < cur_image_file_size){
        LOG_ERR("nRF91: Not enough Space for Image available Size: %x required Size: %x",m_available_size91,cur_image_file_size);
    }
    send_deviceinfo();
}

//Helper for init
void module_init(void){

    k_work_init(&wk_self_update,wk_self_update_handler);
    k_work_init(&wk_self_info,wk_self_info_handler);
    k_work_init(&wk_update_mcuboot_flag, wk_update_mcuboot_flag_handler);
    
    //Add Responses to Flash Commands From 91
    app_cmd_add(CMD_OP_FLASH_WRITE_91, NULL, rsp_cb_flash_write);
    app_cmd_add(CMD_OP_FLASH_ERASE_91, NULL, rsp_cb_flash_erase);
    app_cmd_add(CMD_OP_FLASH_DONE_91, NULL, rsp_cb_flash_done);

    app_cmd_add(CMD_OP_FLASH_READ_91, NULL, NULL);

    app_cmd_add(CMD_OP_DEVINF91,NULL,rsp_dev_fwinfo91);
    app_cmd_add(CMD_OP_DEVINF52,req_dev_fwinfo52,NULL);

}