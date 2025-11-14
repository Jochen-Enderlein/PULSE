#ifndef LIGHT_COMMANDER_H
#define LIGHT_COMMANDER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <vector>
#include <map>

// ============================================================================
// STRUKTUREN & ENUMS
// ============================================================================

// Farb-Struktur
struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    
    Color() : r(0), g(0), b(0) {}
    Color(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
};

// Ring-Typ
enum RingType {
    RING_INNER,   // 8 LEDs
    RING_OUTER,   // 26 LEDs
    RING_BOTH     // Beide Ringe
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
    EFFECT_NONE
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

// Effekt-Parameter für Rotation
struct RotationParams {
    Color activeColor;
    Color inactiveColor;
    uint16_t speed;                    // ms pro Step
    RotationDirection direction;
    RotationPattern pattern;
    uint8_t length;                    // Anzahl aktiver LEDs
    bool trailFade;
    uint8_t trailLength;
    uint8_t startOffset;               // Start-Position (0-LED_COUNT)
    
    RotationParams() : 
        activeColor(255, 0, 0),
        inactiveColor(0, 0, 50),
        speed(100),
        direction(DIRECTION_CLOCKWISE),
        pattern(PATTERN_SINGLE),
        length(1),
        trailFade(false),
        trailLength(3),
        startOffset(0) {}
};

// Basis-Effekt-Parameter
struct EffectParams {
    Color color;
    Color color2;                      // Für Fades, etc.
    uint8_t brightness;
    uint16_t duration;                 // ms
    uint16_t frequency;                // Für Strobe
    RotationParams rotation;           // Spezifisch für Rotation
    
    EffectParams() : 
        color(255, 255, 255),
        color2(0, 0, 0),
        brightness(255),
        duration(0),
        frequency(10) {}
};

// Sequenz-Event
struct SequenceEvent {
    unsigned long timestamp;           // ms seit Sequenz-Start
    std::vector<String> targets;       // Ziel-Scheinwerfer IDs
    RingType ring;
    EffectType effect;
    EffectParams params;
};

// Sequenz
struct Sequence {
    String id;
    String name;
    unsigned long duration;            // Gesamtdauer in ms
    bool loop;
    std::vector<SequenceEvent> events;
    
    // Platzhalter für spätere Spotify-Integration
    String spotifyUri;                 // Spotify Track URI (später)
    bool syncWithSpotify;              // Flag für Spotify-Sync (später)
};

// Scheinwerfer-Gerät
struct Spotlight {
    String id;
    String name;
    String ip;
    uint16_t port;
    bool online;
    unsigned long lastSeen;
    uint8_t innerLedCount;             // Standard: 8
    uint8_t outerLedCount;             // Standard: 26
    
    Spotlight() : 
        port(80),
        online(false),
        lastSeen(0),
        innerLedCount(8),
        outerLedCount(26) {}
};

// Playback-Status
struct PlaybackState {
    bool active;
    String currentSequence;
    unsigned long startTime;
    unsigned long position;            // Aktuelle Position in ms
    bool paused;
    
    // Platzhalter für Spotify
    bool spotifyConnected;             // Später für Spotify-Status
    
    PlaybackState() : 
        active(false),
        startTime(0),
        position(0),
        paused(false),
        spotifyConnected(false) {}
};

// ============================================================================
// LIGHT COMMANDER KLASSE
// ============================================================================

class LightCommander {
public:
    LightCommander();
    
    // Initialisierung
    void begin(const char* ssid, const char* password, bool apMode = false);
    void loop();
    
    // Scheinwerfer-Management
    bool addSpotlight(const String& id, const String& name, const String& ip);
    bool removeSpotlight(const String& id);
    Spotlight* getSpotlight(const String& id);
    std::vector<Spotlight*> getAllSpotlights();
    void discoverSpotlights();
    void checkSpotlightStatus();
    
    // Effekt-Steuerung (Direkt)
    bool sendEffect(const std::vector<String>& targets, RingType ring, EffectType effect, const EffectParams& params);
    bool stopEffect(const std::vector<String>& targets);
    bool setColor(const std::vector<String>& targets, RingType ring, const Color& color, uint8_t brightness = 255);
    
    // Sequenz-Management
    bool loadSequence(const String& json);
    bool saveSequence(const Sequence& seq);
    Sequence* getSequence(const String& id);
    std::vector<String> listSequences();
    bool deleteSequence(const String& id);
    
    // Sequenz-Playback
    bool playSequence(const String& sequenceId);
    bool pauseSequence();
    bool resumeSequence();
    bool stopSequence();
    PlaybackState getPlaybackState();
    
    // Status
    String getStatusJson();
    
    // Platzhalter für spätere Spotify-Integration
    // Diese Methoden sind bereits definiert, aber noch nicht implementiert
    bool connectSpotify(const String& clientId, const String& clientSecret);
    bool playSpotifyTrack(const String& uri);
    unsigned long getSpotifyPosition();
    
private:
    // Netzwerk
    WebServer server;
    String wifiSSID;
    String wifiPassword;
    bool isAPMode;
    
    // Geräte-Registry
    std::map<String, Spotlight> spotlights;
    
    // Sequenzen
    std::map<String, Sequence> sequences;
    
    // Playback
    PlaybackState playback;
    Sequence* currentSequence;
    size_t currentEventIndex;
    
    // Rotation-States (für laufende Rotations-Effekte)
    struct RotationState {
        uint8_t currentPosition;
        unsigned long lastUpdate;
        bool active;
        RotationParams params;
        RingType ring;
    };
    std::map<String, RotationState> rotationStates;
    
    // Platzhalter für Spotify
    String spotifyAccessToken;
    bool spotifyConnected;
    
    // REST-API Handlers
    void setupRoutes();
    void handleRoot();
    void handleGetStatus();
    void handleAddSpotlight();
    void handleListSpotlights();
    void handleSendEffect();
    void handleStopEffect();
    void handleLoadSequence();
    void handleListSequences();
    void handlePlaySequence();
    void handlePauseSequence();
    void handleStopSequence();
    
    // Interne Methoden
    bool sendToWLED(const String& ip, const String& json);
    String buildWLEDJson(RingType ring, EffectType effect, const EffectParams& params);
    String buildRotationPatternJson(const RotationState& state, uint8_t ledCount);
    void updateRotationEffects();
    void updateSequencePlayback();
    void processSequenceEvent(const SequenceEvent& event);
    Color blendColor(const Color& c1, const Color& c2, float factor);
    String colorToJson(const Color& c);
    
    // Hilfsfunktionen
    unsigned long getCurrentTime();
};

#endif // LIGHT_COMMANDER_H
