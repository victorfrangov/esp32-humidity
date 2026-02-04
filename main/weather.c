#include "weather.h"

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

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_FAIL;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE("weather", "open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int64_t clen = esp_http_client_fetch_headers(client);
    if (clen < 0) {
        int eno = esp_http_client_get_errno(client);
        ESP_LOGE("weather", "fetch_headers failed, errno=%d", eno);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int status = esp_http_client_get_status_code(client);
    ESP_LOGI("weather", "status=%d, content_len=%lld", status, clen);

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

    WeatherInfo info = {0};

    if (status != 200) {
        info.ok = false;
        snprintf(info.err, sizeof(info.err), "HTTP error %d", status);
        update_ui(&info);
        free(buffer);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
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

            // assume `desc` is a cJSON* for the "description" field
            const char* desc_str = cJSON_GetStringValue(desc);
            strlcpy(info.desc, desc_str ? desc_str : "", sizeof(info.desc));
            capitalize_first(info.desc);

            info.ok = true;
            info.temp_c  = (temp && cJSON_IsNumber(temp)) ? (int)lround(temp->valuedouble) : 0;
            info.feels_c  = (feels && cJSON_IsNumber(feels)) ? (int)lround(feels->valuedouble) : 0;
            info.tmin_c = (tmin && cJSON_IsNumber(tmin)) ? (int)lround(tmin->valuedouble) : 0;
            info.tmax_c = (tmax && cJSON_IsNumber(tmax)) ? (int)lround(tmax->valuedouble) : 0;
            info.wind_kmh  = (wspd && cJSON_IsNumber(wspd)) ? (int)lround(wspd->valuedouble * 3.6) : 0;
            info.hum_pct  = (hum && cJSON_IsNumber(hum)) ? hum->valueint : 0;

            update_ui(&info);

            cJSON_Delete(root);
        } else {
            info.ok = false;
            strlcpy(info.err, "JSON parse error", sizeof(info.err));
            update_ui(&info);
        }
    } else {
        info.ok = false;
        strlcpy(info.err, "HTTP no body", sizeof(info.err));
        update_ui(&info);
    }
    free(buffer);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ESP_OK;
}