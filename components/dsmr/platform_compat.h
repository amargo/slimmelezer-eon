#pragma once

#ifdef ESP32
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#define PLATFORM_NAME "ESP32"
#define PLATFORM_RESET_WDT() esp_task_wdt_reset()
#define PLATFORM_SLEEP(us) esp_sleep_enable_timer_wakeup(us); esp_light_sleep_start()
#define PLATFORM_MAX_TELEGRAM_SIZE 3000
#define PLATFORM_UART_BUFFER_SIZE 512
#define PLATFORM_WAIT_DELAY 2
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#define PLATFORM_NAME "ESP8266"
#define PLATFORM_RESET_WDT() ESP.wdtFeed()
#define PLATFORM_SLEEP(us) ESP.deepSleep(us)
#define PLATFORM_MAX_TELEGRAM_SIZE 1500
#define PLATFORM_UART_BUFFER_SIZE 256
#define PLATFORM_WAIT_DELAY 5
#else
#error "Unsupported platform"
#endif
