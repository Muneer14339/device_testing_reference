// ============================================================================
// recoil_tracker.cpp - UPDATED
// ============================================================================

#include "imu_qa_manager.h"
#include "imu_types.h"
#include <iostream>
#include <limits>

#ifdef _WIN32
#include <conio.h>
#endif

int main() {
    while (true) {
        int device_count;
        std::cout << "Enter number of GMSync devices to test: ";
        std::cin >> device_count;
        
        if (std::cin.fail() || device_count <= 0) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid input. Please enter a positive number.\n\n";
            continue;
        }
        
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        ImuQaConfig cfg;
        ImuQaManager manager(cfg);

        if (!manager.discover_and_connect(device_count)) {
            std::cout << "\nPress Enter to retry, ESC or Backspace to exit...";
            
#ifdef _WIN32
            int ch = _getch();
            if (ch == 27 || ch == 8) break;
#else
            std::cin.get();
#endif
            std::cout << "\n";
            continue;
        }

        auto results = manager.run_test();

        std::cout << "\n=== QA RESULTS ===\n";
        for (const auto& r : results) {
            std::cout << r.device_id << " -> "
                      << (r.status == QaStatus::PASS ? "PASS" :
                          r.status == QaStatus::WARN ? "WARN" : "FAIL")
                      << "\n";
        }

        std::cout << "\nPress Enter to test again, ESC or Backspace to exit...";
        
#ifdef _WIN32
        int ch = _getch();
        if (ch == 27 || ch == 8) break;
#else
        std::cin.get();
#endif
        std::cout << "\n";
    }
    
    return 0;
}