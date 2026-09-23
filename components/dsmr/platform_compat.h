#pragma once

#ifdef ESP32
#define PLATFORM_NAME "ESP32"
#define PLATFORM_MAX_TELEGRAM_SIZE 3000
#define PLATFORM_WAIT_DELAY 2
#elif defined(ESP8266)
#define PLATFORM_NAME "ESP8266"
#define PLATFORM_MAX_TELEGRAM_SIZE 3000
#define PLATFORM_WAIT_DELAY 5
#else
#error "Unsupported platform"
#endif
