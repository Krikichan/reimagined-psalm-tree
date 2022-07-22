#include <zephyr.h>
#include <kernel.h>
#include <drivers/uart.h>
#include <pm/device.h>
#include <errno.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(uart_drv, 4);

#include "uart_drv.h"

#define DBG_SHOW_BUFFER_LOGIK   false    /* Enable DBG Logs Showing Buffer Adresses and block_list */

//  Important you need to use one more block than you intend to send at once 
//  because Buffer Request for new Buffer comes before releasing the buffer

#define UART_RX_BUF_NUM         MAXIMUM_BLOCK_COUNT+1
#define OOB_INDX                UART_RX_BUF_NUM+1

#define UART_RX_TIMEOUT_MS      20
#define UART_TX_TIMEOUT_MS      SYS_FOREVER_MS

//Memory Managment
static K_MEM_SLAB_DEFINE(uart_slab, UART_RX_LEN, UART_RX_BUF_NUM, 4); /* Main Memory Source for Uart Rx */
/* Holds Syntax Information about usage of the Individual blocks*/
static memory_node block_list[UART_RX_BUF_NUM];
static uint8_t block_count = 0; // Basically Cur Idx Len for block_list
static uint8_t free_entry = UART_RX_BUF_NUM+1; // Cant use negative  

//Callbacks
static uart_rx_cb  m_rx_cb;         /* RX ready interrupt callback */
static uart_tx_cb  m_tx_cb;         /* TX empty interrupt callback */

//CANNOT BE DECLARED STATIC (COMPILER ERROR) I AM SORRY
struct k_timer 		x_uart_renable_timer;

static struct device* m_device;     /* Current UART device */

// Forward Declaration
static int release_buffer(uint8_t* buff,uint8_t called_from); //called_from 0 - Uart 1- Application
static uint8_t* get_buffer();
static void reset_memory_slab();

void update_expiry_function(struct k_timer *timer_id){
	LOG_INF("Reset Uart");
	int err;
    err = uart_renable(m_device);
    if(err != 0){
        LOG_ERR("Hard Problem Enabling Uart");
        return;
    }
    k_timer_stop(&x_uart_renable_timer);

}

static void uart_callback(const struct device *dev,
              struct uart_event *evt,
              void *user_data)
{
    struct device *uart = user_data;
    int err;

    switch (evt->type) {
    case UART_TX_DONE:
        //LOG_INF("Tx sent %d bytes", evt->data.tx.len);
        if(m_tx_cb)
        {
            m_tx_cb(evt->type);
        }
        break;

    case UART_TX_ABORTED:
        LOG_ERR("Tx aborted");
        if (m_tx_cb) {
            m_tx_cb(-1);
        }
        break;

    case UART_RX_RDY:
    {
        //LOG_INF("RX_RDY");
        //In here copy data or give it to a other Process (Thread or what ever)
        //LOG_DBG("Buffer Used to receive: %p",evt->data.rx.buf);
        //LOG_DBG("Length Received %d :: Offset %d",evt->data.rx.len,evt->data.rx.offset);
        if (m_rx_cb)
        {
            m_rx_cb(evt->data.rx.buf, evt->data.rx.len,evt->data.rx.offset);
        }
        break;
    }
    case UART_RX_BUF_REQUEST:
    {   
    #if DBG_SHOW_BUFFER_LOGIK
        LOG_DBG("---------------------------------");
        LOG_DBG("BUF_REQUEST");
    #endif
		err = uart_rx_buf_rsp(uart, get_buffer(), UART_RX_LEN);
		__ASSERT(err == 0, "Failed to provide new buffer");
		break;
    }

    case UART_RX_BUF_RELEASED:
    #if DBG_SHOW_BUFFER_LOGIK
        LOG_DBG("BUF_RELEASED");
        LOG_DBG("Buffer About to be Released: %p",evt->data.rx_buf.buf);
        LOG_DBG("---------------------------------");
    #endif
        release_buffer(evt->data.rx_buf.buf,0);
        break;

    case UART_RX_DISABLED:
    //Releases both Buffers in case of Error No need to do special Error Handling here
        LOG_DBG("UART_RX_DISABLED");
        break;

    case UART_RX_STOPPED:
    //TODO Bug / Error Handler
        LOG_DBG("UART_RX_STOPPED");
        //Restart Uart Module waiting
        k_timer_start(&x_uart_renable_timer,K_SECONDS(2),K_SECONDS(2));
        break;
    }
}

/**@brief Send uart data
 *
 * @param p_data        pointer of data to be sent
 * @param length        length of data to be sent
 *
 * @return 0            success
 * @return -1           failed
 */
int uart_send(const uint8_t *p_data, uint16_t length)
{
    int err=-1;
    //LOG_DBG("%s, length:%d", __func__, length);
    
    if (m_device == NULL) {
        return err;
    }

    if (p_data != NULL && length != 0) {
        err = 0;
        err = uart_tx(m_device, p_data, length, 10000);
        if (err < 0) {
            LOG_ERR("%s, err:%d", __func__, err);
            return err;
        }
    }
    return err;
}

/**@brief Initialize app_uart module
 *
 * @param[in] p_device      pointer of UART device
 * @param[in] p_rx_buff     pointer of UART rx buffer
 * @param[in] rx_max_len    max length of rx buffer
 *
 * @return 0                success
 * @return -1               failed
 */

int uart_init(const struct device* p_device)
{
    int err;
    if (p_device == NULL) {
        return -ENXIO;
    }

    m_device = p_device;


    m_rx_cb = NULL;
    m_tx_cb = NULL;

    //Set Async Api Uart Callback
    err = uart_callback_set(p_device, uart_callback, (void *)p_device);
    __ASSERT(err == 0, "Failed to set callback");

    //Initialize Block Register
    for(size_t i = 0; i < UART_RX_BUF_NUM ; i++){
        block_list[i].buffer_ptr=NULL;
        block_list[i].in_use_app=0;
        block_list[i].in_use_uart=0;
        block_list[i].rec_len=0;
        block_list[i].rec_offset=0;
    }

    //Power Up Uart Instance
    //pm_device_state_set(m_device, PM_DEVICE_STATE_ACTIVE, NULL, NULL);
    //k_sleep(K_MSEC(100));

    err = uart_rx_enable(m_device,get_buffer(),UART_RX_LEN, UART_RX_TIMEOUT_MS);
    __ASSERT(err == 0, "Failed to enable RX");

    return 0;
}
int uart_renable(const struct device* p_device){
    int err;

    if (p_device == NULL) {
        LOG_ERR("UART must be Device");
        return -ENXIO;
    }

    m_device = p_device;
    //Reset Memory

    reset_memory_slab();

    //Set Async Api Uart Callback
    err = uart_callback_set(p_device, uart_callback, (void *)p_device);
    __ASSERT(err == 0, "Failed to set callback");
    
        err = uart_rx_enable(m_device,get_buffer(),UART_RX_LEN, UART_RX_TIMEOUT_MS);
    __ASSERT(err == 0, "Failed to enable RX");

    return 0;
}
/**@brief Un-initialize app_uart module */
void uart_uninit(void)
{

    if (m_device == NULL) {
        return;
    }

    uart_rx_cb_set(NULL);
    uart_tx_cb_set(NULL);
    
    /* Power off UART module */
    uart_rx_disable(m_device);
    //k_sleep(K_MSEC(100));
    //err = pm_device_state_set(m_device, PM_DEVICE_STATE_OFF, NULL, NULL);
    //if (err) {
    //    LOG_WRN("Can't power off uart: %d", err);
    //}
    reset_memory_slab();
    LOG_DBG("%s done", __func__);
}

/**@brief Set rx data ready event callback */
void uart_rx_cb_set(uart_rx_cb cb)
{
    m_rx_cb = cb;
}

/**@brief Set tx empty event callback */
void uart_tx_cb_set(uart_tx_cb cb)
{
    m_tx_cb = cb;
}

void uart_rx_reset(){
    //Reset All Buffer
    reset_memory_slab();
}


static uint8_t* get_buffer(){
    int err = 0;
    uint8_t idx;
    // Do Swap IDX mostly relevant for CMD Receive
    if(free_entry != OOB_INDX){
        idx = free_entry;
        free_entry = OOB_INDX;
    }else{
        idx = block_count;
    }
    //Sets Pointer to Buffer directly into block list

    err = k_mem_slab_alloc(&uart_slab, (void **)&(block_list[idx].buffer_ptr), K_NO_WAIT);
	__ASSERT(err == 0, "Failed to alloc slab");
    block_count++;
    block_list[idx].in_use_uart = 1;

#if DBG_SHOW_BUFFER_LOGIK
    LOG_DBG("Allocate at idx %d",idx);
    LOG_DBG("Allocated Pointer %p || Blocks Allocated %d",block_list[idx].buffer_ptr,block_count);
#endif

    return block_list[idx].buffer_ptr; // Increment Counter After Usage
}

//Error Codes -1 Buffer not Found -2 Buffer Still in Use
static int release_buffer(uint8_t* buff,uint8_t called_from){

    uint8_t found_idx=UART_RX_BUF_NUM;

    //Find Buffer
    for(uint8_t i = 0; i < UART_RX_BUF_NUM ; i++){
#if DBG_SHOW_BUFFER_LOGIK
        LOG_DBG("idx:%d Buffer in Registry: %p",i ,block_list[i].buffer_ptr);
#endif
        if(block_list[i].buffer_ptr == buff){
            found_idx = i;
            break;
        }
    }

    //Check if Buffer is existent
    if(found_idx == UART_RX_BUF_NUM){
        LOG_ERR("Buffer not in Registry");
        return -1;
    }
    

    //Set usage 0- Uart -1 Application
    if(called_from){
        block_list[found_idx].in_use_app = 0;
    }else{
        block_list[found_idx].in_use_uart = 0;
    }

    //Check if Buffer is in Use (Mainly Fails here cuz the Block is used for Data Streaming)
    if(block_list[found_idx].in_use_app || block_list[found_idx].in_use_uart){ //If either is Activ
        LOG_INF("Buffer Still in Use");
        return -2;
    }

    //Free Memory Block
    k_mem_slab_free(&uart_slab, (void **)&buff);
    --block_count;
    free_entry = found_idx;

#if DBG_SHOW_BUFFER_LOGIK
    LOG_DBG("Block Count %d",block_count);
#endif
    //Reset Entry
    block_list[found_idx].buffer_ptr=NULL;
    block_list[found_idx].rec_len=0;
    block_list[found_idx].rec_offset=0;

    return 0;
}

static void reset_memory_slab(){

    LOG_DBG("Reseting Memory Slab, Block Count %d",block_count);

    for(uint16_t i = 0; i < UART_RX_BUF_NUM ; i++){
        if(block_list[i].buffer_ptr != NULL){

            //Free Used Block
#if DBG_SHOW_BUFFER_LOGIK
            LOG_DBG("Free Slab , idx %d",i);
#endif
            k_mem_slab_free(&uart_slab, (void **)&(block_list[i].buffer_ptr));

            //Reset All entrys
            block_list[i].buffer_ptr=NULL;
            block_list[i].in_use_app=0;
            block_list[i].in_use_uart=0;
            block_list[i].rec_len=0;
            block_list[i].rec_offset=0;
        }
    }
    block_count = 0;
    free_entry = -1;
}

K_TIMER_DEFINE(x_uart_renable_timer, update_expiry_function, NULL);