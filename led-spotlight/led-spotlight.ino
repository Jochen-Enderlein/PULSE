/*
 * LED Spotlight - Main Sketch
 * 
 * ESP32-basierter LED-Scheinwerfer mit 2 WS2812B Ringen
 * Empfängt Befehle vom Light Commander
 * 
 * Hardware:
 * - ESP32 DevKit
 * - Inner Ring: 8x WS2812B auf GPIO16
 * - Outer Ring: 26x WS2812B auf GPIO17
 * 
 * Features:
 * - REST API für Effekt-Befehle
 * - Rotation-Effekte (Single, Trail, Opposite, Wave)
 * - Static, Fade, Strobe, Pulse, Rainbow, Chase
 * - Unabhängige Steuerung beider Ringe
 */

#include "LEDSpotlight.h"

// ============================================================================
// KONFIGURATION
// ============================================================================

// WiFi Einstellungen
const char* WIFI_SSID = "FRITZ!Box 5690 JU";        // SSID zum Verbinden
const char* WIFI_PASSWORD = "73849419357971494145";     // WiFi Passwort

// Scheinwerfer ID (eindeutig!)
const char* SPOTLIGHT_ID = "spot-1";             // Ändern für jeden Scheinwerfer!

// Pin-Konfiguration (optional anpassen)
// Definiert in LEDSpotlight.h:
// - PIN_INNER_RING = 16
// - PIN_OUTER_RING = 17
// - NUM_LEDS_INNER = 8
// - NUM_LEDS_OUTER = 26

// ============================================================================
// GLOBALE OBJEKTE
// ============================================================================

LEDSpotlight spotlight;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║     LED SPOTLIGHT                      ║");
    Serial.println("║     Scheinwerfer-Controller            ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();
    
    // Spotlight initialisieren
    spotlight.begin(WIFI_SSID, WIFI_PASSWORD, SPOTLIGHT_ID);
    
    Serial.println("\n═══════════════════════════════════════");
    Serial.println("Spotlight ready! Waiting for commands...");
    Serial.println("═══════════════════════════════════════\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    spotlight.loop();
    
    // Optional: Hier können zusätzliche Tasks eingefügt werden
    // z.B. Button-Handling, Sensor-Auswertung, etc.
}

// ============================================================================
// OPTIONALE HILFSFUNKTIONEN
// ============================================================================

/*
 * Beispiel: Teste LEDs beim Start
 */
void testLEDs() {
    Serial.println("Testing LEDs...");
    
    // Innerer Ring: Rot
    spotlight.setColor(RING_INNER, Color(255, 0, 0), 100);
    delay(500);
    
    // Äußerer Ring: Blau
    spotlight.setColor(RING_OUTER, Color(0, 0, 255), 100);
    delay(500);
    
    // Beide: Grün
    spotlight.setColor(RING_BOTH, Color(0, 255, 0), 100);
    delay(500);
    
    // Aus
    spotlight.clear();
    
    Serial.println("✓ LED test complete");
}

/*
 * Beispiel: Debug-Ausgabe bei Effekt-Empfang
 * (Bereits in LEDSpotlight.cpp implementiert)
 */
