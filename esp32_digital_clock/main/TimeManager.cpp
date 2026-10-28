// TimeManager.cpp - Wi-Fi station, SNTP sync and DS1307 fallback
#include "TimeManager.hpp"
#include <cstdlib>
#include <cstring>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_wifi.h"

namespace Clock {

static const char* TAG = "TimeMgr";

TimeManager* TimeManager::instance_ = nullptr;

void TimeManager::initSystemTime(const char* tz) {
    setenv("TZ", tz, 1);
    tzset();

    DateTime dt;
    if (rtc_.getTime(dt)) {
        struct tm t;
        DS1307::toTm(dt, t);
        timeval tv = {mktime(&t), 0};  // DS1307 holds local time
        settimeofday(&tv, nullptr);
        rtcValid_ = true;
        ESP_LOGI(TAG, "System time loaded from DS1307: %04u-%02u-%02u %02u:%02u:%02u", dt.year,
                 dt.month, dt.dayOfMonth, dt.hour, dt.minute, dt.second);
    } else {
        ESP_LOGW(TAG, "DS1307 time unavailable, waiting for NTP");
    }
}

bool TimeManager::startWifi(const char* ssid, const char* pass, uint8_t channel,
                            const char* ntpServer) {
    instance_ = this;
    events_ = xEventGroupCreate();
    if (!events_) return false;

    esp_err_t err = esp_netif_init();
    if (err == ESP_OK) err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return false;
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t initCfg = WIFI_INIT_CONFIG_DEFAULT();
    if ((err = esp_wifi_init(&initCfg)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return false;
    }
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onNetEvent, this, nullptr);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onNetEvent, this, nullptr);

    wifi_config_t wc = {};
    strlcpy(reinterpret_cast<char*>(wc.sta.ssid), ssid, sizeof(wc.sta.ssid));
    strlcpy(reinterpret_cast<char*>(wc.sta.password), pass, sizeof(wc.sta.password));
    wc.sta.channel = channel;
    wc.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    if ((err = esp_wifi_start()) != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return false;
    }

    // SNTP is configured now but started from the NTP task after GOT_IP
    esp_sntp_config_t sc = ESP_NETIF_SNTP_DEFAULT_CONFIG(ntpServer);
    sc.start = false;
    sc.sync_cb = onTimeSync;
    esp_netif_sntp_init(&sc);

    ESP_LOGI(TAG, "Connecting to Wi-Fi \"%s\" (NTP: %s)", ssid, ntpServer);
    return true;
}

void TimeManager::onNetEvent(void* arg, esp_event_base_t base, int32_t id, void* data) {
    auto* self = static_cast<TimeManager*>(arg);
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(self->events_, WIFI_UP);
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* ev = static_cast<ip_event_got_ip_t*>(data);
        ESP_LOGI(TAG, "Wi-Fi connected, IP " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupSetBits(self->events_, WIFI_UP | GOT_IP_EVT);
    }
}

void TimeManager::onTimeSync(struct timeval* tv) {
    if (instance_ && instance_->events_) xEventGroupSetBits(instance_->events_, SYNC_DONE_EVT);
}

void TimeManager::requestSync() {
    if (events_) xEventGroupSetBits(events_, SYNC_REQ_EVT);
}

void TimeManager::taskNtpLoop() {
    for (;;) {
        if (!events_) {  // Wi-Fi failed to start: nothing to do but stay alive
            vTaskDelay(portMAX_DELAY);
            continue;
        }
        EventBits_t bits = xEventGroupWaitBits(events_, GOT_IP_EVT | SYNC_REQ_EVT | SYNC_DONE_EVT,
                                               pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & GOT_IP_EVT) {
            if (!sntpStarted_) {
                esp_netif_sntp_start();
                sntpStarted_ = true;
            } else {
                esp_sntp_restart();
            }
            ESP_LOGI(TAG, "SNTP request sent");
        }
        if (bits & SYNC_REQ_EVT) {
            if (sntpStarted_ && isWifiConnected()) {
                ESP_LOGI(TAG, "Manual NTP re-sync requested");
                esp_sntp_restart();
            } else {
                ESP_LOGW(TAG, "Cannot sync: Wi-Fi not connected");
            }
        }
        if (bits & SYNC_DONE_EVT) {
            ntpSynced_ = true;
            writeRtcFromSystem();
        }
    }
}

void TimeManager::writeRtcFromSystem() {
    struct tm t;
    now(t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    if (rtc_.setTime(DS1307::fromTm(t))) {
        rtcValid_ = true;
        ESP_LOGI(TAG, "NTP sync OK: %s (DS1307 updated)", buf);
    } else {
        ESP_LOGI(TAG, "NTP sync OK: %s (DS1307 not written)", buf);
    }
}

bool TimeManager::now(struct tm& out) const {
    time_t t = time(nullptr);
    localtime_r(&t, &out);
    return out.tm_year >= (2020 - 1900);
}

bool TimeManager::setLocalTime(const struct tm& t) {
    struct tm copy = t;
    copy.tm_isdst = -1;
    timeval tv = {mktime(&copy), 0};  // mktime also normalises tm_wday
    settimeofday(&tv, nullptr);
    bool ok = rtc_.setTime(DS1307::fromTm(copy));
    if (ok) rtcValid_ = true;
    ESP_LOGI(TAG, "Time set manually to %02d:%02d:%02d (RTC %s)", copy.tm_hour, copy.tm_min,
             copy.tm_sec, ok ? "updated" : "not present");
    return ok;
}

bool TimeManager::isWifiConnected() const {
    return events_ && (xEventGroupGetBits(events_) & WIFI_UP);
}

}  // namespace Clock
