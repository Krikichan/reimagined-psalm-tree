#include <zephyr.h>
#include <device.h>
#include <logging/log.h>
#include <net/fota_download.h>
#include <sys/reboot.h>
#include <sys/byteorder.h>
#include <dfu/dfu_target.h>
#include <dfu/mcuboot.h>
#include <modem/modem_info.h>
#include <nrf_modem.h>
#include <string.h>
#include <dk_buttons_and_leds.h>
#include <logging/log.h>

#include "app_flash.h"

//Necessary simp Module
#include "simp.h"

//Dfu Module
#include "http_client.h"
#include "int_dfu_helper.h"
#include "dfu_modul.h"

LOG_MODULE_REGISTER(dfu_module, 3);
//Global Variable telling you wether or not the Module is enabled
static bool dfu_module_enabled = false;

//Not Necessary could be different its just easier to understand this way
//TODO Rename so its easier to understand
//nRF91
static uint32_t m_image_file_size91;
static uint8_t m_file_majorfw92;
static uint8_t m_file_minorfw92;

static uint8_t  m_cur_majorfw91;
static uint8_t	m_cur_minorfw91;
static uint32_t m_available_size91=0;

//nRF52
static uint32_t m_image_file_size52;
static uint8_t m_file_majorfw52;
static uint8_t m_file_minorfw52;

static uint8_t  m_cur_majorfw52;
static uint8_t	m_cur_minorfw52;
static uint32_t m_available_size52=0;

//Downloaded Image
static uint8_t  m_image_channel;
static uint8_t  m_image_file_type;
static bool 	m_download_busy;
static uint8_t 	more_updates = UPDATE_NOMORE; 


#define FW_NAME_LEN					30 		/* Maximale Fw Namen Länge */

static char* m_http_host = "embbuck.s3.eu-central-1.amazonaws.com";  //Replace mit eigener URL
static char*  m_http_file_52 = "fw52.bin";
static char*  m_http_file_91 = "fw91.bin";

static struct k_work wk_serial_dfu;             /* k_work to perform DFU */
static struct k_work wk_update_mcuboot_flag;	/*Bs wk*/

//Forward Declaration
static void logik_handler();
static void perform_dfu(void);

/**@brief Do mcuboot dfu for 91 */
void do_mcuboot_dfu(void)
{
	//Todo Tell other Chip that you restart
	LOG_INF("Start to reboot...");
	sys_reboot(SYS_REBOOT_WARM);
}

void get_device_status(){
	uint8_t nrf91buffer[6];
	if(m_available_size52 == 0){
		//TODO Implement wait so the app handler goes into idle (nicht zwingend notwendig)
		app_cmd_request(CMD_OP_DEVINF52,NULL,0);
		m_available_size52 = 52000;

	}
	if(m_available_size91 == 0){
		app_flash_get_DevInf(nrf91buffer,6);
		m_available_size91 = sys_get_le32(&nrf91buffer[2]);
		m_cur_majorfw91 = nrf91buffer[1];
		m_cur_minorfw91 = nrf91buffer[0];

		LOG_INF("nRF91:Available Flash:%x FWNum%x.%x",m_available_size91,m_cur_majorfw91,m_cur_minorfw91);
	}
	k_sleep(K_SECONDS(2));//Wait for asynchronous Uart Call to finish
}

bool module_active(){
	return dfu_module_enabled;
}

//converts str to 32integer
static int32_t strto32Int(uint8_t* buffer)
{
    int newint = 0;
    int j = 0;
    char ch;
    // Traverse the string
    while((ch = buffer[j]) != '\0'){
            newint = newint * 10 + (ch - 48);
            j++;
    }
    return newint;
}


void parse_sms(uint8_t *buffer,uint16_t len){
	LOG_INF("Parsing SMS Request into Server Call");
	/**
	 * Required SMS Layout
	 * Length 7 + 2 for Delimiter '-'
	 * 1 Byte for what to Update 2 Bytes for Major and Minor Fw
	 *  5 -> 52
	 *  9 -> 91
	 * 
	 * Length 13+4 for  Delimiter '-'
	 * 1 Byte for what to Update | nrf52  2 Bytes for Major and Minor Fw | nRF91 2 Bytes for Major and Minor Fw
	 * B -> Both
	 */


	 	const char s[2] = "-"; //Delimiter between individual Numbers
	    char *token;
		uint8_t i=1;

		token = strtok(buffer, s);
		more_updates = token[0]-48;

		token = strtok(NULL, s);
		if(more_updates != UPDATE_BOTH){
			while( token != NULL ) {
				if(i == 1){
					if(more_updates == UPDATE_NRF52){
						m_image_file_size52 = strto32Int(token);
					}else{
						m_image_file_size91 = strto32Int(token);
					}
				}
				if(i == 2){
					if(more_updates == UPDATE_NRF52){
						m_file_majorfw52 = token[0]-48;

						m_file_minorfw52 = token[1]-48;
					}else{
						m_file_majorfw92 = token[0]-48;
						m_file_minorfw92 = token[1]-48;
					}

				}
				i++;
	      		token = strtok(NULL, s);
	   		}
		}else{
			while( token != NULL ) {
				if(i == 1){
					m_image_file_size52 = strto32Int(token);
				}
				if(i == 2){
					m_file_majorfw52 = token[0]-48;
					m_file_minorfw52 = token[1]-48;
				}
				if(i== 3){
					m_image_file_size91 = strto32Int(token);
				}
				if(i == 4){
					m_file_majorfw92 = token[0]-48;
					m_file_minorfw92 = token[1]-48;
				}
				i++;
	      		token = strtok(NULL, s);
	   		}
		}

	LOG_INF("Parsed SMS Data: m52: %d m91: %d maj52: %x min52: %x maj91: %x min91: %x",m_image_file_size52,m_image_file_size91,m_file_majorfw52,m_file_minorfw52,m_file_majorfw92,m_file_minorfw92);
	//Call Update Logik Handler
	logik_handler();
}


/**@brief A dummy callback function */
static void dfu_target_cb_dummy(enum dfu_target_evt_id evt_id){;}

/**@brief k_work handler for updating mcuboot flag */
static void wk_update_mcuboot_flag_handler(struct k_work* unused)
{
	if (m_image_file_type == IMAGE_TYPE_NRF52) {
		app_flash_erase_from_end(1);
		perform_dfu();
	}
	else if (m_image_file_type == IMAGE_TYPE_NRF91) {
		dfu_target_init(DFU_TARGET_IMAGE_TYPE_MCUBOOT, 0, dfu_target_cb_dummy);	
		dfu_target_done(true);			
	}
	else {
		LOG_ERR("Inavlid image file type");
	}
}

/**@brief Callback of k_work to perform DFU */
static void wk_serial_dfu_handler(struct k_work* unused)
{
	//call httpfunc
	m_image_file_size52 = get_file_size();
	LOG_INF("Downloaded Image Size:%u",m_image_file_size52);
	
	startdfuprocess(m_image_file_size52);//no logik
}
/**@brief Start to download a file */
static void http_download_start(char* file_path)
{
    LOG_ERR("%s, file_path:%s", __func__, file_path);
	LOG_INF("Download Start");
	m_image_channel = IMAGE_FROM_HTTP;
	m_download_busy = true;

	http_client_download(m_http_host, file_path);
}

/**@brief Start to run DFU to 91 or 52 due to image type */
static void perform_dfu(void)
{
	if (m_image_file_type == IMAGE_TYPE_NRF52) {
		k_work_submit(&wk_serial_dfu);
	}
	else {
		do_mcuboot_dfu();
	}
}

//Todo Tell Bluetooth to stop Advertising and if connected do logik there currently (disconnect and stop advertising)
static void logik_handler(){
	get_device_status();
	if(more_updates==UPDATE_NRF52)
	{
		//Start update Procedure
		if(m_available_size52 > m_image_file_size52){
			m_image_file_type = IMAGE_TYPE_NRF52; 
			more_updates= UPDATE_NOMORE;
			//Start Download
			LOG_INF("Requested Single Update of nRF52");
			LOG_DBG("Request Stop of BT Functionality");
			//app_reqq
			//Insert Request Here
			http_download_start(m_http_file_52);
		}else{
			LOG_ERR("Not Enough available Space av52:%d im52:%d",m_available_size52,m_image_file_size52);
		}
	}
	else if(more_updates==UPDATE_NRF91)
	{
		//Start Download
		if(m_available_size91 > m_image_file_size91){
			more_updates= UPDATE_NOMORE;
			m_image_file_type = IMAGE_TYPE_NRF91;

			LOG_INF("Requested Single Update of nRF91");
			//Maybe Tell nRF52 that you start to Update yourself
			http_download_start(m_http_file_91);
		}else{
			LOG_ERR("Not Enough available Space av91:%d im91:%d",m_available_size91,m_image_file_size91);
		}
	}
	else if(more_updates==UPDATE_BOTH)
	{
		//Start update Procedure with nRF52
		if(m_available_size52 > m_image_file_size52){
			m_image_file_type = IMAGE_TYPE_NRF52; 
			more_updates= UPDATE_NRF91;
			LOG_INF("Requested Double Update of nRF52");
			LOG_DBG("Request Stop of BT Functionality");
			http_download_start(m_http_file_52);
		}else{
			LOG_ERR("Not Enough available Space av52:%d im52:%d",m_available_size52,m_image_file_size52);
		}
	}
}

/**@brief Handler for DFU file is downloaded or received 
 * NRF91 Implementation has more cases so its different
*/
static void dfu_file_ready(void)
{
	LOG_INF("File Ready");


	if (m_image_file_type < 0) {
		LOG_ERR("DFU file is invalid, %d", m_image_file_type);
		return;
	}

	/* If fetching image from http, fota_download library will
	 * take it as a mcuboot image, and adds a flag at the end of
	 * secondary bank, we need to remove it for 52 image to stop
	 * mcuboot behavior after reboot.
	 * If fetching image by UART fromrnrf52, we need to add a 
	 * mcuboot flag manually to make it a valid mcuboot image.
	 */
	if (m_image_channel == IMAGE_FROM_HTTP) {
		if (m_image_file_type == IMAGE_TYPE_NRF52) {
			// Remove the last page of secondary bank
			LOG_INF("Remove MCUboot flag for 52 image");

			k_work_submit(&wk_update_mcuboot_flag);
		}
		else {
			LOG_INF("Press button 2 to do DFU");
		}		
	}
	else if (m_image_channel == IMAGE_FROM_SERIAL) {
		if (m_image_file_type == IMAGE_TYPE_NRF91) {
			// Add mcuboot meta info
			LOG_INF("Add MCUboot flag for 91 application");

			k_work_submit(&wk_update_mcuboot_flag);
		}
		else if (m_image_file_type == IMAGE_TYPE_MODEM) {
			// Add mcuboot meta info
			LOG_INF("Add MCUboot flag for 91 modem");

			k_work_submit(&wk_update_mcuboot_flag);
		}
	}
	else {
		// Should not come here
		LOG_ERR("Invalid image channel");
	}
}

//-------------------------------
// Requests and Responses
//-------------------------------
static void serverresp(void);
//Todo Initalize Threading here with read Buffer
void askserver(struct k_work* unused){

	//-> https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/mcuboot/design.html Look at header//Entire IMG Header is 32 Bytes -> 265 Bits

	//Todo HTTP Req if new Images are available
	//Ask for files and size
	//Utilize REST Client would make this way easier Problem right now is i dont know how Server Structure would look like
	//ALSO IMPORTANT
	//Wait for the Awnser of the Other Processor otherwise you dont have accurate Information of the other Processor
	//Alternativly run get_status() Func before updating
	serverresp();
}

//Todo Implement everything
static void serverresp(void){
	
	//Response Idealy should have 8 + 8 Byte for each Image Size + 1 Byte for OP Code

	//Button Logik
	//this is just placeholder
	if(DK_BTN3_MSK & dk_get_buttons()){
		if(DK_BTN4_MSK & dk_get_buttons()){
			more_updates = UPDATE_NRF91;
			LOG_INF("Update NRF91");
		}else{
			more_updates = UPDATE_NRF52;
			LOG_INF("Update NRF52");
		}
	
	}else{
		more_updates = UPDATE_BOTH;
		LOG_INF("Update Both");
	}
	if(more_updates == UPDATE_NRF91){
		m_image_file_size91 = 100; //safety net
	}else if(more_updates == UPDATE_NRF52){ 
		m_image_file_size52 = 100; //Watchout ANPASSEN
	}else if(more_updates == UPDATE_BOTH){
		m_image_file_size91 = 100;
		m_image_file_size52 = 100;
	}

	//Call Update Logik Starts Update Procedure if new FW available
	if(more_updates != UPDATE_NOMORE){
		logik_handler();
	}else{
		LOG_INF("No New Firmware available");
	}
}

/*	Simp Commands	*/
//size 4 8 die bytes 0 minor fw 1 major fw num
static int req_dev_fwinfo92(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond)
{
	LOG_DBG("Send Fw Information to nRF52");
	uint8_t nrf91buffer[6];
	app_flash_get_DevInf(nrf91buffer,6);
	respond(nrf91buffer, 6);
	return 0;
}

//size 4 8 die bytes 0 minor fw 1 major fw num
static void rsp_dev_fwinfo52(uint8_t* p_rsp, uint16_t rsp_len)
{
	m_available_size52 = sys_get_le32(&p_rsp[2]);
	m_cur_majorfw52 = p_rsp[1];
	m_cur_minorfw52 = p_rsp[0];

	LOG_INF("nRF52:Available Flash:%x FWNum%x.%x",m_available_size52,m_cur_majorfw52,m_cur_minorfw52);
}

static void rsp_stop_bt(uint8_t* p_rsp, uint16_t rsp_len){
	LOG_INF("Bluetooth Stopped. Continue with Download for nRF52");
	http_download_start(m_http_file_52);
}

//-------------------------------
//Handlers
//-------------------------------
/** @brief app_cmd request & response event handler.
 *		 Note, don't run long-time task here. 
 */
//TODO add update info to this like time took
void app_cmd_event_handler(cmd_event_t* p_event)
{
	switch (p_event->op_code) {
	case CMD_OP_FLASH_DONE_52:
		LOG_INF("Finished Serial DFU");
		if(more_updates == UPDATE_NRF91){
			logik_handler();
		}
	case CMD_OP_FLASH_DONE_92:
		LOG_INF("Serial DFU Tx Finished");
		m_image_file_type = IMAGE_TYPE_NRF91;
		m_image_channel = IMAGE_FROM_SERIAL;

		dfu_file_ready();
	default:
		LOG_DBG("cmd op: 0x%02x", p_event->op_code);
		break;
	}
}

/** @brief FOTA download event handler */
void download_event_handler(const struct fota_download_evt* evt)
{
	switch (evt->id) {
	case FOTA_DOWNLOAD_EVT_FINISHED:
		LOG_INF("FOTA download finished");
		m_download_busy = false;
		dfu_file_ready();
		break;

	case FOTA_DOWNLOAD_EVT_ERROR:
		LOG_ERR("FOTA download error");
		m_download_busy = false;
		break;

	default:
		break;
	}
}

void initalize_dfu_module(){
	int err = 0;

	err = http_client_init(download_event_handler);
	if (err) {
		LOG_ERR("HTTP Client init error.");
		return;
	}

	LOG_INF("Connecting LTE...(don't run other tasks now)");
	err = http_client_connect();
	if (err) {
		LOG_ERR("Can't connect to LTE");
		return;
	}

	LOG_INF("LTE network is connected.");
	app_dfu_process_init();

	//Fw Info
	app_cmd_add(CMD_OP_DEVINF52,NULL,rsp_dev_fwinfo52);
	app_cmd_add(CMD_OP_DEVINF92,req_dev_fwinfo92,NULL);

	k_work_init(&wk_serial_dfu, wk_serial_dfu_handler);
	k_work_init(&wk_update_mcuboot_flag, wk_update_mcuboot_flag_handler);

	dfu_module_enabled = true;

}

void reenable_dfu_module(){

	int err;

	LOG_INF("Connecting LTE...(don't run other tasks now)");
	err = http_client_connect();
	if (err) {
		LOG_ERR("Can't connect to LTE");
		return;
	}

	LOG_INF("LTE network is connected.");
	dfu_module_enabled = true;

}

void disable_dfu_module(){
	int err;

	err = pd_modem();
		if (err) {
		LOG_ERR("Can't power down Modem");
		return;
	}
	dfu_module_enabled = false;
}