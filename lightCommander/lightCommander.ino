/*
 * Light Commander - Main Sketch
 * 
 * Zentrale Steuereinheit für LED-Scheinwerfer System
 * 
 * Hardware: ESP32
 * 
 * Features:
 * - Steuerung mehrerer WLED-basierter Scheinwerfer
 * - REST API für App-Integration
 * - Sequenz-Player für choreographierte Shows
 * - Rotation-Effekte auf LED-Ringen
 * - Vorbereitet für Spotify-Integration
 */

#include "LightCommander.h"

// ============================================================================
// KONFIGURATION
// ============================================================================

// WiFi Einstellungen
const char* WIFI_SSID = "FRITZ!Box 5690 JU";      // SSID für AP-Modus oder SSID zum Verbinden
const char* WIFI_PASSWORD = "73849419357971494145";    // Mindestens 8 Zeichen

// Modus: true = Access Point, false = Client (verbindet sich mit bestehendem WiFi)
const bool AP_MODE = false;

// ============================================================================
// GLOBALE OBJEKTE
// ============================================================================

LightCommander commander;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║     LIGHT COMMANDER v1.0               ║");
    Serial.println("║     LED Control System                 ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();
    
    // Light Commander initialisieren
    commander.begin(WIFI_SSID, WIFI_PASSWORD, AP_MODE);
    
    // Beispiel: Scheinwerfer hinzufügen (optional, kann auch über API erfolgen)
    // commander.addSpotlight("spot-1", "Front Left", "192.168.4.2");
    // commander.addSpotlight("spot-2", "Front Right", "192.168.4.3");
    // commander.addSpotlight("spot-3", "Back Left", "192.168.4.4");
    // commander.addSpotlight("spot-4", "Back Right", "192.168.4.5");
    
    // Beispiel-Sequenz laden (optional)
    loadExampleSequence();
    
    Serial.println("\n═══════════════════════════════════════");
    Serial.println("System ready! Access web interface at:");
    if (AP_MODE) {
        Serial.print("http://");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.print("http://");
        Serial.println(WiFi.localIP());
    }
    Serial.println("═══════════════════════════════════════\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    commander.loop();
    
    // Optional: Hier können zusätzliche Tasks eingefügt werden
    // z.B. Button-Handling, Display-Updates, etc.
}

// ============================================================================
// BEISPIEL-SEQUENZ
// ============================================================================

void loadExampleSequence() {
    // Pink Floyd Style "Pulse" Sequenz
    String exampleSequence = R"({
        "id": "pulse-intro",
        "name": "Pulse Intro",
        "duration": 30000,
        "loop": true,
        "spotifyUri": "",
        "syncWithSpotify": false,
        "events": [
            {
                "timestamp": 0,
                "targets": ["spot-1", "spot-2", "spot-3", "spot-4"],
                "ring": "outer",
                "effect": "static",
                "params": {
                    "color": [0, 0, 0],
                    "brightness": 0
                }
            },
            {
                "timestamp": 500,
                "targets": ["spot-1"],
                "ring": "inner",
                "effect": "rotation",
                "params": {
                    "rotation": {
                        "activeColor": [255, 255, 255],
                        "inactiveColor": [0, 0, 0],
                        "speed": 100,
                        "direction": "clockwise",
                        "pattern": "single"
                    }
                }
            },
            {
                "timestamp": 2000,
                "targets": ["spot-2"],
                "ring": "inner",
                "effect": "rotation",
                "params": {
                    "rotation": {
                        "activeColor": [255, 255, 255],
                        "inactiveColor": [0, 0, 0],
                        "speed": 100,
                        "direction": "clockwise",
                        "pattern": "single",
                        "startOffset": 2
                    }
                }
            },
            {
                "timestamp": 4000,
                "targets": ["spot-3"],
                "ring": "inner",
                "effect": "rotation",
                "params": {
                    "rotation": {
                        "activeColor": [255, 255, 255],
                        "inactiveColor": [0, 0, 0],
                        "speed": 100,
                        "direction": "clockwise",
                        "pattern": "single",
                        "startOffset": 4
                    }
                }
            },
            {
                "timestamp": 6000,
                "targets": ["spot-4"],
                "ring": "inner",
                "effect": "rotation",
                "params": {
                    "rotation": {
                        "activeColor": [255, 255, 255],
                        "inactiveColor": [0, 0, 0],
                        "speed": 100,
                        "direction": "clockwise",
                        "pattern": "single",
                        "startOffset": 6
                    }
                }
            },
            {
                "timestamp": 8000,
                "targets": ["spot-1", "spot-2", "spot-3", "spot-4"],
                "ring": "outer",
                "effect": "pulse",
                "params": {
                    "color": [255, 0, 100],
                    "brightness": 180,
                    "duration": 2000
                }
            },
            {
                "timestamp": 15000,
                "targets": ["spot-1", "spot-3"],
                "ring": "both",
                "effect": "strobe",
                "params": {
                    "color": [255, 255, 255],
                    "brightness": 255,
                    "frequency": 15,
                    "duration": 1000
                }
            },
            {
                "timestamp": 20000,
                "targets": ["spot-1", "spot-2", "spot-3", "spot-4"],
                "ring": "outer",
                "effect": "rainbow",
                "params": {
                    "brightness": 200,
                    "duration": 5000
                }
            }
        ]
    })";
    
    if (commander.loadSequence(exampleSequence)) {
        Serial.println("✓ Example sequence loaded: pulse-intro");
    } else {
        Serial.println("✗ Failed to load example sequence");
    }
}

// ============================================================================
// OPTIONALE HILFSFUNKTIONEN
// ============================================================================

/*
 * Beispiel: Button-Handler für physische Start/Stop Buttons am Commander
 */
void handleStartButton() {
    if (commander.getPlaybackState().active) {
        commander.stopSequence();
        Serial.println("Sequence stopped via button");
    } else {
        commander.playSequence("pulse-intro");
        Serial.println("Sequence started via button");
    }
}

/*
 * Beispiel: Spotify-Integration vorbereiten (für später)
 */
void setupSpotify() {
    // Wenn Spotify-Credentials verfügbar sind:
    // const String clientId = "YOUR_SPOTIFY_CLIENT_ID";
    // const String clientSecret = "YOUR_SPOTIFY_CLIENT_SECRET";
    // 
    // if (commander.connectSpotify(clientId, clientSecret)) {
    //     Serial.println("✓ Spotify connected");
    // } else {
    //     Serial.println("✗ Spotify connection failed");
    // }
}
