/**
 * FCI Signal Processing - Bioelectric Signal Analysis
 * 
 * Implements signal processing algorithms for mycelium bioelectric analysis:
 * - Digital filtering (bandpass, notch)
 * - FFT spectral analysis
 * - Pattern detection based on GFST
 * - Spike detection (action potential-like events)
 * 
 * Physics basis: Ion channel dynamics, membrane potentials
 * Signal model: Quasi-periodic oscillations with spike events
 * 
 * (c) 2026 Mycosoft Labs
 */

#ifndef FCI_SIGNAL_H
#define FCI_SIGNAL_H

#include <Arduino.h>
#include "fci_config.h"

// ============================================================================
// SIGNAL PROCESSING CLASS
// ============================================================================

class FCISignalProcessor {
public:
    FCISignalProcessor();
    ~FCISignalProcessor();
    
    /**
     * Initialize signal processor with given sample rate
     * @param sample_rate Samples per second (default 128 Hz)
     * @return true if successful
     */
    bool begin(float sample_rate = ADC_SAMPLE_FREQ);
    
    /**
     * Add a new raw sample to the processing buffer
     * @param raw_value Raw ADC value (16-bit signed)
     * @param timestamp_ms Sample timestamp
     * @return true if buffer is ready for processing
     */
    bool addSample(int16_t raw_value, uint32_t timestamp_ms);
    
    /**
     * Process the current buffer and extract features
     * @param features Output structure for extracted features
     * @return true if processing successful
     */
    bool processBuffer(fci_features_t* features);
    
    /**
     * Convert raw ADC value to microvolts
     * @param raw_value Raw ADC value
     * @param gain ADC gain setting
     * @return Value in microvolts
     */
    float rawToMicrovolts(int16_t raw_value, uint8_t gain = ADC_GAIN_BIOELECTRIC);
    
    /**
     * Apply bandpass filter to signal
     * @param input Input signal array
     * @param output Output signal array
     * @param length Signal length
     */
    void applyBandpassFilter(float* input, float* output, size_t length);
    
    /**
     * Apply notch filter for power line interference
     * @param input Input signal array
     * @param output Output signal array
     * @param length Signal length
     * @param notch_freq 50 or 60 Hz
     */
    void applyNotchFilter(float* input, float* output, size_t length, float notch_freq);
    
    /**
     * Compute FFT and extract spectral features
     * @param signal Time-domain signal
     * @param length Signal length (must be power of 2)
     * @param dominant_freq Output dominant frequency
     * @param total_power Output total spectral power
     */
    void computeFFT(float* signal, size_t length, float* dominant_freq, float* total_power);
    
    /**
     * Detect pattern type from signal features
     * @param features Extracted features
     * @return Detected pattern type
     */
    fci_pattern_t detectPattern(const fci_features_t* features);
    
    /**
     * Detect spike events (action potential-like)
     * @param signal Input signal
     * @param length Signal length
     * @param spike_times Output array of spike timestamps
     * @param max_spikes Maximum spikes to detect
     * @return Number of spikes detected
     */
    int detectSpikes(float* signal, size_t length, uint32_t* spike_times, size_t max_spikes);
    
    /**
     * Compute signal quality metric
     * @param signal Input signal
     * @param length Signal length
     * @return Quality score 0.0 - 1.0
     */
    float computeQuality(float* signal, size_t length);
    
    /**
     * Compute impedance from AC measurement
     * @param stimulus_amp Applied stimulus amplitude (µV)
     * @param response_amp Measured response amplitude (µV)
     * @param frequency Measurement frequency (Hz)
     * @return Impedance in ohms
     */
    float computeImpedance(float stimulus_amp, float response_amp, float frequency);
    
    // Buffer access
    float* getRawBuffer() { return _raw_buffer; }
    float* getFilteredBuffer() { return _filtered_buffer; }
    size_t getBufferSize() { return _buffer_size; }
    size_t getSampleCount() { return _sample_count; }
    
private:
    // Configuration
    float _sample_rate;
    size_t _buffer_size;
    
    // Buffers
    float* _raw_buffer;
    float* _filtered_buffer;
    float* _fft_buffer;
    size_t _sample_count;
    size_t _buffer_index;
    
    // Filter state variables (IIR biquad sections)
    float _hp_state[4];  // Highpass filter state
    float _lp_state[4];  // Lowpass filter state
    float _notch_state[4]; // Notch filter state
    
    // Filter coefficients (computed in begin())
    float _hp_b[3], _hp_a[3];  // Highpass coefficients
    float _lp_b[3], _lp_a[3];  // Lowpass coefficients
    float _notch_b[3], _notch_a[3]; // Notch coefficients
    
    // Statistics for adaptive thresholding
    float _running_mean;
    float _running_std;
    uint32_t _total_samples;
    
    // Spike detection state
    uint32_t _last_spike_time;
    
    // Helper methods
    void computeFilterCoefficients();
    float applyBiquadSection(float input, float* b, float* a, float* state);
    void computeWindowFunction(float* window, size_t length);
    void updateRunningStats(float value);
};

// ============================================================================
// STIMULUS GENERATOR CLASS
// ============================================================================

class FCIStimulusGenerator {
public:
    FCIStimulusGenerator(uint8_t dac_pin = STIMULUS_OUT_PIN);
    
    /**
     * Initialize stimulus generator
     * @return true if successful
     */
    bool begin();
    
    /**
     * Generate stimulus waveform
     * @param waveform Waveform type
     * @param amplitude Amplitude in µV (will be scaled to DAC range)
     * @param frequency Frequency in Hz (for AC waveforms)
     * @param duration Duration in ms
     * @return true if stimulus started
     */
    bool startStimulus(stim_waveform_t waveform, float amplitude, float frequency, uint32_t duration);
    
    /**
     * Stop any active stimulus
     */
    void stopStimulus();
    
    /**
     * Update stimulus output (call from timer ISR)
     */
    void update();
    
    /**
     * Check if stimulus is active
     */
    bool isActive() { return _is_active; }
    
    /**
     * Load custom waveform
     * @param samples Waveform samples (0-255)
     * @param length Number of samples
     * @return true if loaded
     */
    bool loadCustomWaveform(uint8_t* samples, size_t length);
    
private:
    uint8_t _dac_pin;
    bool _is_active;
    stim_waveform_t _current_waveform;
    float _amplitude;
    float _frequency;
    uint32_t _start_time;
    uint32_t _duration;
    uint32_t _last_update;
    float _phase;
    
    // Custom waveform buffer
    uint8_t* _custom_buffer;
    size_t _custom_length;
    size_t _custom_index;
    
    // Safety
    uint32_t _last_stimulus_end;
    
    uint8_t amplitudeToDAC(float amplitude_uv);
};

// ============================================================================
// MATHEMATICAL UTILITIES
// ============================================================================

namespace FCIMath {
    /**
     * Compute mean of array
     */
    float mean(float* data, size_t length);
    
    /**
     * Compute standard deviation
     */
    float stddev(float* data, size_t length, float mean_value);
    
    /**
     * Compute RMS value
     */
    float rms(float* data, size_t length);
    
    /**
     * Find peak-to-peak amplitude
     */
    float peakToPeak(float* data, size_t length);
    
    /**
     * Compute cross-correlation between two signals
     */
    float crossCorrelation(float* sig1, float* sig2, size_t length, int lag);
    
    /**
     * Z-score normalization
     */
    void zScore(float* data, size_t length, float mean_value, float std_value);
    
    /**
     * Linear interpolation
     */
    float lerp(float a, float b, float t);
    
    /**
     * Clamp value to range
     */
    float clamp(float value, float min_val, float max_val);
}

#endif // FCI_SIGNAL_H
