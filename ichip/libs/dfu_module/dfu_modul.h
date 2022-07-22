#ifndef DFU_MODUL_H__
#define DFU_MODUL_H__

#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------*/
/*	 Commands and other Definitions	*/
/*--------------------------------------------------------------------------*/
			/*	 UART Transmission Defines	*/
#define IMG_BLOCK_SIZE           1024 //One Block of Image Data
#define CMD_WRITE_ADDR_SIZE      4
#define CMD_WRITE_LEN_SIZE       4
#define CMD_WRITE_HEADER_SIZE    (CMD_WRITE_ADDR_SIZE + CMD_WRITE_LEN_SIZE)
#define CMD_BUFFER_SIZE          (IMG_BLOCK_SIZE + CMD_WRITE_HEADER_SIZE)//1024 +8 1032 // address + length + image block data
/*--------------------------------------------------------------------------*/

			/*	 CMD Codes	*/
#define CMD_OP_DEVINF52		   0x01
#define CMD_OP_STOPBT		   0x03
// app flash commands: 0x20 - 0x2F
#define CMD_OP_FLASH_52        0x20
#define CMD_OP_FLASH_INFO_52   (CMD_OP_FLASH_52 + 1)
#define CMD_OP_FLASH_READ_52   (CMD_OP_FLASH_52 + 2)
#define CMD_OP_FLASH_WRITE_52  (CMD_OP_FLASH_52 + 3) //IU
#define CMD_OP_FLASH_ERASE_52  (CMD_OP_FLASH_52 + 4) //IU
#define CMD_OP_FLASH_CRC_52    (CMD_OP_FLASH_52 + 5)
#define CMD_OP_FLASH_START_52  (CMD_OP_FLASH_52 + 6)
#define CMD_OP_FLASH_DONE_52   (CMD_OP_FLASH_52 + 7) //IU

#define CMD_OP_DEVINF92		   0x02
// app flash commands 92: 0x30 - 0x2F#
#define CMD_OP_FLASH_92        0x30
#define CMD_OP_FLASH_INFO_92   (CMD_OP_FLASH_92 + 1)
#define CMD_OP_FLASH_READ_92   (CMD_OP_FLASH_92 + 2) 
#define CMD_OP_FLASH_WRITE_92  (CMD_OP_FLASH_92 + 3) //IU -In Use
#define CMD_OP_FLASH_ERASE_92  (CMD_OP_FLASH_92 + 4) //IU
#define CMD_OP_FLASH_CRC_92    (CMD_OP_FLASH_92 + 5)
#define CMD_OP_FLASH_START_92  (CMD_OP_FLASH_92 + 6)
#define CMD_OP_FLASH_DONE_92   (CMD_OP_FLASH_92 + 7) //IU
/*--------------------------------------------------------------------------*/

#define IMAGE_FROM_HTTP				1
#define IMAGE_FROM_SERIAL			2

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
/*--------------------------------------------------------------------------*/
//				Handler
/*--------------------------------------------------------------------------*/
//Default Handler
void download_event_handler(const struct fota_download_evt* evt);
void app_cmd_event_handler(cmd_event_t* p_event);

/*--------------------------------------------------------------------------*/
//				Functions
/*--------------------------------------------------------------------------*/
//	Enty Point Functons
/*--------------------------------------------------------------------------*/

//Default Parser
//Sms Received Function Parse Buffer into it 
//Length should be tied to what to update and how much to update
//TODO add support for downloading differently named fw
void parse_sms(uint8_t *buffer,uint16_t len);

/**@brief Asks Server for new Firmware
 *  Worker Objekt callable only via submit to queue
 */
void askserver(struct k_work* unused);

/*--------------------------------------------------------------------------*/
//	Helper Functions
/*--------------------------------------------------------------------------*/
/**@brief Reboots Chip
 * Todo Signal that to the other Chip
 */
void do_mcuboot_dfu();

/**@brief Retrieves FW Version and Available Space from Both Chips
 */
void get_device_status();

/**@brief Tells you wether or not the Module is currently activ
 */
bool module_active();

/*--------------------------------------------------------------------------*/
//	Module Function
/*--------------------------------------------------------------------------*/

/**@brief Initializes the entire DFU Module including HTTP Client and Flash Resources
 */
void initalize_dfu_module();

void reenable_dfu_module();

void disable_dfu_module();

#ifdef __cplusplus
}
#endif

#endif