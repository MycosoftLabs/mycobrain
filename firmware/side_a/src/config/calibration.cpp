#include "calibration.h"

void Calibration::applyCalibration(const CalibrationConfig& config, uint16_t raw_counts, int channel, float& volts_out) {
  if (channel < 0 || channel >= 4) return;
  
  // Convert raw counts to volts using calibrated reference
  float base_volts = (float)raw_counts * (config.adc_reference / 4095.0f);
  
  // Apply offset and gain
  volts_out = (base_volts + config.analog_offset[channel]) * config.analog_gain[channel];
}

float Calibration::applyBmeTempOffset(const CalibrationConfig& config, float temp) {
  return temp + config.bme_temp_offset;
}

float Calibration::applyBmeHumidityOffset(const CalibrationConfig& config, float humidity) {
  return humidity + config.bme_humidity_offset;
}

