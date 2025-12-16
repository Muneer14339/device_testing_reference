// ============================================================================
// recoil_tracker.cpp - COMPLETE FILE REPLACEMENT
// ============================================================================

#include "imu_qa_manager.h"
#include "imu_types.h"
#include <iostream>

int main() {
    ImuQaConfig cfg;

    ImuQaManager manager(cfg);

    if (!manager.discover_and_connect(10)) {
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    auto results = manager.run_test();

    std::cout << "\n=== QA RESULTS ===\n";
    for (const auto& r : results) {
        std::cout << r.device_id << " -> "
                  << (r.status == QaStatus::PASS ? "PASS" :
                      r.status == QaStatus::WARN ? "WARN" : "FAIL")
                  << "\n";
    }

    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    return 0;
}