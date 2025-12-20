// ============================================================================
// imu_qa_manager.h - COMPLETE REPLACEMENT
// ============================================================================

#pragma once
#include "imu_types.h"
#include "imu_device_session.h"
#include <vector>

#ifdef _WIN32
#include "imu_windows_ble_scanner.h"
#else
#include <simpleble/SimpleBLE.h>
#endif

class ImuQaManager {
public:
    ImuQaManager(const ImuQaConfig& cfg);

    bool discover_and_connect(int max_devices = 10);

    std::vector<ImuQaResult> run_test();

private:
    ImuQaConfig cfg_;
    std::vector<std::unique_ptr<ImuDeviceSession>> sessions_;

    ImuQaResult evaluate_device(const std::string& id,
                                const std::vector<ImuSample>& samples);
    
};