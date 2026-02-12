/**
 * FCI Configuration - Fungal Computer Interface
 * MycoBrain Bioelectric Signal Acquisition System
 * 
 * Based on Global Fungi Symbiosis Theory (GFST)
 * Physics: Ion channel dynamics (K+, Ca2+, Na+), membrane potentials
 * Chemistry: Glutamate, GABA neurotransmitter-like signaling
 * Biology: Mycorrhizal network electrical propagation
 * 
 * (c) 2026 Mycosoft Labs
 */

#ifndef FCI_CONFIG_H
#define FCI_CONFIG_H

// ============================================================================
// DEVICE IDENTIFICATION
// ============================================================================

#define FCI_DEVICE_PREFIX      "FCI"
#define FCI_PROTOCOL_VERSION   0x01
#define FCI_FIRMWARE_VERSION   "1.0.0"

// ============================================================================
// PIN CONFIGURATION (ESP32-S3)
// ============================================================================

// I2C Pins (shared bus for ADC and environmental sensors)
#define I2C_SDA_PIN            8
#define I2C_SCL_PIN            9

// SPI Pins (for high-speed ADC option)
#define SPI_MOSI_PIN           11
#define SPI_MISO_PIN           13
#define SPI_SCK_PIN            12
#define SPI_CS_ADC_PIN         10

// Analog input pins (ESP32 internal ADC for reference)
#define ANALOG_BIOELECTRIC_1   1   // GPIO1 - Primary bioelectric channel
#define ANALOG_BIOELECTRIC_2   2   // GPIO2 - Secondary/reference channel
#define ANALOG_GROUND_REF      3   // GPIO3 - Ground reference electrode

// Digital I/O
#define NEOPIXEL_PIN           48  // Status LED
#define BUZZER_PIN             47  // Audio feedback
#define STIMULUS_OUT_PIN       4   // DAC output for mycelium stimulation
#define BUTTON_PIN             0   // Boot button for calibration

// ============================================================================
// ADC CONFIGURATION (ADS1115 - 16-bit differential ADC)
// ============================================================================

// ADS1115 I2C address (default 0x48, ADDR to GND)
#define ADS1115_I2C_ADDR       0x48

// Channel assignments
#define ADC_CHANNEL_BIO_DIFF   0   // A0-A1: Differential bioelectric (electrodes 1-2)
#define ADC_CHANNEL_BIO_REF    1   // A2-A3: Reference electrode pair
#define ADC_CHANNEL_IMPEDANCE  2   // For impedance measurement

// Gain settings for bioelectric signals
// PGA Gain | Full Scale Range | Resolution
// 2/3      | ±6.144V          | 187.5 µV
// 1        | ±4.096V          | 125 µV
// 2        | ±2.048V          | 62.5 µV
// 4        | ±1.024V          | 31.25 µV
// 8        | ±0.512V          | 15.625 µV
// 16       | ±0.256V          | 7.8125 µV <-- Best for bioelectric
#define ADC_GAIN_BIOELECTRIC   16  // ±256mV range, 7.8µV resolution

// Sample rates
#define ADC_SAMPLE_RATE        128  // Samples per second (SPS)
#define ADC_BUFFER_SIZE        256  // Samples per buffer (2 seconds @ 128 SPS)

// ============================================================================
// BIOELECTRIC SIGNAL PARAMETERS (Based on Scientific Literature)
// ============================================================================

// Expected mycelium signal characteristics
// Scientific References:
//   - Adamatzky (2022): Most fungi 0.007-0.3 mV (7-300 µV)
//   - Buffi et al. (2025): F. oxysporum ~0.1 mV (100 µV)
//   - Olsson & Hansson (1995): Cords 5-50 mV (specialized structures only)
//   - This config optimized for vegetative mycelium (µV range)
//   - ADS1115 @ gain 16: 7.8 µV resolution - IDEAL for fungal signals
#define BIO_SIGNAL_MIN_UV      -100.0f   // Minimum expected signal (µV)
#define BIO_SIGNAL_MAX_UV      100.0f    // Maximum expected signal (µV)
#define BIO_SIGNAL_BASELINE_UV 0.0f      // Expected baseline (µV)

// Frequency bands of interest (from peer-reviewed literature)
// Scientific basis:
//   - Adamatzky (2022): 0.5-5 Hz typical spike activity
//   - Buffi et al. (2025): 1.5-8 Hz biological signature (STFT method)
//   - Fukasawa et al. (2024): 0.0001 Hz (7-day oscillation - record!)
#define FREQ_BAND_ULTRA_LOW    0.0001f // Hz - Week-long oscillations (P. brunnescens)
#define FREQ_BAND_LOW          0.1f    // Hz - Baseline activity
#define FREQ_BAND_MID          1.5f    // Hz - Biological signature threshold
#define FREQ_BAND_HIGH         8.0f    // Hz - Burst activity upper limit
#define FREQ_BAND_MAX          10.0f   // Hz - Above this likely artifacts

// Spike detection parameters
#define SPIKE_THRESHOLD_SIGMA  3.0f    // Standard deviations for spike detection
#define SPIKE_MIN_DURATION_MS  5       // Minimum spike duration
#define SPIKE_REFRACTORY_MS    50      // Refractory period

// ============================================================================
// DIGITAL FILTER PARAMETERS
// ============================================================================

// Bandpass filter for bioelectric signals (0.1 - 50 Hz)
#define FILTER_HIGHPASS_FREQ   0.1f    // High-pass cutoff (Hz)
#define FILTER_LOWPASS_FREQ    50.0f   // Low-pass cutoff (Hz)
#define FILTER_ORDER           4       // Butterworth filter order

// Notch filter for power line interference
#define NOTCH_FREQ_50HZ        50.0f   // 50 Hz (Europe/Asia)
#define NOTCH_FREQ_60HZ        60.0f   // 60 Hz (Americas)
#define NOTCH_Q_FACTOR         30.0f   // Quality factor

// ============================================================================
// FFT CONFIGURATION
// ============================================================================

#define FFT_SAMPLES            256     // Must be power of 2
#define FFT_SAMPLE_FREQ        128.0f  // Hz
#define FFT_WINDOW_HAMMING     1       // Apply Hamming window

// ============================================================================
// ENVIRONMENTAL SENSOR CONFIGURATION (BME688)
// ============================================================================

#define BME688_I2C_ADDR        0x76    // or 0x77
#define ENV_SAMPLE_INTERVAL_MS 1000    // Sample every 1 second

// ============================================================================
// PATTERN DETECTION THRESHOLDS (Based on GFST)
// ============================================================================

// Signal pattern types (electrical signatures)
typedef enum {
    PATTERN_BASELINE        = 0x00,   // Normal resting state
    PATTERN_GROWTH          = 0x01,   // Active hyphal extension
    PATTERN_STRESS          = 0x02,   // Environmental stress response
    PATTERN_NUTRIENT_SEEK   = 0x03,   // Chemotropic behavior
    PATTERN_COMMUNICATION   = 0x04,   // Inter-network signaling
    PATTERN_SEISMIC         = 0x05,   // Earthquake precursor
    PATTERN_SPIKE           = 0x06,   // Action potential-like
    PATTERN_UNKNOWN         = 0xFF    // Unclassified
} fci_pattern_t;

// Growth pattern characteristics
#define GROWTH_FREQ_MIN        0.1f   // Hz
#define GROWTH_FREQ_MAX        5.0f   // Hz
#define GROWTH_AMP_MIN_UV      0.5f   // µV
#define GROWTH_AMP_MAX_UV      1.0f   // µV

// Stress pattern characteristics  
#define STRESS_FREQ_MIN        5.0f   // Hz
#define STRESS_FREQ_MAX        20.0f  // Hz
#define STRESS_AMP_MIN_UV      1.0f   // µV

// Seismic precursor characteristics (M-Wave)
#define SEISMIC_FREQ_MIN       0.01f  // Hz
#define SEISMIC_FREQ_MAX       0.1f   // Hz
#define SEISMIC_DURATION_MIN_S 3600   // 1 hour minimum

// ============================================================================
// COMMUNICATION CONFIGURATION
// ============================================================================

// WiFi
#define WIFI_CONNECT_TIMEOUT_MS  10000
#define WIFI_RECONNECT_DELAY_MS  5000

// Mycorrhizae Protocol endpoints
#define MYCORRHIZAE_DEFAULT_URL  "http://192.168.0.188:8002"
#define MYCORRHIZAE_CHANNEL_FMT  "device.%s.%s"  // device.FCI-001.bioelectric

// Telemetry upload
#define TELEMETRY_INTERVAL_MS    100    // 10 Hz telemetry
#define TELEMETRY_BATCH_SIZE     10     // Batch 10 readings

// WebSocket for real-time streaming
#define WS_RECONNECT_DELAY_MS    3000
#define WS_HEARTBEAT_INTERVAL_MS 30000

// ============================================================================
// STIMULATION PARAMETERS (Write-back to mycelium)
// ============================================================================

// DAC configuration (ESP32 internal 8-bit DAC)
#define STIM_DAC_RESOLUTION      8      // bits
#define STIM_MAX_VOLTAGE_MV      3300   // 3.3V max

// Stimulus waveforms
typedef enum {
    STIM_WAVEFORM_NONE     = 0,
    STIM_WAVEFORM_DC       = 1,    // Constant DC
    STIM_WAVEFORM_PULSE    = 2,    // Square pulse
    STIM_WAVEFORM_SINE     = 3,    // Sinusoidal
    STIM_WAVEFORM_RAMP     = 4,    // Linear ramp
    STIM_WAVEFORM_CUSTOM   = 5     // Custom waveform buffer
} stim_waveform_t;

// Safety limits
#define STIM_MAX_AMPLITUDE_UV    100.0f   // Maximum stimulation amplitude
#define STIM_MAX_DURATION_MS     10000    // Maximum stimulation duration
#define STIM_COOLDOWN_MS         5000     // Cooldown between stimulations

// ============================================================================
// DATA STRUCTURES
// ============================================================================

// Bioelectric sample (packed for efficiency)
typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;    // Milliseconds since boot
    int16_t  bio_channel_1;   // Raw ADC value channel 1
    int16_t  bio_channel_2;   // Raw ADC value channel 2
    uint8_t  quality;         // Signal quality 0-255
    uint8_t  flags;           // Status flags
} fci_sample_t;

// Processed signal features
typedef struct {
    float amplitude_uv;       // Peak-to-peak amplitude
    float rms_uv;            // RMS value
    float mean_uv;           // DC offset
    float std_uv;            // Standard deviation
    float dominant_freq_hz;  // Dominant frequency from FFT
    float total_power;       // Total spectral power
    float snr_db;            // Signal-to-noise ratio
    fci_pattern_t pattern;   // Detected pattern type
    float pattern_confidence; // 0.0 - 1.0
} fci_features_t;

// Full telemetry packet
typedef struct {
    char device_id[16];       // e.g., "FCI-001"
    uint64_t timestamp_unix;  // Unix timestamp (ms)
    uint32_t uptime_ms;       // Device uptime
    
    // Bioelectric data
    fci_features_t bio_features;
    uint16_t sample_count;
    
    // Environmental data
    float temperature_c;
    float humidity_pct;
    float pressure_hpa;
    float voc_index;
    float co2_ppm;
    
    // Impedance (electrode health)
    float impedance_ohms;
    
    // Device status
    float battery_pct;
    int8_t wifi_rssi;
    uint8_t error_flags;
} fci_telemetry_t;

#endif // FCI_CONFIG_H
