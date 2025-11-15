#ifndef LED_SPOTLIGHT_H
#define LED_SPOTLIGHT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <FastLED.h>

// ============================================================================
// PIN KONFIGURATION
// ============================================================================

#define PIN_INNER_RING    16      // GPIO16 für inneren Ring
#define PIN_OUTER_RING    17      // GPIO17 für äußeren Ring

#define NUM_LEDS_INNER    8       // 8 LEDs im inneren Ring
#define NUM_LEDS_OUTER    24      // 26 LEDs im äußeren Ring

// ============================================================================
// STRUKTUREN & ENUMS
// ============================================================================

// Ring-Typ
enum RingType {
    RING_INNER,
    RING_OUTER,
    RING_BOTH
};

// Effekt-Typen
enum EffectType {
    EFFECT_STATIC,
    EFFECT_FADE,
    EFFECT_STROBE,
    EFFECT_PULSE,
    EFFECT_ROTATION,
    EFFECT_RAINBOW,
    EFFECT_CHASE,
    EFFECT_OFF
};

// Rotations-Pattern
enum RotationPattern {
    PATTERN_SINGLE,
    PATTERN_TRAIL,
    PATTERN_OPPOSITE,
    PATTERN_WAVE,
    PATTERN_RAINBOW_CHASE
};

// Rotations-Richtung
enum RotationDirection {
    DIRECTION_CLOCKWISE,
    DIRECTION_COUNTERCLOCKWISE
};

// Farb-Struktur
struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    
    Color() : r(0), g(0), b(0) {}
    Color(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
    
    CRGB toCRGB() const {
        return CRGB(r, g, b);
    }
};

// Rotation-Parameter
struct RotationParams {
    Color activeColor;
    Color inactiveColor;
    uint16_t speed;
    RotationDirection direction;
    RotationPattern pattern;
    uint8_t trailLength;
    
    RotationParams() :
        activeColor(255, 0, 0),
        inactiveColor(0, 0, 0),
        speed(100),
        direction(DIRECTION_CLOCKWISE),
        pattern(PATTERN_SINGLE),
        trailLength(3) {}
};

// Basis-Effekt
struct Effect {
    EffectType type;
    RingType ring;
    Color color;
    Color color2;
    uint8_t brightness;
    uint16_t speed;
    uint16_t duration;
    RotationParams rotation;
    
    Effect() :
        type(EFFECT_OFF),
        ring(RING_BOTH),
        color(0, 0, 0),
        color2(0, 0, 0),
        brightness(255),
        speed(100),
        duration(0) {}
};

// Effekt-State (für laufende Effekte)
struct EffectState {
    bool active;
    Effect effect;
    unsigned long startTime;
    unsigned long lastUpdate;
    uint8_t phase;          // Für verschiedene Effekt-Phasen
    uint8_t position;       // Für Rotation
    
    EffectState() :
        active(false),
        startTime(0),
        lastUpdate(0),
        phase(0),
        position(0) {}
};

// ============================================================================
// LED SPOTLIGHT KLASSE
// ============================================================================

class LEDSpotlight {
public:
    LEDSpotlight();
    
    // Initialisierung
    void begin(const char* ssid, const char* password, const char* spotlightId);
    void loop();
    
    // Effekt-Steuerung
    void setEffect(const Effect& effect);
    void stopEffect(RingType ring = RING_BOTH);
    void stopAllEffects();
    
    // LED-Steuerung
    void setColor(RingType ring, const Color& color, uint8_t brightness = 255);
    void clear(RingType ring = RING_BOTH);
    void setBrightness(uint8_t brightness);
    
    // Status
    String getStatusJson();
    
private:
    // Netzwerk
    WebServer server;
    String wifiSSID;
    String wifiPassword;
    String spotlightId;
    
    // LEDs
    CRGB innerRing[NUM_LEDS_INNER];
    CRGB outerRing[NUM_LEDS_OUTER];
    
    // Effekt-States
    EffectState innerState;
    EffectState outerState;
    
    // REST-API Handlers
    void setupRoutes();
    void handleRoot();
    void handleEffect();
    void handleStop();
    void handleStatus();
    
    // Effekt-Updates
    void updateEffects();
    void updateEffect(EffectState& state, CRGB* leds, uint8_t numLeds);
    
    // Einzelne Effekte
    void updateStatic(EffectState& state, CRGB* leds, uint8_t numLeds);
    void updateFade(EffectState& state, CRGB* leds, uint8_t numLeds);
    void updateStrobe(EffectState& state, CRGB* leds, uint8_t numLeds);
    void updatePulse(EffectState& state, CRGB* leds, uint8_t numLeds);
    void updateRotation(EffectState& state, CRGB* leds, uint8_t numLeds);
    void updateRainbow(EffectState& state, CRGB* leds, uint8_t numLeds);
    void updateChase(EffectState& state, CRGB* leds, uint8_t numLeds);
    
    // Rotation-Helpers
    void renderRotationSingle(CRGB* leds, uint8_t numLeds, const EffectState& state);
    void renderRotationTrail(CRGB* leds, uint8_t numLeds, const EffectState& state);
    void renderRotationOpposite(CRGB* leds, uint8_t numLeds, const EffectState& state);
    void renderRotationWave(CRGB* leds, uint8_t numLeds, const EffectState& state);
    
    // Hilfsfunktionen
    Color blendColor(const Color& c1, const Color& c2, float factor);
    CRGB blendCRGB(const CRGB& c1, const CRGB& c2, float factor);
    void applyBrightness(CRGB* leds, uint8_t numLeds, uint8_t brightness);
};

#endif // LED_SPOTLIGHT_H
