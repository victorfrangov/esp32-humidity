#ifndef WEATHER
#define WEATHER

#include "wifi.h"
#include "weather_icons.h"

//define the callback here instead of in wifi.h
typedef void (*weather_update_callback_t)(const char* text, const char* icon);

esp_err_t weather_fetch_city(const char *city, weather_update_callback_t update_ui);

#endif /* WEATHER */
