#include "imu_qa_manager.h"
#include "imu_types.h"
#include <iostream>

int main() {
    ImuQaConfig cfg;
    // TODO: load from JSON instead of hardcoding

    ImuQaManager manager(cfg);

    if (!manager.discover_and_connect(10)) {
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

    // TODO: write CSV
    return 0;
}

