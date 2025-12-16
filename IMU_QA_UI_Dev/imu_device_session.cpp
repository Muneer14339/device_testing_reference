// ============================================================================
// imu_device_session.cpp - COMPLETE REPLACEMENT
// ============================================================================

#include "imu_device_session.h"

#ifdef _WIN32

ImuDeviceSession::ImuDeviceSession(uint64_t address, const std::string& id)
    : id_(id) {
    connection_ = std::make_unique<WindowsImuConnection>(address, id);
}

ImuDeviceSession::~ImuDeviceSession() {
    stop();
}

bool ImuDeviceSession::start() {
    return connection_->start();
}

void ImuDeviceSession::stop() {
    connection_->stop();
}

std::vector<ImuSample> ImuDeviceSession::drain_samples() {
    return connection_->drain_samples();
}

#else

#include <chrono>
#include <iostream>

static int16_t be16(const uint8_t* p) {
    return static_cast<int16_t>((p[0] << 8) | p[1]);
}

ImuDeviceSession::ImuDeviceSession(SimpleBLE::Peripheral peripheral,
                                   const std::string& id)
    : peripheral_(std::move(peripheral)), id_(id) {}

ImuDeviceSession::~ImuDeviceSession() {
    stop();
}

bool ImuDeviceSession::start() {
    try {
        peripheral_.connect();
    } catch (const SimpleBLE::Exception::OperationFailed& e) {
        std::cerr << "[" << id_ << "] Connect failed: " << e.what() << "\n";
        return false;
    }
    if (!peripheral_.is_connected()) {
        std::cerr << "[" << id_ << "] Not connected after connect().\n";
        return false;
    }

    auto cb = [this](SimpleBLE::ByteArray bytes) {
        this->on_notify(bytes);
    };

    peripheral_.notify("0000b3a0-0000-1000-8000-00805f9b34fb",
                       "0000b3a1-0000-1000-8000-00805f9b34fb",
                       cb);

    send_cmd(0x08, 0x00, {});
    send_cmd(0x0A, 0x00, {});

    running_ = true;
    return true;
}

void ImuDeviceSession::stop() {
    if (!running_) return;
    running_ = false;

    try {
        send_cmd(0xF0, 0x00, {});
    } catch (...) {}

    try {
        peripheral_.unsubscribe("0000b3a0-0000-1000-8000-00805f9b34fb",
                                "0000b3a1-0000-1000-8000-00805f9b34fb");
    } catch (...) {}

    if (peripheral_.is_connected()) {
        try { peripheral_.disconnect(); } catch (...) {}
    }
}

void ImuDeviceSession::send_cmd(uint8_t cmd, uint8_t len,
                                const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> buf;
    buf.reserve(4 + payload.size());
    buf.push_back(0x55);
    buf.push_back(0xAA);
    buf.push_back(cmd);
    buf.push_back(len);
    buf.insert(buf.end(), payload.begin(), payload.end());

    peripheral_.write_request("0000b3a0-0000-1000-8000-00805f9b34fb",
                              "0000b3a2-0000-1000-8000-00805f9b34fb",
                              buf);
}

void ImuDeviceSession::on_notify(SimpleBLE::ByteArray bytes) {
    if (bytes.size() < 10) return;
    const uint8_t* d = reinterpret_cast<const uint8_t*>(bytes.data());
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

std::vector<ImuSample> ImuDeviceSession::drain_samples() {
    std::vector<ImuSample> out;
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    out.assign(buffer_.begin(), buffer_.end());
    buffer_.clear();
    return out;
}

#endif