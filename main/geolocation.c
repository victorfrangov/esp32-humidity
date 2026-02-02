#include "geolocation.h"

static const char* TAG = "GEO";

static bool geo_fetch_once(const char* url, GeoInfo* out) {
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        strlcpy(out->message, "HTTP open failed", sizeof(out->message));
        esp_http_client_cleanup(client);
        return false;
    }

    int headers_ok = esp_http_client_fetch_headers(client);
    if (headers_ok < 0) {
        ESP_LOGE(TAG, "HTTP headers failed");
        strlcpy(out->message, "HTTP headers failed", sizeof(out->message));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    int http_status = esp_http_client_get_status_code(client);
    if (http_status != 200) {
        snprintf(out->message, sizeof(out->message), "HTTP %d", http_status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    char buf[256];
    int len = esp_http_client_read(client, buf, sizeof(buf) - 1);
    buf[len] = '\0';
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (len <= 0) {
        strlcpy(out->message, "Empty response", sizeof(out->message));
        return false;
    }

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        strlcpy(out->message, "JSON parse failed", sizeof(out->message));
        return false;
    }

    cJSON* status = cJSON_GetObjectItem(root, "status");
    cJSON* message = cJSON_GetObjectItem(root, "message");
    cJSON* offset = cJSON_GetObjectItem(root, "offset");
    cJSON* countryCode = cJSON_GetObjectItem(root, "countryCode");
    cJSON* region = cJSON_GetObjectItem(root, "region");
    cJSON* city = cJSON_GetObjectItem(root, "city");

    bool ok = (status && cJSON_IsString(status) &&
               strcmp(status->valuestring, "success") == 0 &&
               offset && cJSON_IsNumber(offset));

    if (!ok && message && cJSON_IsString(message) && message->valuestring) {
        strlcpy(out->message, message->valuestring, sizeof(out->message));
    }

    if (ok) {
        out->offset_sec = (long)offset->valuedouble;
        if (countryCode && cJSON_IsString(countryCode) && countryCode->valuestring)
            strlcpy(out->countryCode, countryCode->valuestring, sizeof(out->countryCode));
        if (region && cJSON_IsString(region) && region->valuestring)
            strlcpy(out->region, region->valuestring, sizeof(out->region));
        if (city && cJSON_IsString(city) && city->valuestring)
            strlcpy(out->city, city->valuestring, sizeof(out->city));
        out->ok = true;
    }

    cJSON_Delete(root);
    return out->ok;
}

bool geo_fetch_info(const char* ip, GeoInfo* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));

    char url[128];
    if (ip && ip[0]) {
        snprintf(url, sizeof(url),
                 "http://ip-api.com/json/%s?fields=status,message,countryCode,region,city,offset",
                 ip);
    } else {
        snprintf(url, sizeof(url),
                 "http://ip-api.com/json/?fields=status,message,countryCode,region,city,offset");
    }

    const int max_retries = 3;
    for (int i = 0; i < max_retries; ++i) {
        if (geo_fetch_once(url, out)) return true;

        int backoff_ms = 500 * (i + 1); // 500ms, 1000ms, 1500ms
        vTaskDelay(pdMS_TO_TICKS(backoff_ms));
    }

    return false;
}