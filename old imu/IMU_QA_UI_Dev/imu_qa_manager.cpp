// ============================================================================
// imu_qa_manager.cpp - UPDATED
// ============================================================================

#include "imu_qa_manager.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <cmath>


static void print_device_summary(
    const std::string& id,
    const std::vector<ImuSample>& samples,
    double test_seconds
) 
{
    std::cout << "\n----------------------------------------\n";
    std::cout << "Device: " << id << "\n";

    if (samples.empty()) {
        std::cout << "NO DATA RECEIVED ❌\n";
        return;
    }

    size_t total = samples.size();
    double rate  = total / test_seconds;

    float min_ax = samples[0].ax, max_ax = samples[0].ax;
    float min_ay = samples[0].ay, max_ay = samples[0].ay;
    float min_az = samples[0].az, max_az = samples[0].az;

    for (const auto& s : samples) {
        min_ax = std::min(min_ax, s.ax);
        max_ax = std::max(max_ax, s.ax);

        min_ay = std::min(min_ay, s.ay);
        max_ay = std::max(max_ay, s.ay);

        min_az = std::min(min_az, s.az);
        max_az = std::max(max_az, s.az);
    }

    std::cout << "Total packets : " << total << "\n";
    std::cout << "Avg rate      : " << rate << " Hz\n";

    std::cout << "AX min/max    : " << min_ax << " / " << max_ax << "\n";
    std::cout << "AY min/max    : " << min_ay << " / " << max_ay << "\n";
    std::cout << "AZ min/max    : " << min_az << " / " << max_az << "\n";
}


ImuQaManager::ImuQaManager(const ImuQaConfig& cfg) : cfg_(cfg) {}

bool ImuQaManager::discover_and_connect(int max_devices) {
#ifdef _WIN32
    std::cout << "Scanning for " << max_devices << " GMSync device(s)...\n";
    std::cout << "Press ESC or Backspace to cancel\n\n";
    
    WindowsBleScanner scanner;
    auto found_devices = scanner.scan_until(max_devices);
    
    if (found_devices.empty()) {
        std::cerr << "\nNo GMSync devices found.\n";
        return false;
    }
    
    std::cout << "\nConnecting to devices...\n";
    
    for (const auto& device : found_devices) {
        std::cout << "Connecting to " << device.name << " [" << device.address << "]... ";
        
        auto session = std::make_unique<ImuDeviceSession>(device.raw_address, device.address);
        if (session->start()) {
            sessions_.push_back(std::move(session));
            std::cout << "OK\n";
        } else {
            std::cerr << "FAILED\n";
        }
    }
    
#else
    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty()) {
        std::cerr << "No BLE adapters found.\n";
        return false;
    }

    SimpleBLE::Adapter& adapter = adapters[0];
    std::cout << "Using adapter: " << adapter.identifier()
              << " (" << adapter.address() << ")\n";

    std::vector<SimpleBLE::Peripheral> found;

    adapter.set_callback_on_scan_found([&](SimpleBLE::Peripheral p) {
        std::string name = p.identifier();
        if (name.find("GMSync") == std::string::npos) return;
        if ((int)found.size() >= max_devices) return;
        
        found.push_back(p);
        std::cout << "Found GMSync device " << found.size() << "/" << max_devices 
                  << ": " << name << " [" << p.address() << "]\n";
    });

    adapter.scan_for(10000);

    if (found.empty()) {
        std::cerr << "No GMSync devices found.\n";
        return false;
    }

    for (auto& p : found) {
        auto id = p.address();
        auto session = std::make_unique<ImuDeviceSession>(p, id);
        if (!session->start()) {
            std::cerr << "[" << id << "] start() failed, skipping.\n";
            continue;
        }
        sessions_.push_back(std::move(session));
    }
#endif

    if (sessions_.empty()) {
        std::cerr << "\nNo sessions started.\n";
        return false;
    }

    std::cout << "\nStarted " << sessions_.size() << " device session(s).\n";
    return true;
}

std::vector<ImuQaResult> ImuQaManager::run_test() {
    using clock = std::chrono::steady_clock;

    auto t0 = clock::now();
    auto settle_end = t0 + std::chrono::duration<double>(cfg_.settle_seconds);
    auto test_end   = settle_end + std::chrono::duration<double>(cfg_.test_seconds);

    std::cout << "\nSettling for " << cfg_.settle_seconds << "s...\n";
    while (clock::now() < settle_end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Collecting samples for " << cfg_.test_seconds << "s...\n";

    std::vector<std::vector<ImuSample>> all_samples(sessions_.size());

    while (clock::now() < test_end) {
        for (size_t i = 0; i < sessions_.size(); ++i) {
            auto chunk = sessions_[i]->drain_samples();
            all_samples[i].insert(all_samples[i].end(),
                                  chunk.begin(), chunk.end());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "Test window ended. Evaluating...\n";

    std::vector<ImuQaResult> results;
    for (size_t i = 0; i < sessions_.size(); ++i) {
        auto id = sessions_[i]->id();
         // ✅ PRINT RAW DATA SUMMARY
        print_device_summary(
          id,
          all_samples[i],
          cfg_.test_seconds
        );
        auto res = evaluate_device(id, all_samples[i]);
        results.push_back(res);
        sessions_[i]->stop();
    }

    return results;
}


ImuQaResult ImuQaManager::evaluate_device(
    const std::string& id,
    const std::vector<ImuSample>& samples
) {
    ImuQaResult res{};
    res.device_id = id;

    if (samples.empty()) {
        res.status            = QaStatus::FAIL;
        res.mac_deg           = 0.0;
        res.noise_sigma       = 0.0;
        res.drift_deg_per_min = 0.0;
        res.gravity_mean_g    = 0.0;
        res.abnormal_count    = 0;
        return res;
    }

    double sum_g = 0.0;
    int count_g  = 0;

    for (const auto& s : samples) {
        if (s.ax != 0.0f || s.ay != 0.0f || s.az != 0.0f) {
            double mag = std::sqrt(
                double(s.ax) * s.ax +
                double(s.ay) * s.ay +
                double(s.az) * s.az
            );
            sum_g += mag;
            ++count_g;
        }
    }

    if (count_g > 0) {
        res.gravity_mean_g = sum_g / count_g;
    } else {
        res.gravity_mean_g = 0.0;
    }

    res.mac_deg           = 0.0;
    res.noise_sigma       = 0.0;
    res.drift_deg_per_min = 0.0;
    res.abnormal_count    = 0;
    res.status            = QaStatus::PASS;

    return res;
}