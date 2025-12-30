/**
 * MycoBrain Science Communications Firmware
 * JSON I/O Helpers Implementation
 */

#include "jsonio.h"
#include <Arduino.h>

// ============================================================================
// BASE64 TABLES
// ============================================================================

static const char base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const int base64DecTable[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

// ============================================================================
// RESPONSE HELPERS
// ============================================================================

void JsonIO::createAck(JsonDoc& doc, const char* cmd, const char* msg) {
    doc.clear();
    doc["type"] = "ack";
    doc["cmd"] = cmd;
    if (msg) {
        doc["message"] = msg;
    }
    addTimestamp(doc);
}

void JsonIO::createError(JsonDoc& doc, const char* cmd, const char* error) {
    doc.clear();
    doc["type"] = "err";
    doc["cmd"] = cmd;
    doc["error"] = error;
    addTimestamp(doc);
}

void JsonIO::createTelemetry(JsonDoc& doc) {
    doc.clear();
    doc["type"] = "telemetry";
    addTimestamp(doc);
    addBoardId(doc);
}

void JsonIO::createPeripheralReport(JsonDoc& doc) {
    doc.clear();
    doc["type"] = "periph_report";
    addTimestamp(doc);
    addBoardId(doc);
}

// ============================================================================
// COMMON FIELDS
// ============================================================================

void JsonIO::addTimestamp(JsonDoc& doc) {
    doc["ts"] = millis();
}

void JsonIO::addBoardId(JsonDoc& doc) {
    char boardId[20];
    getBoardId(boardId, sizeof(boardId));
    doc["board_id"] = boardId;
}

void JsonIO::addFirmwareInfo(JsonDoc& doc) {
    doc["firmware"] = FIRMWARE_NAME;
    doc["version"] = FIRMWARE_VERSION;
    doc["build"] = FIRMWARE_BUILD_DATE;
}

// ============================================================================
// NDJSON EMISSION
// ============================================================================

void JsonIO::emit(const JsonDoc& doc) {
    serializeJson(doc, Serial);
    Serial.println();
}

void JsonIO::emit(const char* jsonString) {
    Serial.println(jsonString);
}

// ============================================================================
// PARSING
// ============================================================================

bool JsonIO::parse(const char* input, JsonDoc& doc) {
    DeserializationError error = deserializeJson(doc, input);
    return error == DeserializationError::Ok;
}

// ============================================================================
// BASE64
// ============================================================================

size_t JsonIO::base64Decode(const char* input, uint8_t* output, size_t maxLen) {
    if (!input || !output) return 0;
    
    size_t inputLen = strlen(input);
    size_t outputLen = 0;
    
    uint32_t buffer = 0;
    int bits = 0;
    
    for (size_t i = 0; i < inputLen && outputLen < maxLen; i++) {
        char c = input[i];
        if (c == '=' || c == '\0') break;
        
        int val = base64DecTable[(unsigned char)c];
        if (val < 0) continue;
        
        buffer = (buffer << 6) | val;
        bits += 6;
        
        if (bits >= 8) {
            bits -= 8;
            output[outputLen++] = (buffer >> bits) & 0xFF;
        }
    }
    
    return outputLen;
}

size_t JsonIO::base64Encode(const uint8_t* input, size_t len, char* output, size_t maxLen) {
    if (!input || !output || maxLen < 4) return 0;
    
    size_t outputLen = 0;
    size_t i = 0;
    
    while (i < len && outputLen + 4 < maxLen) {
        uint32_t octet_a = i < len ? input[i++] : 0;
        uint32_t octet_b = i < len ? input[i++] : 0;
        uint32_t octet_c = i < len ? input[i++] : 0;
        
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        
        output[outputLen++] = base64Chars[(triple >> 18) & 0x3F];
        output[outputLen++] = base64Chars[(triple >> 12) & 0x3F];
        output[outputLen++] = (i > len + 1) ? '=' : base64Chars[(triple >> 6) & 0x3F];
        output[outputLen++] = (i > len) ? '=' : base64Chars[triple & 0x3F];
    }
    
    output[outputLen] = '\0';
    return outputLen;
}

// ============================================================================
// CRC16
// ============================================================================

uint16_t JsonIO::crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// ============================================================================
// BOARD ID
// ============================================================================

void JsonIO::getBoardId(char* buffer, size_t bufSize) {
    uint64_t mac = ESP.getEfuseMac();
    snprintf(buffer, bufSize, "%012llX", mac);
}

