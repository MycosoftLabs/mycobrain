#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "config_schema.h"

class Calibration {
public:
  static void applyCalibration(const CalibrationConfig& config, uint16_t raw_counts, int channel, float& volts_out);
  static float applyBmeTempOffset(const CalibrationConfig& config, float temp);
  static float applyBmeHumidityOffset(const CalibrationConfig& config, float humidity);
};

#endif // CALIBRATION_H

