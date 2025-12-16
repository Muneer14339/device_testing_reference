// ============================================================================
// imu_windows_connection.h - ADD include
// ============================================================================

#pragma once

#ifdef _WIN32

#include "imu_types.h"
#include <string>
#include <atomic>
#include <mutex>
#include <deque>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>

class WindowsImuConnection {
public:
    WindowsImuConnection(uint64_t address, const std::string& id);
    ~WindowsImuConnection();

    bool start();
    void stop();
    std::string id() const { return id_; }
    std::vector<ImuSample> drain_samples();

private:
    uint64_t address_;
    std::string id_;
    std::atomic<bool> running_{false};

    winrt::Windows::Devices::Bluetooth::BluetoothLEDevice device_{nullptr};
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic notify_char_{nullptr};
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic write_char_{nullptr};

    std::mutex buffer_mutex_;
    std::deque<ImuSample> buffer_;

    void send_cmd(uint8_t cmd, uint8_t len, const std::vector<uint8_t>& payload);
    void on_notify(const std::vector<uint8_t>& data);
};

#endif // _WIN32