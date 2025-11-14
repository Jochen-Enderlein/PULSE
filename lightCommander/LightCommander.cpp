#include "LightCommander.h"
#include <SPIFFS.h>

// ============================================================================
// KONSTRUKTOR & INITIALISIERUNG
// ============================================================================

LightCommander::LightCommander() : 
    server(80),
    isAPMode(false),
    currentSequence(nullptr),
    currentEventIndex(0),
    spotifyConnected(false) {
}

void LightCommander::begin(const char* ssid, const char* password, bool apMode) {
    Serial.begin(115200);
    Serial.println("\n=== Light Commander Starting ===");
    
    wifiSSID = String(ssid);
    wifiPassword = String(password);
    isAPMode = apMode;
    
    // SPIFFS initialisieren (für Sequenz-Speicherung)
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
    }
    
    // WiFi Setup
    if (isAPMode) {
        Serial.println("Starting Access Point Mode...");
        WiFi.softAP(ssid, password);
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("Connecting to WiFi...");
        WiFi.begin(ssid, password);
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected!");
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("\nFailed to connect. Starting AP Mode...");
            Serial.print(WiFi.status());
            WiFi.softAP(ssid, password);
            isAPMode = true;
        }
    }
    
    // REST API Routes einrichten
    setupRoutes();
    server.begin();
    
    Serial.println("Light Commander Ready!");
}

void LightCommander::loop() {
    server.handleClient();
    
    // Rotation-Effekte aktualisieren
    updateRotationEffects();
    
    // Sequenz-Playback aktualisieren
    if (playback.active && !playback.paused) {
        updateSequencePlayback();
    }
    
    // Periodisch Scheinwerfer-Status prüfen (alle 10 Sekunden)
    static unsigned long lastStatusCheck = 0;
    if (millis() - lastStatusCheck > 10000) {
        checkSpotlightStatus();
        lastStatusCheck = millis();
    }
}

// ============================================================================
// REST API ROUTES
// ============================================================================

void LightCommander::setupRoutes() {
    // Status & Info
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/api/status", HTTP_GET, [this]() { handleGetStatus(); });
    
    // Scheinwerfer-Management
    server.on("/api/spotlight/add", HTTP_POST, [this]() { handleAddSpotlight(); });
    server.on("/api/spotlight/list", HTTP_GET, [this]() { handleListSpotlights(); });
    
    // Effekte
    server.on("/api/effect/send", HTTP_POST, [this]() { handleSendEffect(); });
    server.on("/api/effect/stop", HTTP_POST, [this]() { handleStopEffect(); });
    
    // Sequenzen
    server.on("/api/sequence/load", HTTP_POST, [this]() { handleLoadSequence(); });
    server.on("/api/sequence/list", HTTP_GET, [this]() { handleListSequences(); });
    server.on("/api/sequence/play", HTTP_POST, [this]() { handlePlaySequence(); });
    server.on("/api/sequence/pause", HTTP_POST, [this]() { handlePauseSequence(); });
    server.on("/api/sequence/stop", HTTP_POST, [this]() { handleStopSequence(); });
}

void LightCommander::handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>Light Commander</title></head><body>";
    html += "<h1>Light Commander Control Panel</h1>";
    html += "<h2>API Endpoints:</h2>";
    html += "<ul>";
    html += "<li>GET /api/status - System status</li>";
    html += "<li>POST /api/spotlight/add - Add spotlight</li>";
    html += "<li>GET /api/spotlight/list - List spotlights</li>";
    html += "<li>POST /api/effect/send - Send effect</li>";
    html += "<li>POST /api/effect/stop - Stop effect</li>";
    html += "<li>POST /api/sequence/load - Load sequence</li>";
    html += "<li>GET /api/sequence/list - List sequences</li>";
    html += "<li>POST /api/sequence/play - Play sequence</li>";
    html += "<li>POST /api/sequence/pause - Pause sequence</li>";
    html += "<li>POST /api/sequence/stop - Stop sequence</li>";
    html += "</ul>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void LightCommander::handleGetStatus() {
    server.send(200, "application/json", getStatusJson());
}

void LightCommander::handleAddSpotlight() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String id = doc["id"] | "";
    String name = doc["name"] | "";
    String ip = doc["ip"] | "";
    
    if (id.isEmpty() || ip.isEmpty()) {
        server.send(400, "application/json", "{\"error\":\"Missing id or ip\"}");
        return;
    }
    
    if (addSpotlight(id, name, ip)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to add spotlight\"}");
    }
}

void LightCommander::handleListSpotlights() {
    StaticJsonDocument<2048> doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (auto& pair : spotlights) {
        JsonObject obj = array.createNestedObject();
        obj["id"] = pair.second.id;
        obj["name"] = pair.second.name;
        obj["ip"] = pair.second.ip;
        obj["online"] = pair.second.online;
        obj["innerLeds"] = pair.second.innerLedCount;
        obj["outerLeds"] = pair.second.outerLedCount;
    }
    
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void LightCommander::handleSendEffect() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Ziele parsen
    std::vector<String> targets;
    JsonArray targetsArray = doc["targets"];
    for (JsonVariant v : targetsArray) {
        targets.push_back(v.as<String>());
    }
    
    // Ring-Typ
    String ringStr = doc["ring"] | "both";
    RingType ring = RING_BOTH;
    if (ringStr == "inner") ring = RING_INNER;
    else if (ringStr == "outer") ring = RING_OUTER;
    
    // Effekt-Typ
    String effectStr = doc["effect"] | "static";
    EffectType effect = EFFECT_STATIC;
    if (effectStr == "fade") effect = EFFECT_FADE;
    else if (effectStr == "strobe") effect = EFFECT_STROBE;
    else if (effectStr == "pulse") effect = EFFECT_PULSE;
    else if (effectStr == "rotation") effect = EFFECT_ROTATION;
    else if (effectStr == "rainbow") effect = EFFECT_RAINBOW;
    else if (effectStr == "chase") effect = EFFECT_CHASE;
    
    // Parameter
    EffectParams params;
    if (doc.containsKey("color")) {
        JsonArray colorArray = doc["color"];
        params.color = Color(colorArray[0], colorArray[1], colorArray[2]);
    }
    if (doc.containsKey("brightness")) {
        params.brightness = doc["brightness"];
    }
    if (doc.containsKey("duration")) {
        params.duration = doc["duration"];
    }
    
    // Rotations-spezifische Parameter
    if (effect == EFFECT_ROTATION && doc.containsKey("rotation")) {
        JsonObject rot = doc["rotation"];
        
        if (rot.containsKey("activeColor")) {
            JsonArray ac = rot["activeColor"];
            params.rotation.activeColor = Color(ac[0], ac[1], ac[2]);
        }
        if (rot.containsKey("inactiveColor")) {
            JsonArray ic = rot["inactiveColor"];
            params.rotation.inactiveColor = Color(ic[0], ic[1], ic[2]);
        }
        if (rot.containsKey("speed")) {
            params.rotation.speed = rot["speed"];
        }
        if (rot.containsKey("direction")) {
            String dir = rot["direction"] | "clockwise";
            params.rotation.direction = (dir == "counterclockwise") ? 
                DIRECTION_COUNTERCLOCKWISE : DIRECTION_CLOCKWISE;
        }
        if (rot.containsKey("pattern")) {
            String pat = rot["pattern"] | "single";
            if (pat == "trail") params.rotation.pattern = PATTERN_TRAIL;
            else if (pat == "opposite") params.rotation.pattern = PATTERN_OPPOSITE;
            else if (pat == "wave") params.rotation.pattern = PATTERN_WAVE;
            else if (pat == "rainbow_chase") params.rotation.pattern = PATTERN_RAINBOW_CHASE;
        }
        if (rot.containsKey("trailFade")) {
            params.rotation.trailFade = rot["trailFade"];
        }
        if (rot.containsKey("trailLength")) {
            params.rotation.trailLength = rot["trailLength"];
        }
        if (rot.containsKey("startOffset")) {
            params.rotation.startOffset = rot["startOffset"];
        }
    }
    
    if (sendEffect(targets, ring, effect, params)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send effect\"}");
    }
}

void LightCommander::handleStopEffect() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    std::vector<String> targets;
    JsonArray targetsArray = doc["targets"];
    for (JsonVariant v : targetsArray) {
        targets.push_back(v.as<String>());
    }
    
    if (stopEffect(targets)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to stop effect\"}");
    }
}

void LightCommander::handleLoadSequence() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    if (loadSequence(server.arg("plain"))) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to load sequence\"}");
    }
}

void LightCommander::handleListSequences() {
    std::vector<String> seqList = listSequences();
    
    StaticJsonDocument<1024> doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (const String& seqId : seqList) {
        Sequence* seq = getSequence(seqId);
        if (seq) {
            JsonObject obj = array.createNestedObject();
            obj["id"] = seq->id;
            obj["name"] = seq->name;
            obj["duration"] = seq->duration;
            obj["eventCount"] = seq->events.size();
        }
    }
    
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void LightCommander::handlePlaySequence() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String sequenceId = doc["sequenceId"] | "";
    
    if (playSequence(sequenceId)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to play sequence\"}");
    }
}

void LightCommander::handlePauseSequence() {
    if (pauseSequence()) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Not playing\"}");
    }
}

void LightCommander::handleStopSequence() {
    if (stopSequence()) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Not playing\"}");
    }
}

// ============================================================================
// SCHEINWERFER-MANAGEMENT
// ============================================================================

bool LightCommander::addSpotlight(const String& id, const String& name, const String& ip) {
    Spotlight spot;
    spot.id = id;
    spot.name = name.isEmpty() ? id : name;
    spot.ip = ip;
    spot.online = true;
    spot.lastSeen = millis();
    
    spotlights[id] = spot;
    
    Serial.printf("Added spotlight: %s (%s) at %s\n", id.c_str(), name.c_str(), ip.c_str());
    return true;
}

bool LightCommander::removeSpotlight(const String& id) {
    auto it = spotlights.find(id);
    if (it != spotlights.end()) {
        spotlights.erase(it);
        return true;
    }
    return false;
}

Spotlight* LightCommander::getSpotlight(const String& id) {
    auto it = spotlights.find(id);
    if (it != spotlights.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<Spotlight*> LightCommander::getAllSpotlights() {
    std::vector<Spotlight*> result;
    for (auto& pair : spotlights) {
        result.push_back(&pair.second);
    }
    return result;
}

void LightCommander::discoverSpotlights() {
    // TODO: Implementiere mDNS Discovery für WLED-Geräte
    // Für jetzt: Manuelles Hinzufügen über API
    Serial.println("Discovery not yet implemented. Use API to add spotlights.");
}

void LightCommander::checkSpotlightStatus() {
    HTTPClient http;
    
    for (auto& pair : spotlights) {
        Spotlight& spot = pair.second;
        
        String url = "http://" + spot.ip + "/json/info";
        http.begin(url);
        http.setTimeout(2000);
        
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            spot.online = true;
            spot.lastSeen = millis();
        } else {
            spot.online = false;
        }
        
        http.end();
    }
}

// ============================================================================
// EFFEKT-STEUERUNG
// ============================================================================

bool LightCommander::sendEffect(const std::vector<String>& targets, RingType ring, 
                                 EffectType effect, const EffectParams& params) {
    bool success = true;
    
    for (const String& targetId : targets) {
        Spotlight* spot = getSpotlight(targetId);
        if (!spot || !spot->online) {
            Serial.printf("Spotlight %s not available\n", targetId.c_str());
            success = false;
            continue;
        }
        
        // Für Rotation: State speichern
        if (effect == EFFECT_ROTATION) {
            RotationState state;
            state.active = true;
            state.currentPosition = params.rotation.startOffset;
            state.lastUpdate = millis();
            state.params = params.rotation;
            state.ring = ring;
            rotationStates[targetId] = state;
        }
        
        String json = buildWLEDJson(ring, effect, params);
        if (!sendToWLED(spot->ip, json)) {
            success = false;
        }
    }
    
    return success;
}

bool LightCommander::stopEffect(const std::vector<String>& targets) {
    bool success = true;
    
    for (const String& targetId : targets) {
        Spotlight* spot = getSpotlight(targetId);
        if (!spot) continue;
        
        // Rotation stoppen
        rotationStates.erase(targetId);
        
        // WLED ausschalten
        String json = "{\"on\":false}";
        if (!sendToWLED(spot->ip, json)) {
            success = false;
        }
    }
    
    return success;
}

bool LightCommander::setColor(const std::vector<String>& targets, RingType ring, 
                               const Color& color, uint8_t brightness) {
    EffectParams params;
    params.color = color;
    params.brightness = brightness;
    
    return sendEffect(targets, ring, EFFECT_STATIC, params);
}

// ============================================================================
// SEQUENZ-MANAGEMENT
// ============================================================================

bool LightCommander::loadSequence(const String& json) {
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.println("Failed to parse sequence JSON");
        return false;
    }
    
    Sequence seq;
    seq.id = doc["id"] | "";
    seq.name = doc["name"] | "";
    seq.duration = doc["duration"] | 0;
    seq.loop = doc["loop"] | false;
    
    // Spotify-Felder (für später)
    seq.spotifyUri = doc["spotifyUri"] | "";
    seq.syncWithSpotify = doc["syncWithSpotify"] | false;
    
    // Events parsen
    JsonArray eventsArray = doc["events"];
    for (JsonObject eventObj : eventsArray) {
        SequenceEvent event;
        event.timestamp = eventObj["timestamp"] | 0;
        
        // Targets
        JsonArray targetsArray = eventObj["targets"];
        for (JsonVariant v : targetsArray) {
            event.targets.push_back(v.as<String>());
        }
        
        // Ring
        String ringStr = eventObj["ring"] | "both";
        if (ringStr == "inner") event.ring = RING_INNER;
        else if (ringStr == "outer") event.ring = RING_OUTER;
        else event.ring = RING_BOTH;
        
        // Effect
        String effectStr = eventObj["effect"] | "static";
        if (effectStr == "fade") event.effect = EFFECT_FADE;
        else if (effectStr == "strobe") event.effect = EFFECT_STROBE;
        else if (effectStr == "pulse") event.effect = EFFECT_PULSE;
        else if (effectStr == "rotation") event.effect = EFFECT_ROTATION;
        else if (effectStr == "rainbow") event.effect = EFFECT_RAINBOW;
        else if (effectStr == "chase") event.effect = EFFECT_CHASE;
        else event.effect = EFFECT_STATIC;
        
        // Params (vereinfacht - nur wichtigste)
        JsonObject params = eventObj["params"];
        if (params.containsKey("color")) {
            JsonArray c = params["color"];
            event.params.color = Color(c[0], c[1], c[2]);
        }
        if (params.containsKey("brightness")) {
            event.params.brightness = params["brightness"];
        }
        if (params.containsKey("duration")) {
            event.params.duration = params["duration"];
        }
        
        // Rotation params
        if (params.containsKey("rotation")) {
            JsonObject rot = params["rotation"];
            if (rot.containsKey("activeColor")) {
                JsonArray ac = rot["activeColor"];
                event.params.rotation.activeColor = Color(ac[0], ac[1], ac[2]);
            }
            if (rot.containsKey("inactiveColor")) {
                JsonArray ic = rot["inactiveColor"];
                event.params.rotation.inactiveColor = Color(ic[0], ic[1], ic[2]);
            }
            if (rot.containsKey("speed")) {
                event.params.rotation.speed = rot["speed"];
            }
        }
        
        seq.events.push_back(event);
    }
    
    sequences[seq.id] = seq;
    
    Serial.printf("Loaded sequence: %s (%d events)\n", seq.name.c_str(), seq.events.size());
    return true;
}

bool LightCommander::saveSequence(const Sequence& seq) {
    // TODO: Speichere Sequenz in SPIFFS
    sequences[seq.id] = seq;
    return true;
}

Sequence* LightCommander::getSequence(const String& id) {
    auto it = sequences.find(id);
    if (it != sequences.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<String> LightCommander::listSequences() {
    std::vector<String> result;
    for (auto& pair : sequences) {
        result.push_back(pair.first);
    }
    return result;
}

bool LightCommander::deleteSequence(const String& id) {
    auto it = sequences.find(id);
    if (it != sequences.end()) {
        sequences.erase(it);
        return true;
    }
    return false;
}

// ============================================================================
// SEQUENZ-PLAYBACK
// ============================================================================

bool LightCommander::playSequence(const String& sequenceId) {
    currentSequence = getSequence(sequenceId);
    if (!currentSequence) {
        Serial.printf("Sequence %s not found\n", sequenceId.c_str());
        return false;
    }
    
    playback.active = true;
    playback.currentSequence = sequenceId;
    playback.startTime = millis();
    playback.position = 0;
    playback.paused = false;
    currentEventIndex = 0;
    
    Serial.printf("Playing sequence: %s\n", currentSequence->name.c_str());
    
    // TODO: Hier später Spotify starten, wenn syncWithSpotify = true
    // if (currentSequence->syncWithSpotify && !currentSequence->spotifyUri.isEmpty()) {
    //     playSpotifyTrack(currentSequence->spotifyUri);
    // }
    
    return true;
}

bool LightCommander::pauseSequence() {
    if (!playback.active) return false;
    
    playback.paused = true;
    playback.position = millis() - playback.startTime;
    
    Serial.println("Sequence paused");
    return true;
}

bool LightCommander::resumeSequence() {
    if (!playback.active || !playback.paused) return false;
    
    playback.paused = false;
    playback.startTime = millis() - playback.position;
    
    Serial.println("Sequence resumed");
    return true;
}

bool LightCommander::stopSequence() {
    if (!playback.active) return false;
    
    playback.active = false;
    playback.paused = false;
    playback.currentSequence = "";
    currentSequence = nullptr;
    currentEventIndex = 0;
    
    // Alle Effekte stoppen
    std::vector<String> allTargets;
    for (auto& pair : spotlights) {
        allTargets.push_back(pair.first);
    }
    stopEffect(allTargets);
    
    Serial.println("Sequence stopped");
    return true;
}

PlaybackState LightCommander::getPlaybackState() {
    if (playback.active && !playback.paused) {
        playback.position = millis() - playback.startTime;
    }
    return playback;
}

void LightCommander::updateSequencePlayback() {
    if (!currentSequence || currentEventIndex >= currentSequence->events.size()) {
        // Sequenz beendet
        if (currentSequence && currentSequence->loop) {
            // Loop: Neu starten
            playback.startTime = millis();
            playback.position = 0;
            currentEventIndex = 0;
        } else {
            stopSequence();
        }
        return;
    }
    
    unsigned long currentTime = millis() - playback.startTime;
    
    // Prüfe, ob nächstes Event ausgelöst werden muss
    while (currentEventIndex < currentSequence->events.size()) {
        const SequenceEvent& event = currentSequence->events[currentEventIndex];
        
        if (currentTime >= event.timestamp) {
            processSequenceEvent(event);
            currentEventIndex++;
        } else {
            break;
        }
    }
}

void LightCommander::processSequenceEvent(const SequenceEvent& event) {
    Serial.printf("Executing event at %lu ms\n", event.timestamp);
    sendEffect(event.targets, event.ring, event.effect, event.params);
}

// ============================================================================
// ROTATION UPDATE
// ============================================================================

void LightCommander::updateRotationEffects() {
    unsigned long now = millis();
    
    for (auto& pair : rotationStates) {
        String spotId = pair.first;
        RotationState& state = pair.second;
        
        if (!state.active) continue;
        
        // Zeit für nächsten Step?
        if (now - state.lastUpdate >= state.params.speed) {
            state.lastUpdate = now;
            
            // Position aktualisieren
            if (state.params.direction == DIRECTION_CLOCKWISE) {
                state.currentPosition++;
            } else {
                state.currentPosition--;
            }
            
            // LED-Count abhängig vom Ring
            Spotlight* spot = getSpotlight(spotId);
            if (!spot) continue;
            
            uint8_t ledCount = (state.ring == RING_INNER) ? 
                spot->innerLedCount : spot->outerLedCount;
            
            state.currentPosition = state.currentPosition % ledCount;
            
            // Neues Pattern an WLED senden
            String json = buildRotationPatternJson(state, ledCount);
            sendToWLED(spot->ip, json);
        }
    }
}

// ============================================================================
// WLED KOMMUNIKATION
// ============================================================================

bool LightCommander::sendToWLED(const String& ip, const String& json) {
    HTTPClient http;
    String url = "http://" + ip + "/json/state";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(json);
    bool success = (httpCode == 200);
    
    if (!success) {
        Serial.printf("WLED request failed: %d\n", httpCode);
    }
    
    http.end();
    return success;
}

String LightCommander::buildWLEDJson(RingType ring, EffectType effect, const EffectParams& params) {
    StaticJsonDocument<1024> doc;
    
    doc["on"] = true;
    doc["bri"] = params.brightness;
    
    JsonArray seg = doc.createNestedArray("seg");
    
    // Segmente für Ringe erstellen
    if (ring == RING_INNER || ring == RING_BOTH) {
        JsonObject innerSeg = seg.createNestedObject();
        innerSeg["id"] = 0;
        innerSeg["start"] = 0;
        innerSeg["stop"] = 8;
        
        JsonArray col = innerSeg.createNestedArray("col");
        JsonArray c1 = col.createNestedArray();
        c1.add(params.color.r);
        c1.add(params.color.g);
        c1.add(params.color.b);
        
        // Effekt-Mapping auf WLED
        switch (effect) {
            case EFFECT_STATIC: innerSeg["fx"] = 0; break;
            case EFFECT_FADE: innerSeg["fx"] = 1; break;
            case EFFECT_STROBE: innerSeg["fx"] = 2; break;
            case EFFECT_PULSE: innerSeg["fx"] = 3; break;
            case EFFECT_RAINBOW: innerSeg["fx"] = 9; break;
            case EFFECT_CHASE: innerSeg["fx"] = 28; break;
            default: innerSeg["fx"] = 0;
        }
    }
    
    if (ring == RING_OUTER || ring == RING_BOTH) {
        JsonObject outerSeg = seg.createNestedObject();
        outerSeg["id"] = 1;
        outerSeg["start"] = 8;
        outerSeg["stop"] = 34;
        
        JsonArray col = outerSeg.createNestedArray("col");
        JsonArray c1 = col.createNestedArray();
        c1.add(params.color.r);
        c1.add(params.color.g);
        c1.add(params.color.b);
        
        switch (effect) {
            case EFFECT_STATIC: outerSeg["fx"] = 0; break;
            case EFFECT_FADE: outerSeg["fx"] = 1; break;
            case EFFECT_STROBE: outerSeg["fx"] = 2; break;
            case EFFECT_PULSE: outerSeg["fx"] = 3; break;
            case EFFECT_RAINBOW: outerSeg["fx"] = 9; break;
            case EFFECT_CHASE: outerSeg["fx"] = 28; break;
            default: outerSeg["fx"] = 0;
        }
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

String LightCommander::buildRotationPatternJson(const RotationState& state, uint8_t ledCount) {
    // Für Rotation: Individuelles LED-Array per UDP würde besser funktionieren
    // Aber für Proof-of-Concept: Nutze WLED Chase-Effekt mit Anpassungen
    
    StaticJsonDocument<512> doc;
    doc["on"] = true;
    doc["bri"] = 255;
    
    JsonArray seg = doc.createNestedArray("seg");
    JsonObject segment = seg.createNestedObject();
    segment["id"] = (state.ring == RING_INNER) ? 0 : 1;
    segment["fx"] = 28; // Chase
    segment["sx"] = map(state.params.speed, 10, 500, 255, 10);
    segment["ix"] = state.currentPosition * (255 / ledCount);
    
    JsonArray col = segment.createNestedArray("col");
    JsonArray c1 = col.createNestedArray();
    c1.add(state.params.activeColor.r);
    c1.add(state.params.activeColor.g);
    c1.add(state.params.activeColor.b);
    
    JsonArray c2 = col.createNestedArray();
    c2.add(state.params.inactiveColor.r);
    c2.add(state.params.inactiveColor.g);
    c2.add(state.params.inactiveColor.b);
    
    String output;
    serializeJson(doc, output);
    return output;
}

// ============================================================================
// STATUS & HILFSFUNKTIONEN
// ============================================================================

String LightCommander::getStatusJson() {
    StaticJsonDocument<2048> doc;
    
    // System Info
    doc["uptime"] = millis();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
    doc["ip"] = isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    
    // Playback Status
    JsonObject pb = doc.createNestedObject("playback");
    pb["active"] = playback.active;
    pb["sequence"] = playback.currentSequence;
    pb["position"] = playback.paused ? playback.position : (millis() - playback.startTime);
    pb["paused"] = playback.paused;
    pb["spotifyConnected"] = playback.spotifyConnected;
    
    // Devices
    JsonArray devices = doc.createNestedArray("devices");
    for (auto& pair : spotlights) {
        JsonObject dev = devices.createNestedObject();
        dev["id"] = pair.second.id;
        dev["name"] = pair.second.name;
        dev["online"] = pair.second.online;
        dev["ip"] = pair.second.ip;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

Color LightCommander::blendColor(const Color& c1, const Color& c2, float factor) {
    return Color(
        c1.r * factor + c2.r * (1.0 - factor),
        c1.g * factor + c2.g * (1.0 - factor),
        c1.b * factor + c2.b * (1.0 - factor)
    );
}

String LightCommander::colorToJson(const Color& c) {
    return "[" + String(c.r) + "," + String(c.g) + "," + String(c.b) + "]";
}

unsigned long LightCommander::getCurrentTime() {
    return millis();
}

// ============================================================================
// SPOTIFY PLATZHALTER (Für spätere Integration)
// ============================================================================

bool LightCommander::connectSpotify(const String& clientId, const String& clientSecret) {
    // TODO: OAuth2 Flow implementieren
    // TODO: Access Token erhalten
    // TODO: Token speichern
    Serial.println("Spotify integration not yet implemented");
    return false;
}

bool LightCommander::playSpotifyTrack(const String& uri) {
    // TODO: Spotify Web API aufrufen
    // POST https://api.spotify.com/v1/me/player/play
    Serial.println("Spotify playback not yet implemented");
    return false;
}

unsigned long LightCommander::getSpotifyPosition() {
    // TODO: Aktuelle Track-Position von Spotify abrufen
    // GET https://api.spotify.com/v1/me/player
    return 0;
}
