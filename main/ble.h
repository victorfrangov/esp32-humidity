#ifndef BLE
#define BLE

#include "esp_err.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <stdbool.h>
#include <stddef.h>

void ble_init(void);
void ble_scan_start(void);
bool ble_devices_take_dirty(void);
void ble_get_devices_text(char* out, size_t out_sz);

esp_err_t configure_ble5_advertising(void);
esp_err_t start_ble5_advertising(void);

#endif /* BLE */
