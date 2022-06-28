#include <string.h>
#include <stdio.h>
#include <zephyr.h>
#include <device.h>
#include <net/socket.h>
#include <modem/lte_lc.h>

#include <modem/nrf_modem_lib.h>
#include <net/fota_download.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(http_client, 4);

#include "http_client.h"

#define HTTP_HOST_LEN_MAX   50
#define HTTP_FILE_LEN_MAX   30

static struct k_work	 wk_http_download;

fota_download_callback_t m_user_download_cb;

static char m_http_host[HTTP_HOST_LEN_MAX];
static char m_http_file[HTTP_FILE_LEN_MAX];
static uint32_t m_file_size;
static bool downloading;

/**@brief Handler for HTTP download worker */
static void http_download_handler(struct k_work* unused)
{
    int rc;
    int sec_tag = -1;
    char* apn = NULL;
    int port = 0;       // HTTP, not HTTPS

    rc = fota_download_start(m_http_host, m_http_file, sec_tag, apn, port);
    if (rc) {
        LOG_ERR("Download file error, %d", rc);
    }
}

/**@brief Start to HTTP download */
int http_client_download(char* host, char* file)
{
    if (downloading) {
        return 0;
    }

    if (strlen(file) > HTTP_FILE_LEN_MAX) {
        LOG_ERR("File path len should be less than %d bytes", HTTP_FILE_LEN_MAX);
        return -1;
    }

    if (strlen(host) > HTTP_HOST_LEN_MAX) {
        LOG_ERR("Host len should be less than %d bytes", HTTP_HOST_LEN_MAX);
        return -1;
    }

    memcpy(m_http_host, host, strlen(host));
    memcpy(m_http_file, file, strlen(file));

    k_work_submit(&wk_http_download);

    downloading = true;

    return 0;
}

/**@brief HTTP download event handler */
static void download_handler(const struct fota_download_evt* evt)
{
    switch (evt->id) {
    case FOTA_DOWNLOAD_EVT_FINISHED:
        LOG_DBG("Download event finished");
        m_file_size= fota_download_file_size();
        downloading = false;
        break;

    case FOTA_DOWNLOAD_EVT_ERROR:
        LOG_ERR("Download event error");
        downloading = false;
        break;

    case FOTA_DOWNLOAD_EVT_PROGRESS:
    case FOTA_DOWNLOAD_EVT_ERASE_PENDING:
    case FOTA_DOWNLOAD_EVT_ERASE_DONE:
    default:
        LOG_DBG("Download event: %d", evt->id);
        break;
    }

    if (m_user_download_cb) {
        m_user_download_cb(evt);
    }
}

/**@brief Start to connect LTE network */
int http_client_connect(void)
{
    return lte_lc_init_and_connect();
}

/**@brief Initialize http client module  */
int http_client_init(fota_download_callback_t download_callback)
{
    int rc;
    
    downloading = false;
    m_user_download_cb = download_callback;

    LOG_DBG("http_client_init\n");

    rc = nrf_modem_lib_get_init_ret();    
    /* Handle return values relating to modem firmware update */
    switch (rc) {
    case 0:
        /* Initialization successful, no action required. */
        break;
    case MODEM_DFU_RESULT_OK:
        LOG_INF("MODEM UPDATE OK. Will run new firmware after reboot");
        
        // TODO: reset?
        k_thread_suspend(k_current_get());
        break;
    case MODEM_DFU_RESULT_UUID_ERROR:
    case MODEM_DFU_RESULT_AUTH_ERROR:
        LOG_ERR("MODEM UPDATE ERROR %d. Will run old firmware", rc);
        break;
    case MODEM_DFU_RESULT_HARDWARE_ERROR:
    case MODEM_DFU_RESULT_INTERNAL_ERROR:
        LOG_ERR("MODEM UPDATE FATAL ERROR %d. Modem failure", rc);
        break;
    default:
        /* All non-zero return codes other than DFU result codes are
         * considered irrecoverable and a reboot is needed.
         */
        LOG_ERR("nRF modem lib initialization failed, error: %d", rc);
        break;
    }


    rc = fota_download_init(download_handler);
    if (rc) {
        LOG_ERR("FOTA init error.");
        return rc;
    }

    k_work_init(&wk_http_download, http_download_handler);

    return rc;
}
uint32_t get_file_size(void){
    LOG_ERR("File Size is Currently commented out");
    return m_file_size;
}