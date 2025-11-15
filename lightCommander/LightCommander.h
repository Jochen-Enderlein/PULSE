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
// STRUKTUREN & ENUMS (identisch mit LED-Scheinwerfer!)
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

// Effekt-Parameter
struct EffectParams {
    Color color;
    Color color2;
    uint8_t brightness;
    uint16_t speed;
    uint16_t duration;
    RotationParams rotation;
    
    EffectParams() :
        color(255, 255, 255),
        color2(0, 0, 0),
        brightness(255),
        speed(100),
        duration(0) {}
};

// Sequenz-Event
struct SequenceEvent {
    unsigned long timestamp;
    std::vector<String> targets;
    RingType ring;
    EffectType effect;
    EffectParams params;
};

// Sequenz
struct Sequence {
    String id;
    String name;
    unsigned long duration;
    bool loop;
    std::vector<SequenceEvent> events;
    String spotifyUri;
    bool syncWithSpotify;
    
    Sequence() : duration(0), loop(false), syncWithSpotify(false) {}
};

// Scheinwerfer
struct Spotlight {
    String id;
    String name;
    String ip;
    bool online;
    unsigned long lastSeen;
    
    Spotlight() : online(false), lastSeen(0) {}
};

// Playback-Status
struct PlaybackState {
    bool active;
    String currentSequence;
    unsigned long startTime;
    bool paused;
    unsigned long pauseTime;
    
    PlaybackState() : active(false), startTime(0), paused(false), pauseTime(0) {}
};

// ============================================================================
// LIGHT COMMANDER KLASSE
// ============================================================================

class LightCommander {
public:
    LightCommander();
    
    // Init
    void begin(const char* ssid, const char* password, bool apMode = false);
    void loop();
    
    // Scheinwerfer-Management
    bool addSpotlight(const String& id, const String& name, const String& ip);
    bool removeSpotlight(const String& id);
    Spotlight* getSpotlight(const String& id);
    std::vector<Spotlight*> getAllSpotlights();
    
    // Effekt-Steuerung
    bool sendEffect(const std::vector<String>& targets, RingType ring, 
                    EffectType effect, const EffectParams& params);
    bool stopEffect(const std::vector<String>& targets, RingType ring = RING_BOTH);
    
    // Sequenz-Management
    bool loadSequence(const String& json);
    Sequence* getSequence(const String& id);
    std::vector<String> listSequences();
    
    // Sequenz-Playback
    bool playSequence(const String& sequenceId);
    bool pauseSequence();
    bool resumeSequence();
    bool stopSequence();
    
    // Status
    String getStatusJson();
    
private:
    // Netzwerk
    WebServer server;
    String wifiSSID;
    String wifiPassword;
    bool isAPMode;
    
    // Geräte
    std::map<String, Spotlight> spotlights;
    
    // Sequenzen
    std::map<String, Sequence> sequences;
    
    // Playback
    PlaybackState playback;
    Sequence* currentSequence;
    size_t currentEventIndex;
    
    // REST API Handlers
    void setupRoutes();
    void handleRoot();
    void handleStatus();
    void handleAddSpotlight();
    void handleListSpotlights();
    void handleSendEffect();
    void handleStopEffect();
    void handleLoadSequence();
    void handleListSequences();
    void handlePlaySequence();
    void handlePauseSequence();
    void handleResumeSequence();
    void handleStopSequence();
    
    // Interne Methoden
    bool sendToSpotlight(const String& ip, const String& json);
    String buildEffectJson(RingType ring, EffectType effect, const EffectParams& params);
    void checkSpotlightStatus();
    void updateSequencePlayback();
    void processSequenceEvent(const SequenceEvent& event);
};

#endif // LIGHT_COMMANDER_H
