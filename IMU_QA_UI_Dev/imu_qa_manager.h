#pragma once
#include "imu_types.h"
#include "imu_device_session.h"
#include <simpleble/SimpleBLE.h>
#include <vector>

class ImuQaManager {
public:
    ImuQaManager(const ImuQaConfig& cfg);

    // Scan and connect up to max_devices GMSync units
    bool discover_and_connect(int max_devices = 10);

    // Run full QA test (settle + window) and return results
    std::vector<ImuQaResult> run_test();

private:
    ImuQaConfig cfg_;
    std::vector<std::unique_ptr<ImuDeviceSession>> sessions_;

    ImuQaResult evaluate_device(const std::string& id,
                                const std::vector<ImuSample>& samples);
    
};
