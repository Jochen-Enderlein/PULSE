/*
 * Light Commander - Master Controller
 * 
 * ESP32-basierter Master-Controller für LED-Scheinwerfer
 * Sendet Effekte und Sequenzen an mehrere Scheinwerfer
 * 
 * Features:
 * - REST API für Steuerung
 * - Sequenz-Management
 * - Multi-Scheinwerfer-Steuerung
 * - WiFi Client oder Access Point Mode
 */

#include "LightCommander.h"

// ============================================================================
// KONFIGURATION
// ============================================================================

// WiFi Einstellungen
const char* WIFI_SSID = "FRITZ!Box 5690 JU";          // Dein WiFi-Name
const char* WIFI_PASSWORD = "73849419357971494145";      // Dein WiFi-Passwort

// Modus
const bool AP_MODE = false;  // false = Client Mode, true = Access Point

// WICHTIG:
// - Client Mode: Commander verbindet sich mit deinem WiFi
// - AP Mode: Commander erstellt eigenes WiFi "LightCommander"

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
    
    // Commander initialisieren
    commander.begin(WIFI_SSID, WIFI_PASSWORD, AP_MODE);
    
    // Optional: Scheinwerfer im Code hinzufügen
    // (Alternativ via REST API)
    // commander.addSpotlight("spot-1", "Vorne Links", "192.168.178.52");
    // commander.addSpotlight("spot-2", "Vorne Rechts", "192.168.178.53");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    commander.loop();
}
