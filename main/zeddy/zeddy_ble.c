#include "zeddy_ble.h"
#include <host/ble_gatt.h>
#include <esp_nimble_hci.h>
#include <esp_log.h>
#include <nimble/ble.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <host/ble_hs_id.h>
#include <host/ble_hs.h>
#include <host/ble_uuid.h>
#include <host/ble_store.h>
#include <esp_ota_ops.h>
#include <inttypes.h>
/* BLE */
#include "console/console.h"
#include "modlog/modlog.h"

static const char *TAG = "ZeddyBle";
#define BLE_RANDOM_STATIC_ADDRESS 1
#define BLE_PUBLIC_ADDRESS 2
uint8_t ownAddressType;
ble_addr_t ownAddress;





void advertise();
void ble_store_config_init(void);

/** GATT server. */
#define GATT_SVR_SVC_ALERT_UUID 0x1811
#define GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID 0x2A47
#define GATT_SVR_CHR_NEW_ALERT 0x2A46
#define GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID 0x2A48
#define GATT_SVR_CHR_UNR_ALERT_STAT_UUID 0x2A45
#define GATT_SVR_CHR_ALERT_NOT_CTRL_PT 0x2A44

/**
 * Utility function to log an array of bytes.
 */
void print_bytes(const uint8_t *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++)
    {
        MODLOG_DFLT(INFO, "%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

void print_addr(const void *addr)
{
    const uint8_t *u8p;

    u8p = (const uint8_t *)addr;
    MODLOG_DFLT(INFO, "%02x:%02x:%02x:%02x:%02x:%02x",
                u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op)
    {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGI(TAG, "registered service %s with handle=%d",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGI(TAG, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGI(TAG, "registering descriptor %s with handle=%d",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

/**
 * Logs information about a connection to the console.
 */
static void
log_connection_description(struct ble_gap_conn_desc *desc)
{
    ESP_LOGI(TAG, "conn_handle=%d our_ota_addr.type=%d our_ota_addr.val=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->conn_handle, desc->our_ota_addr.type,
             desc->our_ota_addr.val[5], desc->our_ota_addr.val[4], desc->our_ota_addr.val[3],
             desc->our_ota_addr.val[2], desc->our_ota_addr.val[1], desc->our_ota_addr.val[0]);

    ESP_LOGI(TAG, "  our_id_addr.type=%d our_id_addr.val=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->our_id_addr.type,
             desc->our_id_addr.val[5], desc->our_id_addr.val[4], desc->our_id_addr.val[3],
             desc->our_id_addr.val[2], desc->our_id_addr.val[1], desc->our_id_addr.val[0]);

    ESP_LOGI(TAG, "  peer_ota_addr.type=%d peer_ota_addr.val=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->peer_ota_addr.type,
             desc->peer_ota_addr.val[5], desc->peer_ota_addr.val[4], desc->peer_ota_addr.val[3],
             desc->peer_ota_addr.val[2], desc->peer_ota_addr.val[1], desc->peer_ota_addr.val[0]);

    ESP_LOGI(TAG, "  peer_id_addr.type=%d peer_id_addr.val=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->peer_id_addr.type,
             desc->peer_id_addr.val[5], desc->peer_id_addr.val[4], desc->peer_id_addr.val[3],
             desc->peer_id_addr.val[2], desc->peer_id_addr.val[1], desc->peer_id_addr.val[0]);

    ESP_LOGI(TAG, "  conn_itvl=%d conn_latency=%d supervision_timeout=%d sec_state.encrypted=%d sec_state.authenticated=%d sec_state.bonded=%d",
             desc->conn_itvl, desc->conn_latency,
             desc->supervision_timeout,
             desc->sec_state.encrypted,
             desc->sec_state.authenticated,
             desc->sec_state.bonded);
}

static int
gap_event_handler(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        ESP_LOGI(TAG, "connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);
        if (event->connect.status == 0)
        {
            ESP_ERROR_CHECK(ble_gap_conn_find(event->connect.conn_handle, &desc));
            log_connection_description(&desc);
        }
        if (event->connect.status != 0)
        {
            advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%s (%d)", esp_err_to_name(event->disconnect.reason), event->disconnect.reason);
        log_connection_description(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        /* Connection terminated; resume advertising. */
        advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        MODLOG_DFLT(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        log_connection_description(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "advertise complete; reason=%d",
                    event->adv_complete.reason);
        advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        log_connection_description(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
                          "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(TAG, "PASSKEY_ACTION_EVENT started \n");
        struct ble_sm_io pkey = {0};
        // int key = 0;

        if (event->passkey.params.action == BLE_SM_IOACT_DISP)
        {
            pkey.action = event->passkey.params.action;
            pkey.passkey = 123456; // This is the passkey to be entered on peer
            ESP_LOGI(TAG, "Enter passkey %"PRIu32" on the peer side", pkey.passkey);
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d\n", rc);
        }
        else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP)
        {
            ESP_LOGI(TAG, "Passkey on device's display: %"PRIu32, event->passkey.params.numcmp);
            ESP_LOGI(TAG, "Accept or reject the passkey through console in this format -> key Y or key N");
            pkey.action = event->passkey.params.action;
            //                if (scli_receive_key(&key)) {
            //                    pkey.numcmp_accept = key;
            //                } else {
            pkey.numcmp_accept = 0;
            ESP_LOGE(TAG, "Timeout! Rejecting the key");
            //                }
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d\n", rc);
        }
        else if (event->passkey.params.action == BLE_SM_IOACT_OOB)
        {
            static uint8_t tem_oob[16] = {0};
            pkey.action = event->passkey.params.action;
            for (int i = 0; i < 16; i++)
            {
                pkey.oob[i] = tem_oob[i];
            }
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d\n", rc);
        }
        else if (event->passkey.params.action == BLE_SM_IOACT_INPUT)
        {
            ESP_LOGI(TAG, "Enter the passkey through console in this format-> key 123456");
            pkey.action = event->passkey.params.action;
            //                if (scli_receive_key(&key)) {
            //                    pkey.passkey = key;
            //                } else {
            pkey.passkey = 0;
            //                    ESP_LOGE(TAG, "Timeout! Passing 0 as the key");
            //                }
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d\n", rc);
        }
        return 0;
    }

    return 0;
}

void advertise()
{
    ESP_LOGI(TAG, "BLE advertise");
    const char *name = ble_svc_gap_device_name();
    struct ble_hs_adv_fields fields = {
        .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
        .num_uuids16 = 1,
        .uuids16_is_complete = 1,
        .name = (uint8_t *)name,
        .name_len = (uint8_t)strlen(name),
        .name_is_complete = 1,
        .tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO,
        .tx_pwr_lvl_is_present = 1,

    };
    fields.uuids16 = (const ble_uuid16_t[]){
        {
            .u = {.type = BLE_UUID_TYPE_16},
            .value = (GATT_SVR_SVC_ALERT_UUID),
        }};
    ESP_ERROR_CHECK(ble_gap_adv_set_fields(&fields));

    const struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };
    ESP_ERROR_CHECK(ble_gap_adv_start(ownAddress.type, NULL, BLE_HS_FOREVER,
                                      &adv_params, gap_event_handler, NULL));
}

void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void _sync_callback(void)
{
    ESP_LOGI(TAG, "BLE OnSync");

    // Assigning address
    if (ownAddressType == BLE_RANDOM_STATIC_ADDRESS)
    {
        ESP_LOGI(TAG, "Generating random address");
        ESP_ERROR_CHECK(ble_hs_id_gen_rnd(0, &ownAddress));
        ESP_ERROR_CHECK(ble_hs_id_set_rnd(ownAddress.val));
    }
    else if (ownAddressType == BLE_PUBLIC_ADDRESS)
    {
        // TODO: workout additional address types
    }

    ESP_LOGI(TAG, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x",
             ownAddress.val[5], ownAddress.val[4], ownAddress.val[3],
             ownAddress.val[2], ownAddress.val[1], ownAddress.val[0]);

    advertise();
}

static void _reset_callback(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

void zeddy_ble_init(const char *deviceName)
{
//    ESP_ERROR_CHECK(esp_nimble_hci_init());
//    nimble_port_init();
    ESP_ERROR_CHECK(nimble_port_init());

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = _reset_callback;
    ble_hs_cfg.sync_cb = _sync_callback;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ESP_ERROR_CHECK(ble_svc_gap_device_name_set(deviceName));
}

void zeddy_ble_gatt_init(const ble_gatt_svc_def *services)
{
    ble_svc_gap_init();
    ble_svc_gatt_init();

    ESP_ERROR_CHECK(ble_gatts_count_cfg(services));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(services));
}

void zeddy_ble_set_random_static_address()
{
    ownAddressType = BLE_RANDOM_STATIC_ADDRESS;
}


void zeddy_ble_start()
{
    ble_store_config_init();
    nimble_port_freertos_init(ble_host_task);
}
