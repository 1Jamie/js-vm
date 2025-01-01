#ifndef DUKTAPE_BINDINGS_H
#define DUKTAPE_BINDINGS_H

#include <Arduino.h>
#include "duktape.h"
#include <Wire.h>
#include <SPI.h>
#include <esp_sleep.h>
#include <driver/adc.h>
#include <driver/touch_sensor.h>
#include <driver/ledc.h>
#include <driver/timer.h>
#include <sys/time.h>
#include <soc/ledc_reg.h>
#include <soc/ledc_struct.h>

// Core bindings
duk_ret_t duk_print(duk_context *ctx);
duk_ret_t duk_delay(duk_context *ctx);
duk_ret_t duk_wait(duk_context *ctx);

// GPIO bindings
duk_ret_t duk_digitalWrite(duk_context *ctx);
duk_ret_t duk_digitalRead(duk_context *ctx);
duk_ret_t duk_analogRead(duk_context *ctx);
duk_ret_t duk_analogWrite(duk_context *ctx);
duk_ret_t duk_pinMode(duk_context *ctx);

// WiFi bindings
duk_ret_t duk_wifiConnect(duk_context *ctx);
duk_ret_t duk_wifiDisconnect(duk_context *ctx);
duk_ret_t duk_getIP(duk_context *ctx);

// I2C bindings
duk_ret_t duk_i2cBegin(duk_context *ctx);
duk_ret_t duk_i2cWrite(duk_context *ctx);
duk_ret_t duk_i2cRead(duk_context *ctx);

// SPI bindings
duk_ret_t duk_spiBegin(duk_context *ctx);
duk_ret_t duk_spiTransfer(duk_context *ctx);

// ADC bindings
duk_ret_t duk_adcConfig(duk_context *ctx);

// Touch sensor bindings
duk_ret_t duk_touchRead(duk_context *ctx);
duk_ret_t duk_touchAttachInterrupt(duk_context *ctx);

// RTC bindings
duk_ret_t duk_rtcGetTime(duk_context *ctx);

// Sleep bindings
duk_ret_t duk_deepSleep(duk_context *ctx);
duk_ret_t duk_lightSleep(duk_context *ctx);

// LED bindings
duk_ret_t duk_ledcSetup(duk_context *ctx);
duk_ret_t duk_ledcAttachPin(duk_context *ctx);
duk_ret_t duk_ledcWrite(duk_context *ctx);

// Timer bindings
duk_ret_t duk_timerAttach(duk_context *ctx);

// Communication bindings
duk_ret_t duk_udpSend(duk_context *ctx);
duk_ret_t duk_udpReceive(duk_context *ctx);
duk_ret_t duk_sendMessage(duk_context *ctx);
duk_ret_t duk_receiveMessage(duk_context *ctx);

// Register all bindings
void registerDuktapeBindings(duk_context *ctx, int vmIndex);

#endif // DUKTAPE_BINDINGS_H
