#include "main.h"
#include "weather.h"
#include <stdarg.h>

// Menu state model
static Screen current_screen = SCREEN_MAIN;
static const Menu* current_menu = NULL;

static int main_selected = 0;
static int weather_selected = 0;
static int settings_selected = 0;
static bool wifi_connected = false;

// Main menu
static const MenuItem main_menu_items[] = {
    { "Games",       action_placeholder },
    { "Weather",     action_open_weather },
    { "Time",        action_placeholder },
    { "Settings",    action_open_settings },
    { "Shutdown",    action_placeholder }
};
#define MAIN_MENU_COUNT (sizeof(main_menu_items) / sizeof(main_menu_items[0]))

static const MenuItem weather_menu_items[] = {
    { "Here", action_tnh },
    { "Montreal", action_weather_mtl }
};
#define WEATHER_MENU_COUNT (sizeof(weather_menu_items) / sizeof(weather_menu_items[0]))

// Settings submenu
static const MenuItem settings_menu_items[] = {
    { "WiFi",      action_wifi },
    { "Bluetooth", action_bt },
    { "Back",      action_back }
};
#define SETTINGS_MENU_COUNT (sizeof(settings_menu_items) / sizeof(settings_menu_items[0]))

static const Menu main_menu = { main_menu_items, MAIN_MENU_COUNT, &main_selected };
static const Menu weather_menu = { weather_menu_items, WEATHER_MENU_COUNT, &weather_selected };
static const Menu settings_menu = { settings_menu_items, SETTINGS_MENU_COUNT, &settings_selected };

u8g2_t u8g2;

static void i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static void u8g2_init(void) {    
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.clk   = PIN_CLK;
    u8g2_esp32_hal.mosi  = PIN_MOSI;
    u8g2_esp32_hal.cs    = PIN_CS;
    u8g2_esp32_hal.dc    = PIN_DC;
    u8g2_esp32_hal.reset = PIN_RESET;

    u8g2_esp32_hal_init(u8g2_esp32_hal);

    u8g2_Setup_ssd1309_128x64_noname2_f(&u8g2, U8G2_R0, u8g2_esp32_spi_byte_cb, u8g2_esp32_gpio_and_delay_cb);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
}

static void uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    //Set UART pins (using UART0 default pins)
    uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static void update_screenf(const char* fmt, ...) {
    char text[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);

    const int max_w = u8g2_GetDisplayWidth(&u8g2);
    const int line_h = 10;
    int y = 12;

    char line[64] = {0};
    char word[32] = {0};
    const char* p = text;

    while (*p) {
        if (*p == '\n') {
            if (line[0]) {
                u8g2_DrawStr(&u8g2, 0, y, line);
                y += line_h;
                line[0] = '\0';
            }
            p++;
            continue;
        }

        while (*p == ' ') p++;

        int wi = 0;
        while (*p && *p != ' ' && *p != '\n' && wi < (int)sizeof(word) - 1) {
            word[wi++] = *p++;
        }
        word[wi] = '\0';
        if (word[0] == '\0') break;

        char trial[64];
        if (line[0]) {
            strlcpy(trial, line, sizeof(trial));
            strlcat(trial, " ", sizeof(trial));
            strlcat(trial, word, sizeof(trial));
        } else {
            strlcpy(trial, word, sizeof(trial));
        }

        if (u8g2_GetStrWidth(&u8g2, trial) > max_w) {
            if (line[0]) {
                u8g2_DrawStr(&u8g2, 0, y, line);
                y += line_h;
                snprintf(line, sizeof(line), "%s", word);
            } else {
                u8g2_DrawStr(&u8g2, 0, y, word);
                y += line_h;
                line[0] = '\0';
            }
        } else {
            snprintf(line, sizeof(line), "%s", trial);
        }
    }

    if (line[0]) {
        u8g2_DrawStr(&u8g2, 0, y, line);
    }

    u8g2_SendBuffer(&u8g2);
}

static void weather_ui_update(const char* text, const char* icon) {
    u8g2_ClearBuffer(&u8g2);

    const WeatherIcon* ic = weather_bitmap_from_code(icon);
    if (ic) {
        u8g2_DrawXBMP(&u8g2, 0, 0, ic->w, ic->h, ic->data);
    }

    // Text (wrapped) to the right of icon
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    draw_wrapped_text(28, 12, u8g2_GetDisplayWidth(&u8g2) - 28, text);

    u8g2_SendBuffer(&u8g2);
}

static void draw_wifi_info(void) {
    wifi_ap_record_t ap_info = {0};
    esp_err_t ap_ret = esp_wifi_sta_get_ap_info(&ap_info);

    esp_netif_ip_info_t ip_info = {0};
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_get_ip_info(netif, &ip_info);
    }

    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);

    if (ap_ret == ESP_OK) {
        char line[64];

        u8g2_DrawStr(&u8g2, 0, 12, "WiFi CONNECTED");

        snprintf(line, sizeof(line), "SSID: %s", (char*)ap_info.ssid);
        u8g2_DrawStr(&u8g2, 0, 24, line);

        snprintf(line, sizeof(line), "RSSI: %d dBm", ap_info.rssi);
        u8g2_DrawStr(&u8g2, 0, 36, line);

        snprintf(line, sizeof(line), "CH: %d", ap_info.primary);
        u8g2_DrawStr(&u8g2, 0, 48, line);

        snprintf(line, sizeof(line), "IP: " IPSTR, IP2STR(&ip_info.ip));
        u8g2_DrawStr(&u8g2, 0, 60, line);
    } else {
        u8g2_DrawStr(&u8g2, 0, 12, "WiFi not connected");
    }

    u8g2_SendBuffer(&u8g2);
}
        
static void action_wifi(void) {
    set_screen(SCREEN_WIFI);
    update_screenf("WiFi: connecting...");

    if (!wifi_connected) {
        esp_err_t status = connect_wifi();
        if (status != WIFI_SUCCESS) {
            update_screenf("WiFi connection failed");
            return;
        }
        wifi_connected = true;
    }
}

static void action_weather_mtl(void) {
    if (wifi_connected) {
        weather_fetch_city("Montreal", weather_ui_update);
        set_screen(SCREEN_WEATHER_MTL);
    } else {
        weather_ui_update("WiFi failed", "");
    }
}

// Generic menu draw helper
static void draw_menu(const Menu* menu) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    for (int i = 0; i < menu->count; i++) {
        if (i == *(menu->selected)) {
            char line[32];
            snprintf(line, sizeof(line), "> %s", menu->items[i].label);
            u8g2_DrawStr(&u8g2, 0, 10 + 12 * i, line);
        } else {
            u8g2_DrawStr(&u8g2, 10, 10 + 12 * i, menu->items[i].label);
        }
    }
    u8g2_SendBuffer(&u8g2);
}

// Handling input
static void handle_input(const uint8_t* data, int len) {
    Key k = decode_key(data, len);
    if (k == KEY_NONE) return;

    if (current_menu) {
        if (k == KEY_UP && *(current_menu->selected) > 0) {
            (*(current_menu->selected))--;
            draw_menu(current_menu);
        } else if (k == KEY_DOWN && *(current_menu->selected) < (current_menu->count - 1)) {
            (*(current_menu->selected))++;
            draw_menu(current_menu);
        } else if (k == KEY_ENTER) {
            MenuAction action = current_menu->items[*(current_menu->selected)].action;
            if (action) action();
        } else if (k == KEY_ESC) {
            set_screen(SCREEN_MAIN);
        }
    } else {
        if (k == KEY_ESC) {
            set_screen(SCREEN_MAIN);
        }
    }
}

// Decodes Hex from buffer into keyboard keys
static Key decode_key(const uint8_t* data, int len) {
    if (len >= 3 && data[0] == 0x1B && data[1] == '[') {
        if (data[2] == 'A') return KEY_UP;
        if (data[2] == 'B') return KEY_DOWN;
    }
    if (len == 1 && (data[0] == '\r' || data[0] == '\n')) return KEY_ENTER;
    if (len == 2 && data[0] == '\r' && data[1] == '\n') return KEY_ENTER;
    if (len == 1 && data[0] == 0x1B) return KEY_ESC;
    return KEY_NONE;
}

static void set_screen(Screen s) {
    current_screen = s;
    switch (s) {
        case SCREEN_MAIN:
            current_menu = &main_menu;
            draw_menu(current_menu);
            break;
        case SCREEN_SETTINGS:
            current_menu = &settings_menu;
            draw_menu(current_menu);
            break;
        case SCREEN_WEATHER:
            current_menu = &weather_menu;
            draw_menu(current_menu);
            break;
        case SCREEN_TNH:
            current_menu = NULL;
            break;
        case SCREEN_WEATHER_MTL:
            current_menu = NULL;
            break;
        case SCREEN_WIFI:
            current_menu = NULL;
            update_screenf("WiFi Action");
            break;
        case SCREEN_BT:
            current_menu = NULL;
            update_screenf("Bluetooth Action");
            break;
    }
}

static void draw_wrapped_text(int x, int y, int max_w, const char* text) {
    const int line_h = 10;
    char line[64] = {0};
    char word[32] = {0};
    const char* p = text;

    while (*p) {
        if (*p == '\n') {
            if (line[0]) {
                u8g2_DrawStr(&u8g2, x, y, line);
                y += line_h;
                line[0] = '\0';
            }
            p++;
            continue;
        }

        while (*p == ' ') p++;

        int wi = 0;
        while (*p && *p != ' ' && *p != '\n' && wi < (int)sizeof(word) - 1) {
            word[wi++] = *p++;
        }
        word[wi] = '\0';
        if (word[0] == '\0') break;

        char trial[64];
        if (line[0]) {
            strlcpy(trial, line, sizeof(trial));
            strlcat(trial, " ", sizeof(trial));
            strlcat(trial, word, sizeof(trial));
        } else {
            strlcpy(trial, word, sizeof(trial));
        }

        if (u8g2_GetStrWidth(&u8g2, trial) > max_w) {
            if (line[0]) {
                u8g2_DrawStr(&u8g2, x, y, line);
                y += line_h;
                strlcpy(line, word, sizeof(line));
            } else {
                u8g2_DrawStr(&u8g2, x, y, word);
                y += line_h;
                line[0] = '\0';
            }
        } else {
            strlcpy(line, trial, sizeof(line));
        }
    }

    if (line[0]) {
        u8g2_DrawStr(&u8g2, x, y, line);
    }
}

static void action_placeholder(void) { update_screenf("Not implemented"); }
static void action_open_weather(void) { set_screen(SCREEN_WEATHER); }
static void action_tnh(void) { set_screen(SCREEN_TNH); }
static void action_open_settings(void) { set_screen(SCREEN_SETTINGS); }
static void action_bt(void) { set_screen(SCREEN_BT); }
static void action_back(void) { set_screen(SCREEN_MAIN); }

// Main app
void app_main(void) {
    i2c_master_init();
    u8g2_init();
    uart_init();
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    set_screen(SCREEN_MAIN); // Start on main menu

    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    bool running = true;
    while (running) {
        int len = read(STDIN_FILENO, data, BUF_SIZE - 1);

        if (len > 0) {
            data[len] = '\0';
            handle_input(data, len);
        }

        if (current_screen == SCREEN_TNH) {
            dht20_display(&u8g2);
        }
        if (current_screen == SCREEN_WIFI) {
            draw_wifi_info();
        }
        vTaskDelay(pdMS_TO_TICKS(100));

        // Add a condition to exit the loop if needed SLEEP?
        // Example: running = false; // Set this based on some condition
    }
    free(data);
}