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

#include "app_flash.h"
#include "simp.h"
#include "app_flash.h"
#include "dfu_module_52.h"

LOG_MODULE_REGISTER(dfu_module, 3);

#define INTERVAL_MIN	0xF	    /* 15 ms */
#define INTERVAL_MAX	0x15	/* 20 ms */

#define CEIL_DIV(A, B) (((A) + (B) - 1) / (B)) //This might already be in a util or common header

static K_SEM_DEFINE(ble_update_param, 0, 1);
//static K_MEM_SLAB_DEFINE(dfu_rx_slab, 1032, 2, 4); /* Main Memory Source for Rx and Storage for Bluetooth Image Data */

static uint8_t more_updates = UPDATE_NOMORE; 

static uint8_t  m_cmd_buffer[CMD_BUFFER_SIZE];


//Not Necessary could be different its just easier to understand this way

static uint8_t  m_cur_majorfw91;
static uint8_t	m_cur_minorfw91;
static uint32_t m_available_size91 = 0;

static uint8_t  m_cur_majorfw52;
static uint8_t	m_cur_minorfw52;
static uint32_t m_available_size52 = 0;

static uint32_t   cur_img_offset = 0;
static uint8_t    cur_image_file_type;
static uint32_t   cur_image_file_size;

static struct k_work wk_self_update;
static struct k_work wk_self_info;

static struct k_work wk_update_mcuboot_flag;

static struct bt_conn *current_conn;

static struct bt_le_conn_param *fast_bt_conn_param =
	BT_LE_CONN_PARAM(INTERVAL_MIN, INTERVAL_MAX, 0, 400);

static struct bt_le_conn_param *normal_bt_conn_param =
	BT_LE_CONN_PARAM(BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, 0, 400);

//------------------------------------------------------------------------
//      Forward Declaration
//------------------------------------------------------------------------
uint32_t ble_send_req(uint8_t req);
uint32_t cmd_request_flash_write(uint8_t* p_data, uint16_t length);
uint32_t cmd_request_flash_read(uint32_t address, uint16_t length);
uint32_t cmd_request_flash_erase(uint32_t address, uint16_t count);
uint32_t cmd_request_flash_done(void);
static int update_bluetooth_params(struct bt_le_conn_param *conn_param);
static void retrieve_dev_info();
/*---------------------------------------------------------------------*/
//  Self Update related Functions
/*---------------------------------------------------------------------*/
/**
 * @brief Handles getting Img Data 
 * Worker Object so one doesnt Block CB for to Long
 * @param unused .
 */
static void wk_self_update_handler(struct k_work* unused){

    int err = 0;

    uint32_t offset = sys_get_le32(&m_cmd_buffer[0]);
    uint32_t cur_bufferlen = sys_get_le32(&m_cmd_buffer[4]);

    uint8_t* p_data = &m_cmd_buffer[8];

    err = app_flash_write(offset,p_data,cur_bufferlen);
    
    if(err < 0){
        if(err == -EACCES)
        LOG_ERR("Problem bei Write %d",err);

        if(err == -EINVAL){
            LOG_ERR ("Invalid Argument");
        }
        return;
    }

    if (cur_img_offset == cur_image_file_size){
        //Mark IMage Transfer Completed
        ble_send_req(REQ_TX_SUCCESS);
        //Add MCUBoot Flag
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
    int err = 0;

    if(m_available_size52 < cur_image_file_size){
        //Should not Come here
        LOG_ERR("Not enough Space for Image available Size: %x required Size: %x",m_available_size52,cur_image_file_size);
        return;
    }

    LOG_INF("Erase Flash This might take a while");
    //err = app_flash_erase_page(0, cur_image_file_size);

    if(err !=0){
        printk("Error during Erase: %d",err);
    }
    //Update Device BT Params
    update_bluetooth_params(fast_bt_conn_param);

    LOG_INF("Start Receiving Data Packages");
    ble_send_req(REQ_GET_IMG_DATA);
}


/*---------------------------------------------------------------------*/
//      Bluetooth Functions
/*---------------------------------------------------------------------*/

/** @brief Notify a request through ble notification to NUS central.
 */
uint32_t ble_send_req(uint8_t req)
{
    const uint8_t data[1] = {req};
    uint16_t len = 1;
    return bt_nus_send(current_conn,data,len);
}

/** @brief Updates BT Connection Parameters with current Connection
 */
static int update_bluetooth_params(struct bt_le_conn_param *conn_param){
	int err;
    struct bt_conn_info* current_info;

    err = bt_conn_get_info(current_conn,current_info);
    if(err){
        LOG_ERR("Couldnt obtain Connection Info %d",err);
    }

    LOG_INF("Maximum Data RX Length: %d Tx Length %d",current_info->le.data_len->rx_max_len,current_info->le.data_len->tx_max_len);

    LOG_INF("Try to Updated Connection parameters for better Tx Rate");

	err = bt_conn_le_param_update(current_conn, conn_param);
	if (err) {
		LOG_ERR(" Connection Param Update Error");
		return err;
	}

    //TODO Set Data length

	err = k_sem_take(&ble_update_param, K_SECONDS(2));
	if (err) {
		LOG_ERR("Connection Param Timeout");
        k_sem_give(&ble_update_param);
		return err;
	}
	k_sem_give(&ble_update_param);
	return 0;
}

/** @brief Tells the Connected BT Device that you will Cut the
 */ //Todo Make this more Robust
int disconnect_from_Central(){
    //Tell Central that youre about to Cut the Connection

    int err = 0;
    //Todo Check if Connection is Valid
    ble_send_req(CUT_CONNECTION);
    // Give it time to process
    k_sleep(K_SECONDS(2));
    //TODO Cut Connection
    err = bt_conn_disconnect(current_conn,BT_HCI_ERR_REMOTE_POWER_OFF);
    if(err){
        LOG_ERR("Couldnt Disconnect from BT Central Reason %d",err);
        return err;
    }
    LOG_INF("Disconnected from BT Device, Still advertising");
    return err;
}

/** @brief Main Entry Function for BT Related Updating
*/
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
            //Watch out it sends 9 Bytes as it cant do chars :)
            more_updates = p_img_data[img_data_len-1];

            LOG_INF("Image file size: %d Image Type Received: %x", cur_image_file_size,more_updates);

            //Change BT 
            if(more_updates == UPDATE_NRF91){
                cur_image_file_type = IMAGE_TYPE_NRF91;
                
                cmd_request_flash_erase(0, CEIL_DIV(cur_image_file_size, 0x1000));
            }
            if(more_updates == UPDATE_NRF52){
                cur_image_file_type = IMAGE_TYPE_NRF52;
                k_work_submit(&wk_self_info); // Start Receiving in Function
            }
            if(more_updates == UPDATE_BOTH){
                cur_image_file_type = IMAGE_TYPE_NRF91;
                
                cmd_request_flash_erase(0, CEIL_DIV(cur_image_file_size, 0x1000));
            }

            break;
        }
        case REQ_UPDATE_SELF:{
            buffer_len = 0;
            cur_img_offset = 0;
            cur_image_file_size = 0;
            ble_send_req(RSP_START_SELF_UPDATE);
            break;
        }
        case RSP_DONE_UPDATING:{
            LOG_INF("Done Updating Chips");
            buffer_len = 0;
            cur_img_offset = 0;
            cur_image_file_size = 0;
            //update_bluetooth_params(normal_bt_conn_param);
            break;
        }
        case REQ_GET_IMG_DATA:{

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

                LOG_INF("Send Bytes %d/%d", cur_img_offset, cur_image_file_size);

                buffer_len = 0;

            }else{
                ble_send_req(REQ_GET_IMG_DATA);
            }
            break;
        }
    }
}
/**@brief Sends DevInfo to Bluetooth Central
 */
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

    bt_nus_send(current_conn,buf,13);
}

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
    //Cut Ties with BT Central
    disconnect_from_Central();

#if AUTO_REBOOT
    //Todo Tell BT Central that Auto_Reboot is Enabled
        LOG_INF("Rebooting ....");
        sys_reboot(SYS_REBOOT_WARM);
#endif
}

static void retrieve_dev_info(){

    if(m_available_size52 == 0){
        get_self_info();
        LOG_INF("nRF52: Available Space %d FwVersion %x.%x \n",m_available_size52,m_cur_majorfw52,m_cur_minorfw52);
    }
    if(m_available_size91 == 0){
        app_cmd_request(CMD_OP_DEVINF91,NULL,0);
        //In Response Send Retrieved Data
    }else{
       send_deviceinfo(); 
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
            LOG_INF("Image received over Bluetooth Press Button 2 to do Reboot");
    }       
    else if (cur_image_channel == IMAGE_FROM_SERIAL) {
            dk_set_leds(DK_LED4);
            cur_image_file_type = IMAGE_TYPE_NRF52;

            k_work_submit(&wk_update_mcuboot_flag);
            LOG_INF("Image received over Uart Press Button 2 to do Reboot");
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
        //TODO Tell BT Device to Abort
    }
    send_deviceinfo();
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

    update_bluetooth_params(fast_bt_conn_param);

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
        //Mark Image as Transmitted for BT Central
        ble_send_req(REQ_TX_SUCCESS);
        //Tell NRF91 That Image Transfer is Complete
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
    LOG_INF("Update of nRF91 Finished");
}
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
//------------------------------------------------------------------------
//      Helper Functions
//------------------------------------------------------------------------
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

//Helper for init
void initalize_dfu_module(){
    

    //Init Worker Items
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

    //Register flash related commands
    app_cmd_add(CMD_OP_FLASH_INFO_52,  req_cb_flash_info,NULL);
    app_cmd_add(CMD_OP_FLASH_WRITE_52, req_cb_flash_write, NULL);
    app_cmd_add(CMD_OP_FLASH_ERASE_52, req_cb_flash_erase, NULL);
    app_cmd_add(CMD_OP_FLASH_DONE_52, req_cb_flash_done, NULL);

    app_cmd_add(CMD_OP_FLASH_READ_52,  req_cb_flash_read, NULL);
    app_cmd_add(CMD_OP_FLASH_CRC_52,   req_cb_flash_crc, NULL);
    app_cmd_add(CMD_OP_FLASH_START_52, req_cb_flash_start, NULL);

    LOG_INF("Initialized DFU Module");
}
