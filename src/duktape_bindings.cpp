#include "include/duktape_bindings.h"
#include "include/vm_manager.h"
#include "include/networking.h"

// === Core Bindings ===
duk_ret_t native_print(duk_context *ctx) {
    const char* message = duk_safe_to_string(ctx, -1);
    Serial.println(message);
    return 0;
}

duk_ret_t native_wait(duk_context *ctx) {
    if (duk_get_top(ctx) < 1) {
        duk_error(ctx, DUK_ERR_TYPE_ERROR, "wait() requires a duration argument");
        return DUK_RET_TYPE_ERROR;
    }
    
    duk_int_t duration = duk_get_int(ctx, -1);
    delay(duration);
    return 0;
}

duk_ret_t duk_delay(duk_context *ctx) {
    int ms = duk_require_int(ctx, 0);
    delay(ms);
    return 0;
}

duk_ret_t duk_digitalWrite(duk_context *ctx) {
    int vmIndex = -1;
    for (int i = 0; i < MAX_VMS; i++) {
        if (vms[i].ctx == ctx) {
            vmIndex = i;
            break;
        }
    }

    if (vmIndex < 0) {
        return DUK_ERR_ERROR;
    }

    int pin = duk_require_int(ctx, 0);
    int value = duk_require_int(ctx, 1);

    if (pin < 0 || pin >= 40) {
        return DUK_ERR_RANGE_ERROR;
    }

    if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE) {
        digitalWrite(pin, value);
        xSemaphoreGive(vms[vmIndex].pinMutex);
    }

    return 0;
}

duk_ret_t duk_digitalRead(duk_context *ctx) {
    int vmIndex = -1;
    for (int i = 0; i < MAX_VMS; i++) {
        if (vms[i].ctx == ctx) {
            vmIndex = i;
            break;
        }
    }

    if (vmIndex < 0) {
        return DUK_ERR_ERROR;
    }

    int pin = duk_require_int(ctx, 0);
    if (pin < 0 || pin >= 40) {
        return DUK_ERR_RANGE_ERROR;
    }

    int value = 0;
    if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE) {
        value = digitalRead(pin);
        xSemaphoreGive(vms[vmIndex].pinMutex);
    }

    duk_push_int(ctx, value);
    return 1;
}

duk_ret_t duk_analogRead(duk_context *ctx) {
    int vmIndex = -1;
    for (int i = 0; i < MAX_VMS; i++) {
        if (vms[i].ctx == ctx) {
            vmIndex = i;
            break;
        }
    }

    if (vmIndex < 0) {
        return DUK_ERR_ERROR;
    }

    int pin = duk_require_int(ctx, 0);
    if (pin < 0 || pin >= 40) {
        return DUK_ERR_RANGE_ERROR;
    }

    int value = 0;
    if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE) {
        value = analogRead(pin);
        xSemaphoreGive(vms[vmIndex].pinMutex);
    }

    duk_push_int(ctx, value);
    return 1;
}

duk_ret_t duk_analogWrite(duk_context *ctx) {
    int vmIndex = -1;
    for (int i = 0; i < MAX_VMS; i++) {
        if (vms[i].ctx == ctx) {
            vmIndex = i;
            break;
        }
    }

    if (vmIndex < 0) {
        return DUK_ERR_ERROR;
    }

    int pin = duk_require_int(ctx, 0);
    int value = duk_require_int(ctx, 1);

    if (pin < 0 || pin >= 40) {
        return DUK_ERR_RANGE_ERROR;
    }

    if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE) {
        analogWrite(pin, value);
        xSemaphoreGive(vms[vmIndex].pinMutex);
    }

    return 0;
}

duk_ret_t duk_pinMode(duk_context *ctx) {
    int vmIndex = -1;
    for (int i = 0; i < MAX_VMS; i++) {
        if (vms[i].ctx == ctx) {
            vmIndex = i;
            break;
        }
    }

    if (vmIndex < 0) {
        return DUK_ERR_ERROR;
    }

    int pin = duk_require_int(ctx, 0);
    int mode = duk_require_int(ctx, 1);

    if (pin < 0 || pin >= 40) {
        return DUK_ERR_RANGE_ERROR;
    }

    if (xSemaphoreTake(vms[vmIndex].pinMutex, portMAX_DELAY) == pdTRUE) {
        pinMode(pin, mode);
        xSemaphoreGive(vms[vmIndex].pinMutex);
    }

    return 0;
}

// === WiFi Functions ===
duk_ret_t duk_wifiConnect(duk_context *ctx) {
    const char* ssid = duk_require_string(ctx, 0);
    const char* password = duk_require_string(ctx, 1);
    
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        duk_push_true(ctx);
        return 1;
    }
    duk_push_false(ctx);
    return 1;
}

duk_ret_t duk_wifiDisconnect(duk_context *ctx) {
    WiFi.disconnect();
    return 0;
}

duk_ret_t duk_getIP(duk_context *ctx) {
    IPAddress ip = WiFi.localIP();
    char ipStr[16];
    sprintf(ipStr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    duk_push_string(ctx, ipStr);
    return 1;
}

// === I2C Functions ===
duk_ret_t duk_i2cBegin(duk_context *ctx) {
    int sda = duk_require_int(ctx, 0);
    int scl = duk_require_int(ctx, 1);
    int freq = duk_get_int_default(ctx, 2, 100000);
    
    Wire.begin(sda, scl, freq);
    return 0;
}

duk_ret_t duk_i2cWrite(duk_context *ctx) {
    int address = duk_require_int(ctx, 0);
    int value = duk_require_int(ctx, 1);
    
    Wire.beginTransmission(address);
    Wire.write(value);
    int error = Wire.endTransmission();
    
    duk_push_int(ctx, error);
    return 1;
}

duk_ret_t duk_i2cRead(duk_context *ctx) {
    int address = duk_require_int(ctx, 0);
    int bytes = duk_require_int(ctx, 1);
    
    Wire.requestFrom(address, bytes);
    
    duk_idx_t arr_idx = duk_push_array(ctx);
    int i = 0;
    
    while (Wire.available()) {
        duk_push_int(ctx, Wire.read());
        duk_put_prop_index(ctx, arr_idx, i++);
    }
    
    return 1;
}

// === SPI Functions ===
duk_ret_t duk_spiBegin(duk_context *ctx) {
    int sck = duk_require_int(ctx, 0);
    int miso = duk_require_int(ctx, 1);
    int mosi = duk_require_int(ctx, 2);
    int ss = duk_require_int(ctx, 3);
    
    SPI.begin(sck, miso, mosi, ss);
    return 0;
}

duk_ret_t duk_spiTransfer(duk_context *ctx) {
    if (!duk_is_array(ctx, 0)) return -1;
    
    duk_get_prop_string(ctx, 0, "length");
    int length = duk_get_int(ctx, -1);
    duk_pop(ctx);
    
    uint8_t* txData = (uint8_t*)malloc(length);
    uint8_t* rxData = (uint8_t*)malloc(length);
    
    for (int i = 0; i < length; i++) {
        duk_get_prop_index(ctx, 0, i);
        txData[i] = duk_get_int(ctx, -1);
        duk_pop(ctx);
    }
    
    SPI.transferBytes(txData, rxData, length);
    
    duk_idx_t arr_idx = duk_push_array(ctx);
    for (int i = 0; i < length; i++) {
        duk_push_int(ctx, rxData[i]);
        duk_put_prop_index(ctx, arr_idx, i);
    }
    
    free(txData);
    free(rxData);
    return 1;
}

// === ADC Functions ===
duk_ret_t duk_adcConfig(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int resolution = duk_require_int(ctx, 1);
    int attenuation = duk_require_int(ctx, 2);
    
    analogReadResolution(resolution);
    analogSetAttenuation((adc_attenuation_t)attenuation);
    analogSetPinAttenuation(pin, (adc_attenuation_t)attenuation);
    return 0;
}

// === Touch Sensor Functions ===
duk_ret_t duk_touchRead(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    duk_push_int(ctx, touchRead(pin));
    return 1;
}

duk_ret_t duk_touchAttachInterrupt(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int threshold = duk_require_int(ctx, 1);
    
    touchAttachInterrupt(pin, [](){}, threshold);
    return 0;
}

// === RTC Functions ===
duk_ret_t duk_rtcGetTime(duk_context *ctx) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    duk_idx_t obj_idx = duk_push_object(ctx);
    
    duk_push_int(ctx, tv.tv_sec);
    duk_put_prop_string(ctx, obj_idx, "seconds");
    
    duk_push_int(ctx, tv.tv_usec);
    duk_put_prop_string(ctx, obj_idx, "microseconds");
    
    return 1;
}

// === Sleep Functions ===
duk_ret_t duk_deepSleep(duk_context *ctx) {
    uint64_t sleepTime = duk_require_int(ctx, 0);
    esp_sleep_enable_timer_wakeup(sleepTime * 1000000ULL);
    esp_deep_sleep_start();
    return 0;
}

duk_ret_t duk_lightSleep(duk_context *ctx) {
    uint64_t sleepTime = duk_require_int(ctx, 0);
    esp_sleep_enable_timer_wakeup(sleepTime * 1000000ULL);
    esp_light_sleep_start();
    return 0;
}

// === LED Control Functions ===
duk_ret_t duk_ledcSetup(duk_context *ctx) {
    int channel = duk_require_int(ctx, 0);
    int frequency = duk_require_int(ctx, 1);
    int resolution = duk_require_int(ctx, 2);
    
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)resolution,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    return 0;
}

duk_ret_t duk_ledcAttachPin(duk_context *ctx) {
    int pin = duk_require_int(ctx, 0);
    int channel = duk_require_int(ctx, 1);
    
    ledc_channel_config_t ledc_channel = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)channel,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);
    return 0;
}

duk_ret_t duk_ledcWrite(duk_context *ctx) {
    int channel = duk_require_int(ctx, 0);
    int duty = duk_require_int(ctx, 1);
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    return 0;
}

// === Timer Functions ===
duk_ret_t duk_timerAttach(duk_context *ctx) {
    int timerNum = duk_require_int(ctx, 0);
    int divider = duk_require_int(ctx, 1);
    bool countUp = duk_require_boolean(ctx, 2);
    
    hw_timer_t* timer = timerBegin(80000000 / divider); // Convert divider to frequency
    if (timer == NULL) {
        duk_push_false(ctx);
    } else {
        duk_push_true(ctx);
    }
    return 1;
}

// === Communication Functions ===
duk_ret_t duk_udpSend(duk_context *ctx) {
    const char *ipStr = duk_require_string(ctx, 0);
    uint16_t port = duk_require_int(ctx, 1);
    const char *msg = duk_require_string(ctx, 2);
    int vmIndex = duk_require_int(ctx, 3);

    IPAddress ip;
    if (!ip.fromString(ipStr)) {
        Serial.printf("VM %d: Invalid IP address\n", vmIndex);
        return -1;
    }

    udp.beginPacket(ip, port);
    udp.write((const uint8_t *)msg, strlen(msg));
    udp.endPacket();
    return 0;
}

duk_ret_t duk_udpReceive(duk_context *ctx) {
    if (udp.parsePacket()) {
        char packetBuffer[MAX_MESSAGE_LENGTH];
        int bytesRead = udp.read(packetBuffer, MAX_MESSAGE_LENGTH - 1);
        if (bytesRead > 0) {
            packetBuffer[bytesRead] = '\0';
            duk_push_string(ctx, packetBuffer);
            return 1;
        }
    }
    duk_push_null(ctx);
    return 1;
}

duk_ret_t duk_sendMessage(duk_context *ctx) {
    int receiverID = duk_require_int(ctx, 0);
    const char* message = duk_require_string(ctx, 1);

    if (receiverID < 0 || receiverID >= MAX_VMS || !vms[receiverID].running) {
        duk_push_boolean(ctx, false);
        return 1;
    }

    if (!vms[receiverID].messageQueue) {
        duk_push_boolean(ctx, false);
        return 1;
    }

    char msg[MAX_MESSAGE_LENGTH];
    strncpy(msg, message, MAX_MESSAGE_LENGTH - 1);
    msg[MAX_MESSAGE_LENGTH - 1] = '\0';

    if (xQueueSend(vms[receiverID].messageQueue, msg, 0) != pdTRUE) {
        duk_push_boolean(ctx, false);
        return 1;
    }

    duk_push_boolean(ctx, true);
    return 1;
}

duk_ret_t duk_receiveMessage(duk_context *ctx) {
    int vmIndex = -1;
    for (int i = 0; i < MAX_VMS; i++) {
        if (vms[i].ctx == ctx) {
            vmIndex = i;
            break;
        }
    }

    if (vmIndex < 0 || !vms[vmIndex].messageQueue) {
        duk_push_null(ctx);
        return 1;
    }

    char msg[MAX_MESSAGE_LENGTH];
    if (xQueueReceive(vms[vmIndex].messageQueue, msg, 0) == pdTRUE) {
        duk_push_string(ctx, msg);
    } else {
        duk_push_null(ctx);
    }
    return 1;
}

// === Register All Bindings ===
void registerDuktapeBindings(duk_context *ctx, int vmIndex) {
    // Register print function
    duk_push_c_function(ctx, native_print, 1);
    duk_put_global_string(ctx, "print");
    
    // Register wait/delay functions
    duk_push_c_function(ctx, native_wait, 1);
    duk_put_global_string(ctx, "wait");
    duk_push_c_function(ctx, native_wait, 1);
    duk_put_global_string(ctx, "delay");

    // GPIO bindings
    duk_push_c_function(ctx, duk_digitalWrite, 2);
    duk_put_global_string(ctx, "digitalWrite");

    duk_push_c_function(ctx, duk_digitalRead, 1);
    duk_put_global_string(ctx, "digitalRead");

    duk_push_c_function(ctx, duk_analogRead, 1);
    duk_put_global_string(ctx, "analogRead");

    duk_push_c_function(ctx, duk_analogWrite, 2);
    duk_put_global_string(ctx, "analogWrite");

    duk_push_c_function(ctx, duk_pinMode, 2);
    duk_put_global_string(ctx, "pinMode");

    // WiFi bindings
    duk_push_c_function(ctx, duk_wifiConnect, 2);
    duk_put_global_string(ctx, "wifiConnect");
    
    duk_push_c_function(ctx, duk_wifiDisconnect, 0);
    duk_put_global_string(ctx, "wifiDisconnect");
    
    duk_push_c_function(ctx, duk_getIP, 0);
    duk_put_global_string(ctx, "getIP");
    
    // I2C bindings
    duk_push_c_function(ctx, duk_i2cBegin, 3);
    duk_put_global_string(ctx, "i2cBegin");
    
    duk_push_c_function(ctx, duk_i2cWrite, 2);
    duk_put_global_string(ctx, "i2cWrite");
    
    duk_push_c_function(ctx, duk_i2cRead, 2);
    duk_put_global_string(ctx, "i2cRead");
    
    // SPI bindings
    duk_push_c_function(ctx, duk_spiBegin, 4);
    duk_put_global_string(ctx, "spiBegin");
    
    duk_push_c_function(ctx, duk_spiTransfer, 1);
    duk_put_global_string(ctx, "spiTransfer");
    
    // ADC bindings
    duk_push_c_function(ctx, duk_adcConfig, 3);
    duk_put_global_string(ctx, "adcConfig");
    
    // Touch sensor bindings
    duk_push_c_function(ctx, duk_touchRead, 1);
    duk_put_global_string(ctx, "touchRead");
    
    duk_push_c_function(ctx, duk_touchAttachInterrupt, 2);
    duk_put_global_string(ctx, "touchAttachInterrupt");
    
    // RTC bindings
    duk_push_c_function(ctx, duk_rtcGetTime, 0);
    duk_put_global_string(ctx, "rtcGetTime");
    
    // Sleep bindings
    duk_push_c_function(ctx, duk_deepSleep, 1);
    duk_put_global_string(ctx, "deepSleep");
    
    duk_push_c_function(ctx, duk_lightSleep, 1);
    duk_put_global_string(ctx, "lightSleep");
    
    // LED bindings
    duk_push_c_function(ctx, duk_ledcSetup, 3);
    duk_put_global_string(ctx, "ledcSetup");
    
    duk_push_c_function(ctx, duk_ledcAttachPin, 2);
    duk_put_global_string(ctx, "ledcAttachPin");
    
    duk_push_c_function(ctx, duk_ledcWrite, 2);
    duk_put_global_string(ctx, "ledcWrite");
    
    // Timer bindings
    duk_push_c_function(ctx, duk_timerAttach, 3);
    duk_put_global_string(ctx, "timerAttach");

    // Communication bindings
    duk_push_c_function(ctx, duk_udpSend, 4);
    duk_put_global_string(ctx, "udpSend");

    duk_push_c_function(ctx, duk_udpReceive, 0);
    duk_put_global_string(ctx, "udpReceive");

    duk_push_c_function(ctx, duk_sendMessage, 2);
    duk_put_global_string(ctx, "sendMessage");

    duk_push_c_function(ctx, duk_receiveMessage, 1);
    duk_put_global_string(ctx, "receiveMessage");

    // Store VM index in global object
    duk_push_global_object(ctx);
    duk_push_int(ctx, vmIndex);
    duk_put_prop_string(ctx, -2, "\xFF\xFFvm_index");
    duk_pop(ctx);

    // Test the bindings
    String testCode = "print('VM " + String(vmIndex) + " initialized')";
    duk_push_string(ctx, testCode.c_str());
    if (duk_peval(ctx) != 0) {
        Serial.printf("VM %d initialization failed: %s\n", vmIndex, duk_safe_to_string(ctx, -1));
    }
    duk_pop(ctx);
}
