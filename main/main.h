#ifndef MAIN
#define MAIN

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>

#include "sdkconfig.h"

#include <driver/i2c.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>


#include <u8g2.h>
#include "wifi.h"
#include "ble.h"
#include "dht20.h"

#include "u8g2_esp32_hal.h"

#define PIN_CLK 6
#define PIN_MOSI 7
#define PIN_CS 18
#define PIN_DC 19
#define PIN_RESET 20

#define SDA_PIN 10
#define SCL_PIN 11

#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_TIMEOUT_MS 1000

#define UART_NUM UART_NUM_0
#define BUF_SIZE (1024)

typedef enum {
    SCREEN_MAIN,
    SCREEN_SETTINGS,
    SCREEN_WEATHER,
    SCREEN_WEATHER_MTL,
    SCREEN_TNH,
    SCREEN_WIFI,
    SCREEN_BT
} Screen;

typedef void (*MenuAction)(void);

typedef struct {
    const char* label;
    MenuAction action;
} MenuItem;

typedef struct {
    const MenuItem* items;
    int count;
    int* selected;
} Menu;

typedef enum {
    KEY_NONE,
    KEY_UP,
    KEY_DOWN,
    KEY_ENTER,
    KEY_ESC
} Key;

static void i2c_master_init(void);
static void u8g2_init(void);
static void uart_init(void);
static void update_screenf(const char* fmt, ...);
static void weather_ui_update(const char* text, const char* icon);
static void draw_wifi_info(void);
static void draw_wrapped_text(int x, int y, int max_w, const char* text);
static void handle_input(const uint8_t* data, int len);
void app_main(void);

// Forward declarations
static void set_screen(Screen s);
static void action_placeholder(void);
static void action_open_weather(void);
static void action_tnh(void);
static void action_weather_mtl(void);
static void action_open_settings(void);
static void action_wifi(void);
static void action_bt(void);
static void action_back(void);
static Key decode_key(const uint8_t* data, int len);

#endif /* MAIN */

