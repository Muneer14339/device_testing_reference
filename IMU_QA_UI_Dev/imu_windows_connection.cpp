// ============================================================================
// imu_windows_connection.cpp - COMPLETE REPLACEMENT
// ============================================================================

#ifdef _WIN32

#include "imu_windows_connection.h"
#include <iostream>
#include <chrono>
#include <winrt/Windows.Foundation.Collections.h>

using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Foundation;

static int16_t be16(const uint8_t* p) {
    return static_cast<int16_t>((p[0] << 8) | p[1]);
}

WindowsImuConnection::WindowsImuConnection(uint64_t address, const std::string& id)
    : address_(address), id_(id) {}

WindowsImuConnection::~WindowsImuConnection() {
    stop();
}

bool WindowsImuConnection::start() {
    try {
        auto async_device = BluetoothLEDevice::FromBluetoothAddressAsync(address_);
        device_ = async_device.get();
        
        if (!device_) {
            std::cerr << "[" << id_ << "] Failed to get device\n";
            return false;
        }

        winrt::guid service_uuid(0x0000b3a0, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb});
        auto services_async = device_.GetGattServicesForUuidAsync(service_uuid);
        auto services_result = services_async.get();
        
        if (services_result.Status() != GattCommunicationStatus::Success) {
            std::cerr << "[" << id_ << "] Service query failed\n";
            return false;
        }

        auto services = services_result.Services();
        uint32_t service_count = services.Size();
        
        if (service_count == 0) {
            std::cerr << "[" << id_ << "] Service not found\n";
            return false;
        }

        auto service = services.GetAt(0);

        winrt::guid notify_uuid(0x0000b3a1, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb});
        auto notify_async = service.GetCharacteristicsForUuidAsync(notify_uuid);
        auto notify_result = notify_async.get();
        
        if (notify_result.Status() != GattCommunicationStatus::Success) {
            std::cerr << "[" << id_ << "] Notify characteristic query failed\n";
            return false;
        }

        auto notify_chars = notify_result.Characteristics();
        if (notify_chars.Size() == 0) {
            std::cerr << "[" << id_ << "] Notify characteristic not found\n";
            return false;
        }
        notify_char_ = notify_chars.GetAt(0);

        winrt::guid write_uuid(0x0000b3a2, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb});
        auto write_async = service.GetCharacteristicsForUuidAsync(write_uuid);
        auto write_result = write_async.get();
        
        if (write_result.Status() != GattCommunicationStatus::Success) {
            std::cerr << "[" << id_ << "] Write characteristic query failed\n";
            return false;
        }

        auto write_chars = write_result.Characteristics();
        if (write_chars.Size() == 0) {
            std::cerr << "[" << id_ << "] Write characteristic not found\n";
            return false;
        }
        write_char_ = write_chars.GetAt(0);

        notify_char_.ValueChanged([this](GattCharacteristic const&, GattValueChangedEventArgs args) {
            auto reader = DataReader::FromBuffer(args.CharacteristicValue());
            std::vector<uint8_t> data(reader.UnconsumedBufferLength());
            if (!data.empty()) {
                reader.ReadBytes(data);
                on_notify(data);
            }
        });

        auto cccd_async = notify_char_.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::Notify);
        auto cccd_result = cccd_async.get();
        
        if (cccd_result != GattCommunicationStatus::Success) {
            std::cerr << "[" << id_ << "] Failed to enable notifications\n";
            return false;
        }

        send_cmd(0x08, 0x00, {});
        send_cmd(0x0A, 0x00, {});

        running_ = true;
        return true;

    } catch (const winrt::hresult_error& e) {
        std::cerr << "[" << id_ << "] Exception: " << winrt::to_string(e.message()) << "\n";
        return false;
    }
}

void WindowsImuConnection::stop() {
    if (!running_) return;
    running_ = false;

    try {
        if (write_char_) {
            send_cmd(0xF0, 0x00, {});
        }
    } catch (...) {}

    if (device_) {
        device_.Close();
        device_ = nullptr;
    }
}

void WindowsImuConnection::send_cmd(uint8_t cmd, uint8_t len, const std::vector<uint8_t>& payload) {
    if (!write_char_) return;

    std::vector<uint8_t> buf;
    buf.reserve(4 + payload.size());
    buf.push_back(0x55);
    buf.push_back(0xAA);
    buf.push_back(cmd);
    buf.push_back(len);
    buf.insert(buf.end(), payload.begin(), payload.end());

    auto writer = DataWriter();
    writer.WriteBytes(buf);
    auto buffer = writer.DetachBuffer();

    auto write_async = write_char_.WriteValueAsync(buffer);
    write_async.get();
}

void WindowsImuConnection::on_notify(const std::vector<uint8_t>& data) {
    if (data.size() < 10) return;
    const uint8_t* d = data.data();
    if (d[0] != 0x55 || d[1] != 0xAA) return;

    uint8_t cmd = d[2];
    uint8_t len = d[3];
    if (len != 0x06) return;

    const uint8_t* p = d + 4;
    int16_t rx = be16(p);
    int16_t ry = be16(p + 2);
    int16_t rz = be16(p + 4);

    double t = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    ImuSample s{};
    s.timestamp_s = t;
    s.temp = 0.0f;

    if (cmd == 0x08) {
        s.ax = 16.0f * rx / 32768.0f;
        s.ay = 16.0f * ry / 32768.0f;
        s.az = 16.0f * rz / 32768.0f;
    } else if (cmd == 0x0A) {
        s.gx = 500.0f * rx / 28571.0f;
        s.gy = 500.0f * ry / 28571.0f;
        s.gz = 500.0f * rz / 28571.0f;
    } else {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_.push_back(s);
    }
}

std::vector<ImuSample> WindowsImuConnection::drain_samples() {
    std::vector<ImuSample> out;
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    out.assign(buffer_.begin(), buffer_.end());
    buffer_.clear();
    return out;
}

#endif // _WIN32