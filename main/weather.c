#include "weather.h"
#include <esp_http_client.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "cJSON.h"
#include <esp_log_buffer.h>
#include "secrets.h"

#define WEATHER_API_KEY API_KEY

static void capitalize_first(char *s) {
    if (s && s[0] >= 'a' && s[0] <= 'z') {
        s[0] = (char)(s[0] - 'a' + 'A');
    }
}

esp_err_t weather_fetch_city(const char *city, weather_update_callback_t update_ui) {
    if (!city || !update_ui) return ESP_ERR_INVALID_ARG;

    char url[256];
    // TODO: URL-encode `city` if needed.
    snprintf(url, sizeof(url),
             "http://api.openweathermap.org/data/2.5/weather?q=%s&units=metric&appid=%s",
             city, WEATHER_API_KEY);
    // UPGRADE TO 3.0 ONECALL INSTEAD OF 2.5, TO GET HOURLY/DAILY DATA
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    esp_http_client_fetch_headers(client);

    char *buffer = calloc(1, 2048 + 1);
    if (!buffer) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    int total = 0;
    while (1) {
        int r = esp_http_client_read(client, buffer + total, 2048 - total);
        if (r <= 0) break;
        total += r;
        if (total >= 2048) break;
    }

    if (total > 0) {
        buffer[total] = '\0';

        cJSON *root = cJSON_Parse(buffer);
        if (root) {
            cJSON *main = cJSON_GetObjectItem(root, "main");
            cJSON *temp = main ? cJSON_GetObjectItem(main, "temp") : NULL;
            cJSON *feels = main ? cJSON_GetObjectItem(main, "feels_like") : NULL;
            cJSON *tmin = main ? cJSON_GetObjectItem(main, "temp_min") : NULL;
            cJSON *tmax = main ? cJSON_GetObjectItem(main, "temp_max") : NULL;
            cJSON *hum  = main ? cJSON_GetObjectItem(main, "humidity") : NULL;

            cJSON *wind = cJSON_GetObjectItem(root, "wind");
            cJSON *wspd = wind ? cJSON_GetObjectItem(wind, "speed") : NULL;

            cJSON *weather = cJSON_GetObjectItem(root, "weather");
            cJSON *w0 = (weather && cJSON_IsArray(weather)) ? cJSON_GetArrayItem(weather, 0) : NULL;
            cJSON *desc = w0 ? cJSON_GetObjectItem(w0, "description") : NULL;
            cJSON *icon = w0 ? cJSON_GetObjectItem(w0, "icon") : NULL;

            char desc_buf[32] = {0};
            if (desc && cJSON_IsString(desc) && desc->valuestring) {
                strncpy(desc_buf, desc->valuestring, sizeof(desc_buf) - 1);
                capitalize_first(desc_buf);
            } else {
                strncpy(desc_buf, "N/A", sizeof(desc_buf) - 1);
            }

            char icon_code[4] = {0};
            if (icon && cJSON_IsString(icon) && icon->valuestring) {
                strncpy(icon_code, icon->valuestring, sizeof(icon_code) - 1);
            }
            
            int t  = (temp && cJSON_IsNumber(temp)) ? (int)lround(temp->valuedouble) : 0;
            int f  = (feels && cJSON_IsNumber(feels)) ? (int)lround(feels->valuedouble) : 0;
            int tn = (tmin && cJSON_IsNumber(tmin)) ? (int)lround(tmin->valuedouble) : 0;
            int tx = (tmax && cJSON_IsNumber(tmax)) ? (int)lround(tmax->valuedouble) : 0;
            unsigned int w  = (wspd && cJSON_IsNumber(wspd)) ? (int)lround(wspd->valuedouble * 3.6) : 0;
            unsigned int h  = (hum && cJSON_IsNumber(hum)) ? hum->valueint : 0;
            
            char line1[64];
            char line2[64];

            snprintf(line1, sizeof(line1), "T:%iC F:%iC H:%u%%", t, f, h);
            snprintf(line2, sizeof(line2), "Min:%iC Max:%iC W:%u KM/H %s", tn, tx, w, desc_buf);

            char msg[128];
            snprintf(msg, sizeof(msg), "%s\n%s", line1, line2);

            update_ui(msg, icon_code);

            cJSON_Delete(root);
        } else {
            update_ui("JSON parse error", "");
        }
    } else {
        update_ui("HTTP no body", "");
    }

    free(buffer);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ESP_OK;
}