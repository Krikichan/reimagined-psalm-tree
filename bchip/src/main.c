/*10
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

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

#include "app_cmd.h"
#include "app_flash.h"
#include "app_flash_cmd.h"
#include "ble_dfu_helper.h"

#include "blecmds.h"


LOG_MODULE_REGISTER(main,3);

#define DEVICE_NAME "hello"
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)

#define UART_DEVICE_LABEL "UART_0"

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

#define CON_STATUS_LED DK_LED2

#define UPDATE_APP 					false


static struct device *uart;

static struct bt_conn *current_conn;

static K_SEM_DEFINE(ble_init_ok, 0, 1);

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

	//k_sem_give(&ble_init_ok);
}



/**@brief Do mcuboot dfu for 52 */
void do_mcuboot_dfu(void)
{
    sys_reboot(SYS_REBOOT_WARM);
}

/**@brief app_cmd request & response event handler.
 *		 Note, don't run long-time task here. 
 */
static void app_cmd_event_handler(cmd_event_t* p_event)
{
	static uint32_t start_time;
	uint32_t total_time;
	uint32_t speed_integer;
	uint32_t speed_fraction;
	uint32_t  m_image_file_size;
	
	switch (p_event->op_code) {
	case CMD_OP_FLASH_INFO_52:
		LOG_INF("Start to receive DFU image by UART");
		start_time = k_uptime_get_32();
		break;

	case CMD_OP_FLASH_DONE_52:
		LOG_INF("DFU image is received by UART");
		m_image_file_size = sys_get_le32(p_event->p_data);
		LOG_INF("File size is: %d", m_image_file_size);	

		total_time = k_uptime_get_32() - start_time;
		speed_integer = m_image_file_size / total_time;
		speed_fraction = (m_image_file_size % total_time) * 100 / total_time;

		LOG_INF("UART transferring speed: %d.%d kB/s", speed_integer, speed_fraction);

		dfu_file_ready(IMAGE_FROM_SERIAL);
		break;

	default:
		LOG_DBG("cmd op: 0x%02x", p_event->op_code);
		break;
	}
}


void button_handel(uint32_t button_state,uint32_t has_changed){
	if(DK_BTN2 & button_state){
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

	//err = bt_enable(bt_ready);
	if (err) {
		error();
	}

	//err = k_sem_take(&ble_init_ok, K_MSEC(100));
	//Need to wait here
	//k_sem_give(&ble_init_ok);
	/* All initializations were successful mark image as working so that we
	 * will not revert upon reboot.
	 */
	boot_write_img_confirmed();


	app_cmd_init(uart);
	app_cmd_event_cb_register(app_cmd_event_handler);


	module_init();
	app_flash_cmd_init(); //bl_flash_cb Declared in DFU_BL_Helper.h

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

