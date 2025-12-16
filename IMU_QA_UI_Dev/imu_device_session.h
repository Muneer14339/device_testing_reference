// ============================================================================
// imu_device_session.h - ADD Windows support
// ============================================================================

#pragma once
#include "imu_types.h"
#include <string>
#include <vector>

#ifdef _WIN32
#include "imu_windows_connection.h"
#else
#include <simpleble/SimpleBLE.h>
#include <atomic>
#include <mutex>
#include <deque>
#endif

class ImuDeviceSession {
public:
#ifdef _WIN32
    ImuDeviceSession(uint64_t address, const std::string& id);
#else
    ImuDeviceSession(SimpleBLE::Peripheral peripheral, const std::string& id);
#endif

    ~ImuDeviceSession();

    bool start();
    void stop();

    std::string id() const { return id_; }
    std::vector<ImuSample> drain_samples();

private:
#ifdef _WIN32
    std::unique_ptr<WindowsImuConnection> connection_;
    std::string id_;
#else
    SimpleBLE::Peripheral peripheral_;
    std::string id_;
    std::atomic<bool> running_{false};
    std::mutex buffer_mutex_;
    std::deque<ImuSample> buffer_;
    
    void on_notify(SimpleBLE::ByteArray bytes);
    void send_cmd(uint8_t cmd, uint8_t len, const std::vector<uint8_t>& payload);
#endif
};