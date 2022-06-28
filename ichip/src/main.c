#include <string.h>
#include <zephyr.h>
#include <net/fota_download.h>
#include <sys/reboot.h>
#include <sys/byteorder.h>
#include <dfu/dfu_target.h>
#include <dfu/mcuboot.h>
#include <modem/modem_info.h>
#include <nrf_modem.h>
#include <posix/time.h>
#include <sys/timeutil.h>
#include <date_time.h>

#include <dk_buttons_and_leds.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(main, 3);

#include "http_client.h"
#include "app_flash.h"
#include "app_cmd.h"
#include "app_uart.h"
#include "app_flash_cmd.h"
#include "int_dfu_helper.h"

// Auf true setzen um Update Image zuerzeugen. NICHT FLASHEN !
#define UPDATE_APP 					false

#define IMAGE_FROM_HTTP				1
#define IMAGE_FROM_SERIAL			2

#define UART_DEVICE_LABLE 			"UART_1"
#define FW_NAME_LEN					30 		/* Maximale Fw Namen Länge */

#define CMD_OP_DEVINF52				0x01
#define CMD_OP_DEVINF92				0x02

#define MODEM_DATA_UNIT_LEN			256

//Codes zur Identifizierung was Geupdated werden soll
#define UPDATE_NRF52				0x00
#define UPDATE_NRF91				0x01
#define UPDATE_BOTH					0x02
#define UPDATE_NOMORE				0x03

//Codes zur Identifizierung was für ein Image Type vorhanden ist
#define IMAGE_TYPE_NRF52		0           /* SDK DFU file for nrf52 firmware */
#define IMAGE_TYPE_NRF91		1           /* Mcuboot file for nrf91 firmware */
#define IMAGE_TYPE_MODEM		2           /* Mcuboot file for nrf91 modem */
#define IMAGE_TYPE_ERROR		(-1)        /* Unknown file type */

static struct device*				m_uart_dev;
struct k_timer 				update_timer;

static char* m_http_host = "embbuck.s3.eu-central-1.amazonaws.com";  //Replace mit eigener URL
static char*  m_http_file_52 = "fw52.bin";
static char*  m_http_file_91 = "fw91.bin";


//Not Necessary could be different its just easier to understand this way
//TODO Rename so its easier to understand
//nRF91
static uint32_t m_image_file_size91;

static uint8_t  m_cur_majorfw91;
static uint8_t	m_cur_minorfw91;
static uint32_t m_available_size91;

//nRF52
static uint32_t m_image_file_size52;

static uint8_t  m_cur_majorfw52;
static uint8_t	m_cur_minorfw52;
static uint32_t m_available_size52;

//Downloaded Image
static uint8_t  m_image_channel;
static uint8_t  m_image_file_type;
static bool 	m_download_busy;
static uint8_t 	more_updates = UPDATE_NOMORE; 
static int 		img_type;

static struct k_work wk_serial_dfu;              /* k_work to perform DFU */
static struct k_work wk_update_mcuboot_flag;	/*Bs wk*/
static struct k_work helper;



static void perform_dfu(void);

/**@brief A dummy callback function */
void dfu_target_cb_dummy(enum dfu_target_evt_id evt_id) {;}

/**@brief Do mcuboot dfu for 91 */
void do_mcuboot_dfu(void)
{
	LOG_INF("Start to reboot...");

	sys_reboot(SYS_REBOOT_WARM);
}


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
	//Change !
	if (m_image_file_type == IMAGE_TYPE_NRF52) {
		// Check nRF52 in application mode or not
		k_work_submit(&wk_serial_dfu);
	}
	else {
		do_mcuboot_dfu();
	}
}

static void logik_handler(void){
	if(more_updates==UPDATE_NRF52)
	{
		//Start update Procedure
		if(m_available_size52 > m_image_file_size52){
			m_image_file_type = IMAGE_TYPE_NRF52; 
			img_type = IMAGE_TYPE_NRF52;
			more_updates= UPDATE_NOMORE;
			//Start Download
			LOG_INF("Starting Single Download NRF52");
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
			img_type = IMAGE_TYPE_NRF91;
			LOG_INF("Starting Single Download NRF91");
			
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
			img_type = IMAGE_TYPE_NRF52;
			more_updates= UPDATE_NRF91;
			LOG_INF("Starting Double Download NRF52");
			http_download_start(m_http_file_52);
			//Start Download
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


	if (img_type < 0) {
		LOG_ERR("DFU file is invalid, %d", img_type);
		return;
	}
	dk_set_led(DK_LED2,1);

	/* If fetching image from http, fota_download library will
	 * take it as a mcuboot image, and adds a flag at the end of
	 * secondary bank, we need to remove it for 52 image to stop
	 * mcuboot behavior after reboot.
	 * If fetching image by UART fromrnrf52, we need to add a 
	 * mcuboot flag manually to make it a valid mcuboot image.
	 */
	if (m_image_channel == IMAGE_FROM_HTTP) {
		if (img_type == IMAGE_TYPE_NRF52) {
			// Remove the last page of secondary bank
			LOG_INF("Remove MCUboot flag for 52 image");

			m_image_file_type = IMAGE_TYPE_NRF52;

			k_work_submit(&wk_update_mcuboot_flag);
		}
		else {
			LOG_INF("Press button 2 to do DFU");
		}		
	}
	else if (m_image_channel == IMAGE_FROM_SERIAL) {
		if (img_type == IMAGE_TYPE_NRF91) {
			// Add mcuboot meta info
			LOG_INF("Add MCUboot flag for 91 application");

			m_image_file_type = IMAGE_TYPE_NRF91;

			k_work_submit(&wk_update_mcuboot_flag);
		}
		else if (img_type == IMAGE_TYPE_MODEM) {
			// Add mcuboot meta info
			LOG_INF("Add MCUboot flag for 91 modem");
			m_image_file_type = IMAGE_TYPE_MODEM;

			k_work_submit(&wk_update_mcuboot_flag);
		}

		LOG_INF("Press button 2 to do DFU");
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
static void askserver(struct k_work* unused){

	uint8_t nrf91buffer[6]; //-> https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/mcuboot/design.html Look at header//Entire IMG Header is 32 Bytes -> 265 Bits

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

	//Todo HTTP Req if new Images are available
	//Ask for fw num of both files and size

	serverresp();
}

//Todo Implement everything XD
static void serverresp(void){
	
	//Response Idealy should have 8 + 8 Byte for each Image + 1 Byte for OP Code
	

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
	//this is just placeholder
	if(more_updates == UPDATE_NRF91){
		m_image_file_size91 = 100; //safety net
	}else if(more_updates == UPDATE_NRF52){ 
		m_image_file_size52 = 50000; //Watchout ANPASSEN
	}else if(more_updates == UPDATE_BOTH){
		m_image_file_size91 = 100;
		m_image_file_size52 = 50000;
	}
	//Call Update Logik Starts Update Procedure if new FW available
	if(more_updates != UPDATE_NOMORE){
		logik_handler();
	}else{
		LOG_INF("No New Firmware available");
	}
}
//size 4 8 die bytes 0 minor fw 1 major fw num
int req_dev_fwinfo92(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond)
{

	LOG_INF("In FW Info");
	uint8_t nrf91buffer[6];
	app_flash_get_DevInf(nrf91buffer,6);
	respond(nrf91buffer, 6);
	return 0;
}
//size 4 8 die bytes 0 minor fw 1 major fw num
void rsp_dev_fwinfo52(uint8_t* p_rsp, uint16_t rsp_len)
{
	m_available_size52 = sys_get_le32(&p_rsp[2]);
	m_cur_majorfw52 = p_rsp[1];
	m_cur_minorfw52 = p_rsp[0];

	LOG_INF("nRF52:Available Flash:%x FWNum%x.%x",m_available_size52,m_cur_majorfw52,m_cur_minorfw52);
}

//-------------------------------
//Handlers
//-------------------------------
/** @brief app_cmd request & response event handler.
 *		 Note, don't run long-time task here. 
 */
//TODO add update info to this like time took
static void app_cmd_event_handler(cmd_event_t* p_event)
{
	switch (p_event->op_code) {
	case CMD_OP_FLASH_DONE_52:
		LOG_INF("Finished Serial DFU");
		if(more_updates == UPDATE_NRF91){
			dk_set_led(DK_LED2,0);
			logik_handler();
		}
	default:
		LOG_DBG("cmd op: 0x%02x", p_event->op_code);
		break;
	}
}

/** @brief CB Function for DK Buttons*/
void button_handler(uint32_t button_state, uint32_t has_changed)
{
	if(DK_BTN1_MSK & button_state){
		//Entry Point Manual Update.
		//Alternate Entry via Selfask or SMS Notification
		//Selfask -> Timer
		//SMS -> Need Phone number comes last (nothing other than parse SMS and call askserver())
		if (m_download_busy) {
			return;
		}
		dk_set_led(DK_LED2,0);
		k_work_submit(&helper);
	}
	if(DK_BTN2_MSK & button_state){
		do_mcuboot_dfu();
	}
}

/** @brief FOTA download event handler */
static void download_event_handler(const struct fota_download_evt* evt)
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


void main(void)
{
	int rc = 0;

#if UPDATE_APP
	LOG_INF("Cross DFU Demo(new)\n");
#else 
	LOG_INF("Cross DFU Demo(ori)\n");
#endif

	rc = dk_leds_init();
	if (rc) {
		LOG_ERR("Cannot init LEDs (err: %d)", rc);
	}

#if UPDATE_APP
	dk_set_led(DK_LED4,1);
#else
	dk_set_led(DK_LED3,1);
#endif


	rc = dk_buttons_init(button_handler);
	if (rc) {
		LOG_ERR("Cant init LED Component"); //output on UART0, which is default for logging
	}

	m_uart_dev = device_get_binding(UART_DEVICE_LABLE);
	if (m_uart_dev == NULL) {
		LOG_ERR("Can't init UART component");
		goto err;
	}

	rc = http_client_init(download_event_handler);
	if (rc) {
		LOG_ERR("HTTP Client init error.");
		goto err;
	}

	LOG_INF("Connecting LTE...(don't run other tasks now)");
	rc = http_client_connect();
	if (rc) {
		LOG_ERR("Can't connect to LTE");
		goto err;
	}
	LOG_INF("LTE network is connected.");	

	// After LTE connection, LED 1 is on
	dk_set_led(DK_LED1,1);	

	LOG_INF("switch left:  single-/-double download");
	LOG_INF("switch right: 91-/-52\n");

	k_work_init(&wk_serial_dfu, wk_serial_dfu_handler);
	k_work_init(&wk_update_mcuboot_flag, wk_update_mcuboot_flag_handler);
	k_work_init(&helper,askserver);

	/* All initializations were successful mark image as working so that we
	 * will not revert upon reboot.
	 */

	boot_write_img_confirmed();
    k_sleep(K_SECONDS(2));

	//Sleep So it has Time to Init Uart
	app_cmd_init(m_uart_dev);
	
	app_cmd_event_cb_register(app_cmd_event_handler);
	
	// Add cmds
	callback_init();
	app_flash_cmd_init();

	//Fw Info
	app_cmd_add(CMD_OP_DEVINF52,NULL,rsp_dev_fwinfo52);
	app_cmd_add(CMD_OP_DEVINF92,req_dev_fwinfo92,NULL);
	
	//Start Timer if you want to test it
	//k_timer_start(&update_timer, K_SECONDS(3), K_SECONDS(3));
	//1440 Minutes -> 24 Hours 
err:
	while (true) {
		k_sleep(K_SECONDS(10));
	}
}

void update_expiry_function(struct k_timer *timer_id){
	LOG_INF("In Timer Expiry Function");
	k_work_submit(&helper);
}

K_TIMER_DEFINE(update_timer, update_expiry_function, NULL);