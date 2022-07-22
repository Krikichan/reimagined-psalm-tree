#ifndef DFU_MODUL_H__
#define DFU_MODUL_H__

#ifdef __cplusplus
extern "C" {
#endif


//Settings
#define AUTO_REBOOT false 		//If Activated Automatically Reboots Device
    //Todo Tell BT Central that Auto_Reboot is Enabled

/*--------------------------------------------------------------------------*/
/*	 Commands and other Definitions	*/
/*--------------------------------------------------------------------------*/
			/*	 UART Transmission Defines	*/

#define IMG_PACKET_SIZE          128 // Referers to Bluetooth MTU Packet Size
#define IMG_BLOCK_SIZE           (IMG_PACKET_SIZE * 8) //1024
#define CMD_WRITE_ADDR_SIZE      4
#define CMD_WRITE_LEN_SIZE       4
#define CMD_WRITE_HEADER_SIZE    (CMD_WRITE_ADDR_SIZE + CMD_WRITE_LEN_SIZE)
#define CMD_BUFFER_SIZE          (IMG_BLOCK_SIZE + CMD_WRITE_HEADER_SIZE)  // 1032 address + length + image block data
/*--------------------------------------------------------------------------*/
			/*	 CMD Codes	*/

#define CMD_OP_DEVINF52				0x01
#define CMD_OP_DEVINF91				0x02

// app flash commands: 0x20 - 0x2F
#define CMD_OP_FLASH_52        0x20
#define CMD_OP_FLASH_INFO_52   (CMD_OP_FLASH_52 + 1)
#define CMD_OP_FLASH_READ_52   (CMD_OP_FLASH_52 + 2)
#define CMD_OP_FLASH_WRITE_52  (CMD_OP_FLASH_52 + 3)
#define CMD_OP_FLASH_ERASE_52  (CMD_OP_FLASH_52 + 4)
#define CMD_OP_FLASH_CRC_52    (CMD_OP_FLASH_52 + 5)
#define CMD_OP_FLASH_START_52  (CMD_OP_FLASH_52 + 6)
#define CMD_OP_FLASH_DONE_52   (CMD_OP_FLASH_52 + 7)
	
// app flash commands 91: 0x30 - 0x2F#
#define CMD_OP_FLASH_91        0x30
#define CMD_OP_FLASH_INFO_91   (CMD_OP_FLASH_91 + 1)
#define CMD_OP_FLASH_READ_91   (CMD_OP_FLASH_91 + 2)
#define CMD_OP_FLASH_WRITE_91  (CMD_OP_FLASH_91 + 3)
#define CMD_OP_FLASH_ERASE_91  (CMD_OP_FLASH_91 + 4)
#define CMD_OP_FLASH_CRC_91    (CMD_OP_FLASH_91 + 5)
#define CMD_OP_FLASH_START_91  (CMD_OP_FLASH_91 + 6)
#define CMD_OP_FLASH_DONE_91   (CMD_OP_FLASH_91 + 7)
/*--------------------------------------------------------------------------*/
			/* Peripheral Bluetooth */

#define REQ_START_DFU            0x00
#define REQ_GET_IMG_INFO         0x01       // get image size and what to update
#define REQ_GET_IMG_DATA         0x02       // get a packet of data

#define REG_IMG_DATA_OFFSET		 0x03		// Start getting data from Offset(used if process stopped suddenly)
#define REQ_GET_IMG_INIT		 0x04		// init Packet for identifying not implemented now
#define REQ_RESTART_PROCESS		 0x05		// ReStart Process (Just Resets it)
#define REQ_FAILED_PROCESS		 0x06		// If DFU Process unrecoverably Fails

#define REQ_TX_SUCCESS           0x07       // Marks TX as Succesfull
#define REQ_UPDATE_SELF			 0x08		// Restart Process Updating nRF52
#define RSP_START_SELF_UPDATE	 0x0A		// Tells Central to start Self Update Process (after NRF91 Update)
#define	REQ_DEVINF				 0x09		//Helper Signaling that BLuetooth Central should ask for Dev Info
#define RSP_DEVINF				 0x20 		//Response Sending FW Num and Size of Both Devices to Bluetooth Central

#define CUT_CONNECTION 			 0xFF		//Tells Central to cut Connection
#define RSP_DONE_UPDATING		 0xFE		//Central Marks Updating as done
/*--------------------------------------------------------------------------*/
	//Codes zur Identifizierung was Geupdated werden soll

#define IMAGE_TYPE_NRF52		0           /* SDK DFU file for nrf52 firmware */
#define IMAGE_TYPE_NRF91		1           /* Mcuboot file for nrf91 firmware */
#define IMAGE_TYPE_MODEM		2           /* Mcuboot file for nrf91 modem */
#define IMAGE_TYPE_ERROR		(-1)        /* Unknown file type */

#define UPDATE_NRF52				0x00
#define UPDATE_NRF91				0x01
#define UPDATE_BOTH 				0x02
#define UPDATE_NOMORE               0x03

#define IMAGE_FROM_HTTP				1
#define IMAGE_FROM_SERIAL			2
#define IMAGE_FROM_BLUETOOTH		3
#define IM	4								//PlaceHolder as Invalid Channel

/*--------------------------------------------------------------------------*/
//				Functions
/*--------------------------------------------------------------------------*/
//			Entry Point Functons
/*--------------------------------------------------------------------------*/

/**@brief Call to either Reboot or stop Process
 */
void dfu_file_ready(uint8_t m_image_channel);

/**@brief Main CB for BT Updating
 */
void nus_received_cb_handler(struct bt_conn *conn, const uint8_t *const p_write_data,uint16_t write_data_len);

/*--------------------------------------------------------------------------*/
//	Helper Functions
/*--------------------------------------------------------------------------*/

/**@brief Tells the Connected BT Device that you will Cut the Connection
 */
int disconnect_from_Central();

/**@brief Send Data to NUS TX Characteristic
 */
uint32_t ble_send_req_conn(const struct bt_conn *conn,uint8_t req);


/*--------------------------------------------------------------------------*/
//	Module Function
/*--------------------------------------------------------------------------*/

/**@brief Initializes the entire DFU Module and Flash Resources
 */
void initalize_dfu_module();

#ifdef __cplusplus
}
#endif

#endif