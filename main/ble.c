#include "ble.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#define BLE_TAG "BLE_DRIVER"
#define APPLE_MANUFACTURER_ID 0x004C

#define BLE_MAX_DEVICES 8
#define BLE_NAME_MAX 32

typedef struct {
    bool used;
    uint8_t addr[6];
    char name[BLE_NAME_MAX];
    int8_t rssi;
} ble_device_t;

static ble_device_t s_devices[BLE_MAX_DEVICES];
static int s_device_count = 0;
static bool s_devices_dirty = false;
static bool s_scanning = false;

static uint8_t own_addr_type;
static bool is_connecting = false;

static int ble_gap_event(struct ble_gap_event *event, void *arg);
static void ble_start_scan(void);

static void addr_to_str(const uint8_t* addr, char* out, size_t out_sz)
{
    snprintf(out, out_sz, "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

static int find_device_index(const uint8_t* addr)
{
    for (int i = 0; i < BLE_MAX_DEVICES; i++) {
        if (s_devices[i].used && memcmp(s_devices[i].addr, addr, 6) == 0) {
            return i;
        }
    }
    return -1;
}

static void upsert_device(const uint8_t* addr, const char* name, int8_t rssi)
{
    int idx = find_device_index(addr);
    if (idx < 0) {
        for (int i = 0; i < BLE_MAX_DEVICES; i++) {
            if (!s_devices[i].used) {
                idx = i;
                s_devices[i].used = true;
                memcpy(s_devices[i].addr, addr, 6);
                s_device_count++;
                break;
            }
        }
    }

    if (idx >= 0) {
        bool changed = false;
        if (name && name[0] != '\0' && strncmp(s_devices[idx].name, name, BLE_NAME_MAX) != 0) {
            strlcpy(s_devices[idx].name, name, BLE_NAME_MAX);
            changed = true;
        } else if (!s_devices[idx].name[0] && name && name[0]) {
            strlcpy(s_devices[idx].name, name, BLE_NAME_MAX);
            changed = true;
        }
        if (s_devices[idx].rssi != rssi) {
            s_devices[idx].rssi = rssi;
            changed = true;
        }
        if (changed) {
            s_devices_dirty = true;
        }
    }
}

static void ble_on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }

    ble_start_scan();
}

static void ble_start_scan(void)
{
    if (s_scanning) {
        return;
    }

    struct ble_gap_disc_params params = {
        .itvl = 0x50,
        .window = 0x30,
        .filter_policy = BLE_HCI_SCAN_FILT_NO_WL,
        .limited = 0,
        .passive = 0,
        .filter_duplicates = 1,
    };

    int rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(BLE_TAG, "ble_gap_disc failed: %d", rc);
    } else {
        s_scanning = true;
        ESP_LOGI(BLE_TAG, "Scanning started");
    }
}

static void ble_log_adv_fields(const struct ble_hs_adv_fields *fields)
{
    if (fields->mfg_data != NULL && fields->mfg_data_len >= 2) {
        uint16_t manufacturer_id = (fields->mfg_data[1] << 8) | fields->mfg_data[0];
        ESP_LOGI(BLE_TAG, "Manufacturer ID: 0x%04X", manufacturer_id);
        ESP_LOG_BUFFER_HEX(BLE_TAG, fields->mfg_data, fields->mfg_data_len);
    }

    if (fields->name != NULL && fields->name_len > 0) {
        ESP_LOGI(BLE_TAG, "Name: %.*s", fields->name_len, fields->name);
    }
}

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        struct ble_hs_adv_fields fields;
        memset(&fields, 0, sizeof(fields));

        int rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        if (rc != 0) {
            return 0;
        }

        char name[BLE_NAME_MAX] = {0};
        if (fields.name != NULL && fields.name_len > 0) {
            int n = fields.name_len;
            if (n >= BLE_NAME_MAX) n = BLE_NAME_MAX - 1;
            memcpy(name, fields.name, n);
            name[n] = '\0';
        }

        if (!name[0]) {
            addr_to_str(event->disc.addr.val, name, sizeof(name));
        }

        upsert_device(event->disc.addr.val, name, (int8_t)event->disc.rssi);

        bool is_apple_device = false;
        if (fields.mfg_data != NULL && fields.mfg_data_len >= 2) {
            uint16_t manufacturer_id = (fields.mfg_data[1] << 8) | fields.mfg_data[0];
            if (manufacturer_id == APPLE_MANUFACTURER_ID) {
                is_apple_device = true;
                ESP_LOGI(BLE_TAG, "Apple device found, RSSI %d", event->disc.rssi);
                ble_log_adv_fields(&fields);
            }
        }

        if (!is_connecting && is_apple_device) {
            is_connecting = true;
            ble_gap_disc_cancel();

            int conn_rc = ble_gap_connect(own_addr_type, &event->disc.addr, 30000, NULL, ble_gap_event, NULL);
            if (conn_rc != 0) {
                ESP_LOGE(BLE_TAG, "ble_gap_connect failed: %d", conn_rc);
                is_connecting = false;
                ble_start_scan();
            }
        }
        return 0;
    }
    case BLE_GAP_EVENT_CONNECT:
        s_scanning = false;
        if (event->connect.status == 0) {
            ESP_LOGI(BLE_TAG, "Connected");
        } else {
            ESP_LOGE(BLE_TAG, "Connect failed: %d", event->connect.status);
            is_connecting = false;
            ble_start_scan();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(BLE_TAG, "Disconnected, reason=%d", event->disconnect.reason);
        is_connecting = false;
        s_scanning = false;
        ble_start_scan();
        return 0;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(BLE_TAG, "Scan complete");
        s_scanning = false;
        if (!is_connecting) {
            ble_start_scan();
        }
        return 0;
    default:
        return 0;
    }
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_init(void)
{
    esp_err_t ret = esp_nimble_hci_init();
    if (ret != ESP_OK) {
        ESP_LOGE(BLE_TAG, "NimBLE HCI init failed: %s", esp_err_to_name(ret));
        return;
    }

    nimble_port_init();

    ble_hs_cfg.sync_cb = ble_on_sync;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("ESP32-Mobile");

    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(BLE_TAG, "NimBLE initialized");
}

esp_err_t configure_ble5_advertising(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t start_ble5_advertising(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

void ble_scan_start(void)
{
    ble_start_scan();
}

bool ble_devices_take_dirty(void)
{
    bool dirty = s_devices_dirty;
    s_devices_dirty = false;
    return dirty;
}

void ble_get_devices_text(char* out, size_t out_sz)
{
    if (!out || out_sz == 0) return;

    if (s_device_count == 0) {
        snprintf(out, out_sz, "Scanning...");
        return;
    }

    int pos = snprintf(out, out_sz, "Found: %d\n", s_device_count);
    for (int i = 0; i < BLE_MAX_DEVICES && pos < (int)out_sz - 1; i++) {
        if (!s_devices[i].used) continue;
        pos += snprintf(out + pos, out_sz - (size_t)pos, "%s (%ddBm)\n",
                        s_devices[i].name[0] ? s_devices[i].name : "Unknown",
                        s_devices[i].rssi);
    }
}