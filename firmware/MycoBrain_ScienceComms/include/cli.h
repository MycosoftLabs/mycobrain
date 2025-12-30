/**
 * MycoBrain Science Communications Firmware
 * CLI Module (Command Line Interface + JSON-CLI)
 * 
 * Handles serial input parsing and command dispatch.
 * Supports both plaintext commands and JSON commands.
 */

#ifndef CLI_H
#define CLI_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// OPERATING MODES
// ============================================================================

enum OperatingMode {
    MODE_HUMAN,     // Banners, help text allowed
    MODE_MACHINE    // NDJSON only, no banners
};

// ============================================================================
// CLI MODULE INTERFACE
// ============================================================================

namespace CLI {
    // Initialization
    void init();
    
    // Processing (call in loop)
    void update();
    
    // Mode control
    void setMode(OperatingMode mode);
    OperatingMode getMode();
    bool isMachineMode();
    
    // Debug control
    void setDebug(bool enable);
    bool isDebugEnabled();
    
    // Response helpers
    void sendAck(const char* cmd, const char* message = nullptr);
    void sendError(const char* cmd, const char* error);
    void sendJson(const char* json);
    void sendTelemetry(const char* json);
    
    // Human mode helpers
    void printBanner();
    void printHelp();
    void printLine(const char* text);
}

// ============================================================================
// COMMAND HANDLER TYPE
// ============================================================================

typedef void (*CommandHandler)(int argc, char* argv[]);

// ============================================================================
// COMMAND REGISTRATION
// ============================================================================

void registerCommand(const char* name, CommandHandler handler, const char* help);

#endif // CLI_H

