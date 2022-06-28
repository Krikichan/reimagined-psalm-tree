#ifndef BLECMDS_H__
#define BLECMDS_H__

#ifdef __cplusplus
extern "C" {
#endif
	
//Peripheral
#define IMG_PACKET_SIZE          128
#define PKTS_PER_BLOCK           8
#define IMG_BLOCK_SIZE           (IMG_PACKET_SIZE * PKTS_PER_BLOCK) //1024

#define CMD_WRITE_ADDR_SIZE      4
#define CMD_WRITE_LEN_SIZE       4
#define CMD_WRITE_HEADER_SIZE    (CMD_WRITE_ADDR_SIZE + CMD_WRITE_LEN_SIZE)
#define CMD_BUFFER_SIZE          (IMG_BLOCK_SIZE + CMD_WRITE_HEADER_SIZE)       // address + length + image block data
									//1032
#define REQ_START_DFU            0x00
#define REQ_GET_IMG_INFO         0x01       // get image size and what to update
#define REQ_GET_IMG_DATA         0x02       // get a packet of data

#define REG_IMG_DATA_OFFSET		 0x03		// Start getting data from Offset(used if process stopped suddenly)
#define REQ_GET_IMG_INIT		 0x04		// init Packet for identifying not implemented now
#define REQ_RESTART_PROCESS		 0x05		// ReStart Process (Just Resets it)
#define REQ_FAILED_PROCESS		 0x06		// If DFU Process unrecoverably Fails

#define REQ_TX_SUCCESS           0x07       // Marks TX as Succesfull
#define REQ_UPDATE_SELF			 0x08		// Restart Process Updating nRF52

#define	REQ_DEVINF				 0x09		//Helper Signaling that BLuetooth Central should ask for Dev Info
#define RSP_DEVINF				 0x20 		//Response Sending FW Num and Size of Both Devices to Bluetooth Central

#define IMAGE_FROM_HTTP				1
#define IMAGE_FROM_SERIAL			2
#define IMAGE_FROM_BLUETOOTH		3
#define IM	4								//PlaceHolder as Invalid Channel
	
#define IMAGE_TYPE_NRF52		0           /* SDK DFU file for nrf52 firmware */
#define IMAGE_TYPE_NRF91		1           /* Mcuboot file for nrf91 firmware */
#define IMAGE_TYPE_MODEM		2           /* Mcuboot file for nrf91 modem */
#define IMAGE_TYPE_ERROR		(-1)        /* Unknown file type */

#define CMD_OP_DEVINF52				0x01
#define CMD_OP_DEVINF91				0x02

#ifdef __cplusplus
}
#endif


#endif