#pragma once
#include <string>
#include <vector>

struct ImuSample {
    double timestamp_s;  // host time in seconds
    float ax, ay, az;
    float gx, gy, gz;
    float temp;
};

struct ImuQaConfig {
    double settle_seconds = 5.0;
    double test_seconds   = 60.0;

    double abnormal_threshold_deg   = 0.30;
    double gravity_deviation_g      = 0.05;
    double gyro_stillness_deg_per_s = 0.5;
    int    max_abnormal_per_window  = 100;
    double max_mac_deg              = 0.20;
    double max_noise_sigma_deg      = 0.05;
    double max_drift_deg_per_min    = 0.10;
};

enum class QaStatus {
    PASS,
    WARN,
    FAIL
};

struct ImuQaResult {
    std::string device_id;  // MAC or serial
    QaStatus    status;
    double      mac_deg;      // angle stability
    double      noise_sigma;  // σ
    double      drift_deg_per_min;
    double      gravity_mean_g;
    int         abnormal_count;
    // add fields as needed
};
