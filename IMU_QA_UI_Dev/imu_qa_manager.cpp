#include "imu_qa_manager.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <cmath>
#include <algorithm>

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

    // List of your device addresses (can stay uppercase, we normalize below)
    std::vector<std::string> target_addresses = {
        "C6:22:D5:9E:0C:53",
        "D8:6C:8A:A8:38:DE",
        "F1:F2:2C:21:47:89"
    };

    // Convert target addresses to lowercase for comparison
    for (auto& addr : target_addresses) {
        std::transform(addr.begin(), addr.end(), addr.begin(), ::tolower);
    }

    std::vector<SimpleBLE::Peripheral> found;
    std::mutex found_mutex;  // 🔥 Thread-safe access

    adapter.set_callback_on_scan_found([&](SimpleBLE::Peripheral p) {
        std::string addr = p.address();
        std::transform(addr.begin(), addr.end(), addr.begin(), ::tolower);

        std::cout << "Scan found: " << p.identifier() << " [" << addr << "]\n";

        // Check if this address is in our target list
        if (std::find(target_addresses.begin(), target_addresses.end(), addr) == target_addresses.end()) {
            return; // Not a device we care about
        }

        std::lock_guard<std::mutex> lock(found_mutex);
        
        // Check if already added (avoid duplicates)
        for (auto& existing : found) {  // 🔥 Remove 'const' to call non-const address()
            std::string existing_addr = existing.address();
            std::transform(existing_addr.begin(), existing_addr.end(), existing_addr.begin(), ::tolower);
            if (existing_addr == addr) {
                std::cout << "  [SKIP] Device already found: " << addr << "\n";
                return;
            }
        }

        if ((int)found.size() >= max_devices) return;
        
        found.push_back(p);
        std::cout << "✅ Found target device[" << found.size()
                  << "]: " << p.identifier() << " [" << addr << "]\n";
    });

    // 🔥 FIX: Scan multiple times until we find minimum 2 devices
    const int MIN_DEVICES = 2;
    const int MAX_SCAN_ATTEMPTS = 5;
    const int SCAN_DURATION_MS = 10000;  // 10 seconds per scan
    
    int scan_attempt = 0;
    
    while (scan_attempt < MAX_SCAN_ATTEMPTS) {
        std::cout << "\n🔍 Scan attempt " << (scan_attempt + 1) 
                  << " (need " << MIN_DEVICES << " devices)...\n";
        
        adapter.scan_for(SCAN_DURATION_MS);
        
        std::lock_guard<std::mutex> lock(found_mutex);
        std::cout << "Found " << found.size() << " device(s) so far.\n";
        
        if ((int)found.size() >= MIN_DEVICES) {
            std::cout << "✅ Minimum " << MIN_DEVICES << " devices found!\n";
            break;
        }
        
        scan_attempt++;
        
        if (scan_attempt < MAX_SCAN_ATTEMPTS) {
            std::cout << "⚠️  Only " << found.size() << " device(s) found. Scanning again...\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    if ((int)found.size() < MIN_DEVICES) {
        std::cerr << "❌ ERROR: Could not find minimum " << MIN_DEVICES 
                  << " devices after " << MAX_SCAN_ATTEMPTS << " scan attempts.\n";
        std::cerr << "Only found: " << found.size() << " device(s).\n";
        return false;
    }

    std::cout << "\n📡 Connecting to " << found.size() << " devices...\n";

    // 🔥 FIX: Connect to ALL devices with delay between connections
    for (size_t i = 0; i < found.size(); i++) {
        auto& p = found[i];
        auto id = p.address();
        
        std::cout << "\nConnecting to device " << (i + 1) << "/" << found.size() 
                  << ": " << id << "...\n";
        
        auto session = std::make_unique<ImuDeviceSession>(p, id);
        if (!session->start()) {
            std::cerr << "[" << id << "] ❌ start() failed, skipping.\n";
            continue;
        }
        
        sessions_.push_back(std::move(session));
        std::cout << "[" << id << "] ✅ Connected and started\n";
        
        // Small delay between device connections
        if (i < found.size() - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    if (sessions_.empty()) {
        std::cerr << "❌ No sessions started.\n";
        return false;
    }

    std::cout << "\n✅ Successfully started " << sessions_.size()
              << " device sessions.\n";
    return true;
}


std::vector<ImuQaResult> ImuQaManager::run_test() {
    using clock = std::chrono::steady_clock;

    auto t0 = clock::now();
    auto settle_end = t0 + std::chrono::duration<double>(cfg_.settle_seconds);
    auto test_end   = settle_end + std::chrono::duration<double>(cfg_.test_seconds);

    std::cout << "\n⏱️  Settling for " << cfg_.settle_seconds << "s...\n";
    while (clock::now() < settle_end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "📊 Collecting samples for " << cfg_.test_seconds << "s...\n\n";

    // Per-device sample accumulation
    std::vector<std::vector<ImuSample>> all_samples(sessions_.size());

    auto last_print = clock::now();
    
    while (clock::now() < test_end) {
        // 🔥 FIX: Poll ALL devices in parallel
        for (size_t i = 0; i < sessions_.size(); ++i) {
            auto chunk = sessions_[i]->drain_samples();
            
            if (!chunk.empty()) {
                all_samples[i].insert(all_samples[i].end(), chunk.begin(), chunk.end());
            }
        }

        // Print progress every 2 seconds
        auto now = clock::now();
        if (std::chrono::duration<double>(now - last_print).count() >= 2.0) {
            std::cout << "\n--- Progress Update ---\n";
            for (size_t i = 0; i < sessions_.size(); ++i) {
                std::cout << "Device " << i << " [" << sessions_[i]->id() << "]: "
                          << all_samples[i].size() << " samples\n";
            }
            last_print = now;
        }

        // Poll frequently to avoid losing packets (10ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n✅ Test window ended. Evaluating...\n";

    // Evaluate results for all devices
    std::vector<ImuQaResult> results;
    for (size_t i = 0; i < sessions_.size(); ++i) {
        auto id = sessions_[i]->id();
        auto res = evaluate_device(id, all_samples[i]);
        results.push_back(res);

        // Print session summary
        std::cout << "\n=== Device [" << id << "] Summary ===\n";
        std::cout << "Total samples: " << all_samples[i].size() << "\n";

        if (!all_samples[i].empty()) {
            std::cout << "First 5 samples:\n";
            for (size_t j = 0; j < std::min(size_t(5), all_samples[i].size()); j++) {
                const auto& s = all_samples[i][j];
                std::cout << "  [" << j << "] "
                          << "ax=" << s.ax << ", ay=" << s.ay << ", az=" << s.az << ", "
                          << "gx=" << s.gx << ", gy=" << s.gy << ", gz=" << s.gz << "\n";
            }
        }

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

    // Calculate average gravity magnitude
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

    // Placeholder values
    res.mac_deg           = 0.0;
    res.noise_sigma       = 0.0;
    res.drift_deg_per_min = 0.0;
    res.abnormal_count    = 0;
    res.status            = QaStatus::PASS;

    return res;
}