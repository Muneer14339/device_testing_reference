// ============================================================================
// imu_windows_ble_scanner.cpp - UPDATED
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

std::vector<WinBleDevice> WindowsBleScanner::scan_until(int target_count) {
    devices_.clear();
    found_count_ = 0;
    should_stop_ = false;
    
    std::set<uint64_t> seen_addresses;
    std::map<uint64_t, std::chrono::steady_clock::time_point> pending_devices;
    
    watcher_ = BluetoothLEAdvertisementWatcher();
    watcher_.ScanningMode(BluetoothLEScanningMode::Active);
    
    watcher_.Received([this, &seen_addresses, &pending_devices, target_count](
        BluetoothLEAdvertisementWatcher const&,
        BluetoothLEAdvertisementReceivedEventArgs args) {
        
        if (should_stop_ || found_count_ >= target_count) return;
        
        uint64_t addr = args.BluetoothAddress();
        
        std::lock_guard<std::mutex> lock(devices_mutex_);
        if (seen_addresses.count(addr)) return;
        
        pending_devices[addr] = std::chrono::steady_clock::now();
    });
    
    watcher_.Start();
    
    while (found_count_ < target_count && !should_stop_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        std::vector<uint64_t> to_check;
        {
            std::lock_guard<std::mutex> lock(devices_mutex_);
            auto now = std::chrono::steady_clock::now();
            
            for (auto it = pending_devices.begin(); it != pending_devices.end();) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count() >= 200) {
                    to_check.push_back(it->first);
                    it = pending_devices.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        for (uint64_t addr : to_check) {
            if (should_stop_ || found_count_ >= target_count) break;
            
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
                            found_count_++;
                            std::cout << "Found GMSync device " << found_count_ << "/" << target_count 
                                      << ": " << name << " [" << win_dev.address << "]\n";
                        }
                    }
                    device.Close();
                }
            } catch (...) {}
        }
    }
    
    stop();
    
    std::lock_guard<std::mutex> lock(devices_mutex_);
    return devices_;
}

void WindowsBleScanner::stop() {
    should_stop_ = true;
    if (watcher_) {
        watcher_.Stop();
        watcher_ = nullptr;
    }
}

#endif // _WIN32