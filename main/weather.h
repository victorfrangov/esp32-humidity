#ifndef WEATHER
#define WEATHER

#include <esp_http_client.h>
#include <math.h>
#include <cJSON.h>
#include "wifi.h"
#include "secrets.h"

typedef struct {
    bool ok;
    char err[64];

    int temp_c;
    int feels_c;
    int tmin_c;
    int tmax_c;
    unsigned hum_pct;
    unsigned wind_kmh;

    char desc[32];
} WeatherInfo;

//define the callback here instead of in wifi.h
typedef void (*weather_update_callback_t)(const WeatherInfo* w);

esp_err_t weather_fetch_city(const char *city, weather_update_callback_t update_ui);

#endif /* WEATHER */
