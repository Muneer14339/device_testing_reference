#include "imu_qa_manager.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>   // ← needed for std::this_thread::sleep_for
#include <cmath>    // for sqrt if you use it

ImuQaManager::ImuQaManager(const ImuQaConfig& cfg) : cfg_(cfg) {}

bool ImuQaManager::discover_and_connect(int max_devices) {
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

        std::cout << "Found GMSync[" << (found.size()-1)
                  << "]: " << name << " [" << p.address() << "]\n";
    });

    adapter.scan_for(10000); // 10s scan

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

    if (sessions_.empty()) {
        std::cerr << "No sessions started.\n";
        return false;
    }

    std::cout << "Started " << sessions_.size()
              << " device sessions.\n";
    return true;
}

std::vector<ImuQaResult> ImuQaManager::run_test() {
    using clock = std::chrono::steady_clock;

    auto t0 = clock::now();
    auto settle_end = t0 + std::chrono::duration<double>(cfg_.settle_seconds);
    auto test_end   = settle_end + std::chrono::duration<double>(cfg_.test_seconds);

    std::cout << "Settling for " << cfg_.settle_seconds << "s...\n";
    while (clock::now() < settle_end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Collecting samples for " << cfg_.test_seconds << "s...\n";

    // Per-device sample accumulation
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
        // If we have no data at all, treat as FAIL (or whatever you want)
        res.status            = QaStatus::FAIL;
        res.mac_deg           = 0.0;
        res.noise_sigma       = 0.0;
        res.drift_deg_per_min = 0.0;
        res.gravity_mean_g    = 0.0;
        res.abnormal_count    = 0;
        return res;
    }

    // --- Very simple placeholder metrics ---
    // Example: average gravity magnitude from accel samples
    double sum_g = 0.0;
    int count_g  = 0;

    for (const auto& s : samples) {
        // if accel is populated (you may refine this condition)
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

    // For now, just fill defaults and mark as PASS.
    // Later we’ll plug in the real MAC / noise / drift logic.
    res.mac_deg           = 0.0;
    res.noise_sigma       = 0.0;
    res.drift_deg_per_min = 0.0;
    res.abnormal_count    = 0;
    res.status            = QaStatus::PASS;

    return res;
}
