// ============================================================================
// imu_windows_ble_scanner.cpp - COMPLETE REPLACEMENT
// ============================================================================

#ifdef _WIN32

#include "imu_windows_ble_scanner.h"
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <set>
#include <iostream>

using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::Advertisement;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace winrt::Windows::Foundation;

std::string address_to_string(uint64_t addr) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::uppercase
        << std::setw(2) << ((addr >> 40) & 0xFF) << ":"
        << std::setw(2) << ((addr >> 32) & 0xFF) << ":"
        << std::setw(2) << ((addr >> 24) & 0xFF) << ":"
        << std::setw(2) << ((addr >> 16) & 0xFF) << ":"
        << std::setw(2) << ((addr >> 8) & 0xFF) << ":"
        << std::setw(2) << (addr & 0xFF);
    return oss.str();
}

std::vector<WinBleDevice> WindowsBleScanner::scan_for(int duration_ms) {
    devices_.clear();
    std::set<uint64_t> seen_addresses;
    std::set<uint64_t> gmsync_without_name;
    
    watcher_ = BluetoothLEAdvertisementWatcher();
    watcher_.ScanningMode(BluetoothLEScanningMode::Active);
    
    watcher_.Received([this, &seen_addresses, &gmsync_without_name](
        BluetoothLEAdvertisementWatcher const&,
        BluetoothLEAdvertisementReceivedEventArgs args) {
        
        uint64_t addr = args.BluetoothAddress();
        
        std::lock_guard<std::mutex> lock(devices_mutex_);
        if (seen_addresses.count(addr)) return;
        
        auto localName = args.Advertisement().LocalName();
        std::string name = winrt::to_string(localName);
        
        if (name.find("GMSync") != std::string::npos) {
            seen_addresses.insert(addr);
            WinBleDevice device;
            device.name = name;
            device.address = address_to_string(addr);
            device.raw_address = addr;
            devices_.push_back(device);
            std::cout << "Found GMSync: " << name << " [" << device.address << "]\n";
        } else if (name.empty()) {
            gmsync_without_name.insert(addr);
        }
    });
    
    watcher_.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    
    std::cout << "Fetching names for devices without LocalName...\n";
    for (uint64_t addr : gmsync_without_name) {
        try {
            auto async_device = BluetoothLEDevice::FromBluetoothAddressAsync(addr);
            auto device = async_device.get();
            
            if (device) {
                std::string name = winrt::to_string(device.Name());
                if (name.find("GMSync") != std::string::npos) {
                    std::lock_guard<std::mutex> lock(devices_mutex_);
                    if (!seen_addresses.count(addr)) {
                        seen_addresses.insert(addr);
                        WinBleDevice win_dev;
                        win_dev.name = name;
                        win_dev.address = address_to_string(addr);
                        win_dev.raw_address = addr;
                        devices_.push_back(win_dev);
                        std::cout << "Found GMSync (via device): " << name << " [" << win_dev.address << "]\n";
                    }
                }
                device.Close();
            }
        } catch (...) {
        }
    }
    
    stop();
    
    std::lock_guard<std::mutex> lock(devices_mutex_);
    return devices_;
}

void WindowsBleScanner::stop() {
    if (watcher_) {
        watcher_.Stop();
        watcher_ = nullptr;
    }
}

#endif // _WIN32