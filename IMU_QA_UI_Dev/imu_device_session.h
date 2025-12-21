#pragma once
#include "imu_types.h"
#include <simpleble/SimpleBLE.h>
#include <atomic>
#include <mutex>
#include <deque>

class ImuDeviceSession {
public:
    ImuDeviceSession(SimpleBLE::Peripheral peripheral,
                     const std::string& id);

    ~ImuDeviceSession();

    bool start();
    void stop();

    std::string id() const { return id_; }

    // Pull samples since last call (for QA processing)
    std::vector<ImuSample> drain_samples();

private:
    SimpleBLE::Peripheral peripheral_;
    std::string id_;

    std::atomic<bool> running_{false};

    std::mutex buffer_mutex_;
    std::deque<ImuSample> buffer_;   // unbounded is fine for 60s at 100 Hz

    void on_notify(SimpleBLE::ByteArray bytes);
    void send_cmd(uint8_t cmd, uint8_t len,
                  const std::vector<uint8_t>& payload);
};