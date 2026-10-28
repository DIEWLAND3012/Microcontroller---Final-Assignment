// TimeManager.hpp - System time owner: DS1307 at boot, Wi-Fi + SNTP when online
#pragma once
#include <atomic>
#include <ctime>
#include "DS1307.hpp"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

namespace Clock {

class TimeManager {
public:
    explicit TimeManager(DS1307& rtc) : rtc_(rtc) {}

    // Applies the TZ and loads the system clock from the DS1307
    void initSystemTime(const char* tz);

    // Starts Wi-Fi STA and prepares SNTP (it starts once an IP is obtained)
    bool startWifi(const char* ssid, const char* pass, uint8_t channel, const char* ntpServer);

    [[noreturn]] void taskNtpLoop();  // FreeRTOS task body
    void requestSync();               // ask the NTP task for an immediate re-sync

    bool now(struct tm& out) const;        // local time
    bool setLocalTime(const struct tm& t); // system clock + DS1307

    bool isWifiConnected() const;
    bool isNtpSynced() const { return ntpSynced_; }
    bool isRtcValid() const { return rtcValid_; }

private:
    static constexpr EventBits_t WIFI_UP = BIT0;      // state, never auto-cleared
    static constexpr EventBits_t GOT_IP_EVT = BIT1;
    static constexpr EventBits_t SYNC_REQ_EVT = BIT2;
    static constexpr EventBits_t SYNC_DONE_EVT = BIT3;

    static void onNetEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    static void onTimeSync(struct timeval* tv);
    void writeRtcFromSystem();

    static TimeManager* instance_;  // SNTP callback has no user argument

    DS1307& rtc_;
    EventGroupHandle_t events_{nullptr};
    std::atomic<bool> ntpSynced_{false};
    std::atomic<bool> rtcValid_{false};
    bool sntpStarted_{false};
};

}  // namespace Clock
