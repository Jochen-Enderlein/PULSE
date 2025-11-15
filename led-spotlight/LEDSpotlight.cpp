#include "LEDSpotlight.h"

// ============================================================================
// KONSTRUKTOR & INITIALISIERUNG
// ============================================================================

LEDSpotlight::LEDSpotlight() : server(80) {
}

void LEDSpotlight::begin(const char* ssid, const char* password, const char* spotId) {
    Serial.begin(115200);
    Serial.println("\n=== LED Spotlight Starting ===");
    
    wifiSSID = String(ssid);
    wifiPassword = String(password);
    spotlightId = String(spotId);
    
    // FastLED Setup
    FastLED.addLeds<WS2812B, PIN_INNER_RING, GRB>(innerRing, NUM_LEDS_INNER);
    FastLED.addLeds<WS2812B, PIN_OUTER_RING, GRB>(outerRing, NUM_LEDS_OUTER);
    FastLED.setBrightness(255);
    FastLED.clear();
    FastLED.show();
    
    Serial.println("✓ FastLED initialized");
    Serial.printf("  Inner Ring: %d LEDs on GPIO%d\n", NUM_LEDS_INNER, PIN_INNER_RING);
    Serial.printf("  Outer Ring: %d LEDs on GPIO%d\n", NUM_LEDS_OUTER, PIN_OUTER_RING);
    
    // WiFi Setup
    Serial.println("Connecting to WiFi...");
    Serial.print("SSID: ");
    Serial.println(ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("Signal: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    } else {
        Serial.println("\n✗ WiFi failed!");
    }
    
    // REST API Routes
    setupRoutes();
    server.begin();
    
    Serial.printf("\n✓ Spotlight '%s' ready!\n", spotId);
    Serial.println("Listening for commands from Light Commander...\n");
    
    // Startup-Animation (kurz)
    for (int i = 0; i < NUM_LEDS_INNER; i++) {
        innerRing[i] = CRGB::Blue;
        FastLED.show();
        delay(50);
    }
    FastLED.clear();
    FastLED.show();
}

void LEDSpotlight::loop() {
    server.handleClient();
    updateEffects();
    FastLED.show();
}

// ============================================================================
// REST API ROUTES
// ============================================================================

void LEDSpotlight::setupRoutes() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/effect", HTTP_POST, [this]() { handleEffect(); });
    server.on("/stop", HTTP_POST, [this]() { handleStop(); });
    server.on("/status", HTTP_GET, [this]() { handleStatus(); });
}

void LEDSpotlight::handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>LED Spotlight</title></head><body>";
    html += "<h1>LED Spotlight: " + spotlightId + "</h1>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "<h2>Status:</h2>";
    html += "<ul>";
    html += "<li>Inner Ring: " + String(innerState.active ? "Active" : "Idle") + "</li>";
    html += "<li>Outer Ring: " + String(outerState.active ? "Active" : "Idle") + "</li>";
    html += "</ul>";
    html += "<h2>API Endpoints:</h2>";
    html += "<ul>";
    html += "<li>POST /effect - Set effect</li>";
    html += "<li>POST /stop - Stop effects</li>";
    html += "<li>GET /status - Get status</li>";
    html += "</ul>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void LEDSpotlight::handleEffect() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    String body = server.arg("plain");
    Serial.println("\n📥 Received command from Commander:");
    Serial.println(body);
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        Serial.println("✗ JSON parse error!");
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Effekt parsen
    Effect effect;
    
    // Ring
    String ringStr = doc["ring"] | "both";
    if (ringStr == "inner") effect.ring = RING_INNER;
    else if (ringStr == "outer") effect.ring = RING_OUTER;
    else effect.ring = RING_BOTH;
    
    // Effekt-Typ
    String effectStr = doc["effect"] | "static";
    if (effectStr == "static") effect.type = EFFECT_STATIC;
    else if (effectStr == "fade") effect.type = EFFECT_FADE;
    else if (effectStr == "strobe") effect.type = EFFECT_STROBE;
    else if (effectStr == "pulse") effect.type = EFFECT_PULSE;
    else if (effectStr == "rotation") effect.type = EFFECT_ROTATION;
    else if (effectStr == "rainbow") effect.type = EFFECT_RAINBOW;
    else if (effectStr == "chase") effect.type = EFFECT_CHASE;
    
    // Farbe
    if (doc.containsKey("color")) {
        JsonArray colorArray = doc["color"];
        effect.color = Color(colorArray[0], colorArray[1], colorArray[2]);
    }
    
    // Zweite Farbe (für Fade)
    if (doc.containsKey("color2")) {
        JsonArray color2Array = doc["color2"];
        effect.color2 = Color(color2Array[0], color2Array[1], color2Array[2]);
    }
    
    // Helligkeit
    if (doc.containsKey("brightness")) {
        effect.brightness = doc["brightness"];
    }
    
    // Speed/Duration
    if (doc.containsKey("speed")) {
        effect.speed = doc["speed"];
    }
    if (doc.containsKey("duration")) {
        effect.duration = doc["duration"];
    }
    
    // Rotation-spezifische Parameter
    if (effect.type == EFFECT_ROTATION && doc.containsKey("rotation")) {
        JsonObject rot = doc["rotation"];
        
        if (rot.containsKey("activeColor")) {
            JsonArray ac = rot["activeColor"];
            effect.rotation.activeColor = Color(ac[0], ac[1], ac[2]);
        }
        if (rot.containsKey("inactiveColor")) {
            JsonArray ic = rot["inactiveColor"];
            effect.rotation.inactiveColor = Color(ic[0], ic[1], ic[2]);
        }
        if (rot.containsKey("speed")) {
            effect.rotation.speed = rot["speed"];
        }
        if (rot.containsKey("direction")) {
            String dir = rot["direction"] | "clockwise";
            effect.rotation.direction = (dir == "counterclockwise") ? 
                DIRECTION_COUNTERCLOCKWISE : DIRECTION_CLOCKWISE;
        }
        if (rot.containsKey("pattern")) {
            String pat = rot["pattern"] | "single";
            if (pat == "trail") effect.rotation.pattern = PATTERN_TRAIL;
            else if (pat == "opposite") effect.rotation.pattern = PATTERN_OPPOSITE;
            else if (pat == "wave") effect.rotation.pattern = PATTERN_WAVE;
            else if (pat == "rainbow_chase") effect.rotation.pattern = PATTERN_RAINBOW_CHASE;
            else effect.rotation.pattern = PATTERN_SINGLE;
        }
        if (rot.containsKey("trailLength")) {
            effect.rotation.trailLength = rot["trailLength"];
        }
    }
    
    // Effekt setzen
    setEffect(effect);
    
    Serial.println("✓ Effect applied!");
    server.send(200, "application/json", "{\"success\":true}");
}

void LEDSpotlight::handleStop() {
    Serial.println("📥 Stop command received");
    
    if (server.hasArg("plain")) {
        StaticJsonDocument<256> doc;
        deserializeJson(doc, server.arg("plain"));
        
        String ringStr = doc["ring"] | "both";
        if (ringStr == "inner") stopEffect(RING_INNER);
        else if (ringStr == "outer") stopEffect(RING_OUTER);
        else stopAllEffects();
    } else {
        stopAllEffects();
    }
    
    server.send(200, "application/json", "{\"success\":true}");
}

void LEDSpotlight::handleStatus() {
    server.send(200, "application/json", getStatusJson());
}

// ============================================================================
// EFFEKT-STEUERUNG
// ============================================================================

void LEDSpotlight::setEffect(const Effect& effect) {
    unsigned long now = millis();
    
    if (effect.ring == RING_INNER || effect.ring == RING_BOTH) {
        innerState.active = true;
        innerState.effect = effect;
        innerState.effect.ring = RING_INNER;
        innerState.startTime = now;
        innerState.lastUpdate = now;
        innerState.phase = 0;
        innerState.position = 0;
        
        Serial.printf("✓ Inner ring: %s\n", 
            effect.type == EFFECT_ROTATION ? "ROTATION" :
            effect.type == EFFECT_PULSE ? "PULSE" :
            effect.type == EFFECT_STROBE ? "STROBE" : "OTHER");
    }
    
    if (effect.ring == RING_OUTER || effect.ring == RING_BOTH) {
        outerState.active = true;
        outerState.effect = effect;
        outerState.effect.ring = RING_OUTER;
        outerState.startTime = now;
        outerState.lastUpdate = now;
        outerState.phase = 0;
        outerState.position = 0;
        
        Serial.printf("✓ Outer ring: %s\n",
            effect.type == EFFECT_ROTATION ? "ROTATION" :
            effect.type == EFFECT_PULSE ? "PULSE" :
            effect.type == EFFECT_STROBE ? "STROBE" : "OTHER");
    }
}

void LEDSpotlight::stopEffect(RingType ring) {
    if (ring == RING_INNER || ring == RING_BOTH) {
        innerState.active = false;
        for (int i = 0; i < NUM_LEDS_INNER; i++) {
            innerRing[i] = CRGB::Black;
        }
    }
    
    if (ring == RING_OUTER || ring == RING_BOTH) {
        outerState.active = false;
        for (int i = 0; i < NUM_LEDS_OUTER; i++) {
            outerRing[i] = CRGB::Black;
        }
    }
}

void LEDSpotlight::stopAllEffects() {
    stopEffect(RING_BOTH);
}

void LEDSpotlight::setColor(RingType ring, const Color& color, uint8_t brightness) {
    CRGB crgb = color.toCRGB();
    
    if (ring == RING_INNER || ring == RING_BOTH) {
        for (int i = 0; i < NUM_LEDS_INNER; i++) {
            innerRing[i] = crgb;
        }
    }
    
    if (ring == RING_OUTER || ring == RING_BOTH) {
        for (int i = 0; i < NUM_LEDS_OUTER; i++) {
            outerRing[i] = crgb;
        }
    }
    
    FastLED.setBrightness(brightness);
}

void LEDSpotlight::clear(RingType ring) {
    setColor(ring, Color(0, 0, 0), 0);
}

void LEDSpotlight::setBrightness(uint8_t brightness) {
    FastLED.setBrightness(brightness);
}

// ============================================================================
// EFFEKT-UPDATE ENGINE
// ============================================================================

void LEDSpotlight::updateEffects() {
    if (innerState.active) {
        updateEffect(innerState, innerRing, NUM_LEDS_INNER);
    }
    
    if (outerState.active) {
        updateEffect(outerState, outerRing, NUM_LEDS_OUTER);
    }
}

void LEDSpotlight::updateEffect(EffectState& state, CRGB* leds, uint8_t numLeds) {
    switch (state.effect.type) {
        case EFFECT_STATIC:
            updateStatic(state, leds, numLeds);
            break;
        case EFFECT_FADE:
            updateFade(state, leds, numLeds);
            break;
        case EFFECT_STROBE:
            updateStrobe(state, leds, numLeds);
            break;
        case EFFECT_PULSE:
            updatePulse(state, leds, numLeds);
            break;
        case EFFECT_ROTATION:
            updateRotation(state, leds, numLeds);
            break;
        case EFFECT_RAINBOW:
            updateRainbow(state, leds, numLeds);
            break;
        case EFFECT_CHASE:
            updateChase(state, leds, numLeds);
            break;
        default:
            break;
    }
}

// ============================================================================
// EINZELNE EFFEKTE
// ============================================================================

void LEDSpotlight::updateStatic(EffectState& state, CRGB* leds, uint8_t numLeds) {
    CRGB color = state.effect.color.toCRGB();
    
    for (int i = 0; i < numLeds; i++) {
        leds[i] = color;
    }
    
    applyBrightness(leds, numLeds, state.effect.brightness);
}

void LEDSpotlight::updateFade(EffectState& state, CRGB* leds, uint8_t numLeds) {
    unsigned long elapsed = millis() - state.startTime;
    
    if (state.effect.duration > 0 && elapsed >= state.effect.duration) {
        // Fade fertig → Zielfarbe setzen
        CRGB color = state.effect.color2.toCRGB();
        for (int i = 0; i < numLeds; i++) {
            leds[i] = color;
        }
        state.active = false;
        return;
    }
    
    // Fade-Fortschritt berechnen
    float progress = (state.effect.duration > 0) ? 
        (float)elapsed / state.effect.duration : 0.0;
    progress = constrain(progress, 0.0, 1.0);
    
    // Farben mischen
    Color blended = blendColor(state.effect.color, state.effect.color2, 1.0 - progress);
    CRGB crgb = blended.toCRGB();
    
    for (int i = 0; i < numLeds; i++) {
        leds[i] = crgb;
    }
    
    applyBrightness(leds, numLeds, state.effect.brightness);
}

void LEDSpotlight::updateStrobe(EffectState& state, CRGB* leds, uint8_t numLeds) {
    unsigned long now = millis();
    unsigned long elapsed = now - state.startTime;
    
    // Duration prüfen
    if (state.effect.duration > 0 && elapsed >= state.effect.duration) {
        state.active = false;
        for (int i = 0; i < numLeds; i++) {
            leds[i] = CRGB::Black;
        }
        return;
    }
    
    // Strobe-Frequenz (in Hz)
    uint16_t intervalMs = 1000 / state.effect.speed;  // speed = Hz
    
    // Toggle zwischen an/aus
    bool on = ((now / intervalMs) % 2) == 0;
    
    if (on) {
        CRGB color = state.effect.color.toCRGB();
        for (int i = 0; i < numLeds; i++) {
            leds[i] = color;
        }
    } else {
        for (int i = 0; i < numLeds; i++) {
            leds[i] = CRGB::Black;
        }
    }
    
    applyBrightness(leds, numLeds, state.effect.brightness);
}

void LEDSpotlight::updatePulse(EffectState& state, CRGB* leds, uint8_t numLeds) {
    unsigned long now = millis();
    
    // Puls-Zyklus (duration = Länge eines kompletten Zyklus)
    uint16_t cycleDuration = state.effect.duration > 0 ? state.effect.duration : 2000;
    unsigned long cyclePosition = (now - state.startTime) % cycleDuration;
    
    // Sinus-Welle für Breathing-Effekt
    float phase = (float)cyclePosition / cycleDuration * 2.0 * PI;
    float brightness = (sin(phase) + 1.0) / 2.0;  // 0.0 - 1.0
    
    CRGB color = state.effect.color.toCRGB();
    
    for (int i = 0; i < numLeds; i++) {
        leds[i] = color;
        leds[i].fadeToBlackBy(255 * (1.0 - brightness));
    }
    
    applyBrightness(leds, numLeds, state.effect.brightness);
}

void LEDSpotlight::updateRotation(EffectState& state, CRGB* leds, uint8_t numLeds) {
    unsigned long now = millis();
    
    // Zeit für nächsten Step?
    if (now - state.lastUpdate >= state.effect.rotation.speed) {
        state.lastUpdate = now;
        
        // Position aktualisieren
        if (state.effect.rotation.direction == DIRECTION_CLOCKWISE) {
            state.position = (state.position + 1) % numLeds;
        } else {
            state.position = (state.position - 1 + numLeds) % numLeds;
        }
    }
    
    // Pattern rendern
    switch (state.effect.rotation.pattern) {
        case PATTERN_SINGLE:
            renderRotationSingle(leds, numLeds, state);
            break;
        case PATTERN_TRAIL:
            renderRotationTrail(leds, numLeds, state);
            break;
        case PATTERN_OPPOSITE:
            renderRotationOpposite(leds, numLeds, state);
            break;
        case PATTERN_WAVE:
            renderRotationWave(leds, numLeds, state);
            break;
        case PATTERN_RAINBOW_CHASE:
            // Rainbow wird über updateRainbow gehandelt
            break;
    }
    
    applyBrightness(leds, numLeds, state.effect.brightness);
}

void LEDSpotlight::updateRainbow(EffectState& state, CRGB* leds, uint8_t numLeds) {
    unsigned long now = millis();
    uint8_t hue = (now / 10) % 256;  // Langsame Rotation durch Farbraum
    
    for (int i = 0; i < numLeds; i++) {
        leds[i] = CHSV(hue + (i * 256 / numLeds), 255, 255);
    }
    
    applyBrightness(leds, numLeds, state.effect.brightness);
}

void LEDSpotlight::updateChase(EffectState& state, CRGB* leds, uint8_t numLeds) {
    unsigned long now = millis();
    
    if (now - state.lastUpdate >= state.effect.speed) {
        state.lastUpdate = now;
        state.position = (state.position + 1) % numLeds;
    }
    
    // Alle auf inaktiv
    for (int i = 0; i < numLeds; i++) {
        leds[i] = CRGB::Black;
    }
    
    // Aktive Position
    leds[state.position] = state.effect.color.toCRGB();
    
    applyBrightness(leds, numLeds, state.effect.brightness);
}

// ============================================================================
// ROTATION PATTERN RENDERER
// ============================================================================

void LEDSpotlight::renderRotationSingle(CRGB* leds, uint8_t numLeds, const EffectState& state) {
    CRGB activeColor = state.effect.rotation.activeColor.toCRGB();
    CRGB inactiveColor = state.effect.rotation.inactiveColor.toCRGB();
    
    for (int i = 0; i < numLeds; i++) {
        if (i == state.position) {
            leds[i] = activeColor;
        } else {
            leds[i] = inactiveColor;
        }
    }
}

void LEDSpotlight::renderRotationTrail(CRGB* leds, uint8_t numLeds, const EffectState& state) {
    CRGB activeColor = state.effect.rotation.activeColor.toCRGB();
    CRGB inactiveColor = state.effect.rotation.inactiveColor.toCRGB();
    uint8_t trailLength = state.effect.rotation.trailLength;
    
    for (int i = 0; i < numLeds; i++) {
        // Distanz von aktueller Position berechnen
        int distance = (state.position - i + numLeds) % numLeds;
        
        if (distance == 0) {
            // Hauptpunkt: Volle Helligkeit
            leds[i] = activeColor;
        } else if (distance <= trailLength) {
            // Schweif: Fade
            float fade = 1.0 - ((float)distance / trailLength);
            leds[i] = blendCRGB(activeColor, inactiveColor, fade);
        } else {
            leds[i] = inactiveColor;
        }
    }
}

void LEDSpotlight::renderRotationOpposite(CRGB* leds, uint8_t numLeds, const EffectState& state) {
    CRGB activeColor = state.effect.rotation.activeColor.toCRGB();
    CRGB inactiveColor = state.effect.rotation.inactiveColor.toCRGB();
    
    uint8_t oppositePos = (state.position + numLeds / 2) % numLeds;
    
    for (int i = 0; i < numLeds; i++) {
        if (i == state.position || i == oppositePos) {
            leds[i] = activeColor;
        } else {
            leds[i] = inactiveColor;
        }
    }
}

void LEDSpotlight::renderRotationWave(CRGB* leds, uint8_t numLeds, const EffectState& state) {
    CRGB activeColor = state.effect.rotation.activeColor.toCRGB();
    CRGB inactiveColor = state.effect.rotation.inactiveColor.toCRGB();
    uint8_t waveLength = 3;  // Anzahl aktiver LEDs
    
    for (int i = 0; i < numLeds; i++) {
        int distance = (i - state.position + numLeds) % numLeds;
        
        if (distance < waveLength) {
            leds[i] = activeColor;
        } else {
            leds[i] = inactiveColor;
        }
    }
}

// ============================================================================
// HILFSFUNKTIONEN
// ============================================================================

Color LEDSpotlight::blendColor(const Color& c1, const Color& c2, float factor) {
    factor = constrain(factor, 0.0, 1.0);
    return Color(
        c1.r * factor + c2.r * (1.0 - factor),
        c1.g * factor + c2.g * (1.0 - factor),
        c1.b * factor + c2.b * (1.0 - factor)
    );
}

CRGB LEDSpotlight::blendCRGB(const CRGB& c1, const CRGB& c2, float factor) {
    factor = constrain(factor, 0.0, 1.0);
    return CRGB(
        c1.r * factor + c2.r * (1.0 - factor),
        c1.g * factor + c2.g * (1.0 - factor),
        c1.b * factor + c2.b * (1.0 - factor)
    );
}

void LEDSpotlight::applyBrightness(CRGB* leds, uint8_t numLeds, uint8_t brightness) {
    if (brightness == 255) return;
    
    for (int i = 0; i < numLeds; i++) {
        leds[i].fadeToBlackBy(255 - brightness);
    }
}

String LEDSpotlight::getStatusJson() {
    StaticJsonDocument<512> doc;
    
    doc["id"] = spotlightId;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime"] = millis();
    
    JsonObject inner = doc.createNestedObject("innerRing");
    inner["active"] = innerState.active;
    inner["effect"] = innerState.active ? 
        (innerState.effect.type == EFFECT_ROTATION ? "rotation" : "other") : "off";
    
    JsonObject outer = doc.createNestedObject("outerRing");
    outer["active"] = outerState.active;
    outer["effect"] = outerState.active ? 
        (outerState.effect.type == EFFECT_ROTATION ? "rotation" : "other") : "off";
    
    String output;
    serializeJson(doc, output);
    return output;
}
