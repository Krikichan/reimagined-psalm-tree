/** @file
 *  @brief Bluetooth Application Chip Programm Enabling DFU of the Chip Structure
 */
#include <zephyr/types.h>
#include <zephyr.h>
#include <drivers/uart.h>
#include <usb/usb_device.h>

#include <sys/reboot.h>

#include <device.h>
#include <soc.h>
#include <sys/byteorder.h>
#include <dfu/dfu_target.h>
#include <dfu/mcuboot.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>

#include <bluetooth/services/nus.h>

#include <dk_buttons_and_leds.h>

#include <settings/settings.h>

#include <stdio.h>

#include <logging/log.h>
#include <dfu/mcuboot.h>

#include "simp.h"
#include "app_flash.h"
#include "dfu_module_52.h"

LOG_MODULE_REGISTER(main,3);
//Bluetooth Config
#define DEVICE_NAME "Appli"
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)

#define CMD_OP_GO_TO_SLEEP			0xC0

static K_SEM_DEFINE(ble_init_ok, 0, 1);

#define UART_DEVICE_LABEL "UART_1"

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

#define CON_STATUS_LED DK_LED2

#define UPDATE_APP 					false

static struct device *uart;

static struct bt_conn *current_conn;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};


static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	struct bt_conn_info info; 	
	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;

	}	else if(bt_conn_get_info(conn, &info)){
		printk("Could not parse connection info\n");
		return;
	}else{

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Connected %s", log_strdup(addr));
	LOG_INF("Connection established!		\n\
		Connected to: %s					\n\
		Role: %u							\n\
		Connection interval: %u				\n\
		Slave latency: %u					\n\
		Connection supervisory timeout: %u	"
		, addr, info.role, info.le.interval, info.le.latency, info.le.timeout);
	}
	

	current_conn = bt_conn_ref(conn);

	dk_set_led_on(CON_STATUS_LED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason %u)", log_strdup(addr), reason);


	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
		dk_set_led_off(CON_STATUS_LED);
	}
}

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	//If acceptable params, return true, otherwise return false.
	return true; 
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout)
{
	struct bt_conn_info info; 
	char addr[BT_ADDR_LE_STR_LEN];
	
	if(bt_conn_get_info(conn, &info))
	{
		printk("Could not parse connection info\n");
	}
	else
	{
		bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
		
		printk("Connection parameters updated!	\n\
		Connected to: %s						\n\
		New Connection Interval: %u				\n\
		New Slave Latency: %u					\n\
		New Connection Supervisory Timeout: %u	\n"
		, addr, info.le.interval, info.le.latency, info.le.timeout);
	}
}



static struct bt_conn_cb conn_callbacks = {
	.connected		    = connected,
	.disconnected   	= disconnected,
	.le_param_req		= le_param_req,
	.le_param_updated	= le_param_updated
};


static struct bt_nus_cb nus_cb = {
	.received = nus_received_cb_handler
};

void error(void)
{
	dk_set_leds_state(DK_ALL_LEDS_MSK, DK_NO_LEDS_MSK);

	while (true) {
		/* Spin for ever */
		k_sleep(K_MSEC(1000));
	}
}

/**@brief app_cmd request & response event handler.
 *		 Note, don't run long-time task here. 
 */
static void app_cmd_event_handler(cmd_event_t* p_event)
{

	switch (p_event->op_code) {
	case CMD_OP_FLASH_INFO_52:
		LOG_INF("Start to receive DFU image by UART");
		break;

	case CMD_OP_FLASH_DONE_52:
		LOG_INF("DFU image is received by UART");
		LOG_INF("File size is: %d", sys_get_le32(p_event->p_data));	
		dfu_file_ready(IMAGE_FROM_SERIAL);

		break;
	default:
		LOG_DBG("cmd op: 0x%02x", p_event->op_code);
		break;
	}
}

void rsp_nrf91_sleep(uint8_t* p_rsp, uint16_t rsp_len){
	LOG_INF("NRF91 is now Sleepy");
	//Todo Maybe remember that nrf91 is sleeping -> boolean
}

static void bt_ready(int err)
{
	LOG_INF("BT Host Stack enabled");
	if (err) {
		printk("BLE init failed with error code %d\n", err);
		return;
	}
	
	bt_conn_cb_register(&conn_callbacks);
	
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}
	
	err = bt_nus_init(&nus_cb);// ->dfu_helper file

	if (err) {
		LOG_ERR("Failed to initialize UART service (err: %d)", err);
		return;
	}
    //Start advertising
	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd,
			      ARRAY_SIZE(sd));

	if (err) 
	{
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	k_sem_give(&ble_init_ok);
}


/**@brief Do mcuboot dfu for 52 */
void do_mcuboot_dfu(void)
{
	LOG_INF("Rebooting ....");
	sys_reboot(SYS_REBOOT_WARM);
}

void button_handel(uint32_t button_state,uint32_t has_changed){
	int err;
	if(DK_BTN1_MSK & button_state){
	}
	if(DK_BTN2_MSK & button_state){
		do_mcuboot_dfu();
	}
}

void main(void)
{

	int err = 0;

#if UPDATE_APP
	LOG_INF("Cross DFU Demo(new)");
#else
	LOG_INF("Cross DFU Demo(ori)");
#endif

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}
	err = dk_buttons_init(button_handel);
	if (err) {
		LOG_ERR("Cannot init Buttons (err: %d)", err);
	}

#if UPDATE_APP
	err = dk_set_led(DK_LED4,1);
#else
	err = dk_set_led(DK_LED3,1);
#endif
	if(err){
		LOG_ERR("Set Led err %d",err);
	}

	uart = device_get_binding(UART_DEVICE_LABEL);
	if (!uart) {
		LOG_ERR("Cant init Uart Component");
		return;
	}

	err = bt_enable(bt_ready);
	if (err) {
		error();
	}

	err = k_sem_take(&ble_init_ok, K_MSEC(100));
	//Need to wait here
	k_sem_give(&ble_init_ok);
	/* All initializations were successful mark image as working so that we
	 * will not revert upon reboot.
	 */
	boot_write_img_confirmed();

    app_cmd_init(uart);
	app_cmd_event_cb_register(app_cmd_event_handler);

	app_cmd_add(CMD_OP_GO_TO_SLEEP,NULL,NULL);

	initalize_dfu_module();

	LOG_INF("Init completed, Idling till Update or Connect");
	
	//Forever Loop :^)
	for (;;) {
		k_sleep(K_SECONDS(10));
	}
}

//TODO Add Options for App like seeing Cur FW Num Available Size let User Decide wether to Update or not
//-> have to send current 
//-> Make own Service for communication 
//-> Use Nordic Uart only for FWData Transfer

//If In Production write a extension for NUS so you dont have to different services rather more attributes

