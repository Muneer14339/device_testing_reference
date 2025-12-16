// ============================================================================
// imu_windows_ble_scanner.h - COMPLETE REPLACEMENT
// ============================================================================

#pragma once

#ifdef _WIN32

#include <string>
#include <vector>
#include <mutex>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

struct WinBleDevice {
    std::string name;
    std::string address;
    uint64_t raw_address;
};

class WindowsBleScanner {
public:
    WindowsBleScanner() = default;
    ~WindowsBleScanner() { stop(); }

    std::vector<WinBleDevice> scan_for(int duration_ms);
    void stop();

private:
    winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher watcher_{nullptr};
    std::vector<WinBleDevice> devices_;
    std::mutex devices_mutex_;
};

#endif // _WIN32