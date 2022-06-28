#include <zephyr.h>
#include <kernel.h>
#include <drivers/uart.h>
#include <pm/device.h>
#include <errno.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(app_uart, 3);

#include "app_uart.h"


#define UART_RX_BUF_NUM         2
#define UART_RX_LEN             1040
#define UART_RX_TIMEOUT_MS      10
#define UART_TX_TIMEOUT_MS      SYS_FOREVER_MS

static uint8_t s_uart_rx_buf[UART_RX_BUF_NUM][UART_RX_LEN];
static uint8_t *s_next_buf = s_uart_rx_buf[1];

static uart_buff_t m_rx_buff;       /* RX buffer */
static uart_rx_cb  m_rx_cb;         /* RX ready interrupt callback */
static uart_tx_cb  m_tx_cb;         /* TX empty interrupt callback */

static struct k_work wk_reinitalize;
static struct device* m_device;     /* Current UART device */


/**@brief Send uart data
 *
 * @param p_data        pointer of data to be sent
 * @param length        length of data to be sent
 *
 * @return 0            success
 * @return -1           failed
 */
int app_uart_send(const uint8_t *p_data, uint16_t length)
{
    int err;
    LOG_ERR("%s, length:%d", __func__, length);
    
    if (m_device == NULL) {
        return -1;
    }

    if (p_data != NULL && length != 0) {
        err = uart_tx(m_device, p_data, length, UART_TX_TIMEOUT_MS);
    }
    if (err < 0) {
        LOG_ERR("%s, err:%d", __func__, err);
    }
    return err;
}

void wk_uart_reinit_handler(struct k_work * unused){
    LOG_INF("Reinitializing Uart");
    int err;
    err = app_uart_init(m_device,m_rx_buff.p_data,m_rx_buff.max_len);
    if(err){
        LOG_ERR("Error in APP Uart Reinit: %d",err);
    }
}

static void uart_callback(const struct device *dev,
              struct uart_event *evt,
              void *user_data)
{
    struct device *uart = user_data;
    int err;

    switch (evt->type) {
    case UART_TX_DONE:
        LOG_INF("Tx sent %d bytes", evt->data.tx.len);
        if(m_tx_cb)
        {
            m_tx_cb(evt->type);
        }
        break;

    case UART_TX_ABORTED:
        if (m_tx_cb) {
            m_tx_cb(-1);
        }
        break;

    case UART_RX_RDY:
        LOG_INF("Received data len:%d bytes,offset:%d", evt->data.rx.len, evt->data.rx.offset);
        memcpy(&m_rx_buff.p_data[m_rx_buff.length], &evt->data.rx.buf[evt->data.rx.offset], 
                evt->data.rx.len);
        m_rx_buff.length += evt->data.rx.len;
        
        if (m_rx_cb)
        {
            m_rx_cb(m_rx_buff.p_data, evt->data.rx.len);
        }
        break;

    case UART_RX_BUF_REQUEST:
    {
        LOG_INF("UART_RX_BUF_REQUEST");

        err = uart_rx_buf_rsp(uart, s_next_buf, sizeof(s_uart_rx_buf[0]));
        if (err == EBUSY) {
            LOG_WRN("UART RX buf rsp: EBUSY");
        }else if(err ==EACCES){
            LOG_WRN("UART RX buf rsp: EACCES");
        }else if(err){
            LOG_WRN("UART RX buf rsp: %d",err);
        }
        break;
    }

    case UART_RX_BUF_RELEASED:
        LOG_INF("UART_RX_BUF_RELEASED");
        s_next_buf = evt->data.rx_buf.buf;
        break;

    case UART_RX_DISABLED:
        LOG_INF("UART_RX_DISABLED");
        break;

    case UART_RX_STOPPED:
        LOG_INF("UART_RX_STOPPED");

        k_work_submit(&wk_reinitalize);
        break;
    }
}

static int uart_receive(void)
{
    int ret;

    ret = uart_rx_enable(m_device, s_uart_rx_buf[0], sizeof(s_uart_rx_buf[0]), UART_RX_TIMEOUT_MS);
    s_next_buf = s_uart_rx_buf[0];
    if (ret) {
        LOG_ERR("%s failed: %d", __func__, ret);
        return ret;
    }
    app_uart_rx_reset();

    return 0;
}

/*
int poweroff_uart(void)
{
    int err=0;

    uart_rx_disable(m_device);
    //k_sleep(K_MSEC(100));
    //err = pm_device_state_set(m_device, PM_DEVICE_STATE_OFF, NULL, NULL);
    //if (err) {
    //    LOG_ERR("Can't power off uart: %d", err);
    //}

    return err;
}

int poweron_uart(void)
{
  int err;
  uint32_t current_state = 0;

  err = pm_device_state_get(m_device, &current_state);
  if (err) {
      LOG_ERR("Device get power_state: %d", err);
      return err;
  }

  if (current_state != PM_DEVICE_STATE_ACTIVE) {
      pm_device_state_set(m_device, PM_DEVICE_STATE_ACTIVE, NULL, NULL);
      //k_sleep(K_MSEC(100));
      err = uart_receive();
  }

  return err;
}
*/

/**@brief Initialize app_uart module
 *
 * @param[in] p_device      pointer of UART device
 * @param[in] p_rx_buff     pointer of UART rx buffer
 * @param[in] rx_max_len    max length of rx buffer
 *
 * @return 0                success
 * @return -1               failed
 */
uint8_t helper_bool = true;
int app_uart_init(struct device* p_device, uint8_t* p_rx_buff,
        uint16_t rx_max_len)
{

    if (p_device == NULL) {
        return -ENXIO;
    }

    m_device = p_device;

    m_rx_buff.p_data = p_rx_buff;
    m_rx_buff.max_len = rx_max_len;
    m_rx_buff.length = 0;

    m_rx_cb = NULL;
    m_tx_cb = NULL;

    if(helper_bool){
        //Called Once
        k_work_init(&wk_reinitalize,wk_uart_reinit_handler);
        helper_bool = false;
    }

    int err;
    err = uart_callback_set(p_device, uart_callback, (void *)p_device);
    __ASSERT(err == 0, "Failed to set callback");

    /* Drain the rx buffer */
    unsigned char c;
    while (uart_poll_in(p_device, &c) >= 0) {
        continue;
    }

    __ASSERT(err == 0, "Failed to enable RX");
    //pm_device_state_set(m_device, PM_DEVICE_STATE_ACTIVE, NULL, NULL);
    //k_sleep(K_MSEC(100));
    err = uart_receive();

    return 0;
}

/**@brief Un-initialize app_uart module */
void app_uart_uninit(void)
{
    //int err;

    if (m_device == NULL) {
        return;
    }

    app_uart_rx_cb_set(NULL);
    app_uart_tx_cb_set(NULL);
    
    /* Power off UART module */
    uart_rx_disable(m_device);
    //k_sleep(K_MSEC(100));
    //err = pm_device_state_set(m_device, PM_DEVICE_STATE_OFF, NULL, NULL);
    //if (err) {
    //    LOG_WRN("Can't power off uart: %d", err);
    //}

    LOG_DBG("%s done", __func__);
}

/**@brief Rest a buffer data
 *
 * @param[in] p_buff: pointer of the buffer
 *
 * @return n/a
 */
void uart_buffer_reset(uart_buff_t* p_buff)
{
    // TODO: review
//  memset(p_buff->p_data, 0, p_buff->max_len);
    p_buff->length = 0;
}

/**@brief Reset rx buffer */
void app_uart_rx_reset(void)
{
    uart_buffer_reset(&m_rx_buff);
}

/**@brief Set rx data ready event callback */
void app_uart_rx_cb_set(uart_rx_cb cb)
{
    m_rx_cb = cb;
}

/**@brief Set tx empty event callback */
void app_uart_tx_cb_set(uart_tx_cb cb)
{
    m_tx_cb = cb;
}
