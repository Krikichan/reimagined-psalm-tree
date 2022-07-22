#include <string.h>
#include <kernel.h>
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
#include <modem/sms.h>

#include <dk_buttons_and_leds.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(main, 3);


#include "app_flash.h"

//Necessary simp Module
#include "simp.h"

//Dfu Module
#include "http_client.h"
#include "int_dfu_helper.h"
#include "dfu_modul.h"

// Auf true setzen um Update Image zuerzeugen. NICHT FLASHEN !
#define UPDATE_APP 					false

//CMDS After 0xC0 - 0XFF for Application use

#define CMD_OP_GO_TO_SLEEP			0xC0

struct k_timer 		update_timer; //Cant be Declared static

static struct k_work helper;
static struct k_work sleep;

//Todo Clean up and move to dfu_module.h
static void sms_callback(struct sms_data *const data, void *context)
{
	if (data == NULL) {
		LOG_INF("%s with NULL data\n", __func__);
		return;
	}
	if (data->type == SMS_TYPE_DELIVER) {
		/* When SMS message is received, print information */
		struct sms_deliver_header *header = &data->header.deliver;

		LOG_INF("SMS received:");
		LOG_INF("Time:   %02d-%02d-%02d %02d:%02d:%02d",
			header->time.year,
			header->time.month,
			header->time.day,
			header->time.hour,
			header->time.minute,
			header->time.second);

		LOG_INF("Text:   '%s'", data->payload);
		parse_sms(data->payload,data->payload_len);

	} else {
		LOG_INF("SMS protocol message with unknown type received");
	}
}

static int req_sleep(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond)
{
	int err = 0;
	LOG_INF("Request to go Sleepy Time");

	k_work_submit(&sleep);

	respond(NULL,0);

	return err;
}


/** @brief CB Function for DK Buttons*/
void button_handler(uint32_t button_state, uint32_t has_changed)
{
	if(DK_BTN1_MSK & button_state){
		//Entry Point Manual Update.
		//Alternate Entry via Timer or SMS Notification
		dk_set_led(DK_LED2,0);
		k_work_submit(&helper);
	}
}

//Sends Chip to Sleep
void try_to_sleep(struct k_work* unused){
	//Disable SMS Receiver and Timer
	disable_dfu_module();

	//Disable SIMP Module
	app_cmd_disable();

	//Send to Sleep via Zephyr Command
	//pm_system_suspend(); //Todo Add Wakeup Source
}

void main(void)
{
	int err = 0;

#if UPDATE_APP
	LOG_INF("Cross DFU Demo(new)\n");
#else 
	LOG_INF("Cross DFU Demo(ori)\n");
#endif

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Cannot init LEDs (err: %d)", err);
	}

#if UPDATE_APP
	dk_set_led(DK_LED4,1);
#else
	dk_set_led(DK_LED3,1);
#endif

	LOG_INF("switch left:  single-/-double download");
	LOG_INF("switch right: 91-/-52\n");

	err = dk_buttons_init(button_handler);
	if (err) {
		LOG_ERR("Cant init LED Component");
	}

	const struct device* m_uart_dev = device_get_binding("UART_1");
	if (m_uart_dev == NULL) {
		LOG_ERR("Can't init UART component");
		return;
	}
	//Initialize Uart before dfu_module
	app_cmd_init(m_uart_dev);
	app_cmd_event_cb_register(app_cmd_event_handler);

	app_cmd_add(CMD_OP_GO_TO_SLEEP,req_sleep,NULL);

	initalize_dfu_module();

	// After LTE connection, LED 1 is on
	dk_set_led(DK_LED1,1);	
	
	//Yes double init i dont know why you have to do that but only then it works
	err = sms_register_listener(sms_callback, NULL);
	if (err) {
		LOG_INF("sms_register_listener returned err: %d", err);
		return;
	}

	sms_unregister_listener(0);

	k_work_init(&helper,askserver);
	k_work_init(&sleep,try_to_sleep);

	err = sms_register_listener(sms_callback, NULL);
	if (err) {
		LOG_INF("sms_register_listener returned err: %d", err);
		return;
	}
	//Enables GPIO As Wake Up Source
	//pm_device_wakeup_enable();
	//Todo enable GPIO

	/* All initializations were successful mark image as working so that we
	 * will not revert upon reboot.
	 */
	boot_write_img_confirmed();
 
	//Start Timer if you want to test it
	//k_timer_start(&update_timer, K_SECONDS(3), K_SECONDS(3));
	//1440 Minutes -> 24 Hours use K_MINUTES()
	//get_device_status();
	LOG_INF("All Initialized Starting Idle Loop");
	while (true) {
		k_sleep(K_SECONDS(10));
	}
}

void timer_expiry(struct k_timer *timer_id){
	LOG_INF("In Timer Expiry Function");
	k_work_submit(&helper);
}

K_TIMER_DEFINE(update_timer, timer_expiry, NULL);