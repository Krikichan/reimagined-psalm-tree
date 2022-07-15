/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <dk_buttons_and_leds.h>
#include <logging/log.h>
#include <device.h>

#include "simp.h"
#include "uart_drv.h"

#define CMD_OP_SEND_DATA 0x66

static uint8_t helper_rx[1024] = {0,1,2,3,4,5,6,7,8,9};

static struct device* m_uart_dev;

LOG_MODULE_REGISTER(main, 3);

static uint8_t i = 0;

/** @brief CB Function for DK Buttons*/
void button_handler(uint32_t button_state, uint32_t has_changed)
{
	if(DK_BTN1_MSK & button_state){
		app_cmd_request(CMD_OP_SEND_DATA,helper_rx,1030);
	}
	if(DK_BTN2_MSK & button_state){

		if(i == 0){
			app_cmd_disable();
			i =1;
		}else{
			app_cmd_init(m_uart_dev);
			i=0;
		}
	}
}
//size 4 8 die bytes 0 minor fw 1 major fw num
int req_send_data(uint8_t* p_req, uint16_t req_len, cmd_respond_t respond)
{
	LOG_INF("Request Received Length %hu",req_len);

	LOG_INF("First Byte of Request %x Second Byte %x",p_req[0],p_req[1]);

    respond(helper_rx,10);

    return 0;
}

//size 4 8 die bytes 0 minor fw 1 major fw num
void rsp_send_data(uint8_t* p_rsp, uint16_t rsp_len)
{
	LOG_INF("Response Received Length %hu",rsp_len);
	
	for(uint16_t i = 0 ; i < rsp_len ; ++i){
		printk("%x",p_rsp[i]);
	}
	printk("\n");
}

void main(void)
{
	LOG_INF("Hello World! %s\n", CONFIG_BOARD);
	int err;

	m_uart_dev = device_get_binding("UART_1");

	if (m_uart_dev == NULL) {
		LOG_ERR("Can't init UART component");
		return;
	}

	err = dk_buttons_init(button_handler);
	if (err) {
		LOG_ERR("Cant init LED Component");
	}

	app_cmd_init(m_uart_dev);
	
    app_cmd_add(CMD_OP_SEND_DATA,req_send_data,rsp_send_data);

	for(;;){
		k_cpu_idle();
	}
}
