/**
 * MycoBrain Science Communications Firmware
 * CLI Module Implementation
 * 
 * Handles serial input parsing and command dispatch.
 */

#include "cli.h"
#include "jsonio.h"
#include "pixel.h"
#include "buzzer.h"
#include "modem_optical.h"
#include "modem_audio.h"
#include "peripherals.h"
#include "outputs.h"
#include "stimulus.h"

// ============================================================================
// STATE
// ============================================================================

static OperatingMode currentMode = MODE_HUMAN;
static bool debugEnabled = false;
static char inputBuffer[256];
static int inputPos = 0;

// ============================================================================
// COMMAND TABLE
// ============================================================================

struct Command {
    const char* name;
    CommandHandler handler;
    const char* help;
};

#define MAX_COMMANDS 32
static Command commands[MAX_COMMANDS];
static int commandCount = 0;

// ============================================================================
// INITIALIZATION
// ============================================================================

void CLI::init() {
    inputPos = 0;
    memset(inputBuffer, 0, sizeof(inputBuffer));
}

// ============================================================================
// MODE CONTROL
// ============================================================================

void CLI::setMode(OperatingMode mode) {
    currentMode = mode;
}

OperatingMode CLI::getMode() {
    return currentMode;
}

bool CLI::isMachineMode() {
    return currentMode == MODE_MACHINE;
}

// ============================================================================
// DEBUG CONTROL
// ============================================================================

void CLI::setDebug(bool enable) {
    debugEnabled = enable;
}

bool CLI::isDebugEnabled() {
    return debugEnabled;
}

// ============================================================================
// RESPONSE HELPERS
// ============================================================================

void CLI::sendAck(const char* cmd, const char* message) {
    JsonDoc doc;
    JsonIO::createAck(doc, cmd, message);
    JsonIO::emit(doc);
}

void CLI::sendError(const char* cmd, const char* error) {
    JsonDoc doc;
    JsonIO::createError(doc, cmd, error);
    JsonIO::emit(doc);
}

void CLI::sendJson(const char* json) {
    Serial.println(json);
}

void CLI::sendTelemetry(const char* json) {
    Serial.println(json);
}

void CLI::printLine(const char* text) {
    if (currentMode == MODE_HUMAN) {
        Serial.println(text);
    }
}

// ============================================================================
// BANNERS
// ============================================================================

void CLI::printBanner() {
    if (currentMode == MODE_MACHINE) return;
    
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════════════╗");
    Serial.println("║     MycoBrain Science Communications Firmware        ║");
    Serial.println("║     " FIRMWARE_VERSION " - " FIRMWARE_BUILD_DATE "                          ║");
    Serial.println("╚══════════════════════════════════════════════════════╝");
    Serial.println();
    Serial.println("Type 'help' for commands, 'mode machine' for NDJSON mode");
    Serial.println();
}

void CLI::printHelp() {
    if (currentMode == MODE_MACHINE) {
        sendAck("help", "Use 'mode human' for readable help");
        return;
    }
    
    Serial.println("\n=== MycoBrain Commands ===\n");
    
    Serial.println("[System]");
    Serial.println("  help              Show this help");
    Serial.println("  status            Show system status");
    Serial.println("  mode <human|machine>  Set output mode");
    Serial.println("  dbg <on|off>      Enable/disable debug");
    
    Serial.println("\n[NeoPixel LED - GPIO15]");
    Serial.println("  led rgb <r> <g> <b>   Set LED color (0-255)");
    Serial.println("  led off               Turn LED off");
    Serial.println("  led status            Show LED status");
    Serial.println("  led pattern <name>    Start pattern (rainbow/pulse/sweep/beacon)");
    
    Serial.println("\n[Buzzer - GPIO16]");
    Serial.println("  buzz tone <hz> <ms>   Play tone");
    Serial.println("  buzz pattern <name>   Play pattern (coin/bump/power/1up/morgio/alert/warning/success/error)");
    Serial.println("  buzz stop             Stop buzzer");
    
    Serial.println("\n[Optical Modem TX - LiFi]");
    Serial.println("  optx start <profile> payload_b64=<base64> rate_hz=<rate>");
    Serial.println("       Profiles: camera_ook, camera_manchester, beacon, morse");
    Serial.println("  optx pattern <name>   Run pattern (pulse/sweep/beacon)");
    Serial.println("  optx stop             Stop transmission");
    Serial.println("  optx status           Show modem status");
    
    Serial.println("\n[Acoustic Modem TX - FSK]");
    Serial.println("  aotx start <profile> payload_b64=<base64>");
    Serial.println("       Profiles: simple_fsk, morse");
    Serial.println("  aotx pattern <name>   Run pattern (sweep/chirp/pulse_train)");
    Serial.println("  aotx stop             Stop transmission");
    Serial.println("  aotx status           Show modem status");
    
    Serial.println("\n[Stimulus Engine]");
    Serial.println("  stim light <pattern> r=<r> g=<g> b=<b> on=<ms> off=<ms> cycles=<n>");
    Serial.println("       Patterns: pulse, flash, ramp, strobe");
    Serial.println("  stim sound <pattern> freq=<hz> on=<ms> off=<ms> cycles=<n>");
    Serial.println("       Patterns: tone, pulse, sweep, chirp");
    Serial.println("  stim stop             Stop all stimuli");
    
    Serial.println("\n[Peripherals]");
    Serial.println("  periph scan           Scan I2C bus");
    Serial.println("  periph list           List known peripherals");
    Serial.println("  periph describe <uid> Show peripheral details");
    Serial.println("  periph hotplug <on|off>  Enable/disable hotplug detection");
    
    Serial.println("\n[Outputs - GPIO12/13/14]");
    Serial.println("  out set <1|2|3> <0|1>       Set digital output");
    Serial.println("  out pwm <1|2|3> <0-255> [freq]  Set PWM output");
    Serial.println("  out status                  Show output status");
    
    Serial.println();
}

// ============================================================================
// COMMAND PARSING
// ============================================================================

static void parseArgs(char* line, int& argc, char* argv[], int maxArgs) {
    argc = 0;
    char* token = strtok(line, " \t");
    while (token && argc < maxArgs) {
        argv[argc++] = token;
        token = strtok(nullptr, " \t");
    }
}

static void processCommand(char* line) {
    // Skip empty lines
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '\n' || *line == '\r') return;
    
    // Check if JSON command
    if (*line == '{') {
        JsonDoc doc;
        if (JsonIO::parse(line, doc)) {
            // Handle JSON command
            const char* cmd = doc["cmd"];
            if (cmd) {
                // Convert JSON to arg array and process
                char cmdCopy[64];
                strncpy(cmdCopy, cmd, sizeof(cmdCopy) - 1);
                
                int argc = 0;
                char* argv[16];
                parseArgs(cmdCopy, argc, argv, 16);
                
                // TODO: Add more JSON command handlers
                if (strcmp(argv[0], "led.rgb") == 0) {
                    uint8_t r = doc["r"] | 0;
                    uint8_t g = doc["g"] | 0;
                    uint8_t b = doc["b"] | 0;
                    Pixel::setColor(r, g, b);
                    CLI::sendAck("led.rgb", "Color set");
                    return;
                }
            }
            CLI::sendError("json", "Unknown JSON command");
            return;
        } else {
            CLI::sendError("json", "Parse error");
            return;
        }
    }
    
    // Parse as text command
    int argc = 0;
    char* argv[16];
    parseArgs(line, argc, argv, 16);
    
    if (argc == 0) return;
    
    // Find and execute command
    const char* cmdName = argv[0];
    
    // Built-in commands
    if (strcmp(cmdName, "help") == 0) {
        CLI::printHelp();
        return;
    }
    
    if (strcmp(cmdName, "mode") == 0) {
        if (argc < 2) {
            CLI::sendError("mode", "Usage: mode <human|machine>");
            return;
        }
        if (strcmp(argv[1], "machine") == 0) {
            CLI::setMode(MODE_MACHINE);
            CLI::sendAck("mode", "machine");
        } else if (strcmp(argv[1], "human") == 0) {
            CLI::setMode(MODE_HUMAN);
            CLI::printLine("Mode set to human");
        } else {
            CLI::sendError("mode", "Unknown mode");
        }
        return;
    }
    
    if (strcmp(cmdName, "dbg") == 0) {
        if (argc < 2) {
            CLI::sendError("dbg", "Usage: dbg <on|off>");
            return;
        }
        CLI::setDebug(strcmp(argv[1], "on") == 0);
        CLI::sendAck("dbg", debugEnabled ? "on" : "off");
        return;
    }
    
    if (strcmp(cmdName, "fmt") == 0) {
        if (argc < 2) {
            CLI::sendError("fmt", "Usage: fmt <json>");
            return;
        }
        if (strcmp(argv[1], "json") == 0) {
            // Machine mode already outputs NDJSON, but acknowledge for compatibility
            CLI::sendAck("fmt", "json");
        } else {
            CLI::sendError("fmt", "Unknown format (use 'json')");
        }
        return;
    }
    
    if (strcmp(cmdName, "status") == 0) {
        JsonDoc doc;
        JsonIO::createTelemetry(doc);
        JsonIO::addFirmwareInfo(doc);
        
        char pixelStatus[128], buzzerStatus[128], outputStatus[256];
        Pixel::getStatus(pixelStatus, sizeof(pixelStatus));
        Buzzer::getStatus(buzzerStatus, sizeof(buzzerStatus));
        Outputs::getStatus(outputStatus, sizeof(outputStatus));
        
        doc["uptime_ms"] = millis();
        doc["mode"] = (currentMode == MODE_MACHINE) ? "machine" : "human";
        doc["debug"] = debugEnabled;
        
        // Parse and embed sub-status
        JsonDoc pixelDoc, buzzerDoc, outputDoc;
        deserializeJson(pixelDoc, pixelStatus);
        deserializeJson(buzzerDoc, buzzerStatus);
        deserializeJson(outputDoc, outputStatus);
        
        doc["led"] = pixelDoc;
        doc["buzzer"] = buzzerDoc;
        doc["outputs"] = outputDoc;
        
        JsonIO::emit(doc);
        return;
    }
    
    // LED commands
    if (strcmp(cmdName, "led") == 0) {
        if (argc < 2) {
            CLI::sendError("led", "Usage: led <rgb|off|status|pattern>");
            return;
        }
        
        if (strcmp(argv[1], "rgb") == 0) {
            if (argc < 5) {
                CLI::sendError("led", "Usage: led rgb <r> <g> <b>");
                return;
            }
            uint8_t r = atoi(argv[2]);
            uint8_t g = atoi(argv[3]);
            uint8_t b = atoi(argv[4]);
            Pixel::setColor(r, g, b);
            CLI::sendAck("led", "Color set");
        } else if (strcmp(argv[1], "off") == 0) {
            Pixel::off();
            CLI::sendAck("led", "LED off");
        } else if (strcmp(argv[1], "status") == 0) {
            char status[128];
            Pixel::getStatus(status, sizeof(status));
            Serial.println(status);
        } else if (strcmp(argv[1], "pattern") == 0) {
            if (argc < 3) {
                CLI::sendError("led", "Usage: led pattern <name>");
                return;
            }
            Pixel::startPattern(argv[2], 200);
            CLI::sendAck("led", "Pattern started");
        } else {
            CLI::sendError("led", "Unknown subcommand");
        }
        return;
    }
    
    // Buzzer commands
    if (strcmp(cmdName, "buzz") == 0) {
        if (argc < 2) {
            CLI::sendError("buzz", "Usage: buzz <tone|pattern|stop>");
            return;
        }
        
        if (strcmp(argv[1], "tone") == 0) {
            if (argc < 4) {
                CLI::sendError("buzz", "Usage: buzz tone <hz> <ms>");
                return;
            }
            uint16_t freq = atoi(argv[2]);
            uint16_t dur = atoi(argv[3]);
            Buzzer::tone(freq, dur);
            CLI::sendAck("buzz", "Tone playing");
        } else if (strcmp(argv[1], "pattern") == 0) {
            if (argc < 3) {
                CLI::sendError("buzz", "Usage: buzz pattern <name>");
                return;
            }
            Buzzer::playPattern(argv[2]);
            CLI::sendAck("buzz", "Pattern playing");
        } else if (strcmp(argv[1], "stop") == 0) {
            Buzzer::stop();
            CLI::sendAck("buzz", "Stopped");
        } else {
            CLI::sendError("buzz", "Unknown subcommand");
        }
        return;
    }
    
    // Legacy pattern aliases
    if (strcmp(cmdName, "coin") == 0) {
        Buzzer::playPattern(PATTERN_COIN);
        CLI::sendAck("coin", "Playing");
        return;
    }
    if (strcmp(cmdName, "morgio") == 0) {
        Buzzer::playPattern(PATTERN_MORGIO);
        CLI::sendAck("morgio", "Playing");
        return;
    }
    if (strcmp(cmdName, "1up") == 0) {
        Buzzer::playPattern(PATTERN_1UP);
        CLI::sendAck("1up", "Playing");
        return;
    }
    
    // Optical modem commands
    if (strcmp(cmdName, "optx") == 0) {
        if (argc < 2) {
            CLI::sendError("optx", "Usage: optx <start|pattern|stop|status>");
            return;
        }
        
        if (strcmp(argv[1], "start") == 0) {
            // Parse start parameters
            OpticalTxConfig config;
            memset(&config, 0, sizeof(config));
            config.profile = OPTX_PROFILE_CAMERA_OOK;
            config.rate_hz = 10;
            config.color_on = PixelColor(255, 255, 255);
            config.color_off = PixelColor(0, 0, 0);
            
            // Parse profile
            if (argc > 2) {
                config.profile = opticalProfileFromName(argv[2]);
            }
            
            // Parse key=value pairs
            static uint8_t payloadBuf[128];
            for (int i = 3; i < argc; i++) {
                char* eq = strchr(argv[i], '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(argv[i], "rate_hz") == 0) {
                        config.rate_hz = atoi(eq + 1);
                    } else if (strcmp(argv[i], "payload_b64") == 0) {
                        config.payload_len = JsonIO::base64Decode(eq + 1, payloadBuf, sizeof(payloadBuf));
                        config.payload = payloadBuf;
                    } else if (strcmp(argv[i], "repeat") == 0) {
                        config.repeat = (strcmp(eq + 1, "true") == 0);
                    }
                }
            }
            
            if (config.payload_len > 0) {
                OpticalModem::startTransmit(config);
                CLI::sendAck("optx", "Transmission started");
            } else {
                CLI::sendError("optx", "No payload provided");
            }
        } else if (strcmp(argv[1], "pattern") == 0) {
            if (argc < 3) {
                CLI::sendError("optx", "Usage: optx pattern <name>");
                return;
            }
            OpticalPatternConfig cfg;
            cfg.pattern = argv[2];
            cfg.color = PixelColor(255, 255, 255);
            cfg.tempo_ms = 500;
            OpticalModem::startPattern(cfg);
            CLI::sendAck("optx", "Pattern started");
        } else if (strcmp(argv[1], "stop") == 0) {
            OpticalModem::stop();
            CLI::sendAck("optx", "Stopped");
        } else if (strcmp(argv[1], "status") == 0) {
            char status[256];
            OpticalModem::getStatus(status, sizeof(status));
            Serial.println(status);
        } else {
            CLI::sendError("optx", "Unknown subcommand");
        }
        return;
    }
    
    // Acoustic modem commands
    if (strcmp(cmdName, "aotx") == 0) {
        if (argc < 2) {
            CLI::sendError("aotx", "Usage: aotx <start|pattern|stop|status>");
            return;
        }
        
        if (strcmp(argv[1], "start") == 0) {
            AcousticTxConfig config;
            memset(&config, 0, sizeof(config));
            config.profile = AOTX_PROFILE_SIMPLE_FSK;
            config.f0 = ACOUSTIC_DEFAULT_F0;
            config.f1 = ACOUSTIC_DEFAULT_F1;
            config.symbol_ms = ACOUSTIC_DEFAULT_SYMBOL_MS;
            
            if (argc > 2) {
                config.profile = acousticProfileFromName(argv[2]);
            }
            
            static uint8_t payloadBuf[128];
            for (int i = 3; i < argc; i++) {
                char* eq = strchr(argv[i], '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(argv[i], "payload_b64") == 0) {
                        config.payload_len = JsonIO::base64Decode(eq + 1, payloadBuf, sizeof(payloadBuf));
                        config.payload = payloadBuf;
                    } else if (strcmp(argv[i], "f0") == 0) {
                        config.f0 = atoi(eq + 1);
                    } else if (strcmp(argv[i], "f1") == 0) {
                        config.f1 = atoi(eq + 1);
                    } else if (strcmp(argv[i], "symbol_ms") == 0) {
                        config.symbol_ms = atoi(eq + 1);
                    } else if (strcmp(argv[i], "repeat") == 0) {
                        config.repeat = (strcmp(eq + 1, "true") == 0);
                    }
                }
            }
            
            if (config.payload_len > 0) {
                AcousticModem::startTransmit(config);
                CLI::sendAck("aotx", "Transmission started");
            } else {
                CLI::sendError("aotx", "No payload provided");
            }
        } else if (strcmp(argv[1], "pattern") == 0) {
            if (argc < 3) {
                CLI::sendError("aotx", "Usage: aotx pattern <name>");
                return;
            }
            AcousticPatternConfig cfg;
            cfg.pattern = argv[2];
            cfg.from_hz = 500;
            cfg.to_hz = 2000;
            cfg.duration_ms = 2000;
            AcousticModem::startPattern(cfg);
            CLI::sendAck("aotx", "Pattern started");
        } else if (strcmp(argv[1], "stop") == 0) {
            AcousticModem::stop();
            CLI::sendAck("aotx", "Stopped");
        } else if (strcmp(argv[1], "status") == 0) {
            char status[256];
            AcousticModem::getStatus(status, sizeof(status));
            Serial.println(status);
        } else {
            CLI::sendError("aotx", "Unknown subcommand");
        }
        return;
    }
    
    // Stimulus commands
    if (strcmp(cmdName, "stim") == 0) {
        if (argc < 2) {
            CLI::sendError("stim", "Usage: stim <light|sound|stop|status>");
            return;
        }
        
        if (strcmp(argv[1], "light") == 0) {
            if (argc < 3) {
                CLI::sendError("stim", "Usage: stim light <pattern> [params]");
                return;
            }
            LightStimulusConfig cfg;
            cfg.pattern = argv[2];
            cfg.color = PixelColor(255, 255, 255);
            cfg.on_ms = 500;
            cfg.off_ms = 500;
            cfg.ramp_ms = 1000;
            cfg.cycles = 0;
            
            for (int i = 3; i < argc; i++) {
                char* eq = strchr(argv[i], '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(argv[i], "r") == 0) cfg.color.r = atoi(eq + 1);
                    else if (strcmp(argv[i], "g") == 0) cfg.color.g = atoi(eq + 1);
                    else if (strcmp(argv[i], "b") == 0) cfg.color.b = atoi(eq + 1);
                    else if (strcmp(argv[i], "on") == 0) cfg.on_ms = atoi(eq + 1);
                    else if (strcmp(argv[i], "off") == 0) cfg.off_ms = atoi(eq + 1);
                    else if (strcmp(argv[i], "ramp") == 0) cfg.ramp_ms = atoi(eq + 1);
                    else if (strcmp(argv[i], "cycles") == 0) cfg.cycles = atoi(eq + 1);
                }
            }
            
            Stimulus::startLight(cfg);
            CLI::sendAck("stim", "Light stimulus started");
        } else if (strcmp(argv[1], "sound") == 0) {
            if (argc < 3) {
                CLI::sendError("stim", "Usage: stim sound <pattern> [params]");
                return;
            }
            SoundStimulusConfig cfg;
            cfg.pattern = argv[2];
            cfg.freq_hz = 1000;
            cfg.freq_end_hz = 2000;
            cfg.on_ms = 500;
            cfg.off_ms = 500;
            cfg.cycles = 0;
            
            for (int i = 3; i < argc; i++) {
                char* eq = strchr(argv[i], '=');
                if (eq) {
                    *eq = '\0';
                    if (strcmp(argv[i], "freq") == 0) cfg.freq_hz = atoi(eq + 1);
                    else if (strcmp(argv[i], "freq_end") == 0) cfg.freq_end_hz = atoi(eq + 1);
                    else if (strcmp(argv[i], "on") == 0) cfg.on_ms = atoi(eq + 1);
                    else if (strcmp(argv[i], "off") == 0) cfg.off_ms = atoi(eq + 1);
                    else if (strcmp(argv[i], "cycles") == 0) cfg.cycles = atoi(eq + 1);
                }
            }
            
            Stimulus::startSound(cfg);
            CLI::sendAck("stim", "Sound stimulus started");
        } else if (strcmp(argv[1], "stop") == 0) {
            Stimulus::stopAll();
            CLI::sendAck("stim", "All stimuli stopped");
        } else if (strcmp(argv[1], "status") == 0) {
            char status[256];
            Stimulus::getStatus(status, sizeof(status));
            Serial.println(status);
        } else {
            CLI::sendError("stim", "Unknown subcommand");
        }
        return;
    }
    
    // Scan command (alias for periph scan - website compatibility)
    if (strcmp(cmdName, "scan") == 0) {
        int found = Peripherals::scan();
        JsonDoc doc;
        if (CLI::isMachineMode()) {
            doc["type"] = "periph_list";
            JsonIO::addTimestamp(doc);
            JsonIO::addBoardId(doc);
            
            JsonArray peripherals = doc["peripherals"].to<JsonArray>();
            for (int i = 0; i < Peripherals::getCount(); i++) {
                PeripheralDescriptor* desc = Peripherals::getByIndex(i);
                if (desc && desc->present) {
                    JsonObject periph = peripherals.createNestedObject();
                    periph["uid"] = desc->uid;
                    periph["address"] = desc->address;
                    periph["type"] = peripheralTypeName(desc->type);
                    periph["vendor"] = desc->vendor;
                    periph["product"] = desc->product;
                    periph["present"] = desc->present;
                }
            }
            doc["count"] = found;
        } else {
            doc["type"] = "periph_scan";
            doc["found"] = found;
        }
        JsonIO::emit(doc);
        return;
    }
    
    // Peripheral commands
    if (strcmp(cmdName, "periph") == 0) {
        if (argc < 2) {
            CLI::sendError("periph", "Usage: periph <scan|list|describe|hotplug>");
            return;
        }
        
        if (strcmp(argv[1], "scan") == 0) {
            int found = Peripherals::scan();
            JsonDoc doc;
            if (CLI::isMachineMode()) {
                doc["type"] = "periph_list";
                JsonIO::addTimestamp(doc);
                JsonIO::addBoardId(doc);
                
                JsonArray peripherals = doc["peripherals"].to<JsonArray>();
                for (int i = 0; i < Peripherals::getCount(); i++) {
                    PeripheralDescriptor* desc = Peripherals::getByIndex(i);
                    if (desc && desc->present) {
                        JsonObject periph = peripherals.createNestedObject();
                        periph["uid"] = desc->uid;
                        periph["address"] = desc->address;
                        periph["type"] = peripheralTypeName(desc->type);
                        periph["vendor"] = desc->vendor;
                        periph["product"] = desc->product;
                        periph["present"] = desc->present;
                    }
                }
                doc["count"] = found;
            } else {
                doc["type"] = "periph_scan";
                doc["found"] = found;
            }
            JsonIO::emit(doc);
        } else if (strcmp(argv[1], "list") == 0) {
            char list[1024];
            Peripherals::getListJson(list, sizeof(list));
            Serial.println(list);
        } else if (strcmp(argv[1], "describe") == 0) {
            if (argc < 3) {
                CLI::sendError("periph", "Usage: periph describe <address>");
                return;
            }
            uint8_t addr = strtol(argv[2], NULL, 0);
            PeripheralDescriptor* desc = Peripherals::getByAddress(addr);
            if (desc) {
                char json[512];
                Peripherals::getDescriptorJson(desc, json, sizeof(json));
                Serial.println(json);
            } else {
                CLI::sendError("periph", "Device not found");
            }
        } else if (strcmp(argv[1], "hotplug") == 0) {
            if (argc < 3) {
                CLI::sendError("periph", "Usage: periph hotplug <on|off>");
                return;
            }
            Peripherals::enableHotplug(strcmp(argv[2], "on") == 0);
            CLI::sendAck("periph", Peripherals::isHotplugEnabled() ? "Hotplug enabled" : "Hotplug disabled");
        } else {
            CLI::sendError("periph", "Unknown subcommand");
        }
        return;
    }
    
    // Output commands
    if (strcmp(cmdName, "out") == 0) {
        if (argc < 2) {
            CLI::sendError("out", "Usage: out <set|pwm|status>");
            return;
        }
        
        if (strcmp(argv[1], "set") == 0) {
            if (argc < 4) {
                CLI::sendError("out", "Usage: out set <1|2|3> <0|1>");
                return;
            }
            uint8_t ch = atoi(argv[2]);
            bool state = atoi(argv[3]) != 0;
            Outputs::set(ch, state);
            CLI::sendAck("out", state ? "On" : "Off");
        } else if (strcmp(argv[1], "pwm") == 0) {
            if (argc < 4) {
                CLI::sendError("out", "Usage: out pwm <1|2|3> <0-255> [freq]");
                return;
            }
            uint8_t ch = atoi(argv[2]);
            uint8_t val = atoi(argv[3]);
            uint16_t freq = (argc > 4) ? atoi(argv[4]) : 1000;
            Outputs::setPwm(ch, val, freq);
            CLI::sendAck("out", "PWM set");
        } else if (strcmp(argv[1], "status") == 0) {
            char status[256];
            Outputs::getStatus(status, sizeof(status));
            Serial.println(status);
        } else {
            CLI::sendError("out", "Unknown subcommand");
        }
        return;
    }
    
    // Unknown command
    CLI::sendError(cmdName, "Unknown command (try 'help')");
}

// ============================================================================
// SERIAL UPDATE
// ============================================================================

void CLI::update() {
    while (Serial.available()) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (inputPos > 0) {
                inputBuffer[inputPos] = '\0';
                processCommand(inputBuffer);
                inputPos = 0;
            }
        } else if (inputPos < (int)sizeof(inputBuffer) - 1) {
            inputBuffer[inputPos++] = c;
        }
    }
}

// ============================================================================
// COMMAND REGISTRATION
// ============================================================================

void registerCommand(const char* name, CommandHandler handler, const char* help) {
    if (commandCount < MAX_COMMANDS) {
        commands[commandCount].name = name;
        commands[commandCount].handler = handler;
        commands[commandCount].help = help;
        commandCount++;
    }
}

