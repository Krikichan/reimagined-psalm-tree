#ifndef UART_DRV_H__
#define UART_DRV_H__

#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAXIMUM_BLOCK_COUNT     3
#define UART_RX_LEN             1040 //IMPORTANT: This should be the maximum you want to receive Compare with Send length in your dfu module
//Standard is 1024 Bytes Data + 8 Relational Data + 8 For SIMP

/** @typedef memory node for asynchronous use of blocks from memory slab*/
typedef struct
{
    uint8_t*    buffer_ptr;			/* Pointer of data */
    uint16_t 	rec_offset;			/* rec_offset  Necessary ??*/
    uint16_t    rec_len;            /* rec_offset  Necessary ??*/
    uint8_t     in_use_uart;        /* check wether or not was already released*/
    uint8_t     in_use_app;        /* check wether or not already read or used(for Data Streams)*/
} memory_node;

/** @typedef uart receiving callback function type
 * @detail Callback is responsible for Copying RX Data
 * @param[in] p_buffer 	pointer of data buffer
 * @param[in] length 	data length
 * @param[in] offset    offset from where to read Data
 */
typedef void (*uart_rx_cb)(uint8_t* p_data, uint16_t length, uint16_t offset);

/** @typedef uart sending callback function type 
 *
 * @param[in] event     tx event id. 0: success, -1: failed.
 */
typedef void (*uart_tx_cb)(int event);

/** @brief Initialize app_uart module
 *
 * @param[in] p_device 		pointer of UART device
 * 
 * @return 0				success
 * @return -1				failed
 */
int uart_init(const struct device* p_device);

/** @brief Renable Uart (Used by Timer)
 *
 * @param[in] p_device 		pointer of UART device
 * 
 * @return 0				success
 * @return -1				failed
 */
int uart_renable(const struct device* p_device);

/** @brief Un-initialize app_uart module */
void uart_uninit(void);

/** @brief Send uart data
 *
 * @param p_data		pointer of data to be sent
 * @param length		length of data to be sent
 *
 * @return 0			success
 * @return -1			failed
 */
int uart_send(const uint8_t* p_data, uint16_t length);

/**@brief Reset rx buffer */
void uart_rx_reset(void);

/**@brief Set rx data ready event callback */
void uart_rx_cb_set(uart_rx_cb cb);

/**@brief Set tx empty event callback */
void uart_tx_cb_set(uart_tx_cb cb);

void uart_rx_reset();

#ifdef __cplusplus
}
#endif

#endif /* UART_DRV_H__ */
