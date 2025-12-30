/**
 * MycoBrain Science Communications Firmware
 * JSON I/O Helpers
 * 
 * NDJSON emission and JSON parsing utilities.
 * Ensures consistent JSON output format.
 */

#ifndef JSONIO_H
#define JSONIO_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================================================
// JSON DOCUMENT TYPES
// ============================================================================

// ArduinoJson v7 uses JsonDocument directly
using JsonDoc = JsonDocument;

// ============================================================================
// JSON HELPERS
// ============================================================================

namespace JsonIO {
    // Create standard response objects
    void createAck(JsonDoc& doc, const char* cmd, const char* msg = nullptr);
    void createError(JsonDoc& doc, const char* cmd, const char* error);
    void createTelemetry(JsonDoc& doc);
    void createPeripheralReport(JsonDoc& doc);
    
    // Add common fields
    void addTimestamp(JsonDoc& doc);
    void addBoardId(JsonDoc& doc);
    void addFirmwareInfo(JsonDoc& doc);
    
    // Emit NDJSON line
    void emit(const JsonDoc& doc);
    void emit(const char* jsonString);
    
    // Parse incoming JSON
    bool parse(const char* input, JsonDoc& doc);
    
    // Base64 helpers
    size_t base64Decode(const char* input, uint8_t* output, size_t maxLen);
    size_t base64Encode(const uint8_t* input, size_t len, char* output, size_t maxLen);
    
    // CRC16 helper
    uint16_t crc16(const uint8_t* data, size_t len);
    
    // Board ID (MAC address based)
    void getBoardId(char* buffer, size_t bufSize);
}

#endif // JSONIO_H

